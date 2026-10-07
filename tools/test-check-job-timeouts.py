#!/usr/bin/env python3
"""Table test for check-job-timeouts.py, over workflows written to a temp directory."""
import contextlib
import importlib.util
import io
import sys
import tempfile
from pathlib import Path

MOD = Path(__file__).with_name("check-job-timeouts.py")
spec = importlib.util.spec_from_file_location("check_job_timeouts", MOD)
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)

OK = "jobs:\n  one:\n    runs-on: ubuntu-latest\n    timeout-minutes: 10\n"
BARE = "jobs:\n  one:\n    runs-on: ubuntu-latest\n"
TWO = "  two:\n    runs-on: ubuntu-latest\n"
PAIR = OK + TWO
BOTH_BARE = BARE + TWO


def run(files):
    """(stdout, exit code) from main() over a directory holding name -> body."""
    with tempfile.TemporaryDirectory() as workflows:
        for name, body in files.items():
            (Path(workflows) / name).write_text(body, encoding="utf-8")
        out = io.StringIO()
        try:
            with contextlib.redirect_stdout(out):
                code = check.main(["check-job-timeouts.py", workflows])
        except Exception as e:  # one row must not abort the rows after it
            code = f"raised {e!r}"
    return out.getvalue(), code


CASES = [  # name, the directory to write, the exit code wanted, text wanted in stdout
    ("every job covered", {"a.yml": OK}, 0, "times out after 10 minutes"),
    ("a .yaml file is read too", {"a.yaml": BARE}, 1, "declares no timeout-minutes"),
    ("one job of two bare", {"a.yml": PAIR}, 1, "job two declares no timeout-minutes"),
    ("the bad file is named", {"a.yml": OK, "b.yml": BARE}, 1, "b.yml: job one"),
    ("both bad jobs are named", {"a.yml": BOTH_BARE}, 1, "job two declares no"),
    ("zero is not a timeout", {"a.yml": OK.replace(": 10", ": 0")}, 1, "timeout-minutes 0"),
    ("nor is a string", {"a.yml": OK.replace(": 10", ": '10'")}, 1, "timeout-minutes '10'"),
    ("nor is a boolean", {"a.yml": OK.replace(": 10", ": true")}, 1, "timeout-minutes True"),
    ("an uppercase extension is read", {"a.YML": BARE}, 1, "declares no timeout-minutes"),
    ("an empty jobs map", {"a.yml": "jobs: {}\n"}, 1, "no job, so this guard"),
    ("a jobs key with no value", {"a.yml": "jobs:\n"}, 1, "no job, so this guard"),
    ("an empty file", {"a.yml": ""}, 1, "no job, so this guard"),
    ("a workflow with no jobs key", {"a.yml": "on: push\n"}, 1, "no job, so this guard"),
    ("no workflow at all", {}, 1, "no workflow found"),
    ("a .txt file is not one", {"a.txt": BARE}, 1, "no workflow found"),
]

bad = 0
for name, files, want_code, want_text in CASES:
    out, code = run(files)
    problems = []
    if code != want_code:
        problems.append(f"exit {code}, wanted {want_code}")
    if want_text not in out:
        problems.append(f"missing {want_text!r}")
    if problems:
        bad += 1
        print(f"FAIL {name}: {'; '.join(problems)}\n{out}")
    else:
        print(f"ok   {name}")

# Pin the count, or deleting a row leaves this green.
if len(CASES) != 15:
    print(f"FAIL expected 15 cases, table has {len(CASES)}")
    bad += 1

print(f"{len(CASES)} cases, {bad} failed")
sys.exit(1 if bad else 0)
