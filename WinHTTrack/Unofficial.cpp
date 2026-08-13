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

#include "stdafx.h"
#include "WinHTTrack.h"
#include "Shell.h"

#include "resource.h"
#include "SignatureCheck.h"
#include "Unofficial.h"

/* Deliberately not in lang.def: a warning about a repackaged build is worth nothing if
   whoever repackaged it can quietly replace the wording through a language file. */
static const char WhttUnofficialHeadline[] =
  "This copy of WinHTTrack was not published by the HTTrack project.";

static const char WhttUnofficialBody[] =
  "HTTrack is free software, and free of charge. Whoever supplied this copy has "
  "changed it, and may have added software you never asked for.\r\n\r\n"
  "HTTrack and WinHTTrack are the project's own names, and this build carries no "
  "endorsement from it. The official version is always at www.httrack.com.";

static BOOL WhttThemeColour(COLORREF wanted, COLORREF* colour)
{
	HIGHCONTRAST hc;

	memset(&hc, 0, sizeof(hc));
	hc.cbSize = sizeof(hc);
	if (SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0)
	    && (hc.dwFlags & HCF_HIGHCONTRASTON) != 0) {
		return FALSE;
	}
	*colour = wanted;
	return TRUE;
}

BOOL WhttWarningColour(COLORREF* colour)
{
	return WhttThemeColour(RGB(192, 0, 0), colour);
}

BOOL WhttSigLineColour(COLORREF* colour)
{
	switch (WhttSigOverall()) {
	case WHTT_SIG_TAMPERED:
	case WHTT_SIG_OTHERS:
		return WhttWarningColour(colour);
	case WHTT_SIG_NONE:
	case WHTT_SIG_UNKNOWN:
		/* Amber, not red: unverifiable is not the accusation that a wrong name is. */
		return WhttThemeColour(RGB(176, 88, 0), colour);
	default:
		return FALSE;
	}
}

CUnofficial::CUnofficial(CWnd* pParent)
	: CDialog(CUnofficial::IDD, pParent)
{
}

BEGIN_MESSAGE_MAP(CUnofficial, CDialog)
	ON_WM_CTLCOLOR()
	ON_BN_CLICKED(IDC_SIGVISIT, OnVisit)
END_MESSAGE_MAP()

BOOL CUnofficial::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetDlgItemText(IDC_SIGWARN, WhttUnofficialHeadline);
	SetDlgItemText(IDC_SIGWHO, WhttSigSummary());
	SetDlgItemText(IDC_SIGBODY, WhttUnofficialBody);
	MessageBeep(MB_ICONEXCLAMATION);
	return TRUE;
}

HBRUSH CUnofficial::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH brush = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	COLORREF colour;

	/* Colour on top of the wording, never instead of it: the sentence has to carry the
	   warning on a screen that cannot show the red at all. */
	if (nCtlColor == CTLCOLOR_STATIC && pWnd->GetDlgCtrlID() == IDC_SIGWARN
	    && WhttWarningColour(&colour)) {
		pDC->SetTextColor(colour);
	}
	return brush;
}

void CUnofficial::OnVisit()
{
	if (!ShellOpen("https://www.httrack.com", SW_RESTORE)) {
		AfxMessageBox("Cannot open a web browser for https://www.httrack.com");
	}
}
