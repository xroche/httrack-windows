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
#if !defined(AFX_CEasyDropTarget_H__5EBE1984_98CD_11D2_A2B1_0000E84E7CA1__INCLUDED_)
#define AFX_CEasyDropTarget_H__5EBE1984_98CD_11D2_A2B1_0000E84E7CA1__INCLUDED_

#include <afxole.h>

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// CEasyDropTarget.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CEasyDropTarget document

static int ReadString(CArchive& ar, char* pString, int nMaxLen);

class CEasyDropTarget : public COleDropTarget
{
public: 
  CEasyDropTarget();
  CEasyDropTarget(CWnd* wnd);
  BOOL IsRegistered();
  void SetTextCallback(UINT msg);
  //
  static char** StringToArray(CString st);
  static int ReleaseStringToArray(char** st);

  // OLE
  DROPEFFECT OnDragEnter( CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point ); 
  DROPEFFECT OnDragOver( CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point ); 
  DROPEFFECT OnDragScroll( CWnd* pWnd, DWORD dwKeyState, CPoint point );     
  BOOL OnDrop( CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point ); 
  CString DoImportText(CArchive &ar);
  CString DoImportFile(HDROP hDropInfo);
private:
  BOOL registered;
  void SendWndText(CString DroppedData,CLIPFORMAT cfFormat);
  int DataToString(COleDataObject* pDataObject,CString &DroppedData);
  //
  CWnd* wnd;
  int cedt_text;
};


#endif // !defined(AFX_CEasyDropTarget_H__5EBE1984_98CD_11D2_A2B1_0000E84E7CA1__INCLUDED_)
