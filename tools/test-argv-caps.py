#!/usr/bin/env python3
"""Compile and run tools/argv-caps-test.cpp, then check what no C code here can check
about itself: that --selftest still runs the same cases, and that both callers pin the
same count.

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

EXPECTED_LINE = "argv caps ok on 37 checks"

# The caps and the footer default the cases read, copied out of the engine rather than
# kept here: a copy would stop agreeing with the engine the day the engine's moved.
ENGINE_MACROS = (
    "HTTRACK_AFF_VERSION",
    "HTTRACK_AFF_AUTHORS",
    "HTS_DEFAULT_FOOTER",
    "HTS_NOPARAM",
    "HTS_CDLMAXSIZE",
    "HTS_FOOTER_MAXSIZE",
    "HTS_LANGISO_MAXSIZE",
    "HTS_REFERER_MAXSIZE",
    "HTS_MAX_RETRY_AFTER_LIMIT",
)

# The functions that moved. A copy left in Shell.cpp would mean the Windows build and this
# run test different code.
MOVED = (
    "fitsEngineArgument",
    "isEngineArgument",
    "isCappedArgument",
    "isUserAgentArgument",
    "isFooterArgument",
    "isLangIsoArgument",
    "isRefererArgument",
    "optionValue",
    "isSingleFileMaxArgument",
    "isMaxRetryAfterArgument",
    "argvCapsCheckCases",
)


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def macro_lines(header, names):
    """The exact #define lines for NAMES, continuations included."""
    out = []
    for name in names:
        m = re.search(r"^#\s*define\s+%s(?:[^\n]*\\\n)*[^\n]*$" % name, header, re.M)
        if m is None:
            sys.exit("no %s in the engine's htsglobal.h" % name)
        out.append(m.group(0))
    return "\n".join(out)


def llint_typedef(header):
    """The engine's LLint, which isSingleFileMaxArgument() parses into. Copied for the
    same reason as the macros: this is the type whose range the overflow case rides on.
    """
    m = re.search(r"^typedef\s+[^;\n]*\bLLint\s*;$", header, re.M)
    if m is None:
        sys.exit("no LLint typedef in the engine's htsglobal.h")
    return m.group(0)


def retry_after_cap(shell_h):
    """HTS_MAXRETRYAFTER_MAXBYTES, read out of Shell.h so the argv length the cases pin is
    the one the shell enforces."""
    m = re.search(r"^#\s*define\s+HTS_MAXRETRYAFTER_MAXBYTES[^\n]*$", shell_h, re.M)
    if m is None:
        sys.exit("no HTS_MAXRETRYAFTER_MAXBYTES in WinHTTrack/Shell.h")
    return m.group(0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--engine",
        default=os.path.join(os.path.dirname(ROOT), "httrack"),
        help="the engine checkout holding src/htsglobal.h",
    )
    ap.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = ap.parse_args()

    htsglobal = os.path.join(args.engine, "src", "htsglobal.h")
    if not os.path.exists(htsglobal):
        sys.exit(
            "%s: no engine source, so the caps cannot be the engine's. Check the engine "
            "out, or pass --engine." % htsglobal
        )

    checked = 0

    # 1. --selftest has to still run these cases, or the move dropped the Windows half.
    selftest = read(os.path.join(ROOT, "WinHTTrack", "WinHTTrack.cpp"))
    if "argvCapsCheckCases(&err, &nskipped)" not in selftest:
        sys.exit(
            "WinHTTrack.cpp no longer calls argvCapsCheckCases(): --selftest stopped "
            "running the option-value cases"
        )
    checked += 1

    # 2. The two expected counts have to agree, or one of them stops being a pin.
    want = re.search(r"const int expected = (\d+) - nskipped;", selftest)
    mine = re.search(
        r"define AC_EXPECTED_CHECKS (\d+)",
        read(os.path.join(ROOT, "tools", "argv-caps-test.cpp")),
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

    # 3. One definition only.
    shell = read(os.path.join(ROOT, "WinHTTrack", "Shell.cpp"))
    for name in MOVED:
        if re.search(r"^\w[^;()\n]*\b%s\s*\(" % name, shell, re.M):
            sys.exit("%s is defined in Shell.cpp as well as argv-caps.cpp" % name)
        checked += 1

    # 4. The count has to be the accumulator. A literal returned there would satisfy both
    #    pins with every loop deleted, and no run of the C can tell the difference.
    cases = read(os.path.join(ROOT, "WinHTTrack", "argv-caps.cpp"))
    if "\n  return nchecks;\n}\n" not in cases:
        sys.exit(
            "argvCapsCheckCases() no longer ends by returning nchecks: the printed count "
            "would hold whether or not the cases ran"
        )
    checked += 1

    print("%d source checks pass" % checked)

    with tempfile.TemporaryDirectory() as tmp:
        gen = os.path.join(tmp, "argv-caps-engine-macros.h")
        with open(gen, "w", encoding="utf-8") as f:
            f.write(
                "/* Copied by tools/test-argv-caps.py out of the engine's src/htsglobal.h"
                " and out of WinHTTrack/Shell.h. */\n"
                "#include <stdint.h>\n"
            )
            f.write(llint_typedef(read(htsglobal)) + "\n")
            f.write(macro_lines(read(htsglobal), ENGINE_MACROS) + "\n")
            f.write(
                retry_after_cap(read(os.path.join(ROOT, "WinHTTrack", "Shell.h")))
                + "\n"
            )
        exe = os.path.join(tmp, "argv-caps-test")
        subprocess.run(
            [
                args.cxx,
                "-std=c++14",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-o",
                exe,
                os.path.join(ROOT, "tools", "argv-caps-test.cpp"),
                os.path.join(ROOT, "WinHTTrack", "argv-caps.cpp"),
                "-DARGV_CAPS_TEST",
                "-I" + tmp,
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
