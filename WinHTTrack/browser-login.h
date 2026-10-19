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

#ifndef WHTT_BROWSER_LOGIN_LOAD_H
#define WHTT_BROWSER_LOGIN_LOAD_H

#include "WhttBrowserLogin.h"

/* Where loading WhttBrowserLogin.dll stopped. Windows 7 stops at WhttLoginNotLoadable,
   which is the designed answer there and not a failure to report. */
enum WhttLoginHelper {
  WhttLoginReady,
  WhttLoginNotInstalled, /* nothing of that name beside the program */
  WhttLoginNotLoadable,  /* Windows 7, or a file we cannot run */
  WhttLoginNoExport,
  WhttLoginWrongAbi      /* a DLL from another build of the program */
};

/* Loads the helper on the first call and keeps the answer. The UI thread owns it. */
WhttLoginHelper WhttLoginHelperLoad(void);

/* What to show a user, or --selftest, for a state. */
const char *WhttLoginHelperWhy(WhttLoginHelper state);

/* Is a WebView2 runtime installed? FALSE unless the helper is ready. VERSION, when
   given, takes the runtime's version and is never left empty on TRUE. */
BOOL WhttLoginRuntimeAvailable(char *version, size_t version_size);

#endif
