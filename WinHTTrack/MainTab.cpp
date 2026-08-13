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
// Tab Control Principal

#include "stdafx.h"
#include "Shell.h"
#include "Maintab.h"
#include "direct.h"

#include "winsvc.h "

#include "windows.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Icone HTTrack
extern HICON httrack_icon;

// Helper
extern LaunchHelp* HtsHelper;


/////////////////////////////////////////////////////////////////////////////
// CMainTab

//IMPLEMENT_DYNAMIC(CMainTab, CPropertySheet)

//HINSTANCE hInst = NULL;
//SC_HANDLE hSCMan = NULL;


CMainTab::CMainTab(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
  AddControlPages();
}

CMainTab::CMainTab(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
  AddControlPages();
}

CMainTab::~CMainTab()
{
}

/* NULL for every sheet here: MFC installs a callback only on the modeless Create path. */
static PFNPROPSHEETCALLBACK sheetCallback = NULL;

/* A sizing border only works if it is in the dialog template comctl32 builds: adding
   WS_THICKFRAME to the created window draws it but leaves the size fixed. */
static int CALLBACK SheetPreCreate(HWND hWnd, UINT message, LPARAM lParam)
{
  if (message == PSCB_PRECREATE && lParam != 0) {
    /* Either template shape can turn up. The extended one carries a 0xFFFF signature
       where the plain one has the top half of its style. */
    struct DLGTEMPLATEEX_HEAD {
      WORD dlgVer, signature;
      DWORD helpID, exStyle, style;
    };
    DLGTEMPLATEEX_HEAD* const ex = (DLGTEMPLATEEX_HEAD*) lParam;
    if (ex->signature == 0xFFFF)
      ex->style |= WS_THICKFRAME|WS_MAXIMIZEBOX;
    else
      ((DLGTEMPLATE*) lParam)->style |= WS_THICKFRAME|WS_MAXIMIZEBOX;
  }
  // Chain whatever was there, so a sheet brought up modeless keeps MFC's own callback.
  return (sheetCallback != NULL) ? sheetCallback(hWnd, message, lParam) : 0;
}

void CMainTab::AddControlPages()
{
  m_minSize.cx = m_minSize.cy = 0;    // OnInitDialog measures it
  // UnDefineDefaultProxy calls this again; chaining onto ourselves would recurse forever.
  if (m_psh.pfnCallback != SheetPreCreate) {
    sheetCallback = m_psh.pfnCallback;
    m_psh.pfnCallback = SheetPreCreate;
    m_psh.dwFlags |= PSH_USECALLBACK;
  }
  m_hIcon = httrack_icon;
  m_psh.dwFlags |= PSP_USEHICON;  // utiliser icône
  m_psh.dwFlags &= ~PSH_HASHELP;  // pas de bouton help
  m_psh.hIcon = m_hIcon;
  //m_psh.pszIcon = "test";

  // pas de "apply"
  this->m_psh.dwFlags|=PSH_NOAPPLYNOW;

  // Ajout des Control TAB dans la feuille principale (MainTab)
  AddPage(&m_option10);       /* Proxy */
  AddPage(&m_option7);        /* Filters */
  AddPage(&m_option5);        /* Limits */
  AddPage(&m_option4);        /* Flow Control */
  AddPage(&m_option1);        /* Links */
  AddPage(&m_option2);        /* Build */
  AddPage(&m_option8);        /* Spider */
  AddPage(&m_option11);       /* MIME types */
  AddPage(&m_option6);        /* Browser ID */
  AddPage(&m_option9);        /* Log, Index, cache */
  AddPage(&m_option3);        /* Expert */
}

void CMainTab::DefineDefaultProxy()
{
  while(GetPageCount()>0)
    RemovePage(0);
  AddPage(&m_option10);       /* Only proxy */
}

void CMainTab::UnDefineDefaultProxy() {
  AddControlPages();
}

BEGIN_MESSAGE_MAP(CMainTab, CPropertySheet)
//{{AFX_MSG_MAP(CMainTab)
ON_WM_QUERYDRAGICON()
ON_WM_SYSCOMMAND()
ON_WM_TIMER()
ON_WM_HELPINFO()
ON_WM_SIZE()
ON_WM_GETMINMAXINFO()
ON_WM_DESTROY()
//}}AFX_MSG_MAP
ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
ON_COMMAND(ID_HELP,OnHelpInfo2)
ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
//ON_BN_CLICKED(IDOK, OnOK)
//ON_BN_CLICKED(IDCANCEL, OnCancel)
ON_COMMAND(ID_APPLY_NOW,OnApplyNow)
ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMainTab message handlers

BOOL CMainTab::OnInitDialog()
{
  // IDM_ABOUTBOX must be in the system command range.
  //ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
  //ASSERT(IDM_ABOUTBOX < 0xF000);
  
  SetIcon(m_hIcon, TRUE);
  SetIcon(m_hIcon, FALSE);
  EnableToolTips(true);     // TOOL TIPS

/*
  CMenu* pSysMenu = GetSystemMenu(FALSE);
  if (pSysMenu != NULL)
  {
    CString strAboutMenu;
    strAboutMenu.LoadString(IDS_ABOUTBOX);
    if (!strAboutMenu.IsEmpty())
    {
      pSysMenu->AppendMenu(MF_SEPARATOR);
      pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
    }
  }
*/  
  // Chargement des préférences
  LoadPrefs();
  
  // Appliquer préférences
  Apply();
  
  int r = CPropertySheet::OnInitDialog();
  //SetActivePage(GetPageCount()-1);
  SetActivePage(0);

  /* Fallback for a sheet SheetPreCreate could not reach, and a no-op once it did: the
     thicker border comes out of the client area, so give that back. */
  CRect before, after, frame;
  GetClientRect(before);
  ModifyStyle(0, WS_THICKFRAME|WS_MAXIMIZEBOX);
  SetWindowPos(NULL, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
  GetClientRect(after);
  GetWindowRect(frame);
  SetWindowPos(NULL, 0, 0,
               frame.Width()  + before.Width()  - after.Width(),
               frame.Height() + before.Height() - after.Height(),
               SWP_NOMOVE|SWP_NOZORDER);
  GetWindowRect(frame);
  m_minSize = frame.Size();
  m_sheetLayout.Build(this);

  // mode modif à la volée
  return r;
}

void CMainTab::OnSize(UINT nType, int cx, int cy)
{
  CPropertySheet::OnSize(nType, cx, cy);
  m_sheetLayout.Apply(cx, cy);
}

/* OptPannel re-shows this sheet, so what was measured for one window must not reach the
   next: it clamps that window's size and is applied before Build can measure it. */
void CMainTab::OnDestroy()
{
  m_sheetLayout.Build(NULL);
  m_minSize.cx = m_minSize.cy = 0;
  CPropertySheet::OnDestroy();
}

void CMainTab::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
  CPropertySheet::OnGetMinMaxInfo(lpMMI);
  if (m_minSize.cx > 0) {
    lpMMI->ptMinTrackSize.x = m_minSize.cx;
    lpMMI->ptMinTrackSize.y = m_minSize.cy;
    /* Last word on the maximum as well: a sheet proc that means to stay fixed pins this
       to the size it computed, and the drag then has nowhere to go. */
    lpMMI->ptMaxTrackSize.x = max(lpMMI->ptMaxTrackSize.x, ::GetSystemMetrics(SM_CXMAXTRACK));
    lpMMI->ptMaxTrackSize.y = max(lpMMI->ptMaxTrackSize.y, ::GetSystemMetrics(SM_CYMAXTRACK));
  }
}

LRESULT CMainTab::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  const LRESULT r = CPropertySheet::WindowProc(message, wParam, lParam);
  m_sheetLayout.HandleMessage(message, wParam, lParam);
  return r;
}

HCURSOR CMainTab::OnQueryDragIcon()
{
  return (HCURSOR) m_hIcon;
}

BOOL CMainTab::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext)
{
  //removing the default DS_CONTEXT_HELP style
  //dwStyle= WS_SYSMENU | WS_POPUP | WS_CAPTION | DS_MODALFRAME | WS_VISIBLE;
  return CWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext);
}

void CMainTab::OnSysCommand(UINT nID, LPARAM lParam)
{
  /*if ((nID & 0xFFF0) == IDM_ABOUTBOX)
  {
    SetActivePage(GetPageCount()-1);    // Afficher informations sur le programme et affichant la dernière page des control TAB
 	}
  else
  {
  */
    CPropertySheet::OnSysCommand(nID, lParam);
  /*}
  */
}

// L'utilisateur a appuyé sur "Apply"
void CMainTab::OnApplyNow()
{
  EnableWindow(false);
  Default();
  ApplyAndSave();
  EnableWindow(true);
}

// Sauver et appliquer les préférences
void CMainTab::ApplyAndSave() {
  CWaitCursor wait;      // Afficher curseur sablier
  bool err=false;  // Erreur lors de l'écriture des paramètres
  
  // Appliquer les préférences
  Apply();
  
  // Sauver préférences
  CWinApp* pApp = AfxGetApp();
  //if (!pApp->WriteProfileInt("Driver", "DriverId",numero_driver))          // No du driver
  //  err=true;
 
  if (err)
    AfxMessageBox(LANG(LANG_DIAL2));
}

// Appliquer préférences
void CMainTab::Apply() {
  // Appliquer préférences
}

// Chargement des préférences
void CMainTab::LoadPrefs() {
  CWinApp* pApp = AfxGetApp();
  //n = pApp->GetProfileInt("Driver", "DriverId",0);   // No du driver
}

// Appel aide
void CMainTab::OnHelpInfo2() {
  (void)OnHelpInfo(NULL);
}

BOOL CMainTab::OnHelpInfo(HELPINFO* dummy) 
{
  //return CDialog::OnHelpInfo(pHelpInfo);
  //AfxGetApp()->WinHelp(0,HELP_FINDER);    // Index du fichier Hlp
  //LaunchHelp(pHelpInfo);

  if (this->GetActivePage() == &m_option1)
    HtsHelper->HelpTopic("opt-links");
  else if (this->GetActivePage() == &m_option5)
    HtsHelper->HelpTopic("opt-limits");
  else if (this->GetActivePage() == &m_option4)
    HtsHelper->HelpTopic("opt-flow-control");
  else if (this->GetActivePage() == &m_option7)
    HtsHelper->HelpTopic("opt-scan-rules");
  else if (this->GetActivePage() == &m_option2)
    HtsHelper->HelpTopic("opt-build");
  else if (this->GetActivePage() == &m_option8)
    HtsHelper->HelpTopic("opt-spider");
  else if (this->GetActivePage() == &m_option10)
    HtsHelper->HelpTopic("opt-proxy");
  else if (this->GetActivePage() == &m_option6)
    HtsHelper->HelpTopic("opt-browser-id");
  else if (this->GetActivePage() == &m_option9)
    HtsHelper->HelpTopic("opt-log-index-cache");
  else if (this->GetActivePage() == &m_option3)
    HtsHelper->HelpTopic("opt-experts-only");
  else if (this->GetActivePage() == &m_option11)
    HtsHelper->HelpTopic("opt-mime-types");
  else
    HtsHelper->Help();
  return true;
}


/*
// Capturer OK et Cancel
void CMainTab::OnOK( ) {
  // Sauver et appliquer préférences
  ApplyAndSave();
}
void CMainTab::OnCancel( ) {
  // Recharger préférences
  LoadPrefs();
}
*/



// ------------------------------------------------------------
// TOOL TIPS
//
// ajouter dans le .cpp:
// remplacer les deux <nom classe>:: par le nom de la classe::
// dans la message map, ajouter
// ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
// dans initdialog ajouter
// EnableToolTips(true);     // TOOL TIPS
//
// ajouter dans le .h:
// char* GetTip(int id);
// et en generated message map
// afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
const char* CMainTab::GetTip(int ID)
{
  switch(ID) {
    case IDOK:           return LANG(LANG_TIPOK); break;      
    case IDCANCEL:       return LANG(LANG_TIPCANCEL); break;
    case IDHELP:         return LANG_TIPHELP; break;
  }
  return "";
}
BOOL CMainTab::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
{
  TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
  UINT_PTR nID = pNMHDR->idFrom;
  if (pTTT->uFlags & TTF_IDISHWND)
  {
    // idFrom is actually the HWND of the tool
    nID = ::GetDlgCtrlID((HWND)nID);
    if(nID)
    {
      const char* st = GetTip((int) nID);
      if (st != NULL && *st) {
        pTTT->lpszText = (LPSTR)st;
        pTTT->hinst = AfxGetResourceHandle();
        return(TRUE);
      }
    }
  }
  return(FALSE);
}
// TOOL TIPS
// ------------------------------------------------------------

