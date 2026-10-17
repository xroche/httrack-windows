/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2026 Xavier Roche and other contributors

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


/* ------------------------------------------------------------ */
/* File: WinHTTrack subroutines:                                */
/*       the Accept headers box                                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#if !defined(AFX_ACCEPTOPTIONS_H__INCLUDED_)
#define AFX_ACCEPTOPTIONS_H__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "accept-rows.h"

/////////////////////////////////////////////////////////////////////////////
// CAcceptOptions dialog

/* The twelve Accept values, opened from the Browser ID page. They do not fit that page,
   which is why they live in a dialog of their own. */
class CAcceptOptions : public CDialog
{
// Construction
public:
	CAcceptOptions(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	enum { IDD = IDD_AcceptHeaders };
  /* One value per ACCEPT_ROWS row, as --accept will carry it. Empty asks for the
     automatic choice, which the box shows as LANG_ACCEPTAUTO rather than as a blank
     field, so no language ever writes that word into a profile. */
  CString m_accept[ACCEPT_ROW_COUNT];

  /* Returns the preset TEXT matches for ROW, worded as the tooltip shows it, or empty
     for anything else. A value two browsers both send names both. Exposed for --selftest. */
  static CString acceptPresetTip(int row, const CString &text);

  /* How many values a row can offer: one per browser the box knows, plus the catch-all. */
#define ACCEPT_ROW_OFFERS_MAX 4

  /* Writes the values ROW offers into OUT, at most MAX of them, and returns how many.
     Each value appears once however many browsers send it, and the catch-all comes last.
     Exposed for --selftest, which holds the list against the engine's own presets. */
  static int acceptRowOffers(int row, CString *out, int max);

  /* The row ID belongs to, or -1 for any other control. The tooltip asks a row for its
     own text through this, so a wrong answer would describe another row. Exposed for
     --selftest. */
  static int acceptRowOfId(int id);

// Implementation
protected:
  void OnHelpInfo2();
  const char* GetTip(int id);
  CString m_tip;   /* holds what GetTip() returns, since the tooltip keeps the pointer */

	afx_msg BOOL OnHelpInfo(HELPINFO* dummy);
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	DECLARE_MESSAGE_MAP()
};

#endif // !defined(AFX_ACCEPTOPTIONS_H__INCLUDED_)
