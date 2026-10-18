/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2026 Xavier Roche and other contributors

SPDX-License-Identifier: GPL-3.0-or-later

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

Ethical use: we kindly ask that you NOT use this software to harvest email
addresses or to collect any other private information about people. Doing so
would dishonor our work and waste the many hours we have spent on it.

Please visit our Website: http://www.httrack.com
*/


/* ------------------------------------------------------------ */
/* File: WinHTTrack subroutines:                                */
/*       the Netscape cookies.txt the engine preloads           */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Its own unit so tools/cookie-file-test.cpp can compile the real writer off Windows.
   Nothing here needs MFC beyond CString, and nothing here touches a file: the caller
   owns where the text goes. */

#ifdef COOKIE_FILE_TEST
#include "mfc-test-stubs.h"       /* CString, as the harness sees it */
#include "cookie-file.h"
#else
#include "stdafx.h"
#include "cookie-file.h"
#endif

#include <inttypes.h>
#include <stdio.h>

/* 23:59:59 on 9999-12-31, where browsers cap an expiry themselves. Clamping to it also
   keeps the cast in expiryField() inside int64_t, which a raw double would not. */
#define COOKIE_EXPIRY_MAX 253402300799.0

/* TAB ends a field and LF ends a record, so either one inside a value would forge the
   rest of the line. The engine splits on TAB alone (cookie_get()) and reads a line with
   hts_readline(), which keeps every other byte. */
static BOOL unwritableByte(const CString &field) {
  for(int i=0 ; i<field.GetLength() ; i++) {
    const char c = field[i];

    if (c == '\t' || c == '\r' || c == '\n')
      return TRUE;
  }
  return FALSE;
}

/* Field 4. The engine parses it and then ignores it, but curl and the user read it. */
static int64_t expiryField(const CookieRecord &c) {
  if (c.session || !(c.expires > 0.0))
    return 0;
  if (c.expires > COOKIE_EXPIRY_MAX)
    return (int64_t) COOKIE_EXPIRY_MAX;
  return (int64_t) c.expires;
}

// see cookie-file.h
CookieReject cookieFileLine(const CookieRecord &c, CString &line) {
  char middle[64];

  line = "";
  if (c.name.IsEmpty() || c.domain.IsEmpty())
    return CookieEmptyField;
  /* A domain opening with '#' would make cookie_load() read the record as a comment. */
  if (c.domain[0] == '#' || unwritableByte(c.name) || unwritableByte(c.value)
      || unwritableByte(c.domain) || unwritableByte(c.path))
    return CookieUnwritableByte;
  if (c.domain.GetLength() > COOKIE_DOMAIN_MAXLEN
      || c.name.GetLength() > COOKIE_NAME_MAXLEN
      || c.value.GetLength() > COOKIE_VALUE_MAXLEN
      || c.path.GetLength() > COOKIE_PATH_MAXLEN)
    return CookieFieldTooLong;

  snprintf(middle, sizeof(middle), "\t%s\t%" PRId64 "\t",
           c.secure ? "TRUE" : "FALSE", expiryField(c));
  /* Field 1: a leading dot is how this format spells "and every subdomain". */
  line = c.domain;
  line += c.domain[0] == '.' ? "\tTRUE\t" : "\tFALSE\t";
  line += c.path.IsEmpty() ? "/" : c.path;   /* the browser's own default */
  line += middle;
  line += c.name;
  line += "\t";
  line += c.value;
  line += "\n";
  return CookieWritable;
}

// see cookie-file.h
CString cookieFileText(const CookieRecord *cookies, int count, CookieFileStats *stats) {
  CString text;

  stats->written = 0;
  for(int k=0 ; k<CookieRejectCount ; k++)
    stats->dropped[k] = 0;
  for(int i=0 ; i<count ; i++) {
    CString line;
    const CookieReject why = cookieFileLine(cookies[i], line);

    if (why == CookieWritable) {
      text += line;
      stats->written++;
    } else {
      stats->dropped[why]++;
    }
  }
  return text;
}

/* The baseline the reject and length cases vary one field of. */
static CookieRecord plainRecord(void) {
  CookieRecord c;

  c.name = "SID";
  c.value = "abc";
  c.domain = "example.com";
  c.path = "/";
  c.secure = FALSE;
  c.session = FALSE;
  c.expires = 1700000000.0;
  return c;
}

/* A file the engine refuses to read looks exactly like a login that did not work, and
   only a real browser produces the values that break this format. Both --selftest and
   tools/cookie-file-test.cpp call this, and both pin the count. */
int cookieFileCheckCases(CString *err) {
  char msg[1024];
  int nchecks = 0;

  /* One record at a time: what goes out, and what is refused instead of going out. */
  {
    static const struct {
      const char *name, *value, *domain, *path;
      BOOL secure, session;
      double expires;
      CookieReject want;
      const char *line;
    } records[] = {
      { "SID", "abc", "example.com", "/", FALSE, FALSE, 1700000000.0, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t1700000000\tSID\tabc\n" },
      { "SID", "abc", ".example.com", "/", FALSE, FALSE, 1700000000.0, CookieWritable,
        ".example.com\tTRUE\t/\tFALSE\t1700000000\tSID\tabc\n" },
      { "SID", "abc", "example.com", "/", TRUE, FALSE, 1700000000.0, CookieWritable,
        "example.com\tFALSE\t/\tTRUE\t1700000000\tSID\tabc\n" },
      /* A session cookie has no expiry of its own, whatever the browser left in it. */
      { "SID", "abc", "example.com", "/", FALSE, TRUE, 1700000000.0, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t0\tSID\tabc\n" },
      /* WebView2 also spells "session" as -1 in the expiry itself. */
      { "SID", "abc", "example.com", "/", FALSE, FALSE, -1.0, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t0\tSID\tabc\n" },
      { "SID", "abc", "example.com", "/", FALSE, FALSE, 1700000000.9, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t1700000000\tSID\tabc\n" },
      /* Clamped, or the cast would leave int64_t. */
      { "SID", "abc", "example.com", "/", FALSE, FALSE, 1e300, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t253402300799\tSID\tabc\n" },
      { "SID", "abc", "example.com", "/app/v2", FALSE, FALSE, 1700000000.0, CookieWritable,
        "example.com\tFALSE\t/app/v2\tFALSE\t1700000000\tSID\tabc\n" },
      /* No path from the browser means this format's own default. */
      { "SID", "abc", "example.com", "", FALSE, FALSE, 1700000000.0, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t1700000000\tSID\tabc\n" },
      /* An empty value is legal, and is how a site clears a cookie. */
      { "SID", "", "example.com", "/", FALSE, FALSE, 1700000000.0, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t1700000000\tSID\t\n" },
      /* ';' and '=' are ordinary bytes in the value field. */
      { "SID", "a=b; c=d", "example.com", "/", FALSE, FALSE, 1700000000.0, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t1700000000\tSID\ta=b; c=d\n" },
      { "", "abc", "example.com", "/", FALSE, FALSE, 1700000000.0, CookieEmptyField, NULL },
      { "SID", "abc", "", "/", FALSE, FALSE, 1700000000.0, CookieEmptyField, NULL },
      /* A TAB or a newline in any field would forge a field or a whole record. */
      { "SID", "a\tb", "example.com", "/", FALSE, FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      { "SID", "a\nb", "example.com", "/", FALSE, FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      { "SID", "a\rb", "example.com", "/", FALSE, FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      { "S\tID", "abc", "example.com", "/", FALSE, FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      { "S\nID", "abc", "example.com", "/", FALSE, FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      { "SID", "abc", "exa\tmple.com", "/", FALSE, FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      { "SID", "abc", "example.com", "/a\tb", FALSE, FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      /* cookie_load() would read this record as a comment and skip it. */
      { "SID", "abc", "#example.com", "/", FALSE, FALSE, 1700000000.0, CookieUnwritableByte, NULL }
    };
    const int n = (int) (sizeof(records) / sizeof(records[0]));

    for(int k=0 ; k<n ; k++) {
      CookieRecord c;
      CString line;
      CookieReject got;

      c.name = records[k].name;
      c.value = records[k].value;
      c.domain = records[k].domain;
      c.path = records[k].path;
      c.secure = records[k].secure;
      c.session = records[k].session;
      c.expires = records[k].expires;
      got = cookieFileLine(c, line);
      if (got != records[k].want) {
        snprintf(msg, sizeof(msg), "cookie %d ('%s'='%s' on '%s') came back %d, expected %d",
                 k, records[k].name, records[k].value, records[k].domain,
                 (int) got, (int) records[k].want);
        *err = msg;
        return 0;
      }
      nchecks++;
      if (records[k].line != NULL ? line != records[k].line : !line.IsEmpty()) {
        snprintf(msg, sizeof(msg), "cookie %d wrote '%s', expected '%s'",
                 k, (LPCSTR) line, records[k].line != NULL ? records[k].line : "");
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }

  /* Each cap is where the engine starts refusing, so the longest allowed field has to go
     out and one byte more has to not. */
  {
    static const char *const named[] = { "domain", "name", "value", "path" };
    const int caps[] = { COOKIE_DOMAIN_MAXLEN, COOKIE_NAME_MAXLEN,
                         COOKIE_VALUE_MAXLEN, COOKIE_PATH_MAXLEN };

    for(int f=0 ; f<4 ; f++) {
      for(int over=0 ; over<=1 ; over++) {
        const CString big('a', caps[f] + over);
        const CookieReject want = over != 0 ? CookieFieldTooLong : CookieWritable;
        CookieRecord c = plainRecord();
        CString line;
        CookieReject got;

        switch(f) {
          case 0: c.domain = big; break;
          case 1: c.name = big; break;
          case 2: c.value = big; break;
          default: c.path = big; break;
        }
        got = cookieFileLine(c, line);
        if (got != want) {
          snprintf(msg, sizeof(msg), "a %d-byte %s came back %d, expected %d (cap %d)",
                   big.GetLength(), named[f], (int) got, (int) want, caps[f]);
          *err = msg;
          return 0;
        }
        nchecks++;
      }
    }
  }

  /* The line gate in cookie_load() stays unreachable only while the four caps keep the
     longest possible record under it. Raise one past this and the gate goes live. */
  {
    CookieRecord c = plainRecord();
    CString line;

    c.domain = CString('d', COOKIE_DOMAIN_MAXLEN);
    c.name = CString('n', COOKIE_NAME_MAXLEN);
    c.value = CString('v', COOKIE_VALUE_MAXLEN);
    c.path = CString('p', COOKIE_PATH_MAXLEN);
    c.expires = COOKIE_EXPIRY_MAX;
    if (cookieFileLine(c, line) != CookieWritable
        || line.GetLength() >= COOKIE_LINE_MAXLEN) {
      snprintf(msg, sizeof(msg),
               "the longest record the caps allow is %d bytes, and cookie_load() skips %d",
               line.GetLength(), COOKIE_LINE_MAXLEN);
      *err = msg;
      return 0;
    }
    nchecks++;
  }

  /* The whole file, where a refused cookie has to be counted rather than go missing,
     because a login dropped in silence looks like a site that just logged us out. */
  {
    CookieRecord c[6];
    CookieFileStats stats;
    CString text;

    c[0] = plainRecord();
    c[1] = plainRecord();
    c[1].domain = ".example.com";
    c[2] = plainRecord();
    c[2].secure = TRUE;
    c[3] = plainRecord();
    c[3].name = "";
    c[4] = plainRecord();
    c[4].value = "a\tb";
    c[5] = plainRecord();
    c[5].value = CString('a', COOKIE_VALUE_MAXLEN + 1);
    text = cookieFileText(c, 6, &stats);
    if (stats.written != 3 || stats.dropped[CookieWritable] != 0
        || stats.dropped[CookieEmptyField] != 1
        || stats.dropped[CookieUnwritableByte] != 1
        || stats.dropped[CookieFieldTooLong] != 1) {
      snprintf(msg, sizeof(msg), "6 cookies gave %d written and %d/%d/%d/%d dropped",
               stats.written, stats.dropped[CookieWritable],
               stats.dropped[CookieEmptyField], stats.dropped[CookieUnwritableByte],
               stats.dropped[CookieFieldTooLong]);
      *err = msg;
      return 0;
    }
    nchecks++;
    if (text != "example.com\tFALSE\t/\tFALSE\t1700000000\tSID\tabc\n"
                ".example.com\tTRUE\t/\tFALSE\t1700000000\tSID\tabc\n"
                "example.com\tFALSE\t/\tTRUE\t1700000000\tSID\tabc\n") {
      snprintf(msg, sizeof(msg), "the file came out as '%s'", (LPCSTR) text);
      *err = msg;
      return 0;
    }
    nchecks++;
  }

  return nchecks;
}
