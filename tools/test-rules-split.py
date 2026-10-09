#!/usr/bin/env python3
"""Compile and run tools/rules-split-test.cpp, then check what no C code here can check
about itself: that --selftest still runs the same cases, and that both callers pin the
same count.

The GUI only builds under MSVC, so the --selftest run cannot be rerun in review and cannot
be mutated in a pull request. This is the half that runs on ubuntu.

hts_scan_token() decides where a rule ends, so the engine has to be checked out. --engine
says where, and defaults to the sibling directory CI uses.
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

EXPECTED_LINE = "rule splitting ok on 49 checks"


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def engine_token_function(htscore):
    """The engine's hts_scan_token(), copied out rather than reproduced here, so the
    splitter under test ends a token where the engine ends it."""
    m = re.search(r"^hts_boolean hts_scan_token\(.*?^\}$", htscore, re.M | re.S)
    if m is None:
        sys.exit("no hts_scan_token() in the engine's htscore.c")
    return m.group(0)


def engine_space_set(htssafe):
    """The byte set that function splits on, copied out for the same reason. A set kept
    here instead would stop agreeing with the engine the day the engine's moved."""
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
    htssafe = os.path.join(args.engine, "src", "htssafe.h")
    for path in (htscore, htssafe):
        if not os.path.exists(path):
            sys.exit(
                "%s: no engine source, so the token scanner cannot be the engine's. "
                "Check the engine out, or pass --engine." % path
            )

    checked = 0

    # 1. --selftest has to still run these cases, or the move dropped the Windows half.
    selftest = read(os.path.join(ROOT, "WinHTTrack", "WinHTTrack.cpp"))
    if "rulesSplitCheckCases(&err)" not in selftest:
        sys.exit(
            "WinHTTrack.cpp no longer calls rulesSplitCheckCases(): --selftest "
            "stopped running the rule cases"
        )
    checked += 1

    # 2. The two expected counts have to agree, or one of them stops being a pin.
    want = re.search(r"rule splitting ran %d checks, expected (\d+)", selftest)
    mine = re.search(
        r"define RS_EXPECTED_CHECKS (\d+)",
        read(os.path.join(ROOT, "tools", "rules-split-test.cpp")),
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

    # 3. One definition only: a copy left in Shell.cpp would mean the Windows build and
    #    this run test different code.
    shell = read(os.path.join(ROOT, "WinHTTrack", "Shell.cpp"))
    for name in (
        "splitRulesInArray",
        "applyRulePreset",
        "ruleListHoldsPreset",
        "splitTokensInArray",
        "ruleListHoldsRule",
    ):
        if re.search(r"^\w[^;()\n]*\b%s\s*\(" % name, shell, re.M):
            sys.exit("%s is defined in Shell.cpp as well as rules-split.cpp" % name)
        checked += 1

    print("%d source checks pass" % checked)

    with tempfile.TemporaryDirectory() as tmp:
        gen = os.path.join(tmp, "engine-token.cpp")
        with open(gen, "w", encoding="utf-8") as f:
            f.write(
                "/* Copied by tools/test-rules-split.py out of the engine's "
                "src/htscore.c and src/htssafe.h. */\n"
                "#include <assert.h>\n#include <ctype.h>\n#include <stddef.h>\n"
                "#include <string.h>\n"
                "typedef int hts_boolean;\n"
                "#define HTS_TRUE 1\n#define HTS_FALSE 0\n"
                "#define assertf(x) assert(x)\n"
                "%s\n"
                'extern "C" {\n%s\n}\n'
                % (
                    engine_space_set(read(htssafe)),
                    engine_token_function(read(htscore)),
                )
            )
        exe = os.path.join(tmp, "rules-split-test")
        subprocess.run(
            [
                args.cxx,
                "-std=c++14",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-o",
                exe,
                os.path.join(ROOT, "tools", "rules-split-test.cpp"),
                os.path.join(ROOT, "WinHTTrack", "rules-split.cpp"),
                gen,
                "-DRULES_SPLIT_TEST",
                "-I" + os.path.join(ROOT, "tools"),
                "-I" + os.path.join(ROOT, "WinHTTrack"),
            ],
            check=True,
        )
        out = subprocess.run([exe], check=True, capture_output=True, text=True).stdout
        # The count is printed and has to be matched, or coverage can shrink in silence.
        if EXPECTED_LINE not in out:
            sys.exit(
                "the harness printed %r, expected %r" % (out.strip(), EXPECTED_LINE)
            )
        print(out, end="")


if __name__ == "__main__":
    main()
