#!/usr/bin/env python3
"""Table test for the accept/stale logic in check-native-deps.py.

Stubs the advisory feed, so it needs no network and no vcpkg tree.
"""
import contextlib
import importlib.util
import io
import sys
import tempfile
from pathlib import Path

MOD = Path(__file__).with_name("check-native-deps.py")
spec = importlib.util.spec_from_file_location("check_native_deps", MOD)
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)


def advisory(cve, severity, lo, less_than):
    return {
        "cveMetadata": {"cveId": cve},
        "containers": {"cna": {
            "metrics": [{"other": {"content": {"text": severity}}}],
            "affected": [{"vendor": "OpenSSL", "product": "OpenSSL",
                          "versions": [{"status": "affected", "version": lo,
                                        "lessThan": less_than, "versionType": "semver"}]}],
        }},
    }


# HITS and LOW both match 3.6.3; MISSES cannot, so accepting MISSES is a spent acceptance.
HITS = advisory("CVE-2026-11111", "Moderate", "3.6.0", "3.6.4")
MISSES = advisory("CVE-2026-22222", "Moderate", "3.4.0", "3.4.7")
LOW = advisory("CVE-2026-33333", "Low", "3.6.0", "3.6.4")
# The feed really says "unknown" on 83 of its 297 records, and a rank table that scored
# it 0 put it under every floor. NOSEV is the shape that was already blocked.
UNKNOWN = advisory("CVE-2026-44444", "unknown", "3.6.0", "3.6.4")
ODD = advisory("CVE-2026-55555", "high (CVSS 7.5)", "3.6.0", "3.6.4")
NOSEV = advisory("CVE-2026-66666", None, "3.6.0", "3.6.4")
del NOSEV["containers"]["cna"]["metrics"]


def run(feed, accept):
    """(exit status, output, stub calls). Status is None when main() returns instead."""
    calls = []

    def stub():
        calls.append(1)
        return feed

    real_advisories, real_argv = check.advisories, sys.argv
    argv = ["check-native-deps.py", "--openssl-version", "3.6.3", "--min-severity", "moderate"]
    if accept:
        argv += ["--accept", accept]
    out, status = io.StringIO(), None
    try:
        check.advisories, sys.argv = stub, argv
        with contextlib.redirect_stdout(out):
            try:
                check.main()
            except SystemExit as e:
                status = e.code
    finally:
        check.advisories, sys.argv = real_advisories, real_argv
    return status, out.getvalue(), len(calls)


CASES = [
    # (name, feed, accept, want_status, want_in_output, want_not_in_output)
    # want_status is a substring of the exit reason, or None when the run must succeed.
    ("spent acceptance fails", [MISSES], "CVE-2026-22222", "1 spent acceptance(s)",
     "CVE-2026-22222 is accepted but no longer applies", None),
    ("live acceptance passes", [HITS], "CVE-2026-11111", None,
     "accepted by policy", "no longer applies"),
    ("unaccepted advisory still blocks", [HITS], "", "1 advisory(ies) at or above moderate",
     "affected by CVE-2026-11111", "no longer applies"),
    ("clean feed passes", [MISSES], "", None, "clean at >=", None),
    # One live, one spent: the spent half must still fail, and name only itself.
    ("live plus spent fails on the spent one", [HITS, MISSES],
     "CVE-2026-11111,CVE-2026-22222", "1 spent acceptance(s)",
     "CVE-2026-22222 is accepted but no longer applies",
     "CVE-2026-11111 is accepted but no longer applies"),
    ("case and spacing are normalised", [MISSES], " cve-2026-22222 ", "1 spent acceptance(s)",
     "CVE-2026-22222 is accepted but no longer applies", None),
    # Acceptance is judged before severity, so a live one below the floor is not spent.
    ("below-threshold acceptance is live", [LOW], "CVE-2026-33333", None,
     "accepted by policy", "no longer applies"),
    ("below-threshold advisory only warns", [LOW], "", None, "1 below threshold", "::error::"),
    # A severity the table cannot rank is not a low one. Each of these passed the gate
    # with a warning before, because RANK.get(sev, 0) scored them under the floor.
    ("unrankable severity blocks", [UNKNOWN], "", "1 advisory(ies) at or above moderate",
     "affected by CVE-2026-44444 (unknown)", "below threshold"),
    ("severity with decoration blocks", [ODD], "", "1 advisory(ies) at or above moderate",
     "affected by CVE-2026-55555", "below threshold"),
    ("absent severity still blocks", [NOSEV], "", "1 advisory(ies) at or above moderate",
     "affected by CVE-2026-66666 (unrated)", "below threshold"),
    # Accepting one is still allowed: the fix must not make an unrankable CVE unwaivable.
    ("unrankable severity is acceptable", [UNKNOWN], "CVE-2026-44444", None,
     "accepted by policy", "::error::"),
]

bad = 0
for name, feed, accept, want_status, want_in, want_not in CASES:
    status, out, calls = run(feed, accept)
    problems = []
    if (status not in (None, 0)) != (want_status is not None):
        problems.append(f"expected {'failure' if want_status else 'success'}, got {status!r}")
    elif want_status and want_status not in str(status):
        problems.append(f"exit reason {str(status)!r} lacks {want_status!r}")
    if want_in not in out:
        problems.append(f"missing {want_in!r}")
    if want_not and want_not in out:
        problems.append(f"unexpected {want_not!r}")
    # A refactor that stops calling advisories() would reach the network and still pass.
    if calls != 1:
        problems.append(f"stubbed feed read {calls} times, expected 1")
    if problems:
        bad += 1
        print(f"FAIL {name}: {'; '.join(problems)}\n{out}")
    else:
        print(f"ok   {name}")

# Pin the count, or deleting a row leaves this green.
if len(CASES) != 12:
    print(f"FAIL expected 12 cases, table has {len(CASES)}")
    bad += 1


# resolved_versions() reads a tree rather than the feed, so the table above cannot reach
# it. Two triplets at different versions used to resolve by whichever rglob arrived
# first, and --expect-openssl then agreed with that arbitrary pick.
def tree(root, versions):
    for triplet, ver in versions.items():
        d = root / "vcpkg_installed" / triplet / triplet / "share" / "openssl"
        d.mkdir(parents=True)
        (d / "vcpkg.spdx.json").write_text(
            '{"packages":[{"SPDXID":"SPDXRef-port","versionInfo":"%s"}]}' % ver,
            encoding="utf-8")
    return str(root / "vcpkg_installed")


def resolve(versions):
    """(result, exit reason). reason is None when it resolved instead of exiting."""
    with tempfile.TemporaryDirectory() as td:
        root = tree(Path(td), versions)
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                return check.resolved_versions([root]), None
        except SystemExit as e:
            return None, str(e.code)


TREES = [
    ("one triplet resolves", {"x64-windows": "3.6.4"}, "3.6.4", None),
    ("two triplets agreeing resolve",
     {"x64-windows": "3.6.4", "x86-windows": "3.6.4"}, "3.6.4", None),
    ("two triplets disagreeing is fatal",
     {"x64-windows": "3.6.5", "x86-windows": "3.6.4"}, None, "which one ships is undefined"),
]

for name, versions, want_ver, want_reason in TREES:
    got, reason = resolve(versions)
    problems = []
    if want_reason:
        if reason is None:
            problems.append(f"expected a fatal exit, resolved {got!r}")
        elif want_reason not in reason:
            problems.append(f"exit reason {reason!r} lacks {want_reason!r}")
        elif "3.6.4" not in reason or "3.6.5" not in reason:
            problems.append(f"exit reason names neither version: {reason!r}")
    else:
        if reason is not None:
            problems.append(f"unexpected fatal exit: {reason!r}")
        elif got.get("openssl") != want_ver:
            problems.append(f"resolved {got!r}, expected openssl {want_ver}")
    if problems:
        bad += 1
        print(f"FAIL {name}: {'; '.join(problems)}")
    else:
        print(f"ok   {name}")

if len(TREES) != 3:
    print(f"FAIL expected 3 tree cases, table has {len(TREES)}")
    bad += 1

print(f"{len(CASES) + len(TREES)} cases, {bad} failed")
sys.exit(1 if bad else 0)
