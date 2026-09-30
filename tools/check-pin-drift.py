#!/usr/bin/env python3
"""Report how each pinned vcpkg baseline differs from upstream, and from the others.

Reads baseline.json files already fetched, so it needs no network. Two pins are
given because the installer ships DLLs from both the GUI's and the engine's, and
watching one covers the other only while they happen to be equal.
"""
import argparse
import json
import sys

PORTS = ("openssl", "zlib", "brotli", "zstd")


def ports_of(path):
    """port -> pinned version, from a vcpkg versions/baseline.json."""
    return json.load(open(path, encoding="utf-8"))["default"]


def report(pins, upstream, lines):
    """Append the drift report to lines. Returns port version -> the pins carrying it.

    pins is a list of (name, sha, baseline path), in the order to report them.
    A line carrying the word DRIFT is what the caller greps for.
    """
    shas = {sha for _, sha, _ in pins}
    if len(shas) > 1:
        named = ", ".join(f"{n} {s[:12]}" for n, s, _ in pins)
        lines.append(f"the pins differ: {named}   <-- DRIFT")

    head = ports_of(upstream)
    versions = {}
    for name, sha, path in pins:
        pinned = ports_of(path)
        lines.append(f"--- {name} pin {sha[:12]} ---")
        for port in PORTS:
            a = pinned.get(port, {}).get("baseline")
            b = head.get(port, {}).get("baseline")
            lines.append(f"{port}: pinned {a}, upstream {b}" + ("" if a == b else "   <-- DRIFT"))
        openssl = pinned.get("openssl", {}).get("baseline")
        if not openssl:
            sys.exit(f"FATAL: {path} names no openssl baseline -- a silent skip is not clean")
        versions.setdefault(openssl, []).append(name)
    return versions


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--upstream", required=True, help="vcpkg master baseline.json")
    ap.add_argument("--pin", action="append", required=True, metavar="NAME=SHA:PATH",
                    help="a pinned baseline, repeatable; reported in the order given")
    ap.add_argument("--versions-out",
                    help="write one 'version name+name' line per distinct OpenSSL")
    args = ap.parse_args()

    pins = []
    for spec in args.pin:
        name, _, rest = spec.partition("=")
        sha, _, path = rest.partition(":")
        if not (name and sha and path):
            sys.exit(f"FATAL: --pin wants NAME=SHA:PATH, got {spec!r}")
        pins.append((name, sha, path))

    lines = []
    versions = report(pins, args.upstream, lines)
    print("\n".join(lines))
    if args.versions_out:
        with open(args.versions_out, "w", encoding="utf-8") as fh:
            for version, who in sorted(versions.items()):
                fh.write(f"{version} {'+'.join(who)}\n")


if __name__ == "__main__":
    main()
