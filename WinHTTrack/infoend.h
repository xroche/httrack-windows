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
#if !defined(AFX_INFOEND_H__B057B1C2_A192_11D2_A2B1_0000E84E7CA1__INCLUDED_)
#define AFX_INFOEND_H__B057B1C2_A192_11D2_A2B1_0000E84E7CA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// infoend.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// Cinfoend dialog

class Cinfoend : public CPropertyPage
{
public:
	Cinfoend();           // protected constructor used by dynamic creation
	~Cinfoend();           // protected constructor used by dynamic creation
  DECLARE_DYNCREATE(Cinfoend)
    // Construction
public:
	//Cinfoend(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(Cinfoend)
	enum { IDD = IDD_fin };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(Cinfoend)
	public:
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  const char* GetTip(int id);
  void OnHelpInfo2();
  void OnBye();
  UINT_PTR tm;

	// Generated message map functions
	//{{AFX_MSG(Cinfoend)
	virtual BOOL OnInitDialog();
	afx_msg void Onlog();
	afx_msg void Onbrowse();
	afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
  virtual BOOL OnSetActive( );
  virtual BOOL OnWizardFinish( );
	DECLARE_MESSAGE_MAP()
  BOOL OnQueryCancel( );
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_INFOEND_H__B057B1C2_A192_11D2_A2B1_0000E84E7CA1__INCLUDED_)
