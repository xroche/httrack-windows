/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Xavier Roche and other contributors

   The scan-rule splitter and the preset checkboxes, tested off Windows. The GUI only
   builds under MSVC, so the --selftest run cannot be rerun by a reviewer and cannot be
   mutated in a pull request. This compiles WinHTTrack/rules-split.cpp against a stub
   CString and calls the same rulesSplitCheckCases() --selftest calls.

   Driven by tools/test-rules-split.py. */

#include "mfc-test-stubs.h"

int rulesSplitCheckCases(CString *err);
BOOL patternReadsAsOptions(const char *pattern);

extern "C" {
  /* the engine's own answer, wrapped under one name by tools/test-rules-split.py */
  int engine_reads_as_opt(const char *s);
};

/* Our copy of the engine's option-or-rule decision against the engine's own, over every
   pattern of up to three bytes that can change the answer. The Windows --selftest cannot
   do this, because the engine we build against may not export that decision yet. */
static int engineAgreesOnPatterns(void) {
  static const char alphabet[] = "a./*%-";
  char rule[8];
  int nchecks = 0;

  for(size_t i = 0 ; i < sizeof(alphabet) ; i++)
    for(size_t j = 0 ; j < sizeof(alphabet) ; j++)
      for(size_t k = 0 ; k < sizeof(alphabet) ; k++) {
        size_t at = 1;

        rule[0] = '-';
        if (alphabet[i] != '\0')
          rule[at++] = alphabet[i];
        if (alphabet[i] != '\0' && alphabet[j] != '\0')
          rule[at++] = alphabet[j];
        if (alphabet[i] != '\0' && alphabet[j] != '\0' && alphabet[k] != '\0')
          rule[at++] = alphabet[k];
        rule[at] = '\0';
        if (patternReadsAsOptions(rule + 1) != (engine_reads_as_opt(rule) ? TRUE : FALSE)) {
          fprintf(stderr, "FAIL: '%s' reads as %s here and as %s in the engine\n", rule,
                  patternReadsAsOptions(rule + 1) ? "options" : "a rule",
                  engine_reads_as_opt(rule) ? "options" : "a rule");
          return 0;
        }
        nchecks++;
      }
  return nchecks;
}

/* Both callers pin the count, so a case deleted from rules-split.cpp reds here as well. */
#define RS_EXPECTED_CHECKS 157

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
  {
    const int agreed = engineAgreesOnPatterns();

    if (agreed == 0)
      return 1;
    printf("the engine agrees on %d patterns\n", agreed);
  }
  printf("rule splitting ok on %d checks\n", nchecks);
  return 0;
}
