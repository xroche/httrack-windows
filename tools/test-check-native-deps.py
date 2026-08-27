#!/usr/bin/env python3
"""Table test for the accept/stale logic in check-native-deps.py.

Stubs the advisory feed, so it needs no network and no vcpkg tree.
"""
import contextlib
import importlib.util
import io
import sys
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
if len(CASES) != 8:
    print(f"FAIL expected 8 cases, table has {len(CASES)}")
    bad += 1

print(f"{len(CASES)} cases, {bad} failed")
sys.exit(1 if bad else 0)
