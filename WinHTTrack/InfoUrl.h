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
#if !defined(AFX_INFOURL_H__FF725966_B6BB_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_INFOURL_H__FF725966_B6BB_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// InfoUrl.h : header file
//

extern int termine;

/////////////////////////////////////////////////////////////////////////////
// CInfoUrl dialog

class CInfoUrl : public CDialog
{
// Construction
public:
	CInfoUrl(CWnd* pParent = NULL);   // standard constructor
  int id;
// Dialog Data
	//{{AFX_DATA(CInfoUrl)
	enum { IDD = IDD_InfoUrl };
	CComboBox	m_ctl_backlist;
	CProgressCtrl	m_slider;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CInfoUrl)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  UINT_PTR timer;
  void StartTimer();
  void StopTimer();

	// Generated message map functions
	//{{AFX_MSG(CInfoUrl)
	virtual BOOL OnInitDialog();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnClose();
	afx_msg void OnFreeze();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSelchangebacklist();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_INFOURL_H__FF725966_B6BB_11D3_A2B3_0000E84E7CA1__INCLUDED_)
