#!/usr/bin/env python3
"""Refuse a vcpkg builtin-baseline that does not move forward.

Dependabot's vcpkg ecosystem tracks release TAGS, so a baseline hand-picked as a
commit reads as behind the newest tag and gets "upgraded" down to it. That is how
pull request #202 put the GUI's OpenSSL back from 3.6.4 to 3.6.3, auto-merged and
unread.
"""
import argparse
import json
import os
import re
import sys
import urllib.request

API = "https://api.github.com/repos/microsoft/vcpkg/compare/"
SHA = re.compile(r"\A[0-9a-f]{40}\Z")


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
    """How microsoft/vcpkg relates the new commit to the old one, or None when unreadable."""
    req = urllib.request.Request(f"{API}{old}...{new}")
    token = os.environ.get("GITHUB_TOKEN", "")
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    try:
        with urllib.request.urlopen(req, timeout=30) as fh:
            return json.load(fh).get("status")
    except (OSError, ValueError) as e:
        print(f"::warning::cannot compare {old[:12]}..{new[:12]}: {e}")
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--old", required=True, help="the manifest as the merge target has it")
    ap.add_argument("--new", required=True, help="the manifest this change proposes")
    args = ap.parse_args()

    old, new = baseline_of(args.old), baseline_of(args.new)
    # The common case, and the one that keeps this off the network.
    if old == new:
        print(f"vcpkg baseline unchanged at {old[:12]}")
        return

    status = compare_status(old, new)
    # An answer we cannot read must not pass for a descendant.
    if status is None:
        print(f"::error::cannot tell whether {new[:12]} descends from {old[:12]}")
        sys.exit(1)
    if status != "ahead":
        print(f"::error::the vcpkg baseline goes backwards: {new[:12]} reads {status} "
              f"relative to {old[:12]}, so every port version the old baseline "
              f"carried is given up, OpenSSL's included")
        sys.exit(1)
    print(f"vcpkg baseline moves {old[:12]} -> {new[:12]} (ahead)")


if __name__ == "__main__":
    main()
