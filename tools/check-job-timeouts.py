#!/usr/bin/env python3
"""Every job under .github/workflows must declare a positive timeout-minutes.

A job with none runs until GitHub's 360-minute default.
"""
import pathlib
import sys

# yaml ships on the runner image, so a missing import fails this loudly.
import yaml

WORKFLOWS = ".github/workflows"


def rows(paths):
    """(workflow, job, timeout) per job, and (name, None, None) for a file with none."""
    out = []
    for path in paths:
        jobs = (yaml.safe_load(path.read_text(encoding="utf-8")) or {}).get("jobs")
        if not jobs:
            out.append((path.name, None, None))
            continue
        for name, job in jobs.items():
            out.append((path.name, name, job.get("timeout-minutes")))
    return out


def main(argv):
    root = pathlib.Path(argv[1] if len(argv) > 1 else WORKFLOWS)
    paths = sorted(p for p in root.iterdir() if p.suffix.lower() in (".yml", ".yaml"))
    if not paths:
        print(f"no workflow found under {root}")
        return 1

    bad = 0
    for workflow, job, timeout in rows(paths):
        if job is None:
            print(f"{workflow}: no job, so this guard would pass without checking one")
            bad += 1
        elif timeout is None:
            print(f"{workflow}: job {job} declares no timeout-minutes")
            bad += 1
        elif type(timeout) is not int or timeout <= 0:  # a bool is an int to isinstance
            print(f"{workflow}: job {job} declares timeout-minutes {timeout!r}")
            bad += 1
        else:
            print(f"{workflow}: job {job} times out after {timeout} minutes")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
