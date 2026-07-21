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
#if !defined(AFX_INSERTURL_H__2A8B8FE2_952E_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_INSERTURL_H__2A8B8FE2_952E_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// InsertUrl.h : header file
//

// Attention, définition existante également dans htslib.h
// (à modifier avec celle-ci)
#define POSTTOK "?>post"

/* Externe C */
extern "C" {
  #include "htscatchurl.h"
}
extern "C" {
  #include "HTTrackInterface.h"
	#include "httrack-library.h"
}

#include "CatchUrl.h"

/////////////////////////////////////////////////////////////////////////////
// CInsertUrl dialog

class CInsertUrl : public CDialog
{
// Construction
public:
	CInsertUrl(CWnd* pParent = NULL);   // standard constructor
  const char* GetTip(int id);
  //
  CString dest_path;
  //
  CCatchUrl dial;
  T_SOC soc;
  char adr_prox[1024];
  int port_prox;

// Dialog Data
	//{{AFX_DATA(CInsertUrl)
	enum { IDD = IDD_InsertUrl };
	CString	m_urllogin;
	CString	m_urlpass;
	CString	m_urladr;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CInsertUrl)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  void OnHelpInfo2();

	// Generated message map functions
	//{{AFX_MSG(CInsertUrl)
	virtual BOOL OnInitDialog();
	afx_msg void Oncapt();
	//}}AFX_MSG
  afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_INSERTURL_H__2A8B8FE2_952E_11D3_A2B3_0000E84E7CA1__INCLUDED_)
