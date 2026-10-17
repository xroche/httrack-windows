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

#include "stdafx.h"
#include "Shell.h"
#include "AcceptOptions.h"
#include "DarkMode.h"
extern "C" {
  #include "HTTrackInterface.h"
};

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Helper
extern LaunchHelp* HtsHelper;

/* Each row's value combo and the label beside it, in ACCEPT_ROWS order. */
#define ACCEPT_DIALOG_ROWS(ROW)                                                \
  ROW(IDC_accept_default, IDC_STATIC_accept_default, LANG_ACCEPTDEFAULT)       \
  ROW(IDC_accept_document, IDC_STATIC_accept_document, LANG_ACCEPTDOCUMENT)    \
  ROW(IDC_accept_iframe, IDC_STATIC_accept_iframe, LANG_ACCEPTIFRAME)          \
  ROW(IDC_accept_frame, IDC_STATIC_accept_frame, LANG_ACCEPTFRAME)             \
  ROW(IDC_accept_image, IDC_STATIC_accept_image, LANG_ACCEPTIMAGE)             \
  ROW(IDC_accept_style, IDC_STATIC_accept_style, LANG_ACCEPTSTYLE)             \
  ROW(IDC_accept_script, IDC_STATIC_accept_script, LANG_ACCEPTSCRIPT)          \
  ROW(IDC_accept_font, IDC_STATIC_accept_font, LANG_ACCEPTFONT)                \
  ROW(IDC_accept_audio, IDC_STATIC_accept_audio, LANG_ACCEPTAUDIO)             \
  ROW(IDC_accept_video, IDC_STATIC_accept_video, LANG_ACCEPTVIDEO)             \
  ROW(IDC_accept_object, IDC_STATIC_accept_object, LANG_ACCEPTOBJECT)          \
  ROW(IDC_accept_embed, IDC_STATIC_accept_embed, LANG_ACCEPTEMBED)

#define ACCEPT_DIALOG_VALUE_ID(value, label, lang) value,
#define ACCEPT_DIALOG_LABEL_ID(value, label, lang) label,
#define ACCEPT_DIALOG_LABEL(value, label, lang) LANG(lang),

static const int acceptValueIds[] = { ACCEPT_DIALOG_ROWS(ACCEPT_DIALOG_VALUE_ID) };
static const int acceptLabelIds[] = { ACCEPT_DIALOG_ROWS(ACCEPT_DIALOG_LABEL_ID) };
static const char *const acceptKinds[] = { ACCEPT_ROWS(ACCEPT_ROW_KIND) };

/* A row missing here would leave a value nothing in the box can reach. */
static_assert(sizeof(acceptValueIds) / sizeof(acceptValueIds[0]) == ACCEPT_ROW_COUNT,
              "ACCEPT_DIALOG_ROWS and ACCEPT_ROWS disagree on the rows");

/////////////////////////////////////////////////////////////////////////////
// CAcceptOptions dialog

CAcceptOptions::CAcceptOptions(CWnd* pParent /*=NULL*/)
	: CDialog(CAcceptOptions::IDD, pParent)
{
}

BEGIN_MESSAGE_MAP(CAcceptOptions, CDialog)
	ON_WM_HELPINFO()
	ON_WM_CTLCOLOR()
  ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipNotify )
  ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
  ON_COMMAND(ID_HELP,OnHelpInfo2)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAcceptOptions message handlers

// Appel aide
void CAcceptOptions::OnHelpInfo2() {
  (void) OnHelpInfo(NULL);
}

BOOL CAcceptOptions::OnHelpInfo(HELPINFO* dummy)
{
  HtsHelper->Help();
  return true;
}

/* A row offers each value once, the catch-all last. The default row gets no browser
   value, because neither browser has one outside a kind. */
static void fillAcceptCombo(CComboBox *combo, const char *kind)
{
  static const char *const browsers[] = { "chrome", "firefox", "legacy", NULL };

  for(int b=0 ; browsers[b] != NULL ; b++) {
    const char *const preset = hts_accept_preset(browsers[b], kind);

    if (preset != NULL && *preset != '\0' && combo->FindStringExact(-1, preset) == CB_ERR)
      combo->AddString(preset);
  }
  if (combo->FindStringExact(-1, "*/*") == CB_ERR)
    combo->AddString("*/*");
}

BOOL CAcceptOptions::OnInitDialog()
{
	CDialog::OnInitDialog();
	WhttDarkInitWindow(this);
  EnableToolTips(true);     // TOOL TIPS

  if (LANG_T(-1)) {    // Patcher en français
    SetDlgItemTextLang(this, IDOK, LANG(LANG_OK));
    SetDlgItemTextLang(this, IDCANCEL, LANG(LANG_CANCEL));
  }
  /* Everything below sets a caption, so it has to stay under that block. */
  {
    const char *const title = LANG(LANG_ACCEPT);
    if (title[0] != '\0')    /* else the .rc caption stays, as SetDlgItemTextLang() does */
      SetWindowTextCP(this, title);
  }
  SetDlgItemTextLang(this, IDC_STATIC_accepttip, LANG(LANG_ACCEPTTIP));

  /* LANG() is a call, so the labels cannot sit in a static table. */
  const char *const labels[] = { ACCEPT_DIALOG_ROWS(ACCEPT_DIALOG_LABEL) };
  const CString automatic = LANG(LANG_ACCEPTAUTO);

  for(int row=0 ; row<ACCEPT_ROW_COUNT ; row++) {
    CComboBox *const combo = (CComboBox*) GetDlgItem(acceptValueIds[row]);

    SetDlgItemTextLang(this, acceptLabelIds[row], labels[row]);
    if (combo == NULL)
      continue;              /* a row the .rc lost keeps the value it was given */
    combo->AddString(automatic);   /* first, and it stands for the empty value */
    fillAcceptCombo(combo, acceptKinds[row]);
    SetDlgItemTextCP(this, acceptValueIds[row], acceptBoxText(m_accept[row], automatic));
  }

	return TRUE;  // return TRUE unless you set the focus to a control
}

void CAcceptOptions::OnOK()
{
  /* acceptBoxValue() keeps the automatic word out of the value, whatever language it is
     in; --selftest holds both directions. */
  const CString automatic = LANG(LANG_ACCEPTAUTO);

  for(int row=0 ; row<ACCEPT_ROW_COUNT ; row++) {
    CString text;

    if (GetDlgItem(acceptValueIds[row]) == NULL)
      continue;              /* same reason as above */
    GetDlgItemText(acceptValueIds[row], text);
    m_accept[row] = acceptBoxValue(text, automatic);
  }
  CDialog::OnOK();
}

// see AcceptOptions.h
CString CAcceptOptions::acceptPresetTip(int row, const CString &text)
{
  /* The browser names are proper nouns, so only the old HTTrack value is translated. */
  static const struct { const char *browser; const char *name; } known[] = {
    { "chrome", "Chrome" }, { "firefox", "Firefox" }, { "legacy", NULL }, { NULL, NULL }
  };
  static const char *const kinds[] = { ACCEPT_ROWS(ACCEPT_ROW_KIND) };
  CString tip;

  if (text.IsEmpty() || row < 0 || row >= ACCEPT_ROW_COUNT)
    return tip;
  for(int k=0 ; known[k].browser != NULL ; k++) {
    const char *const preset = hts_accept_preset(known[k].browser, kinds[row]);

    if (preset == NULL || text != preset)
      continue;
    if (!tip.IsEmpty())
      tip += ", ";
    tip += known[k].name != NULL ? CString(known[k].name) : CString(LANG(LANG_ACCEPTLEGACY));
  }
  return tip;
}

// TOOL TIPS, as the option pages do it
BOOL CAcceptOptions::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
{
  TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
  UINT_PTR nID =pNMHDR->idFrom;
  if (pTTT->uFlags & TTF_IDISHWND)
  {
    // idFrom is actually the HWND of the tool
    nID = ::GetDlgCtrlID((HWND)nID);
    if(nID)
    {
      const char* st=GetTip((int)nID);
      if (st != NULL && *st) {
        pTTT->lpszText = (LPSTR)st;
        pTTT->hinst = AfxGetResourceHandle();
        return(TRUE);
      }
    }
  }
  return(FALSE);
}

const char* CAcceptOptions::GetTip(int ID)
{
  for(int row=0 ; row<ACCEPT_ROW_COUNT ; row++) {
    CString text;

    if (acceptValueIds[row] != ID)
      continue;
    GetDlgItemText(ID, text);
    m_tip = acceptPresetTip(row, text);   /* member, because the tooltip keeps the pointer */
    return m_tip;
  }
  return "";
}

HBRUSH CAcceptOptions::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
  const HBRUSH brush = WhttDarkCtlColor(pDC, pWnd, nCtlColor);

  return brush != NULL ? brush : CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}
