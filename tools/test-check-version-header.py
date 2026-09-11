#!/usr/bin/env python3
"""Table test for check-version-header.py, plus the mutations it must catch.

The guard only ever sees one header, and that header agrees with itself, so a
guard that returned "no complaints" unconditionally would look just as green.
The mutation rows are what tell the two apart.
"""
import importlib.util
import sys
from pathlib import Path

MOD = Path(__file__).with_name("check-version-header.py")
spec = importlib.util.spec_from_file_location("check_version_header", MOD)
guard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guard)

REAL = Path(__file__).resolve().parent.parent / "WinHTTrack" / "version.h"

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


print("--- dotted() over every shape version.h has carried, and the ones it must refuse ---")
DERIVED = [
    ("3.50", (3, 50, 0, 0), "a bare release"),
    ("3.50-2", (3, 50, 2, 0), "a patch release takes the third field"),
    ("3.50-beta-8", (3, 49, 99, 8), "a beta borrows a minor so 3.50 outranks it"),
    ("3.50-beta-1", (3, 49, 99, 1), "the first beta"),
    ("4.0-beta-1", (3, 99, 99, 1), "a beta of an x.0 borrows a major"),
    ("0.0-beta-1", None, "nothing sorts below 0.0"),
    ("3.50-beta", None, "beta with no number"),
    ("3.50-beta-x", None, "beta number is not a number"),
    ("v3.50-2", None, "v-prefix"),
    ("3.50.2", None, "already dotted"),
    ("3.50-2-beta-1", None, "a beta of a patch release: teach the rule before shipping one"),
    ("3.50 ", None, "trailing space"),
    (" 3.50", None, "leading space"),
    ("3.50\n", None, "trailing newline, which a bare $ would have let through"),
    ("", None, "empty"),
    ("3.50-70000", None, "a patch number past the 16-bit field"),
    ("3.50-beta-70000", None, "a beta number past the 16-bit field"),
]
for version, want, why in DERIVED:
    expect(f"{why}: {version or '(empty)'}", guard.dotted(version), want)

print("--- the real header agrees with itself ---")
real = REAL.read_text(encoding="utf-8")
expect("WinHTTrack/version.h", guard.check(real, str(REAL)), [])

SYNTH = (
    "#ifndef WINHTTRACK_VERSION_H\n"
    "#define WINHTTRACK_VERSION_H\n"
    '#define WINHTTRACK_VERSION "3.50-beta-7"\n'
    '#define WINHTTRACK_VERSIONID "3.49.99.7"\n'
    "#define WINHTTRACK_VERSION_NUM 3, 49, 99, 7\n"
    "#endif\n"
)
print("--- macros() binds to the right macro ---")
expect("picks VERSION, not VERSIONID/NUM/_H",
       guard.macros(SYNTH), ("3.50-beta-7", "3.49.99.7", (3, 49, 99, 7)))
expect("a consistent synthetic header passes", guard.check(SYNTH), [])

print("--- a damaged header is caught, and says which macro is wrong ---")
# Each row edits one macro of SYNTH, and names the complaints it must raise, by the macro
# each one has to name. Counting alone would keep a guard that blames the wrong macro green.
MUTANTS = [
    ('"3.49.99.7"', '"3.50.0.7"', ["WINHTTRACK_VERSIONID"], "VERSIONID alone is wrong"),
    ("3, 49, 99, 7", "3, 49, 99, 8", ["WINHTTRACK_VERSION_NUM"], "VERSION_NUM alone is wrong"),
    ('"3.50-beta-7"', '"3.50-7"', ["WINHTTRACK_VERSIONID", "WINHTTRACK_VERSION_NUM"],
     "VERSION bumped out of beta, the other two left behind"),
    ('"3.50-beta-7"', '"3.50-beta-8"', ["WINHTTRACK_VERSIONID", "WINHTTRACK_VERSION_NUM"],
     "the beta number bumped in one place only"),
    ('"3.49.99.7"', '"3.49.99.70"', ["WINHTTRACK_VERSIONID"], "a digit appended to VERSIONID"),
    ('"3.49.99.7"', '"3.49.99.7 "', ["WINHTTRACK_VERSIONID"], "a trailing space in VERSIONID"),
    # Settled, not a defect: a leading zero names the same release, and refusing one
    # would also refuse a two-digit minor like 3.05.
    ('"3.50-beta-7"', '"3.50-beta-07"', [], "a leading zero still agrees"),
]
for old, new, want, why in MUTANTS:
    mutant = SYNTH.replace(old, new)
    if mutant == SYNTH:
        print(f"  FAIL {why} : the edit {old!r} -> {new!r} changed nothing")
        fail += 1
        continue
    bad = guard.check(mutant)
    expect(why, [m for m in want if not any(m in line for line in bad)], [])
    expect(f"{why}, and says nothing else", len(bad), len(want))

print("--- a definition the preprocessor would not use is an error, not a pass ---")
# The live macros below disagree. A guard reading the FIRST match reads the dead triple
# above them instead and reports clean, which is the very state #175 exists to catch.
LIVE = ('#define WINHTTRACK_VERSION "3.50-2"\n'
        '#define WINHTTRACK_VERSIONID "3.99.9.9"\n'
        "#define WINHTTRACK_VERSION_NUM 3, 99, 9, 9\n")
for dead, why in [
    ("/* was:\n" + SYNTH + "*/\n", "a commented-out triple above the live macros"),
    ("#if 0\n" + SYNTH + "#endif\n", "an #if 0 triple above the live macros"),
    ('#define WINHTTRACK_VERSION "3.50-1"\n', "a stale VERSION line left by a bump"),
]:
    exited = ""
    try:
        guard.check(dead + LIVE)
    except SystemExit as e:
        exited = str(e.code)
    expect(why, "defines WINHTTRACK_VERSION" in exited and "times" in exited, True)

print("--- a header the guard cannot read is an error, not a pass ---")
for text, macro, why in [
    (SYNTH.replace('#define WINHTTRACK_VERSION "3.50-beta-7"\n', ""), "VERSION", "VERSION absent"),
    (SYNTH.replace('#define WINHTTRACK_VERSIONID "3.49.99.7"\n', ""), "VERSIONID",
     "VERSIONID absent"),
    (SYNTH.replace("#define WINHTTRACK_VERSION_NUM 3, 49, 99, 7\n", ""), "VERSION_NUM",
     "VERSION_NUM absent"),
]:
    exited = ""
    try:
        guard.check(text)
    except SystemExit as e:
        exited = str(e.code)
    # The message, not just the exit: any raise would satisfy a bare except.
    expect(why, exited, f"cannot read WINHTTRACK_{macro} from {guard.HEADER}")

print("--- an unknown VERSION shape is an error, not a pass ---")
expect("VERSION shape the rule does not cover",
       ["no dotted form" in line for line in guard.check(SYNTH.replace('"3.50-beta-7"',
                                                                       '"3.50rc1"'))], [True])

# Counted, not written down: a row deleted by hand would otherwise still be claimed. Exact,
# not a floor, because a floor passes the deletion it exists to catch (#126, #127).
# 17 derived + 2 per mutant + 10 standalone.
if ran != 41:
    sys.exit(f"{ran} cases ran, expected 41: rows have gone missing or been added")
if fail:
    sys.exit(f"{fail} version-header case(s) failed")
print(f"::notice::version-header guard: {ran} cases pass")
