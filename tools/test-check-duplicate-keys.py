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


def catalog(keys):
    """A catalog whose every value is the key back, so only the key column varies."""
    return "\n".join(f"{k}\n{k} translated" for k in keys) + "\n"


# English repeats "Exit" three times and "Cancel" twice, plus filler to clear the floors.
FILLER = [f"Key{i}" for i in range(40)]
ENGLISH = ["Exit", "Cancel", "Exit", "Only once", "Exit", "Cancel"] + FILLER
# Each filler key is repeated too, so the tree clears MIN_REPEATED without the rows above.
ENGLISH += FILLER


def tree(tmp, catalogs):
    """A lang/ tree: English plus MIN_CATALOGS copies of the given catalogs."""
    root = Path(tmp)
    (root / "lang").mkdir(parents=True)
    (root / "lang" / "English.txt").write_text(catalog(ENGLISH), encoding="utf-8")
    for i, keys in enumerate(catalogs):
        (root / "lang" / f"Lang{i:02d}.txt").write_text(catalog(keys), encoding="utf-8")
    return root


def run(catalogs):
    """The guard's verdict on a tree: None when it passes, else what it exited with."""
    with TemporaryDirectory() as tmp:
        root = tree(tmp, catalogs)
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
    expect(f"{what} is reported", got is not None and key in got and want in got, True)
    expect(f"{what} names the catalog", got is not None and "Lang00.txt" in got, True)

print("--- shapes the bug cannot take are left alone ---")
expect("a key English states once may be missing",
       run([[k for k in ENGLISH if k != "Only once"]] + [ENGLISH] * (guard.MIN_CATALOGS - 1)),
       None)
expect("more copies than English is not this bug",
       run([ENGLISH + ["Exit"]] + [ENGLISH] * (guard.MIN_CATALOGS - 1)), None)

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

expect("too few catalogs proves nothing",
       "only" in (run([ENGLISH] * 3) or ""), True)

print("--- an odd line count is refused rather than misread ---")
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

print(f"\n{ran - fail}/{ran} checks passed")
sys.exit(1 if fail else 0)
