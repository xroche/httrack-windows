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

/* WinHTTrack.exe loads this DLL with LoadLibrary and never links it, because the WebView2
   loader inside hard-imports EventSetInformation, which Windows 7 does not export. The
   loader that does it is WinHTTrack/browser-login.cpp. */

/* Bump on any change below, because the GUI refuses a DLL answering another number. */
#define WHTT_BROWSER_LOGIN_ABI 2

/* Fits any version WhttBrowserLoginAvailable() reports, with room to spare. */
#define WHTT_BROWSER_LOGIN_VERSION_MAX 64

#ifdef __cplusplus
extern "C" {
#endif

unsigned __cdecl WhttBrowserLoginAbi(void);

/* Is a WebView2 runtime installed? version, when given, takes its version as UTF-8, cut
   to fit. A WHTT_BROWSER_LOGIN_VERSION_MAX buffer always holds one whole. */
int __cdecl WhttBrowserLoginAvailable(char *version, size_t version_size);

/* One cookie as the browser holds it. Every string is UTF-8 and belongs to the DLL. */
typedef struct WhttBrowserLoginCookie {
  const char *name;
  const char *value;
  const char *domain;
  const char *path;
  int secure;
  /* Seconds since the epoch. Zero or less for a cookie that goes with the session. */
  double expires;
} WhttBrowserLoginCookie;

/* Takes one cookie. WhttBrowserLoginRun() calls it once per cookie, after the window has
   closed, so it may do whatever the caller likes. Returning zero takes no more. */
typedef int (__cdecl *WhttBrowserLoginCookieFn)(void *ctx, const WhttBrowserLoginCookie *cookie);

typedef struct WhttBrowserLoginRequest {
  /* sizeof(WhttBrowserLoginRequest). WhttBrowserLoginRun() refuses any other value, which
     is what catches a build whose struct moved under an ABI number that did not. */
  size_t size;
  void *parent;         /* the HWND to own the window and sit disabled under it, or NULL */
  const char *url;      /* UTF-8 page to open first, or NULL for a blank one */
  const char *profile;  /* UTF-8 folder the browser keeps its profile in, or NULL */
  const char *title;    /* UTF-8 window caption, or NULL for the DLL's own */
  WhttBrowserLoginCookieFn onCookie;  /* may be NULL, which harvests nothing */
  void *onCookieCtx;
  char *userAgent;      /* takes the browser's own User-Agent, UTF-8, may be NULL */
  size_t userAgentSize;
} WhttBrowserLoginRequest;

/* What WhttBrowserLoginRun() answers. Everything but WhttBrowserLoginDone means the user
   never saw a browser, so the caller has something to tell them. */
enum WhttBrowserLoginResult {
  WhttBrowserLoginDone = 0,
  WhttBrowserLoginBadRequest,   /* the request is not the one this DLL was built against */
  WhttBrowserLoginNoRuntime,    /* no WebView2 runtime is installed */
  WhttBrowserLoginNoApartment,  /* the calling thread is not in a single-threaded apartment */
  WhttBrowserLoginNoWindow,     /* our own window would not open */
  WhttBrowserLoginNoBrowser     /* a runtime is installed, and it would not start */
};

/* Opens a browser window on REQUEST->url and returns once the user closes it, having
   handed every cookie the profile then holds to REQUEST->onCookie. Only a thread in a
   single-threaded apartment may call it, and it runs a message loop of its own. */
int __cdecl WhttBrowserLoginRun(const WhttBrowserLoginRequest *request);

#ifdef __cplusplus
}
#endif

typedef unsigned (__cdecl *WhttBrowserLoginAbiFn)(void);
typedef int (__cdecl *WhttBrowserLoginAvailableFn)(char *version, size_t version_size);
typedef int (__cdecl *WhttBrowserLoginRunFn)(const WhttBrowserLoginRequest *request);

#endif
