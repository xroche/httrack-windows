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
#include <uxtheme.h>
#include <vsstyle.h>
#include "DarkMode.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define DARK_DLG_BG   RGB(32, 32, 32)
#define DARK_EDIT_BG  RGB(45, 45, 45)
#define DARK_TEXT     RGB(240, 240, 240)
#define DARK_TEXT_OFF RGB(140, 140, 140)

/* Between the glyph and its label, at 96 dpi. */
#define DARK_GLYPH_GAP 3

/* DWMWA_USE_IMMERSIVE_DARK_MODE, and the number it answered to before Windows 10 20H1. */
#define DARK_DWMWA_DARK_MODE      20
#define DARK_DWMWA_DARK_MODE_OLD  19

#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif

typedef HRESULT (WINAPI *dark_DwmSetWindowAttribute_t)(HWND, DWORD, LPCVOID, DWORD);
typedef HRESULT (WINAPI *dark_SetWindowTheme_t)(HWND, LPCWSTR, LPCWSTR);
typedef HTHEME (WINAPI *dark_OpenThemeData_t)(HWND, LPCWSTR);
typedef HRESULT (WINAPI *dark_CloseThemeData_t)(HTHEME);
typedef HRESULT (WINAPI *dark_DrawThemeBackground_t)(HTHEME, HDC, int, int, LPCRECT, LPCRECT);
typedef HRESULT (WINAPI *dark_GetThemePartSize_t)(HTHEME, HDC, int, int, LPCRECT, THEMESIZE,
                                                  SIZE *);

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

/* The theme calls the glyph needs, all named exports: the dark-mode ordinals have moved
   between Windows builds. */
static struct {
  BOOL resolved;
  dark_OpenThemeData_t open;
  dark_CloseThemeData_t close;
  dark_DrawThemeBackground_t draw;
  dark_GetThemePartSize_t size;
} darkTheme;

static BOOL darkThemeApi(void)
{
  if (!darkTheme.resolved) {
    darkTheme.open = (dark_OpenThemeData_t) darkProc(L"uxtheme.dll", "OpenThemeData");
    darkTheme.close = (dark_CloseThemeData_t) darkProc(L"uxtheme.dll", "CloseThemeData");
    darkTheme.draw =
      (dark_DrawThemeBackground_t) darkProc(L"uxtheme.dll", "DrawThemeBackground");
    darkTheme.size = (dark_GetThemePartSize_t) darkProc(L"uxtheme.dll", "GetThemePartSize");
    darkTheme.resolved = TRUE;
  }
  return darkTheme.open != NULL && darkTheme.close != NULL && darkTheme.draw != NULL
    && darkTheme.size != NULL;
}

/* The dark glyph where this Windows has one: a class it does not know answers NULL. The
   last name is the light glyph, bright against the dark page but still native. */
static HTHEME darkOpenButtonTheme(HWND hwnd)
{
  static const wchar_t *const classes[] = {
    L"DarkMode_Explorer::Button", L"DarkMode::Button", L"Button"
  };
  HTHEME theme = NULL;

  for(int i=0 ; theme == NULL && i < (int) (sizeof(classes)/sizeof(classes[0])) ; i++)
    theme = darkTheme.open(hwnd, classes[i]);
  return theme;
}

static BOOL darkIsRadio(DWORD style)
{
  const DWORD type = style & BS_TYPEMASK;

  return type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON;
}

int WhttDarkGlyphState(DWORD style, UINT state)
{
  /* vsstyle.h numbers both glyphs alike: unchecked 1 to 4 then checked 5 to 8, each run
     reading normal, hot, pressed, disabled. Mixed, 9 to 12, is the check box only: a
     radio has no such art, and never reports BST_INDETERMINATE either. */
  int base = CBS_UNCHECKEDNORMAL;

  if ((state & BST_INDETERMINATE) != 0)
    base = CBS_MIXEDNORMAL;
  else if ((state & BST_CHECKED) != 0)
    base = CBS_CHECKEDNORMAL;
  if ((style & WS_DISABLED) != 0)
    return base + 3;
  if ((state & BST_PUSHED) != 0)
    return base + 2;
  if ((state & BST_HOT) != 0)
    return base + 1;
  return base;
}

/* What DrawFrameControl wants for the same glyph, where no theme is active. */
static UINT darkFrameState(DWORD style, UINT state)
{
  UINT flags = darkIsRadio(style) ? DFCS_BUTTONRADIO : DFCS_BUTTONCHECK;

  if ((state & BST_INDETERMINATE) != 0)
    flags |= DFCS_BUTTON3STATE | DFCS_CHECKED;
  else if ((state & BST_CHECKED) != 0)
    flags |= DFCS_CHECKED;
  if ((style & WS_DISABLED) != 0)
    flags |= DFCS_INACTIVE;
  if ((state & BST_PUSHED) != 0)
    flags |= DFCS_PUSHED;
  if ((state & BST_HOT) != 0)
    flags |= DFCS_HOT;
  return flags;
}

/* How the label is laid out, from the control's style and the dialog's UI-cue state. */
static UINT darkTextFlags(DWORD style, UINT ui)
{
  UINT flags = ((style & BS_MULTILINE) != 0) ? (DT_WORDBREAK | DT_TOP)
    : (DT_SINGLELINE | DT_VCENTER);

  if ((style & BS_CENTER) == BS_CENTER)
    flags |= DT_CENTER;
  else if ((style & BS_RIGHT) != 0)
    flags |= DT_RIGHT;
  if ((ui & UISF_HIDEACCEL) != 0)
    flags |= DT_HIDEPREFIX;
  return flags;
}

/* Paints the whole control, because a themed check or radio button draws its own label
   and ignores WM_CTLCOLORBTN, so the text would stay black on the dark page. */
static void darkPaintButton(HWND hwnd, HDC hdc)
{
  const DWORD style = (DWORD) GetWindowLong(hwnd, GWL_STYLE);
  const UINT ui = (UINT) SendMessage(hwnd, WM_QUERYUISTATE, 0, 0);
  const UINT state = (UINT) SendMessage(hwnd, BM_GETSTATE, 0, 0);
  const UINT flags = darkTextFlags(style, ui);
  const int part = darkIsRadio(style) ? BP_RADIOBUTTON : BP_CHECKBOX;
  const int glyphState = WhttDarkGlyphState(style, state);
  const HFONT font = (HFONT) SendMessage(hwnd, WM_GETFONT, 0, 0);
  const int gap = MulDiv(DARK_GLYPH_GAP, GetDeviceCaps(hdc, LOGPIXELSX), 96);
  HGDIOBJ oldFont = NULL;
  COLORREF oldColor;
  int oldBkMode;
  HTHEME theme = NULL;
  SIZE glyph = { 0, 0 };
  RECT client, box, text;
  WCHAR caption[512];

  GetClientRect(hwnd, &client);
  FillRect(hdc, &client, darkDlgBrush);
  caption[0] = L'\0';
  GetWindowTextW(hwnd, caption, sizeof(caption) / sizeof(caption[0]));
  if (font != NULL)
    oldFont = SelectObject(hdc, font);
  if (darkThemeApi())
    theme = darkOpenButtonTheme(hwnd);
  /* 13 by 13 at 96 dpi is what the theme answers, and what Windows drew before themes. */
  if (theme == NULL
      || FAILED(darkTheme.size(theme, hdc, part, glyphState, NULL, TS_DRAW, &glyph))
      || glyph.cx <= 0 || glyph.cy <= 0)
    glyph.cx = glyph.cy = MulDiv(13, GetDeviceCaps(hdc, LOGPIXELSX), 96);

  box.top = client.top + (client.bottom - client.top - glyph.cy) / 2;
  box.bottom = box.top + glyph.cy;
  text.top = client.top;
  text.bottom = client.bottom;
  /* BS_LEFTTEXT and BS_RIGHTBUTTON are the same bit: the glyph moves to the far end. */
  if ((style & BS_RIGHTBUTTON) != 0) {
    box.right = client.right;
    box.left = box.right - glyph.cx;
    text.left = client.left;
    text.right = box.left - gap;
  } else {
    box.left = client.left;
    box.right = box.left + glyph.cx;
    text.left = box.right + gap;
    text.right = client.right;
  }

  /* DrawText centres one line for us, but a wrapped block has to be measured first. */
  if ((style & BS_MULTILINE) != 0) {
    RECT measured = text;
    int height;

    DrawTextW(hdc, caption, -1, &measured, flags | DT_CALCRECT);
    height = measured.bottom - measured.top;
    if (height < text.bottom - text.top) {
      text.top += (text.bottom - text.top - height) / 2;
      text.bottom = text.top + height;
    }
  }

  if (theme != NULL)
    darkTheme.draw(theme, hdc, part, glyphState, &box, NULL);
  else
    DrawFrameControl(hdc, &box, DFC_BUTTON, darkFrameState(style, state));

  /* The DC belongs to the caller under WM_PRINT, and to the parent under CS_PARENTDC. */
  oldBkMode = SetBkMode(hdc, TRANSPARENT);
  oldColor = SetTextColor(hdc, ((style & WS_DISABLED) != 0) ? DARK_TEXT_OFF : DARK_TEXT);
  DrawTextW(hdc, caption, -1, &text, flags);
  if ((state & BST_FOCUS) != 0 && (ui & UISF_HIDEFOCUS) == 0) {
    RECT focus = text;

    DrawTextW(hdc, caption, -1, &focus, flags | DT_CALCRECT);
    /* DT_CALCRECT ignores DT_VCENTER, so put the single-line box back where the text is. */
    if ((style & BS_MULTILINE) == 0) {
      const int height = focus.bottom - focus.top;

      focus.top = text.top + (text.bottom - text.top - height) / 2;
      focus.bottom = focus.top + height;
    }
    /* DT_CALCRECT measures from the left edge whatever the alignment is. */
    if ((flags & DT_RIGHT) != 0)
      OffsetRect(&focus, text.right - focus.right, 0);
    else if ((flags & DT_CENTER) != 0)
      OffsetRect(&focus, (text.right - focus.right) / 2, 0);
    InflateRect(&focus, 1, 1);
    DrawFocusRect(hdc, &focus);
  }

  if (theme != NULL)
    darkTheme.close(theme);
  SetTextColor(hdc, oldColor);
  SetBkMode(hdc, oldBkMode);
  if (oldFont != NULL)
    SelectObject(hdc, oldFont);
}

static LRESULT CALLBACK darkButtonProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                       UINT_PTR id, DWORD_PTR)
{
  PAINTSTRUCT ps;

  switch (msg) {
  case WM_NCDESTROY:
    RemoveWindowSubclass(hwnd, darkButtonProc, id);
    break;
  case WM_ERASEBKGND:
    return 1;   /* the paint fills the whole client area */
  case WM_PAINT:
    /* Fall through on failure: returning validates nothing, so Windows reposts. */
    if (BeginPaint(hwnd, &ps) != NULL) {
      darkPaintButton(hwnd, ps.hdc);
      EndPaint(hwnd, &ps);
      return 0;
    }
    break;
  /* PrintWindow reaches a child by either message, depending on the flags. */
  case WM_PRINTCLIENT:
    darkPaintButton(hwnd, (HDC) wParam);
    return 0;
  case WM_PRINT:
    if ((lParam & PRF_CLIENT) != 0) {
      darkPaintButton(hwnd, (HDC) wParam);
      return 0;
    }
    break;
  }
  return DefSubclassProc(hwnd, msg, wParam, lParam);
}

/* Leaves the control a real check or radio button: the style bits, the auto-toggle and
   BM_GETCHECK all stay as they were. */
static void darkOwnerDrawButton(HWND hwnd)
{
  const DWORD style = (DWORD) GetWindowLong(hwnd, GWL_STYLE);
  const DWORD type = style & BS_TYPEMASK;

  /* A push-like, bitmap or icon button draws neither the glyph nor the label we would. */
  if ((style & (BS_PUSHLIKE | BS_BITMAP | BS_ICON)) != 0)
    return;
  if (type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE
      || type == BS_AUTO3STATE || darkIsRadio(style))
    SetWindowSubclass(hwnd, darkButtonProc, 0, 0);
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
  if (GetClassNameA(hwnd, name, sizeof(name)) == 0)
    return;
  /* A button needs no theme call: we paint it ourselves. */
  if (lstrcmpiA(name, "Button") == 0)
    darkOwnerDrawButton(hwnd);
  else if (setTheme == NULL)
    return;
  /* A theme name the system does not know falls back to the default one, so an older
     Windows just keeps its own look. The theme carries the control's scrollbars. */
  else if (lstrcmpiA(name, "SysTreeView32") == 0 || lstrcmpiA(name, "SysListView32") == 0
           || lstrcmpiA(name, "Edit") == 0)
    setTheme(hwnd, L"DarkMode_Explorer", NULL);
  /* A combo box paints its closed field from the theme and never asks for a brush. */
  else if (lstrcmpiA(name, "ComboBox") == 0)
    setTheme(hwnd, L"DarkMode_CFD", NULL);
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
  case CTLCOLOR_LISTBOX:   /* the list a combo box drops down */
    pDC->SetTextColor(DARK_TEXT);
    pDC->SetBkColor(DARK_EDIT_BG);
    return darkEditBrush;
  }
  return NULL;
}
