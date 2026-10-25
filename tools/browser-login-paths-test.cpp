/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Xavier Roche and other contributors

   Which page a browser login opens, and whose jar it opens it with, tested off Windows.
   The GUI only builds under MSVC, so the --selftest run cannot be rerun by a reviewer and
   cannot be mutated in a pull request. This compiles WinHTTrack/browser-login-paths.cpp
   against a stub CString and calls the same browserLoginCheckCases() --selftest calls.

   Driven by tools/test-browser-login-paths.py. */

#include "mfc-test-stubs.h"

int browserLoginCheckCases(CString *err);

/* Both callers pin the count, so a case deleted from the unit reds here as well. */
#define BL_EXPECTED_CHECKS 83

int main(void) {
  CString err;
  const int nchecks = browserLoginCheckCases(&err);

  if (nchecks == 0) {
    fprintf(stderr, "FAIL: browser login helpers: %s\n", (const char *) err);
    return 1;
  }
  if (nchecks != BL_EXPECTED_CHECKS) {
    fprintf(stderr, "FAIL: browser login helpers ran %d checks, expected %d\n",
            nchecks, BL_EXPECTED_CHECKS);
    return 1;
  }
  printf("browser login helpers ok on %d checks\n", nchecks);
  return 0;
}
