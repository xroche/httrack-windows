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

EXPECTED_LINE = "rule splitting ok on 158 checks"


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


def engine_opt_function(htscoremain):
    """The engine's hts_rule_would_be_read_as_opt(), copied out so the harness links the
    same function rules-split.cpp calls in the Windows build.

    The engine exported it and dropped cmdl_opt() in xroche/httrack#1948, and
    rules-split.cpp calls it directly now, so an engine without it cannot build this GUI
    at all."""
    m = re.search(
        r"^HTSEXT_API hts_boolean hts_rule_would_be_read_as_opt\(const char \*arg\) "
        r"\{.*?^\}$",
        htscoremain,
        re.M | re.S,
    )
    if m is None:
        if "hts_rule_would_be_read_as_opt" in htscoremain:
            sys.exit(
                "the engine names hts_rule_would_be_read_as_opt() and this harness "
                "cannot read its definition. Fix the pattern in engine_opt_function()."
            )
        sys.exit(
            "the engine has no hts_rule_would_be_read_as_opt(), so it is older than "
            "xroche/httrack#1948 and this GUI cannot be built against it"
        )
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
    htscoremain = os.path.join(args.engine, "src", "htscoremain.c")
    htssafe = os.path.join(args.engine, "src", "htssafe.h")
    for path in (htscore, htscoremain, htssafe):
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

    # 2b. And so does every copy CI matches. One of them was missed once, and only a
    #     Windows leg said so, half an hour after the ubuntu one went green.
    pinned = [
        os.path.join(".github", "workflows", "windows-build.yml"),
        os.path.join(".github", "actions", "smoke-test", "action.yml"),
    ]
    for rel in pinned:
        text = read(os.path.join(ROOT, rel))
        counts = set(re.findall(r"rule splitting ok on (\d+) checks", text))
        if not counts:
            sys.exit("%s no longer matches the rule-splitting line at all" % rel)
        if counts != {mine.group(1)}:
            sys.exit(
                "%s matches %s checks, where the code runs %s"
                % (rel, ", ".join(sorted(counts)), mine.group(1))
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

    # 4. The Add Filter rules have to be the ones tested here, not a copy left in the
    #    dialog code, and both the refusal and the build must ask for the same thing.
    tab7 = read(os.path.join(ROOT, "WinHTTrack", "OptionTab7.cpp"))
    addfilter = read(os.path.join(ROOT, "WinHTTrack", "AddFilter.cpp"))
    for name, text in (("OptionTab7.cpp", tab7), ("AddFilter.cpp", addfilter)):
        if "filterRulesForField(" not in text:
            sys.exit(
                "%s no longer calls filterRulesForField(), so the dialog's rules are "
                "not the ones under test" % name
            )
        checked += 1
        if "FILTER_RULES_MAXSIZE" not in text:
            sys.exit(
                "%s no longer sizes its buffer with FILTER_RULES_MAXSIZE, so the "
                "refusal and the build can disagree on the last rule" % name
            )
        checked += 1
    if re.search(r"sprintf\s*\(", tab7):
        sys.exit("OptionTab7.cpp builds a filter rule of its own again")
    checked += 1

    # 5. Nothing off Windows can open the dialog, so the shape of OnOK()'s refusal is
    #    checked as text: the verdict has to be read, and it has to stop the dialog.
    guard = re.search(
        r"if \(!filterRulesForField\([^;]*?\)\) \{(.*?)\n    \}", addfilter, re.S
    )
    if guard is None:
        sys.exit(
            "AddFilter.cpp no longer refuses on filterRulesForField()'s verdict, so "
            "OnOK() lets a keyword through that builds no rule"
        )
    checked += 1
    if not re.search(r"^\s*return;\s*$", guard.group(1), re.M):
        sys.exit(
            "AddFilter.cpp's refusal no longer returns, so the dialog closes on a "
            "keyword it just refused"
        )
    checked += 1

    # 6. Our keyword separators have to hold every byte the engine ends a token on, or
    #    one keyword reaches it as two arguments and the second one carries no sign.
    ours = re.search(
        r'#\s*define\s+FILTER_KEYWORD_SEPARATORS\s+"([^"]*)"',
        read(os.path.join(ROOT, "WinHTTrack", "rules-split.cpp")),
    )
    if ours is None:
        sys.exit("no FILTER_KEYWORD_SEPARATORS in rules-split.cpp")
    mine = set(ours.group(1).encode().decode("unicode_escape"))
    theirs = set(
        re.search(r'HTS_REALSPACES\s+"([^"]*)"', read(htssafe))
        .group(1)
        .encode()
        .decode("unicode_escape")
    )
    if not theirs <= mine:
        sys.exit(
            "the engine ends a token on %r, which does not separate two keywords"
            % "".join(sorted(theirs - mine))
        )
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
            exe = os.path.join(tmp, "rules-split-test-" + label)
            subprocess.run(
                [
                    args.cxx,
                    "-std=c++14",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-g",
                    "-o",
                    exe,
                ]
                + extra
                + [
                    os.path.join(ROOT, "tools", "rules-split-test.cpp"),
                    os.path.join(ROOT, "WinHTTrack", "rules-split.cpp"),
                    gen,
                    "-DRULES_SPLIT_TEST",
                    "-I" + os.path.join(ROOT, "tools"),
                    "-I" + os.path.join(ROOT, "WinHTTrack"),
                ],
                check=True,
            )
            # A case that loops rather than fails would otherwise wedge CI, and a
            # failing case prints its disagreement on stderr.
            try:
                run = subprocess.run([exe], capture_output=True, text=True, timeout=300)
            except subprocess.TimeoutExpired:
                sys.exit(
                    "the %s cases did not finish in 300 seconds, so one of them loops"
                    % label
                )
            if run.returncode != 0:
                sys.exit("%s: %s" % (label, (run.stdout + run.stderr).strip()))
            out = run.stdout
            # Both counts are printed and have to be matched, or coverage can shrink in
            # silence.
            for want in (EXPECTED_LINE,):
                if want not in out:
                    sys.exit(
                        "the %s harness printed %r, expected %r"
                        % (label, out.strip(), want)
                    )
            # named per build, so the log shows the sanitized one ran as well
            for line in out.splitlines():
                print("%s: %s" % (label, line))


if __name__ == "__main__":
    main()
