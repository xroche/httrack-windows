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
CSimpleArray<CookieRecord> browserLoginBundleCookies(const CSimpleArray<CookieRecord> &cookies,
                                                     const CSimpleArray<CString> &urls);

/* What the engine stub in tools/test-browser-login-paths.py was last given. */
extern "C" char bl_scope_seen[4][1024];
extern "C" size_t bl_scope_seen_n;

/* The cases are ASCII, where the conversion is the identity. */
char *strdupt_utf8(const char *s) {
  return strdup(s);
}

/* Both callers pin the count, so a case deleted from the unit reds here as well. */
#define BL_EXPECTED_CHECKS 107

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
  /* The engine gets every address it is handed, in order, and nothing else. */
  {
    CSimpleArray<CString> urls;
    CSimpleArray<CookieRecord> one;
    CookieRecord cookie;

    urls.Add("https://a.example.com/#httrack-bundle=example.com");
    urls.Add("https://b.example.com/#httrack-bundle=example.com");
    cookie.domain = "example.com";
    one.Add(cookie);
    browserLoginBundleCookies(one, urls);
    if (bl_scope_seen_n != 2 || strcmp(bl_scope_seen[0], urls[0]) != 0
        || strcmp(bl_scope_seen[1], urls[1]) != 0) {
      fprintf(stderr, "FAIL: browser login helpers did not hand the engine its addresses\n");
      return 1;
    }
  }
  printf("browser login helpers ok on %d checks\n", nchecks);
  return 0;
}
