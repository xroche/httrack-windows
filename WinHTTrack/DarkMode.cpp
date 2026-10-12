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
#define DARK_FRAME    RGB(90, 90, 90)
#define DARK_TEXT     RGB(240, 240, 240)
#define DARK_TEXT_OFF RGB(140, 140, 140)

/* Between the glyph and its label, at 96 dpi. */
#define DARK_GLYPH_GAP 3

/* From the left edge of a group box to its caption, and the gap each side of the caption
   where the frame stops, both what Windows uses at 96 dpi. */
#define DARK_GROUP_INDENT 9
#define DARK_GROUP_PAD    2

/* An unselected tab sits behind the page. The selected one takes the page's own colour
   and merges into it. */
#define DARK_TAB_BG RGB(20, 20, 20)

/* How far the selected tab grows past the others, at 96 dpi, which is what the classic
   control draws. */
#define DARK_TAB_RAISE 2

/* The control's window runs past the page frame it draws, by the depth of the raised edge.
   No message answers that depth: TCM_ADJUSTRECT answers a page area the sheet does not use,
   so these two come from measuring the light control. */
#define DARK_TAB_PANE_RIGHT  2
#define DARK_TAB_PANE_BOTTOM 1

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
static HBRUSH darkFrameBrush = NULL;
static HBRUSH darkTabBrush = NULL;

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
  darkFrameBrush = CreateSolidBrush(DARK_FRAME);
  darkTabBrush = CreateSolidBrush(DARK_TAB_BG);
  /* One brush short would leave half the window unpainted, so give up on all of it. */
  if (darkDlgBrush == NULL || darkEditBrush == NULL || darkFrameBrush == NULL
      || darkTabBrush == NULL) {
    DeleteObject(darkDlgBrush);
    DeleteObject(darkEditBrush);
    DeleteObject(darkFrameBrush);
    DeleteObject(darkTabBrush);
    darkDlgBrush = darkEditBrush = darkFrameBrush = darkTabBrush = NULL;
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

static BOOL darkIsRadio(DWORD style)
{
  const DWORD type = style & BS_TYPEMASK;

  return type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON;
}

static BOOL darkIsGroupBox(DWORD style)
{
  return (style & BS_TYPEMASK) == BS_GROUPBOX;
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

/* The box itself, one colour per phase: normal, hot, pressed, disabled. */
static const COLORREF darkGlyphBorder[4] = {
  RGB(140, 140, 140), RGB(190, 190, 190), RGB(110, 110, 110), RGB(85, 85, 85)
};
static const COLORREF darkGlyphFill[4] = {
  RGB(45, 45, 45), RGB(58, 58, 58), RGB(50, 50, 50), RGB(40, 40, 40)
};

/* Draws the check or radio glyph. The themed and the classic BUTTON art are both drawn
   for a light page, and the dark art needs the undocumented opt-in. */
static void darkDrawGlyph(HDC hdc, const RECT *box, BOOL radio, int glyphState)
{
  /* The vsstyle ids run in fours, so the glyph and its phase divide out. */
  const int kind = (glyphState - 1) / 4;
  const int phase = (glyphState - 1) % 4;
  const COLORREF mark = (phase == 3) ? DARK_TEXT_OFF : DARK_TEXT;
  const int width = box->right - box->left;
  const int height = box->bottom - box->top;
  const HPEN pen = CreatePen(PS_SOLID, 1, darkGlyphBorder[phase]);
  const HBRUSH fill = CreateSolidBrush(darkGlyphFill[phase]);
  HGDIOBJ oldPen, oldBrush;

  if (pen == NULL || fill == NULL) {
    DeleteObject(pen);
    DeleteObject(fill);
    return;
  }
  oldPen = SelectObject(hdc, pen);
  oldBrush = SelectObject(hdc, fill);
  if (radio)
    Ellipse(hdc, box->left, box->top, box->right, box->bottom);
  else
    Rectangle(hdc, box->left, box->top, box->right, box->bottom);
  SelectObject(hdc, oldBrush);
  SelectObject(hdc, oldPen);
  DeleteObject(pen);
  DeleteObject(fill);

  if (kind == 0)   /* unchecked, and the empty box is the whole glyph */
    return;
  if (radio) {
    const int inset = (width + 2) / 4;
    const HPEN dotPen = CreatePen(PS_SOLID, 1, mark);
    const HBRUSH dot = CreateSolidBrush(mark);

    if (dotPen != NULL && dot != NULL) {
      oldPen = SelectObject(hdc, dotPen);
      oldBrush = SelectObject(hdc, dot);
      Ellipse(hdc, box->left + inset, box->top + inset, box->right - inset,
              box->bottom - inset);
      SelectObject(hdc, oldBrush);
      SelectObject(hdc, oldPen);
    }
    DeleteObject(dotPen);
    DeleteObject(dot);
  } else if (kind == 2) {   /* mixed, which the check box alone reports */
    const HBRUSH square = CreateSolidBrush(mark);
    RECT inner = *box;

    if (square != NULL) {
      InflateRect(&inner, -(width * 3 / 8), -(height * 3 / 8));
      FillRect(hdc, &inner, square);
      DeleteObject(square);
    }
  } else {
    const HPEN tick = CreatePen(PS_SOLID, (width >= 12) ? 2 : 1, mark);
    POINT stroke[3];

    if (tick != NULL) {
      stroke[0].x = box->left + width * 22 / 100;
      stroke[0].y = box->top + height * 50 / 100;
      stroke[1].x = box->left + width * 42 / 100;
      stroke[1].y = box->top + height * 72 / 100;
      stroke[2].x = box->left + width * 78 / 100;
      stroke[2].y = box->top + height * 26 / 100;
      oldPen = SelectObject(hdc, tick);
      Polyline(hdc, stroke, 3);
      SelectObject(hdc, oldPen);
      DeleteObject(tick);
    }
  }
}

int WhttDarkGlyphLuminance(int glyphState, BOOL radio, int *changed)
{
  const int side = 13;
  const HDC dc = CreateCompatibleDC(NULL);
  HGDIOBJ oldBitmap;
  HBITMAP bitmap;
  BITMAPINFO info;
  BYTE *bits = NULL;
  HBRUSH page;
  RECT box;
  int total = 0, drawn = 0;

  if (changed != NULL)
    *changed = 0;
  /* The caller indexes the colour tables with the phase, so refuse a state they have no
     row for rather than reading behind them. */
  if (glyphState < 1 || glyphState > 12 || dc == NULL)
    return -1;
  ZeroMemory(&info, sizeof(info));
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = side;
  info.bmiHeader.biHeight = side;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **) &bits, NULL, 0);
  if (bitmap == NULL || bits == NULL) {
    DeleteObject(bitmap);
    DeleteDC(dc);
    return -1;
  }
  page = CreateSolidBrush(DARK_DLG_BG);
  if (page == NULL) {
    DeleteObject(bitmap);
    DeleteDC(dc);
    return -1;
  }
  oldBitmap = SelectObject(dc, bitmap);
  SetRect(&box, 0, 0, side, side);
  /* The page goes under the glyph, so one that drew nothing is told apart from one that
     drew the page's own colour. */
  FillRect(dc, &box, page);
  DeleteObject(page);
  darkDrawGlyph(dc, &box, radio, glyphState);
  GdiFlush();   /* the drawing reaches the bits only once the batch has run */
  for (int i = 0; i < side * side; i++) {
    const COLORREF pixel = RGB(bits[i * 4 + 2], bits[i * 4 + 1], bits[i * 4]);

    total += (bits[i * 4] + bits[i * 4 + 1] + bits[i * 4 + 2]) / 3;
    if (pixel != DARK_DLG_BG)
      drawn++;
  }
  SelectObject(dc, oldBitmap);
  DeleteObject(bitmap);
  DeleteDC(dc);
  if (changed != NULL)
    *changed = drawn;
  return total / (side * side);
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

/* The box DrawText inks inside in, which DT_CALCRECT measures without the alignment
   DrawText then applies for us. */
static void darkTextRect(HDC hdc, const WCHAR *text, UINT flags, const RECT *in, RECT *out)
{
  *out = *in;
  DrawTextW(hdc, text, -1, out, flags | DT_CALCRECT);
  /* DT_CALCRECT ignores DT_VCENTER, so put a single line back where the text is. */
  if ((flags & DT_SINGLELINE) != 0) {
    const int height = out->bottom - out->top;

    out->top = in->top + (in->bottom - in->top - height) / 2;
    out->bottom = out->top + height;
  }
  /* And it measures from the left edge whatever the alignment is. */
  if ((flags & DT_RIGHT) != 0)
    OffsetRect(out, in->right - out->right, 0);
  else if ((flags & DT_CENTER) != 0)
    OffsetRect(out, (in->right - out->right) / 2, 0);
}

/* Paints the frame and the caption of a group box, which is a Button too and would
   otherwise draw a caption in the system's black. The interior is left untouched: the
   controls standing in a group box are its siblings, not its children. */
static void darkPaintGroupBox(HWND hwnd, HDC hdc, DWORD style)
{
  const UINT ui = (UINT) SendMessage(hwnd, WM_QUERYUISTATE, 0, 0);
  /* A group box caption never wraps, whatever the style asks for. */
  const UINT flags =
    (darkTextFlags(style, ui) & ~(DT_WORDBREAK | DT_TOP | DT_CENTER | DT_RIGHT))
    | DT_SINGLELINE | DT_VCENTER;
  const HFONT font = (HFONT) SendMessage(hwnd, WM_GETFONT, 0, 0);
  const int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
  const int indent = MulDiv(DARK_GROUP_INDENT, dpi, 96);
  const int pad = MulDiv(DARK_GROUP_PAD, dpi, 96);
  HGDIOBJ oldFont = NULL;
  TEXTMETRICW metrics;
  RECT client, frame, text;
  WCHAR caption[512];

  GetClientRect(hwnd, &client);
  caption[0] = L'\0';
  GetWindowTextW(hwnd, caption, sizeof(caption) / sizeof(caption[0]));
  if (font != NULL)
    oldFont = SelectObject(hdc, font);
  if (!GetTextMetricsW(hdc, &metrics))
    metrics.tmHeight = MulDiv(13, GetDeviceCaps(hdc, LOGPIXELSY), 96);

  /* The caption sits on the top line, so the frame starts half a line down, captioned
     or not. Windows etches that line the same whether the control is enabled or not. */
  frame = client;
  frame.top += metrics.tmHeight / 2;
  FrameRect(hdc, &frame, darkFrameBrush);

  if (caption[0] != L'\0') {
    COLORREF oldColor;
    int oldBkMode, width;
    RECT measured;

    text = client;
    text.left += indent;
    text.right -= indent;
    text.bottom = text.top + metrics.tmHeight;
    measured = text;
    DrawTextW(hdc, caption, -1, &measured, flags | DT_CALCRECT);
    width = measured.right - measured.left;
    if (width > text.right - text.left)
      width = text.right - text.left;
    /* BS_CENTER and BS_RIGHT move the caption along the top line. */
    if ((style & BS_CENTER) == BS_CENTER)
      text.left = client.left + (client.right - client.left - width) / 2;
    else if ((style & BS_RIGHT) != 0)
      text.left = text.right - width;
    text.right = text.left + width;

    measured = text;
    InflateRect(&measured, pad, 0);
    FillRect(hdc, &measured, darkDlgBrush);   /* the frame must not run behind the text */
    oldBkMode = SetBkMode(hdc, TRANSPARENT);
    oldColor = SetTextColor(hdc, ((style & WS_DISABLED) != 0) ? DARK_TEXT_OFF : DARK_TEXT);
    DrawTextW(hdc, caption, -1, &text, flags);
    SetTextColor(hdc, oldColor);
    SetBkMode(hdc, oldBkMode);
  }
  if (oldFont != NULL)
    SelectObject(hdc, oldFont);
}

/* Paints the whole control, because a themed check or radio button draws its own label
   and ignores WM_CTLCOLORBTN, so the text would stay black on the dark page. */
static void darkPaintButton(HWND hwnd, HDC hdc, DWORD style)
{
  const UINT ui = (UINT) SendMessage(hwnd, WM_QUERYUISTATE, 0, 0);
  const UINT state = (UINT) SendMessage(hwnd, BM_GETSTATE, 0, 0);
  const UINT flags = darkTextFlags(style, ui);
  const BOOL radio = darkIsRadio(style);
  const int glyphState = WhttDarkGlyphState(style, state);
  const HFONT font = (HFONT) SendMessage(hwnd, WM_GETFONT, 0, 0);
  const int gap = MulDiv(DARK_GLYPH_GAP, GetDeviceCaps(hdc, LOGPIXELSX), 96);
  /* 13 by 13 at 96 dpi is what Windows has drawn since it had themes. */
  const int glyph = MulDiv(13, GetDeviceCaps(hdc, LOGPIXELSX), 96);
  HGDIOBJ oldFont = NULL;
  COLORREF oldColor;
  int oldBkMode;
  RECT client, box, text;
  WCHAR caption[512];

  GetClientRect(hwnd, &client);
  FillRect(hdc, &client, darkDlgBrush);
  caption[0] = L'\0';
  GetWindowTextW(hwnd, caption, sizeof(caption) / sizeof(caption[0]));
  if (font != NULL)
    oldFont = SelectObject(hdc, font);
  box.top = client.top + (client.bottom - client.top - glyph) / 2;
  box.bottom = box.top + glyph;
  text.top = client.top;
  text.bottom = client.bottom;
  /* BS_LEFTTEXT and BS_RIGHTBUTTON are the same bit: the glyph moves to the far end. */
  if ((style & BS_RIGHTBUTTON) != 0) {
    box.right = client.right;
    box.left = box.right - glyph;
    text.left = client.left;
    text.right = box.left - gap;
  } else {
    box.left = client.left;
    box.right = box.left + glyph;
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

  darkDrawGlyph(hdc, &box, radio, glyphState);

  /* The DC belongs to the caller under WM_PRINT, and to the parent under CS_PARENTDC. */
  oldBkMode = SetBkMode(hdc, TRANSPARENT);
  oldColor = SetTextColor(hdc, ((style & WS_DISABLED) != 0) ? DARK_TEXT_OFF : DARK_TEXT);
  DrawTextW(hdc, caption, -1, &text, flags);
  if ((state & BST_FOCUS) != 0 && (ui & UISF_HIDEFOCUS) == 0) {
    RECT focus;

    darkTextRect(hdc, caption, flags, &text, &focus);
    InflateRect(&focus, 1, 1);
    DrawFocusRect(hdc, &focus);
  }

  SetTextColor(hdc, oldColor);
  SetBkMode(hdc, oldBkMode);
  if (oldFont != NULL)
    SelectObject(hdc, oldFont);
}

/* The three brushes a tab item is painted with, passed in so --selftest can draw one with
   dark mode off and the module brushes unmade. */
typedef struct {
  HBRUSH page;
  HBRUSH tab;
  HBRUSH frame;
} dark_tab_brushes_t;

/* One tab item, in client coordinates, where bodyTop is the line along the top of the
   page. The selected one grows past its neighbours and runs over that line, which is how
   Windows tells it apart. Reaching the line assumes the control put the selected tab in
   the row next to the page, which it does whenever there is more than one row. */
static void darkDrawTabItem(HDC hdc, const RECT *item, const WCHAR *label, UINT flags,
                            const dark_tab_brushes_t *brush, int bodyTop, BOOL selected,
                            BOOL focus)
{
  const int wide = MulDiv(DARK_TAB_RAISE, GetDeviceCaps(hdc, LOGPIXELSX), 96);
  const int tall = MulDiv(DARK_TAB_RAISE, GetDeviceCaps(hdc, LOGPIXELSY), 96);
  RECT box = *item, edge, text;

  if (selected) {
    InflateRect(&box, wide, 0);
    box.top -= tall;
    if (box.bottom < bodyTop + 1)
      box.bottom = bodyTop + 1;
  }
  FillRect(hdc, &box, selected ? brush->page : brush->tab);
  /* Three sides, because the line under a tab is the next row's top edge, or the page's. */
  edge = box;
  edge.bottom = edge.top + 1;
  FillRect(hdc, &edge, brush->frame);
  edge = box;
  edge.right = edge.left + 1;
  FillRect(hdc, &edge, brush->frame);
  edge = box;
  edge.left = edge.right - 1;
  FillRect(hdc, &edge, brush->frame);

  text = *item;
  if (selected)
    text.top -= tall;   /* the label rises with the tab */
  SetTextColor(hdc, DARK_TEXT);
  DrawTextW(hdc, label, -1, &text, flags);
  if (focus) {
    RECT ring;

    darkTextRect(hdc, label, flags, &text, &ring);
    InflateRect(&ring, 1, 1);
    DrawFocusRect(hdc, &ring);
  }
}

int WhttDarkTabLuminance(const WCHAR *label, BOOL selected, int *changed)
{
  const int width = 72, height = 24, bodyTop = 20;
  const HDC dc = CreateCompatibleDC(NULL);
  dark_tab_brushes_t brush;
  HGDIOBJ oldBitmap;
  HBITMAP bitmap;
  BITMAPINFO info;
  BYTE *bits = NULL;
  RECT all, item;
  int total = 0, drawn = 0;

  if (changed != NULL)
    *changed = 0;
  if (label == NULL || dc == NULL) {
    DeleteDC(dc);
    return -1;
  }
  ZeroMemory(&info, sizeof(info));
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **) &bits, NULL, 0);
  brush.page = CreateSolidBrush(DARK_DLG_BG);
  brush.tab = CreateSolidBrush(DARK_TAB_BG);
  brush.frame = CreateSolidBrush(DARK_FRAME);
  if (bitmap == NULL || bits == NULL || brush.page == NULL || brush.tab == NULL
      || brush.frame == NULL) {
    DeleteObject(bitmap);
    DeleteObject(brush.page);
    DeleteObject(brush.tab);
    DeleteObject(brush.frame);
    DeleteDC(dc);
    return -1;
  }
  oldBitmap = SelectObject(dc, bitmap);
  SetRect(&all, 0, 0, width, height);
  /* The control's background goes under the item, so one that drew nothing is told apart
     from one that drew the page's own colour. */
  FillRect(dc, &all, brush.page);
  SetBkMode(dc, TRANSPARENT);
  SetRect(&item, 4, 4, width - 4, bodyTop);
  darkDrawTabItem(dc, &item, label,
                  DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS, &brush, bodyTop,
                  selected, FALSE);
  GdiFlush();   /* the drawing reaches the bits only once the batch has run */
  for (int i = 0; i < width * height; i++) {
    const COLORREF pixel = RGB(bits[i * 4 + 2], bits[i * 4 + 1], bits[i * 4]);

    total += (bits[i * 4] + bits[i * 4 + 1] + bits[i * 4 + 2]) / 3;
    if (pixel != DARK_DLG_BG)
      drawn++;
  }
  SelectObject(dc, oldBitmap);
  DeleteObject(bitmap);
  DeleteObject(brush.page);
  DeleteObject(brush.tab);
  DeleteObject(brush.frame);
  DeleteDC(dc);
  if (changed != NULL)
    *changed = drawn;
  return total / (width * height);
}

/* Paints the whole control, background included, because no DarkMode_* theme class covers
   SysTabControl32 and its own art is light whatever is asked of it. Every tab is drawn on
   the rect the control laid out for it, so nothing here moves one. */
static void darkPaintTab(HWND hwnd, HDC hdc)
{
  const dark_tab_brushes_t brush = { darkDlgBrush, darkTabBrush, darkFrameBrush };
  const int count = (int) SendMessage(hwnd, TCM_GETITEMCOUNT, 0, 0);
  const int current = (int) SendMessage(hwnd, TCM_GETCURSEL, 0, 0);
  const UINT ui = (UINT) SendMessage(hwnd, WM_QUERYUISTATE, 0, 0);
  const HFONT font = (HFONT) SendMessage(hwnd, WM_GETFONT, 0, 0);
  /* The ring goes on the selected tab, and only while the control has the keyboard. */
  const BOOL focused = GetFocus() == hwnd && (ui & UISF_HIDEFOCUS) == 0;
  UINT flags = DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS;
  HGDIOBJ oldFont = NULL;
  COLORREF oldColor;
  RECT client, body;
  int oldBkMode, pass, i;

  if ((ui & UISF_HIDEACCEL) != 0)
    flags |= DT_HIDEPREFIX;
  GetClientRect(hwnd, &client);
  FillRect(hdc, &client, brush.page);
  if (font != NULL)
    oldFont = SelectObject(hdc, font);
  oldBkMode = SetBkMode(hdc, TRANSPARENT);
  oldColor = GetTextColor(hdc);

  /* The page frame starts under the lowest row of tabs, and TCM_GETITEMRECT answers the
     layout rect, which the selected item's raised drawing is not part of. */
  body = client;
  body.right -= MulDiv(DARK_TAB_PANE_RIGHT, GetDeviceCaps(hdc, LOGPIXELSX), 96);
  body.bottom -= MulDiv(DARK_TAB_PANE_BOTTOM, GetDeviceCaps(hdc, LOGPIXELSY), 96);
  for (i = 0; i < count; i++) {
    RECT item;

    if (SendMessage(hwnd, TCM_GETITEMRECT, i, (LPARAM) &item) && item.bottom > body.top)
      body.top = item.bottom;
  }
  FrameRect(hdc, &body, brush.frame);

  /* The selected tab overlaps its neighbours and the page's top line, so it goes last. */
  for (pass = 0; pass < 2; pass++) {
    for (i = 0; i < count; i++) {
      const BOOL isCurrent = (i == current);
      TCITEMW info;
      RECT item;
      WCHAR label[256];

      if (isCurrent != (pass == 1)
          || !SendMessage(hwnd, TCM_GETITEMRECT, i, (LPARAM) &item))
        continue;
      label[0] = L'\0';
      ZeroMemory(&info, sizeof(info));
      info.mask = TCIF_TEXT;
      info.pszText = label;
      info.cchTextMax = sizeof(label) / sizeof(label[0]);
      SendMessage(hwnd, TCM_GETITEMW, i, (LPARAM) &info);
      darkDrawTabItem(hdc, &item, label, flags, &brush, body.top, isCurrent,
                      isCurrent && focused);
    }
  }

  SetTextColor(hdc, oldColor);
  SetBkMode(hdc, oldBkMode);
  if (oldFont != NULL)
    SelectObject(hdc, oldFont);
}

static void darkPaint(HWND hwnd, HDC hdc)
{
  const DWORD style = (DWORD) GetWindowLong(hwnd, GWL_STYLE);

  if (darkIsGroupBox(style))
    darkPaintGroupBox(hwnd, hdc, style);
  else
    darkPaintButton(hwnd, hdc, style);
}

typedef void (*dark_painter_t)(HWND, HDC);

/* Takes over a control the common controls would draw light. The subclass id says whether
   the painter fills the whole client area, and the reference data is the painter. */
static LRESULT CALLBACK darkPaintProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                      UINT_PTR fills, DWORD_PTR data)
{
  const dark_painter_t paint = (dark_painter_t) data;
  PAINTSTRUCT ps;

  switch (msg) {
  case WM_NCDESTROY:
    RemoveWindowSubclass(hwnd, darkPaintProc, fills);
    break;
  case WM_ERASEBKGND:
    if (fills == 0)
      break;
    return 1;
  case WM_PAINT:
    /* Fall through on failure: returning validates nothing, so Windows reposts. */
    if (BeginPaint(hwnd, &ps) != NULL) {
      paint(hwnd, ps.hdc);
      EndPaint(hwnd, &ps);
      return 0;
    }
    break;
  /* PrintWindow reaches a child by either message, depending on the flags. */
  case WM_PRINTCLIENT:
    paint(hwnd, (HDC) wParam);
    return 0;
  case WM_PRINT:
    if ((lParam & PRF_CLIENT) != 0) {
      paint(hwnd, (HDC) wParam);
      return 0;
    }
    break;
  }
  return DefSubclassProc(hwnd, msg, wParam, lParam);
}

/* Leaves the control exactly what it was: the style bits, the auto-toggle and
   BM_GETCHECK all stay as they were. */
static void darkOwnerDrawButton(HWND hwnd)
{
  const DWORD style = (DWORD) GetWindowLong(hwnd, GWL_STYLE);
  const DWORD type = style & BS_TYPEMASK;

  /* A push-like, bitmap or icon button draws neither the glyph nor the label we would. */
  if ((style & (BS_PUSHLIKE | BS_BITMAP | BS_ICON)) != 0)
    return;
  if (type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE
      || type == BS_AUTO3STATE || type == BS_GROUPBOX || darkIsRadio(style))
    /* A group box paints its frame and its caption only, so it keeps its usual erase. */
    SetWindowSubclass(hwnd, darkPaintProc, !darkIsGroupBox(style), (DWORD_PTR) darkPaint);
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
  /* Nor does the tab control, which no theme class reaches. */
  else if (lstrcmpiA(name, "SysTabControl32") == 0)
    SetWindowSubclass(hwnd, darkPaintProc, 1, (DWORD_PTR) darkPaintTab);
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
