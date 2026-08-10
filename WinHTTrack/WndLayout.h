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
/*       Percentage anchoring for fixed dialog templates        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef WINHTTRACK_WNDLAYOUT_H
#define WINHTTRACK_WNDLAYOUT_H

#include <afxtempl.h>   /* CArray; StdAfx.h brings in the rest of MFC */
#include <afxdlgs.h>    /* CPropertySheet */

/* Lets a dialog laid out from a fixed template follow its parent's size.
   Each control keeps the rect the template gave it and then takes a percentage of
   however much the window has grown: "move" shifts its origin, "size" stretches it.
   Two controls sharing the growth 50/50 use move 0/size 50 and move 50/size 50.

   Record the controls once the dialog exists (OnInitDialog), then call Apply from
   OnSize. Positions are always recomputed from the recorded template rects, so
   repeated resizes cannot accumulate rounding drift. */
class CWndLayout
{
public:
  CWndLayout();

  /* Forget every control and take the parent's current client size as the baseline.
     Call before the first Add. */
  void Reset(CWnd* parent);

  /* Anchor one control. Percentages are of the growth in that axis; 0 pins the
     control to the template. A control that is missing, or a zero ID, is ignored so
     a caller can loop over an optional row.
     AddId is deliberately not an overload of Add: the IDC_* macros are plain ints, so
     Add(this, IDC_x, 0, 100) would bind to Add(CWnd*, int, int, int) and silently
     anchor the dialog itself with a four-digit percentage. */
  void Add(CWnd* ctrl, int moveX, int sizeX, int moveY = 0, int sizeY = 0);
  void AddId(CWnd* parent, UINT id, int moveX, int sizeX, int moveY = 0, int sizeY = 0);

  /* Reposition everything for a client area of cx by cy. Cheap enough for WM_SIZE:
     one DeferWindowPos pass, no redraw of untouched controls. */
  void Apply(int cx, int cy) const;

  BOOL IsEmpty() const { return m_items.GetSize() == 0; }

private:
  struct ITEM {
    HWND hwnd;
    CRect base;                  /* template rect, in parent client coordinates */
    int moveX, sizeX, moveY, sizeY;
  };
  /* Where one item lands once the window has grown by (dx,dy). */
  static CRect Target(const ITEM& item, int dx, int dy);

  CArray<ITEM,ITEM&> m_items;
  HWND m_parent;
  CSize m_base;                  /* client size the rects above were recorded at */
};

/* The sheet-level half of the same idea: the furniture a property sheet owns (its
   buttons, the wizard rule, the tab control) follows the sheet, and the active page is
   stretched to match. Each page still anchors its own controls; this only gives them
   the room. */
class CSheetLayout
{
public:
  CSheetLayout();

  /* Record template geometry. Run once the sheet is built and its active page has a
     window, so the page's own rect is measured instead of the tab control standing in
     for it. */
  void Build(CPropertySheet* sheet);

  /* Furniture and active page, for a client area of cx by cy. */
  void Apply(int cx, int cy);

  /* Give the page comctl32 has just shown the sheet's current size: it restores the rect
     the page had at creation, so this has to run after every page change. */
  void LayoutActivePage();

  /* Re-lay the active page out if this message may have swapped it. Call from the
     sheet's WindowProc, once the base class has run. */
  void HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

private:
  /* A page comctl32 creates on demand comes up at the rect it cached before the sheet
     grew, so every way of reaching a new page has to be caught. */
  static BOOL IsPageChange(UINT message, WPARAM wParam, LPARAM lParam);

  CPropertySheet* m_sheet;
  CWndLayout m_layout;
  CRect m_pageBase;              /* page rect, and the client size it was measured at */
  CSize m_baseClient;
};

#endif
