/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Xavier Roche and other contributors

   The scan-rule splitter and the preset checkboxes, tested off Windows. The GUI only
   builds under MSVC, so the --selftest run cannot be rerun by a reviewer and cannot be
   mutated in a pull request. This compiles WinHTTrack/rules-split.cpp against a stub
   CString and calls the same rulesSplitCheckCases() --selftest calls.

   Driven by tools/test-rules-split.py. */

#include "mfc-test-stubs.h"

int rulesSplitCheckCases(CString *err);

/* Both callers pin the count, so a case deleted from rules-split.cpp reds here as well. */
#define RS_EXPECTED_CHECKS 49

int main(void) {
  CString err;
  const int nchecks = rulesSplitCheckCases(&err);

  if (nchecks == 0) {
    fprintf(stderr, "FAIL: rule splitting: %s\n", (const char *) err);
    return 1;
  }
  if (nchecks != RS_EXPECTED_CHECKS) {
    fprintf(stderr, "FAIL: rule splitting ran %d checks, expected %d\n",
            nchecks, RS_EXPECTED_CHECKS);
    return 1;
  }
  printf("rule splitting ok on %d checks\n", nchecks);
  return 0;
}
