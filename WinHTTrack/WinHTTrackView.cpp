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
// WinHTTrackView.cpp : implementation of the CWinHTTrackView class
//

#include "stdafx.h"
#include "WinHTTrack.h"

#include "WinHTTrackDoc.h"
#include "WinHTTrackView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackView

IMPLEMENT_DYNCREATE(CWinHTTrackView, CView)

BEGIN_MESSAGE_MAP(CWinHTTrackView, CView)
	//{{AFX_MSG_MAP(CWinHTTrackView)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackView construction/destruction

CWinHTTrackView::CWinHTTrackView()
{
	// TODO: add construction code here

}

CWinHTTrackView::~CWinHTTrackView()
{
}

BOOL CWinHTTrackView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CView::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackView drawing

void CWinHTTrackView::OnDraw(CDC* pDC)
{
	CWinHTTrackDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	// TODO: add draw code for native data here
}

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackView diagnostics

#ifdef _DEBUG
void CWinHTTrackView::AssertValid() const
{
	CView::AssertValid();
}

void CWinHTTrackView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CWinHTTrackDoc* CWinHTTrackView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CWinHTTrackDoc)));
	return (CWinHTTrackDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackView message handlers
