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

/* Unfinished. It ships off, and --dark turns it on. Push buttons keep the light visual
   style, and res/Toolbar.bmp is a 1998 bitmap drawn for a light background.

   A window calls WhttDarkInitWindow from OnInitDialog (or OnInitialUpdate) and forwards
   its OnCtlColor to WhttDarkCtlColor. Both do nothing while dark mode is off. */

/* Reads the switch and the system preference once. Call it from InitInstance.
   requested is what the command line asked for. */
void WhttDarkModeInit(BOOL requested);

/* Gives a top-level window a dark title bar, and the common controls under it their
   dark theme. Check boxes, radio buttons, group boxes and the tab control are taken over
   for painting here, so nothing in this file draws anything while dark mode is off. Safe
   to call on a child window. */
void WhttDarkInitWindow(CWnd *wnd);

/* The vsstyle.h state id for the check or radio glyph, from the control's window style
   and its BM_GETSTATE answer. Reachable with dark mode off, for --selftest. */
int WhttDarkGlyphState(DWORD style, UINT state);

/* Draws that glyph over the dark page in a memory bitmap and answers the mean of its
   pixels, 0 to 255, or -1 where GDI refused or glyphState is outside 1 to 12. changed,
   where it is not NULL, takes the count of pixels the glyph moved off the page colour.
   Nothing reaches the screen, and dark mode may be off: this is for --selftest, which
   has no window to measure. */
int WhttDarkGlyphLuminance(int glyphState, BOOL radio, int *changed);

/* What --selftest reads off one painted row of tabs. Every field is a bitmap coordinate or
   a pixel value, so a painter that moved or recoloured something answers differently. */
typedef struct {
  int selectedTop;        /* the topmost row the selected tab painted */
  int unselectedTop;      /* and the topmost its neighbour painted */
  int selectedFill;       /* the pixel inside the selected tab */
  int unselectedFill;     /* and inside its neighbour */
  int frameOnOther;       /* the pixel on an unselected tab's own frame */
  int raiseLeft;          /* the columns the selected tab reaches above the row */
  int raiseRight;
  int labelInk;           /* the brightest pixel the selected tab's label drew */
  int labelLeft;          /* the columns that label's ink spans */
  int labelRight;
  int labelTop;           /* and the rows */
  int labelBottom;
  int otherLabelTop;      /* the rows the neighbour's label inked */
  int otherLabelBottom;
  int itemLeft;           /* and the columns the selected tab's own rect spans */
  int itemRight;
  int itemTop;            /* and its rows */
  int itemBottom;
  int seamUnderSelected;  /* the pixel on the page's top line under the selected tab */
  int seamUnderOther;     /* and under its neighbour */
} whtt_dark_tab_probe_t;

/* Paints three tabs with the middle one selected into a memory bitmap, and fills probe from
   what came out. FALSE where GDI refused. Nothing reaches the screen, and dark mode may be
   off: this is for --selftest, which has no property sheet to measure. */
BOOL WhttDarkTabProbe(whtt_dark_tab_probe_t *probe);

/* The brush a dark dialog, static text, edit control or list wants, after setting the
   text and background colours on pDC. NULL means the caller keeps its base class
   result. */
HBRUSH WhttDarkCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor);

#endif
