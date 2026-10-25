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

#include "stdafx.h"
#include "Shell.h"
extern "C" {
  #include "HTTrackInterface.h"
};

#include "browser-login.h"

#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>

/* Kept for the life of the process, because the browser the helper opens owns threads of
   its own. */
static BOOL asked = FALSE;
static BrowserLoginState state = BrowserLoginMissing;
static WhttBrowserLoginAvailableFn available = NULL;
static WhttBrowserLoginRunFn runner = NULL;

/* Fills the two entry points above on BrowserLoginReady and leaves them alone
   otherwise. */
static BrowserLoginState loadHelper(void) {
  char path[MAX_PATH];
  const DWORD n = GetModuleFileNameA(NULL, path, (DWORD) sizeof(path));

  if (n == 0 || n >= sizeof(path)) {
    return BrowserLoginMissing;
  }
  if (!PathRemoveFileSpecA(path) || !PathAppendA(path, "WhttBrowserLogin.dll")) {
    return BrowserLoginMissing;
  }
  if (!PathFileExistsA(path)) {
    return BrowserLoginMissing;
  }
  /* By full path, so nothing on the search path can answer in its place, and with
     critical errors off, because on Windows 7 this load is meant to fail. */
  DWORD mode = 0;
  SetThreadErrorMode(SEM_FAILCRITICALERRORS, &mode);
  HMODULE dll = LoadLibraryA(path);
  SetThreadErrorMode(mode, NULL);
  if (dll == NULL) {
    return BrowserLoginNotLoadable;
  }
  const WhttBrowserLoginAbiFn abi =
    (WhttBrowserLoginAbiFn) GetProcAddress(dll, "WhttBrowserLoginAbi");
  const WhttBrowserLoginAvailableFn avail =
    (WhttBrowserLoginAvailableFn) GetProcAddress(dll, "WhttBrowserLoginAvailable");
  const WhttBrowserLoginRunFn run =
    (WhttBrowserLoginRunFn) GetProcAddress(dll, "WhttBrowserLoginRun");
  if (abi == NULL || avail == NULL || run == NULL) {
    FreeLibrary(dll);
    return BrowserLoginNoExport;
  }
  /* A DLL and an exe from two builds would otherwise call through a signature that moved. */
  if (abi() != WHTT_BROWSER_LOGIN_ABI) {
    FreeLibrary(dll);
    return BrowserLoginWrongAbi;
  }
  available = avail;
  runner = run;
  return BrowserLoginReady;
}

BrowserLoginState browserLoginLoad(void) {
  if (!asked) {
    asked = TRUE;
    state = loadHelper();
  }
  return state;
}

const char *browserLoginWhy(BrowserLoginState state) {
  switch(state) {
  case BrowserLoginReady:
    return "ready";
  case BrowserLoginMissing:
    return "WhttBrowserLogin.dll is not beside the program";
  case BrowserLoginNotLoadable:
    return "WhttBrowserLogin.dll will not load, as it does not on Windows 7";
  case BrowserLoginNoExport:
    return "WhttBrowserLogin.dll has lost an entry point";
  case BrowserLoginWrongAbi:
    return "WhttBrowserLogin.dll comes from another build";
  }
  return "WhttBrowserLogin.dll is in a state this build has no name for";
}

BOOL browserLoginRuntime(char *version, size_t version_size) {
  if (version != NULL && version_size != 0) {
    version[0] = '\0';
  }
  if (browserLoginLoad() != BrowserLoginReady) {
    return FALSE;
  }
  return available(version, version_size) ? TRUE : FALSE;
}

/* Takes one cookie out of the DLL. */
static int __cdecl takeCookie(void *ctx, const WhttBrowserLoginCookie *cookie) {
  BrowserLoginHarvest *const harvest = (BrowserLoginHarvest *) ctx;
  CookieRecord record;

  record.name = cookie->name;
  record.value = cookie->value;
  record.domain = cookie->domain;
  record.path = cookie->path;
  record.secure = cookie->secure ? TRUE : FALSE;
  record.expires = cookie->expires;
  harvest->cookies.Add(record);
  return 1;
}

/* The DLL reads and writes UTF-8, and every CString here is in the ANSI codepage. */
static CString utf8Of(const CString &text) {
  char *converted = strdupt_utf8(text);
  const CString out(converted != NULL ? converted : (LPCSTR) text);

  freet(converted);
  return out;
}

// see browser-login.h
WhttBrowserLoginResult browserLoginRun(HWND parent, const CString &url,
                                       const CString &profile,
                                       BrowserLoginHarvest &harvest) {
  /* -F takes whatever this holds, and the engine refuses an argument at HTS_CDLMAXSIZE,
     so a longer agent would lose the mirror rather than the field. */
  char agent[HTS_CDLMAXSIZE];
  char ansi[HTS_CDLMAXSIZE];
  /* The engine refuses a longer address anyway, and the field caps shorter still. */
  char landed[HTS_URLMAXSIZE * 2];
  const CString wantUrl = utf8Of(url), wantProfile = utf8Of(profile);
  /* The DLL reaches no catalog, so its window carries whatever we hand it, and an empty
     field there means it falls back to its own English. */
  const CString wantTitle = utf8Of(LANG(LANG_BROWSERLOGINTITLE));
  const CString wantGo = utf8Of(LANG(LANG_BROWSERLOGINGO));
  const CString wantDone = utf8Of(LANG(LANG_BROWSERLOGINDONE));
  const CString wantClear = utf8Of(LANG(LANG_BROWSERLOGINCLEAR));
  WhttBrowserLoginRequest request;

  if (browserLoginLoad() != BrowserLoginReady)
    return WhttBrowserLoginNoBrowser;

  memset(&request, 0, sizeof(request));
  request.size = sizeof(request);
  request.parent = parent;
  request.url = wantUrl.IsEmpty() ? NULL : (LPCSTR) wantUrl;
  request.profile = wantProfile.IsEmpty() ? NULL : (LPCSTR) wantProfile;
  request.title = wantTitle.IsEmpty() ? "WinHTTrack - log in with a browser"
                                     : (LPCSTR) wantTitle;
  request.goText = wantGo.IsEmpty() ? NULL : (LPCSTR) wantGo;
  request.doneText = wantDone.IsEmpty() ? NULL : (LPCSTR) wantDone;
  request.clearText = wantClear.IsEmpty() ? NULL : (LPCSTR) wantClear;
  request.onCookie = takeCookie;
  request.onCookieCtx = &harvest;
  request.userAgent = agent;
  request.userAgentSize = sizeof(agent);
  request.finalUrl = landed;
  request.finalUrlSize = sizeof(landed);

  /* The DLL empties the buffer before it does anything else, so a run that stops early
     leaves no stale agent behind. */
  const WhttBrowserLoginResult result = (WhttBrowserLoginResult) runner(&request);

  if (result == WhttBrowserLoginDone) {
    CopyTextUTF8ToCP(ansi, sizeof(ansi), agent);
    harvest.userAgent = ansi;

    char here[HTS_URLMAXSIZE * 2];

    CopyTextUTF8ToCP(here, sizeof(here), landed);
    harvest.finalUrl = here;
  }
  return result;
}

// see browser-login.h
const char *browserLoginRunWhy(WhttBrowserLoginResult result) {
  switch(result) {
  case WhttBrowserLoginDone:
    return "done";
  case WhttBrowserLoginBadRequest:
    return "WhttBrowserLogin.dll comes from another build";
  case WhttBrowserLoginNoRuntime:
    return "no WebView2 runtime is installed";
  case WhttBrowserLoginNoApartment:
    return "the program is not in a single-threaded apartment";
  case WhttBrowserLoginNoWindow:
    return "the browser window would not open";
  case WhttBrowserLoginNoBrowser:
    return "the WebView2 runtime would not start";
  case WhttBrowserLoginBrowserGone:
    return "the browser stopped while the window was open";
  }
  return "the browser stopped for a reason this build has no name for";
}

// see browser-login.h
const char *browserLoginText(const char *translated, const char *english) {
  return translated != NULL && *translated != '\0' ? translated : english;
}

// see browser-login.h
CString browserLoginUserDir(void) {
  char path[MAX_PATH];

  path[0] = '\0';
  if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, path)))
    return CString();
  return CString(path);
}

/* A silent recursive delete. CWinHTTrackApp::RmDir does the same walk, but it raises a
   message box on every failure, and a profile the browser still holds open fails often,
   so a sweep using it would greet the user with a stack of dialogs. */
static void removeTree(const CString &dir) {
  WIN32_FIND_DATAA found;
  const HANDLE walk = FindFirstFileA(pathUnder(dir, "*"), &found);

  if (walk != INVALID_HANDLE_VALUE) {
    do {
      if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0)
        continue;

      const CString here = pathUnder(dir, found.cFileName);

      /* Never descend a reparse point: its target is outside the folder we chose. */
      if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
          && !(found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        removeTree(here);
      else if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        RemoveDirectoryA(here);
      else {
        SetFileAttributesA(here, FILE_ATTRIBUTE_NORMAL);
        DeleteFileA(here);
      }
    } while (FindNextFileA(walk, &found));
    FindClose(walk);
  }
  RemoveDirectoryA(dir);
}

// see browser-login.h
void browserLoginForgetProfile(const CString &dir) {
  if (!dir.IsEmpty())
    removeTree(dir);
}

// see browser-login.h
void browserLoginSweepProfiles(void) {
  const CString root = browserLoginProfileRoot(browserLoginUserDir());

  if (root.IsEmpty())
    return;

  WIN32_FIND_DATAA found;
  const HANDLE walk = FindFirstFileA(pathUnder(root, "*"), &found);

  if (walk == INVALID_HANDLE_VALUE)
    return;
  do {
    unsigned long run = 0;

    if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        || !browserLoginStaleProfile(found.cFileName, &run))
      continue;

    /* Another copy of the program may be signed in right now, so a profile whose run
       is still going is left alone. Skipping is the safe direction, because the next
       startup sweeps it, where deleting it would sign that copy out mid-login. */
    const HANDLE live = OpenProcess(SYNCHRONIZE, FALSE, (DWORD) run);

    if (live != NULL) {
      CloseHandle(live);
      continue;
    }
    /* Owned by another user, so it is running and simply not ours to open. */
    if (GetLastError() == ERROR_ACCESS_DENIED)
      continue;
    removeTree(pathUnder(root, found.cFileName));
  } while (FindNextFileA(walk, &found));
  FindClose(walk);
}

// see browser-login.h
/* Binary, because the engine reads these as bytes and a CR of ours would end up inside
   the last field of every cookie record. */
static CookieSaveResult writeHarvestFile(const CString &path, const CString &text) {
  if (path.IsEmpty())
    return CookieSaveNoFile;
  structcheck(path);

  FILE *const fp = fopen(path, "wb");

  if (fp == NULL)
    return CookieSaveNoFile;

  const size_t len = (size_t) text.GetLength();
  const BOOL written = fwrite((LPCSTR) text, 1, len, fp) == len;

  return fclose(fp) == 0 && written ? CookieSaveWritten : CookieSaveNoFile;
}

CookieSaveResult browserLoginSaveBundle(const BrowserLoginHarvest &harvest,
                                        const CString &path, CookieFileStats &stats) {
  const CString records = cookieFileText(harvest.cookies, stats);

  return writeHarvestFile(path, browserLoginBundleText(harvest.userAgent, records));
}
