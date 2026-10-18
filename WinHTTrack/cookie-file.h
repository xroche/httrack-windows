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

#ifndef WHTT_COOKIE_FILE_H
#define WHTT_COOKIE_FILE_H

/* These caps are the engine's. Each is the smallest of three gates in src/htsbauth.c:
   the buffer cookie_load() reads the field into, cookie_host()'s buffer for the domain,
   and cookie_add()'s refusals. The engine drops a longer record with nothing but a
   log line, so the writer refuses it first and the caller can say so.
   tools/test-cookie-file.py recomputes all three from the engine's source. */
#define COOKIE_DOMAIN_MAXLEN 255
#define COOKIE_NAME_MAXLEN   256
#define COOKIE_VALUE_MAXLEN  1024
#define COOKIE_PATH_MAXLEN   255

/* cookie_load() skips a whole line this long or longer. The four caps above keep the
   longest record well under it, and cookieFileCheckCases() is what keeps it that way. */
#define COOKIE_LINE_MAXLEN   8000

/* The jar the engine loads into, and the headroom cookie_add() keeps free on every call.
   cookie_load() just stops once the jar fills, logging nothing, so a file over this
   budget loses whichever cookies sit at its end and says nothing. */
#define COOKIE_JAR_MAXLEN    30000
#define COOKIE_JAR_RESERVE   256

/* CookieRecord holds one browser cookie, every string in the engine's UTF-8. */
struct CookieRecord {
  CString name, value, domain, path;
  /* Both default to the safe answer, so a caller that fills in only the strings gets a
     plain session cookie rather than whatever the stack held. */
  BOOL secure = FALSE;
  /* Seconds since the epoch. Zero or less means the cookie goes when the browser session
     does, which is how this format spells it and how a browser reports it. */
  double expires = 0.0;
};

/* The caller counts these, because the cookie it drops may be the one carrying the
   login. */
enum CookieReject {
  CookieWritable,
  CookieEmptyField,      /* no name, or no domain to scope it to */
  CookieUnwritableByte,  /* a byte that would forge a field or a record */
  CookieUntrustedDomain, /* see badDomain() in cookie-file.cpp */
  CookieFieldTooLong,
  CookieJarFull,         /* the engine's jar filled up, so cookieFileText() alone */
  CookieRejectCount
};

/* CookieFileStats counts what cookieFileText() wrote and what it left out. dropped[] is
   indexed by the CookieReject that refused the cookie, so its CookieWritable slot stays
   zero. */
struct CookieFileStats {
  int written;
  int dropped[CookieRejectCount];
};

/* Renders C as one Netscape cookies.txt record, its closing LF included. Returns
   CookieWritable, or says why it refused and empties LINE. */
CookieReject cookieFileLine(const CookieRecord &c, CString &line);

/* Renders every writable record of COOKIES, in order, stopping before the engine's jar
   would overflow, and fills STATS. */
CString cookieFileText(const CSimpleArray<CookieRecord> &cookies, CookieFileStats &stats);

/* Shared with --selftest. See tools/cookie-file-test.cpp. Returns the number of checks
   run, or zero with ERR set. */
int cookieFileCheckCases(CString *err);

#endif
