#!/usr/bin/env python3
"""Fail if a language catalog's LANG_F22s has no line break to title the panel with.

The stopped-mirror panel titles itself from the first line of its own message
(httrack-windows#176), so a catalog whose LANG_F22s runs on unbroken would put the
whole message in the title bar. Nothing on the engine side pins that: its
62_lang-linebreaks test counts per-catalog drift and names no key, and it skips
English, which is the one every untranslated copy inherits.

Offsets are counted in the catalog's UTF-8 bytes. The GUI converts to the ANSI
codepage before cutting, and no codepage is longer than UTF-8 for the same text,
so a break inside the buffer here is inside the buffer there.
"""
import sys
from pathlib import Path

KEY = "LANG_F22s"
TITLE_MAX = 256  # sizeof(end_mirror_title) in WinHTTrack/Shell.h


def value_of(lines, key):
    """KEY's value, or None. Both files are strict key/value pairs from the first line,
    and stepping by two is what keeps an untranslated entry, whose value repeats its key,
    from matching as a key and handing back the next entry's."""
    for i in range(0, len(lines) - 1, 2):
        if lines[i] == key:
            return lines[i + 1]
    return None


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "httrack")
    defs = (root / "lang.def").read_text(encoding="utf-8").split("\n")
    # The catalogs are keyed by the English text, which lang.def maps the macro to.
    msgid = value_of(defs, KEY)
    if not msgid:
        sys.exit("%s carries no %s" % (root / "lang.def", KEY))

    catalogs = sorted((root / "lang").glob("*.txt"))
    bad, checked = [], 0
    for path in catalogs:
        lines = path.read_text(encoding="utf-8").split("\n")
        text = value_of(lines, msgid)
        if text is None:
            bad.append("%s: no %s entry" % (path.name, KEY))
            continue
        checked += 1
        at = text.find("\\n")
        if at < 0:
            bad.append("%s: %s runs on with no line break, so its whole body would be "
                       "the window title" % (path.name, KEY))
        elif len(text[:at].encode("utf-8")) >= TITLE_MAX:
            bad.append("%s: %s breaks at %d bytes, past the %d-byte title"
                       % (path.name, KEY, len(text[:at].encode("utf-8")), TITLE_MAX))
    # The floor is the control: a msgid that matched nothing would report every catalog clean.
    if checked < 20:
        sys.exit("only %d of %d catalogs carry %s: this check would prove nothing"
                 % (checked, len(catalogs), KEY))
    if bad:
        sys.exit("\n".join(bad))
    print("%s breaks inside the title in all %d catalogs" % (KEY, checked))


if __name__ == "__main__":
    main()
