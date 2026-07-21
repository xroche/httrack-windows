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
// FirstInfo.cpp : implementation file
//

#include "stdafx.h"
#include "winhttrack.h"
#include "FirstInfo.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/* Main WizTab frame */
#include "WizTab.h"
extern CWizTab* this_CWizTab;

/* Main splitter frame */
#include "DialogContainer.h"
#include "splitter.h"
extern CSplitterFrame* this_CSplitterFrame;

/* DirTreeView */
#include "DirTreeView.h"
extern CDirTreeView* this_DirTreeView;


/////////////////////////////////////////////////////////////////////////////
// CFirstInfo property page

IMPLEMENT_DYNCREATE(CFirstInfo, CPropertyPage)

CFirstInfo::CFirstInfo() : CPropertyPage(CFirstInfo::IDD)
{
	//{{AFX_DATA_INIT(CFirstInfo)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CFirstInfo::~CFirstInfo()
{
}

void CFirstInfo::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFirstInfo)
	DDX_Control(pDX, IDC_SPLASH, m_splash);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CFirstInfo, CPropertyPage)
	//{{AFX_MSG_MAP(CFirstInfo)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFirstInfo message handlers

BOOL CFirstInfo::OnSetActive( ) {
  WHTT_LOCATION("FirstInfo");
  this_CWizTab->SetWizardButtons(PSWIZB_NEXT);
  SetDlgItemTextCP(this_CWizTab,IDCANCEL,LANG_QUIT);
  this_DirTreeView->ResetTree();
  this_CSplitterFrame->EnableSaveEntries(TRUE);
  //GetParent()->GetDlgItem(IDCANCEL)->ModifyStyle(0,WS_DISABLED);
  return 1;
}

BOOL CFirstInfo::OnQueryCancel( ) {
  //  if (AfxMessageBox(LANG(LANG_J1),MB_OKCANCEL)==IDOK) {
  /* Envoyer un WM_CLOSE à notre fenêtre principale */
  GetMainWindow()->SendMessage(WM_CLOSE,0,0);
  //  }
  return FALSE;
}

BOOL CFirstInfo::OnInitDialog() 
{

	CPropertyPage::OnInitDialog();
	EnableToolTips(true);     // TOOL TIPS

  WINDOWPLACEMENT wp;
  m_splash.GetWindowPlacement(&wp);
  wp.rcNormalPosition.right=wp.rcNormalPosition.left+300+1;
  wp.rcNormalPosition.bottom=wp.rcNormalPosition.top+69+1; 
  m_splash.SetWindowPlacement(&wp);

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    //SetDlgItemText(,"");
    
    SetDlgItemTextCP(this,IDC_STATIC_welcome, LANG_Y1);
  }

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


// !! COPIE DE ABOUT.CPP !!
void CFirstInfo::OnMouseMove(UINT nFlags, CPoint point) 
{
  int id=0;
  CWnd* w=ChildWindowFromPoint(point);
  if (w)
    id=w->GetDlgCtrlID();

  // Select
  switch(id) {
  case 0: break;
  case IDC_SPLASH:
    this->ClientToScreen(&point);
    w->ScreenToClient(&point);
    HCURSOR curs=NULL;
    if (
         (point.y>=120) && (point.y<=140)
      || (point.y<=80)
      || (point.y>=100) && (point.y<=110)
      ) {
      curs=AfxGetApp()->LoadCursor(IDC_CURSWWW);
    } else {
      curs=AfxGetApp()->LoadStandardCursor(IDC_ARROW);
    }
    if (curs) {
      //if (curs != currentCurs) {
        SetCursor(curs);
        // currentCurs=curs;
      //}
    }
  }  

	CDialog::OnMouseMove(nFlags, point);
}

void CFirstInfo::OnLButtonDown(UINT nFlags, CPoint point) 
{
  int id=0;
  CWnd* w=ChildWindowFromPoint(point);
  if (w)
    id=w->GetDlgCtrlID();

  // Select
  switch(id) {
    case 0: break;
    case IDC_SPLASH:
      this->ClientToScreen(&point);
      w->ScreenToClient(&point);
    if ((point.y>=100) && (point.y<=110) || (point.y<=80)) {
      if (!ShellOpen("http://www.httrack.com", SW_RESTORE)) {
        AfxMessageBox("Cannot open a web browser for http://www.httrack.com");
      }
    }
    break;
  }

  CDialog::OnLButtonDown(nFlags, point);
}
// !! FIN COPIE DE ABOUT.CPP !!

