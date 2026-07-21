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
#if !defined(AFX_NEWPROJ_H__B87C5B22_80E5_11D3_A2B2_0000E84E7CA1__INCLUDED_)
#define AFX_NEWPROJ_H__B87C5B22_80E5_11D3_A2B2_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#include "resource.h"
#endif // _MSC_VER >= 1000
// NewProj.h : header file
//


#include "EasyDropTarget.h"

/////////////////////////////////////////////////////////////////////////////
// CNewProj dialog

class CNewProj : public CPropertyPage
{
	DECLARE_DYNCREATE(CNewProj)

    // Construction
public:
	CNewProj();                       // standard constructor
	~CNewProj();                      // standard destructor
  CString GetName();                // nom complet projet
  CString GetPath();                // path complet projet
  CString GetPath0();               // path complet projet sans dernier slash
  CString GetBasePath();            // base path

// Dialog Data
	//{{AFX_DATA(CNewProj)
	enum { IDD = IDD_NewProj };
	CComboBox	m_ctl_projcateg;
	CComboBox	m_ctl_projname;
	CString	m_projname;
	CString	m_projpath;
	CString	m_projcateg;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNewProj)
	protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  BOOL can_click_next;
  const char* GetTip(int id);
  void OnHelpInfo2();
  void Changeprojname(CString stl);
	// Generated message map functions
	//{{AFX_MSG(CNewProj)
	afx_msg void Onbr();
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeprojpath();
	afx_msg void OnChangeprojname();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSelchangeprojname();
	//}}AFX_MSG
  virtual BOOL OnKillActive( );
  virtual BOOL OnQueryCancel( );
  virtual LRESULT OnWizardNext();
  virtual BOOL OnSetActive( );
	afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
  afx_msg LRESULT DragDropText(WPARAM wParam,LPARAM lParam);
	DECLARE_MESSAGE_MAP()

private: 
  CEasyDropTarget* drag;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NEWPROJ_H__B87C5B22_80E5_11D3_A2B2_0000E84E7CA1__INCLUDED_)
