#!/usr/bin/env python3
"""Compile and run tools/browser-login-paths-test.cpp, then check what no C code here can
check about itself: that --selftest still runs the same cases, and that every caller pins
the same count.

The GUI only builds under MSVC, so the --selftest run cannot be rerun in review and cannot
be mutated in a pull request. This is the half that runs on ubuntu.

hts_scan_token() decides which address the browser opens, so the engine has to be checked
out. --engine says where, and defaults to the sibling directory CI uses.
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

EXPECTED_LINE = "browser login helpers ok on 17 checks"


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def engine_token_function(htscore):
    """The engine's hts_scan_token(), copied out rather than reproduced here, so the
    address box ends an address where the crawl ends it."""
    m = re.search(r"^hts_boolean hts_scan_token\(.*?^\}$", htscore, re.M | re.S)
    if m is None:
        sys.exit("no hts_scan_token() in the engine's htscore.c")
    return m.group(0)


def engine_opt_function(htscoremain):
    """rules-split.cpp calls it, so linking the splitter here needs the engine's own
    definition rather than a stub of ours."""
    m = re.search(
        r"^HTSEXT_API hts_boolean hts_rule_would_be_read_as_opt\(const char \*arg\) "
        r"\{.*?^\}$",
        htscoremain,
        re.M | re.S,
    )
    if m is None:
        sys.exit("no hts_rule_would_be_read_as_opt() in the engine's htscoremain.c")
    return m.group(0)


def engine_space_set(htssafe):
    """The byte set that function splits on, copied out for the same reason."""
    m = re.search(r"^#\s*define\s+HTS_REALSPACES\s+[^\n]*$", htssafe, re.M)
    if m is None:
        sys.exit("no HTS_REALSPACES in the engine's htssafe.h")
    return m.group(0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--engine",
        default=os.path.join(os.path.dirname(ROOT), "httrack"),
        help="the engine checkout holding src/htscore.c and src/htssafe.h",
    )
    ap.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = ap.parse_args()

    htscore = os.path.join(args.engine, "src", "htscore.c")
    htscoremain = os.path.join(args.engine, "src", "htscoremain.c")
    htssafe = os.path.join(args.engine, "src", "htssafe.h")
    for path in (htscore, htscoremain, htssafe):
        if not os.path.exists(path):
            sys.exit(
                "%s: no engine source, so the token scanner cannot be the engine's. "
                "Check the engine out, or pass --engine." % path
            )

    checked = 0

    # 1. --selftest has to still run these cases, or the Windows half stopped checking.
    selftest = read(os.path.join(ROOT, "WinHTTrack", "WinHTTrack.cpp"))
    if "browserLoginCheckCases(&err)" not in selftest:
        sys.exit(
            "WinHTTrack.cpp no longer calls browserLoginCheckCases(): --selftest "
            "stopped running the browser-login path cases"
        )
    checked += 1

    # 2. Every count has to agree, or one of them stops being a pin.
    want = re.search(r"browser login helpers ran %d checks, expected (\d+)", selftest)
    mine = re.search(
        r"define BL_EXPECTED_CHECKS (\d+)",
        read(os.path.join(ROOT, "tools", "browser-login-paths-test.cpp")),
    )
    if want is None or mine is None:
        sys.exit("cannot read the expected check count from both callers")
    if want.group(1) != mine.group(1) or mine.group(1) not in EXPECTED_LINE:
        sys.exit(
            "the expected check count differs between --selftest (%s), this harness "
            "(%s) and the line CI matches (%s)"
            % (want.group(1), mine.group(1), EXPECTED_LINE)
        )
    checked += 1

    text = read(os.path.join(ROOT, ".github", "workflows", "windows-build.yml"))
    counts = set(re.findall(r"browser login helpers ok on (\d+) checks", text))
    if not counts:
        sys.exit("windows-build.yml no longer matches the browser-login helper line")
    if counts != {mine.group(1)}:
        sys.exit(
            "windows-build.yml matches %s checks, where the code runs %s"
            % (", ".join(sorted(counts)), mine.group(1))
        )
    checked += 1

    # 3. One definition only: a copy left behind would mean the Windows build and this
    #    run test different code.
    unit = os.path.join(ROOT, "WinHTTrack", "browser-login-paths.cpp")
    for name in (
        "browserLoginStartUrl",
        "browserLoginProfileDir",
        "browserLoginCookieFile",
    ):
        for rel in ("browser-login.cpp", "Shell.cpp", "OptionTab8.cpp"):
            other = read(os.path.join(ROOT, "WinHTTrack", rel))
            if re.search(r"^\w[^;()\n]*\b%s\s*\(" % name, other, re.M):
                sys.exit(
                    "%s is defined in %s as well as browser-login-paths.cpp"
                    % (name, rel)
                )
        if re.search(r"^CString %s\(" % name, read(unit), re.M) is None:
            sys.exit("browser-login-paths.cpp no longer defines %s()" % name)
        checked += 1

    # 4. The button has to call them, or the cases hold for code nothing reaches.
    tab8 = read(os.path.join(ROOT, "WinHTTrack", "OptionTab8.cpp"))
    for name in (
        "browserLoginStartUrl",
        "browserLoginProfileDir",
        "browserLoginCookieFile",
        "browserLoginRun",
    ):
        if name + "(" not in tab8:
            sys.exit(
                "OptionTab8.cpp no longer calls %s(), so the button bypasses it" % name
            )
        checked += 1

    print("%d source checks pass" % checked)

    with tempfile.TemporaryDirectory() as tmp:
        gen = os.path.join(tmp, "engine-token.cpp")
        with open(gen, "w", encoding="utf-8") as f:
            f.write(
                "/* Copied by tools/test-browser-login-paths.py out of the engine's "
                "src/htscore.c and src/htssafe.h. */\n"
                "#include <assert.h>\n#include <ctype.h>\n#include <stddef.h>\n"
                "#include <string.h>\n"
                "typedef int hts_boolean;\n#define HTSEXT_API\n"
                "#define HTS_TRUE 1\n#define HTS_FALSE 0\n"
                "#define assertf(x) assert(x)\n"
                "%s\n"
                'extern "C" {\n%s\n%s\n}\n'
                % (
                    engine_space_set(read(htssafe)),
                    engine_token_function(read(htscore)),
                    engine_opt_function(read(htscoremain)),
                )
            )
        # Built and run twice: once as the Windows build sees it, once with the
        # sanitizers, because a walk that reads one byte past a field passes every case.
        for label, extra in (
            ("plain", []),
            ("sanitized", ["-fsanitize=address,undefined"]),
        ):
            exe = os.path.join(tmp, "browser-login-paths-test-" + label)
            subprocess.run(
                [args.cxx, "-std=c++14", "-Wall", "-Wextra", "-Werror", "-g", "-o", exe]
                + extra
                + [
                    os.path.join(ROOT, "tools", "browser-login-paths-test.cpp"),
                    os.path.join(ROOT, "WinHTTrack", "browser-login-paths.cpp"),
                    os.path.join(ROOT, "WinHTTrack", "cookie-file.cpp"),
                    os.path.join(ROOT, "WinHTTrack", "rules-split.cpp"),
                    gen,
                    "-DBROWSER_LOGIN_PATHS_TEST",
                    "-DCOOKIE_FILE_TEST",
                    "-DRULES_SPLIT_TEST",
                    "-I" + os.path.join(ROOT, "tools"),
                    "-I" + os.path.join(ROOT, "WinHTTrack"),
                ],
                check=True,
            )
            try:
                run = subprocess.run([exe], capture_output=True, text=True, timeout=300)
            except subprocess.TimeoutExpired:
                sys.exit(
                    "the %s cases did not finish in 300 seconds, so one of them loops"
                    % label
                )
            if run.returncode != 0:
                sys.exit("%s: %s" % (label, (run.stdout + run.stderr).strip()))
            if EXPECTED_LINE not in run.stdout:
                sys.exit(
                    "the %s harness printed %r, expected %r"
                    % (label, run.stdout.strip(), EXPECTED_LINE)
                )
            for line in run.stdout.splitlines():
                print("%s: %s" % (label, line))


if __name__ == "__main__":
    main()
