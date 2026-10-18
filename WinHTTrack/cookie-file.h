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

/* What one jar record may carry, from cookie_add() and cookie_host() in the engine's
   src/htsbauth.c. A longer field is refused there with nothing but a log line, so the
   writer refuses it here instead and the caller can say so. tools/test-cookie-file.py
   checks these against the engine's own source. */
#define COOKIE_DOMAIN_MAXLEN 255
#define COOKIE_NAME_MAXLEN   256
#define COOKIE_VALUE_MAXLEN  1024
#define COOKIE_PATH_MAXLEN   256

/* cookie_load() skips a whole line this long or longer. Unreachable under the four caps
   above, and cookieFileCheckCases() is what keeps it that way. */
#define COOKIE_LINE_MAXLEN   8000

/* One cookie as the browser handed it over, every string in the engine's UTF-8. */
struct CookieRecord {
  CString name, value, domain, path;
  BOOL secure;
  BOOL session;   /* expires with the browser session, so field 4 goes out as 0 */
  double expires; /* seconds since the epoch, read only when SESSION is clear */
};

/* Why a cookie cannot be written. The caller counts these, because the one it drops may
   be the one carrying the login. */
enum CookieReject {
  CookieWritable,
  CookieEmptyField,      /* no name, or no domain to scope it to */
  CookieUnwritableByte,  /* a byte that would forge a field, a record or a comment */
  CookieFieldTooLong,
  CookieRejectCount
};

/* What cookieFileText() made of the cookies it was given. */
struct CookieFileStats {
  int written;
  int dropped[CookieRejectCount];   /* indexed by the CookieReject that refused it */
};

/* Renders C as one Netscape cookies.txt record, its closing LF included, or says why it
   cannot and leaves LINE empty. */
CookieReject cookieFileLine(const CookieRecord &c, CString &line);

/* Renders every writable record of COOKIES, in order, and fills STATS. */
CString cookieFileText(const CookieRecord *cookies, int count, CookieFileStats *stats);

/* Shared with --selftest; see tools/cookie-file-test.cpp. Returns the number of checks
   run, or zero with ERR set. */
int cookieFileCheckCases(CString *err);

#endif
