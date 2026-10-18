#!/usr/bin/env python3
"""Compile and run tools/cookie-file-test.cpp, then check what no C code here can check
about itself: that the lengths the writer holds a cookie against are still the engine's,
that --selftest still runs the same cases, and that all three callers pin one count.

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

EXPECTED_LINE = "cookie file ok on 97 checks"
SWEEP_LINE = "cookie file properties ok on"

# Each cap is the smallest of the gates below, so each is read from the engine rather than
# kept here. "add" is cookie_add()'s own refusal, "buffer" the array cookie_load() reads
# the field into, which refuses at len >= sizeof and so caps one byte lower.
FIELD_GATES = {
    "domain": (r"strlen\(domain\)\s*>\s*(\d+)", r"char domain\[(\d+)\];"),
    "name": (r"strlen\(cook_name\)\s*>\s*(\d+)", r"char cook_name\[(\d+)\];"),
    "value": (r"strlen\(cook_value\)\s*>\s*(\d+)", r"char BIGSTK cook_value\[(\d+)\];"),
    "path": (r"strlen\(path\)\s*>\s*(\d+)", r"char path\[(\d+)\];"),
}

LITERALS = r'strcatbuff\(cook,\s*"((?:[^"\\]|\\.)*)"\)'

MACROS = {
    "domain": "COOKIE_DOMAIN_MAXLEN",
    "name": "COOKIE_NAME_MAXLEN",
    "value": "COOKIE_VALUE_MAXLEN",
    "path": "COOKIE_PATH_MAXLEN",
    "line": "COOKIE_LINE_MAXLEN",
    "jar": "COOKIE_JAR_MAXLEN",
    "reserve": "COOKIE_JAR_RESERVE",
    "record": "COOKIE_JAR_RECORD",
}


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def one(pattern, text, where):
    m = re.search(pattern, text, re.M)
    if m is None:
        sys.exit("%s does not carry %s any more" % (where, pattern))
    return int(m.group(1))


def region(text, opening, closing, what):
    """The slice of TEXT between OPENING and CLOSING. Both field names and buffer sizes
    repeat elsewhere in htsbauth.c, so every search below is anchored to one function.
    """
    start = re.search(opening, text)
    if start is None:
        sys.exit("the engine's %s is gone, so no cap can be read from it" % what)
    opened = start.end()
    rest = text[opened:]
    end = re.search(closing, rest)
    if end is None:
        sys.exit("the engine's %s does not end where expected" % what)
    return rest[: end.start()]


def engine_caps(bauth_c, core_c):
    """The longest each field may be before the engine drops the cookie, and the budget
    its jar leaves for the whole file."""
    adding = region(bauth_c, r"\bint cookie_add\(", r"\n\}", "cookie_add()")
    loading = region(bauth_c, r"// then cookies\.txt", r"fclose\(fp\)", "cookie_load()")
    caps = {}
    for field, (add, buf) in FIELD_GATES.items():
        caps[field] = min(
            one(add, adding, "cookie_add()"), one(buf, loading, "cookie_load()") - 1
        )
    # cookie_add() normalises the domain through cookie_host() first, which refuses a host
    # that does not fit its buffer rather than truncating it.
    caps["domain"] = min(
        caps["domain"], one(r"char host\[(\d+)\];", adding, "cookie_add()") - 1
    )
    caps["line"] = one(r"const size_t line_max\s*=\s*(\d+);", loading, "cookie_load()")
    caps["jar"] = one(r"cookie\.max_len\s*=\s*(\d+);", core_c, "the engine's htscore.c")
    # The headroom the parts[] loop keeps free on top of the record it is adding.
    caps["reserve"] = one(
        r"const size_t parts\[\][^;]*?(\d+)\};",
        adding.replace("\n", " "),
        "cookie_add()",
    )
    # What the stored record spends beyond the four fields, summed from the literals
    # cookie_add() concatenates. Charging our own shorter line would overrun the jar.
    literals = re.findall(LITERALS, adding)
    if len(literals) < 6:
        sys.exit("cookie_add() no longer builds its record with strcatbuff(cook, ...)")
    caps["record"] = sum(len(lit.encode().decode("unicode_escape")) for lit in literals)
    return caps


def header_caps(header):
    """What cookie-file.h holds a cookie against."""
    return {
        key: one(r"^#\s*define\s+%s\s+(\d+)\s*$" % macro, header, "cookie-file.h")
        for key, macro in MACROS.items()
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--engine",
        default=os.path.join(os.path.dirname(ROOT), "httrack"),
        help="the engine checkout holding src/htsbauth.c",
    )
    ap.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = ap.parse_args()

    sources = [os.path.join(args.engine, "src", n) for n in ("htsbauth.c", "htscore.c")]
    for path in sources:
        if not os.path.exists(path):
            sys.exit(
                "%s: no engine source, so the caps cannot be the engine's. Check the "
                "engine out, or pass --engine." % path
            )

    checked = 0
    bauth_c, core_c = (read(p) for p in sources)
    header = read(os.path.join(ROOT, "WinHTTrack", "cookie-file.h"))

    # 1. Every cap the writer enforces is the engine's own refusal point.
    engine = engine_caps(bauth_c, core_c)
    ours = header_caps(header)
    if engine != ours:
        sys.exit(
            "the caps in cookie-file.h no longer match the engine: ours %r, the engine's "
            "%r. A cookie the engine will not store would be written and then dropped "
            "with only a log line." % (ours, engine)
        )
    checked += 1

    # 2. The comparison above has to be able to fail, on both kinds of gate. Doctoring the
    #    real source and re-running the real parse is the control; comparing two literals
    #    would not be one.
    for what, pattern, replacement in (
        ("cookie_add()'s refusal", FIELD_GATES["value"][0], "strlen(cook_value) > 77"),
        ("cookie_load()'s buffer", FIELD_GATES["path"][1], "char path[77];"),
    ):
        doctored = re.sub(pattern, replacement, bauth_c, count=1)
        if doctored == bauth_c:
            sys.exit("%s could not be doctored, so check 1 proves nothing" % what)
        if engine_caps(doctored, core_c) == ours:
            sys.exit(
                "a doctored %s still compared equal, so check 1 proves nothing" % what
            )
        checked += 1

    # 3. --selftest has to still run the cases, or the Windows half stopped being checked.
    #    Anchored on the assignment, because a bare call can sit in a comment.
    selftest = read(os.path.join(ROOT, "WinHTTrack", "WinHTTrack.cpp"))
    if "const int nchecks = cookieFileCheckCases(&err);" not in selftest:
        sys.exit(
            "WinHTTrack.cpp no longer calls cookieFileCheckCases(): --selftest stopped "
            "running the cookie-file cases"
        )
    checked += 1

    # 4. All three expected counts have to agree, or one of them stops being a pin. The
    #    workflow matcher is in here because the pairing step in that same workflow
    #    compares only the text before the number.
    workflow = read(os.path.join(ROOT, ".github", "workflows", "windows-build.yml"))
    pins = {
        "--selftest": re.search(r"cookie file ran %d checks, expected (\d+)", selftest),
        "this harness": re.search(
            r"define CF_EXPECTED_CHECKS (\d+)",
            read(os.path.join(ROOT, "tools", "cookie-file-test.cpp")),
        ),
        "the workflow matcher": re.search(r"cookie file ok on (\d+) checks", workflow),
    }
    if any(m is None for m in pins.values()):
        sys.exit(
            "cannot read the expected check count from %s"
            % ", ".join(k for k, m in pins.items() if m is None)
        )
    wanted = re.search(r"cookie file ok on (\d+) checks", EXPECTED_LINE).group(1)
    disagree = {k: m.group(1) for k, m in pins.items() if m.group(1) != wanted}
    if disagree:
        sys.exit(
            "the expected check count is %s here, and %s"
            % (wanted, ", ".join("%s in %s" % (v, k) for k, v in disagree.items()))
        )
    checked += 1

    # 5. The count has to be the accumulator. A literal returned there would satisfy every
    #    pin with each case deleted, and no run of the C could tell the difference.
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
        # stdout only: capturing stderr too would swallow the harness's own FAIL line into
        # the traceback, leaving a reader the count but not which case broke.
        out = subprocess.run(
            [exe], check=True, stdout=subprocess.PIPE, text=True
        ).stdout
        # Both lines are matched, or either half can stop running in silence.
        for line in (EXPECTED_LINE, SWEEP_LINE):
            if line not in out:
                sys.exit("the harness printed %r, with no %r in it" % (out, line))
        sys.stdout.write(out)


if __name__ == "__main__":
    main()
