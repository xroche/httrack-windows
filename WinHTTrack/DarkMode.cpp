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
/*       Dark colours for dialogs and common controls           */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "stdafx.h"
#include "DarkMode.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define DARK_DLG_BG   RGB(32, 32, 32)
#define DARK_EDIT_BG  RGB(45, 45, 45)
#define DARK_TEXT     RGB(240, 240, 240)

/* DWMWA_USE_IMMERSIVE_DARK_MODE, and the number it answered to before Windows 10 20H1. */
#define DARK_DWMWA_DARK_MODE      20
#define DARK_DWMWA_DARK_MODE_OLD  19

#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif

typedef HRESULT (WINAPI *dark_DwmSetWindowAttribute_t)(HWND, DWORD, LPCVOID, DWORD);
typedef HRESULT (WINAPI *dark_SetWindowTheme_t)(HWND, LPCWSTR, LPCWSTR);

static BOOL darkOn = FALSE;
static HBRUSH darkDlgBrush = NULL;
static HBRUSH darkEditBrush = NULL;

/* Resolved rather than imported. Neither API has to exist on Windows 7, and a missing
   import would stop the app loading. */
static FARPROC darkProc(const wchar_t *dll, const char *name)
{
  HMODULE module = GetModuleHandleW(dll);

  /* System32 only, never the application directory, where a copy could be planted.
     A Windows 7 without KB2533623 rejects the flag, and the feature stays off. */
  if (module == NULL)
    module = LoadLibraryExW(dll, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
  return module != NULL ? GetProcAddress(module, name) : NULL;
}

/* An absent value means light, which is also the only answer before Windows 10. */
static BOOL darkSystemPrefersDark(void)
{
  HKEY key;
  DWORD light = 1, size = sizeof(light), type = REG_DWORD;

  if (RegOpenKeyExA(HKEY_CURRENT_USER,
                    "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                    0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
    return FALSE;
  if (RegQueryValueExA(key, "AppsUseLightTheme", NULL, &type, (LPBYTE) &light,
                       &size) != ERROR_SUCCESS
      || type != REG_DWORD || size != sizeof(light))
    light = 1;   /* a short or wrong-typed value leaves light half-written */
  RegCloseKey(key);
  return light == 0;
}

void WhttDarkModeInit(BOOL requested)
{
  darkOn = requested && darkSystemPrefersDark();
  if (!darkOn)
    return;
  darkDlgBrush = CreateSolidBrush(DARK_DLG_BG);
  darkEditBrush = CreateSolidBrush(DARK_EDIT_BG);
  /* One brush short would leave half the window unpainted, so give up on all of it. */
  if (darkDlgBrush == NULL || darkEditBrush == NULL) {
    DeleteObject(darkDlgBrush);
    DeleteObject(darkEditBrush);
    darkDlgBrush = darkEditBrush = NULL;
    darkOn = FALSE;
  }
}

static void darkTitleBar(HWND hwnd)
{
  static dark_DwmSetWindowAttribute_t setAttribute = NULL;
  static BOOL resolved = FALSE;
  const BOOL on = TRUE;

  if (!resolved) {
    setAttribute =
      (dark_DwmSetWindowAttribute_t) darkProc(L"dwmapi.dll", "DwmSetWindowAttribute");
    resolved = TRUE;
  }
  if (setAttribute == NULL)
    return;
  /* Windows 7 knows neither attribute and answers an error, which leaves the window
     exactly as it was. */
  if (FAILED(setAttribute(hwnd, DARK_DWMWA_DARK_MODE, &on, sizeof(on))))
    setAttribute(hwnd, DARK_DWMWA_DARK_MODE_OLD, &on, sizeof(on));
}

static void darkThemeControl(HWND hwnd)
{
  static dark_SetWindowTheme_t setTheme = NULL;
  static BOOL resolved = FALSE;
  char name[32];

  if (!resolved) {
    setTheme = (dark_SetWindowTheme_t) darkProc(L"uxtheme.dll", "SetWindowTheme");
    resolved = TRUE;
  }
  if (setTheme == NULL || GetClassNameA(hwnd, name, sizeof(name)) == 0)
    return;
  /* A theme name the system does not know falls back to the default one, so an older
     Windows just keeps its own look. The theme carries the control's scrollbars. */
  if (lstrcmpiA(name, "SysTreeView32") == 0 || lstrcmpiA(name, "SysListView32") == 0)
    setTheme(hwnd, L"DarkMode_Explorer", NULL);
}

static BOOL CALLBACK darkThemeChild(HWND hwnd, LPARAM)
{
  darkThemeControl(hwnd);
  return TRUE;
}

void WhttDarkInitWindow(CWnd *wnd)
{
  HWND hwnd;

  if (!darkOn || wnd == NULL || (hwnd = wnd->GetSafeHwnd()) == NULL)
    return;
  /* A property page is a child window and has no title bar of its own. */
  if ((GetWindowLong(hwnd, GWL_STYLE) & WS_CHILD) == 0)
    darkTitleBar(hwnd);
  darkThemeControl(hwnd);
  EnumChildWindows(hwnd, darkThemeChild, 0);
}

HBRUSH WhttDarkCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor)
{
  if (!darkOn || pDC == NULL)
    return NULL;
  switch (nCtlColor) {
  case CTLCOLOR_DLG:
  case CTLCOLOR_STATIC:   /* static text and group boxes sit on the dialog's background */
    pDC->SetTextColor(DARK_TEXT);
    pDC->SetBkColor(DARK_DLG_BG);
    return darkDlgBrush;
  case CTLCOLOR_EDIT:
    pDC->SetTextColor(DARK_TEXT);
    pDC->SetBkColor(DARK_EDIT_BG);
    return darkEditBrush;
  }
  return NULL;
}
