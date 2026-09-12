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


# What linput() drops before the catalog text is ever compared (engine htslib.c).
DROPPED = {ord(c): None for c in "\r\t\f"}


def logical_lines(path):
    """The file's lines as linput_cpp() hands them over, not as they sit on disk.

    It drops CR, tab and form feed, trims spaces and tabs off both ends, and joins a line
    ending in a backslash to the one after it. A guard comparing raw text would fail a
    catalog over a trailing space the program never sees, and a continuation would shift
    every later key onto the wrong half of its pair without the guard noticing.
    """
    out, joined = [], ""
    # newline="" keeps CRLF and a lone CR intact, so the drop below is this guard's own and
    # not Python's. The engine does not end a line on CR, and universal newlines would.
    with path.open(encoding="utf-8", newline="") as fp:
        text = fp.read()
    for raw in text.split("\n"):
        line = raw.translate(DROPPED).strip(" \t")
        if line.endswith("\\"):
            joined += line[:-1]
            continue
        out.append(joined + line)
        joined = ""
    if joined:
        out.append(joined)
    if out and out[-1] == "":
        out.pop()  # the newline every catalog ends with
    return out


def carried(path):
    """Every key this catalog actually defines, in file order, repeats kept.

    A pair whose key or value is empty is skipped by LANG_LOAD(), so its key line does not
    define anything however present it looks. Empty values already ship in quantity, none
    yet on a repeated key, and one landing there is exactly the blank this guard is for.

    check-stop-title.py parses the same files into a dict, which collapses exactly the
    repeats this guard exists to count, so the two parsers stay apart on purpose.
    """
    lines = logical_lines(path)
    if len(lines) % 2 != 0:
        sys.exit(f"{path.name} holds {len(lines)} lines, an odd number, so its key/value "
                 f"pairs do not line up and every key past the gap would be read as a value")
    return [lines[i] for i in range(0, len(lines), 2) if lines[i] and lines[i + 1]]


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
    repeated = {k: n for k, n in Counter(carried(english)).items() if n > 1}
    if len(repeated) < MIN_REPEATED:
        sys.exit(f"English.txt repeats only {len(repeated)} keys, under the {MIN_REPEATED} this "
                 f"check needs to prove anything")

    bad, checked = [], 0
    # English against itself can never be short, so that row would be a vacuous pass.
    for path in sorted((root / "lang").glob("*.txt")):
        if path.name == english.name:
            continue
        checked += 1
        bad += shortfalls(path.name, Counter(carried(path)), repeated)
    if checked < MIN_CATALOGS:
        sys.exit(f"only {checked} catalogs read, under the {MIN_CATALOGS} this check needs to "
                 f"prove anything")
    if bad:
        sys.exit("\n".join(bad))
    print(f"all {len(repeated)} repeated keys are complete in all {checked} catalogs")


if __name__ == "__main__":
    main()
