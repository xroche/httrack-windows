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

#ifndef HTS_DEFNEWLANG
#define HTS_DEFNEWLANG

void LANG_LOAD(char* limit_to);
void LANG_INIT();
int LANG_T(int);
int QLANG_T(int l);
//char* LANGSEL(char* lang0,...);
/* Set by --selftest (WinHTTrack.cpp): report fatals on stderr and exit non-zero
   instead of raising a message box nobody can click. */
extern int WhttSelfTest;
void WhttEnsureConsole(void);

const char* LANGSEL(const char* name);
const char* LANGINTKEY(const char* name);
void LANG_DELETE();
void conv_printf(char* from,char* to);
#define LANG(A) A

BOOL SetDlgItemTextCP(HWND hDlg, int nIDDlgItem, LPCSTR lpString);
BOOL SetDlgItemTextCP(CWnd* wnd, int nIDDlgItem, LPCSTR lpString);
BOOL SetDlgItemTextUTF8(HWND hDlg, int nIDDlgItem, LPCSTR lpString);
BOOL SetDlgItemTextUTF8(CWnd* wnd, int nIDDlgItem, LPCSTR lpString);
BOOL SetWindowTextCP(HWND hWnd, LPCSTR lpString);
BOOL SetWindowTextCP(CWnd* wnd, LPCSTR lpString);
BOOL SetWindowTextUTF8(HWND hWnd, LPCSTR lpString);
BOOL SetWindowTextUTF8(CWnd* wnd, LPCSTR lpString);
BOOL ModifyMenuCP(HMENU hMnu, UINT uPosition, UINT uFlags, UINT uIDNewItem, LPCSTR lpNewItem);
BOOL ModifyMenuCP(CMenu* menu, UINT uPosition, UINT uFlags, UINT uIDNewItem, LPCSTR lpNewItem);

#endif

