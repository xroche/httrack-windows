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
#if !defined(AFX_OPTIONTAB3_H__E6FA3FE4_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_OPTIONTAB3_H__E6FA3FE4_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// OptionTab3.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionTab3 dialog

class COptionTab3 : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionTab3)

// Construction
public:
	COptionTab3();
	~COptionTab3();
  int modify;
  const char* GetTip(int id);


// Dialog Data
	//{{AFX_DATA(COptionTab3)
	enum { IDD = IDD_OPTION3 };
	CComboBox	m_ctl_travel3;
	CComboBox	m_ctl_travel2;
	CComboBox	m_ctl_travel;
	CComboBox	m_ctl_filter;
	int		m_filter;
	int		m_travel;
	int		m_travel2;
	BOOL	m_windebug;
	BOOL	m_cache;
	int		m_travel3;
	CString	m_stripquery;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionTab3)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COptionTab3)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONTAB3_H__E6FA3FE4_A5B5_11D3_A2B3_0000E84E7CA1__INCLUDED_)
