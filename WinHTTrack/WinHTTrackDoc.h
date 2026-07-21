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
// WinHTTrackDoc.h : interface of the CWinHTTrackDoc class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_WINHTTRACKDOC_H__2DCA7A1F_3A25_4E86_A859_31511341823A__INCLUDED_)
#define AFX_WINHTTRACKDOC_H__2DCA7A1F_3A25_4E86_A859_31511341823A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


class CWinHTTrackDoc : public CDocument
{
protected: // create from serialization only
	CWinHTTrackDoc();
	DECLARE_DYNCREATE(CWinHTTrackDoc)

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWinHTTrackDoc)
	public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);
  virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
	virtual void OnCloseDocument();
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CWinHTTrackDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CWinHTTrackDoc)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WINHTTRACKDOC_H__2DCA7A1F_3A25_4E86_A859_31511341823A__INCLUDED_)
