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
// OptionTab1.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "OptionTab1.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COptionTab1 property page

IMPLEMENT_DYNCREATE(COptionTab1, CPropertyPage)

COptionTab1::COptionTab1() : CPropertyPage(COptionTab1::IDD)
{
  // Patcher titre
  if (LANG_T(-1)) {    // Patcher en français
    m_strCaption = LANG(LANG_IOPT1); // page-owned copy; the hash-table string is freed on a language change
    m_psp.pszTitle = m_strCaption;
    m_psp.dwFlags|=PSP_USETITLE;
  }
  m_psp.dwFlags|=PSP_HASHELP;
  //
  //{{AFX_DATA_INIT(COptionTab1)
	m_link = FALSE;
	m_parseall = FALSE;
	m_testall = FALSE;
	m_htmlfirst = FALSE;
	m_keepwww = FALSE;
	m_keepslashes = FALSE;
	m_keepqueryorder = FALSE;
	//}}AFX_DATA_INIT
}

COptionTab1::~COptionTab1()
{
}

void COptionTab1::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionTab1)
	DDX_Check(pDX, IDC_link, m_link);
	DDX_Check(pDX, IDC_parseall, m_parseall);
	DDX_Check(pDX, IDC_testall, m_testall);
	DDX_Check(pDX, IDC_htmlfirst, m_htmlfirst);
	DDX_Check(pDX, IDC_keepwww, m_keepwww);
	DDX_Check(pDX, IDC_keepslashes, m_keepslashes);
	DDX_Check(pDX, IDC_keepqueryorder, m_keepqueryorder);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionTab1, CPropertyPage)
	//{{AFX_MSG_MAP(COptionTab1)
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionTab1 message handlers

BOOL COptionTab1::OnInitDialog() 
{
  CPropertyPage::OnInitDialog();
  EnableToolTips(true);     // TOOL TIPS
	
  // mode modif à la volée
  if (modify==1) {
    GetDlgItem(IDC_parseall) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_link)     ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_testall)  ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_htmlfirst)->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_keepwww)  ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_keepslashes)->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_keepqueryorder)->ModifyStyle(0,WS_DISABLED);
  } else {
    GetDlgItem(IDC_parseall) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_link)     ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_testall)  ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_htmlfirst)->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_keepwww)  ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_keepslashes)->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_keepqueryorder)->ModifyStyle(WS_DISABLED,0);
  }

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    //SetWindowTextCP(this, LANG(LANG_I30)); // "Options (les champs laissés vides seront fixés sur les valeurs par défaut)");
    SetDlgItemTextCP(this, IDC_link,LANG(LANG_I31)); // "Capturer les fichiers non html proches (ex: fichiers ZIP situés à l'extérieur)");
    SetDlgItemTextCP(this, IDC_testall,LANG(LANG_I32)); // "Tester tous les liens (même ceux interdits)");
    SetDlgItemTextCP(this, IDC_parseall,LANG(LANG_I32b));
    SetDlgItemTextCP(this, IDC_htmlfirst,LANG(LANG_I32c));
    SetDlgItemTextCP(this, IDC_keepwww,LANG(LANG_KEEPWWW));
    SetDlgItemTextCP(this, IDC_keepslashes,LANG(LANG_KEEPSLASHES));
    SetDlgItemTextCP(this, IDC_keepqueryorder,LANG(LANG_KEEPQUERYORDER));
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
BOOL COptionTab1::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
const char* COptionTab1::GetTip(int ID)
{
  switch(ID) {
    case IDC_link:    return LANG(LANG_I1); break; // "Get files even in foreign addresses","Récupérer les fichiers même sur les liens extérieurs"); break;
    case IDC_testall: return LANG(LANG_I2); break; // "Test all links in pages","Tester tous les liens dans les pages"); break;
    case IDC_parseall: return LANG(LANG_I2b); break;
    case IDC_htmlfirst: return LANG(LANG_I2c); break;
    case IDC_keepwww:   return LANG(LANG_KEEPWWWTIP); break;
    case IDC_keepslashes: return LANG(LANG_KEEPSLASHESTIP); break;
    case IDC_keepqueryorder: return LANG(LANG_KEEPQUERYORDERTIP); break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------


