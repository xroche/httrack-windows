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
// wizard2.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "wizard2.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
extern HICON httrack_icon;


/////////////////////////////////////////////////////////////////////////////
// wizard2 dialog


wizard2::wizard2(CWnd* pParent /*=NULL*/)
	: CDialog(wizard2::IDD, pParent)
{
	//{{AFX_DATA_INIT(wizard2)
	m_question = _T("");
	//}}AFX_DATA_INIT
}


void wizard2::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(wizard2)
	DDX_Text(pDX, IDC_question, m_question);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(wizard2, CDialog)
	//{{AFX_MSG_MAP(wizard2)
	ON_WM_TIMER()
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_HELP, OnHelp)
END_MESSAGE_MAP()



BOOL wizard2::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);  
	
  tm=SetTimer(WM_TIMER,1000,NULL);
  SetForegroundWindow();   // yop en premier plan!

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    //SetDlgItemText(,"");
    SetWindowTextCP(this, LANG(LANG_N1)); // "Question du wizard");
    SetDlgItemTextCP(this, IDCANCEL,LANG(LANG_N2)); // "NON");
  }


  return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void wizard2::OnTimer(UINT_PTR nIDEvent) 
{
  wflag=!wflag;
	FlashWindow(wflag);
	CDialog::OnTimer(nIDEvent);
}

void wizard2::OnDestroy() 
{
  if (tm!=0) {
    KillTimer(tm); 
    tm=-1; 
  }
	CDialog::OnDestroy();	
}
