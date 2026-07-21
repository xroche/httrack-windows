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
#if !defined(AFX_CATCHURL_H__589FEA02_D671_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_CATCHURL_H__589FEA02_D671_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CatchUrl.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CCatchUrl dialog

class CCatchUrl : public CDialog
{
// Construction
public:
	CCatchUrl(CWnd* pParent = NULL);   // standard constructor
  const char* GetTip(int id);

// Dialog Data
	//{{AFX_DATA(CCatchUrl)
	enum { IDD = IDD_CatchUrl };
	CString	m_info;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCatchUrl)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CCatchUrl)
	afx_msg void OnClose();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CATCHURL_H__589FEA02_D671_11D3_A2B3_0000E84E7CA1__INCLUDED_)
