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
// WinHTTrackDoc.cpp : implementation of the CWinHTTrackDoc class
//

#include "stdafx.h"
#include "WinHTTrack.h"

#include "WinHTTrackDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern "C" {
  #include "HTTrackInterface.h"
}

/* dialog0 */
#include "NewProj.h"
extern CNewProj* dialog0;

/* Main WizTab frame */
#include "WizTab.h"
extern CWizTab* this_CWizTab;

// dirtreeview
#include "DirTreeView.h"
extern CDirTreeView* this_DirTreeView;

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackDoc

IMPLEMENT_DYNCREATE(CWinHTTrackDoc, CDocument)

BEGIN_MESSAGE_MAP(CWinHTTrackDoc, CDocument)
	//{{AFX_MSG_MAP(CWinHTTrackDoc)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackDoc construction/destruction

CWinHTTrackDoc::CWinHTTrackDoc()
{
	// TODO: add one-time construction code here

}

CWinHTTrackDoc::~CWinHTTrackDoc()
{
}

BOOL CWinHTTrackDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)

	return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackDoc serialization

void CWinHTTrackDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackDoc diagnostics

#ifdef _DEBUG
void CWinHTTrackDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CWinHTTrackDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackDoc commands

BOOL CWinHTTrackDoc::OnSaveDocument(LPCTSTR lpszPathName) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	//return CDocument::OnSaveDocument(lpszPathName);
  //AfxMessageBox(lpszPathName);

  if (dialog0) {
    CString PathName=lpszPathName;
    int pos=PathName.ReverseFind('.');
    if (pos>=0) {
      if (PathName.Mid(pos)==".whtt") {
        if (dialog0->GetPath0()+".whtt" != LPCSTR(lpszPathName)) {
          switch(AfxMessageBox(LANG_G26b,MB_YESNOCANCEL)) {
          case IDYES:
            {
              CString path=lpszPathName;
              int pos=path.ReverseFind('.');
              if (pos>=0) {
                CString dir=path.Left(pos);           // enlever .whtt
                pos=path.ReverseFind('\\');
                dialog0->m_projname=dir.Mid(pos+1);
                dialog0->m_projpath=dir.Left(pos);
              }
            }
            break;
          case IDNO:
            break;
          default:
            return 0;
            break;
          }
        }
      }
    }
    
    Save_current_profile(0);
  }
 
  return 1;
}

BOOL CWinHTTrackDoc::OnOpenDocument(LPCTSTR lpszPathName) 
{
	/*if (!CDocument::OnOpenDocument(lpszPathName))
		return FALSE;
    */
  CFileStatus status;
  if (CFile::GetStatus(lpszPathName,status)) {
    CString PathName=lpszPathName;

    /* si répertoire, convertir d'abord en .whtt */
    if (status.m_attribute & 0x10 ) {       /* directory = 0x10 */
      PathName+=".whtt";
    }
    
    int pos=PathName.ReverseFind('.');
    if (pos>=0) {
      if (PathName.Mid(pos)==".whtt") {
        CString dir=PathName.Left(pos)+"\\";
        CString profile=dir+"hts-cache\\winprofile.ini";    
        Read_profile((char*) LPCTSTR(profile),1);
        //if (fexist((char*) LPCTSTR(profile))) {
        //} else {
          //AfxMessageBox(LANG(LANG_G26 /*"File not found!","Fichier introuvable!"*/));
          /*return FALSE;*/
        //}
		if (dialog0->m_projname.GetLength() > 0) {
          this_CWizTab->PressButton(PSBTN_NEXT);
          this_CWizTab->PressButton(PSBTN_NEXT);
		}
      } else {
        AfxMessageBox(LANG(LANG_G26 /*"File not found!","Fichier introuvable!"*/));
        return FALSE;
      }
    } else {
      AfxMessageBox(LANG(LANG_G26 /*"File not found!","Fichier introuvable!"*/));
      return FALSE;
    }
  } else {
    AfxMessageBox(LANG(LANG_G26 /*"File not found!","Fichier introuvable!"*/));
    return FALSE;
  }
  
  return TRUE;
}

void CWinHTTrackDoc::OnCloseDocument() 
{
	// TODO: Add your specialized code here and/or call the base class
	CDocument::OnCloseDocument();
}
