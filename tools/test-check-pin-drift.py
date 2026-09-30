#!/usr/bin/env python3
"""Table test for check-pin-drift.py. Writes its own baseline.json files, so no network."""
import contextlib
import importlib.util
import io
import json
import sys
import tempfile
from pathlib import Path

MOD = Path(__file__).with_name("check-pin-drift.py")
spec = importlib.util.spec_from_file_location("check_pin_drift", MOD)
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)

GUI_SHA = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
ENG_SHA = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"


def baseline(d, name, **ports):
    """A vcpkg versions/baseline.json holding only the ports a case cares about."""
    p = d / f"bl-{name}.json"
    p.write_text(json.dumps({"default": {k: {"baseline": v} for k, v in ports.items()}}),
                 encoding="utf-8")
    return str(p)


FULL = dict(openssl="3.6.4", zlib="1.3.2", brotli="1.2.0", zstd="1.5.7")


def run(gui_ports, engine_ports, upstream_ports, same_sha=False):
    """(stdout, versions file lines, exit reason). reason is None when it returned."""
    with tempfile.TemporaryDirectory() as td:
        d = Path(td)
        eng_sha = GUI_SHA if same_sha else ENG_SHA
        pins = [f"GUI={GUI_SHA}:{baseline(d, 'gui', **gui_ports)}",
                f"engine={eng_sha}:{baseline(d, 'eng', **engine_ports)}"]
        out_file = d / "versions.txt"
        argv = ["check-pin-drift.py", "--upstream", baseline(d, 'head', **upstream_ports),
                "--versions-out", str(out_file)]
        for pin in pins:
            argv += ["--pin", pin]
        real_argv, sys.argv = sys.argv, argv
        out, reason = io.StringIO(), None
        try:
            with contextlib.redirect_stdout(out):
                try:
                    check.main()
                except SystemExit as e:
                    reason = str(e.code)
                # Any other exception is a failure to report, not a traceback that
                # takes the whole table down and hides the rows after it.
                except Exception as e:  # noqa: BLE001
                    reason = f"{type(e).__name__}: {e}"
        finally:
            sys.argv = real_argv
        versions = out_file.read_text(encoding="utf-8").splitlines() if out_file.exists() else []
        return out.getvalue(), versions, reason


CASES = [
    # (name, gui, engine, upstream, same_sha, want_in, want_not_in, want_versions)
    ("equal pins level with upstream report no drift",
     FULL, FULL, FULL, True, "openssl: pinned 3.6.4, upstream 3.6.4", "DRIFT",
     ["3.6.4 GUI+engine"]),
    # The whole reason the tool takes two pins.
    ("a stale engine pin drifts even when ours does not",
     FULL, dict(FULL, openssl="3.6.3"), FULL, True,
     "openssl: pinned 3.6.3, upstream 3.6.4   <-- DRIFT", None,
     ["3.6.3 engine", "3.6.4 GUI"]),
    ("differing pin shas are themselves drift",
     FULL, FULL, FULL, False, "the pins differ: GUI aaaaaaaaaaaa, engine bbbbbbbbbbbb   <-- DRIFT",
     None, ["3.6.4 GUI+engine"]),
    ("our own pin drifting is still reported",
     dict(FULL, openssl="3.6.3"), FULL, FULL, True,
     "--- GUI pin aaaaaaaaaaaa ---", None, ["3.6.3 GUI", "3.6.4 engine"]),
    ("a port missing upstream reads as None rather than passing quietly",
     FULL, FULL, dict(openssl="3.6.4"), True, "zlib: pinned 1.3.2, upstream None   <-- DRIFT",
     None, ["3.6.4 GUI+engine"]),
    # A baseline without openssl would otherwise contribute no version and scan nothing.
    ("a pin naming no openssl is fatal",
     FULL, dict(zlib="1.3.2"), FULL, True, None, None, None),
]

bad = 0
for name, gui, eng, head, same, want_in, want_not, want_versions in CASES:
    out, versions, reason = run(gui, eng, head, same_sha=same)
    problems = []
    if want_versions is None:
        if reason is None:
            problems.append("expected a fatal exit")
        elif "names no openssl" not in reason:
            problems.append(f"exit reason {reason!r} lacks 'names no openssl'")
    else:
        if reason is not None:
            problems.append(f"unexpected exit: {reason!r}")
        if versions != want_versions:
            problems.append(f"versions {versions!r}, expected {want_versions!r}")
    if want_in and want_in not in out:
        problems.append(f"missing {want_in!r}")
    if want_not and want_not in out:
        problems.append(f"unexpected {want_not!r}")
    if problems:
        bad += 1
        print(f"FAIL {name}: {'; '.join(problems)}\n{out}")
    else:
        print(f"ok   {name}")

# Pin the count, or deleting a row leaves this green.
if len(CASES) != 6:
    print(f"FAIL expected 6 cases, table has {len(CASES)}")
    bad += 1

print(f"{len(CASES)} cases, {bad} failed")
sys.exit(1 if bad else 0)
