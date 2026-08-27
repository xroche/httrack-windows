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


# HITS matches 3.6.3 and MISSES cannot, so accepting MISSES is a spent acceptance.
HITS = advisory("CVE-2026-11111", "Moderate", "3.6.0", "3.6.4")
MISSES = advisory("CVE-2026-22222", "Moderate", "3.4.0", "3.4.7")


def run(feed, accept):
    """(exit_status, output) of one main() call. Status is None when it returns."""
    check.advisories = lambda: feed
    argv = ["check-native-deps.py", "--openssl-version", "3.6.3", "--min-severity", "moderate"]
    if accept:
        argv += ["--accept", accept]
    out, status = io.StringIO(), None
    with contextlib.redirect_stdout(out):
        sys.argv = argv
        try:
            check.main()
        except SystemExit as e:
            status = e.code
    return status, out.getvalue()


CASES = [
    # (name, feed, accept, want_fail, want_in_output, want_not_in_output)
    ("spent acceptance fails", [MISSES], "CVE-2026-22222", True,
     "CVE-2026-22222 is accepted but no longer applies", None),
    ("live acceptance passes", [HITS], "CVE-2026-11111", False,
     "accepted by policy", "no longer applies"),
    ("unaccepted advisory still blocks", [HITS], "", True,
     "affected by CVE-2026-11111", "no longer applies"),
    ("clean feed passes", [MISSES], "", False, "clean at >=", None),
    # One live, one spent: the spent half must still fail, and name only itself.
    ("live plus spent fails on the spent one", [HITS, MISSES],
     "CVE-2026-11111,CVE-2026-22222", True,
     "CVE-2026-22222 is accepted but no longer applies",
     "CVE-2026-11111 is accepted but no longer applies"),
    ("case and spacing are normalised", [MISSES], " cve-2026-22222 ", True,
     "CVE-2026-22222 is accepted but no longer applies", None),
]

bad = 0
for name, feed, accept, want_fail, want_in, want_not in CASES:
    status, out = run(feed, accept)
    failed = status not in (None, 0)
    problems = []
    if failed != want_fail:
        problems.append(f"expected {'failure' if want_fail else 'success'}, got status {status!r}")
    if want_in not in out:
        problems.append(f"missing {want_in!r}")
    if want_not and want_not in out:
        problems.append(f"unexpected {want_not!r}")
    if problems:
        bad += 1
        print(f"FAIL {name}: {'; '.join(problems)}\n{out}")
    else:
        print(f"ok   {name}")

print(f"{len(CASES)} cases, {bad} failed")
sys.exit(1 if bad else 0)
