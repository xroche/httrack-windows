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
/*       which page a browser login opens, and whose jar        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Its own unit so tools/browser-login-paths-test.cpp can compile it off Windows. The
   rest of the login needs the helper DLL and a browser, and neither runs here. */

#ifdef BROWSER_LOGIN_PATHS_TEST
#include "mfc-test-stubs.h"       /* CString and CSimpleArray, as the harness sees them */
#include "cookie-file.h"
/* What the harness cannot take from browser-login.h, which names the helper DLL's own
   header and every Windows type that comes with it. */
void splitTokensInArray(CSimpleArray<CString> &args, const CString &str);
CString browserLoginStartUrl(const CString &urls);
CString browserLoginProfileDir(const CString &projectPath, const CString &localAppData);
CString browserLoginCookieFile(const CString &projectPath, const CString &localAppData);
int browserLoginCheckCases(CString *err);
#else
#include "stdafx.h"
#include "Shell.h"
#include "browser-login.h"
#endif

#include <stdio.h>

// see browser-login.h
CString browserLoginStartUrl(const CString &urls) {
  CSimpleArray<CString> tokens;

  splitTokensInArray(tokens, urls);
  for(int i = 0 ; i < tokens.GetSize() ; i++) {
    CString one = tokens[i];

    if (one.IsEmpty())
      continue;
    /* A filter rule in the address box is no page anyone can log in to. */
    if (one[0] == '+' || one[0] == '-')
      continue;
    /* The test Wid1 uses when it tidies that box, so the browser opens what the crawl
       would fetch. */
    if (one.Find(":/") < 0)
      one = "http://" + one;
    return one;
  }
  return CString();
}

// see browser-login.h
CString browserLoginProfileDir(const CString &projectPath, const CString &localAppData) {
  if (!projectPath.IsEmpty())
    return pathUnder(projectPath, "hts-cache\\browser-profile");
  return pathUnder(localAppData, "HTTrack\\browser-profile");
}

// see browser-login.h
CString browserLoginCookieFile(const CString &projectPath, const CString &localAppData) {
  if (!projectPath.IsEmpty())
    return cookieFileInProject(projectPath);
  return pathUnder(localAppData, "HTTrack\\browser-cookies.txt");
}

/* A login that opens the wrong page, or one project's browser reading another's jar,
   both look like a site that simply refused the login. */
int browserLoginCheckCases(CString *err) {
  char msg[1024];
  int nchecks = 0;

  {
    static const struct { const char *urls, *want; } starts[] = {
      { "http://example.com/", "http://example.com/" },
      { "https://example.com/in", "https://example.com/in" },
      /* the box takes a bare host, and the crawl gives it the same scheme */
      { "example.com/in", "http://example.com/in" },
      /* several addresses: the browser opens the one the crawl starts from */
      { "example.com two.example.com", "http://example.com" },
      { "  \t example.com \r\n two.test ", "http://example.com" },
      /* a rule is no page, and one leading the box must not become the start */
      { "-*.gif +*.css https://example.com/", "https://example.com/" },
      { "-*.gif", "" },
      { "", "" },
      /* ftp:/ and file:/ carry the ":/" test, so neither gains a second scheme */
      { "ftp://example.com/", "ftp://example.com/" },
      { NULL, NULL }
    };

    for(int k=0 ; starts[k].urls != NULL ; k++) {
      const CString got = browserLoginStartUrl(starts[k].urls);

      if (got != starts[k].want) {
        snprintf(msg, sizeof(msg), "address box '%s' opens '%s', expected '%s'",
                 starts[k].urls, (LPCSTR) got, starts[k].want);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }
  {
    static const struct { const char *project, *local, *want; } homes[] = {
      { "C:\\web\\proj", "C:\\users\\me\\AppData\\Local",
        "C:\\web\\proj\\hts-cache\\browser-profile" },
      { "C:\\web\\proj\\", "C:\\users\\me\\AppData\\Local",
        "C:\\web\\proj\\hts-cache\\browser-profile" },
      /* no project yet, so the login lives with the user rather than nowhere */
      { "", "C:\\users\\me\\AppData\\Local",
        "C:\\users\\me\\AppData\\Local\\HTTrack\\browser-profile" },
      { "", "", "" },
      { NULL, NULL, NULL }
    };

    for(int k=0 ; homes[k].project != NULL ; k++) {
      const CString got = browserLoginProfileDir(homes[k].project, homes[k].local);

      if (got != homes[k].want) {
        snprintf(msg, sizeof(msg), "project '%s' keeps its login in '%s', expected '%s'",
                 homes[k].project, (LPCSTR) got, homes[k].want);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }
  {
    static const struct { const char *project, *local, *want; } files[] = {
      /* the project's own copy, which is the one --cookies-file is given untouched */
      { "C:\\web\\proj", "C:\\local", "C:\\web\\proj\\hts-cache\\cookies.txt" },
      { "", "C:\\local", "C:\\local\\HTTrack\\browser-cookies.txt" },
      { "", "", "" },
      { NULL, NULL, NULL }
    };

    for(int k=0 ; files[k].project != NULL ; k++) {
      const CString got = browserLoginCookieFile(files[k].project, files[k].local);

      if (got != files[k].want) {
        snprintf(msg, sizeof(msg), "project '%s' saves the login to '%s', expected '%s'",
                 files[k].project, (LPCSTR) got, files[k].want);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
    /* The browser profile must not sit where the cookies file goes, or the save would
       land on a folder and the login would be lost on every run. */
    if (browserLoginProfileDir("C:\\web\\proj", "C:\\local")
        == browserLoginCookieFile("C:\\web\\proj", "C:\\local")) {
      *err = "the browser profile and the cookies file want the same path";
      return 0;
    }
    nchecks++;
  }
  return nchecks;
}
