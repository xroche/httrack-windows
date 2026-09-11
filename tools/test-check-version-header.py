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


def check(what, got, want):
    global fail, ran
    ran += 1
    if got != want:
        print("  FAIL %s : got %r, wanted %r" % (what, got, want))
        fail += 1
    else:
        print("  ok   %s" % what)


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
    ("3.50\n", None, "trailing newline, which a bare $ would have let through"),
    (" 3.50", None, "leading space"),
    ("", None, "empty"),
    ("3.50-70000", None, "a patch number past the 16-bit field"),
    ("3.50-beta-70000", None, "a beta number past the 16-bit field"),
]
for version, want, why in DERIVED:
    check("%s: %s" % (why, version or "(empty)"), guard.dotted(version), want)

print("--- the real header agrees with itself ---")
real = REAL.read_text(encoding="utf-8")
check("WinHTTrack/version.h", guard.check(real, str(REAL)), [])

print("--- macros() binds to the right macro ---")
SYNTH = (
    "#ifndef WINHTTRACK_VERSION_H\n"
    "#define WINHTTRACK_VERSION_H\n"
    '#define WINHTTRACK_VERSION "3.50-beta-7"\n'
    '#define WINHTTRACK_VERSIONID "3.49.99.7"\n'
    "#define WINHTTRACK_VERSION_NUM 3, 49, 99, 7\n"
    "#endif\n"
)
check("picks VERSION, not VERSIONID/NUM/_H",
      guard.macros(SYNTH), ("3.50-beta-7", "3.49.99.7", (3, 49, 99, 7)))
check("a consistent synthetic header passes", guard.check(SYNTH), [])

print("--- a damaged header is caught ---")
# Each row edits one macro of SYNTH and names how many complaints that must raise.
MUTANTS = [
    ('"3.49.99.7"', '"3.50.0.7"', 1, "VERSIONID alone is wrong"),
    ("3, 49, 99, 7", "3, 49, 99, 8", 1, "VERSION_NUM alone is wrong"),
    ('"3.50-beta-7"', '"3.50-7"', 2, "VERSION bumped out of beta, the other two left behind"),
    ('"3.50-beta-7"', '"3.50-beta-8"', 2, "the beta number bumped in one place only"),
    ('"3.49.99.7"', '"3.49.99.70"', 1, "a digit appended to VERSIONID"),
    ('"3.49.99.7"', '"3.49.99.7 "', 1, "a trailing space in VERSIONID"),
    # Settled, not a defect: a leading zero names the same release, and refusing one
    # would also refuse a two-digit minor like 3.05.
    ('"3.50-beta-7"', '"3.50-beta-07"', 0, "a leading zero still agrees"),
]
for old, new, want, why in MUTANTS:
    mutant = SYNTH.replace(old, new)
    if mutant == SYNTH:
        print("  FAIL %s : the edit %r -> %r changed nothing" % (why, old, new))
        fail += 1
        continue
    check(why, len(guard.check(mutant)), want)

print("--- a header the guard cannot read is an error, not a pass ---")
for text, why in [
    (SYNTH.replace('#define WINHTTRACK_VERSION "3.50-beta-7"\n', ""), "VERSION absent"),
    (SYNTH.replace('#define WINHTTRACK_VERSIONID "3.49.99.7"\n', ""), "VERSIONID absent"),
    (SYNTH.replace("#define WINHTTRACK_VERSION_NUM 3, 49, 99, 7\n", ""), "VERSION_NUM absent"),
]:
    exited = False
    try:
        guard.check(text)
    except SystemExit:
        exited = True
    check(why, exited, True)

print("--- an unknown VERSION shape is an error, not a pass ---")
check("VERSION shape the rule does not cover",
      len(guard.check(SYNTH.replace('"3.50-beta-7"', '"3.50rc1"'))), 1)

if fail:
    sys.exit("%d version-header case(s) failed" % fail)
# Counted, not written down: a row deleted by hand would otherwise still be claimed.
if ran < 30:
    sys.exit("only %d cases ran; rows have gone missing" % ran)
print("::notice::version-header guard: %d cases pass" % ran)
