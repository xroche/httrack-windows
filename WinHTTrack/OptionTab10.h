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
#if !defined(AFX_OPTIONTAB10_H__758B3902_A9A6_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_OPTIONTAB10_H__758B3902_A9A6_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// OptionTab10.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionTab10 dialog

class COptionTab10 : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionTab10)

// Construction
public:
	COptionTab10();
	~COptionTab10();
  const char* GetTip(int id);
  int modify;
  int prox_status;
  int m_proxytype;   /* 0 = HTTP proxy, 1 = SOCKS5 proxy, 2 = HTTP CONNECT tunnel */
  char ProxyDetectBuff[16][1024];
  CString ProxyDetectName[16];

// Dialog Data
	//{{AFX_DATA(COptionTab10)
	enum { IDD = IDD_OPTION10 };
	CComboBox	m_ctl_prox;
	CButton	m_ctl_pwdhide;
	CButton	m_ctl_ftpprox;
	CEdit	m_ctl_portprox;
	CStatic	m_ctl_proxtitle;
	CString	m_proxy;
	CString	m_port;
	BOOL	m_ftpprox;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionTab10)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COptionTab10)
	afx_msg void Onproxyconfigure();
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeprox();
	afx_msg void OnPwdhide();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
  afx_msg LRESULT ProxySearch0(WPARAM wParam,LPARAM lParam);
  afx_msg LRESULT ProxySearch1(WPARAM wParam,LPARAM lParam);
  afx_msg LRESULT ProxySearch2(WPARAM wParam,LPARAM lParam);
  afx_msg LRESULT ProxySearch3(WPARAM wParam,LPARAM lParam);
  afx_msg LRESULT ProxySearch4(WPARAM wParam,LPARAM lParam);
  afx_msg LRESULT ProxySearch5(WPARAM wParam,LPARAM lParam);
  afx_msg LRESULT ProxySearch6(WPARAM wParam,LPARAM lParam);
  afx_msg LRESULT ProxySearch7(WPARAM wParam,LPARAM lParam);
  afx_msg LRESULT ProxySearch8(WPARAM wParam,LPARAM lParam);
  afx_msg LRESULT ProxySearch9(WPARAM wParam,LPARAM lParam);
  LRESULT ProxySearch(int id,WPARAM wParam,LPARAM lParam);
  DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONTAB10_H__758B3902_A9A6_11D3_A2B3_0000E84E7CA1__INCLUDED_)
