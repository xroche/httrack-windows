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
#if !defined(AFX_HTMLHELP_H__CA6ABCFD_0D37_4DC2_A214_AD0BB7AFBBD5__INCLUDED_)
#define AFX_HTMLHELP_H__CA6ABCFD_0D37_4DC2_A214_AD0BB7AFBBD5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// HTMLHelp.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CHTMLHelp html view

#ifndef __AFXEXT_H__
#include <afxext.h>
#endif
#include <afxhtml.h>

class CHTMLHelp : public CHtmlView
{
public:
	CHTMLHelp();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CHTMLHelp)

// html Data
public:
	//{{AFX_DATA(CHTMLHelp)
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CHTMLHelp)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CHTMLHelp();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
	//{{AFX_MSG(CHTMLHelp)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_HTMLHELP_H__CA6ABCFD_0D37_4DC2_A214_AD0BB7AFBBD5__INCLUDED_)
