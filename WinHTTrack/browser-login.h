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
/*       loading the browser-login helper DLL                   */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef WHTT_BROWSER_LOGIN_LOADER_H
#define WHTT_BROWSER_LOGIN_LOADER_H

/* The entry points this loader resolves. A Whtt prefix marks what crosses into the DLL,
   so everything named below stays inside the program. */
#include "WhttBrowserLogin.h"

/* Windows 7 stops at BrowserLoginNotLoadable, which is the designed answer there. */
enum BrowserLoginState {
  BrowserLoginReady,
  BrowserLoginMissing, /* nothing of that name beside the program */
  BrowserLoginNotLoadable,  /* Windows 7, or a file we cannot run */
  BrowserLoginNoExport,
  BrowserLoginWrongAbi      /* a DLL from another build of the program */
};

/* Loads the helper on the first call and keeps the answer. Only the UI thread may call
   it, which is also what keeps the cached answer safe. */
BrowserLoginState browserLoginLoad(void);

/* Returns a string literal naming STATE, for a user or for --selftest. */
const char *browserLoginWhy(BrowserLoginState state);

/* Is a WebView2 runtime installed? FALSE unless the helper is ready. VERSION, when
   given, takes the runtime's version and is never left empty on TRUE. */
BOOL browserLoginRuntime(char *version, size_t version_size);

#endif
