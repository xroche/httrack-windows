#!/usr/bin/env python3
"""Table test for check-stop-title.py, over a synthesised catalog tree.

Needs no engine checkout, so it runs in the encoding job while the guard itself
runs where httrack/lang exists. Four of the rows below are shapes the first version
of the guard passed, so they are the reason this file exists (#176).
"""
import importlib.util
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

MOD = Path(__file__).with_name("check-stop-title.py")
spec = importlib.util.spec_from_file_location("check_stop_title", MOD)
guard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guard)

MSGID = r"Mirroring operation stopped before the end.\nThe files already downloaded are kept."
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


print("--- decode() resolves the escapes the engine resolves ---")
DECODED = [
    (r"a\nb", "a\nb", "a line break"),
    (r"a\rb", "a\rb", "a carriage return"),
    (r"a\tb", "a\tb", "a tab"),
    (r"a\\nb", "a\\nb", "a doubled backslash is one backslash, so no break at all"),
    (r"a\\\nb", "a\\\nb", "three backslashes are one backslash then a break"),
    ("a\\", "a\\", "a trailing backslash with nothing to escape"),
    (r"a\qb", "aqb", "an escape nothing maps drops the backslash"),
    ("plain", "plain", "no escapes"),
]
for raw, want, why in DECODED:
    expect(f"{why}: {raw}", guard.decode(raw), want)

print("--- complain() judges a decoded value ---")
JUDGED = [
    ("Stopped.\nFiles kept.", None, "a break inside the title"),
    ("Stopped.\rFiles kept.", None, "a carriage return counts, as firstLineOf does"),
    ("Stopped before the end. Files kept.", "runs on", "no break at all"),
    ("\nStopped.\nFiles kept.", "opens with", "a leading break leaves the title empty"),
    ("x" * 255 + "\ny", None, "a first line just inside the title"),
    ("x" * 256 + "\ny", "past the", "a first line one byte too long"),
    ("Ø" * 128 + "\ny", "past the", "256 bytes of two-byte characters, not 256 characters"),
    ("Stopped.\tFiles kept.", "runs on", "a tab is not a line break"),
]
for text, want, why in JUDGED:
    got = guard.complain("Cat.txt", text)
    # Spelt out rather than a conditional: `want in got` raises when want is None and got
    # is a string, so a failing row would crash instead of naming itself.
    ok = got is None if want is None else got is not None and want in got
    expect(f"{why} -> {got}", ok, True)

print("--- entries() refuses a file whose pairs do not line up ---")
with TemporaryDirectory() as d:
    root = Path(d)
    (root / "lang").mkdir()
    good = f"LANGUAGE_NAME\nTest\n{MSGID}\n{MSGID}\n"
    for name, text, want in [
        ("pairs.txt", good, {"LANGUAGE_NAME": "Test", MSGID: MSGID}),
        ("odd.txt", good + "ORPHAN_KEY\n", None),
    ]:
        (root / "lang" / name).write_text(text, encoding="utf-8")
        try:
            got = guard.entries(root / "lang" / name)
        except SystemExit:
            got = None
        expect(name, got, want)
    # The value repeats the key here, as it does in all 30 shipped catalogs, so a guard
    # scanning line by line would match the value and hand back the next entry's text.
    expect("an untranslated entry is read as itself, not as a key",
           guard.entries(root / "lang" / "pairs.txt")[MSGID], MSGID)

print("--- main() over a whole tree ---")


def tree(d, values):
    root = Path(d)
    (root / "lang").mkdir()
    (root / "lang.def").write_text(f"LANG_F22s\n{MSGID}\n", encoding="utf-8")
    for i, v in enumerate(values):
        (root / "lang" / f"Lang{i:02d}.txt").write_text(
            f"LANGUAGE_NAME\nLang{i:02d}\n{MSGID}\n{v}\n", encoding="utf-8")
    return root


def run(values):
    """main()'s exit status over a tree of these LANG_F22s values: None when it passes."""
    with TemporaryDirectory() as d:
        argv = sys.argv
        try:
            sys.argv = ["check-stop-title.py", str(tree(d, values))]
            guard.main()
            return None
        except SystemExit as e:
            return e.code
        finally:
            sys.argv = argv


OK = MSGID
for values, want, why in [
    ([OK] * 30, None, "thirty sound catalogs pass"),
    ([OK] * 29 + [r"Stopped before the end. Files kept."], "runs on",
     "one catalog in thirty with no break"),
    ([OK] * 29 + [r"\nStopped.\nKept."], "opens with", "one catalog opening with a break"),
    ([OK] * 29 + [r"Stopped.\\nKept."], "runs on", "one catalog whose backslash is escaped"),
    ([OK] * 19, "would prove nothing", "too few catalogs to prove anything"),
]:
    got = run(values)
    ok = got is None if want is None else got is not None and want in str(got)
    expect(f"{why} -> {got}", ok, True)

# Exact, not a floor: a floor passes the deletion it exists to catch (#126, #127).
if ran != 24:
    sys.exit(f"{ran} cases ran, expected 24: rows have gone missing or been added")
if fail:
    sys.exit(f"{fail} stopped-title case(s) failed")
print(f"::notice::stopped-title guard: {ran} cases pass")
