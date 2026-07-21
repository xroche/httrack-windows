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
// OptionTab6.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "OptionTab6.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COptionTab6 property page

IMPLEMENT_DYNCREATE(COptionTab6, CPropertyPage)

COptionTab6::COptionTab6() : CPropertyPage(COptionTab6::IDD)
{
  // Patcher titre
  if (LANG_T(-1)) {    // Patcher en français
    m_psp.pszTitle=LANG(LANG_IOPT6); // titre
    m_psp.dwFlags|=PSP_USETITLE;
  }
  m_psp.dwFlags|=PSP_HASHELP;
  //
	//{{AFX_DATA_INIT(COptionTab6)
	m_user = _T("");
	m_footer = _T("");
	m_accept_language = _T("");
	m_other_headers = _T("");
	m_default_referer = _T("");
	//}}AFX_DATA_INIT
}

COptionTab6::~COptionTab6()
{
}

void COptionTab6::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionTab6)
	DDX_CBString(pDX, IDC_user, m_user);
	DDX_CBString(pDX, IDC_footer, m_footer);
	DDX_Text(pDX, IDC_accept_language, m_accept_language);
	DDX_Text(pDX, IDC_other_headers, m_other_headers);
	DDX_Text(pDX, IDC_default_referer, m_default_referer);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionTab6, CPropertyPage)
	//{{AFX_MSG_MAP(COptionTab6)
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionTab6 message handlers

BOOL COptionTab6::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
  EnableToolTips(true);     // TOOL TIPS
	
  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    SetDlgItemTextCP(this, IDC_STATIC_browsid,LANG(LANG_I43)); // "Identité");
    SetDlgItemTextCP(this, IDC_STATIC_footer,LANG(LANG_I43b));
    SetDlgItemTextCP(this, IDC_STATIC_accept_language,LANG(LANG_I43c));
    SetDlgItemTextCP(this, IDC_STATIC_other_headers,LANG(LANG_I43d));
    SetDlgItemTextCP(this, IDC_STATIC_default_referer,LANG(LANG_I43e));
  }
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}



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
BOOL COptionTab6::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
const char* COptionTab6::GetTip(int ID)
{
  switch(ID) {
    case IDC_user: return LANG(LANG_I23); break; // "Browser identity","Identité du browser"); break;
    case IDC_footer: return LANG(LANG_I23b); break;
    case IDC_accept_language: return LANG(LANG_I23c); break;
    case IDC_other_headers: return LANG(LANG_I23d); break;
    case IDC_default_referer: return LANG(LANG_I23e); break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------

