#!/usr/bin/env python3
"""Compile and run tools/newlang-key-test.cpp, then check what no C code here can check
about itself: that newlang.cpp reaches the symbol-key fallback only where the English-text
lookup missed, and that no key the engine ships has the symbol shape.

A catalog keys its rows on the English text, so rewording a string orphans its 30
translations. newlang.cpp accepts a LANG_ or LISTDEF_ symbol as the key instead. Nothing
ships symbol-keyed yet, so the fallback cannot be reached and no string changes. Check 2
is what carries that, because the replay it drives rebuilds the loader around the one
shipped function and so cannot speak for the real path.

It also builds the fixtures of the engine's tests/576_webhttrack-lang-symbol-keys.test,
which re-key a catalog in whole or in part, and check 1 holds the shipped loader against
four of the five rules the engine's lang_load_catalog() follows. Both front ends have to
resolve a part-re-keyed catalog to the same strings.

The engine has to be checked out. --engine says where, and defaults to the sibling
directory CI uses.
"""

import argparse
import collections
import glob
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SYMBOL = re.compile(r"^(?:LANG_|LISTDEF_)[A-Za-z0-9_]+$")

# The two copies of "Start" in lang.def. The harness pins their French values, and the
# --selftest 'catalog keys' block pins that both resolve, so a rename has to land here.
START, REPEAT = "LANG_J9", "LANG_J13"

# What LANG_LOAD_CATALOG() in newlang.cpp has to say, whitespace apart, one entry per rule
# the engine's lang_load_catalog() follows. The harness replays a copy of this loader, so
# without these the two could drift apart and every check there would still pass.
RULES = (
    (
        "intkey=LANGINTKEY(extkey);constBOOLis_symbol="
        "(!strnotempty(intkey)&&LangKeyIsSymbol(extkey))?TRUE:FALSE;",
        "rule 1, the text lookup deciding before the shape test",
    ),
    (
        "if(is_symbol!=symbols)continue;",
        "rules 2 and 3, each kind of line left to its own pass",
    ),
    (
        "if(symbols){if(strnotempty(LANGSEL(extkey)))continue;intkey=extkey;}",
        "rule 2, the first symbol line winning and never incrementing",
    ),
    ("if(fallback)continue;", "rule 5, the English pass filling only empty slots"),
)
# Rule 4, the pass order, is not here. A pattern over the two calls waves through an extra
# English symbol pass hoisted above them and fails a loop that calls them in order, so it
# blocks the refactor and misses the bug. Nothing checks rule 4 in the shipped loader.


def read(path, encoding="utf-8"):
    with open(path, encoding=encoding) as f:
        return f.read()


def pack(source):
    """The source with its comments and every space gone, so a pattern can be exact."""
    source = re.sub(r"/\*.*?\*/", " ", source, flags=re.S)
    return re.sub(r"\s+", "", re.sub(r"//[^\n]*", " ", source))


def catalog_keys(path):
    """The key line of each pair, read as the engine's linput_cpp() reads it."""
    lines = read(path, "latin-1").replace("\r", "").replace("\t", "").split("\n")
    return [lines[i].strip(" ") for i in range(0, len(lines), 2)]


def catalog_pairs(path):
    """Each key and value pair of a catalog, paired up as linput_cpp() pairs them.

    Read and written back as latin-1, so the bytes of a UTF-8 catalog survive untouched.
    """
    lines = [line.rstrip("\r") for line in read(path, "latin-1").split("\n")]
    if lines and lines[-1] == "":
        lines.pop()
    if not lines or len(lines) % 2:
        sys.exit("%s holds %d lines, so its pairs do not line up" % (path, len(lines)))
    if any(line.rstrip(" \t").endswith("\\") for line in lines):
        sys.exit("%s continues a line with a backslash, which this builder cannot pair up"
                 % path)
    return [(lines[i].strip(" \t"), lines[i + 1]) for i in range(0, len(lines), 2)]


def symbol_table(langdef):
    """Every lookup key lang.def carries, mapped to the internal key it resolves to."""
    out = {}
    for internal, text in catalog_pairs(langdef):
        key, n = text, 0
        while key in out:
            n += 1
            key = text + str(n)
        out[key] = internal
    return out


def keyed(path, symbols):
    """Each catalog pair, plus the symbol it resolves to, or None where it has none.

    A pair with an empty key or value is never stored, so it claims no slot and takes no
    suffix either.
    """
    seen, out = collections.Counter(), []
    for text, value in catalog_pairs(path):
        internal = None
        if text and value:
            n = seen[text]
            seen[text] += 1
            internal = symbols.get(text + (str(n) if n else ""))
            if internal is not None and not SYMBOL.match(internal):
                internal = None
        out.append((text, internal, value))
    return out


def write_fixture(path, catalog, pick, moved=False):
    """One fixture catalog, with the lines pick() names re-keyed to their symbols."""
    lines = [
        (True, [internal, value]) if internal and pick(i) else (False, [text, value])
        for i, (text, internal, value) in enumerate(catalog)
    ]
    # A symbol line holds its meaning wherever it sits, unlike a repeated text.
    if moved:
        lines.sort(key=lambda line: line[0])
    with open(path, "w", encoding="latin-1", newline="") as f:
        f.write("\r\n".join(x for _, pair in lines for x in pair) + "\r\n")


def build_fixtures(engine, langdef, where):
    """The fixtures of the engine's tests/576_webhttrack-lang-symbol-keys.test."""
    symbols = symbol_table(langdef)
    if (symbols.get("Start"), symbols.get("Start1")) != (START, REPEAT):
        sys.exit(
            "lang.def keys the two copies of 'Start' as %s and %s, not %s and %s: the "
            "harness controls and the --selftest 'catalog keys' block name the old pair"
            % (symbols.get("Start"), symbols.get("Start1"), START, REPEAT)
        )
    french = keyed(os.path.join(engine, "lang", "Francais.txt"), symbols)
    english = keyed(os.path.join(engine, "lang", "English.txt"), symbols)
    repeats = sum(1 for text, internal, _ in french if internal and text + "1" in symbols)
    if repeats < 20:
        sys.exit("only %d repeated French texts: the fixtures prove nothing" % repeats)
    if [v for _, internal, v in english if internal == REPEAT] != ["Start"]:
        sys.exit("English does not resolve %s to 'Start', which the gap check expects"
                 % REPEAT)
    write_fixture(os.path.join(where, "fr-none.txt"), french, lambda i: False)
    write_fixture(os.path.join(where, "fr-all.txt"), french, lambda i: True)
    write_fixture(os.path.join(where, "fr-odd-last.txt"), french, lambda i: i % 2 == 1,
                  moved=True)
    write_fixture(os.path.join(where, "en-none.txt"), english, lambda i: False)
    write_fixture(os.path.join(where, "en-all.txt"), english, lambda i: True)
    # The gap: French without the second copy of a repeated text, which only a
    # symbol-keyed English can fill.
    gap = [row for row in french if row[1] != REPEAT]
    if len(gap) != len(french) - 1:
        sys.exit("dropping %s left %d of %d French pairs" % (REPEAT, len(gap), len(french)))
    write_fixture(os.path.join(where, "fr-gap.txt"), gap, lambda i: False)


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

    # 1. newlang.cpp has to carry the five rules the harness replays, since the harness
    #    compiles its own copy of the loader and so cannot speak for the shipped one.
    #    Check 5 there proves the two fallback directions resolve differently.
    loader = read(os.path.join(ROOT, "WinHTTrack", "newlang.cpp"))
    packed = pack(loader)
    for shape, rule in RULES:
        if shape not in packed:
            sys.exit(
                "newlang.cpp does not carry %s: expected '%s', whitespace apart"
                % (rule, shape)
            )
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
        # Keyed by a symbol the harness's synthetic lang.def also lists, which is the only
        # way to tell the guarded fallback from the inverted one.
        guard = os.path.join(tmp, "guard.txt")
        with open(guard, "w", encoding="utf-8") as f:
            f.write("LANG_PROBE\nvia lang.def\n")
        build_fixtures(args.engine, langdef, tmp)
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
        env = dict(os.environ, NEWLANG_KEY_PROBE=probe, NEWLANG_KEY_GUARD=guard,
                   NEWLANG_PARITY_DIR=tmp)
        subprocess.run([exe, args.engine], check=True, env=env)


if __name__ == "__main__":
    main()
