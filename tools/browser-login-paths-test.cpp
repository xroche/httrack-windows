/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Xavier Roche and other contributors

   Which page a browser login opens, and whose jar it opens it with, tested off Windows.
   The GUI only builds under MSVC, so the --selftest run cannot be rerun by a reviewer and
   cannot be mutated in a pull request. This compiles WinHTTrack/browser-login-paths.cpp
   against a stub CString and calls the same browserLoginCheckCases() --selftest calls.

   Driven by tools/test-browser-login-paths.py. */

#include "mfc-test-stubs.h"
#include "cookie-file.h"

int browserLoginCheckCases(CString *err);
CString browserLoginPasteText(const CSimpleArray<CookieRecord> &cookies, int *dropped);
/* The engine's own test, copied out of htsbundle.c by the driver. */
int engine_paste_is_curl(const char *s);

/* Latin-1 to UTF-8, so a case that skips the conversion sees different bytes. */
char *strdupt_utf8(const char *s) {
  char *const out = (char *) malloc(strlen(s) * 2 + 1);
  char *o = out;

  for(const unsigned char *p = (const unsigned char *) s ; *p != '\0' ; p++) {
    if (*p < 0x80) {
      *o++ = (char) *p;
    } else {
      *o++ = (char) (0xC0 | (*p >> 6));
      *o++ = (char) (0x80 | (*p & 0x3F));
    }
  }
  *o = '\0';
  return out;
}

/* Both callers pin the count, so a case deleted from the unit reds here as well. */
#define BL_EXPECTED_CHECKS 71

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
  /* --selftest cannot reach the engine's static test, so only this run asks it. */
  CSimpleArray<CookieRecord> one;
  CookieRecord c;
  int dropped = 0;

  c.domain = "curl";
  c.name = "id";
  one.Add(c);
  if (engine_paste_is_curl(browserLoginPasteText(one, &dropped))) {
    fprintf(stderr, "FAIL: the engine reads a paste for the host curl as a cURL command\n");
    return 1;
  }
  printf("browser login helpers ok on %d checks\n", nchecks);
  return 0;
}
