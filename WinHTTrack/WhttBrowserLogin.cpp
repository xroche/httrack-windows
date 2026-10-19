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
/*       WhttBrowserLogin.dll entry points                      */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include <windows.h>
#include <objbase.h>
#include <string.h>

/* WebView2LoaderStatic.lib defines what this declares, and linking the static loader is
   what keeps Microsoft's loader DLL out of the package. */
#include <WebView2.h>

#include "WhttBrowserLogin.h"

unsigned __cdecl WhttBrowserLoginAbi(void) {
  return WHTT_BROWSER_LOGIN_ABI;
}

int __cdecl WhttBrowserLoginAvailable(char *version, size_t version_size) {
  LPWSTR found = NULL;
  char utf8[WHTT_BROWSER_LOGIN_VERSION_MAX * 2];
  HRESULT hr;
  size_t len;

  utf8[0] = '\0';
  if (version != NULL && version_size != 0) {
    version[0] = '\0';
  }
  /* NULL: the Evergreen runtime, wherever it is installed. A failure is the answer "none
     installed", not an error to report: there is nothing the user could fix. */
  hr = GetAvailableCoreWebView2BrowserVersionString(NULL, &found);
  if (SUCCEEDED(hr) && found != NULL
      && WideCharToMultiByte(CP_UTF8, 0, found, -1, utf8, (int) sizeof(utf8), NULL, NULL) <= 0) {
    utf8[0] = '\0';
  }
  if (found != NULL) {
    CoTaskMemFree(found);
  }
  /* No runtime and a version we cannot read are the same answer to the caller. */
  if (utf8[0] == '\0') {
    return 0;
  }
  if (version != NULL && version_size != 0) {
    len = strlen(utf8);
    if (len > version_size - 1) {
      len = version_size - 1;
    }
    memcpy(version, utf8, len);
    version[len] = '\0';
  }
  return 1;
}
