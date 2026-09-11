#!/usr/bin/env python3
"""Fail if the three version macros in WinHTTrack/version.h disagree.

CI holds each macro to the binary stamped from it and never to its siblings, so
"3.50-2" beside "3.50.3.0" builds, signs and publishes. The About box and the
installer name would then read one number where Explorer reads another (#175).

WINHTTRACK_VERSION is the source: the other two are derived from it and compared,
so a version shape this file does not know is an error rather than a pass.
"""
import re
import sys

HEADER = "WinHTTrack/version.h"
FIELD_MAX = 0xFFFF  # a PE and an Inno version field are both 16-bit


def dotted(version):
    """The four version fields WINHTTRACK_VERSION implies, or None for an unknown shape.

    A beta sorts below the release it precedes, so 3.50-beta-5 borrows a minor and
    becomes 3.49.99.5. A patch release takes the third field: 3.50-2 is 3.50.2.0.
    """
    m = re.match(r"\A(\d+)\.(\d+)(?:-beta-(\d+)|-(\d+))?\Z", version)
    if not m:
        return None
    major, minor = int(m.group(1)), int(m.group(2))
    beta, patch = m.group(3), m.group(4)
    if beta is None:
        fields = (major, minor, int(patch or 0), 0)
    elif minor > 0:
        fields = (major, minor - 1, 99, int(beta))
    elif major > 0:
        fields = (major - 1, 99, 99, int(beta))
    else:
        return None  # nothing sorts below 0.0
    return fields if all(0 <= f <= FIELD_MAX for f in fields) else None


def macros(text, path=HEADER):
    """The three version macros, as (version, versionid, version_num tuple)."""
    def one(name, pattern):
        m = re.search(r"#define\s+WINHTTRACK_%s\s+%s" % (name, pattern), text)
        if not m:
            sys.exit("cannot read WINHTTRACK_%s from %s" % (name, path))
        return m
    # \s+ cannot match the _ or I that follow, so VERSIONID and VERSION_NUM never bind here.
    version = one("VERSION", r'"([^"]*)"').group(1)
    versionid = one("VERSIONID", r'"([^"]*)"').group(1)
    num = one("VERSION_NUM", r"(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)")
    return version, versionid, tuple(int(g) for g in num.groups())


def check(text, path=HEADER):
    """The complaints about this header, empty when the three macros agree."""
    version, versionid, num = macros(text, path)
    want = dotted(version)
    if want is None:
        return ['WINHTTRACK_VERSION "%s" has a shape with no dotted form. Add it to '
                "dotted(), and give it a row in the table test" % version]
    want_str = ".".join(str(f) for f in want)
    bad = []
    if versionid != want_str:
        bad.append('WINHTTRACK_VERSION "%s" means WINHTTRACK_VERSIONID "%s", not "%s"'
                   % (version, want_str, versionid))
    if num != want:
        bad.append("WINHTTRACK_VERSION_NUM is %s, not %s"
                   % (", ".join(str(n) for n in num), ", ".join(str(f) for f in want)))
    return bad


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else HEADER
    with open(path, encoding="utf-8") as f:
        text = f.read()
    bad = check(text, path)
    if bad:
        sys.exit("%s disagrees with itself:\n  %s" % (path, "\n  ".join(bad)))
    version, versionid, _ = macros(text, path)
    print("%s: %s is %s" % (path, version, versionid))


if __name__ == "__main__":
    main()
