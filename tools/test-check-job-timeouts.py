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
PAIR = OK + "  two:\n    runs-on: ubuntu-latest\n"


def run(files):
    """(stdout, exit code) from main() over a directory holding name -> body."""
    with tempfile.TemporaryDirectory() as td:
        for name, body in files.items():
            (Path(td) / name).write_text(body, encoding="utf-8")
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = check.main(["check-job-timeouts.py", td])
    return out.getvalue(), code


CASES = [
    ("every job covered", {"a.yml": OK}, 0, "times out after 10 minutes"),
    ("a .yaml file is read too", {"a.yaml": BARE}, 1, "declares no timeout-minutes"),
    ("one job of two bare", {"a.yml": PAIR}, 1, "job two declares no timeout-minutes"),
    ("the bad file is named", {"a.yml": OK, "b.yml": BARE}, 1, "b.yml: job one"),
    ("zero is not a timeout", {"a.yml": OK.replace("10", "0")}, 1, "timeout-minutes 0"),
    ("nor is a string", {"a.yml": OK.replace("10", "'10'")}, 1, "timeout-minutes '10'"),
    ("an empty jobs map", {"a.yml": "jobs:\n"}, 1, "no job, so this guard"),
    ("a workflow with no jobs key", {"a.yml": "on: push\n"}, 1, "no job, so this guard"),
    ("no workflow at all", {}, 1, "no workflow found"),
    ("a .txt file is not one", {"a.txt": BARE}, 1, "no workflow found"),
]

bad = 0
for name, files, want_code, want in CASES:
    out, code = run(files)
    problems = []
    if code != want_code:
        problems.append(f"exit {code}, wanted {want_code}")
    if want not in out:
        problems.append(f"missing {want!r}")
    if problems:
        bad += 1
        print(f"FAIL {name}: {'; '.join(problems)}\n{out}")
    else:
        print(f"ok   {name}")

# Pin the count, or deleting a row leaves this green.
if len(CASES) != 10:
    print(f"FAIL expected 10 cases, table has {len(CASES)}")
    bad += 1

print(f"{len(CASES)} cases, {bad} failed")
sys.exit(1 if bad else 0)
