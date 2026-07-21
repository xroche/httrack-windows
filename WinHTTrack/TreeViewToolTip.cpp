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
// TreeViewToolTip.cpp : implementation file
//

#include "stdafx.h"
#include "winhttrack.h"
#include "TreeViewToolTip.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTreeViewToolTip

CTreeViewToolTip::CTreeViewToolTip()
{
  EnableToolTips(true);
}

CTreeViewToolTip::~CTreeViewToolTip()
{
}


BEGIN_MESSAGE_MAP(CTreeViewToolTip, CToolTipCtrl)
	//{{AFX_MSG_MAP(CTreeViewToolTip)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTreeViewToolTip message handlers



// ------------------------------------------------------------
// TOOL TIPS
//
// ajouter dans le .cpp:
// remplacer les deux Wid1:: par le nom de la classe::
// dans la message map, ajouter
// ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
// dans initdialog ajouter
// EnableToolTips(true);     // TOOL TIPS
//
// ajouter dans le .h:
// char* GetTip(int id);
// et en generated message map
// afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
BOOL CTreeViewToolTip::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
{
  TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
  UINT_PTR nID =pNMHDR->idFrom;
  if (pTTT->uFlags & TTF_IDISHWND)
  {
    // idFrom is actually the HWND of the tool
    nID = ::GetDlgCtrlID((HWND)nID);
    if(nID)
    {
      const char* st=GetTip((int)nID);
      if (st != NULL && *st) {
        pTTT->lpszText = (LPSTR)st;
        pTTT->hinst = AfxGetResourceHandle();
        return(TRUE);
      }
    }
  }
  return(FALSE);
}
const char* CTreeViewToolTip::GetTip(int ID)
{
  /*
  switch(ID) {
    case 0:  return ""; break;
  }
  */
  return "[File]";
}
// TOOL TIPS
// ------------------------------------------------------------


