#!/usr/bin/env python3
"""Refuse a vcpkg builtin-baseline that does not move forward.

Dependabot's vcpkg ecosystem tracks release TAGS, so a baseline hand-picked as a
commit reads as behind the newest tag and gets "upgraded" down to it. That is how
#202 put the GUI's OpenSSL back from 3.6.4 to 3.6.3, auto-merged and unread.
"""
import argparse
import json
import os
import re
import sys
import urllib.request

API = "https://api.github.com/repos/microsoft/vcpkg/compare/"
SHA = re.compile(r"\A[0-9a-f]{40}\Z")
FORWARD = ("identical", "ahead")
# A live pair with a known answer each way, so an API that stops saying what we read
# reads as broken rather than as clean.
CONTROL_OLD = "04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4"
CONTROL_NEW = "e182cb4dd2df2ab02f66a1aabd5f35bbdc9522c7"


def baseline_of(path):
    """The builtin-baseline a vcpkg manifest pins, as a full commit sha."""
    try:
        text = open(path, encoding="utf-8").read()
    except OSError as e:
        sys.exit(f"FATAL: cannot read {path}: {e}")
    try:
        pinned = json.loads(text).get("builtin-baseline")
    except json.JSONDecodeError as e:
        sys.exit(f"FATAL: {path} is not valid JSON: {e}")
    sha = (pinned or "").strip()
    if not SHA.match(sha):
        sys.exit(f"FATAL: {path} pins builtin-baseline {sha!r}, expected a 40-hex commit")
    return sha


def compare_status(old, new):
    """How microsoft/vcpkg sees new relative to old, or None when it cannot be read."""
    req = urllib.request.Request(
        f"{API}{old}...{new}", headers={"Accept": "application/vnd.github+json"}
    )
    token = os.environ.get("GITHUB_TOKEN", "")
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    try:
        with urllib.request.urlopen(req, timeout=30) as fh:
            return json.load(fh).get("status")
    except (OSError, ValueError) as e:
        print(f"::warning::cannot compare {old[:12]}..{new[:12]}: {e}")
        return None


def verdict(old, new, status):
    """Why to refuse the new baseline, or None when it moves forward."""
    if old == new:
        return None
    if status is None:
        return f"cannot tell whether {new[:12]} descends from {old[:12]}"
    if status not in FORWARD:
        return (f"the vcpkg baseline goes backwards: {new[:12]} reads {status} "
                f"relative to {old[:12]}, so every port version the old baseline "
                f"carried is given up, OpenSSL's included")
    return None


def selftest():
    """Prove the live endpoint answers both directions before a verdict trusts it."""
    for old, new, want in ((CONTROL_OLD, CONTROL_NEW, "ahead"),
                           (CONTROL_NEW, CONTROL_OLD, "behind")):
        got = compare_status(old, new)
        if got != want:
            sys.exit(f"FATAL: compare {old[:12]}..{new[:12]} reads {got!r}, expected "
                     f"{want!r}: this guard can no longer read vcpkg's history")
    print("vcpkg compare answers ahead and behind as expected")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--old", help="the manifest as the merge target has it")
    ap.add_argument("--new", help="the manifest this change proposes")
    ap.add_argument("--selftest", action="store_true",
                    help="check the compare endpoint against a known pair and exit")
    args = ap.parse_args()

    if args.selftest:
        selftest()
        return
    if not (args.old and args.new):
        sys.exit("FATAL: --old and --new are both required")

    old, new = baseline_of(args.old), baseline_of(args.new)
    if old == new:
        print(f"vcpkg baseline unchanged at {old[:12]}")
        return
    status = compare_status(old, new)
    bad = verdict(old, new, status)
    if bad:
        sys.exit("FATAL: " + bad)
    print(f"vcpkg baseline moves {old[:12]} -> {new[:12]} ({status})")


if __name__ == "__main__":
    main()
