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
#if !defined(AFX_ABOUT_H__E4D816E1_19A9_11D2_A290_60D144C12802__INCLUDED_)
#define AFX_ABOUT_H__E4D816E1_19A9_11D2_A290_60D144C12802__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// about.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// Cabout dialog

class Cabout : public CDialog
{
// Construction
public:
	Cabout(CWnd* pParent = NULL);   // standard constructor
  const char* GetTip(int id);

// Dialog Data
	//{{AFX_DATA(Cabout)
	enum { IDD = IDD_ABOUT };
	CStatic	m_splash;
	CComboBox	m_ctl_lang;
	CString	m_infover;
	int		m_lang;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(Cabout)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  void OnHelpInfo2();
  void setlang();

  HCURSOR currentCurs;

	// Generated message map functions
	//{{AFX_MSG(Cabout)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnSelchangelang();
	afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ABOUT_H__E4D816E1_19A9_11D2_A290_60D144C12802__INCLUDED_)
