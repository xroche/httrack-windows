/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Xavier Roche and other contributors

   The Netscape cookies.txt writer, tested off Windows. The GUI only builds under MSVC, so
   the --selftest run cannot be rerun by a reviewer and cannot be mutated in a pull
   request. This compiles WinHTTrack/cookie-file.cpp against a stub CString and calls the
   same cookieFileCheckCases() --selftest calls.

   Driven by tools/test-cookie-file.py. */

#include "mfc-test-stubs.h"
#include "../WinHTTrack/cookie-file.h"

/* Both callers pin the count, so a case deleted from cookie-file.cpp reds here as well. */
#define CF_EXPECTED_CHECKS 53

int main(void) {
  CString err;
  const int nchecks = cookieFileCheckCases(&err);

  if (nchecks == 0) {
    fprintf(stderr, "FAIL: cookie file: %s\n", (const char *) err);
    return 1;
  }
  if (nchecks != CF_EXPECTED_CHECKS) {
    fprintf(stderr, "FAIL: cookie file ran %d checks, expected %d\n",
            nchecks, CF_EXPECTED_CHECKS);
    return 1;
  }
  printf("cookie file ok on %d checks\n", nchecks);
  return 0;
}
