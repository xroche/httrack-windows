#!/usr/bin/env python3
"""Table test for check-baseline-forward.py. Stubs the compare call, so no network."""
import contextlib
import importlib.util
import io
import json
import sys
import tempfile
from pathlib import Path

MOD = Path(__file__).with_name("check-baseline-forward.py")
spec = importlib.util.spec_from_file_location("check_baseline_forward", MOD)
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)

OLD = "04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4"
NEW = "e182cb4dd2df2ab02f66a1aabd5f35bbdc9522c7"
# What dependabot #202 did: the newest release tag, older than the pin it replaced.
TAG = "9e593bb18ea69cc5095e012465dcd675a822ed0d"


def manifest(d, name, body):
    """Write a manifest, a dict as JSON and a string verbatim so a case can malform it."""
    p = d / f"{name}.json"
    p.write_text(body if isinstance(body, str) else json.dumps(body), encoding="utf-8")
    return str(p)


def run(old_body, new_body, status):
    """(stdout, exit code, compare calls). The code is None when main() returned."""
    with tempfile.TemporaryDirectory() as td:
        d = Path(td)
        argv = ["check-baseline-forward.py",
                "--old", manifest(d, "old", old_body),
                "--new", manifest(d, "new", new_body)]
        real_argv, sys.argv = sys.argv, argv
        real_compare, calls = check.compare_status, []

        def fake(old, new):
            calls.append((old, new))
            return status

        check.compare_status = fake
        out, code = io.StringIO(), None
        try:
            with contextlib.redirect_stdout(out):
                try:
                    check.main()
                except SystemExit as e:
                    code = e.code
                except Exception as e:  # noqa: BLE001
                    code = f"{type(e).__name__}: {e}"
        finally:
            sys.argv, check.compare_status = real_argv, real_compare
        return out.getvalue(), code, len(calls)


def pin(sha):
    return {"name": "winhttrack", "builtin-baseline": sha, "dependencies": ["openssl"]}


CASES = [
    # (name, old manifest, new manifest, stubbed status, want in output, must be refused)
    ("a descendant is the bump we want", pin(OLD), pin(NEW), "ahead",
     "04a9d8e5212d -> e182cb4dd2df (ahead)", False),
    ("an unchanged pin asks nothing of the API", pin(OLD), pin(OLD), None,
     "unchanged at 04a9d8e5212d", False),
    ("the #202 revert is refused", pin(OLD), pin(TAG), "behind",
     "goes backwards: 9e593bb18ea6 reads behind", True),
    ("a pin off our own history is refused", pin(OLD), pin(NEW), "diverged",
     "reads diverged", True),
    # Fail closed: an API that cannot answer must not read as a forward move.
    ("an unreadable comparison is refused", pin(OLD), pin(NEW), None,
     "cannot tell whether e182cb4dd2df descends", True),
    # A status we have never seen means the API changed, not that the pin is fine.
    ("an unknown status is refused", pin(OLD), pin(NEW), "sideways", "reads sideways", True),
    ("a manifest with no baseline is refused", pin(OLD), {"name": "winhttrack"}, "ahead",
     "expected a 40-hex commit", True),
    ("a tag name in place of a commit is refused", pin(OLD), pin("2026.07.29"), "ahead",
     "expected a 40-hex commit", True),
    ("a truncated sha is refused", pin(OLD), pin(NEW[:12]), "ahead",
     "expected a 40-hex commit", True),
    # Length alone is not a commit, so a check that counts characters must fail here.
    ("a 40-character non-hex pin is refused", pin(OLD), pin("z" * 40), "ahead",
     "expected a 40-hex commit", True),
    ("a manifest that is not JSON is refused", pin(OLD), "{oops", "ahead",
     "not valid JSON", True),
]

bad = 0
for name, old_body, new_body, status, want, refuse in CASES:
    out, code, calls = run(old_body, new_body, status)
    problems = []
    # A refusal is a non-zero exit. A message printed before sys.exit(0) is not one.
    if refuse and not code:
        problems.append(f"accepted the bump, exit {code!r}")
    if not refuse and code is not None:
        problems.append(f"refused it: {code!r}")
    if want not in out + (code if isinstance(code, str) else ""):
        problems.append(f"missing {want!r}")
    if old_body == new_body and calls:
        problems.append(f"compared {calls} time(s) for an unchanged pin")
    if problems:
        bad += 1
        print(f"FAIL {name}: {'; '.join(problems)}\n{out}{code}")
    else:
        print(f"ok   {name}")

# Pin the count, or deleting a row leaves this green.
if len(CASES) != 11:
    print(f"FAIL expected 11 cases, table has {len(CASES)}")
    bad += 1

print(f"{len(CASES)} cases, {bad} failed")
sys.exit(1 if bad else 0)
