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
   Nothing here needs MFC beyond CString and CSimpleArray, and nothing here touches a
   file: the caller owns where the text goes. */

#ifdef COOKIE_FILE_TEST
#include "mfc-test-stubs.h"       /* CString and CSimpleArray, as the harness sees them */
#include "cookie-file.h"
#else
#include "stdafx.h"
#include "cookie-file.h"
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

/* 23:59:59 on 9999-12-31, where browsers cap an expiry themselves. Clamping to it also
   keeps the cast in expiryField() inside int64_t, which a raw double would not. */
#define COOKIE_EXPIRY_MAX 253402300799.0

/* Four bytes cannot go in a field. TAB ends a field for cookie_get(), and CR or LF ends
   the record for hts_readline(), so any of them forges the rest of the line. A NUL cuts
   it short instead, because the engine's readers are all strlen-based. */
static BOOL hasUnwritableByte(const CString &field) {
  for(int i=0 ; i<field.GetLength() ; i++) {
    const char c = field[i];

    if (c == '\t' || c == '\r' || c == '\n' || c == '\0')
      return TRUE;
  }
  return FALSE;
}

/* A domain the engine would not scope the cookie to. cookie_host() passes it through
   jump_identification_const(), which returns whatever follows the last '@' before the
   first '/', so "attacker.test@bank.test" would send the cookie to bank.test instead.
   A leading '#' makes cookie_load() read the whole record as a comment. A port or an
   IPv6 literal is left alone, because the engine folds those to the same host we meant. */
static BOOL badDomain(const CString &domain) {
  if (domain.GetLength() != 0 && domain[0] == '#')
    return TRUE;
  return domain.Find('@') >= 0 || domain.Find('/') >= 0;
}

/* The engine parses this field and then ignores it, but curl and the user read it. */
static int64_t expiryField(const CookieRecord &c) {
  /* NaN fails every comparison, so this spelling rather than expires <= 0.0 is what
     keeps a NaN away from the cast below, where it would be undefined. */
  if (!(c.expires > 0.0))
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
  if (hasUnwritableByte(c.name) || hasUnwritableByte(c.value)
      || hasUnwritableByte(c.domain) || hasUnwritableByte(c.path))
    return CookieUnwritableByte;
  if (badDomain(c.domain))
    return CookieUntrustedDomain;
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
CString cookieFileText(const CSimpleArray<CookieRecord> &cookies, CookieFileStats &stats) {
  /* cookie_add() charges the jar each record plus its reserve, and the record it stores
     has the same shape as ours, so our own byte count is the budget. Stopping here rather
     than letting the engine refuse the tail is what makes the loss countable. */
  const int budget = COOKIE_JAR_MAXLEN - COOKIE_JAR_RESERVE;
  CString text;

  stats.written = 0;
  for(int k=0 ; k<CookieRejectCount ; k++)
    stats.dropped[k] = 0;
  for(int i=0 ; i<cookies.GetSize() ; i++) {
    CString line;
    const CookieReject why = cookieFileLine(cookies[i], line);

    if (why != CookieWritable) {
      stats.dropped[why]++;
    } else if (line.GetLength() > budget - text.GetLength()) {
      /* Room left on one side, never a sum: text never passes budget, so the subtraction
         stays non-negative and cannot wrap. */
      stats.dropped[CookieJarFull]++;
    } else {
      text += line;
      stats.written++;
    }
  }
  return text;
}

/* The baseline the reject, length and expiry cases vary one field of. */
static CookieRecord plainRecord(void) {
  CookieRecord c;

  c.name = "SID";
  c.value = "abc";
  c.domain = "example.com";
  c.path = "/";
  c.secure = FALSE;
  c.expires = 1700000000.0;
  return c;
}

/* A file the engine refuses to read looks exactly like a login that did not work. Only a
   real browser produces the values that break this format. Both --selftest and
   tools/cookie-file-test.cpp call this, and both pin the count. */
int cookieFileCheckCases(CString *err) {
  char msg[1024];
  int nchecks = 0;

  /* One record at a time: what goes out, and what is refused instead of going out. */
  {
    static const struct {
      const char *name, *value, *domain, *path;
      BOOL secure;
      double expires;
      CookieReject want;
      const char *line;
    } records[] = {
      { "SID", "abc", "example.com", "/", FALSE, 1700000000.0, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t1700000000\tSID\tabc\n" },
      { "SID", "abc", ".example.com", "/", FALSE, 1700000000.0, CookieWritable,
        ".example.com\tTRUE\t/\tFALSE\t1700000000\tSID\tabc\n" },
      { "SID", "abc", "example.com", "/", TRUE, 1700000000.0, CookieWritable,
        "example.com\tFALSE\t/\tTRUE\t1700000000\tSID\tabc\n" },
      /* Zero or less is how this format spells "goes with the browser session". */
      { "SID", "abc", "example.com", "/", FALSE, 0.0, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t0\tSID\tabc\n" },
      { "SID", "abc", "example.com", "/", FALSE, -1.0, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t0\tSID\tabc\n" },
      { "SID", "abc", "example.com", "/", FALSE, 1700000000.9, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t1700000000\tSID\tabc\n" },
      /* Clamped, or the cast would leave int64_t. */
      { "SID", "abc", "example.com", "/", FALSE, 1e300, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t253402300799\tSID\tabc\n" },
      { "SID", "abc", "example.com", "/app/v2", FALSE, 1700000000.0, CookieWritable,
        "example.com\tFALSE\t/app/v2\tFALSE\t1700000000\tSID\tabc\n" },
      /* No path from the browser means this format's own default. */
      { "SID", "abc", "example.com", "", FALSE, 1700000000.0, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t1700000000\tSID\tabc\n" },
      /* An empty value is legal, and is how a site clears a cookie. */
      { "SID", "", "example.com", "/", FALSE, 1700000000.0, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t1700000000\tSID\t\n" },
      /* ';' and '=' are ordinary bytes in the value field, and so is a vertical tab:
         cookie_load() passes hts_readline() no flags, so it drops nothing but CR. */
      { "SID", "a=b; c=d", "example.com", "/", FALSE, 1700000000.0, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t1700000000\tSID\ta=b; c=d\n" },
      { "SID", "a\vb", "example.com", "/", FALSE, 1700000000.0, CookieWritable,
        "example.com\tFALSE\t/\tFALSE\t1700000000\tSID\ta\vb\n" },
      { "", "abc", "example.com", "/", FALSE, 1700000000.0, CookieEmptyField, NULL },
      { "SID", "abc", "", "/", FALSE, 1700000000.0, CookieEmptyField, NULL },
      /* A TAB or a newline in any field would forge a field or a whole record, and the
         edges matter as much as the middle: a scan starting at 1 would miss these. */
      { "SID", "a\tb", "example.com", "/", FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      { "SID", "\tab", "example.com", "/", FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      { "SID", "ab\t", "example.com", "/", FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      { "SID", "a\nb", "example.com", "/", FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      { "SID", "ab\n", "example.com", "/", FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      { "SID", "a\rb", "example.com", "/", FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      { "S\tID", "abc", "example.com", "/", FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      { "S\nID", "abc", "example.com", "/", FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      { "SID", "abc", "exa\tmple.com", "/", FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      { "SID", "abc", "example.com", "/a\tb", FALSE, 1700000000.0, CookieUnwritableByte, NULL },
      /* cookie_load() would read this record as a comment and skip it. */
      { "SID", "abc", "#example.com", "/", FALSE, 1700000000.0, CookieUntrustedDomain, NULL },
      /* The engine would store these as bank.test and as http, so the cookie would go to
         a host the browser never set it for. */
      { "SID", "abc", "attacker.test@bank.test", "/", FALSE, 1700000000.0,
        CookieUntrustedDomain, NULL },
      { "SID", "abc", "http://evil.test", "/", FALSE, 1700000000.0,
        CookieUntrustedDomain, NULL },
      /* A port and an IPv6 literal stay: cookie_host() folds both to the host we meant. */
      { "SID", "abc", "example.com:8080", "/", FALSE, 1700000000.0, CookieWritable,
        "example.com:8080\tFALSE\t/\tFALSE\t1700000000\tSID\tabc\n" },
      { "SID", "abc", "[::1]", "/", FALSE, 1700000000.0, CookieWritable,
        "[::1]\tFALSE\t/\tFALSE\t1700000000\tSID\tabc\n" }
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

  /* A NUL cannot go in the table above, which is written as C strings, and it is the one
     byte the engine's strlen-based readers would cut the record at. */
  {
    static const char *const named[] = { "value", "name", "domain" };

    for(int f=0 ; f<3 ; f++) {
      CookieRecord c = plainRecord();
      CString holed("a");
      CString line;
      CookieReject got;

      holed += '\0';
      holed += 'b';
      switch(f) {
        case 0: c.value = holed; break;
        case 1: c.name = holed; break;
        default: c.domain = holed; break;
      }
      got = cookieFileLine(c, line);
      if (got != CookieUnwritableByte || !line.IsEmpty()) {
        snprintf(msg, sizeof(msg), "a NUL in the %s came back %d, expected %d",
                 named[f], (int) got, (int) CookieUnwritableByte);
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

  /* The doubles only a browser can hand us. NaN is why expiryField() tests
     !(expires > 0.0): every comparison against a NaN is false, so the plain
     expires <= 0.0 would fall through to a cast that is undefined. */
  {
    static const char *const named[] = { "a NaN", "an infinity", "a negative infinity",
                                         "a negative zero", "the clamp itself" };
    const double odd[] = { strtod("NAN", NULL), strtod("INF", NULL),
                           -strtod("INF", NULL), -0.0, COOKIE_EXPIRY_MAX };
    static const char *const want[] = { "0", "253402300799", "0", "0", "253402300799" };

    for(int k=0 ; k<5 ; k++) {
      CookieRecord c = plainRecord();
      CString line;
      const CString wanted = CString("example.com\tFALSE\t/\tFALSE\t") + want[k]
                             + "\tSID\tabc\n";

      c.expires = odd[k];
      if (cookieFileLine(c, line) != CookieWritable || line != wanted) {
        snprintf(msg, sizeof(msg), "%s expired as '%s', expected '%s'",
                 named[k], (LPCSTR) line, (LPCSTR) wanted);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }

  /* The header promises an emptied LINE on a refusal. A caller reusing one buffer is what
     would otherwise write the previous cookie's record a second time. */
  {
    CookieRecord bad = plainRecord();
    CString line;

    bad.name = "";
    if (cookieFileLine(plainRecord(), line) != CookieWritable || line.IsEmpty()) {
      *err = "the baseline cookie did not write a record";
      return 0;
    }
    nchecks++;
    if (cookieFileLine(bad, line) != CookieEmptyField || !line.IsEmpty()) {
      snprintf(msg, sizeof(msg), "a refusal left '%s' in the caller's buffer", (LPCSTR) line);
      *err = msg;
      return 0;
    }
    nchecks++;
  }

  /* The whole file, where a refused cookie has to be counted rather than go missing,
     because a login dropped in silence looks like a site that just logged us out. */
  {
    CSimpleArray<CookieRecord> cookies;
    CookieFileStats stats;
    CString text;
    CookieRecord c = plainRecord();

    cookies.Add(c);                              /* written */
    c.domain = ".example.com";
    cookies.Add(c);                              /* written */
    c = plainRecord();
    c.secure = TRUE;
    cookies.Add(c);                              /* written */
    c = plainRecord();
    c.name = "";
    cookies.Add(c);                              /* CookieEmptyField */
    c = plainRecord();
    c.value = "a\tb";
    cookies.Add(c);                              /* CookieUnwritableByte */
    c = plainRecord();
    c.domain = "attacker.test@bank.test";
    cookies.Add(c);                              /* CookieUntrustedDomain */
    c = plainRecord();
    c.value = CString('a', COOKIE_VALUE_MAXLEN + 1);
    cookies.Add(c);                              /* CookieFieldTooLong */
    text = cookieFileText(cookies, stats);
    if (stats.written != 3 || stats.dropped[CookieEmptyField] != 1
        || stats.dropped[CookieUnwritableByte] != 1
        || stats.dropped[CookieUntrustedDomain] != 1
        || stats.dropped[CookieFieldTooLong] != 1
        || stats.dropped[CookieJarFull] != 0) {
      snprintf(msg, sizeof(msg), "7 cookies gave %d written and %d/%d/%d/%d/%d dropped",
               stats.written, stats.dropped[CookieEmptyField],
               stats.dropped[CookieUnwritableByte], stats.dropped[CookieUntrustedDomain],
               stats.dropped[CookieFieldTooLong], stats.dropped[CookieJarFull]);
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

  /* Past the jar's budget the engine stops reading and logs nothing, so the writer stops
     first and says how many cookies it left out. */
  {
    const int budget = COOKIE_JAR_MAXLEN - COOKIE_JAR_RESERVE;
    CSimpleArray<CookieRecord> cookies;
    CookieFileStats stats;
    CString text;

    for(int k=0 ; k<64 ; k++) {
      CookieRecord c = plainRecord();

      c.value = CString('v', COOKIE_VALUE_MAXLEN);
      cookies.Add(c);
    }
    text = cookieFileText(cookies, stats);
    if (text.GetLength() > budget || stats.dropped[CookieJarFull] == 0
        || stats.written + stats.dropped[CookieJarFull] != 64) {
      snprintf(msg, sizeof(msg),
               "64 big cookies wrote %d bytes (budget %d), %d in and %d left out",
               text.GetLength(), budget, stats.written, stats.dropped[CookieJarFull]);
      *err = msg;
      return 0;
    }
    nchecks++;
  }

  return nchecks;
}
