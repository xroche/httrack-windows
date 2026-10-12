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

/* Every colour above is grey, and a memory bitmap reads back as the mean of the three
   channels, so this turns one into what --selftest will measure. */
#define DARK_LUMA(c) ((GetRValue(c) + GetGValue(c) + GetBValue(c)) / 3)

/* A property sheet takes at most 100 pages, and that sheet owns the only tab control this
   paints, so the cap is above anything the app can build. A caption is far shorter. */
#define DARK_TAB_MAX   100
#define DARK_TAB_LABEL 48

/* The selected tab grows this many pixels past the others, at the classic control's own
   96 dpi scale. */
#define DARK_TAB_RAISE 2

/* The control's window runs past the page frame it draws, by the depth of the raised edge.
   No message answers that depth, because TCM_ADJUSTRECT answers a page area this sheet does
   not use. So these two come from measuring the light control. */
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

static BOOL darkIsTab(HWND hwnd)
{
  char name[32];

  return GetClassNameA(hwnd, name, sizeof(name)) != 0
    && lstrcmpiA(name, "SysTabControl32") == 0;
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

/* A memory bitmap pre-filled with the page colour, so --selftest can draw over it and read
   the pixels back. Top-down, so a row index is a y coordinate. Nothing reaches the screen,
   and dark mode may be off. */
typedef struct {
  HDC dc;
  HBITMAP bitmap;
  HGDIOBJ oldBitmap;
  BYTE *bits;
  int width;
  int height;
} dark_canvas_t;

static void darkCanvasClose(dark_canvas_t *canvas)
{
  if (canvas->oldBitmap != NULL)
    SelectObject(canvas->dc, canvas->oldBitmap);
  DeleteObject(canvas->bitmap);
  DeleteDC(canvas->dc);
  ZeroMemory(canvas, sizeof(*canvas));
}

static BOOL darkCanvasOpen(dark_canvas_t *canvas, int width, int height)
{
  BITMAPINFO info;
  HBRUSH page;
  RECT all;

  ZeroMemory(canvas, sizeof(*canvas));
  canvas->width = width;
  canvas->height = height;
  canvas->dc = CreateCompatibleDC(NULL);
  if (canvas->dc == NULL)
    return FALSE;
  ZeroMemory(&info, sizeof(info));
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  canvas->bitmap =
    CreateDIBSection(canvas->dc, &info, DIB_RGB_COLORS, (void **) &canvas->bits, NULL, 0);
  page = CreateSolidBrush(DARK_DLG_BG);
  if (canvas->bitmap == NULL || canvas->bits == NULL || page == NULL) {
    DeleteObject(page);
    darkCanvasClose(canvas);
    return FALSE;
  }
  canvas->oldBitmap = SelectObject(canvas->dc, canvas->bitmap);
  /* The page goes under the drawing, so one that drew nothing is told apart from one that
     drew the page's own colour. That test compares luma, so a non-grey pixel averaging to
     the page's would read as undrawn. Every colour here is grey. */
  SetRect(&all, 0, 0, width, height);
  FillRect(canvas->dc, &all, page);
  DeleteObject(page);
  return TRUE;
}

/* The mean of one pixel's three channels, 0 to 255, or -1 outside the bitmap. */
static int darkCanvasAt(const dark_canvas_t *canvas, int x, int y)
{
  const BYTE *pixel;

  if (x < 0 || y < 0 || x >= canvas->width || y >= canvas->height)
    return -1;
  pixel = canvas->bits + ((size_t) y * canvas->width + x) * 4;
  return (pixel[0] + pixel[1] + pixel[2]) / 3;
}

int WhttDarkGlyphLuminance(int glyphState, BOOL radio, int *changed)
{
  const int side = 13;
  dark_canvas_t canvas;
  RECT box;
  int total = 0, drawn = 0, x, y;

  if (changed != NULL)
    *changed = 0;
  /* The caller indexes the colour tables with the phase, so refuse a state they have no
     row for rather than reading behind them. */
  if (glyphState < 1 || glyphState > 12)
    return -1;
  if (!darkCanvasOpen(&canvas, side, side))
    return -1;
  SetRect(&box, 0, 0, side, side);
  darkDrawGlyph(canvas.dc, &box, radio, glyphState);
  GdiFlush();   /* the drawing reaches the bits only once the batch has run */
  for (y = 0; y < side; y++) {
    for (x = 0; x < side; x++) {
      const int luma = darkCanvasAt(&canvas, x, y);

      total += luma;
      if (luma != DARK_LUMA(DARK_DLG_BG))
        drawn++;
    }
  }
  darkCanvasClose(&canvas);
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

/* DT_CALCRECT measures `in` without the alignment DrawText applies when it actually draws,
   so this puts the measured box back where the drawn text lands. */
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

/* Passing these three brushes in lets --selftest draw a tab with dark mode off and the
   module brushes unmade. */
typedef struct {
  HBRUSH page;
  HBRUSH tab;
  HBRUSH frame;
} dark_tab_brushes_t;

typedef struct {
  RECT rect;
  const WCHAR *label;
} dark_tab_item_t;

/* Everything the painter below needs. Taking it as a value rather than reading it off an
   HWND is what lets --selftest drive that painter without a property sheet. */
typedef struct {
  RECT client;
  RECT body;                      /* the page frame, whose top line the tabs stand on */
  const dark_tab_item_t *items;
  int count;
  int current;                    /* the selected item, or -1 for none */
  BOOL focused;                   /* does the keyboard sit on the control? */
  const dark_tab_brushes_t *brush;
} dark_tab_layout_t;

/* A tab label is centred on one line. No catalog gives one an ampersand, checked across the
   .rc captions and every translation of LANG_IOPT1 to LANG_IOPT11, so the dialog's
   accelerator cue never applies here. */
#define DARK_TAB_TEXT (DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS)

/* item is one tab's rect in client coordinates, and bodyTop is the page's top line. The
   selected one grows past its neighbours and runs over that line, which is how Windows
   tells it apart. Reaching the line assumes the selected tab sits in the row next to the
   page, true only above one row. */
static void darkDrawTabItem(HDC hdc, const dark_tab_item_t *item, BOOL selected,
                            BOOL drawFocusRing, int bodyTop,
                            const dark_tab_brushes_t *brush)
{
  const int wide = MulDiv(DARK_TAB_RAISE, GetDeviceCaps(hdc, LOGPIXELSX), 96);
  const int tall = MulDiv(DARK_TAB_RAISE, GetDeviceCaps(hdc, LOGPIXELSY), 96);
  RECT box = item->rect, edge, text;

  if (selected) {
    InflateRect(&box, wide, 0);
    box.top -= tall;
    if (box.bottom < bodyTop + 1)
      box.bottom = bodyTop + 1;
  }
  FillRect(hdc, &box, selected ? brush->page : brush->tab);
  /* The three sides drawn skip the bottom, which belongs to the next row or to the page. */
  edge = box;
  edge.bottom = edge.top + 1;
  FillRect(hdc, &edge, brush->frame);
  edge = box;
  edge.right = edge.left + 1;
  FillRect(hdc, &edge, brush->frame);
  edge = box;
  edge.left = edge.right - 1;
  FillRect(hdc, &edge, brush->frame);

  text = item->rect;
  if (selected)
    text.top -= tall;   /* the label rises with the tab */
  SetTextColor(hdc, DARK_TEXT);
  DrawTextW(hdc, item->label, -1, &text, DARK_TAB_TEXT);
  if (drawFocusRing) {
    RECT ring;

    darkTextRect(hdc, item->label, DARK_TAB_TEXT, &text, &ring);
    InflateRect(&ring, 1, 1);
    DrawFocusRect(hdc, &ring);
  }
}

/* Paints a control's whole client area from a layout, touching no window. */
static void darkDrawTabs(HDC hdc, const dark_tab_layout_t *tabs)
{
  const int oldBkMode = SetBkMode(hdc, TRANSPARENT);
  const COLORREF oldColor = GetTextColor(hdc);
  int pass, i;

  FillRect(hdc, &tabs->client, tabs->brush->page);
  FrameRect(hdc, &tabs->body, tabs->brush->frame);
  /* The selected tab overlaps its neighbours and the page's top line, so it goes last. */
  for (pass = 0; pass < 2; pass++) {
    const BOOL drawSelectedNow = (pass == 1);

    for (i = 0; i < tabs->count; i++) {
      const BOOL selected = (i == tabs->current);

      if (selected == drawSelectedNow)
        darkDrawTabItem(hdc, &tabs->items[i], selected, selected && tabs->focused,
                        tabs->body.top, tabs->brush);
    }
  }
  SetTextColor(hdc, oldColor);
  SetBkMode(hdc, oldBkMode);
}

/* The first row from the top where a column stops showing the page's own colour. */
static int darkTopOfColumn(const dark_canvas_t *canvas, int x, int below)
{
  int y;

  for (y = 0; y < below; y++) {
    if (darkCanvasAt(canvas, x, y) != DARK_LUMA(DARK_DLG_BG))
      return y;
  }
  return below;
}

/* The brightest pixel one label left inside a tab's own rect, and the box its ink spans.
   The frame is the only other thing drawn there, and it never reaches the text colour. */
typedef struct {
  int brightest;
  int left;
  int right;
  int top;
  int bottom;
} dark_ink_t;

static void darkInkIn(const dark_canvas_t *canvas, const RECT *box, dark_ink_t *ink)
{
  int x, y;

  ink->brightest = 0;
  ink->left = box->right;
  ink->right = box->left;
  ink->top = box->bottom;
  ink->bottom = box->top;
  for (y = box->top; y < box->bottom; y++) {
    for (x = box->left; x < box->right; x++) {
      const int luma = darkCanvasAt(canvas, x, y);

      if (luma > DARK_LUMA(DARK_FRAME)) {
        if (luma > ink->brightest)
          ink->brightest = luma;
        if (x < ink->left)
          ink->left = x;
        if (x > ink->right)
          ink->right = x;
        if (y < ink->top)
          ink->top = y;
        if (y > ink->bottom)
          ink->bottom = y;
      }
    }
  }
}

BOOL WhttDarkTabProbe(whtt_dark_tab_probe_t *probe)
{
  /* Three tabs on one row with the middle one selected, the smallest layout that has a
     neighbour to compare against and a seam to look under. The same glyph in each, so one
     label's ink rows mean the same as another's. */
  static const WCHAR *const labels[3] = { L"X", L"X", L"X" };
  const int rowTop = 6, rowBottom = 26, bodyBottom = 44;
  dark_tab_item_t items[3];
  dark_tab_brushes_t brush;
  dark_tab_layout_t tabs;
  dark_canvas_t canvas;
  dark_ink_t ink;
  int selectedMid, otherMid, x, i;

  ZeroMemory(probe, sizeof(*probe));
  if (!darkCanvasOpen(&canvas, 180, 48))
    return FALSE;
  brush.page = CreateSolidBrush(DARK_DLG_BG);
  brush.tab = CreateSolidBrush(DARK_TAB_BG);
  brush.frame = CreateSolidBrush(DARK_FRAME);
  if (brush.page == NULL || brush.tab == NULL || brush.frame == NULL) {
    DeleteObject(brush.page);
    DeleteObject(brush.tab);
    DeleteObject(brush.frame);
    darkCanvasClose(&canvas);
    return FALSE;
  }
  for (i = 0; i < 3; i++) {
    SetRect(&items[i].rect, 4 + i * 58, rowTop, 4 + (i + 1) * 58, rowBottom);
    items[i].label = labels[i];
  }
  SetRect(&tabs.client, 0, 0, canvas.width, canvas.height);
  SetRect(&tabs.body, 0, rowBottom, canvas.width, bodyBottom);
  tabs.items = items;
  tabs.count = 3;
  tabs.current = 1;
  tabs.focused = FALSE;
  tabs.brush = &brush;
  darkDrawTabs(canvas.dc, &tabs);
  GdiFlush();   /* the drawing reaches the bits only once the batch has run */

  probe->itemLeft = items[1].rect.left;
  probe->itemRight = items[1].rect.right;
  probe->itemTop = items[1].rect.top;
  probe->itemBottom = items[1].rect.bottom;
  selectedMid = (items[1].rect.left + items[1].rect.right) / 2;
  otherMid = (items[0].rect.left + items[0].rect.right) / 2;
  probe->seamUnderSelected = darkCanvasAt(&canvas, selectedMid, rowBottom);
  probe->seamUnderOther = darkCanvasAt(&canvas, otherMid, rowBottom);
  /* A point well inside each tab and clear of its centred label. */
  probe->selectedFill = darkCanvasAt(&canvas, items[1].rect.left + 4, rowTop + 4);
  probe->unselectedFill = darkCanvasAt(&canvas, items[0].rect.left + 4, rowTop + 4);
  /* An unselected tab's own left edge, which is not the page frame the seams read. */
  probe->frameOnOther = darkCanvasAt(&canvas, items[0].rect.left, rowTop + 4);
  /* On the row above the layout row, the selected tab's edges are the only frame drawn, so
     how far past its own rect they sit is the raise's width. */
  probe->raiseLeft = canvas.width;
  probe->raiseRight = -1;
  for (x = 0; x < canvas.width; x++) {
    if (darkCanvasAt(&canvas, x, rowTop - 1) == DARK_LUMA(DARK_FRAME)) {
      if (x < probe->raiseLeft)
        probe->raiseLeft = x;
      probe->raiseRight = x;
    }
  }
  /* How far each tab reaches above the row separates a raise that happened from one that
     did not, which no whole-bitmap mean can see. */
  probe->selectedTop = darkTopOfColumn(&canvas, selectedMid, rowTop);
  probe->unselectedTop = darkTopOfColumn(&canvas, otherMid, rowTop);
  /* The label's own ink, and where it sits inside its tab. Its neighbour's rows come too,
     because the raise moves them and nothing else does. */
  darkInkIn(&canvas, &items[1].rect, &ink);
  probe->labelInk = ink.brightest;
  probe->labelLeft = ink.left;
  probe->labelRight = ink.right;
  probe->labelTop = ink.top;
  probe->labelBottom = ink.bottom;
  darkInkIn(&canvas, &items[0].rect, &ink);
  probe->otherLabelTop = ink.top;
  probe->otherLabelBottom = ink.bottom;
  DeleteObject(brush.page);
  DeleteObject(brush.tab);
  DeleteObject(brush.frame);
  darkCanvasClose(&canvas);
  return TRUE;
}

/* Paints the whole control, background included, because no DarkMode_* theme class covers
   SysTabControl32 and its own art is light whatever is asked of it. Every tab is drawn on
   the rect the control laid out for it, so nothing here moves one. */
static void darkPaintTab(HWND hwnd, HDC hdc)
{
  const dark_tab_brushes_t brush = { darkDlgBrush, darkTabBrush, darkFrameBrush };
  const HFONT font = (HFONT) SendMessage(hwnd, WM_GETFONT, 0, 0);
  const int count = (int) SendMessage(hwnd, TCM_GETITEMCOUNT, 0, 0);
  const int selected = (int) SendMessage(hwnd, TCM_GETCURSEL, 0, 0);
  dark_tab_item_t items[DARK_TAB_MAX];
  WCHAR labels[DARK_TAB_MAX][DARK_TAB_LABEL];
  dark_tab_layout_t tabs;
  HGDIOBJ oldFont = NULL;
  int i;

  ZeroMemory(&tabs, sizeof(tabs));
  tabs.items = items;
  tabs.brush = &brush;
  tabs.current = -1;
  /* The ring goes on the selected tab, and only while the control has the keyboard. */
  tabs.focused = GetFocus() == hwnd
    && (SendMessage(hwnd, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS) == 0;
  GetClientRect(hwnd, &tabs.client);

  tabs.body = tabs.client;
  tabs.body.right -= MulDiv(DARK_TAB_PANE_RIGHT, GetDeviceCaps(hdc, LOGPIXELSX), 96);
  tabs.body.bottom -= MulDiv(DARK_TAB_PANE_BOTTOM, GetDeviceCaps(hdc, LOGPIXELSY), 96);
  for (i = 0; i < count && tabs.count < DARK_TAB_MAX; i++) {
    dark_tab_item_t *const item = &items[tabs.count];
    TCITEMW info;

    if (!SendMessage(hwnd, TCM_GETITEMRECT, i, (LPARAM) &item->rect))
      continue;
    labels[tabs.count][0] = L'\0';
    ZeroMemory(&info, sizeof(info));
    info.mask = TCIF_TEXT;
    info.pszText = labels[tabs.count];
    info.cchTextMax = DARK_TAB_LABEL;
    SendMessage(hwnd, TCM_GETITEMW, i, (LPARAM) &info);
    item->label = labels[tabs.count];
    /* The page frame starts under the lowest row of tabs, and TCM_GETITEMRECT answers the
       layout rect, which the selected item's raised drawing is not part of. */
    if (item->rect.bottom > tabs.body.top)
      tabs.body.top = item->rect.bottom;
    if (i == selected)
      tabs.current = tabs.count;
    tabs.count++;
  }

  if (font != NULL)
    oldFont = SelectObject(hdc, font);
  darkDrawTabs(hdc, &tabs);
  if (oldFont != NULL)
    SelectObject(hdc, oldFont);
}

static void darkPaint(HWND hwnd, HDC hdc)
{
  const DWORD style = (DWORD) GetWindowLong(hwnd, GWL_STYLE);

  /* The class decides first: a tab control's TCS_ bits land where BS_TYPEMASK reads. */
  if (darkIsTab(hwnd))
    darkPaintTab(hwnd, hdc);
  else if (darkIsGroupBox(style))
    darkPaintGroupBox(hwnd, hdc, style);
  else
    darkPaintButton(hwnd, hdc, style);
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
    /* A group box draws its frame and its caption only, so it keeps its usual erase. */
    if (darkIsGroupBox((DWORD) GetWindowLong(hwnd, GWL_STYLE)) && !darkIsTab(hwnd))
      break;
    return 1;   /* the paint fills the whole client area */
  case WM_PAINT:
    /* Fall through on failure: returning validates nothing, so Windows reposts. */
    if (BeginPaint(hwnd, &ps) != NULL) {
      darkPaint(hwnd, ps.hdc);
      EndPaint(hwnd, &ps);
      return 0;
    }
    break;
  /* PrintWindow reaches a child by either message, depending on the flags. */
  case WM_PRINTCLIENT:
    darkPaint(hwnd, (HDC) wParam);
    return 0;
  case WM_PRINT:
    if ((lParam & PRF_CLIENT) != 0) {
      darkPaint(hwnd, (HDC) wParam);
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
  /* Nor does the tab control, which no theme class reaches. */
  else if (lstrcmpiA(name, "SysTabControl32") == 0)
    SetWindowSubclass(hwnd, darkButtonProc, 0, 0);
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
