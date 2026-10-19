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

#include "stdafx.h"

#include "browser-login.h"

#include <shlwapi.h>

/* Kept for the life of the process: the browser the helper opens owns threads of its own. */
static BOOL asked = FALSE;
static WhttLoginHelper state = WhttLoginNotInstalled;
static WhttBrowserLoginAvailableFn available = NULL;

static WhttLoginHelper loadHelper(void) {
  char path[MAX_PATH];
  const DWORD n = GetModuleFileNameA(NULL, path, (DWORD) sizeof(path));
  HMODULE dll;
  DWORD mode = 0;
  WhttBrowserLoginAbiFn abi;
  WhttBrowserLoginAvailableFn avail;

  if (n == 0 || n >= sizeof(path)) {
    return WhttLoginNotInstalled;
  }
  if (!PathRemoveFileSpecA(path) || !PathAppendA(path, "WhttBrowserLogin.dll")) {
    return WhttLoginNotInstalled;
  }
  if (!PathFileExistsA(path)) {
    return WhttLoginNotInstalled;
  }
  /* By full path, so nothing on the search path can answer in its place, and with
     critical errors off, because on Windows 7 this load is meant to fail. */
  SetThreadErrorMode(SEM_FAILCRITICALERRORS, &mode);
  dll = LoadLibraryA(path);
  SetThreadErrorMode(mode, NULL);
  if (dll == NULL) {
    return WhttLoginNotLoadable;
  }
  abi = (WhttBrowserLoginAbiFn) GetProcAddress(dll, "WhttBrowserLoginAbi");
  avail = (WhttBrowserLoginAvailableFn) GetProcAddress(dll, "WhttBrowserLoginAvailable");
  if (abi == NULL || avail == NULL) {
    FreeLibrary(dll);
    return WhttLoginNoExport;
  }
  /* A DLL and an exe from two builds would otherwise call through a signature that moved. */
  if (abi() != WHTT_BROWSER_LOGIN_ABI) {
    FreeLibrary(dll);
    return WhttLoginWrongAbi;
  }
  available = avail;
  return WhttLoginReady;
}

WhttLoginHelper WhttLoginHelperLoad(void) {
  if (!asked) {
    asked = TRUE;
    state = loadHelper();
  }
  return state;
}

const char *WhttLoginHelperWhy(WhttLoginHelper s) {
  switch(s) {
  case WhttLoginReady:
    return "ready";
  case WhttLoginNotInstalled:
    return "WhttBrowserLogin.dll is not beside the program";
  case WhttLoginNotLoadable:
    return "WhttBrowserLogin.dll will not load, as it does not on Windows 7";
  case WhttLoginNoExport:
    return "WhttBrowserLogin.dll has lost an entry point";
  case WhttLoginWrongAbi:
    return "WhttBrowserLogin.dll comes from another build";
  }
  return "WhttBrowserLogin.dll is in a state this build has no name for";
}

BOOL WhttLoginRuntimeAvailable(char *version, size_t version_size) {
  if (version != NULL && version_size != 0) {
    version[0] = '\0';
  }
  if (WhttLoginHelperLoad() != WhttLoginReady) {
    return FALSE;
  }
  return available(version, version_size) ? TRUE : FALSE;
}
