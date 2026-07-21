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
// BuildOptions.cpp : implementation file
//

#include "stdafx.h"
#include "Shell.h"
#include "BuildOptions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Helper
extern LaunchHelp* HtsHelper;


/////////////////////////////////////////////////////////////////////////////
// CBuildOptions dialog


CBuildOptions::CBuildOptions(CWnd* pParent /*=NULL*/)
	: CDialog(CBuildOptions::IDD, pParent)
{
	//{{AFX_DATA_INIT(CBuildOptions)
	m_BuildString = _T("");
	//}}AFX_DATA_INIT
}


void CBuildOptions::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CBuildOptions)
	DDX_Text(pDX, IDC_BuildString, m_BuildString);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CBuildOptions, CDialog)
	//{{AFX_MSG_MAP(CBuildOptions)
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
  ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
  ON_COMMAND(ID_HELP,OnHelpInfo2)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBuildOptions message handlers

// Appel aide
void CBuildOptions::OnHelpInfo2() {
  (void) OnHelpInfo(NULL);
}

BOOL CBuildOptions::OnHelpInfo(HELPINFO* dummy) 
{
  //return CDialog::OnHelpInfo(pHelpInfo);
  HtsHelper->Help();
  return true;
  //AfxGetApp()->WinHelp(0,HELP_FINDER);    // Index du fichier Hlp
  //return true;
}

BOOL CBuildOptions::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    SetWindowTextCP(this, LANG(LANG_Q1)); // struct
    SetDlgItemTextCP(this, IDOK,LANG(LANG_OK ));
    SetDlgItemTextCP(this, IDC_STATIC_options,LANG_O2);
    SetDlgItemTextCP(this, IDCANCEL,LANG(LANG_CANCEL));  // cancel 
  }
  SetDlgItemTextCP(this, IDC_STATIC_bo1,LANG(LANG_Q2));
  SetDlgItemTextCP(this, IDC_STATIC_bo2,LANG(LANG_Q3));

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
