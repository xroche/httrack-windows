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
// OptionTab8.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "OptionTab8.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COptionTab8 property page

IMPLEMENT_DYNCREATE(COptionTab8, CPropertyPage)

/* Called again before each show: the page is built before a first run knows its
   language. The copy is page-owned, since a language change frees the hash string. */
void COptionTab8::SetLangTitle(void)
{
  const char *const title = LANG(LANG_IOPT8);
  /* English keeps the .rc caption, as it did when this ran only from the constructor. */
  if (LANG_T(-1) != 0 && title[0] != '\0') {
    m_strCaption = title;
    m_psp.pszTitle = m_strCaption;
    m_psp.dwFlags|=PSP_USETITLE;
  }
}

COptionTab8::COptionTab8() : CPropertyPage(COptionTab8::IDD)
{
  SetLangTitle();
  m_psp.dwFlags|=PSP_HASHELP;
  //
	//{{AFX_DATA_INIT(COptionTab8)
	m_checktype = -1;
	m_cookies = FALSE;
	m_parsejava = FALSE;
	m_robots = -1;
	m_http10 = FALSE;
	m_toler = FALSE;
	m_updhack = FALSE;
	m_urlhack = FALSE;
	m_cookiesfile = _T("");
	m_sitemap = FALSE;
	m_sitemapurl = _T("");
	m_hostalias = _T("");
	//}}AFX_DATA_INIT
  // only the modify-on-the-fly path ever writes it, and OnInitDialog reads it
  modify = 0;
}

COptionTab8::~COptionTab8()
{
}

void COptionTab8::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionTab8)
	DDX_CBIndex(pDX, IDC_checktype, m_checktype);
	DDX_Check(pDX, IDC_cookies, m_cookies);
	DDX_Check(pDX, IDC_parsejava, m_parsejava);
	DDX_CBIndex(pDX, IDC_robots, m_robots);
	DDX_Check(pDX, IDC_http10, m_http10);
	DDX_Check(pDX, IDC_toler, m_toler);
	DDX_Check(pDX, IDC_updhack, m_updhack);
	DDX_Check(pDX, IDC_urlhack, m_urlhack);
	DDX_Text(pDX, IDC_cookiesfile, m_cookiesfile);
	DDX_Check(pDX, IDC_sitemap, m_sitemap);
	DDX_Text(pDX, IDC_sitemapurl, m_sitemapurl);
	DDV_MaxChars(pDX, m_sitemapurl, 1023);  // HTS_URLMAXSIZE, past which the engine aborts the mirror
	DDX_Text(pDX, IDC_hostalias, m_hostalias);
	DDV_MaxChars(pDX, m_hostalias, 1023);   // caps the field, so no rule in it can reach the engine's cap
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionTab8, CPropertyPage)
	//{{AFX_MSG_MAP(COptionTab8)
	ON_BN_CLICKED(IDC_cookiesfilebrowse, OnCookiesFileBrowse)
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
  ON_BN_CLICKED(IDC_sitemap, OnGateClicked)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionTab8 message handlers

void COptionTab8::OnCookiesFileBrowse()
{
  // writable buffer: CFileDialog rewrites the '|' separators in place
  char szFilter[] = "Cookies file (cookies.txt, *.txt)|*.txt|All files (*.*)|*.*||";
  CFileDialog dial(TRUE, "txt", NULL, OFN_HIDEREADONLY | WhttOfnFlags() | OFN_FILEMUSTEXIST, szFilter);
  if (dial.DoModal() == IDOK)
    SetDlgItemTextCP(this, IDC_cookiesfile, dial.GetPathName());
}

void COptionTab8::OnGateClicked() 
{
  UpdateGatedControls();
}

// see OptionTab8.h
void COptionTab8::UpdateGatedControls() 
{
  if (modify == 1)   // a running mirror already disabled the whole page
    return;

  const BOOL sitemap = IsDlgButtonChecked(IDC_sitemap);

  // EnableWindow, not the ModifyStyle above: this one runs with the page on screen
  GetDlgItem(IDC_sitemapurl)->EnableWindow(sitemap);
  GetDlgItem(IDC_STATIC_sitemapurl)->EnableWindow(sitemap);
}

BOOL COptionTab8::OnInitDialog() 
{
	CDialog::OnInitDialog();
  BuildLayout();
  EnableToolTips(true);     // TOOL TIPS

  // mode modif à la volée
  if (modify==1) {
    GetDlgItem(IDC_cookies)          ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_checktype)        ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_parsejava)        ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_http10)           ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_toler)            ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_updhack)          ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_urlhack)          ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_robots)           ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_STATIC_checktype) ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_STATIC_spider)    ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_sitemap)          ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_sitemapurl)       ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_STATIC_sitemapurl)->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_hostalias)        ->ModifyStyle(0,WS_DISABLED);
    GetDlgItem(IDC_STATIC_hostalias) ->ModifyStyle(0,WS_DISABLED);
  } else {
    GetDlgItem(IDC_cookies)          ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_checktype)        ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_parsejava)        ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_http10)           ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_toler)            ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_updhack)          ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_urlhack)          ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_robots)           ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_STATIC_checktype) ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_STATIC_spider)    ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_sitemap)          ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_sitemapurl)       ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_STATIC_sitemapurl)->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_hostalias)        ->ModifyStyle(WS_DISABLED,0);
    GetDlgItem(IDC_STATIC_hostalias) ->ModifyStyle(WS_DISABLED,0);
  }

  if (LANG_T(-1)) {    // Patcher en français
    SetDlgItemTextCP(this, IDC_robots,LANG(LANG_I55));
    SetDlgItemTextCP(this, IDC_cookies,LANG(LANG_I58));
    SetDlgItemTextCP(this, IDC_STATIC_checktype,LANG(LANG_I59));
    SetDlgItemTextCP(this, IDC_parsejava,LANG(LANG_I60));
    SetDlgItemTextCP(this, IDC_http10,LANG(LANG_I63));
    SetDlgItemTextCP(this, IDC_toler,LANG(LANG_I62));
    SetDlgItemTextCP(this, IDC_updhack,LANG(LANG_I62b));
    SetDlgItemTextCP(this, IDC_urlhack,LANG(LANG_I62b2));
    SetDlgItemTextCP(this, IDC_STATIC_cookiesfile,LANG(LANG_COOKIEFILE));
    SetDlgItemTextLang(this, IDC_sitemap,LANG(LANG_SITEMAP));
    SetDlgItemTextLang(this, IDC_STATIC_sitemapurl,LANG(LANG_SITEMAPURL));
    SetDlgItemTextLang(this, IDC_STATIC_hostalias,LANG(LANG_HOSTALIAS));
    SetCombo(this,IDC_checktype,LISTDEF_7);
    SetCombo(this,IDC_robots,LISTDEF_8);
  }  

  UpdateGatedControls();

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
BOOL COptionTab8::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
const char* COptionTab8::GetTip(int ID)
{
  switch(ID) {
    case IDC_robots:    return LANG(LANG_I28); break; // robots.txt
    case IDC_cookies:   return LANG(LANG_I1b); break;
    case IDC_checktype: return LANG(LANG_I1c); break;
    case IDC_parsejava: return LANG(LANG_I1d); break;
    case IDC_http10:    return LANG(LANG_I1j); break;
    case IDC_toler:     return LANG(LANG_I1i); break;
    case IDC_updhack:   return LANG(LANG_I1k); break;
    case IDC_urlhack:   return LANG(LANG_I1k2); break;
    case IDC_cookiesfile: return LANG(LANG_COOKIEFILETIP); break;
    case IDC_sitemap:   return LANG(LANG_SITEMAPTIP); break;
    case IDC_sitemapurl: return LANG(LANG_SITEMAPURLTIP); break;
    case IDC_hostalias: return LANG(LANG_HOSTALIASTIP); break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------



void COptionTab8::BuildLayout()
{
  m_layout.Reset(this);
  m_layout.AddId(this, IDC_checktype, 0, 100);
  m_layout.AddId(this, IDC_robots, 0, 100);
  m_layout.AddId(this, IDC_cookiesfile, 0, 100);
  m_layout.AddId(this, IDC_cookiesfilebrowse, 100, 0);
  m_layout.AddId(this, IDC_sitemapurl, 0, 100);
  m_layout.AddId(this, IDC_hostalias, 0, 100);
}

void COptionTab8::OnSize(UINT nType, int cx, int cy)
{
  CPropertyPage::OnSize(nType, cx, cy);
  m_layout.Apply(cx, cy);
}
