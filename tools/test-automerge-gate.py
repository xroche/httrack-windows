#!/usr/bin/env python3
"""Mutation test for the two CI guards from #166.

Both are config, so nothing else exercises them: the arm gate only runs on a
dependabot event, and the CVE gate's severity only shows on a push. A test that
grepped for their text would pass with `&&` flipped to `||`, which arms every PR
in the repo, so each guard is parsed and each mutation below must be caught.

yaml is preinstalled on the runner image; a missing import fails this loudly.
"""
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
ARM = ROOT / ".github/workflows/dependabot-automerge.yml"
BUILD = ROOT / ".github/workflows/windows-build.yml"

# Reached only after both refusals, so it is the thing every mutation must not expose.
ARM_CMD = "gh pr merge --auto --squash"

CLAUSES = (
    "github.repository == 'xroche/httrack-windows'",
    "github.event.pull_request.user.login == 'dependabot[bot]'",
    "github.event.pull_request.head.repo.full_name == github.repository",
    "startsWith(github.event.pull_request.head.ref, 'dependabot/vcpkg/')",
)


def arm_problems(text):
    """Everything wrong with the auto-merge gate. Empty means sound."""
    bad = []
    jobs = yaml.safe_load(text).get("jobs") or {}
    # A second job could arm without any of this.
    if list(jobs) != ["automerge"]:
        return [f"jobs are {list(jobs)}, expected exactly ['automerge']"]
    job = jobs["automerge"]

    gate = " ".join(str(job.get("if", "")).split())
    # A disjunction anywhere makes the whole gate satisfiable by one clause.
    if "||" in gate:
        bad.append("the gate contains a disjunction")
    for clause in ("github.repository == 'xroche/httrack-windows'",
                   "github.event.pull_request.user.login == 'dependabot[bot]'",
                   "github.event.pull_request.head.repo.full_name == github.repository",
                   "startsWith(github.event.pull_request.head.ref, 'dependabot/vcpkg/')"):
        if clause not in gate:
            bad.append(f"the gate is missing: {clause}")
    if gate.count("&&") != 3:
        bad.append(f"the gate joins with {gate.count('&&')} '&&', expected 3")

    steps = job.get("steps") or []
    if len(steps) != 1:
        return bad + [f"{len(steps)} steps, expected exactly 1"]
    run = steps[0].get("run", "")

    # gh exports this author as app/dependabot, so a login match here matches nothing
    # and the guard holds on every dependabot PR, itself included.
    if ".author.is_bot" not in run:
        bad.append("the held lookup does not match a bot on is_bot")
    if "env.PR_NUMBER" not in run:
        bad.append("the held lookup does not exclude the current PR")
    if "WinHTTrack/vcpkg.json" not in run:
        bad.append("the held lookup does not name the manifest")
    # Polarity, not presence: -z for -n arms exactly when a human PR is held.
    if 'if [ -n "$held" ]; then' not in run:
        bad.append("the held branch does not refuse on a non-empty result")
    if 'if [ "$files" != "WinHTTrack/vcpkg.json" ]; then' not in run:
        bad.append("the files branch does not refuse on a mismatch")
    if run.count("--disable-auto") != 2:
        bad.append(f"{run.count('--disable-auto')} disarm calls, expected 2")
    if run.count(ARM_CMD) != 1:
        bad.append(f"{run.count(ARM_CMD)} arm calls, expected 1")
    elif run.index(ARM_CMD) < run.rfind("exit 0"):
        bad.append("the arm runs before a refusal branch")
    return bad


def cve_problems(text):
    """Everything wrong with the CVE gate's severity. Empty means sound."""
    want = "${{ github.event_name == 'pull_request' }}"
    for job in (yaml.safe_load(text).get("jobs") or {}).values():
        for step in job.get("steps") or []:
            if "native dependencies" in step.get("name", ""):
                got = str(step.get("continue-on-error", ""))
                # Advisory anywhere but a pull request is how a red master hides.
                return [] if got == want else [f"continue-on-error is {got!r}, want {want!r}"]
    return ["no native-dependency step found"]


ARM_TEXT, BUILD_TEXT = ARM.read_text(), BUILD.read_text()
CVE_LINE = "continue-on-error: ${{ github.event_name == 'pull_request' }}"

# Each row is the file as committed, or one edit to it that must be caught. The
# committed rows run first, so breaking a guard reports the guard, not a stale edit.
CASES = [
    ("arm gate as committed", arm_problems, ARM_TEXT, None, False),
    ("CVE gate as committed", cve_problems, BUILD_TEXT, None, False),
    ("&& flipped to ||", arm_problems, ARM_TEXT,
     ("&& " + CLAUSES[3], "|| " + CLAUSES[3]), True),
    ("author clause dropped", arm_problems, ARM_TEXT,
     ("&& " + CLAUSES[1] + "\n", ""), True),
    ("head-repo clause dropped", arm_problems, ARM_TEXT,
     ("&& " + CLAUSES[2] + "\n", ""), True),
    ("vcpkg branch clause dropped", arm_problems, ARM_TEXT,
     ("&& " + CLAUSES[3] + "\n", ""), True),
    ("bot matched by login again", arm_problems, ARM_TEXT,
     ("select(.author.is_bot | not)", 'select(.author.login != "dependabot[bot]")'), True),
    ("self-exclusion dropped", arm_problems, ARM_TEXT,
     ("| select(.number != (env.PR_NUMBER | tonumber))\n", ""), True),
    ("held guard disarmed", arm_problems, ARM_TEXT,
     ('if [ -n "$held" ]; then', 'if [ -z "$held" ]; then'), True),
    ("files guard inverted", arm_problems, ARM_TEXT,
     ('if [ "$files" != "WinHTTrack/vcpkg.json" ]; then',
      'if [ "$files" = "WinHTTrack/vcpkg.json" ]; then'), True),
    ("CVE gate advisory everywhere", cve_problems, BUILD_TEXT,
     (CVE_LINE, "continue-on-error: true"), True),
    ("CVE gate advisory off a tag", cve_problems, BUILD_TEXT,
     (CVE_LINE, "continue-on-error: ${{ !startsWith(github.ref, 'refs/tags/') }}"), True),
]

bad = 0
for name, check, text, mutation, want_problem in CASES:
    if mutation:
        old, new = mutation
        # An edit that no longer applies proves nothing, so say which it is.
        if old not in text:
            bad += 1
            print(f"FAIL {name}: the workflow no longer contains {old!r}, so this case "
                  f"tests nothing. Update it if the guard legitimately changed shape.")
            continue
        text = text.replace(old, new, 1)
    found = check(text)
    if want_problem and not found:
        bad += 1
        print(f"FAIL {name}: the edit went unnoticed")
    elif not want_problem and found:
        bad += 1
        print(f"FAIL {name}: {'; '.join(found)}")
    else:
        print(f"ok   {name}")

# Pin the count, or deleting a row leaves this green.
if len(CASES) != 12:
    print(f"FAIL expected 12 cases, table has {len(CASES)}")
    bad += 1

print(f"{len(CASES)} cases, {bad} failed")
sys.exit(1 if bad else 0)
