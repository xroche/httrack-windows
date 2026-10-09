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
/*       Dark colours for dialogs and common controls           */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef WINHTTRACK_DARKMODE_H
#define WINHTTRACK_DARKMODE_H

/* Unfinished, so it ships off and --dark turns it on: buttons and checkboxes keep the
   light visual style, and res/Toolbar.bmp is a 1998 bitmap drawn for a light background.

   A window calls WhttDarkInitWindow from OnInitDialog (or OnInitialUpdate) and forwards
   its OnCtlColor to WhttDarkCtlColor. Both do nothing while dark mode is off. */

/* Reads the switch and the system preference once; call it from InitInstance.
   requested is what the command line asked for. */
void WhttDarkModeInit(BOOL requested);

/* Gives a top-level window a dark title bar, and the common controls under it their
   dark theme. Safe to call on a child window, which has neither. */
void WhttDarkInitWindow(CWnd *wnd);

/* The brush a dark dialog, static text or edit control wants, after setting the text
   and background colours on pDC. NULL means the caller keeps its base class result. */
HBRUSH WhttDarkCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor);

#endif
