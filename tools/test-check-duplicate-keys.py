#!/usr/bin/env python3
"""Table test for check-duplicate-keys.py, over a synthesised catalog tree.

Needs no engine checkout, so it runs in the encoding job while the guard itself runs
where httrack/lang exists. Every row below damages the tree in one way and states what
the guard must say about it, because a guard nobody has damaged has never been proved
to notice anything (#167).
"""
import importlib.util
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

MOD = Path(__file__).with_name("check-duplicate-keys.py")
spec = importlib.util.spec_from_file_location("check_duplicate_keys", MOD)
guard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guard)

fail = 0
ran = 0


def expect(what, got, want):
    global fail, ran
    ran += 1
    if got != want:
        print(f"  FAIL {what} : got {got!r}, wanted {want!r}")
        fail += 1
    else:
        print(f"  ok   {what}")


def catalog(keys, tag="translated"):
    """A catalog of these keys. TAG varies the value column independently of the keys, so
    a row can tell a guard that reads the key half from one that reads the value half."""
    return "\n".join(f"{k}\n{k} {tag}" for k in keys) + "\n"


# English repeats "Exit" three times and "Cancel" twice, plus filler to clear the floors.
FILLER = [f"Key{i}" for i in range(40)]
ENGLISH = ["Exit", "Cancel", "Exit", "Only once", "Exit", "Cancel"] + FILLER
# Each filler key is repeated too, so the tree clears MIN_REPEATED without the rows above.
ENGLISH += FILLER


def tree(tmp, catalogs, tag="translated", extra=None, english=None):
    """A lang/ tree: English plus one file per entry in CATALOGS, plus any EXTRA files."""
    root = Path(tmp)
    (root / "lang").mkdir(parents=True)
    (root / "lang" / "English.txt").write_text(catalog(english or ENGLISH), encoding="utf-8")
    for i, keys in enumerate(catalogs):
        (root / "lang" / f"Lang{i:02d}.txt").write_text(catalog(keys, tag), encoding="utf-8")
    for name, text in (extra or {}).items():
        (root / "lang" / name).write_text(text, encoding="utf-8")
    return root


def run(catalogs, tag="translated", extra=None, english=None):
    """The guard's verdict on a tree: None when it passes, else what it exited with."""
    with TemporaryDirectory() as tmp:
        root = tree(tmp, catalogs, tag, extra, english)
        argv = sys.argv
        sys.argv = ["check-duplicate-keys.py", str(root)]
        try:
            guard.main()
            return None
        except SystemExit as e:
            return str(e.code)
        finally:
            sys.argv = argv


COMPLETE = [ENGLISH] * guard.MIN_CATALOGS

print("--- a tree nothing is wrong with passes ---")
expect("every catalog carries every copy", run(COMPLETE), None)

print("--- a short count on a repeated key fails, and names it ---")
ROWS = [
    ("two of three copies of Exit",
     [k for k in ENGLISH if k != "Exit"] + ["Exit", "Exit"], "Exit", "2 of English's 3"),
    ("one of three copies of Exit",
     [k for k in ENGLISH if k != "Exit"] + ["Exit"], "Exit", "1 of English's 3"),
    # The backfill fills the first copy only, so losing every copy still blanks the rest.
    ("no copies of Cancel at all",
     [k for k in ENGLISH if k != "Cancel"], "Cancel", "0 of English's 2"),
]
for what, keys, key, want in ROWS:
    got = run([keys] + [ENGLISH] * (guard.MIN_CATALOGS - 1))
    expect(f"{what} is reported", got is not None and repr(key) in got and want in got, True)
    expect(f"{what} names the catalog", got is not None and "Lang00.txt" in got, True)

print("--- shapes the bug cannot take are left alone ---")
expect("a key English states once may be missing",
       run([[k for k in ENGLISH if k != "Only once"]] + [ENGLISH] * (guard.MIN_CATALOGS - 1)),
       None)
expect("more copies than English is not this bug",
       run([ENGLISH + ["Exit"]] + [ENGLISH] * (guard.MIN_CATALOGS - 1)), None)
# Values that carry none of their key: a guard reading the value half sees English's
# values missing everywhere, so only a row whose columns differ can tell the halves apart.
expect("a catalog whose values are all translated",
       run(COMPLETE, tag="traduit"), None)
# lang/ really does hold Makefile.am and README.md, and neither is a catalog.
expect("a non-catalog file in lang/ is left alone",
       run(COMPLETE, extra={"README.md": "one\ntwo\nthree\n"}), None)

print("--- the guard reads a catalog the way LANG_LOAD does ---")
# An empty value makes LANG_LOAD skip the pair, so the key line defines nothing.
with TemporaryDirectory() as tmp:
    root = tree(tmp, COMPLETE)
    short = root / "lang" / "Lang00.txt"
    text = short.read_text(encoding="utf-8").split("\n")
    text[text.index("Exit", text.index("Exit", text.index("Exit") + 1) + 1) + 1] = ""
    short.write_text("\n".join(text), encoding="utf-8")
    sys.argv = ["check-duplicate-keys.py", str(root)]
    try:
        guard.main()
        verdict = None
    except SystemExit as e:
        verdict = str(e.code)
    expect("a copy whose value is empty defines nothing",
           verdict is not None and repr("Exit") in verdict and "2 of English's 3" in verdict, True)
# linput() drops CR and tab and linput_trim() strips the ends, so a catalog saved on
# Windows, or padded, carries the same keys. Only a CRLF row can see the CR being dropped.
with TemporaryDirectory() as tmp:
    root = tree(tmp, COMPLETE)
    padded = root / "lang" / "Lang00.txt"
    padded.write_text(catalog([f"\t {k} " for k in ENGLISH]).replace("\n", "\r\n"),
                      encoding="utf-8")
    sys.argv = ["check-duplicate-keys.py", str(root)]
    try:
        guard.main()
        verdict = None
    except SystemExit as e:
        verdict = str(e.code)
    expect("CRLF and padding the program never sees are not a shortfall", verdict, None)
# linput() treats CR as a character to drop, not as a line ending, so a file terminated
# with lone CRs is ONE line to the engine. Reading it with universal newlines would split
# it into many and report a tidy answer about a file the program cannot read at all.
with TemporaryDirectory() as tmp:
    root = tree(tmp, COMPLETE)
    mac = root / "lang" / "Lang00.txt"
    mac.write_text(catalog(ENGLISH).replace("\n", "\r"), encoding="utf-8", newline="")
    sys.argv = ["check-duplicate-keys.py", str(root)]
    try:
        guard.main()
        verdict = None
    except SystemExit as e:
        verdict = str(e.code)
    expect("CR alone does not end a line", verdict is not None and "odd number" in verdict, True)
# LANG_LOAD skips a pair with no key too, so a repeated empty key is not a repeated key.
expect("a pair with no key defines nothing",
       run([ENGLISH + [""]] * guard.MIN_CATALOGS, english=ENGLISH + ["", ""]), None)

print("--- the controls fire rather than pass vacuously ---")
with TemporaryDirectory() as tmp:
    root = tree(tmp, COMPLETE)
    (root / "lang" / "English.txt").write_text(catalog(["One", "Two", "Three"]), encoding="utf-8")
    sys.argv = ["check-duplicate-keys.py", str(root)]
    try:
        guard.main()
        verdict = None
    except SystemExit as e:
        verdict = str(e.code)
    expect("English with nothing repeated proves nothing",
           verdict is not None and "repeats only" in verdict, True)

# Exactly one under the floor: a guard that counted English too would clear it here.
expect("too few catalogs proves nothing",
       "only" in (run([ENGLISH] * (guard.MIN_CATALOGS - 1)) or ""), True)

print("--- a file whose pairs do not line up is refused rather than misread ---")
# linput_cpp() joins a line ending in a backslash, so a value ending in one eats the next
# key. The program loses that key too, which is why refusing the file is the right answer.
with TemporaryDirectory() as tmp:
    root = tree(tmp, COMPLETE)
    bad = root / "lang" / "Lang00.txt"
    text = bad.read_text(encoding="utf-8").split("\n")
    text[1] += "\\"
    bad.write_text("\n".join(text), encoding="utf-8")
    sys.argv = ["check-duplicate-keys.py", str(root)]
    try:
        guard.main()
        verdict = None
    except SystemExit as e:
        verdict = str(e.code)
    expect("a value ending in a backslash swallows the next key",
           verdict is not None and "odd number" in verdict, True)


with TemporaryDirectory() as tmp:
    root = tree(tmp, COMPLETE)
    bad = root / "lang" / "Lang00.txt"
    bad.write_text(bad.read_text(encoding="utf-8") + "orphan key\n", encoding="utf-8")
    sys.argv = ["check-duplicate-keys.py", str(root)]
    try:
        guard.main()
        verdict = None
    except SystemExit as e:
        verdict = str(e.code)
    expect("a value with no key is refused",
           verdict is not None and "odd number" in verdict, True)

# Exact, not a floor: a floor passes the deletion it exists to catch (#126, #127).
if ran != 19:
    sys.exit(f"{ran} cases ran, expected 19: rows have gone missing or been added")
if fail:
    sys.exit(f"{fail} repeated-key case(s) failed")
print(f"::notice::repeated-key guard: {ran} cases pass")
