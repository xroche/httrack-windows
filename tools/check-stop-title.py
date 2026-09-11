#!/usr/bin/env python3
"""Fail if a language catalog's LANG_F22s has no line break to title the panel with.

The stopped-mirror panel titles itself from the first line of its own message
(httrack-windows#176). A catalog whose LANG_F22s runs on unbroken would put the whole
message in the title bar, and one that opens with a break would leave the title empty,
which falls back to LANG_F18b and says the mirror finished again.

Nothing on the engine side pins this: its 62_lang-linebreaks test counts per-catalog
drift, names no key, and skips English, the copy every untranslated catalog inherits.

Offsets are counted in the catalog's UTF-8 bytes. The GUI converts to the ANSI codepage
before cutting, and no codepage this project ships is longer than UTF-8 for the same
text, because the engine blocks best-fit and an unmappable character becomes one default
byte. So a break inside the buffer here is inside the buffer there.
"""
import sys
from pathlib import Path

KEY = "LANG_F22s"
TITLE_MAX = 256  # sizeof(end_mirror_title) in WinHTTrack/Shell.h
BREAKS = "\n\r"  # firstLineOf() stops at either


def decode(text):
    """The catalog's escapes resolved, as the engine resolves them before the GUI cuts.

    A doubled backslash is one backslash and not the start of an escape, so "a\\\\nb" has
    no break in it at all.
    """
    out, i = [], 0
    while i < len(text):
        if text[i] == "\\" and i + 1 < len(text):
            out.append({"n": "\n", "r": "\r", "t": "\t"}.get(text[i + 1], text[i + 1]))
            i += 2
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def entries(path):
    """A file's key/value pairs. Both files are strict pairs from the first line, so a
    line inserted anywhere shifts every later key onto an odd index. Stepping by two
    would then read one entry's value as a key, so the parity is checked, not assumed."""
    lines = path.read_text(encoding="utf-8").split("\n")
    if len(lines) % 2 != 1:
        sys.exit(f"{path.name} has {len(lines) - 1} lines, an odd number, so its key/value "
                 f"pairs do not line up and every key past the gap would be read as a value")
    return {lines[i]: lines[i + 1] for i in range(0, len(lines) - 1, 2)}


def complain(name, text):
    """What is wrong with this catalog's LANG_F22s, or None."""
    at = min((text.find(b) for b in BREAKS if b in text), default=-1)
    if at < 0:
        return (f"{name}: {KEY} runs on with no line break, so its whole body would be "
                f"the window title")
    if at == 0:
        # An empty title falls back to LANG_F18b, which is the bug #176 is about.
        return f"{name}: {KEY} opens with a line break, so the title would be empty"
    width = len(text[:at].encode("utf-8"))
    if width >= TITLE_MAX:
        return f"{name}: {KEY} breaks at {width} bytes, past the {TITLE_MAX}-byte title"
    return None


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "httrack")
    # The catalogs are keyed by the English text, which lang.def maps the macro to.
    msgid = entries(root / "lang.def").get(KEY)
    if not msgid:
        sys.exit(f"{root / 'lang.def'} carries no {KEY}")

    catalogs = sorted((root / "lang").glob("*.txt"))
    bad, checked = [], 0
    for path in catalogs:
        text = entries(path).get(msgid)
        if text is None:
            bad.append(f"{path.name}: no {KEY} entry")
            continue
        checked += 1
        hit = complain(path.name, decode(text))
        if hit:
            bad.append(hit)
    # The floor is the control: a msgid matching nothing would report every catalog clean.
    if checked < 20:
        sys.exit(f"only {checked} of {len(catalogs)} catalogs carry {KEY}: this check would "
                 f"prove nothing")
    if bad:
        sys.exit("\n".join(bad))
    print(f"{KEY} breaks inside the title in all {checked} catalogs")


if __name__ == "__main__":
    main()
