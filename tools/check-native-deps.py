#!/usr/bin/env python3
"""Fail if a shipped native dependency is affected by a published vendor advisory.

Reads the resolved versions out of vcpkg's SPDX SBOMs, then checks OpenSSL's own
CVE-5.x feed. Exits non-zero on an advisory at or above --min-severity.

Only OpenSSL is checked against a CVE feed: no equivalent per-version machine-
readable feed exists for zlib, brotli or zstd (see findings/vcpkg-staleness-guard.md).
Those are reported for human review, never gated.
"""
import argparse
import json
import re
import sys
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

FEED = "https://openssl-library.org/news/secjson/"
RANK = {"low": 1, "moderate": 2, "high": 3, "critical": 4}
UNGATED = ("zlib", "brotli", "zstd")


def get(url):
    with urllib.request.urlopen(url, timeout=60) as r:
        return r.read()


def vkey(v):
    """OpenSSL version string to a comparable tuple, or None if unparseable."""
    m = re.match(r"^(\d+)\.(\d+)\.(\d+)([a-z]*)$", (v or "").strip())
    if not m:
        return None
    a, b, c, suf = m.groups()
    return (int(a), int(b), int(c), len(suf), tuple(map(ord, suf)))


def branch(v):
    """(major, minor) of a version string, or None. Tolerates unparseable tails
    like '3.6.0-beta1', so a same-branch bound can be recognised even when vkey fails."""
    m = re.match(r"^(\d+)\.(\d+)", (v or "").strip())
    return (int(m.group(1)), int(m.group(2))) if m else None


def resolved_versions(installed_dirs):
    """port -> version, read from vcpkg's per-package SPDX SBOMs.

    Recursive on purpose. MSBuild integration installs into
    vcpkg_installed/<triplet>/<triplet>/share/..., one level deeper than the CLI
    (microsoft/vcpkg#23306), so no fixed glob depth is right for both layouts.
    """
    found = {}
    for root in installed_dirs:
        # Fail closed on a missing or empty tree: a vanished engine layout would
        # otherwise scan only the other tree and pass, checking the wrong binaries.
        if not Path(root).is_dir():
            sys.exit(f"FATAL: --installed path '{root}' is missing -- partial scan is not clean")
        n = 0
        for sbom in Path(root).rglob("vcpkg.spdx.json"):
            # Build intermediates, not the installed tree that ships.
            if {"pkgs", "buildtrees", "packages"} & set(sbom.parts):
                continue
            doc = json.loads(sbom.read_text(encoding="utf-8"))
            port = sbom.parent.name
            for pkg in doc.get("packages", []):
                if pkg.get("SPDXID") == "SPDXRef-port":
                    ver = (pkg.get("versionInfo") or "").split("#")[0].strip()
                    if ver:
                        found.setdefault(port, ver)
                        n += 1
        if n == 0:
            sys.exit(f"FATAL: no SBOMs under '{root}' -- extraction is broken, not clean")
    return found


def advisories():
    """Every OpenSSL CVE record. The HTML listing is the only discovery path."""
    html = get(FEED).decode("utf-8", "replace")
    names = sorted(set(re.findall(r'href="(cve-\d{4}-\d+\.json)"', html)))
    # A layout change would yield an empty scan and a vacuous pass.
    if len(names) < 200:
        sys.exit(f"FATAL: advisory index yielded {len(names)} records, expected 200+")
    with ThreadPoolExecutor(max_workers=8) as ex:
        return list(ex.map(lambda n: json.loads(get(FEED + n)), names))


def hits(doc, ver, ver_branch):
    """(cve, severity) if ver is in an affected range, else None. severity is None
    when the record carries none in the expected place.

    Fails closed on a same-branch shape it cannot evaluate -- an open-ended affected
    range, or an unparseable bound on ver's own branch -- rather than skipping it, so
    a feed-shape drift surfaces as a finding instead of a silent clean. Cross-branch
    unparseable bounds (ancient 0.9.x/fips entries) still skip: fail-closing there
    would flag every modern version against pre-1.0 CVEs."""
    cna = doc.get("containers", {}).get("cna", {})
    cve = doc.get("cveMetadata", {}).get("cveId")
    sev = None
    for m in cna.get("metrics", []):
        text = m.get("other", {}).get("content", {}).get("text")
        if text:
            sev = text.strip().lower()  # the feed mixes 'Low' and 'LOW'
    for aff in cna.get("affected", []):
        if (aff.get("vendor"), aff.get("product")) != ("OpenSSL", "OpenSSL"):
            continue
        for rng in aff.get("versions", []):
            if rng.get("status") != "affected":
                continue
            lo_raw = rng.get("version")
            lo = vkey(lo_raw)
            if lo is None:
                # Unparseable lower bound: suspicious only if it names ver's branch.
                if branch(lo_raw) == ver_branch:
                    return cve, sev
                continue
            if not lo <= ver:
                continue
            hi_raw, hie_raw = rng.get("lessThan"), rng.get("lessThanOrEqual")
            if hi_raw:
                hi = vkey(hi_raw)
                if hi is None:
                    if branch(hi_raw) == ver_branch:
                        return cve, sev
                    continue
                if ver < hi:
                    return cve, sev
                continue
            if hie_raw:
                # CVE-2022-3996 is the one record bounding inclusively.
                hie = vkey(hie_raw)
                if hie is None:
                    if branch(hie_raw) == ver_branch:
                        return cve, sev
                    continue
                if ver <= hie:
                    return cve, sev
                continue
            # lo <= ver with no upper bound at all: open-ended affected range.
            return cve, sev
    return None


def main():
    ap = argparse.ArgumentParser()
    # Build gate reads SBOMs from an installed tree; the weekly scan has no build and
    # passes a bare version resolved from the pinned baseline. Exactly one is required.
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--installed", nargs="+",
                     help="vcpkg_installed directories to scan")
    src.add_argument("--openssl-version",
                     help="check this OpenSSL version directly, without a build")
    ap.add_argument("--min-severity", default="moderate", choices=RANK)
    ap.add_argument("--expect-openssl",
                    help="version the pinned baseline promised; mismatch is fatal")
    ap.add_argument("--accept", default="", help="comma-separated CVE IDs to accept")
    args = ap.parse_args()

    if args.openssl_version:
        ossl = args.openssl_version.strip()
        print(f"  checking openssl {ossl}")
    else:
        versions = resolved_versions(args.installed)
        if not versions:
            sys.exit("FATAL: no vcpkg SBOMs found -- extraction is broken, not clean")
        for port, ver in sorted(versions.items()):
            print(f"  resolved {port} {ver}")
        ossl = versions.get("openssl")
        if not ossl:
            sys.exit("FATAL: openssl not found in the installed tree")

    if args.expect_openssl and args.expect_openssl != ossl:
        sys.exit(f"FATAL: baseline promised openssl {args.expect_openssl}, "
                 f"installed tree has {ossl}")

    if args.installed:
        for port in UNGATED:
            if port in versions:
                print(f"::notice::{port} {versions[port]} is not CVE-checked "
                      f"(no machine-readable upstream feed); review manually")

    ver = vkey(ossl)
    if ver is None:
        sys.exit(f"FATAL: cannot parse openssl version {ossl!r}")
    ver_branch = (ver[0], ver[1])

    accepted = {c.strip().upper() for c in args.accept.split(",") if c.strip()}
    floor = RANK[args.min_severity]
    blocking, noted = [], []
    for doc in advisories():
        h = hits(doc, ver, ver_branch)
        if not h:
            continue
        cve, sev = h
        if cve.upper() in accepted:
            print(f"::warning::{cve} ({sev}) accepted by policy")
        # sev is None only when a matched record lacks severity where OpenSSL always
        # puts it -- an unexpected shape, so block rather than silently downgrade.
        elif sev is None or RANK.get(sev, 0) >= floor:
            blocking.append((cve, sev or "unrated"))
        else:
            noted.append((cve, sev))

    for cve, sev in sorted(noted):
        print(f"::warning::openssl {ossl} affected by {cve} ({sev}), below threshold")
    for cve, sev in sorted(blocking):
        print(f"::error::openssl {ossl} affected by {cve} ({sev})")

    if blocking:
        sys.exit(f"{len(blocking)} advisory(ies) at or above {args.min_severity}")
    print(f"openssl {ossl}: clean at >= {args.min_severity} "
          f"({len(noted)} below threshold)")


if __name__ == "__main__":
    main()
