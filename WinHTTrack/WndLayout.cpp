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

void CWndLayout::Add(CWnd* parent, UINT id, int moveX, int sizeX, int moveY, int sizeY)
{
  if (parent == NULL || id == 0)
    return;
  Add(parent->GetDlgItem(id), moveX, sizeX, moveY, sizeY);
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

  HDWP defer = ::BeginDeferWindowPos((int) count);
  for(INT_PTR i=0 ; i<count ; i++) {
    const ITEM& item = m_items[i];
    if (!::IsWindow(item.hwnd))
      continue;
    const int x = item.base.left + dx * item.moveX / 100;
    const int y = item.base.top + dy * item.moveY / 100;
    const int w = item.base.Width() + dx * item.sizeX / 100;
    const int h = item.base.Height() + dy * item.sizeY / 100;
    const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS;
    if (defer != NULL)
      defer = ::DeferWindowPos(defer, item.hwnd, NULL, x, y, w, h, flags);
    else
      ::SetWindowPos(item.hwnd, NULL, x, y, w, h, flags);
  }
  if (defer != NULL)
    ::EndDeferWindowPos(defer);

  // Statics and group boxes leave their old pixels behind when they move.
  if (::IsWindow(m_parent))
    ::InvalidateRect(m_parent, NULL, TRUE);
}
