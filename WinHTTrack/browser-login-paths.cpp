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
BOOL browserLoginMovedHost(const CString &startUrl, const CString &landedUrl);
CString browserLoginBundleName(const CString &landedUrl);
CString browserLoginBundleFile(const CString &projectPath, const CString &name);
CString browserLoginBundleUrl(const CString &landedUrl, const CString &name);
CString browserLoginBundleText(const CString &userAgent, const CString &records);
CString browserLoginProfileDir(const CString &localAppData, unsigned long run);
CString browserLoginProfileRoot(const CString &localAppData);
BOOL browserLoginStaleProfile(const CString &name, unsigned long *run);
int browserLoginCheckCases(CString *err);
extern "C" {
  /* copied in or stubbed by tools/test-browser-login-paths.py */
  int hts_registrable_domain(const char *host, char *out, size_t size);
  const char *hts_bundle_name_refusal(const char *name);
};
#else
#include "stdafx.h"
#include "Shell.h"
#include "browser-login.h"
extern "C" {
  #include "HTTrackInterface.h"
};
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

/* The host of URL, lowercase, with no userinfo, no port and no trailing dot. Empty when
   URL carries no host. Lowercased by hand because MakeLower() folds the wrong letters
   under a Turkish locale. */
static CString urlHost(const CString &url) {
  const int scheme = url.Find("://");

  if (scheme < 0)
    return CString();

  CString host = url.Mid(scheme + 3);
  int end = host.GetLength();

  for(int i = 0 ; i < host.GetLength() ; i++) {
    const char c = host[i];

    if (c == '/' || c == '?' || c == '#') {
      end = i;
      break;
    }
  }
  host = host.Left(end);
  /* The LAST one, because that is the host the crawl would really fetch. */
  const int at = host.ReverseFind('@');

  if (at >= 0)
    host = host.Mid(at + 1);
  if (host.Left(1) == "[") {
    const int close = host.Find(']');

    host = (close < 0) ? CString() : host.Left(close + 1);
  } else {
    const int colon = host.Find(':');

    if (colon >= 0)
      host = host.Left(colon);
  }

  CString plain;

  for(int i = 0 ; i < host.GetLength() ; i++) {
    const char c = host[i];

    plain += (char) ((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
  }
  while (plain.GetLength() > 0 && plain[plain.GetLength() - 1] == '.')
    plain = plain.Left(plain.GetLength() - 1);
  return plain;
}

/* urlHost() without a leading "www.". */
static CString comparableHost(const CString &url) {
  const CString host = urlHost(url);

  return host.Left(4) == "www." ? host.Mid(4) : host;
}

// see browser-login.h
BOOL browserLoginMovedHost(const CString &startUrl, const CString &landedUrl) {
  const CString start = comparableHost(startUrl);
  const CString landed = comparableHost(landedUrl);

  if (start.IsEmpty() || landed.IsEmpty())
    return FALSE;
  return start != landed;
}

/* From the engine's htsbundle.h. */
#define BROWSER_BUNDLE_SUFFIX "#httrack-bundle="

// see browser-login.h
CString browserLoginBundleName(const CString &landedUrl) {
  const CString host = comparableHost(landedUrl);
  char site[256];

  /* Two hosts of one site must share a file, because overlapping bundles abort the mirror.
     The lookup keeps the "www.", because www.blogspot.com is a site of its own. */
  if (hts_registrable_domain(urlHost(landedUrl), site, sizeof(site))
      && hts_bundle_name_refusal(site) == NULL)
    return CString(site);
  if (hts_bundle_name_refusal(host) == NULL)
    return host;
  /* A name every host falls back to, because a login with nowhere to go is lost. */
  return CString("login");
}

// see browser-login.h
CString browserLoginBundleFile(const CString &projectPath, const CString &name) {
  if (projectPath.IsEmpty() || name.IsEmpty())
    return CString();
  return pathUnder(projectPath, name + ".hts");
}

// see browser-login.h
CString browserLoginBundleUrl(const CString &landedUrl, const CString &name) {
  if (landedUrl.IsEmpty() || name.IsEmpty())
    return CString();
  /* The engine reads the FIRST suffix and refuses a name holding another, so an address
     already carrying one would abort the mirror. */
  if (landedUrl.Find(BROWSER_BUNDLE_SUFFIX) >= 0)
    return CString();
  return landedUrl + BROWSER_BUNDLE_SUFFIX + name;
}

/* What the engine reads a bundle line into, from htsbundle.c. A longer line stops the
   run, so the identity is dropped rather than written. */
#define BROWSER_BUNDLE_LINE_MAX 8192

// see browser-login.h
CString browserLoginBundleText(const CString &userAgent, const CString &records) {
  CString text = "# HTTrack bundle 1\n";
  BOOL oneLine = !userAgent.IsEmpty()
    && userAgent.GetLength() + 3 < BROWSER_BUNDLE_LINE_MAX;

  for(int i = 0 ; oneLine && i < userAgent.GetLength() ; i++)
    if ((unsigned char) userAgent[i] < ' ')
      oneLine = FALSE;
  if (oneLine) {
    text += "ua ";
    text += userAgent;
    text += "\n";
  }
  /* The engine takes every line after this one as a jar record. */
  text += "cookies\n";
  text += records;
  return text;
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
    /* A wrong FALSE is the costly one: it crawls a host the user never named. */
    static const struct { const char *start, *landed; BOOL moved; } away[] = {
      { "http://example.com", "https://example.com/members/", FALSE },
      { "http://example.com/in", "https://idp.test/u/login", TRUE },
      /* "www." is the same site, in both directions */
      { "http://www.example.com", "https://example.com/", FALSE },
      { "http://example.com", "https://www.example.com/", FALSE },
      /* but a longer label starting with the same bytes is not */
      { "http://example.com", "https://wwwexample.com/", TRUE },
      /* another host of the same company still moves the crawl start */
      { "http://example.com", "https://login.example.com/", TRUE },
      /* the host is case-insensitive, and a root label may be written out */
      { "http://EXAMPLE.com", "https://example.com/", FALSE },
      { "http://example.com.", "https://example.com/", FALSE },
      /* a port is not the host, and neither is a credential before it */
      { "http://example.com:8080", "https://example.com/", FALSE },
      { "http://me:pw@example.com", "https://example.com/", FALSE },
      /* an @ in the authority names the real host, as the crawl would read it */
      { "http://example.com@evil.test", "https://evil.test/", FALSE },
      { "http://example.com", "https://example.com@evil.test/", TRUE },
      /* an IPv6 literal keeps its colons rather than ending at the first one */
      { "http://[::1]:8080", "http://[::1]/", FALSE },
      { "http://[::1]", "http://[::2]/", TRUE },
      /* nothing to compare is not a warning: an empty box asked for no site */
      { "", "https://example.com/", FALSE },
      { "http://example.com", "", FALSE },
      { "http://example.com", "about:blank", FALSE },
      { NULL, NULL, FALSE }
    };

    for(int k=0 ; away[k].start != NULL ; k++) {
      const BOOL got = browserLoginMovedHost(away[k].start, away[k].landed);

      if (got != away[k].moved) {
        snprintf(msg, sizeof(msg), "'%s' landing on '%s' read as moved=%d, expected %d",
                 away[k].start, away[k].landed, (int) got, (int) away[k].moved);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }
  {
    /* The engine ABORTS a mirror over a name it refuses. An odd name must still be one
       it takes. */
    static const struct { const char *landed, *name; } names[] = {
      { "https://example.com/members/", "example.com" },
      /* the same site is one bundle, as the moved-host warning also reads it */
      { "https://www.example.com/", "example.com" },
      { "https://example.com:8443/x", "example.com" },
      { "https://EXAMPLE.com/", "example.com" },
      /* two hosts of one site are one bundle, because the engine scopes it so */
      { "https://a.example.com/", "example.com" },
      { "https://login.shop.example.co.uk/", "example.co.uk" },
      /* no registrable domain, so the host names it */
      { "https://localhost/", "localhost" },
      /* www.github.io is a site of its own, since github.io is a public suffix */
      { "https://www.github.io/", "www.github.io" },
      { "http://192.168.1.10/", "192.168.1.10" },
      /* a label the engine would read as a Windows device, up to the first dot */
      { "https://con/", "login" },
      { "https://CON.example/", "login" },
      /* a refused site still leaves the host to name it */
      { "https://shop.con.com/", "shop.con.com" },
      { "https://com1/", "login" },
      /* com0 is no device, and a four-letter label is not one either */
      { "https://com0/", "com0" },
      { "https://conx/", "conx" },
      /* an IPv6 literal keeps bytes no bundle name may hold */
      { "http://[::1]:8080/", "login" },
      /* nothing to name it after */
      { "about:blank", "login" },
      { "", "login" },
      { NULL, NULL }
    };

    for(int k=0 ; names[k].landed != NULL ; k++) {
      const CString got = browserLoginBundleName(names[k].landed);

      if (got != names[k].name) {
        snprintf(msg, sizeof(msg), "landing on '%s' names the bundle '%s', expected '%s'",
                 names[k].landed, (LPCSTR) got, names[k].name);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }
  {
    static const struct { const char *landed, *name, *want; } urls[] = {
      { "https://example.com/x", "example.com",
        "https://example.com/x#httrack-bundle=example.com" },
      /* the suffix must appear once, so an address already carrying one is refused */
      { "https://example.com/x#httrack-bundle=other", "example.com", "" },
      { "", "example.com", "" },
      { "https://example.com/x", "", "" },
      { NULL, NULL, NULL }
    };

    for(int k=0 ; urls[k].landed != NULL ; k++) {
      const CString got = browserLoginBundleUrl(urls[k].landed, urls[k].name);

      if (got != urls[k].want) {
        snprintf(msg, sizeof(msg), "'%s' plus bundle '%s' gives '%s', expected '%s'",
                 urls[k].landed, urls[k].name, (LPCSTR) got, urls[k].want);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
    /* The engine resolves a name against the project, so the file has to land there. */
    if (browserLoginBundleFile("C:\\web\\proj", "example.com")
        != "C:\\web\\proj\\example.com.hts") {
      *err = "the bundle no longer lands in the project folder";
      return 0;
    }
    nchecks++;
    if (!browserLoginBundleFile("", "example.com").IsEmpty()) {
      *err = "a bundle with no project got a relative path";
      return 0;
    }
    nchecks++;
  }
  {
    /* The engine refuses a bundle whose first line is not this, and takes every line
       after "cookies" as a jar record, so the shape IS the contract. */
    static const struct { const char *agent, *records, *want; } texts[] = {
      { "Mozilla/5.0", "a\tFALSE\t/\tFALSE\t0\tn\tv\n",
        "# HTTrack bundle 1\nua Mozilla/5.0\ncookies\na\tFALSE\t/\tFALSE\t0\tn\tv\n" },
      /* no identity is a bundle too, because the cookies are the point */
      { "", "a\tFALSE\t/\tFALSE\t0\tn\tv\n",
        "# HTTrack bundle 1\ncookies\na\tFALSE\t/\tFALSE\t0\tn\tv\n" },
      /* a byte that would forge the next line takes the identity out, not the bundle */
      { "Mozilla\n ua evil", "r\n", "# HTTrack bundle 1\ncookies\nr\n" },
      { "Mozilla\rx", "r\n", "# HTTrack bundle 1\ncookies\nr\n" },
      { "Mozilla\tx", "r\n", "# HTTrack bundle 1\ncookies\nr\n" },
      { NULL, NULL, NULL }
    };

    for(int k=0 ; texts[k].agent != NULL ; k++) {
      const CString got = browserLoginBundleText(texts[k].agent, texts[k].records);

      if (got != texts[k].want) {
        snprintf(msg, sizeof(msg), "identity '%s' makes a bundle '%s', expected '%s'",
                 texts[k].agent, (LPCSTR) got, texts[k].want);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
    /* A line the engine cannot read stops the run, so a long identity is left out. */
    {
      const CString huge('x', 9000);

      if (browserLoginBundleText(huge, "r\n").Find("ua ") >= 0) {
        *err = "a bundle wrote an identity longer than the engine reads a line";
        return 0;
      }
      nchecks++;
    }
  }
  return nchecks;
}
