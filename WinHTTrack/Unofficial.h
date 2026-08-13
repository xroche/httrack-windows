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
/*       "not published by us" warning                          */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef WINHTTRACK_UNOFFICIAL_H
#define WINHTTRACK_UNOFFICIAL_H

#include "resource.h"

/* The colour to draw a signature warning in. FALSE under a high-contrast theme, where
   a fixed red can come out unreadable on the theme's own background: the wording has
   to carry the warning by itself anyway. */
BOOL WhttWarningColour(COLORREF* colour);

/* The colour for the summary line, by verdict: red where it accuses, amber where it
   only cannot vouch. FALSE for a verified copy, leaving the dialog's own colour. */
BOOL WhttSigLineColour(COLORREF* colour);

/* Shown once per run when the binaries carry somebody else's signature, or none of
   ours any more. It informs and then gets out of the way: the program is free software
   and a modified build has every right to run, it just may not pass itself off as
   HTTrack. */
class CUnofficial : public CDialog
{
public:
	CUnofficial(CWnd* pParent = NULL);

	enum { IDD = IDD_UNOFFICIAL };

protected:
	virtual BOOL OnInitDialog();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnVisit();
	DECLARE_MESSAGE_MAP()
};

#endif
