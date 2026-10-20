/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Xavier Roche and other contributors

   The Netscape cookies.txt writer, tested off Windows. The GUI only builds under MSVC, so
   the --selftest run cannot be rerun by a reviewer and cannot be mutated in a pull
   request. This compiles WinHTTrack/cookie-file.cpp against a stub CString and calls the
   same cookieFileCheckCases() --selftest calls, then sweeps random records past it.

   Driven by tools/test-cookie-file.py. */

#include "mfc-test-stubs.h"
#include "../WinHTTrack/cookie-file.h"
#include <stdint.h>

/* Both callers pin the count, so a case deleted from cookie-file.cpp reds here as well. */
#define CF_EXPECTED_CHECKS 103

/* How many random records the sweep below builds. */
#define CF_RANDOM_RECORDS 200000

/* Fixed seed, so a record that breaks the sweep breaks it again on the next run. */
static uint32_t prngState = 123456789u;

static unsigned prngNext(unsigned below) {
  prngState = prngState * 1103515245u + 12345u;
  return (unsigned) ((prngState >> 16) % below);
}

/* Two alphabets, and one field in thirty-two runs long. The whole byte range reaches the
   byte refusals, NUL included, which the shared cases can barely spell. A host-name
   alphabet is what lets a long field reach the length caps. 1200 random bytes almost
   always hold a TAB, which is refused first. */
static CString randomField(int clean) {
  static const char host[] = "abcdefghijklmnopqrstuvwxyz0123456789.-_";
  const unsigned len = prngNext(32) == 0 ? prngNext(1200) : prngNext(12);
  CString s;

  for(unsigned i=0 ; i<len ; i++)
    s += clean != 0 ? host[prngNext((unsigned) sizeof(host) - 1)] : (char) prngNext(256);
  return s;
}

/* The one property the format rests on: a written record is exactly seven tab-separated
   fields and ends at its own LF. Returns an empty string, or what went wrong. */
static const char *recordIsWellFormed(const CString &line) {
  int tabs = 0;

  if (line.GetLength() == 0)
    return "a writable cookie wrote nothing";
  if (line[line.GetLength() - 1] != '\n')
    return "a record does not end in LF";
  if (line.GetLength() >= COOKIE_LINE_MAXLEN)
    return "a record reaches the length cookie_load() skips";
  for(int i=0 ; i<line.GetLength() ; i++) {
    const char c = line[i];

    if (c == '\t')
      tabs++;
    else if (c == '\0' || c == '\r')
      return "a record carries a NUL or a CR";
    else if (c == '\n' && i != line.GetLength() - 1)
      return "a record carries an LF before its end";
  }
  if (tabs != 6)
    return "a record does not have seven fields";
  return "";
}

static int sweepRandomRecords(void) {
  CookieFileStats stats;
  CSimpleArray<CookieRecord> batch;
  CString text;

  /* One buffer for every record, because a fresh one per call would hide a refusal that
     leaves the caller's own buffer filled. */
  CString line;

  for(int k=0 ; k<CF_RANDOM_RECORDS ; k++) {
    CookieRecord c;
    const int clean = (int) prngNext(2);

    c.name = randomField(clean);
    c.value = randomField(clean);
    c.domain = randomField(clean);
    c.path = randomField(clean);
    c.secure = (BOOL) prngNext(2);
    c.expires = (double) prngNext(2000000000u);
    if (cookieFileLine(c, line) == CookieWritable) {
      const char *const bad = recordIsWellFormed(line);

      if (*bad != '\0') {
        fprintf(stderr, "FAIL: record %d: %s\n", k, bad);
        return 0;
      }
    } else if (!line.IsEmpty()) {
      fprintf(stderr, "FAIL: record %d: a refusal left the caller's buffer filled\n", k);
      return 0;
    }
    batch.Add(c);
  }
  /* The same records as a file, so the jar budget and the counts are swept too. */
  text = cookieFileText(batch, stats);
  if (text.GetLength() > COOKIE_JAR_MAXLEN - COOKIE_JAR_RESERVE) {
    fprintf(stderr, "FAIL: the sweep wrote %d bytes, past the jar budget\n",
            text.GetLength());
    return 0;
  }
  {
    int total = stats.written;

    for(int k=0 ; k<CookieRejectCount ; k++)
      total += stats.dropped[k];
    if (total != CF_RANDOM_RECORDS) {
      fprintf(stderr, "FAIL: the sweep accounted for %d of %d records\n",
              total, CF_RANDOM_RECORDS);
      return 0;
    }
  }
  return 1;
}

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
  if (!sweepRandomRecords())
    return 1;
  printf("cookie file properties ok on %d random records\n", CF_RANDOM_RECORDS);
  return 0;
}
