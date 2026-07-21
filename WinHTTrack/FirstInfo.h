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
#if !defined(AFX_FirstInfo_H__DC893229_C7D6_448C_860C_54F4E35FFA84__INCLUDED_)
#define AFX_FirstInfo_H__DC893229_C7D6_448C_860C_54F4E35FFA84__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#include "resource.h"
#endif // _MSC_VER > 1000
// FirstInfo.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CFirstInfo dialog

class CFirstInfo : public CPropertyPage
{
	DECLARE_DYNCREATE(CFirstInfo)

// Construction
public:
	CFirstInfo();
	~CFirstInfo();

// Dialog Data
	//{{AFX_DATA(CFirstInfo)
	enum { IDD = IDD_FirstInfo };
	CStatic	m_splash;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CFirstInfo)
	public:
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CFirstInfo)
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	//}}AFX_MSG
	BOOL OnInitDialog();
  BOOL OnSetActive();
  BOOL OnQueryCancel();
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FirstInfo_H__DC893229_C7D6_448C_860C_54F4E35FFA84__INCLUDED_)
