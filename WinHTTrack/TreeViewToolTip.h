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
#if !defined(AFX_TREEVIEWTOOLTIP_H__C5F45954_56A7_49B9_84B2_9C8BEBC46D08__INCLUDED_)
#define AFX_TREEVIEWTOOLTIP_H__C5F45954_56A7_49B9_84B2_9C8BEBC46D08__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TreeViewToolTip.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CTreeViewToolTip window

class CTreeViewToolTip : public CToolTipCtrl
{
// Construction
public:
	CTreeViewToolTip();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTreeViewToolTip)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CTreeViewToolTip();

	// Generated message map functions
protected:
  const char* GetTip(int id);
	//{{AFX_MSG(CTreeViewToolTip)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TREEVIEWTOOLTIP_H__C5F45954_56A7_49B9_84B2_9C8BEBC46D08__INCLUDED_)
