#!/usr/bin/env python3
"""Compile and run tools/winprofile-bind-test.cpp, then check what no C code here can
check about itself: that every key named hand-written really is written and read, and that
every catalog list holds the entry count the engine's table states.

The GUI only builds under MSVC, so --selftest cannot be rerun in review and cannot be
mutated in a pull request. This is the half that runs on ubuntu.

The engine has to be checked out. --engine says where, and defaults to the sibling
directory CI uses.
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The hand-written lists below are what the written bytes are held against. Each has to
# be reachable in the source, which only a text check can see.
HAND_WRITTEN_IN = ("Write_profile", "Read_profile")


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def read_latin1(path):
    """lang.def is the engine's, and it is Latin-1."""
    with open(path, encoding="latin-1") as f:
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


def function_body(src, name):
    """A function's body in Shell.cpp, comments stripped. Both functions carry commented-out
    calls naming real keys, and a check reading those would pass on dead code."""
    i = src.index("void %s(CString path,int load_path) {" % name)
    j = src.index("\n}\n", i)
    body = re.sub(r"/\*.*?\*/", "", src[i:j], flags=re.S)
    return "\n".join(re.sub(r"//.*$", "", line) for line in body.split("\n"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--engine",
        default=os.path.join(os.path.dirname(ROOT), "httrack"),
        help="the engine checkout holding src/winprofile-keys.h",
    )
    ap.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = ap.parse_args()

    engine_src = os.path.join(args.engine, "src")
    keys = os.path.join(engine_src, "winprofile-keys.h")
    if not os.path.exists(keys):
        sys.exit(
            "%s: no generated key table, so there is nothing to check against. "
            "Check the engine out, or pass --engine." % keys
        )

    bind = read(os.path.join(ROOT, "WinHTTrack", "winprofile-bind.h"))
    shell = read(os.path.join(ROOT, "WinHTTrack", "Shell.cpp"))
    checked = 0

    # 1. Every hand-written key has to appear in both profile functions. Nothing in the C
    #    sees this, so a key moved out of WINPROFILE_BINDINGS and into the hand-written
    #    list would otherwise sweep clean while being saved and loaded nowhere.
    hand = re.findall(r'KEY\("([A-Za-z0-9]+)"\)', bind)
    if len(hand) < 8:
        sys.exit("found %d hand-written keys: the pattern no longer matches the list" % len(hand))
    bodies = {name: function_body(shell, name) for name in HAND_WRITTEN_IN}
    scope = dict(re.findall(r'\{"([^"]+)", "[^"]*", "([^"]*)"', read(keys)))
    for key in hand:
        # The table decides which direction a key has to appear in. ProfileFormat is
        # write_only: the format stamp is for the other front ends, and nothing reads it.
        want = {"write_only": ("Write_profile",), "read_only": ("Read_profile",)}.get(
            scope.get(key), HAND_WRITTEN_IN
        )
        for name in want:
            if '"%s"' % key not in bodies[name]:
                sys.exit(
                    "%s is named hand-written, but %s does not mention it: that "
                    "setting is saved or loaded nowhere" % (key, name)
                )
            checked += 1

    # 2. And no bound key may be hand-written in either function as well, or one of the two
    #    writes silently wins.
    bound = re.findall(r'\b(?:CHECKBOX|LIST|NUMBER|TEXT)\("([A-Za-z0-9]+)"', bind)
    if len(bound) < 80:
        sys.exit("found %d bound keys: the pattern no longer matches the list" % len(bound))
    for key in bound:
        for name, body in bodies.items():
            if '"%s"' % key in body:
                sys.exit("%s is bound and also written out by hand in %s" % (key, name))
            checked += 1

    # 3. The macro bodies the two functions expand have to be the ones the harness uses, or
    #    the harness proves nothing about the real writer.
    for name, want in (
        ("Write_profile", ("MyWriteProfileInt(", "MyWriteProfileString(")),
        ("Read_profile", ("MyGetProfileInt(", "MyGetProfileString(")),
    ):
        body = bodies[name]
        if "WINPROFILE_BINDINGS(" not in body:
            sys.exit("%s no longer expands WINPROFILE_BINDINGS" % name)
        for call in want:
            if call not in body:
                sys.exit("%s expands WINPROFILE_BINDINGS without calling %s" % (name, call))
            checked += 1

    # 4. Each list key's combo is filled by SetCombo() from a catalog list, and the count
    #    the table states has to match. --selftest counts the loaded catalog; here the
    #    same count comes out of lang.def, so a reviewer can rerun this half too.
    combos = re.findall(r'COMBO\("([A-Za-z0-9]+)",\s*(LISTDEF_\d+)\)', bind)
    if len(combos) < 9:
        sys.exit("found %d catalog lists: the pattern no longer matches the list" % len(combos))
    lang = read_latin1(os.path.join(args.engine, "lang.def")).split("\n")
    listdef = {}
    for i, line in enumerate(lang):
        if re.match(r"^LISTDEF_\d+$", line.strip()):
            # Split as SetCombo() does: on newlines, trimmed, an empty entry dropped.
            listdef[line.strip()] = len([e for e in lang[i + 1].split("\\n") if e.strip()])
    kinds = dict(re.findall(r'\{"([^"]+)", "[^"]*", "[^"]*", "([^"]*)"', read(keys)))
    for key, name in combos:
        offered = listdef.get(name)
        if offered is None:
            sys.exit("the engine's lang.def has no %s" % name)
        want = kinds.get(key, "")
        if want != "list:0:%d" % offered:
            sys.exit(
                "the %s combo offers %d entries from %s, the table types it '%s'"
                % (key, offered, name, want)
            )
        checked += 1

    print(
        "%d source checks pass over %d bound and %d hand-written keys, and %d catalog lists"
        % (checked, len(bound), len(hand), len(combos))
    )

    # 4. Compile the real writer against the stubs and run it.
    with tempfile.TemporaryDirectory() as tmp:
        gen = os.path.join(tmp, "winprofile-engine-macros.h")
        with open(gen, "w", encoding="utf-8") as f:
            f.write(
                "/* Copied by tools/test-winprofile-bind.py out of the engine's "
                "src/htsglobal.h. */\n"
            )
            f.write(
                macro_lines(
                    read(os.path.join(engine_src, "htsglobal.h")),
                    ("HTTRACK_AFF_VERSION", "HTTRACK_AFF_AUTHORS", "HTS_DEFAULT_FOOTER"),
                )
            )
            f.write("\n")
        exe = os.path.join(tmp, "winprofile-bind-test")
        cmd = [
            args.cxx,
            "-std=c++14",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Wno-unused-parameter",
            "-o",
            exe,
            os.path.join(ROOT, "tools", "winprofile-bind-test.cpp"),
            os.path.join(ROOT, "WinHTTrack", "winprofile-io.cpp"),
            "-DWINPROFILE_IO_TEST",
            "-I" + tmp,
            "-I" + os.path.join(ROOT, "tools"),
            "-I" + os.path.join(ROOT, "WinHTTrack"),
            "-I" + engine_src,
        ]
        subprocess.run(cmd, check=True)
        subprocess.run([exe, os.path.join(tmp, "profile.ini")], check=True)


if __name__ == "__main__":
    main()
