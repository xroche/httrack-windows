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
// ProxyId.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "ProxyId.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern HICON httrack_icon;

// Helper
extern LaunchHelp* HtsHelper;

/////////////////////////////////////////////////////////////////////////////
// CProxyId dialog


CProxyId::CProxyId(CWnd* pParent /*=NULL*/)
	: CDialog(CProxyId::IDD, pParent)
{
	//{{AFX_DATA_INIT(CProxyId)
	m_proxadr = _T("");
	m_proxlogin = _T("");
	m_proxpass = _T("");
	m_proxport = _T("");
	m_proxytype = 0;
	//}}AFX_DATA_INIT
}


void CProxyId::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CProxyId)
	DDX_Text(pDX, IDC_proxadr, m_proxadr);
	DDX_Text(pDX, IDC_proxlogin, m_proxlogin);
	DDX_Text(pDX, IDC_proxpass, m_proxpass);
	DDX_Text(pDX, IDC_proxport, m_proxport);
	DDX_Control(pDX, IDC_proxytype, m_ctl_proxytype);
	DDX_CBIndex(pDX, IDC_proxytype, m_proxytype);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CProxyId, CDialog)
	//{{AFX_MSG_MAP(CProxyId)
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
  ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
  ON_COMMAND(ID_HELP,OnHelpInfo2)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
END_MESSAGE_MAP()


BOOL CProxyId::OnInitDialog() 
{
	CDialog::OnInitDialog();
  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);
  EnableToolTips(true);     // TOOL TIPS
  SetForegroundWindow();   // yop en premier plan!

  /* Proxy protocol selector. Index maps to m_proxytype; the scheme is
     prepended to the -P argument by the shell. */
  m_ctl_proxytype.AddString("HTTP");
  m_ctl_proxytype.AddString("SOCKS5");
  m_ctl_proxytype.AddString("HTTP (CONNECT tunnel)");
  m_ctl_proxytype.SetCurSel((m_proxytype >= 0 && m_proxytype < m_ctl_proxytype.GetCount()) ? m_proxytype : 0);
	
  if (LANG_T(-1)) {    // Patcher en français
    SetWindowTextCP(this,  LANG(LANG_R1));
    SetDlgItemTextCP(this, IDC_STATIC_ptype,LANG(LANG_PROXYTYPE));
    SetDlgItemTextCP(this, IDC_STATIC_adr,LANG(LANG_R2));
    SetDlgItemTextCP(this, IDC_STATIC_port,LANG(LANG_R3));
    SetDlgItemTextCP(this, IDC_STATIC_auth,LANG(LANG_R4));
    SetDlgItemTextCP(this, IDC_STATIC_login,LANG(LANG_R5));
    SetDlgItemTextCP(this, IDC_STATIC_pass,LANG(LANG_R6));
    SetDlgItemTextCP(this, IDOK,LANG(LANG_OK));
    SetDlgItemTextCP(this, IDCANCEL,LANG(LANG_CANCEL));
  }

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


// Appel aide
void CProxyId::OnHelpInfo2() {
  (void) OnHelpInfo(NULL);
}

BOOL CProxyId::OnHelpInfo(HELPINFO* dummy) 
{
  //return CDialog::OnHelpInfo(pHelpInfo);
  //AfxGetApp()->WinHelp(0,HELP_FINDER);    // Index du fichier Hlp
  //LaunchHelp(pHelpInfo);
  HtsHelper->Help();
  return true;
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
BOOL CProxyId::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
const char* CProxyId::GetTip(int ID)
{
  switch(ID) {
    case IDC_proxytype:  return LANG(LANG_PROXYTYPETIP); break;
    case IDC_proxadr:    return LANG(LANG_R10); break;
    case IDC_proxport:   return LANG(LANG_R11); break;
    case IDC_proxlogin:  return LANG(LANG_R12); break;
    case IDC_proxpass:   return LANG(LANG_R13); break;
    case IDOK:           return LANG(LANG_TIPOK); break;      
    case IDCANCEL:       return LANG(LANG_TIPCANCEL); break;
    //case : return ""; break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------

