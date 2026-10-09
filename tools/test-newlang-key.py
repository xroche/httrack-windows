#!/usr/bin/env python3
"""Compile and run tools/newlang-key-test.cpp, then check what no C code here can check
about itself: that newlang.cpp reaches the symbol-key fallback only where the English-text
lookup missed, and that no key the engine ships has the symbol shape.

A catalog keys its rows on the English text, so rewording a string orphans its 30
translations. newlang.cpp accepts a LANG_ or LISTDEF_ symbol as the key instead. Nothing
ships symbol-keyed yet, so the fallback cannot be reached and no string changes. Check 2
is what carries that, because the replay it drives rebuilds the loader around the one
shipped function and so cannot speak for the real path.

The engine has to be checked out. --engine says where, and defaults to the sibling
directory CI uses.
"""

import argparse
import glob
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SYMBOL = re.compile(r"^(?:LANG_|LISTDEF_)[A-Za-z0-9_]+$")


def read(path, encoding="utf-8"):
    with open(path, encoding=encoding) as f:
        return f.read()


def catalog_keys(path):
    """The key line of each pair, read as the engine's linput_cpp() reads it."""
    lines = read(path, "latin-1").replace("\r", "").replace("\t", "").split("\n")
    return [lines[i].strip(" ") for i in range(0, len(lines), 2)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--engine",
        default=os.path.join(os.path.dirname(ROOT), "httrack"),
        help="the engine checkout holding lang.def and lang/*.txt",
    )
    ap.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = ap.parse_args()

    langdef = os.path.join(args.engine, "lang.def")
    if not os.path.exists(langdef):
        sys.exit(
            "%s: no catalog index, so there is nothing to replay the join over. "
            "Check the engine out, or pass --engine." % langdef
        )

    # 1. The fallback must sit behind the miss. Guarded the other way round it would
    #    preempt a key lang.def does list, and the harness replays the guard rather than
    #    compiling newlang.cpp, so only a source check sees this.
    loader = read(os.path.join(ROOT, "WinHTTrack", "newlang.cpp"))
    wanted = (
        "intkey=LANGINTKEY(extkey);",
        "if (!strnotempty(intkey) && LangKeyIsSymbol(extkey))",
        "intkey=extkey;",
    )
    at = 0
    for line in wanted:
        at = loader.find(line, at)
        if at < 0:
            sys.exit(
                "newlang.cpp does not reach the fallback through a missed text lookup: "
                "expected %r after the lines before it" % line
            )
        at += len(line)
    if loader.count("LangKeyIsSymbol") != 1:
        sys.exit("newlang.cpp names LangKeyIsSymbol more than once, so a second call site")

    # 2. Independently of the C, no key the engine ships has the symbol shape, in lang.def
    #    or in any catalog. That is why the branch cannot be reached today.
    lang = read(langdef, "latin-1").replace("\r", "").replace("\t", "").split("\n")
    shaped = [k for k in (x.strip(" ") for x in lang[1::2]) if SYMBOL.match(k)]
    if shaped:
        sys.exit("lang.def looks keys up by %s, which the fallback would claim" % shaped)
    catalogs = sorted(glob.glob(os.path.join(args.engine, "lang", "*.txt")))
    if len(catalogs) < 30:
        sys.exit("found %d catalogs under %s/lang" % (len(catalogs), args.engine))
    for path in catalogs:
        shaped = [k for k in catalog_keys(path) if SYMBOL.match(k)]
        if shaped:
            sys.exit("%s is keyed by %s: the change is no longer a no-op" % (path, shaped))

    # 3. The engine's own symbols all have the shape the fallback accepts, or a migrated
    #    row would silently stay unresolved.
    missed = [s for s in lang[0::2] if s.startswith(("LANG_", "LISTDEF_")) and not SYMBOL.match(s)]
    if missed:
        sys.exit("lang.def names %s, which the fallback would not accept" % missed)
    header = read(os.path.join(ROOT, "WinHTTrack", "cpp_lang.h"))
    defines = re.findall(r"^#\s*define\s+((?:LANG_|LISTDEF_)\S*)", header, re.M)
    if len(defines) < 480:
        sys.exit(
            "found %d call-site symbols: the pattern no longer matches cpp_lang.h" % len(defines)
        )
    missed = [s for s in defines if not SYMBOL.match(s)]
    if missed:
        sys.exit("cpp_lang.h names %s, which the fallback would not accept" % missed)

    # Flushed, because the replay below writes to the same stream from a subprocess and
    # this is the line that carries the no-op.
    print(
        "%d catalogs and %d lang.def keys carry no symbol-shaped key, and the fallback "
        "accepts all %d call-site symbols" % (len(catalogs), len(lang[1::2]), len(defines)),
        flush=True,
    )

    # 4. Replay both joins and diff them, with a symbol-keyed probe proving the diff
    #    can see one.
    with tempfile.TemporaryDirectory() as tmp:
        probe = os.path.join(tmp, "probe.txt")
        with open(probe, "w", encoding="utf-8") as f:
            f.write("LANG_OK\nprobe value\n")
        exe = os.path.join(tmp, "newlang-key-test")
        subprocess.run(
            [
                args.cxx,
                "-std=c++14",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-o",
                exe,
                os.path.join(ROOT, "tools", "newlang-key-test.cpp"),
                os.path.join(ROOT, "WinHTTrack", "newlang-key.cpp"),
                "-I" + os.path.join(ROOT, "WinHTTrack"),
            ],
            check=True,
        )
        env = dict(os.environ, NEWLANG_KEY_PROBE=probe)
        subprocess.run([exe, args.engine], check=True, env=env)


if __name__ == "__main__":
    main()
