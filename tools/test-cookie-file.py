#!/usr/bin/env python3
"""Compile and run tools/cookie-file-test.cpp, then check what no C code here can check
about itself: that the lengths the writer holds a cookie against are still the engine's,
that --selftest still runs the same cases, and that both callers pin the same count.

The GUI only builds under MSVC, so the --selftest run cannot be rerun in review and cannot
be mutated in a pull request. This is the half that runs on ubuntu.

The caps are the engine's, so the engine has to be checked out. --engine says where, and
defaults to the sibling directory CI uses.
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

EXPECTED_LINE = "cookie file ok on 53 checks"

# Where each cap comes from in the engine, as a regex over src/htsbauth.c. cookie_add()
# refuses a longer field with nothing but a log line, so the writer has to refuse it first.
ADD_REFUSALS = {
    "name": r"strlen\(cook_name\)\s*>\s*(\d+)",
    "value": r"strlen\(cook_value\)\s*>\s*(\d+)",
    "path": r"strlen\(path\)\s*>\s*(\d+)",
    "domain": r"strlen\(domain\)\s*>\s*(\d+)",
}


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def one(pattern, text, where):
    m = re.search(pattern, text, re.M)
    if m is None:
        sys.exit("%s does not carry %s any more" % (where, pattern))
    return int(m.group(1))


def engine_caps(bauth_c):
    """The longest each field may be before the engine drops the cookie."""
    caps = {
        k: one(p, bauth_c, "the engine's htsbauth.c") for k, p in ADD_REFUSALS.items()
    }
    # cookie_add() normalises the domain through cookie_host() first, which refuses a host
    # that does not fit its buffer (len >= dst_size) rather than truncating it. So the
    # buffer, not the refusal above, is what caps a domain.
    host = one(r"char host\[(\d+)\];", bauth_c, "the engine's htsbauth.c")
    caps["domain"] = min(caps["domain"], host - 1)
    # cookie_load() skips a whole line this long or longer.
    caps["line"] = one(
        r"const size_t line_max\s*=\s*(\d+);", bauth_c, "the engine's htsbauth.c"
    )
    return caps


def header_caps(header):
    """What cookie-file.h holds a cookie against."""
    names = {
        "domain": "COOKIE_DOMAIN_MAXLEN",
        "name": "COOKIE_NAME_MAXLEN",
        "value": "COOKIE_VALUE_MAXLEN",
        "path": "COOKIE_PATH_MAXLEN",
        "line": "COOKIE_LINE_MAXLEN",
    }
    out = {}
    for key, macro in names.items():
        out[key] = one(r"^#\s*define\s+%s\s+(\d+)\s*$" % macro, header, "cookie-file.h")
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--engine",
        default=os.path.join(os.path.dirname(ROOT), "httrack"),
        help="the engine checkout holding src/htsbauth.c",
    )
    ap.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = ap.parse_args()

    bauth = os.path.join(args.engine, "src", "htsbauth.c")
    if not os.path.exists(bauth):
        sys.exit(
            "%s: no engine source, so the caps cannot be the engine's. Check the engine "
            "out, or pass --engine." % bauth
        )

    checked = 0
    bauth_c = read(bauth)
    header = read(os.path.join(ROOT, "WinHTTrack", "cookie-file.h"))

    # 1. Every cap the writer enforces is the engine's own refusal point.
    engine = engine_caps(bauth_c)
    ours = header_caps(header)
    if engine != ours:
        sys.exit(
            "the caps in cookie-file.h no longer match the engine: ours %r, the engine's "
            "%r. A cookie longer than the engine accepts would be written and then "
            "dropped with only a log line." % (ours, engine)
        )
    checked += 1

    # 2. The comparison above has to be able to fail. Doctoring the real source and
    #    re-running the real parse is the control; comparing two literals would not be one.
    doctored = re.sub(
        ADD_REFUSALS["value"], "strlen(cook_value) > 4096", bauth_c, count=1
    )
    if doctored == bauth_c:
        sys.exit("the value refusal could not be doctored, so check 1 proves nothing")
    if engine_caps(doctored) == ours:
        sys.exit(
            "a doctored engine cap still compared equal, so check 1 proves nothing"
        )
    checked += 1

    # 3. --selftest has to still run the cases, or the Windows half stopped being checked.
    selftest = read(os.path.join(ROOT, "WinHTTrack", "WinHTTrack.cpp"))
    if "cookieFileCheckCases(&err)" not in selftest:
        sys.exit(
            "WinHTTrack.cpp no longer calls cookieFileCheckCases(): --selftest stopped "
            "running the cookie-file cases"
        )
    checked += 1

    # 4. The two expected counts have to agree, or one of them stops being a pin.
    want = re.search(r"cookie file ran %d checks, expected (\d+)", selftest)
    mine = re.search(
        r"define CF_EXPECTED_CHECKS (\d+)",
        read(os.path.join(ROOT, "tools", "cookie-file-test.cpp")),
    )
    if want is None or mine is None:
        sys.exit("cannot read the expected check count from both callers")
    if want.group(1) != mine.group(1) or mine.group(1) not in EXPECTED_LINE:
        sys.exit(
            "the expected check count differs between --selftest (%s), this harness (%s) "
            "and the line CI matches (%s)"
            % (want.group(1), mine.group(1), EXPECTED_LINE)
        )
    checked += 1

    # 5. The count has to be the accumulator. A literal returned there would satisfy both
    #    pins with every case deleted, and no run of the C could tell the difference.
    cases = read(os.path.join(ROOT, "WinHTTrack", "cookie-file.cpp"))
    if "\n  return nchecks;\n}\n" not in cases:
        sys.exit(
            "cookieFileCheckCases() no longer ends by returning nchecks: the printed "
            "count would hold whether or not the cases ran"
        )
    checked += 1

    print("%d source checks pass" % checked)

    with tempfile.TemporaryDirectory() as tmp:
        exe = os.path.join(tmp, "cookie-file-test")
        subprocess.run(
            [
                args.cxx,
                "-std=c++14",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-o",
                exe,
                os.path.join(ROOT, "tools", "cookie-file-test.cpp"),
                os.path.join(ROOT, "WinHTTrack", "cookie-file.cpp"),
                "-DCOOKIE_FILE_TEST",
                "-I" + os.path.join(ROOT, "tools"),
                "-I" + os.path.join(ROOT, "WinHTTrack"),
            ],
            check=True,
        )
        # stdout only: capturing stderr too would swallow the harness's own FAIL line
        # into the traceback, leaving a reader the count but not which case broke.
        out = subprocess.run(
            [exe], check=True, stdout=subprocess.PIPE, text=True
        ).stdout
        # The count is printed and has to be matched, or coverage can shrink in silence.
        if EXPECTED_LINE not in out:
            sys.exit("the harness printed %r, expected %r" % (out, EXPECTED_LINE))
        sys.stdout.write(out)


if __name__ == "__main__":
    main()
