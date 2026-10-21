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
CString browserLoginSafeUrl(const CString &landed);
CString browserLoginProfileDir(const CString &localAppData, unsigned long run);
CString browserLoginProfileRoot(const CString &localAppData);
BOOL browserLoginStaleProfile(const CString &name, unsigned long *run);
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

    /* A filter rule in the address box is no page anyone can log in to. */
    if (one.Left(1) == "+" || one.Left(1) == "-")
      continue;
    /* The same test the crawl applies to that box, so the browser opens the address the
       crawl would fetch. One slash, because file:/x carries a scheme too. */
    if (one.Find(":/") < 0)
      one = "http://" + one;
    return one;
  }
  return CString();
}

// see browser-login.h
CString browserLoginSafeUrl(const CString &landed) {
  const int scheme = landed.Find("://");

  if (scheme < 0)
    return CString();
  /* Only what the crawler can fetch. A browser can end up on about: or edge:, and
     WebView2 hands back a normalised address, so the scheme is already lower case. */
  if (landed.Left(scheme) != "http" && landed.Left(scheme) != "https")
    return CString();

  CString kept = landed;
  const int hash = kept.Find('#');

  /* A fragment never goes on the wire, so dropping it costs the crawl nothing. It is
     also where a sign-in hands an access token back. */
  if (hash >= 0)
    kept = kept.Left(hash);

  const CString rest = kept.Mid(scheme + 3);
  const int slash = rest.Find('/');

  /* Refused whole rather than stripped, because the engine would authenticate with it,
     and the box above this one has its own fields for a login. */
  if ((slash < 0 ? rest : rest.Left(slash)).Find('@') >= 0)
    return CString();
  return kept;
}

/* What a profile folder is called. The sweep reads the run back out of it. */
#define BROWSER_PROFILE_PREFIX "profile-"

// see browser-login.h
CString browserLoginProfileRoot(const CString &localAppData) {
  return pathUnder(localAppData, "HTTrack\\browser-profiles");
}

// see browser-login.h
CString browserLoginProfileDir(const CString &localAppData, unsigned long run) {
  const CString root = browserLoginProfileRoot(localAppData);
  char name[32];

  if (root.IsEmpty())
    return root;
  snprintf(name, sizeof(name), BROWSER_PROFILE_PREFIX "%lu", run);
  return pathUnder(root, name);
}

// see browser-login.h
BOOL browserLoginStaleProfile(const CString &name, unsigned long *run) {
  static const char prefix[] = BROWSER_PROFILE_PREFIX;
  const int skip = (int) sizeof(prefix) - 1;
  unsigned long got = 0;

  /* Ten digits is every process id there can be, and refusing a longer one keeps the
     multiply below from wrapping into a run that is alive. */
  if (name.Left(skip) != prefix || name.GetLength() <= skip
      || name.GetLength() > skip + 10)
    return FALSE;
  for(int i = skip ; i < name.GetLength() ; i++) {
    if (name[i] < '0' || name[i] > '9')
      return FALSE;
    got = got * 10 + (unsigned long) (name[i] - '0');
  }
  *run = got;
  return TRUE;
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
      /* one slash is still a scheme, and no row above has that shape */
      { "file:/tmp/page.html", "file:/tmp/page.html" },
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
    static const struct { const char *local; unsigned long run; const char *want; } homes[] = {
      { "C:\\users\\me\\AppData\\Local", 4321,
        "C:\\users\\me\\AppData\\Local\\HTTrack\\browser-profiles\\profile-4321" },
      { "C:\\users\\me\\AppData\\Local\\", 7,
        "C:\\users\\me\\AppData\\Local\\HTTrack\\browser-profiles\\profile-7" },
      /* nowhere of ours to put it, so the caller gets nothing rather than a relative
         path next to whatever directory the program happens to be in */
      { "", 4321, "" },
      { NULL, 0, NULL }
    };

    for(int k=0 ; homes[k].local != NULL ; k++) {
      const CString got = browserLoginProfileDir(homes[k].local, homes[k].run);

      if (got != homes[k].want) {
        snprintf(msg, sizeof(msg), "run %lu under '%s' gave '%s', expected '%s'",
                 homes[k].run, homes[k].local, (LPCSTR) got, homes[k].want);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
    /* A profile must never land where the cookies file goes, nor inside a project. */
    if (browserLoginProfileDir("C:\\local", 1).Find("hts-cache") >= 0) {
      *err = "the browser profile is inside a project again";
      return 0;
    }
    nchecks++;
  }
  {
    /* The sweep deletes what this says is ours, so a wrong TRUE removes a stranger's
       folder and a wrong FALSE leaves a signed-in session on disk for ever. */
    static const struct { const char *name; BOOL ours; unsigned long run; } stale[] = {
      { "profile-1234", TRUE, 1234 },
      { "profile-7", TRUE, 7 },
      { "profile-0000000012", TRUE, 12 },
      { "profile-", FALSE, 0 },
      { "profile-12a", FALSE, 0 },
      { "profile-1 2", FALSE, 0 },
      { "profile--1", FALSE, 0 },
      { "Default", FALSE, 0 },
      { "", FALSE, 0 },
      /* longer than any process id, so the digits cannot wrap onto a live run */
      { "profile-99999999999", FALSE, 0 },
      { NULL, FALSE, 0 }
    };

    for(int k=0 ; stale[k].name != NULL ; k++) {
      unsigned long run = 0;
      const BOOL got = browserLoginStaleProfile(stale[k].name, &run);

      if (got != stale[k].ours || (got && run != stale[k].run)) {
        snprintf(msg, sizeof(msg), "'%s' read as ours=%d run=%lu, expected ours=%d run=%lu",
                 stale[k].name, (int) got, run, (int) stale[k].ours, stale[k].run);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
    /* The name a real run makes has to be one the sweep recognises, or nothing is ever
       cleaned up and no case above would notice. */
    {
      const CString dir = browserLoginProfileDir("C:\\local", 24680);
      const int cut = dir.ReverseFind('\\');
      unsigned long run = 0;

      if (cut < 0 || !browserLoginStaleProfile(dir.Mid(cut + 1), &run) || run != 24680) {
        *err = "the sweep does not recognise the folder a run actually makes";
        return 0;
      }
      nchecks++;
    }
  }
  {
    static const struct { const char *landed, *want; } safe[] = {
      { "https://example.com/members/", "https://example.com/members/" },
      /* a fragment is never sent, and a sign-in returns its token in one */
      { "https://example.com/cb#access_token=secret", "https://example.com/cb" },
      { "https://example.com/members/#", "https://example.com/members/" },
      /* userinfo is a credential, and the engine would authenticate with it */
      { "https://user:pass@example.com/", "" },
      { "https://user@example.com/", "" },
      /* an @ past the authority is an ordinary path byte */
      { "https://example.com/a@b/c", "https://example.com/a@b/c" },
      { "https://example.com/a@b/c#x", "https://example.com/a@b/c" },
      /* nothing the crawler could fetch */
      { "about:blank", "" },
      { "edge://settings/profiles", "" },
      { "", "" },
      /* the query is kept: a crawl start legitimately carries one */
      { "https://example.com/s?q=1", "https://example.com/s?q=1" },
      { NULL, NULL }
    };

    for(int k=0 ; safe[k].landed != NULL ; k++) {
      const CString got = browserLoginSafeUrl(safe[k].landed);

      if (got != safe[k].want) {
        snprintf(msg, sizeof(msg), "landing on '%s' keeps '%s', expected '%s'",
                 safe[k].landed, (LPCSTR) got, safe[k].want);
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
  }
  return nchecks;
}
