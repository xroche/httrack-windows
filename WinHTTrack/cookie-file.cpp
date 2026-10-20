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

/* The domain must be a plain host name. cookie_host() rewrites anything else into a
   different host, and the cookie then goes there. It keeps what follows the last '@',
   keeps only what a leading '[' brackets, and strips a scheme before a ':'. A leading '#'
   makes cookie_load() skip the record as a comment. A raw IPv6 literal is refused by this
   rule too, and loses its preloaded cookies. */
static BOOL plainHostName(const CString &domain) {
  for(int i=0 ; i<domain.GetLength() ; i++) {
    const char c = domain[i];

    if (c != '.' && c != '-' && c != '_'
        && !(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z'))
      return FALSE;
  }
  return TRUE;
}

/* cookie_set() charges the jar the four fields as it stores them. Our own line spells the
   flags and the expiry more cheaply, so charging that would overrun the jar. */
static int jarCost(const CookieRecord &c) {
  return c.domain.GetLength() + (c.path.IsEmpty() ? 1 : c.path.GetLength())
         + c.name.GetLength() + c.value.GetLength();
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
  if (!plainHostName(c.domain))
    return CookieUntrustedDomain;
  if (c.domain.GetLength() > COOKIE_DOMAIN_MAXLEN
      || c.name.GetLength() > COOKIE_NAME_MAXLEN
      || c.path.GetLength() > COOKIE_PATH_MAXLEN)
    return CookieFieldTooLong;
  /* The pair against the room the name leaves, never their sum, and the name's own cap
     above is what keeps this subtraction from going negative. */
  if (c.value.GetLength() > COOKIE_PAIR_MAXLEN - c.name.GetLength())
    return CookieFieldTooLong;

  snprintf(middle, sizeof(middle), "\t%s\t%" PRId64 "\t",
           c.secure ? "TRUE" : "FALSE", expiryField(c));
  /* Field 1: a leading dot is how this format spells "and every subdomain". */
  line = c.domain;
  line += c.domain[0] == '.' ? "\tTRUE\t" : "\tFALSE\t";
  /* Both arms cast, because MFC's CString converts several ways and the ternary is
     ambiguous without it. The browser sending no path means this format's default. */
  line += c.path.IsEmpty() ? "/" : (LPCSTR) c.path;
  line += middle;
  line += c.name;
  line += "\t";
  line += c.value;
  line += "\n";
  return CookieWritable;
}

// see cookie-file.h
CString cookieFileText(const CSimpleArray<CookieRecord> &cookies, CookieFileStats &stats) {
  /* cookie_load() stops reading once the jar fills and that loop exit logs nothing, so
     the writer stops first and counts the rest. The budget is the engine's own: every
     record already stored, plus this record's fields, plus the reserve cookie_set() keeps
     free on each call. */
  const int budget = COOKIE_JAR_MAXLEN - COOKIE_JAR_RESERVE;
  int stored = 0;
  CString text;

  stats.written = 0;
  for(int k=0 ; k<CookieRejectCount ; k++)
    stats.dropped[k] = 0;
  for(int i=0 ; i<cookies.GetSize() ; i++) {
    CString line;
    const CookieReject why = cookieFileLine(cookies[i], line);

    if (why != CookieWritable) {
      stats.dropped[why]++;
    } else if (stored > budget || jarCost(cookies[i]) > budget - stored) {
      /* Room left on one side, never a sum, and the first test is what keeps the
         subtraction in the second from going negative. */
      stats.dropped[CookieJarFull]++;
    } else {
      text += line;
      stored += jarCost(cookies[i]) + COOKIE_JAR_RECORD;
      stats.written++;
    }
  }
  return text;
}

// see cookie-file.h
CString cookieFileInProject(const CString &projectPath) {
  CString path = projectPath;

  if (path.IsEmpty())
    return path;
  if (path.Right(1) != "\\" && path.Right(1) != "/")
    path += "\\";
  path += "hts-cache\\cookies.txt";
  return path;
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
      /* A leading '[' keeps only what it brackets, so this one would reach bank.test's
         neighbour evil.test, and a scheme needs no slash to be stripped. */
      { "SID", "abc", "[evil.test]bank.test", "/", FALSE, 1700000000.0,
        CookieUntrustedDomain, NULL },
      { "SID", "abc", "http:bank.test", "/", FALSE, 1700000000.0,
        CookieUntrustedDomain, NULL },
      /* A port and an IPv6 literal go the same way, which costs a raw-literal address
         its preloaded cookies. */
      { "SID", "abc", "example.com:8080", "/", FALSE, 1700000000.0,
        CookieUntrustedDomain, NULL },
      { "SID", "abc", "[::1]", "/", FALSE, 1700000000.0, CookieUntrustedDomain, NULL },
      /* What a host name is allowed to be made of, so the guard cannot just refuse all. */
      { "SID", "abc", "192.168.1.1", "/", FALSE, 1700000000.0, CookieWritable,
        "192.168.1.1\tFALSE\t/\tFALSE\t1700000000\tSID\tabc\n" },
      { "SID", "abc", "sub_do-main.example.com", "/", FALSE, 1700000000.0, CookieWritable,
        "sub_do-main.example.com\tFALSE\t/\tFALSE\t1700000000\tSID\tabc\n" }
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
    static const char *const named[] = { "domain", "name", "path" };
    const int caps[] = { COOKIE_DOMAIN_MAXLEN, COOKIE_NAME_MAXLEN, COOKIE_PATH_MAXLEN };

    for(int f=0 ; f<3 ; f++) {
      for(int over=0 ; over<=1 ; over++) {
        const CString big('a', caps[f] + over);
        const CookieReject want = over != 0 ? CookieFieldTooLong : CookieWritable;
        CookieRecord c = plainRecord();
        CString line;
        CookieReject got;

        switch(f) {
          case 0: c.domain = big; break;
          /* No value beside it, because the pair budget would refuse the baseline's
             three bytes next to a name this long. */
          case 1: c.name = big; c.value = ""; break;
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

  /* The name and the value share one budget in cookie_set(). The rows walk the budget,
     one byte past it, and both sides of the split. The last row is the one no
     single-field cap can refuse. */
  {
    static const struct { int name, value; CookieReject want; } pairs[] = {
      { COOKIE_PAIR_MAXLEN, 0, CookieWritable },
      { 1, COOKIE_PAIR_MAXLEN - 1, CookieWritable },
      { 1, COOKIE_PAIR_MAXLEN, CookieFieldTooLong },
      { COOKIE_PAIR_MAXLEN, 1, CookieFieldTooLong }
    };

    for(int k=0 ; k<(int) (sizeof(pairs) / sizeof(pairs[0])) ; k++) {
      CookieRecord c = plainRecord();
      CString line;
      CookieReject got;

      c.name = CString('n', pairs[k].name);
      c.value = CString('v', pairs[k].value);
      got = cookieFileLine(c, line);
      if (got != pairs[k].want) {
        snprintf(msg, sizeof(msg),
                 "a %d-byte name with a %d-byte value came back %d, expected %d (budget %d)",
                 pairs[k].name, pairs[k].value, (int) got, (int) pairs[k].want,
                 COOKIE_PAIR_MAXLEN);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }

  /* The line gate in cookie_load() stays unreachable only while the caps keep the longest
     possible record under it. Raise one past this and the gate goes live. */
  {
    CookieRecord c = plainRecord();
    CString line;

    c.domain = CString('d', COOKIE_DOMAIN_MAXLEN);
    c.name = CString('n', COOKIE_NAME_MAXLEN);
    c.value = CString('v', COOKIE_PAIR_MAXLEN - COOKIE_NAME_MAXLEN);
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

    if (odd[0] == odd[0]) {
      *err = "strtod(\"NAN\") did not return a NaN, so the case below proves nothing";
      return 0;
    }
    nchecks++;
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

  /* The header promises an emptied LINE on a refusal, on every path that refuses. A
     caller reusing one buffer is what would otherwise write the previous record twice. */
  {
    static const char *const named[] = { "an empty name", "a TAB in the value",
                                         "an untrusted domain", "an over-long value" };
    CookieRecord bad[4];
    CString line;

    for(int k=0 ; k<4 ; k++)
      bad[k] = plainRecord();
    bad[0].name = "";
    bad[1].value = "a\tb";
    bad[2].domain = "attacker.test@bank.test";
    bad[3].value = CString('a', COOKIE_PAIR_MAXLEN);
    for(int k=0 ; k<4 ; k++) {
      if (cookieFileLine(plainRecord(), line) != CookieWritable || line.IsEmpty()) {
        *err = "the baseline cookie did not write a record";
        return 0;
      }
      nchecks++;
      if (cookieFileLine(bad[k], line) == CookieWritable || !line.IsEmpty()) {
        snprintf(msg, sizeof(msg), "%s left '%s' in the caller's buffer",
                 named[k], (LPCSTR) line);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
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
    c.value = CString('a', COOKIE_PAIR_MAXLEN);
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
     first, says how many it left out, and still packs the jar as full as it will go. */
  {
    const int budget = COOKIE_JAR_MAXLEN - COOKIE_JAR_RESERVE;
    const CookieRecord base = plainRecord();
    /* The widest value the pair budget leaves this name, so the jar fills in the fewest
       records. Every length comes off the record itself, because a hand-counted one
       drifts the day the baseline changes. */
    const int wide = COOKIE_PAIR_MAXLEN - base.name.GetLength();
    const int fields = base.domain.GetLength() + base.path.GetLength()
                       + base.name.GetLength() + wide;
    CSimpleArray<CookieRecord> cookies;
    CookieFileStats stats;
    CString text;

    for(int k=0 ; k<64 ; k++) {
      CookieRecord c = plainRecord();

      c.value = CString('v', wide);
      cookies.Add(c);
    }
    text = cookieFileText(cookies, stats);
    if (stats.dropped[CookieJarFull] == 0
        || stats.written + stats.dropped[CookieJarFull] != 64) {
      snprintf(msg, sizeof(msg), "64 big cookies gave %d written and %d left out",
               stats.written, stats.dropped[CookieJarFull]);
      *err = msg;
      return 0;
    }
    nchecks++;
    /* These two hold the writer to its own budget: it fills the jar as full as that
       budget allows, and stops there. Whether the budget is the engine's is
       tools/test-cookie-file.py's job, which derives every byte of it. */
    if ((stats.written - 1) * (fields + COOKIE_JAR_RECORD) + fields > budget) {
      snprintf(msg, sizeof(msg), "%d records charge the jar %d bytes, past its %d",
               stats.written, (stats.written - 1) * (fields + COOKIE_JAR_RECORD) + fields,
               budget);
      *err = msg;
      return 0;
    }
    nchecks++;
    /* And one more would have to overflow, or the writer stopped early and threw away
       cookies the engine would have taken. */
    if (stats.written * (fields + COOKIE_JAR_RECORD) + fields <= budget) {
      snprintf(msg, sizeof(msg),
               "%d records is %d jar bytes of %d, so another would still have fitted",
               stats.written, stats.written * (fields + COOKIE_JAR_RECORD), budget);
      *err = msg;
      return 0;
    }
    nchecks++;
  }

  /* These are the paths the copy must land on, so the engine overwrites ours and never
     the user's own file. */
  {
    static const struct { const char *project, *want; } homes[] = {
      { "C:\\web\\proj", "C:\\web\\proj\\hts-cache\\cookies.txt" },
      { "C:\\web\\proj\\", "C:\\web\\proj\\hts-cache\\cookies.txt" },
      { "C:/web/proj/", "C:/web/proj/hts-cache\\cookies.txt" },
      { "", "" }
    };

    for(int k=0 ; k<(int) (sizeof(homes) / sizeof(homes[0])) ; k++) {
      const CString got = cookieFileInProject(homes[k].project);

      if (got != homes[k].want) {
        snprintf(msg, sizeof(msg), "project '%s' gave '%s', expected '%s'",
                 homes[k].project, (LPCSTR) got, homes[k].want);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
    /* A length cap anywhere in there would pass every row above, and silently drop the
       cookies of any project filed deep enough. */
    {
      const CString deep('d', 400);
      const CString got = cookieFileInProject(deep);

      if (got != deep + "\\hts-cache\\cookies.txt") {
        snprintf(msg, sizeof(msg), "a %d-byte project path gave %d bytes",
                 deep.GetLength(), got.GetLength());
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }

  return nchecks;
}
