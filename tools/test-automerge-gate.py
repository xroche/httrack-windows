#!/usr/bin/env python3
"""Mutation test for the two CI guards from #166.

Both are config, so nothing else exercises them. The arm gate only runs on a
dependabot event, and the CVE gate's severity only shows on a push.

This reads the workflows as text, so it catches a careless edit rather than a
crafted one. An edit that keeps every string below while changing what the
shell means will pass, so review still owns these two files.
"""
import sys
from pathlib import Path

# yaml ships on the runner image, so a missing import fails this loudly.
import yaml

ROOT = Path(__file__).resolve().parent.parent
ARM = ROOT / ".github/workflows/dependabot-automerge.yml"
BUILD = ROOT / ".github/workflows/windows-build.yml"

# Reached only after both refusals, so it is what every edit must not expose.
# Any merge method, because a second arm spelled --rebase arms just as hard.
ARM_CMD = "gh pr merge --auto"
# Each refusal must disarm and then leave, or it falls through to the arm.
REFUSAL = 'gh pr merge --disable-auto "$PR_URL" || true exit 0'
HELD_GUARD = 'if [ -n "$held" ]; then'
FILES_GUARD = 'if [ "$files" != "WinHTTrack/vcpkg.json" ]; then'

REPO_CLAUSE = "github.repository == 'xroche/httrack-windows'"
AUTHOR_CLAUSE = "github.event.pull_request.user.login == 'dependabot[bot]'"
HEAD_REPO_CLAUSE = "github.event.pull_request.head.repo.full_name == github.repository"
VCPKG_CLAUSE = "startsWith(github.event.pull_request.head.ref, 'dependabot/vcpkg/')"
CLAUSES = (REPO_CLAUSE, AUTHOR_CLAUSE, HEAD_REPO_CLAUSE, VCPKG_CLAUSE)


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
    for clause in CLAUSES:
        if clause not in gate:
            bad.append(f"the gate is missing: {clause}")
    # This pins the clause count, so a fifth clause has to pass a reader.
    if gate.count("&&") != len(CLAUSES) - 1:
        bad.append(f"the gate joins with {gate.count('&&')} '&&', expected {len(CLAUSES) - 1}")

    steps = job.get("steps") or []
    if len(steps) != 1:
        return bad + [f"{len(steps)} steps, expected exactly 1"]
    run = steps[0].get("run", "")
    # Whitespace-collapsed, so indentation changes do not read as damage.
    flat = " ".join(run.split())

    # A failed lookup must not fall through to arming.
    if "set -euo pipefail" not in run:
        bad.append("the step does not fail closed")
    # A second assignment to either would leave its refusal vacuous.
    for var in ("files=", "held="):
        if run.count(var) != 1:
            bad.append(f"{run.count(var)} assignments to {var}, expected 1")
    # This checks polarity, so swapping -n for -z would arm on a held human PR.
    if HELD_GUARD not in run:
        bad.append("the held branch does not refuse on a non-empty result")
    if FILES_GUARD not in run:
        bad.append("the files branch does not refuse on a mismatch")
    # Pairs, not totals. A comment holding the words would restore a bare count.
    if flat.count(REFUSAL) != 2:
        bad.append(f"{flat.count(REFUSAL)} refusals that disarm and leave, expected 2")

    # The files branch names the manifest too, so an unscoped search would pass
    # even with a lookup that filters nothing.
    lookup = run[run.find("held=$("):run.find(HELD_GUARD)]
    if ".author.is_bot" not in lookup:
        bad.append("the lookup does not match a bot on is_bot")
    if "env.PR_NUMBER" not in lookup:
        bad.append("the lookup does not exclude the current PR")
    if "WinHTTrack/vcpkg.json" not in lookup:
        bad.append("the lookup does not name the manifest")
    # Narrowing either would empty the result while every name above survives.
    if "--state open" not in lookup:
        bad.append("the lookup does not read open pull requests")
    if ".files[].path" not in lookup:
        bad.append("the lookup does not read every changed path")

    if flat.count(ARM_CMD) != 1:
        bad.append(f"{flat.count(ARM_CMD)} arm calls, expected 1")
    elif flat.index(ARM_CMD) < flat.rfind(REFUSAL):
        bad.append("the arm runs before a refusal branch")
    return bad


def cve_problems(text):
    """Everything wrong with the CVE gate's severity. Empty means sound."""
    want = "${{ github.event_name == 'pull_request' }}"
    # A later advisory copy of the step would otherwise go unread.
    steps = [s for job in (yaml.safe_load(text).get("jobs") or {}).values()
             for s in job.get("steps") or []
             if "native dependencies" in s.get("name", "")]
    if not steps:
        return ["no native-dependency step found"]
    bad = []
    for step in steps:
        got = str(step.get("continue-on-error", ""))
        if got != want:
            bad.append(f"continue-on-error is {got!r}, want {want!r}")
        # A skipped step reports nothing at all, which beats being advisory.
        if "if" in step:
            bad.append(f"the step is conditional on {step['if']!r}")
        if "check-native-deps.py" not in step.get("run", ""):
            bad.append("the step does not run the checker")
    return bad


ARM_TEXT, BUILD_TEXT = ARM.read_text(), BUILD.read_text()
CVE_STEP_NAME = "      - name: Check shipped native dependencies for advisories\n"
NEXT_STEP = "      - name: Build the installer\n"
CVE_LINE = "continue-on-error: ${{ github.event_name == 'pull_request' }}"

# A grep-based check would miss && flipped to ||, which arms every pull request.
# The committed rows run first, so a broken guard reports the guard, not a stale row.
# (name, check, text, (old, new) edit or None, must be caught)
CASES = [
    ("arm gate as committed", arm_problems, ARM_TEXT, None, False),
    ("CVE gate as committed", cve_problems, BUILD_TEXT, None, False),
    ("&& flipped to ||", arm_problems, ARM_TEXT,
     ("&& " + VCPKG_CLAUSE, "|| " + VCPKG_CLAUSE), True),
    ("author clause dropped", arm_problems, ARM_TEXT,
     ("&& " + AUTHOR_CLAUSE + "\n", ""), True),
    ("head-repo clause dropped", arm_problems, ARM_TEXT,
     ("&& " + HEAD_REPO_CLAUSE + "\n", ""), True),
    ("vcpkg branch clause dropped", arm_problems, ARM_TEXT,
     ("&& " + VCPKG_CLAUSE + "\n", ""), True),
    ("fifth clause bolted on", arm_problems, ARM_TEXT,
     ("&& " + VCPKG_CLAUSE, "&& true\n      && " + VCPKG_CLAUSE), True),
    ("fail-closed dropped", arm_problems, ARM_TEXT,
     ("          set -euo pipefail\n", ""), True),
    ("lookup result overwritten", arm_problems, ARM_TEXT,
     (HELD_GUARD, 'held=""\n          ' + HELD_GUARD), True),
    ("refusal falls through", arm_problems, ARM_TEXT,
     ("            gh pr merge --disable-auto \"$PR_URL\" || true\n            exit 0\n",
      "            gh pr merge --disable-auto \"$PR_URL\" || true\n"), True),
    ("bot matched by login again", arm_problems, ARM_TEXT,
     ("select(.author.is_bot | not)", 'select(.author.login != "dependabot[bot]")'), True),
    ("self-exclusion dropped", arm_problems, ARM_TEXT,
     ("| select(.number != (env.PR_NUMBER | tonumber))\n", ""), True),
    ("lookup stops naming the manifest", arm_problems, ARM_TEXT,
     ('select(any(.files[].path; . == "WinHTTrack/vcpkg.json"))', "select(true)"), True),
    ("held guard disarmed", arm_problems, ARM_TEXT,
     (HELD_GUARD, 'if [ -z "$held" ]; then'), True),
    ("files guard inverted", arm_problems, ARM_TEXT,
     (FILES_GUARD, 'if [ "$files" = "WinHTTrack/vcpkg.json" ]; then'), True),
    ("CVE gate advisory everywhere", cve_problems, BUILD_TEXT,
     (CVE_LINE, "continue-on-error: true"), True),
    ("CVE gate advisory off a tag", cve_problems, BUILD_TEXT,
     (CVE_LINE, "continue-on-error: ${{ !startsWith(github.ref, 'refs/tags/') }}"), True),
    ("files result overwritten", arm_problems, ARM_TEXT,
     (FILES_GUARD, 'files="WinHTTrack/vcpkg.json"\n          ' + FILES_GUARD), True),
    ("CVE step made conditional", cve_problems, BUILD_TEXT,
     (CVE_LINE, CVE_LINE + "\n        if: false"), True),
    ("CVE step stops running the checker", cve_problems, BUILD_TEXT,
     ("python httrack-windows/tools/check-native-deps.py", "echo skipped #"), True),
    ("advisory copy after the sound step", cve_problems, BUILD_TEXT,
     (NEXT_STEP, CVE_STEP_NAME + "        continue-on-error: true\n"
      "        run: python httrack-windows/tools/check-native-deps.py --help\n" + NEXT_STEP),
     True),
]

failed = 0
for name, check, text, edit, must_catch in CASES:
    if edit:
        old, new = edit
        # An edit that no longer applies proves nothing, so say which it is.
        if old not in text:
            failed += 1
            print(f"FAIL {name}: the workflow no longer contains {old!r}, so this case "
                  f"tests nothing. Update it if the guard legitimately changed shape.")
            continue
        text = text.replace(old, new, 1)
    found = check(text)
    if must_catch and not found:
        failed += 1
        print(f"FAIL {name}: the edit went unnoticed")
    elif not must_catch and found:
        failed += 1
        print(f"FAIL {name}: {'; '.join(found)}")
    else:
        print(f"ok   {name}")

# Pin the count, or deleting a row leaves this green.
if len(CASES) != 21:
    print(f"FAIL expected 21 cases, table has {len(CASES)}")
    failed += 1

print(f"{len(CASES)} cases, {failed} failed")
sys.exit(1 if failed else 0)
