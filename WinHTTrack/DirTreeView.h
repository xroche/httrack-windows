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
#if !defined(AFX_DIRTREEVIEW_H__DFB224E0_828F_426D_A9A3_471D7A2F5108__INCLUDED_)
#define AFX_DIRTREEVIEW_H__DFB224E0_828F_426D_A9A3_471D7A2F5108__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DirTreeView.h : header file
//

#include <afxcview.h>
#include "TreeViewToolTip.h"

/////////////////////////////////////////////////////////////////////////////
// CDirTreeView view

class CDirTreeView : public CTreeView
{
protected:
	CDirTreeView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CDirTreeView)
protected:
  UINT_PTR timer;
  HANDLE    whandle[1024];
  HTREEITEM pos_whandle[1024];
  int       count_whandle;
  void StartTimer();
  void StopTimer();
  //
  void BuildTrackHandles();
  void DestroyTrackHandles();
  void DoTrackHandles();

// Attributes
public:
	CTreeViewToolTip m_TreeViewToolTip;
  //
  CString refreshPath;      /* callback */
  CString docType;          /* document type (ex: .doc) */
  //
  int redraw_in_progress;

// Operations
public:
  void RefreshTree();
  BOOL EnsureVisible(CString path);
  BOOL EnsureVisibleBl(CString path);
  void ResetTree();
  void WaitThreads();

protected:
  BOOL RefreshDir(HTREEITEM position,BOOL nohide=FALSE);
  // First item in the sibling chain from first whose name is lname (must be lowercase);
  // NULL if none matches.
  HTREEITEM FindSibling(HTREEITEM first,const CString& lname);
  void RestoreVisibles(HTREEITEM position,CString& backup_visibles);
  void RefreshPos(HTREEITEM position);
  CString GetItemPath(HTREEITEM position);
  HTREEITEM GetPathItem(CString path,BOOL open=FALSE,BOOL nofail=FALSE,BOOL nohide=FALSE,HTREEITEM rootposition=NULL);
  CString GetItemName(HTREEITEM position);
  //
  CImageList imagelist;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDirTreeView)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CDirTreeView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CDirTreeView)
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnItemexpanding(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclk(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRclick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEndlabeledit(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydown(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnSelchanged(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CDirTreeViewShellEx
/*
class CDirTreeViewShellEx : public CWinThread
{
  DECLARE_DYNAMIC(CDirTreeViewShellEx)
public:
  CString File;
  virtual BOOL InitInstance( );
};
*/
UINT CDirTreeViewShellEx( LPVOID pP );
UINT CDirTreeViewRefresh( LPVOID pP );

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.





#endif // !defined(AFX_DIRTREEVIEW_H__DFB224E0_828F_426D_A9A3_471D7A2F5108__INCLUDED_)
