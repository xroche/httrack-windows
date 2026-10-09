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
// OptionTab7.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "OptionTab7.h"
#include "AddFilter.h"
#include "DarkMode.h"

/* basic HTTrack defs */
extern "C" {
  #include "HTTrackInterface.h"
  //#include "htsglobal.h"
  //#include "htsbase.h"
}

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COptionTab7 property page

IMPLEMENT_DYNCREATE(COptionTab7, CPropertyPage)

/* Called again before each show: the page is built before a first run knows its
   language. The copy is page-owned, since a language change frees the hash string. */
void COptionTab7::SetLangTitle(void)
{
  const char *const title = LANG(LANG_IOPT7);
  /* English keeps the .rc caption, as it did when this ran only from the constructor. */
  if (LANG_T(-1) != 0 && title[0] != '\0') {
    m_strCaption = title;
    m_psp.pszTitle = m_strCaption;
    m_psp.dwFlags|=PSP_USETITLE;
  }
}

COptionTab7::COptionTab7() : CPropertyPage(COptionTab7::IDD)
{
  SetLangTitle();
  m_psp.dwFlags|=PSP_HASHELP;
  //
	//{{AFX_DATA_INIT(COptionTab7)
	m_url2 = _T("");
	//}}AFX_DATA_INIT
  // only the modify-on-the-fly path ever writes it, and OnInitDialog reads it
  modify = 0;
  m_writingRules = 0;
}

COptionTab7::~COptionTab7()
{
}

void COptionTab7::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionTab7)
	DDX_Text(pDX, IDC_URL2, m_url2);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionTab7, CPropertyPage)
	//{{AFX_MSG_MAP(COptionTab7)
	ON_BN_CLICKED(IDC_ADD1, OnAdd1)
	ON_BN_CLICKED(IDC_ADD2, OnAdd2)
	ON_BN_CLICKED(IDC_CHECK1, OnCheck1)
	ON_BN_CLICKED(IDC_CHECK2, OnCheck2)
	ON_BN_CLICKED(IDC_CHECK3, OnCheck3)
	ON_WM_SIZE()
	ON_WM_CTLCOLOR()
	//}}AFX_MSG_MAP
  ON_EN_CHANGE(IDC_URL2, OnChangeUrl2)
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionTab7 message handlers

static void NewFilter(int i,char* s,size_t ssize) {  // 0: forbid 1: accept
  CAddFilter AddF;
  AddF.type=i;
  if (AddF.type==0)
    AddF.m_addtype=LANG(LANG_B20); /*"Links following this rule will be forbidden:"*/
  else
    AddF.m_addtype=LANG(LANG_B21); // "Links following this rule will be accepted:";
  if (AddF.DoModal()==IDOK) {
    char query[2048],t[2048],as[2100];
    char* q;

    // error
    if (AddF.m_afquery.GetLength() >= sizeof(query) - 2 ) {
      return;
    }

    strlcpybuff(s,"",ssize);
    strcpybuff(query,AddF.m_afquery);
    q=query;
    
    if (AddF.m_aftype==10) {
      if (i==0)
        strlcpybuff(s,"-",ssize);
      else
        strlcpybuff(s,"+",ssize);
      strlcatbuff(s,"*",ssize);
    } else {
      while(strlen(q)>0) {
        while ((*q==' ') || (*q==',')) q++;
        strcpybuff(t,"");
        {  // prochain (séparé par des ,)
          char *a,*b;
          a=strchr(q,' ');
          b=strchr(q,',');
          if (a && b) {  // départager
            if ( b < a)
              a=b;
          } else if (b) a=b;
          
          if (a) {
            t[0]='\0';
            strlncatbuff(t,q,sizeof(t),a-q);
            q=a+1;
          } else {
            strlcpybuff(t,q,sizeof(t));
            strcpybuff(q,"");
          }
        }
        
        if (strlen(t)>0) {
          strcpybuff(as,"");
          switch (AddF.m_aftype) {
          case 0:  // ext
            sprintf(as,"*.%s",t);
            break;
          case 1:  // contient
            sprintf(as,"*/*%s*",t);
            break;
          case 2:  // this one
            sprintf(as,"*/%s",t);
            break;
          case 3:  // folder contains
            sprintf(as,"*/*%s*/*",t);
            break;
          case 4:  // this folder
            sprintf(as,"*/%s/*",t);
            break;
          case 5:  // domaine
            sprintf(as,"*[name].%s/*",t);
            break;
          case 6:  // contien
            sprintf(as,"*[name].*[name]%s*[name].*[name]/*",t);
            break;
          case 7:  // host
            sprintf(as,"%s/*",t);
            break;
          case 8:  // link contient
            sprintf(as,"*%s*",t);
            break;
          case 9:  // lien exact
            sprintf(as,"%s",t);
            break;
          }
          if (strlen(as)>0) {
            /* Stop at capacity rather than abort: strlcatbuff() does not truncate. */
            if (strlen(s) + strlen(as) + 4 >= ssize)
              break;
            if (i==0)
              strlcatbuff(s,"-",ssize);
            else
              strlcatbuff(s,"+",ssize);
            strlcatbuff(s,as,ssize);
            strlcatbuff(s,"\x0d\x0a",ssize);
          }
        }
      }
      
    }

  } else strlcpybuff(s,"",ssize);
}


void COptionTab7::OnAdd1() 
{
  char s[1024]; s[0]='\0';
  NewFilter(0,s,sizeof(s));
  if (strlen(s)>0) {
    char tempo[HTS_URLMAXSIZE*16];
    {
      CString st;
      GetDlgItemText(IDC_URL2,st);
      if (st.GetLength() < sizeof(tempo) - 2)
        strcpybuff(tempo,st);
      else
        tempo[0] = '\0';
    }
    if (strlen(tempo)>0) {
      if ((tempo[strlen(tempo)-1]!=' ') && (tempo[strlen(tempo)-1]!='\n') && (tempo[strlen(tempo)-1]!=13))
        strcatbuff(tempo,"\x0d\x0a");
    }
    strcatbuff(tempo,s);
    //	m_url2=tempo;
    SetDlgItemTextCP(this, IDC_URL2,tempo);
  }
}

void COptionTab7::OnAdd2() 
{
  char s[1024]; s[0]='\0';
  NewFilter(1,s,sizeof(s));
  if (strlen(s)>0) {
    char tempo[HTS_URLMAXSIZE*16];
    {
      CString st;
      GetDlgItemText(IDC_URL2,st);
      if (st.GetLength() < sizeof(tempo) - 2)
        strcpybuff(tempo,st);
      else
        tempo[0] = '\0';
    }
    if (strlen(tempo)>0) {
      if ((tempo[strlen(tempo)-1]!=' ') && (tempo[strlen(tempo)-1]!='\n') && (tempo[strlen(tempo)-1]!=13))
        strcatbuff(tempo,"\x0d\x0a");
    }
    strcatbuff(tempo,s);
    //	m_url2=tempo;
    SetDlgItemTextCP(this, IDC_URL2,tempo);
  }
}

// The engine would refuse a malformed rule too, but only once this page has closed.
BOOL COptionTab7::OnKillActive()
{
  if (!CPropertyPage::OnKillActive())      // DDX runs here, so m_url2 is current
    return FALSE;

  const CString refusal = liveScanRuleRefusal(m_rulesAtOpen, m_url2, modify==1 ? TRUE : FALSE);

  if (!refusal.IsEmpty()) {
    // the tip rather than a message box, whose modal loop would run the
    // end-of-mirror teardown under this page
    SetDlgItemTextCP(this, IDC_STATIC_tip, refusal);
    GetDlgItem(IDC_URL2)->SetFocus();
    MessageBeep(MB_ICONEXCLAMATION);
    return FALSE;
  }
  if (modify==1)
    SetDlgItemTextLang(this, IDC_STATIC_tip, LANG(LANG_LIVERULES));   // drop a stale refusal
  return TRUE;
}

BOOL COptionTab7::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	WhttDarkInitWindow(this);
  BuildLayout();
  EnableToolTips(true);     // TOOL TIPS

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    SetWindowTextCP(this, LANG(LANG_B9)); // "Filtres");
    SetDlgItemTextCP(this, IDC_STATIC_finfo,LANG(LANG_B10)); // "Vous pouvez exclure ou accepter plusieurs URLs ou liens, en utilisant les jokers\nVous pouvez utiliser les virgules ou les espaces entre les filtres\nExemple: +*.zip,-www.*.com,-www.*.edu/cgi-bin/*.cgi");
    SetDlgItemTextCP(this, IDC_ADD1,LANG(LANG_B11)); // "Exclure lien(s)..");
    SetDlgItemTextCP(this, IDC_ADD2,LANG(LANG_B12)); // "Accepter lien(s)..");
    SetDlgItemTextCP(this, IDC_STATIC_tip,LANG(LANG_B13)); // "Conseil: Si vous voulez accepter tous les fichiers gif d'un site, utilisez quelque chose comme +www.monweb.com/*.gif\n(+*.gif autorisera TOUS les fichiers gif sur TOUS les sites)");
  }

  // mode modif à la volée, après le patch de langue qui réécrit le même libellé
  if (modify==1)
    SetDlgItemTextLang(this, IDC_STATIC_tip, LANG(LANG_LIVERULES));

  RefreshPresetChecks();      // after the base class filled IDC_URL2, which is what it reads

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
BOOL COptionTab7::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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
const char* COptionTab7::GetTip(int ID)
{
  switch(ID) {
    case IDC_ADD1: return LANG(LANG_C1);   /*"Add refuse filter"*/ break;
    case IDC_ADD2: return LANG(LANG_C2);   /*"Add accept filter"*/ break;
    case IDC_URL2: return LANG(LANG_C3);   /*"Extra filters"*/ break;
  }
  return "";
}
// TOOL TIPS
// ------------------------------------------------------------

/* Restores the count even when the write throws, which would otherwise leave the page
   never following the field again. */
class RulesBoxWrite {
public:
  RulesBoxWrite(int &count) : m_count(count) { m_count++; }
  ~RulesBoxWrite() { m_count--; }
private:
  int &m_count;
};

void COptionTab7::EnsureIncluded(BOOL checked, const CString &preset)  {
  CString st;

  GetDlgItemText(IDC_URL2,st);
  /* The click already set the checkmark, so skip the refresh this write would trigger. */
  RulesBoxWrite writing(m_writingRules);
  SetDlgItemTextCP(this, IDC_URL2, applyRulePreset(st, preset, checked));
}

void COptionTab7::RefreshPresetChecks()
{
  CString st;

  GetDlgItemText(IDC_URL2,st);
  CheckDlgButton(IDC_CHECK1, ruleListHoldsPreset(st, rulePresetImages) ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(IDC_CHECK2, ruleListHoldsPreset(st, rulePresetArchives) ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(IDC_CHECK3, ruleListHoldsPreset(st, rulePresetMovies) ? BST_CHECKED : BST_UNCHECKED);
}

void COptionTab7::OnChangeUrl2()
{
  if (m_writingRules == 0)
    RefreshPresetChecks();
}

void COptionTab7::OnCheck1() 
{
  EnsureIncluded(this->IsDlgButtonChecked(IDC_CHECK1),rulePresetImages);
}

void COptionTab7::OnCheck2() 
{
  EnsureIncluded(this->IsDlgButtonChecked(IDC_CHECK2),rulePresetArchives);
}

void COptionTab7::OnCheck3() 
{
  EnsureIncluded(this->IsDlgButtonChecked(IDC_CHECK3),rulePresetMovies);
}

void COptionTab7::BuildLayout()
{
  /* The rules box takes the room; the tip stays under it. */
  m_layout.Reset(this);
  m_layout.AddId(this, IDC_STATIC_finfo, 0, 100);
  m_layout.AddId(this, IDC_URL2, 0, 100, 0, 100);
  m_layout.AddId(this, IDC_STATIC_tip, 0, 100, 100, 0);
}

void COptionTab7::OnSize(UINT nType, int cx, int cy)
{
  CPropertyPage::OnSize(nType, cx, cy);
  m_layout.Apply(cx, cy);
}

HBRUSH COptionTab7::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
  const HBRUSH brush = WhttDarkCtlColor(pDC, pWnd, nCtlColor);

  return brush != NULL ? brush : CPropertyPage::OnCtlColor(pDC, pWnd, nCtlColor);
}
