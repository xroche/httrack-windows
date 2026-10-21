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
/*       the browser-login helper DLL, and the login it runs    */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef WHTT_BROWSER_LOGIN_LOADER_H
#define WHTT_BROWSER_LOGIN_LOADER_H

/* The entry points this loader resolves. A Whtt prefix marks what crosses into the DLL,
   so everything named below stays inside the program. */
#include "WhttBrowserLogin.h"

/* CookieRecord, which is what a harvested cookie becomes on this side. */
#include "cookie-file.h"

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

/* What one browser login brought back. The cookies are the whole profile, not one
   site's, because a login that crosses two hosts leaves the cookie that matters on the
   host nobody asked about. */
struct BrowserLoginHarvest {
  CSimpleArray<CookieRecord> cookies;
  CString userAgent;
  /* Where the user ended up. This is the address to crawl, because navigating to it by
     hand is what the browser was opened for. */
  CString finalUrl;
};

/* Opens the browser on URL with PARENT greyed out under it, and fills HARVEST once the
   user closes it. PROFILE is the folder the browser keeps its login in. */
WhttBrowserLoginResult browserLoginRun(HWND parent, const CString &url,
                                       const CString &profile,
                                       BrowserLoginHarvest &harvest);

/* Returns a string literal naming RESULT, for a user or for --selftest. */
const char *browserLoginRunWhy(WhttBrowserLoginResult result);

/* The page the browser opens first: the first address of URLS, given the scheme the
   crawl would give it. Empty when URLS holds no address. */
CString browserLoginStartUrl(const CString &urls);

/* What of a landed address may be kept. The browser can finish holding a secret in the
   URL, and the address box is saved into the project's winprofile.ini in clear. Returns
   empty when the address must not be kept at all. */
CString browserLoginSafeUrl(const CString &landed);

/* The folder the browser keeps its profile in. The project's own when PROJECTPATH names
   one, so one project's jar never fills up with another's cookies, and LOCALAPPDATA when
   there is no project yet. Empty when neither is given. */
CString browserLoginProfileDir(const CString &projectPath, const CString &localAppData);

/* Where the harvest is written. The project's own cookies.txt when there is a project,
   so the engine refreshes it at the end of every mirror; a file of the user's own while
   there is no project, which the shell then copies into one. */
CString browserLoginCookieFile(const CString &projectPath, const CString &localAppData);

/* The user's own data folder, which is where a login made before there is a project
   goes. Empty when the shell will not say. */
CString browserLoginUserDir(void);

/* What browserLoginSaveCookies() did. */
enum CookieSaveResult {
  CookieSaveWritten,
  CookieSaveNothingToWrite,  /* the engine would refuse every cookie the browser gave */
  CookieSaveNoFile           /* the folder or the file would not take it */
};

/* Writes HARVEST over PATH as one Netscape cookies.txt, making the folder first, and
   fills STATS with what went in and what was dropped. */
CookieSaveResult browserLoginSaveCookies(const BrowserLoginHarvest &harvest,
                                         const CString &path, CookieFileStats &stats);

/* TRANSLATED, or ENGLISH when the catalog does not carry the key. LANGSEL() answers an
   empty string for a key it does not know, and an empty message box tells nobody
   anything, so every call site that cannot keep its own text goes through this. */
const char *browserLoginText(const char *translated, const char *english);

/* Shared with --selftest. Returns the number of checks run, or zero with ERR set. */
int browserLoginCheckCases(CString *err);

#endif
