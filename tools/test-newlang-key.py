#!/usr/bin/env python3
"""Compile and run tools/newlang-key-test.cpp over the engine's catalogs, then rerun it
against a loader mutated one ordering rule at a time.

A catalog keys its rows on the English text, so rewording a string orphans its 30
translations. The loader in WinHTTrack/newlang-key.cpp accepts a LANG_ or LISTDEF_ symbol
as the key instead. Nothing ships symbol-keyed yet, so the symbol path cannot be reached
and no string changes, which is what check 2 below carries.

The harness builds the fixtures of the engine's
tests/576_webhttrack-lang-symbol-keys.test, which re-key a catalog in whole or in part,
and resolves them through the shipped loader. Each mutation in MUTATIONS breaks one of the
five rules the engine's lang_load_catalog() follows and has to red the harness, and the
last entry rewrites the two passes as a loop and has to stay green: that pair is what says
the rules are checked by what the loader does rather than by how it is written.

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

# One mutation per rule the engine's lang_load_catalog() follows, each breaking it in the
# shipped loader. `where` has to appear exactly once in WinHTTrack/newlang-key.cpp, `fix`
# replaces it, and `says` is what the harness has to answer with. The last entry rewrites
# the two passes as a loop, which changes nothing and has to stay green: a pattern over the
# source failed both ways round, waving the rule 4 mutation through and rejecting that
# rewrite.
Mutation = collections.namedtuple("Mutation", "rule where fix says red")

MUTATIONS = (
    Mutation(
        "rule 1, the text lookup deciding before the shape test",
        "is_symbol = (!notempty(intkey) && LangKeyIsSymbol(extkey)) ? 1 : 0;",
        "is_symbol = LangKeyIsSymbol(extkey) ? 1 : 0;",
        "the shape test is deciding before the text lookup",
        True,
    ),
    Mutation(
        "rules 2 and 3, each kind of line left to its own pass",
        """    if (is_symbol != symbols)
      continue;""",
        "    (void) is_symbol;",
        "strings resolved under a key lang.def never names",
        True,
    ),
    Mutation(
        "rule 2, the first symbol line winning",
        """      if (notempty(io->resolved(io->ctx, extkey)))
        continue;
      intkey = extkey;""",
        "      intkey = extkey;",
        "re-keying both catalogs",
        True,
    ),
    Mutation(
        "rule 2, no symbol line ever incrementing",
        "      intkey = extkey;",
        """      {
        size_t pos = strlen(extkey);
        int increment = 0;

        do {
          increment++;
          sprintf(extkey + pos, "%d", increment);
        }  while(notempty(io->resolved(io->ctx, extkey)));
      }
      intkey = extkey;""",
        "re-keying every French line",
        True,
    ),
    Mutation(
        "rule 3, a repeated text taking the next copy's slot",
        """        size_t pos = strlen(extkey);
        int increment = 0;

        if (fallback)
          continue;
        /* Repeated English text: lang.def keys its Nth copy as text + N. */
        do {
          increment++;
          sprintf(extkey + pos, "%d", increment);
          intkey = io->internalKey(io->ctx, extkey);
        }  while(notempty(intkey) && notempty(io->resolved(io->ctx, intkey)));
        if (!notempty(intkey))
          continue;""",
        """        (void) fallback;
        continue;""",
        "the text-keyed French catalog resolved LANG_J13",
        True,
    ),
    Mutation(
        "rule 4, each catalog's symbol lines read inside the language loop",
        "  for(index = 0 ; index < 2 ; index++) {",
        """  FILE *const hoisted = io->open(io->ctx, 1);

  LangLoadCatalogPass(io, hoisted, 1, 1);
  fclose(hoisted);
  for(index = 0 ; index < 2 ; index++) {""",
        "re-keying English alone",
        True,
    ),
    Mutation(
        "rule 5, the English pass filling only empty slots",
        """        if (fallback)
          continue;""",
        "        (void) fallback;",
        "fr-gap.txt still resolves LANG_J13",
        True,
    ),
    Mutation(
        "the same two passes, called from a loop",
        """    LangLoadCatalogPass(io, fp, index > 0, 1);
    LangLoadCatalogPass(io, fp, index > 0, 0);""",
        """    int symbols;

    for(symbols = 1 ; symbols >= 0 ; symbols--)
      LangLoadCatalogPass(io, fp, index > 0, symbols);""",
        "",
        False,
    ),
)


def read(path, encoding="utf-8"):
    with open(path, encoding=encoding) as f:
        return f.read()


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


def build(cxx, exe, loader):
    """The harness, linked against `loader` as its catalog loader."""
    subprocess.run(
        [
            cxx,
            "-std=c++14",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-o",
            exe,
            os.path.join(ROOT, "tools", "newlang-key-test.cpp"),
            loader,
            "-I" + os.path.join(ROOT, "WinHTTrack"),
        ],
        check=True,
    )


def mutate(source, mutation, path):
    """`source` with one rule broken, written to `path`."""
    if source.count(mutation.where) != 1:
        sys.exit(
            "newlang-key.cpp holds %d copies of what %s mutates, so the mutation says "
            "nothing: expected '%s'"
            % (source.count(mutation.where), mutation.rule, mutation.where)
        )
    with open(path, "w", encoding="utf-8") as f:
        f.write(source.replace(mutation.where, mutation.fix))


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

    # 1. The loader lives in its own unit, so the harness links the shipped one. A copy
    #    back in newlang.cpp would be tested by nothing.
    if "LangKeyIsSymbol" in read(os.path.join(ROOT, "WinHTTrack", "newlang.cpp")):
        sys.exit("newlang.cpp resolves a catalog key itself, so it holds a second loader")

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

    # 4. Resolve every catalog and every re-keyed fixture through the shipped loader, with
    #    a symbol-keyed probe proving the symbol path can be taken at all.
    with tempfile.TemporaryDirectory() as tmp:
        probe = os.path.join(tmp, "probe.txt")
        with open(probe, "w", encoding="utf-8") as f:
            f.write("LANG_OK\nprobe value\n")
        # Keyed by a symbol the harness's synthetic lang.def also lists, which is the only
        # way to tell the shipped order from the inverted one.
        guard = os.path.join(tmp, "guard.txt")
        with open(guard, "w", encoding="utf-8") as f:
            f.write("LANG_PROBE\nvia lang.def\n")
        build_fixtures(args.engine, langdef, tmp)
        exe = os.path.join(tmp, "newlang-key-test")
        env = dict(os.environ, NEWLANG_KEY_PROBE=probe, NEWLANG_KEY_GUARD=guard,
                   NEWLANG_PARITY_DIR=tmp)
        shipped = os.path.join(ROOT, "WinHTTrack", "newlang-key.cpp")
        build(args.cxx, exe, shipped)
        subprocess.run([exe, args.engine], check=True, env=env)

        # 5. Each rule broken in turn has to red the harness, naming the rule it broke,
        #    and the rewritten pass loop has to stay green.
        source = read(shipped)
        killed, survived = 0, 0
        for i, mutation in enumerate(MUTATIONS):
            loader = os.path.join(tmp, "mutant-%d.cpp" % i)
            mutate(source, mutation, loader)
            build(args.cxx, exe + "-mutant", loader)
            ran = subprocess.run([exe + "-mutant", args.engine], env=env,
                                 capture_output=True, text=True)
            if mutation.red:
                if ran.returncode == 0:
                    sys.exit("breaking %s changed nothing the harness checks" % mutation.rule)
                if mutation.says not in ran.stderr:
                    sys.exit(
                        "breaking %s was caught by something else: expected '%s', got\n%s"
                        % (mutation.rule, mutation.says, ran.stderr)
                    )
                killed += 1
            else:
                if ran.returncode != 0:
                    sys.exit("%s reds the harness:\n%s" % (mutation.rule, ran.stderr))
                survived += 1
        print("%d broken rules each red the harness and %d rewrite of the same rules "
              "stays green" % (killed, survived))


if __name__ == "__main__":
    main()
