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
// InsertUrl.cpp : implementation file
//

#include "stdafx.h"

// Note: weird C2894 error if not included here
#include <ws2tcpip.h>
#include <Wspiapi.h>
extern "C" {
  #include "HTTrackInterface.h"
  //#include "htsglobal.h"
  //#include "htsbase.h"
  //#include "htslib.h"
}
//#include "winsock2.h"

#include "Shell.h"
#include "InsertUrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern HICON httrack_icon;

// Helper
extern LaunchHelp* HtsHelper;


/////////////////////////////////////////////////////////////////////////////
// CInsertUrl dialog


CInsertUrl::CInsertUrl(CWnd* pParent /*=NULL*/)
	: CDialog(CInsertUrl::IDD, pParent)
{
	//{{AFX_DATA_INIT(CInsertUrl)
	m_urllogin = _T("");
	m_urlpass = _T("");
	m_urladr = _T("");
	//}}AFX_DATA_INIT
}


void CInsertUrl::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CInsertUrl)
	DDX_Text(pDX, IDC_urllogin, m_urllogin);
	DDX_Text(pDX, IDC_urlpass, m_urlpass);
	DDX_Text(pDX, IDC_urladr, m_urladr);
	//}}AFX_DATA_MAP
  DDV_MaxChars(pDX, m_urladr, 2000);
}


BEGIN_MESSAGE_MAP(CInsertUrl, CDialog)
	//{{AFX_MSG_MAP(CInsertUrl)
	ON_BN_CLICKED(ID_capt, Oncapt)
	//}}AFX_MSG_MAP
  ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
  ON_COMMAND(ID_HELP,OnHelpInfo2)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CInsertUrl message handlers

BOOL CInsertUrl::OnInitDialog() 
{
	CDialog::OnInitDialog();
  GetDlgItem(IDC_urladr)->SendMessage(EM_SETLIMITTEXT, 2000, 0);
  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);
  EnableToolTips(true);     // TOOL TIPS
  SetForegroundWindow();   // yop en premier plan!
	
  if (LANG_T(-1)) {    // Patcher en français
    SetWindowTextCP(this,  LANG(LANG_T1));
    SetDlgItemTextCP(this, IDC_STATIC_adr,LANG(LANG_T2));
    SetDlgItemTextCP(this, IDC_STATIC_auth,LANG(LANG_T4));
    SetDlgItemTextCP(this, IDC_STATIC_login,LANG(LANG_T5));
    SetDlgItemTextCP(this, IDC_STATIC_pass,LANG(LANG_T6));
    SetDlgItemTextCP(this, IDC_STATIC_capt,LANG(LANG_T7));
    SetDlgItemTextCP(this, ID_capt,LANG(LANG_T8));
    SetDlgItemTextCP(this, IDOK,LANG(LANG_OK));
    SetDlgItemTextCP(this, IDCANCEL,LANG(LANG_CANCEL));
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
BOOL CInsertUrl::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
const char* CInsertUrl::GetTip(int ID)
{
  switch(ID) {
    case IDC_urladr:    return LANG(LANG_T10); break;
    case IDC_urllogin:  return LANG(LANG_T12); break;
    case IDC_urlpass:   return LANG(LANG_T13); break;
    case ID_capt:       return LANG(LANG_T14); break;
    case IDOK:           return LANG(LANG_TIPOK); break;      
    case IDCANCEL:       return LANG(LANG_TIPCANCEL); break;
    //case : return ""; break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------

// Appel aide
void CInsertUrl::OnHelpInfo2() {
  (void)OnHelpInfo(NULL);
}

BOOL CInsertUrl::OnHelpInfo(HELPINFO* dummy) 
{
  HtsHelper->Help();
  return true;
}

// Ne fait pas partie de la classe
UINT RunBackCatchServer( LPVOID pP ) {
  CInsertUrl* _this=(CInsertUrl*) pP;
  //
  char url[HTS_URLMAXSIZE*2]; url[0]='\0';
  char method[32]; method[0]='\0';
  char data[32768]; data[0]='\0';
  if (catch_url(_this->soc,url,method,data)) {
    if (_this->m_hWnd) {
      char dest[HTS_URLMAXSIZE*2];
      int i=0;
      strcpybuff(dest,_this->dest_path);
      {
        char* a;
        while(a=strchr(dest,'\\')) *a='/';
        structcheck(dest);
      }
      do {
        sprintf(dest,"%s%s%d",(LPCTSTR)_this->dest_path,"hts-post",i);
        i++;
      } while(fexist(dest));
      {
        FILE* fp=fopen(dest,"wb");
        if (fp) {
          fwrite(data,strlen(data),1,fp);
          fclose(fp);
        }
      }
      // former URL!
      {
        char finalurl[HTS_URLMAXSIZE*4+16];
        inplace_escape_check_url(dest, sizeof(dest));
        /* Both sources are HTS_URLMAXSIZE*2 and the token is 11 bytes; snprintf is the belt. */
        _snprintf(finalurl,sizeof(finalurl)-1,"%s" POSTTOK "file:%s",url,dest);
        finalurl[sizeof(finalurl)-1]='\0';
        SetDlgItemTextCP(_this, IDC_urladr,finalurl);
      }
    }
  } else {
    CString info;
    info.Format("Error while capturing URL\n(from %s)",url);
    AfxMessageBox(info);
  }
#ifdef _WIN32
  closesocket(_this->soc);
#else
  close(_this->soc);
#endif
  // Quitter
  if (_this->m_hWnd) {
    if (_this->dial.m_hWnd) {
      ::SendMessage(_this->dial.m_hWnd,WM_CLOSE,0,0);
    }
  }
  //_this->dial.EndDialog(IDOK);
  return 0;
}

void CInsertUrl::Oncapt() 
{
  // Demander un port d'écoute
  adr_prox[0]='\0';
  port_prox=0;
  soc=catch_url_init_std(&port_prox,adr_prox);
  if (soc != INVALID_SOCKET) {
    //
    char s[8192];
    sprintf(s,"Please TEMPORARILY set your browser's proxy preferences to:\r\n\r\nProxy's address:\r\n%s\r\nProxy's port:\r\n%d\r\n",adr_prox,port_prox);
    //
    dial.m_info=s;
    // ECOUTER EN THREAD
    AfxBeginThread(RunBackCatchServer,this);
    // AFFICHER INFOS
    {
      CWaitCursor wait;      // Afficher curseur sablier
      if (dial.DoModal() != IDOK) {
      }
    }
    //
  } else
    AfxMessageBox("Error!..",MB_SYSTEMMODAL);
}

