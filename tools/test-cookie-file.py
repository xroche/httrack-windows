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

EXPECTED_LINE = "cookie file ok on 104 checks"
SWEEP_LINE = "cookie file properties ok on"

# The buffer cookie_load_records() reads each field into. cookie_load_field() refuses at
# len >= sizeof, so each buffer caps one byte lower than it looks.
LOAD_BUFFERS = {
    "domain": r"char domain\[(\d+)\];",
    "value": r"char BIGSTK cook_value\[(\d+)\];",
    "path": r"char path\[(\d+)\];",
}

MACROS = {
    "domain": "COOKIE_DOMAIN_MAXLEN",
    "name": "COOKIE_NAME_MAXLEN",
    "pair": "COOKIE_PAIR_MAXLEN",
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


def engine_caps(bauth_c, bauth_h, core_c, expiry_max):
    """The longest each field may be before the engine drops the cookie, the budget it
    charges the name and the value to together, and what its jar leaves for the file."""
    # cookie_add() is a wrapper now, so the refusals and the record live in cookie_set().
    adding = region(bauth_c, r"\bint cookie_set\(", r"\n\}", "cookie_set()")
    # cookie_load_records() is where the per-record buffers live, and each one matches
    # exactly once in it. Anchoring on cookie_load() instead still FINDS a region, and
    # that region no longer holds them, so the anchor has to name this function.
    loading = region(
        bauth_c, r"static int cookie_load_records\(", r"\n\}", "cookie_load_records()"
    )
    caps = {}
    # Every buffer below caps one byte lower than it looks, and these two comparisons are
    # the whole reason. Read them, rather than resting a -1 on a comment.
    for where, opening in (
        ("cookie_load_field()", r"cookie_load_field\(httrackp"),
        ("cookie_host()", r"hts_boolean cookie_host\("),
    ):
        if not re.search(
            r"len\s*>=\s*dst_size", region(bauth_c, opening, r"\n\}", where)
        ):
            sys.exit(
                "%s no longer refuses at len >= dst_size, so every cap it bounds moved "
                "one byte" % where
            )
    buffers = {
        field: one(buf, loading, "cookie_load_records()") - 1
        for field, buf in LOAD_BUFFERS.items()
    }

    # The name and the value are charged to one budget, so neither has a cap of its own.
    # Both halves of that gate are read, because the first is what keeps the subtraction
    # in the second from wrapping.
    pair = one(r"#\s*define\s+COOKIE_MAX_PAIR\s+(\d+)", bauth_h, "htsbauth.h")
    if not re.search(r"strlen\(cook_name\)\s*>\s*COOKIE_MAX_PAIR", adding):
        sys.exit("cookie_set() no longer holds the name to COOKIE_MAX_PAIR")
    if not re.search(
        r"strlen\(cook_value\)\s*>\s*COOKIE_MAX_PAIR\s*-\s*strlen\(cook_name\)", adding
    ):
        sys.exit("cookie_set() no longer charges the name and the value to one budget")
    caps["pair"] = pair
    # The value's own buffer binds only if it ever drops under that budget.
    if pair > buffers["value"]:
        sys.exit(
            "cookie_load_records()'s value buffer caps at %d, under the %d budget, "
            "so a value the budget allows would now be refused by the buffer instead"
            % (buffers["value"], pair)
        )
    # The name's buffer is written in terms of the same budget.
    plus = one(
        r"char BIGSTK cook_name\[COOKIE_MAX_PAIR \+ (\d+)\];",
        loading,
        "cookie_load_records()",
    )
    caps["name"] = min(pair, pair + plus - 1)
    caps["path"] = min(
        buffers["path"], one(r"strlen\(path\)\s*>\s*(\d+)", adding, "cookie_set()")
    )
    # cookie_set() normalises the domain through cookie_host() first, which refuses a host
    # that does not fit its buffer rather than truncating it. No refusal of its own is
    # left in cookie_set(), so these two buffers are the whole gate.
    caps["domain"] = min(
        buffers["domain"], one(r"char host\[(\d+)\];", adding, "cookie_set()") - 1
    )
    caps["line"] = one(
        r"const size_t line_max\s*=\s*(\d+);", loading, "cookie_load_records()"
    )
    caps["jar"] = one(r"cookie\.max_len\s*=\s*(\d+);", core_c, "the engine's htscore.c")
    # The headroom the parts[] loop keeps free on top of the record it is adding.
    caps["reserve"] = one(
        r"const size_t parts\[\][^;]*?(\d+)\};",
        adding.replace("\n", " "),
        "cookie_set()",
    )
    # The stored record spends three things beyond the four fields: the separators of the
    # snprintf that assembles it, the widest word each of its two flag fields takes, and
    # the digits of the latest expiry this writer emits. The separator count includes the
    # tab of the HttpOnly field our writer never sets, so it charges one byte too many.
    record = region(adding, r"snprintf\(cook,", r"\);", "cookie_set()'s record")
    seps = len(re.findall(r"\\[tn]", record))
    if seps < 7:
        sys.exit(
            "cookie_set()'s record carries %d separators, so its shape moved" % seps
        )
    flags = re.findall(r'"(TRUE|FALSE)"', record)
    if set(flags) != {"TRUE", "FALSE"}:
        sys.exit("cookie_set() no longer spells its flag fields TRUE and FALSE")
    caps["record"] = seps + 2 * max(len(f) for f in flags) + len(str(expiry_max))
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

    sources = [
        os.path.join(args.engine, "src", n)
        for n in ("htsbauth.c", "htsbauth.h", "htscore.c")
    ]
    for path in sources:
        if not os.path.exists(path):
            sys.exit(
                "%s: no engine source, so the caps cannot be the engine's. Check the "
                "engine out, or pass --engine." % path
            )

    checked = 0
    bauth_c, bauth_h, core_c = (read(p) for p in sources)
    header = read(os.path.join(ROOT, "WinHTTrack", "cookie-file.h"))
    cases = read(os.path.join(ROOT, "WinHTTrack", "cookie-file.cpp"))
    # The record cost below counts the digits of the latest expiry this writer can emit,
    # so that cap is read from the writer rather than assumed.
    stamped = re.search(r"#\s*define\s+COOKIE_EXPIRY_MAX\s+([\d.]+)", cases)
    if stamped is None:
        sys.exit("cookie-file.cpp no longer defines COOKIE_EXPIRY_MAX")
    expiry = int(float(stamped.group(1)))

    # 1. Every cap the writer enforces is the engine's own refusal point.
    engine = engine_caps(bauth_c, bauth_h, core_c, expiry)
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
    for what, header_side, pattern, replacement in (
        (
            "cookie_set()'s path refusal",
            False,
            r"strlen\(path\)\s*>\s*(\d+)",
            "strlen(path) > 77",
        ),
        (
            "cookie_load_records()'s buffer",
            False,
            r"char path\[(\d+)\];",
            "char path[77];",
        ),
        (
            "the COOKIE_MAX_PAIR budget",
            True,
            r"#\s*define\s+COOKIE_MAX_PAIR\s+(\d+)",
            "#define COOKIE_MAX_PAIR 77",
        ),
    ):
        source = bauth_h if header_side else bauth_c
        doctored = re.sub(pattern, replacement, source, count=1)
        if doctored == source:
            sys.exit("%s could not be doctored, so check 1 proves nothing" % what)
        got = (
            engine_caps(bauth_c, doctored, core_c, expiry)
            if header_side
            else engine_caps(doctored, bauth_h, core_c, expiry)
        )
        if got == ours:
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
        # Anchored on the call, because every --selftest block has an nchecks guard.
        "--selftest": re.search(
            r"cookieFileCheckCases\(&err\);[\s\S]*?nchecks != (\d+)\)", selftest
        ),
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
