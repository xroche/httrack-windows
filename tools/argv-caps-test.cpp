/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Xavier Roche and other contributors

   The caps on what one option value may carry, tested off Windows. The GUI only builds
   under MSVC, so the --selftest run cannot be rerun by a reviewer and cannot be mutated
   in a pull request. This compiles WinHTTrack/argv-caps.cpp against a stub CString and
   calls the same argvCapsCheckCases() --selftest calls.

   Driven by tools/test-argv-caps.py. */

#include "mfc-test-stubs.h"

int argvCapsCheckCases(CString *err, int *nskipped);

/* Both callers pin the count, so a case deleted from argv-caps.cpp reds here as well. */
#define AC_EXPECTED_CHECKS 37

int main(void) {
  CString err;
  int nskipped = -1;
  const int nchecks = argvCapsCheckCases(&err, &nskipped);

  if (nchecks == 0) {
    fprintf(stderr, "FAIL: argv caps: %s\n", (const char *) err);
    return 1;
  }
  /* The stub codepage is a legacy one, so the accented case has to have run here. */
  if (nskipped != 0) {
    fprintf(stderr, "FAIL: argv caps skipped %d checks under a legacy codepage\n", nskipped);
    return 1;
  }
  if (nchecks != AC_EXPECTED_CHECKS) {
    fprintf(stderr, "FAIL: argv caps ran %d checks, expected %d\n",
            nchecks, AC_EXPECTED_CHECKS);
    return 1;
  }
  printf("argv caps ok on %d checks\n", nchecks);
  return 0;
}
