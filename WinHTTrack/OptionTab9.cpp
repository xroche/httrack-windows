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
// OptionTab9.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "OptionTab9.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COptionTab9 property page

IMPLEMENT_DYNCREATE(COptionTab9, CPropertyPage)

COptionTab9::COptionTab9() : CPropertyPage(COptionTab9::IDD)
{
  // Patcher titre
  if (LANG_T(-1)) {    // Patcher en français
    m_psp.pszTitle=LANG(LANG_IOPT9); // titre
    m_psp.dwFlags|=PSP_USETITLE;
  }
  m_psp.dwFlags|=PSP_HASHELP;
  //
	//{{AFX_DATA_INIT(COptionTab9)
	m_index = FALSE;
	m_logf = FALSE;
	m_Cache2 = FALSE;
	m_logtype = -1;
	m_norecatch = FALSE;
	m_index2 = FALSE;
	m_index_mail = FALSE;
	m_warc = FALSE;
	//}}AFX_DATA_INIT
}

COptionTab9::~COptionTab9()
{
}

void COptionTab9::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionTab9)
	DDX_Check(pDX, IDC_index, m_index);
	DDX_Check(pDX, IDC_logf, m_logf);
	DDX_Check(pDX, IDC_Cache2, m_Cache2);
	DDX_CBIndex(pDX, IDC_logtype, m_logtype);
	DDX_Check(pDX, IDC_norecatch, m_norecatch);
	DDX_Check(pDX, IDC_index2, m_index2);
	DDX_Check(pDX, IDC_index_mail, m_index_mail);
	DDX_Check(pDX, IDC_warc, m_warc);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionTab9, CPropertyPage)
	//{{AFX_MSG_MAP(COptionTab9)
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionTab9 message handlers

BOOL COptionTab9::OnInitDialog() 
{
	CDialog::OnInitDialog();
  EnableToolTips(true);     // TOOL TIPS

  if (LANG_T(-1)) {    // Patcher en français
    SetDlgItemTextCP(this, IDC_index,LANG(LANG_I35)); // "Faire un index");
    SetDlgItemTextCP(this, IDC_index2,LANG(LANG_I35b));
    SetDlgItemTextCP(this, IDC_index_mail,LANG(LANG_I35c));
    SetDlgItemTextCP(this, IDC_logf,LANG(LANG_I36)); // "Fichiers d'audit");
    SetDlgItemTextCP(this, IDC_Cache2,LANG(LANG_I61));
    SetDlgItemTextCP(this, IDC_norecatch,LANG(LANG_I34b));
    SetCombo(this,IDC_logtype,LISTDEF_9);
  }  

  // mode modif à la volée
  if (modify==1) {
    GetDlgItem(IDC_norecatch)->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_index)   ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_index2)  ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_index_mail)->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_logf)    ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_Cache2)  ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_warc)    ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_logtype) ->ModifyStyle(0,WS_DISABLED);
  } else {
    GetDlgItem(IDC_norecatch)->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_index)   ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_index2)  ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_index_mail)->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_logf)    ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_Cache2)  ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_warc)    ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_logtype) ->ModifyStyle(WS_DISABLED,0);
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
BOOL COptionTab9::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
const char* COptionTab9::GetTip(int ID)
{
  switch(ID) {
    case IDC_norecatch: return LANG(LANG_I5b); break;
    case IDC_index:   return LANG(LANG_I6); break; // "Create a start page","Générer une page de départ"); break;
    case IDC_index2:   return LANG(LANG_I6b); break; // "Create a start page","Générer une page de départ"); break;
    case IDC_index_mail:return LANG(LANG_I6c); break;
    case IDC_logf:    return LANG(LANG_I7); break; // "Create log files for error and info report","Générer des fichiers d'audit pour les erreurs et les messages"); break;
    case IDC_Cache2:  return LANG(LANG_I1e); break;
    case IDC_warc:    return "Write an ISO-28500 WARC archive (.warc.gz)"; break;
    case IDC_logtype: return LANG(LANG_I1f); break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------

