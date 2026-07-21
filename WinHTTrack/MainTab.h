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
#if !defined(__MAINTAB_H_HSGFYZEGCHXHVCHD52485454857545)
#define __MAINTAB_H_HSGFYZEGCHXHVCHD52485454857545

// Tab Control Principal

// En-tête pour l'affichage des tabs
#include "OptionTab1.h"
#include "OptionTab2.h"
#include "OptionTab3.h"
#include "OptionTab4.h"
#include "OptionTab5.h"
#include "OptionTab6.h"
#include "OptionTab7.h"
#include "OptionTab8.h"
#include "OptionTab9.h"
#include "OptionTab10.h"
#include "OptionTab11.h"

class CMainTab : public CPropertySheet
{
  //DECLARE_DYNAMIC(CMainTab)
  
private:
  
  // Construction
public:
  CMainTab(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
  CMainTab(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
  
protected:
  void AddControlPages(void);
  
  // Attributes
public:
  // Déclaration des classes-dialog pour les différents Tab Control
  COptionTab1        m_option1;
  COptionTab2        m_option2;
  COptionTab3        m_option3;
  COptionTab4        m_option4;
  COptionTab5        m_option5;
  COptionTab6        m_option6;
  COptionTab7        m_option7;
  COptionTab8        m_option8;
  COptionTab9        m_option9;
  COptionTab10       m_option10;
  COptionTab11       m_option11;
  // Operations
public:
	int prec;
	int next;
	int cancel;
  
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CMainTab)
public:
  virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
  //}}AFX_VIRTUAL
  
  // Implementation
public:
  virtual ~CMainTab();
  virtual BOOL OnInitDialog();
  void ApplyAndSave();
  void Apply();
  void LoadPrefs();
  HICON m_hIcon;
  void DefineDefaultProxy();
  void UnDefineDefaultProxy();
  
  // Generated message map functions
protected:
  
  const char* GetTip(int id);
  //{{AFX_MSG(CMainTab)
  afx_msg HCURSOR OnQueryDragIcon();
  afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
  //}}AFX_MSG
	afx_msg void OnHelpInfo2();
  //afx_msg void OnOK( );
  //afx_msg void OnCancel( );
  afx_msg void OnApplyNow();
  // afx_msg LONG IconRestore(UINT a, LONG b); // LONG msg, LONG b,CPoint point) {
  afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
  //afx_msg LONG IconRestore(UINT a,LONG b);
  DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

#endif

