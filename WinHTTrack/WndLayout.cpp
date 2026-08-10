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

#include "stdafx.h"
#include "WndLayout.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CWndLayout::CWndLayout()
{
  m_parent = NULL;
  m_base.cx = m_base.cy = 0;
}

void CWndLayout::Reset(CWnd* parent)
{
  m_items.RemoveAll();
  m_parent = NULL;
  m_base.cx = m_base.cy = 0;
  if (parent != NULL && parent->m_hWnd != NULL) {
    CRect client;
    parent->GetClientRect(client);
    m_parent = parent->m_hWnd;
    m_base = client.Size();
  }
}

void CWndLayout::Add(CWnd* ctrl, int moveX, int sizeX, int moveY, int sizeY)
{
  if (ctrl == NULL || ctrl->m_hWnd == NULL)
    return;
  CWnd* const parent = ctrl->GetParent();
  if (parent == NULL)
    return;

  ITEM item;
  item.hwnd = ctrl->m_hWnd;
  ctrl->GetWindowRect(item.base);
  parent->ScreenToClient(item.base);
  item.moveX = moveX;
  item.sizeX = sizeX;
  item.moveY = moveY;
  item.sizeY = sizeY;
  m_items.Add(item);
}

void CWndLayout::AddId(CWnd* parent, UINT id, int moveX, int sizeX, int moveY, int sizeY)
{
  if (parent == NULL || id == 0)
    return;
  Add(parent->GetDlgItem(id), moveX, sizeX, moveY, sizeY);
}

CRect CWndLayout::Target(const ITEM& item, int dx, int dy)
{
  const CPoint origin(item.base.left + dx * item.moveX / 100,
                      item.base.top  + dy * item.moveY / 100);
  return CRect(origin, CSize(item.base.Width()  + dx * item.sizeX / 100,
                             item.base.Height() + dy * item.sizeY / 100));
}

void CWndLayout::Apply(int cx, int cy) const
{
  const INT_PTR count = m_items.GetSize();
  if (count == 0 || m_base.cx == 0 || m_base.cy == 0)
    return;

  // Never shrink below the template: the form view scrolls instead, so the controls
  // stay whole rather than folding into each other.
  const int dx = max(cx - m_base.cx, 0);
  const int dy = max(cy - m_base.cy, 0);

  static const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS;

  HDWP defer = ::BeginDeferWindowPos((int) count);
  for(INT_PTR i=0 ; defer != NULL && i<count ; i++) {
    const ITEM& item = m_items[i];
    if (!::IsWindow(item.hwnd))
      continue;
    const CRect to = Target(item, dx, dy);
    // A failure destroys the HDWP and drops everything already deferred, so the pass
    // has to be redone the plain way rather than continued.
    defer = ::DeferWindowPos(defer, item.hwnd, NULL,
                             to.left, to.top, to.Width(), to.Height(), flags);
  }
  if (defer != NULL)
    ::EndDeferWindowPos(defer);
  else {
    for(INT_PTR i=0 ; i<count ; i++) {
      const ITEM& item = m_items[i];
      if (::IsWindow(item.hwnd)) {
        const CRect to = Target(item, dx, dy);
        ::SetWindowPos(item.hwnd, NULL, to.left, to.top, to.Width(), to.Height(), flags);
      }
    }
  }

  // Statics and group boxes leave their old pixels behind when they move.
  if (::IsWindow(m_parent))
    ::InvalidateRect(m_parent, NULL, TRUE);
}

CSheetLayout::CSheetLayout()
{
  m_sheet = NULL;
  m_baseClient.cx = m_baseClient.cy = 0;
}

static BOOL IsSheetPage(CPropertySheet* sheet, HWND hwnd)
{
  for(int i=0 ; i<sheet->GetPageCount() ; i++) {
    const CPropertyPage* const page = sheet->GetPage(i);
    if (page != NULL && page->m_hWnd == hwnd)
      return TRUE;
  }
  return FALSE;
}

void CSheetLayout::Build(CPropertySheet* sheet)
{
  m_sheet = NULL;
  m_pageBase.SetRectEmpty();
  m_baseClient.cx = m_baseClient.cy = 0;
  if (sheet == NULL || sheet->m_hWnd == NULL)
    return;
  m_sheet = sheet;
  m_layout.Reset(sheet);

  CRect client;
  sheet->GetClientRect(client);
  m_baseClient = client.Size();

  CTabCtrl* const tabs = sheet->GetTabControl();
  const HWND tabsHwnd = (tabs != NULL) ? tabs->m_hWnd : NULL;

  // Wizard mode hides the tab control but still gives it the page area, so it serves
  // as the reference before any page has a window.
  const int active = sheet->GetActiveIndex();
  CWnd* ref = (active >= 0 && active < sheet->GetPageCount()) ? (CWnd*) sheet->GetPage(active) : NULL;
  if (ref == NULL || ref->m_hWnd == NULL)
    ref = tabs;
  if (ref != NULL && ref->m_hWnd != NULL) {
    ref->GetWindowRect(m_pageBase);
    sheet->ScreenToClient(m_pageBase);
  }

  for(CWnd* child = sheet->GetWindow(GW_CHILD) ; child != NULL ; child = child->GetWindow(GW_HWNDNEXT)) {
    if (IsSheetPage(sheet, child->m_hWnd))
      continue;                      // LayoutActivePage owns these
    if (child->m_hWnd == tabsHwnd) {
      m_layout.Add(child, 0, 100, 0, 100);   // frames the pages, so it grows with them
      continue;
    }
    switch(child->GetDlgCtrlID()) {
      case ID_WIZBACK: case ID_WIZNEXT: case ID_WIZFINISH:
      case IDOK: case IDCANCEL: case IDHELP: case ID_APPLY_NOW:
        m_layout.Add(child, 100, 0, 100, 0);   // keep the strip in the bottom right
        break;
      default: {
        TCHAR cls[16];
        // The rule above the buttons is the only other static a sheet owns.
        if (::GetClassName(child->m_hWnd, cls, (int)_countof(cls)) > 0
            && _tcsicmp(cls, _T("Static")) == 0)
          m_layout.Add(child, 0, 100, 100, 0);
        break;
      }
    }
  }
}

void CSheetLayout::LayoutActivePage()
{
  if (m_sheet == NULL || m_pageBase.IsRectEmpty() || m_baseClient.cx == 0)
    return;
  /* Not GetActivePage(): FinalInProgress empties the page array between RemovePage and
     AddPage, and it then indexes -1 and hands back a wild pointer. */
  const int active = m_sheet->GetActiveIndex();
  if (active < 0 || active >= m_sheet->GetPageCount())
    return;
  CPropertyPage* const page = m_sheet->GetPage(active);
  if (page == NULL || page->m_hWnd == NULL)
    return;
  CRect client;
  m_sheet->GetClientRect(client);
  const int dx = max(client.Width() - m_baseClient.cx, 0);
  const int dy = max(client.Height() - m_baseClient.cy, 0);
  page->SetWindowPos(NULL, m_pageBase.left, m_pageBase.top,
                     m_pageBase.Width() + dx, m_pageBase.Height() + dy,
                     SWP_NOZORDER|SWP_NOACTIVATE);
}

void CSheetLayout::Apply(int cx, int cy)
{
  m_layout.Apply(cx, cy);
  LayoutActivePage();
}
