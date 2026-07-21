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
#if !defined(AFX_DIALOGHTMLHELP_H__FED0CE81_AB10_11D3_A2B3_0000E84E7CA1__INCLUDED_)
#define AFX_DIALOGHTMLHELP_H__FED0CE81_AB10_11D3_A2B3_0000E84E7CA1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DialogHtmlHelp.h : header file
//

#include "resource.h"
//#include "LaunchHelpBase.h"
#include "HtmlCtrl.h"

// type callback
typedef void (* Helper_CallBack ) (CWnd*);
typedef void (* OnSize_CallBack ) (CWnd*,UINT nType, int cx, int cy); 

/////////////////////////////////////////////////////////////////////////////
// CDialogHtmlHelp dialog

class CDialogHtmlHelp : public CDialog
{
// Construction
public:
  void Go(CString st);
	CDialogHtmlHelp(CWnd* pParent = NULL);   // standard constructor
  Helper_CallBack callback;
  OnSize_CallBack callbackOnSize;
  const char* GetTip(int id);
  //
  CString page;
protected:
	CHtmlCtrl m_page;
	CToolBar     m_wndToolBar;
  char home[1024];
  char home_dir[1024];
//private:
//  LaunchHelpBase base;

// Dialog Data
	//{{AFX_DATA(CDialogHtmlHelp)
	enum { IDD = IDD_HtmlHelp };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDialogHtmlHelp)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL
	afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );

// Implementation
protected:
  void OnHelpInfo2();

	// Generated message map functions
	//{{AFX_MSG(CDialogHtmlHelp)
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnBack();
	afx_msg void OnForward();
	afx_msg void OnStop();
	afx_msg void OnReload();
	afx_msg void OnHome();
	afx_msg void OnFilePrint();
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DIALOGHTMLHELP_H__FED0CE81_AB10_11D3_A2B3_0000E84E7CA1__INCLUDED_)
