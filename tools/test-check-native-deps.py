#!/usr/bin/env python3
"""Table test for the accept/stale logic in check-native-deps.py.

Stubs both feeds, so it needs no network and no vcpkg tree.
"""
import contextlib
import importlib.util
import io
import json
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
            except Exception as e:
                status = f"raised {type(e).__name__}: {e}"
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
    # This mode scans OpenSSL alone and shares the one accept list, so an entry for a
    # port it cannot see must not read as spent. It did before the list was scoped.
    ("an unscanned port's acceptance is not spent", [MISSES], "zlib=CVE-2026-76844",
     None, "clean at >=", "no longer applies"),
    ("the openssl half of a mixed list is still judged", [MISSES],
     "zlib=CVE-2026-76844,CVE-2026-22222", "1 spent acceptance(s)",
     "CVE-2026-22222 is accepted but no longer applies to openssl 3.6.3", None),
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
if len(CASES) != 14:
    print(f"FAIL expected 14 cases, table has {len(CASES)}")
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


def record(ident, aliases=None, withdrawn=None):
    v = {"id": ident}
    if aliases:
        v["aliases"] = aliases
    if withdrawn:
        v["withdrawn"] = withdrawn
    return v


def run_installed(tree_versions, osv, accept):
    """(exit status, output, the ports OSV was asked about). Status is None on success."""
    asked = []

    def stub(port, version):
        asked.append(port)
        return osv.get(port, [])

    real_osv, real_adv, real_argv = check.osv_query, check.advisories, sys.argv
    out, status = io.StringIO(), None
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        for port, ver in tree_versions.items():
            d = root / "vcpkg_installed" / "x64-windows" / "x64-windows" / "share" / port
            d.mkdir(parents=True)
            (d / "vcpkg.spdx.json").write_text(
                '{"packages":[{"SPDXID":"SPDXRef-port","versionInfo":"%s"}]}' % ver,
                encoding="utf-8")
        argv = ["check-native-deps.py", "--installed", str(root / "vcpkg_installed")]
        if accept:
            argv += ["--accept", accept]
        try:
            check.osv_query, check.advisories, sys.argv = stub, lambda: [], argv
            with contextlib.redirect_stdout(out):
                try:
                    check.main()
                except SystemExit as e:
                    status = e.code
                except Exception as e:
                    status = f"raised {type(e).__name__}: {e}"
        finally:
            check.osv_query, check.advisories, sys.argv = real_osv, real_adv, real_argv
    return status, out.getvalue(), asked


INSTALLED = {"openssl": "3.6.5", "zlib": "1.3.2", "brotli": "1.2.1", "zstd": "1.5.7"}
BROTLI_HIT = {"brotli": [record("CVE-2026-111497")]}
# zstd's real record carries no CVE, so the accept list has to take an OSV id too.
NOCVE = {"zstd": [record("OSV-2022-15")]}
# OSV keys the record on its own id and lists the CVE as an alias for some ecosystems.
ALIASED = {"zlib": [record("GHSA-xxxx-yyyy-zzzz", ["CVE-2026-76844", "PYSEC-2026-1"])]}
# zlib really does carry two, which is what the accept list holds the day this lands.
ZLIB_PAIR = {"zlib": [record("CVE-2026-76844"), record("CVE-2026-85091")]}

OSV_CASES = [
    # (name, osv, accept, want_status, want_in_output, want_not_in_output)
    ("an OSV hit blocks", BROTLI_HIT, "", "1 OSV advisory(ies)",
     "::error::brotli 1.2.1 affected by CVE-2026-111497", "accepted by policy"),
    ("an accepted OSV hit passes and is counted", BROTLI_HIT, "brotli=CVE-2026-111497",
     None, "clean at >= moderate (0 below threshold, 1 accepted)", "::error::"),
    ("a record with no CVE blocks under its OSV id", NOCVE, "", "1 OSV advisory(ies)",
     "::error::zstd 1.5.7 affected by OSV-2022-15", None),
    ("a record with no CVE is acceptable by that id", NOCVE, "zstd=OSV-2022-15", None,
     "OSV-2022-15 (zstd) accepted by policy", "::error::"),
    # Keying the acceptance on the record id would make the CVE in the list read as spent.
    ("an aliased record answers to its CVE", ALIASED, "zlib=CVE-2026-76844", None,
     "CVE-2026-76844 (zlib) accepted by policy", "::error::"),
    # A bare id means openssl, so it must not quietly waive another port's finding.
    ("a bare id does not accept a brotli finding", BROTLI_HIT, "CVE-2026-111497",
     "1 OSV advisory(ies)", "::error::brotli 1.2.1 affected by CVE-2026-111497", None),
    ("an acceptance for the wrong port does not apply", BROTLI_HIT, "zlib=CVE-2026-111497",
     "1 OSV advisory(ies)", "::error::brotli 1.2.1 affected by CVE-2026-111497", None),
    ("two acceptances on one port are counted separately", ZLIB_PAIR,
     "zlib=CVE-2026-76844,zlib=CVE-2026-85091", None,
     "clean at >= moderate (0 below threshold, 2 accepted)", "::error::"),
    ("a port name is matched case-insensitively", BROTLI_HIT, " BroTLi = cve-2026-111497 ",
     None, "CVE-2026-111497 (brotli) accepted by policy", "::error::"),
    ("no records at all passes", {}, "", None, "clean at >=", "::error::"),
    # An acceptance for a port OSV no longer flags is spent, same as for OpenSSL.
    ("a spent OSV acceptance fails", {}, "brotli=CVE-2026-111497", "1 spent acceptance(s)",
     "CVE-2026-111497 is accepted but no longer applies to brotli 1.2.1", None),
]

for name, osv, accept, want_status, want_in, want_not in OSV_CASES:
    status, out, asked = run_installed(INSTALLED, osv, accept)
    problems = []
    if (status not in (None, 0)) != (want_status is not None):
        problems.append(f"expected {'failure' if want_status else 'success'}, got {status!r}")
    elif want_status and want_status not in str(status):
        problems.append(f"exit reason {str(status)!r} lacks {want_status!r}")
    if want_in not in out:
        problems.append(f"missing {want_in!r}")
    if want_not and want_not in out:
        problems.append(f"unexpected {want_not!r}")
    # Dropping a port from OSV_REPOS would leave every case above green.
    if sorted(asked) != ["brotli", "zlib", "zstd"]:
        problems.append(f"queried {sorted(asked)}, expected brotli, zlib and zstd")
    if problems:
        bad += 1
        print(f"FAIL {name}: {'; '.join(problems)}\n{out}")
    else:
        print(f"ok   {name}")

if len(OSV_CASES) != 11:
    print(f"FAIL expected 11 OSV cases, table has {len(OSV_CASES)}")
    bad += 1

# A gated port whose SBOM never turned up was scanned by nobody and still read as
# clean, which is how zlib and brotli could both be dropped from a green run.
status, out, asked = run_installed({"openssl": "3.6.5", "zstd": "1.5.7"}, osv={}, accept="")
if ("no SBOM resolved for brotli, zlib" not in str(status)
        or "clean at >=" in out or asked):
    print(f"FAIL a missing SBOM is fatal: status {status!r} asked {asked}\n{out}")
    bad += 1
else:
    print("ok   a missing SBOM is fatal and queries nothing")

# webview2 has no OSV coverage, so it must be reported and never gated.
status, out, asked = run_installed(
    dict(INSTALLED, webview2="1.0.4191.47"), osv={}, accept="")
if "::notice::webview2 1.0.4191.47 is not CVE-checked" not in out or status not in (None, 0):
    print(f"FAIL webview2 is noted and not gated: status {status!r}\n{out}")
    bad += 1
else:
    print("ok   webview2 is noted and not gated")


def query(payload, raises=None):
    """(records, exit reason, the Request sent). reason is None when it returned."""
    sent = []

    def fake(req, *a, **k):
        sent.append(req)
        if raises:
            raise raises
        return io.BytesIO(json.dumps(payload).encode())

    real = check.urllib.request.urlopen
    try:
        check.urllib.request.urlopen = fake
        try:
            return check.osv_query("brotli", "1.2.0"), None, sent
        except SystemExit as e:
            return None, str(e.code), sent
        except Exception as e:
            return None, f"raised {type(e).__name__}: {e}", sent
    finally:
        check.urllib.request.urlopen = real


LIVE = record("CVE-2026-111497")
DEAD = record("CVE-2026-99999", withdrawn="2026-10-25T00:00:00Z")

QUERIES = [
    # (name, payload, raises, want_ids, want_reason)
    ("a live record is returned", {"vulns": [LIVE]}, None, ["CVE-2026-111497"], None),
    ("a withdrawn record is dropped", {"vulns": [DEAD]}, None, [], None),
    ("a withdrawn record beside a live one drops alone",
     {"vulns": [LIVE, DEAD]}, None, ["CVE-2026-111497"], None),
    ("an answer with no vulns key is empty", {}, None, [], None),
    ("a vulns key that is not a list is fatal", {"vulns": {}}, None, None, "no vulns list"),
    # Any error identity must fail closed, or a timeout reads as a port with no records.
    ("a transport error is fatal", {}, OSError("connection reset"), None,
     "OSV query for brotli 1.2.0 failed"),
    ("a body that is not JSON is fatal", {}, ValueError("no json"), None, "failed"),
]

for name, payload, raises, want_ids, want_reason in QUERIES:
    got, reason, sent = query(payload, raises)
    problems = []
    if want_reason:
        if reason is None:
            problems.append(f"expected a fatal exit, got {got!r}")
        elif want_reason not in reason:
            problems.append(f"exit reason {reason!r} lacks {want_reason!r}")
    elif reason is not None:
        problems.append(f"unexpected fatal exit: {reason!r}")
    elif [v["id"] for v in got] != want_ids:
        problems.append(f"returned {[v['id'] for v in got]}, expected {want_ids}")
    # The old fake ignored the Request, so asking OSV about the wrong version or the
    # wrong repository stayed green. The whole gate rests on it being the right one.
    if len(sent) != 1:
        problems.append(f"sent {len(sent)} requests, expected 1")
    else:
        req = sent[0]
        body = json.loads(req.data)
        if req.full_url != "https://api.osv.dev/v1/query":
            problems.append(f"posted to {req.full_url!r}, not OSV's query endpoint")
        if body.get("version") != "1.2.0":
            problems.append(f"asked about version {body.get('version')!r}, expected '1.2.0'")
        if body.get("package", {}).get("name") != check.OSV_REPOS["brotli"]:
            problems.append(f"asked about {body.get('package')!r}")
        if body.get("package", {}).get("ecosystem") != "GIT":
            problems.append(f"ecosystem {body.get('package', {}).get('ecosystem')!r}")
        if req.get_header("Content-type") != "application/json":
            problems.append("the request carries no application/json content type")
    if problems:
        bad += 1
        print(f"FAIL {name}: {'; '.join(problems)}")
    else:
        print(f"ok   {name}")

if len(QUERIES) != 7:
    print(f"FAIL expected 7 query cases, table has {len(QUERIES)}")
    bad += 1


# A port no scanner knows is fatal, so a typo cannot sit in the list accepting nothing.
status, out, asked = run_installed(INSTALLED, osv={}, accept="libpng=CVE-2026-1")
if "names port(s) nothing scans: libpng" not in str(status):
    print(f"FAIL a port nothing scans is fatal: status {status!r}\n{out}")
    bad += 1
else:
    print("ok   a port nothing scans is fatal")

IDS = [
    ("a bare CVE record", {"id": "CVE-2026-111497"}, "CVE-2026-111497"),
    ("an OSV id with no CVE", {"id": "OSV-2022-15"}, "OSV-2022-15"),
    ("a CVE alias wins over the record id",
     {"id": "GHSA-a", "aliases": ["PYSEC-1", "CVE-2026-1"]}, "CVE-2026-1"),
    ("the lowest of two CVE aliases wins",
     {"id": "GHSA-a", "aliases": ["CVE-2026-9", "CVE-2026-1"]}, "CVE-2026-1"),
    # Without the CVE- filter any alias would do, and this record's first one is not a CVE.
    ("a non-CVE alias does not win",
     {"id": "GHSA-a", "aliases": ["BIT-2026-1", "CVE-2026-9"]}, "CVE-2026-9"),
    ("a lowercase id is normalised", {"id": "cve-2026-1"}, "CVE-2026-1"),
]

for name, vuln, want in IDS:
    got = check.osv_id(vuln)
    if got != want:
        print(f"FAIL {name}: osv_id gave {got!r}, expected {want!r}")
        bad += 1
    else:
        print(f"ok   {name}")

if len(IDS) != 6:
    print(f"FAIL expected 6 id cases, table has {len(IDS)}")
    bad += 1

# The 4 is the checks outside the tables; drop one and this total lies.
total = len(CASES) + len(TREES) + len(OSV_CASES) + len(QUERIES) + len(IDS) + 4
print(f"{total} cases, {bad} failed")
sys.exit(1 if bad else 0)
