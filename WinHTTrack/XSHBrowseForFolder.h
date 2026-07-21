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
// ----------------------------------------------------------------------
// 'extended' SHBrowseForFolder routine. ('New folder' button added)
// Written by Xavier Roche, with the help of Gil Rosin, 
// Todd Fast's routines from Pencilneck Software and other Usenet contributors.
// 
// Usage: (example)
// CString path = XSHBrowseForFolder(this->m_hWnd,"Select path","c:\\") {
//
// Freeware, but no warranty!
//
// #include "NewFolder.h"             for Input dialog (new folder)
// #include <direct.h>                for _mkdir
// ----------------------------------------------------------------------

#if !defined(__XSHBrowseForFolder_routines)
#define __XSHBrowseForFolder_routines 

// TODO: Put here your ressource definition
#include "resource.h"
#include "NewFolder.h"

#include <direct.h>
#include "shlobj.h"

#define XSHBrowseForFolder_SETSTRING 1234
#define XSHBrowseForFolder_OK 1

CString        XSHBrowseForFolder (HWND hwnd,const char* title,char* _path);
LRESULT __stdcall XSHBFF_WndProc     (HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam);
int __stdcall  XSHBFF_CallbackProc(HWND hwnd,UINT uMsg,LPARAM lParam,LPARAM lpData);
LPITEMIDLIST   XSHBFF_PathConvert (HWND hwnd,char* _path);

#endif

