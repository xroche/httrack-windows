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
#if !defined(AFX_WIZARD2_H__1D129B84_1981_11D2_A290_60D144C12802__INCLUDED_)
#define AFX_WIZARD2_H__1D129B84_1981_11D2_A290_60D144C12802__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// wizard2.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// wizard2 dialog

class wizard2 : public CDialog
{
  // gestion timer flash
  bool wflag;
  UINT_PTR tm;

// Construction
public:
	wizard2(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(wizard2)
	enum { IDD = IDD_wizard_yn };
	CString	m_question;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(wizard2)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(wizard2)
	virtual BOOL OnInitDialog();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WIZARD2_H__1D129B84_1981_11D2_A290_60D144C12802__INCLUDED_)
