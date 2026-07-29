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
// LaunchHelp.cpp : implementation file
//

#include "stdafx.h"
#include "LaunchHelp.h"
#include "Shell.h"

/* Shared with WebHTTrack and Android; #win picks our screenshots. */
#define DOC_GUIDE "guide.html#win/"

/* Unreserved, plus ':' and '/' so the drive letter and the separators survive. */
static bool IsUrlSafe(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
      || c == '-' || c == '_' || c == '.' || c == '~' || c == '/' || c == ':';
}

static void AppendEscaped(CString& url, const char* s) {
  for(; *s != '\0' ; s++) {
    const unsigned char c = (unsigned char) *s;
    if (IsUrlSafe(c)) {
      url += (char) c;
    } else {
      char hex[4];
      sprintfbuff(hex, "%%%02X", c);
      url += hex;
    }
  }
}

// Contract in LaunchHelp.h.
bool BuildDocUrl(const char* dir, const char* page, CString& url) {
  const char* const frag = strchr(page, '#');
  CString path(dir);
  path += (frag != NULL) ? CString(page, (int) (frag - page)) : CString(page);
  path.Replace('\\', '/');

  /* A browser reads a file: URL's escaped bytes as UTF-8, not as our ANSI codepage. */
  char* utf8 = strdupt_utf8(path);   /* freet() nulls it, so not const */
  if (utf8 == NULL)
    return false;
  /* A UNC path is already rooted at "//server", so it takes two slashes, not three. */
  url = (utf8[0] == '/' && utf8[1] == '/') ? "file:" : "file:///";
  AppendEscaped(url, utf8);
  freet(utf8);

  if (frag != NULL) {
    url += '#';
    AppendEscaped(url, frag + 1);
  }
  return true;
}

/* The doc tree sits beside the executable, where the installer stages it. Not the
   engine's hts_rootdir(): that is seeded late in InitInstance, after --selftest. */
static bool DocDir(char* dir, size_t size) {
  const DWORD n = ::GetModuleFileName(NULL, dir, (DWORD) size);
  if (n == 0 || n >= size)   /* n == size on truncation, which leaves dir unterminated */
    return false;
  char* p = dir + n;
  while(p > dir && p[-1] != '\\' && p[-1] != '/')
    p--;
  if ((size_t) (p - dir) + sizeof("html\\") > size)
    return false;
  memcpy(p, "html\\", sizeof("html\\"));
  return true;
}

void LaunchHelp::Help(CString page) {
  char dir[1024];
  CString url;
  if (!DocDir(dir, sizeof(dir)) || !BuildDocUrl(dir, page, url)
      || !ShellOpen(url, SW_SHOWNORMAL))
    AfxMessageBox(LANG(LANG_DIAL1));
}

void LaunchHelp::HelpTopic(const char* anchor) {
  Help(CString(DOC_GUIDE) + anchor);
}

void LaunchHelp::Help() {
  Help("index.html");
}

