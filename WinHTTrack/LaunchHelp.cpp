/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998 Xavier Roche and other contributors

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
// LaunchHelp.cpp : implementation file
//

#include "stdafx.h"

/* Must follow the PCH include: anything before it is discarded, so defining
   this above would silently do nothing. */
#define VIEW_HELP 0
#include "LaunchHelp.h"
#include "DialogHtmlHelp.h"
#include "process.h"

#if VIEW_HELP
#include "htmlfrm.h"
#endif

LaunchHelp::LaunchHelp() {
  page="";
}
LaunchHelp::~LaunchHelp() {
  if (b.m_hWnd) {
    b.EndDialog(IDCANCEL);
  }
}

void LaunchHelp::Help(CString page) {
#if VIEW_HELP
#else
  if (!b)
    this->page=page;
  else {
    if (b.m_hWnd)
      this->b.Go(page);
    else
      this->page=page;
  }
#endif
  GoHelp();
}

void LaunchHelp::Help() {
  Help("index.html");
}
  
void LaunchHelp::GoHelp() {
#if VIEW_HELP
  CHtmlFrame* frm=new CHtmlFrame;
	if (!frm->LoadFrame(IDR_HELPFRM))
		return;
  frm->ShowWindow(SW_SHOWNORMAL);
	frm->UpdateWindow();
#else
  if (!b.m_hWnd) {
    b.page=page;
    RECT rect;
    rect.bottom=rect.left=rect.right=rect.top=0;
    b.Create(NULL,NULL,WS_OVERLAPPEDWINDOW,rect,NULL,0);
    b.ShowWindow(SW_SHOWNORMAL);
  } else {
    b.SetForegroundWindow();
  }
#endif
}

