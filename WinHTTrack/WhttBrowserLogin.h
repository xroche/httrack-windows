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
/*       WhttBrowserLogin.dll, the on-demand WebView2 host       */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef WHTT_BROWSER_LOGIN_H
#define WHTT_BROWSER_LOGIN_H

#include <stddef.h>

/* WinHTTrack.exe loads this DLL with LoadLibrary and never links it: the WebView2 loader
   inside hard-imports EventSetInformation, which Windows 7 does not export, so an import
   of ours would stop the program starting there at all. */

/* Bump on any change to the exports below; the GUI refuses a DLL answering another number. */
#define WHTT_BROWSER_LOGIN_ABI 1

/* Fits any version WhttBrowserLoginAvailable() reports, with room to spare. */
#define WHTT_BROWSER_LOGIN_VERSION_MAX 64

#ifdef __cplusplus
extern "C" {
#endif

/* The WHTT_BROWSER_LOGIN_ABI this DLL was built against. */
unsigned __cdecl WhttBrowserLoginAbi(void);

/* Is a WebView2 runtime installed? version, when given, takes its version as UTF-8 and
   is never left empty on success. */
int __cdecl WhttBrowserLoginAvailable(char *version, size_t version_size);

#ifdef __cplusplus
}
#endif

/* What the GUI casts each GetProcAddress result to. */
typedef unsigned (__cdecl *WhttBrowserLoginAbiFn)(void);
typedef int (__cdecl *WhttBrowserLoginAvailableFn)(char *version, size_t version_size);

#endif
