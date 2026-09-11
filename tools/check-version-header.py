#!/usr/bin/env python3
"""Fail if the three version macros in WinHTTrack/version.h disagree.

CI holds WINHTTRACK_VERSION to the ProductVersion of the binary stamped from it, and
WINHTTRACK_VERSIONID to that binary's numeric parts, which the .rc stamps from
WINHTTRACK_VERSION_NUM. Nothing compares the version string to the dotted pair, so
"3.50-2" beside "3.50.3.0" builds, signs and publishes. The About box would then read
one number where Explorer reads another (#175).

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
    # \A and \Z, not ^ and $: a trailing newline would otherwise pass.
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
    return fields if all(f <= FIELD_MAX for f in fields) else None


def macros(text, path=HEADER):
    """The three version macros, as (version, versionid, version_num tuple)."""

    def one(name, pattern):
        # \s+ cannot match the _ or I that follow, so VERSIONID and VERSION_NUM never bind
        # to VERSION. Every occurrence, not the first: a regex cannot tell which definition
        # is live, so one inside a comment, an #if 0 or an #ifdef would be read instead. That
        # refuses a header defining a macro conditionally, which is correct here, because
        # TagVersion.ps1, build-installer/action.yml and windows-build.yml all regex this same
        # file and would each silently take the first match.
        found = re.findall(rf"#\s*define\s+WINHTTRACK_{name}\s+{pattern}", text)
        if not found:
            sys.exit(f"cannot read WINHTTRACK_{name} from {path}")
        if len(found) > 1:
            sys.exit(f"{path} defines WINHTTRACK_{name} {len(found)} times, so which one the "
                     f"compiler uses cannot be read off the file: {found}")
        return found[0]

    version = one("VERSION", r'"([^"]*)"')
    versionid = one("VERSIONID", r'"([^"]*)"')
    num = one("VERSION_NUM", r"(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)")
    return version, versionid, tuple(int(g) for g in num)


def check(text, path=HEADER):
    """The complaints about this header, empty when the three macros agree."""
    version, versionid, num = macros(text, path)
    want = dotted(version)
    if want is None:
        return [f'WINHTTRACK_VERSION "{version}" has a shape with no dotted form. Add it to '
                f"dotted(), and give it a row in the table test"]
    want_str = ".".join(str(f) for f in want)
    bad = []
    if versionid != want_str:
        bad.append(f'WINHTTRACK_VERSION "{version}" means WINHTTRACK_VERSIONID "{want_str}", '
                   f'not "{versionid}"')
    if num != want:
        nums = ", ".join(str(n) for n in num)
        bad.append(f"WINHTTRACK_VERSION_NUM is {nums}, not {', '.join(str(f) for f in want)}")
    return bad


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else HEADER
    with open(path, encoding="utf-8") as f:
        text = f.read()
    bad = check(text, path)
    if bad:
        joined = "\n  ".join(bad)
        sys.exit(f"{path} disagrees with itself:\n  {joined}")
    version, versionid, _ = macros(text, path)
    print(f"{path}: {version} is {versionid}")


if __name__ == "__main__":
    main()
