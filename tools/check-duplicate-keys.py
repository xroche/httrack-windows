#!/usr/bin/env python3
"""Fail if a catalog carries fewer copies of a repeated English key than English does.

Two catalog lines can share one English text, and LANG_LOAD() in WinHTTrack/newlang.cpp
tells them apart by position: the Nth line resolves through the Nth spelling, "Exit",
then "Exit1", then "Exit2". Its English backfill pass does not walk that chain. It stops
at the first spelling that already holds a value, so it can fill at most the first copy,
and any later copy the catalog itself did not carry renders blank (httrack-windows#171).

Every shipped catalog carries every copy today, which is why #171 is unreachable and was
left unfixed. This is the tripwire for the day that stops being true, not a fix. It fires
on the catalog that would expose the bug, before the blank reaches a user.

A catalog missing a key English states ONCE is fine and common, because the backfill does
cover that. Only a short count on a repeated key is the reachable shape.
"""
import sys
from collections import Counter
from pathlib import Path

# Controls. Both guard against a parse that silently yields nothing, which would report
# every catalog clean. English repeats 27 keys, and 29 other catalogs ship beside it.
MIN_REPEATED = 20
MIN_CATALOGS = 25


def key_lines(path):
    """The key half of a catalog, in file order, repeats kept.

    check-stop-title.py parses the same files into a dict, which collapses exactly the
    repeats this guard exists to count, so the two parsers stay apart on purpose.

    Both files are strict pairs from the first line, so a line inserted anywhere shifts
    every later key onto an odd index. Stepping by two would then read one entry's value
    as a key, so the parity is checked, not assumed.
    """
    lines = path.read_text(encoding="utf-8").split("\n")
    if len(lines) % 2 != 1:
        sys.exit(f"{path.name} has {len(lines) - 1} lines, an odd number, so its key/value "
                 f"pairs do not line up and every key past the gap would be read as a value")
    return [lines[i] for i in range(0, len(lines) - 1, 2)]


def shortfalls(name, counts, repeated):
    """Which repeated keys this catalog is short of, worst first."""
    short = [(want - counts.get(key, 0), key, counts.get(key, 0), want)
             for key, want in repeated.items() if counts.get(key, 0) < want]
    return [f"{name}: {got} of English's {want} copies of {key!r}, and the backfill fills "
            f"at most one, so a later copy renders blank (#171)"
            for _, key, got, want in sorted(short, reverse=True)]


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "httrack")
    english = root / "lang" / "English.txt"
    repeated = {k: n for k, n in Counter(key_lines(english)).items() if n > 1}
    if len(repeated) < MIN_REPEATED:
        sys.exit(f"English.txt repeats only {len(repeated)} keys, under the {MIN_REPEATED} this "
                 f"check needs to prove anything")

    bad, checked = [], 0
    # English against itself can never be short, so that row would be a vacuous pass.
    for path in sorted((root / "lang").glob("*.txt")):
        if path.name == english.name:
            continue
        checked += 1
        bad += shortfalls(path.name, Counter(key_lines(path)), repeated)
    if checked < MIN_CATALOGS:
        sys.exit(f"only {checked} catalogs read, under the {MIN_CATALOGS} this check needs to "
                 f"prove anything")
    if bad:
        sys.exit("\n".join(bad))
    print(f"all {len(repeated)} repeated keys are complete in all {checked} catalogs")


if __name__ == "__main__":
    main()
