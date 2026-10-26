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
CString browserLoginUtf8(const CString &text);
CSimpleArray<int> browserLoginBundlesToWrite(const CSimpleArray<CString> &names,
                                             const CString &box, const CString &list);
typedef CString (*BrowserLoginListReader)(const CString &path);
typedef BOOL (*BrowserLoginWriter)(void *ctx, int index);
BOOL browserLoginWriteHeld(const CSimpleArray<CString> &names, const CString &box,
                           const CString &listFile, BrowserLoginListReader read,
                           BrowserLoginWriter write, void *ctx);
CString browserLoginPasteText(const CSimpleArray<CookieRecord> &cookies, int *dropped);
CString browserLoginProfileDir(const CString &localAppData, unsigned long run);
CString browserLoginProfileRoot(const CString &localAppData);
BOOL browserLoginStaleProfile(const CString &name, unsigned long *run);
int browserLoginCheckCases(CString *err);
/* The harness's own, defined in tools/browser-login-paths-test.cpp. */
char *strdupt_utf8(const char *s);
#else
#include "stdafx.h"
#include "Shell.h"
#include "browser-login.h"
extern "C" {
  #include "HTTrackInterface.h"
};
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
CString browserLoginUtf8(const CString &text) {
  char *converted = strdupt_utf8(text);  // freet() nulls it, so not const
  const CString out(converted != NULL ? converted : (LPCSTR) text);

  freet(converted);
  return out;
}

// see browser-login.h
CSimpleArray<int> browserLoginBundlesToWrite(const CSimpleArray<CString> &names,
                                             const CString &box, const CString &list) {
  CSimpleArray<CString> tokens, urls;
  CSimpleArray<int> write;
  CString line;
  BOOL nul = FALSE;

  splitTokensInArray(tokens, box);
  for(int i = 0 ; i < tokens.GetSize() ; i++) {
    const CString url = browserLoginUtf8(tokens[i]);

    /* A leading + or - makes it a filter, and the engine drops a longer address. */
    if (url[0] != '+' && url[0] != '-' && url.GetLength() < HTS_URLMAXSIZE)
      urls.Add(url);
  }
  /* The engine reads the -%L file as bytes, a line each, with every CR dropped. */
  for(int i = 0 ; i <= list.GetLength() ; i++) {
    if (i == list.GetLength() || list[i] == '\n') {
      /* Its line buffer holds HTS_URLMAXSIZE bytes and the NUL, one more than the box. */
      if (!nul && line.GetLength() <= HTS_URLMAXSIZE)
        urls.Add(line);
      line = CString();
      nul = FALSE;
    } else if (list[i] == '\0') {
      nul = TRUE;
    } else if (list[i] != '\r') {
      line += list[i];
    }
  }
  /* The engine reads the name after the first suffix. Held order is kept, so a later
     login under the same name is written last and wins. */
  for(int k = 0 ; k < names.GetSize() ; k++) {
    for(int i = 0 ; i < urls.GetSize() ; i++) {
      const int at = urls[i].Find(BROWSER_BUNDLE_SUFFIX);

      if (!names[k].IsEmpty() && at >= 0
          && urls[i].Mid(at + (int) strlen(BROWSER_BUNDLE_SUFFIX)) == names[k]) {
        write.Add(k);
        break;
      }
    }
  }
  return write;
}

// see browser-login.h
BOOL browserLoginWriteHeld(const CSimpleArray<CString> &names, const CString &box,
                           const CString &listFile, BrowserLoginListReader read,
                           BrowserLoginWriter write, void *ctx) {
  const CSimpleArray<int> todo = browserLoginBundlesToWrite(names, box, read(listFile));

  for(int i = 0 ; i < todo.GetSize() ; i++) {
    if (!write(ctx, todo[i]))
      return FALSE;
  }
  return TRUE;
}

/* From the engine's htsbundle.c, which refuses a paste of this many bytes or more. */
#define BROWSER_PASTE_MAX (256 * 1024)

// see browser-login.h
CString browserLoginPasteText(const CSimpleArray<CookieRecord> &cookies, int *dropped) {
  CString text;

  *dropped = 0;
  for(int i = 0 ; i < cookies.GetSize() ; i++) {
    CString line;

    /* The text stays under the cap, so the subtraction cannot go negative. */
    if (cookieFileLine(cookies[i], line) != CookieWritable
        || line.GetLength() >= BROWSER_PASTE_MAX - text.GetLength())
      (*dropped)++;
    else
      text += line;
  }
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
    /* Which held logins the finish writes. Held: example.com, then other.org. */
    static const struct { const char *box, *list, *want; } held[] = {
      { "https://example.com/#httrack-bundle=example.com", "", "0" },
      { "https://example.com/#httrack-bundle=example.com "
        "https://other.org/#httrack-bundle=other.org", "", "0 1" },
      /* Back to a project whose addresses name neither */
      { "https://elsewhere.net/", "", "" },
      { "", "", "" },
      /* an address edited after the login still names it, and one cut back does not */
      { "https://shop.example.org/x#httrack-bundle=example.com", "", "0" },
      { "https://example.com/", "", "" },
      /* only the name after the first suffix counts */
      { "https://example.com/#httrack-bundle=example.com.au", "", "" },
      { "https://example.com/#httrack-bundle=x#httrack-bundle=example.com", "", "" },
      /* a filter names no bundle */
      { "+example.com/*#httrack-bundle=example.com", "", "" },
      { "-example.com/*#httrack-bundle=example.com", "", "" },
      /* the -%L file, CR LF or not */
      { "", "https://a.test/\r\nhttps://other.org/#httrack-bundle=other.org\r\n", "1" },
      /* named twice, written once */
      { "https://example.com/#httrack-bundle=example.com",
        "https://example.com/a#httrack-bundle=example.com\n"
        "https://other.org/#httrack-bundle=other.org", "0 1" },
      { NULL, NULL, NULL }
    };
    CSimpleArray<CString> names;

    names.Add("example.com");
    names.Add("other.org");
    for(int k = 0 ; held[k].box != NULL ; k++) {
      const CSimpleArray<int> got = browserLoginBundlesToWrite(names, held[k].box,
                                                               held[k].list);
      CString joined;

      for(int i = 0 ; i < got.GetSize() ; i++) {
        char one[16];

        snprintf(one, sizeof(one), i > 0 ? " %d" : "%d", got[i]);
        joined += one;
      }
      if (joined != held[k].want) {
        snprintf(msg, sizeof(msg), "'%s' and list '%s' write logins '%s', not '%s'",
                 held[k].box, held[k].list, (LPCSTR) joined, held[k].want);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
    /* The box is in the ANSI codepage and the -%L file is read as it is, so only the box
       is converted to the UTF-8 the engine names a bundle in. */
    {
      const CString ansi = "https://caf\xE9.test/#httrack-bundle=caf\xE9.test";
      CSimpleArray<CString> utf8;

      utf8.Add(browserLoginUtf8("caf\xE9.test"));
      if (browserLoginBundlesToWrite(utf8, ansi, "").GetSize() != 1
          || browserLoginBundlesToWrite(utf8, "", ansi).GetSize() != 0
          || browserLoginBundlesToWrite(utf8, "", browserLoginUtf8(ansi)).GetSize() != 1) {
        *err = "a non-ASCII bundle name was matched in the wrong encoding";
        return 0;
      }
      nchecks++;
    }
    /* The engine drops an address of HTS_URLMAXSIZE bytes or more from the box, and a
       -%L line longer than HTS_URLMAXSIZE or holding a NUL. */
    {
      const CString suffix = "#httrack-bundle=example.com";
      const CString base = "https://example.com/";
      const CString fits = base + CString('a', HTS_URLMAXSIZE - 1 - base.GetLength()
                                           - suffix.GetLength()) + suffix;
      const CString atCap = base + CString('a', HTS_URLMAXSIZE - base.GetLength()
                                            - suffix.GetLength()) + suffix;
      const CString past = base + CString('a', HTS_URLMAXSIZE + 1 - base.GetLength()
                                           - suffix.GetLength()) + suffix;
      CString nul = "https://exa";

      nul += '\0';
      nul += "mple.com/";
      nul += suffix;
      const struct { const CString *url; BOOL inList; int want; } longs[] = {
        { &fits, FALSE, 1 }, { &atCap, FALSE, 0 },
        { &fits, TRUE, 1 }, { &atCap, TRUE, 1 }, { &past, TRUE, 0 }, { &nul, TRUE, 0 },
        { NULL, FALSE, 0 }
      };

      for(int k = 0 ; longs[k].url != NULL ; k++) {
        const CString &url = *longs[k].url;
        const int got = browserLoginBundlesToWrite(names, longs[k].inList ? "" : url,
                                                   longs[k].inList ? url : "").GetSize();

        if (got != longs[k].want) {
          snprintf(msg, sizeof(msg), "a %d-byte address in the %s wrote %d logins, not %d",
                   url.GetLength(), longs[k].inList ? "list" : "box", got, longs[k].want);
          *err = msg;
          return 0;
        }
        nchecks++;
      }
      /* A NUL spoils its own line only. */
      CString after = nul;

      after += "\nhttps://example.com/";
      after += suffix;
      if (browserLoginBundlesToWrite(names, "", after).GetSize() != 1) {
        *err = "a NUL in one -%L line hid the next line";
        return 0;
      }
      nchecks++;
    }
  }
  {
    /* The finish writes in held order and stops at the first failure. Held twice under
       one name, the later login is written last, so its file is the one that stays. */
    static const struct { const char *box; int fail; const char *want; BOOL ok; } writes[] = {
      { "https://example.com/#httrack-bundle=example.com", -1, "0 2", TRUE },
      { "https://example.com/#httrack-bundle=example.com "
        "https://other.org/#httrack-bundle=other.org", -1, "0 1 2", TRUE },
      { "https://example.com/#httrack-bundle=example.com "
        "https://other.org/#httrack-bundle=other.org", 1, "0 1", FALSE },
      { "https://other.org/#httrack-bundle=other.org", 1, "1", FALSE },
      { "https://elsewhere.net/", 0, "", TRUE },
      { NULL, -1, NULL, FALSE }
    };
    struct Recorder {
      int fail;
      CString calls;
      static BOOL write(void *ctx, int index) {
        Recorder *const r = (Recorder *) ctx;
        char one[16];

        snprintf(one, sizeof(one), r->calls.IsEmpty() ? "%d" : " %d", index);
        r->calls += one;
        return index != r->fail;
      }
      static CString read(const CString &path) {
        return path == "list.txt" ? CString("https://other.org/#httrack-bundle=other.org")
                                  : CString();
      }
    };
    CSimpleArray<CString> names;

    names.Add("example.com");
    names.Add("other.org");
    names.Add("example.com");
    for(int k = 0 ; writes[k].box != NULL ; k++) {
      Recorder rec;

      rec.fail = writes[k].fail;
      const BOOL ok = browserLoginWriteHeld(names, writes[k].box, "", Recorder::read,
                                            Recorder::write, &rec);

      if (ok != writes[k].ok || rec.calls != writes[k].want) {
        snprintf(msg, sizeof(msg), "'%s' failing at %d wrote '%s' ok=%d, not '%s' ok=%d",
                 writes[k].box, writes[k].fail, (LPCSTR) rec.calls, (int) ok,
                 writes[k].want, (int) writes[k].ok);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
    /* The -%L file is read from the path the finish gives. */
    {
      Recorder rec;

      rec.fail = -1;
      if (!browserLoginWriteHeld(names, "", "list.txt", Recorder::read, Recorder::write,
                                 &rec) || rec.calls != "1") {
        *err = "the finish did not read the -%L file it was given";
        return 0;
      }
      nchecks++;
    }
  }
  {
    /* The engine scopes the paste to the site and caps its own jar, so the paste carries
       every cookie it can, past the jar's 30000 bytes, and stops only at the paste cap. */
    CSimpleArray<CookieRecord> small, big;
    CookieRecord c;
    int dropped = -1;

    c.domain = ".example.com";
    c.value = CString('v', 3000);
    for(int i = 0 ; i < 20 ; i++) {
      char name[16];

      snprintf(name, sizeof(name), "c%d", i);
      c.name = name;
      small.Add(c);
    }
    c.name = "bad\tname";
    small.Add(c);
    if (browserLoginPasteText(small, &dropped).GetLength() <= COOKIE_JAR_MAXLEN
        || dropped != 1) {
      snprintf(msg, sizeof(msg), "a 60 KB harvest with one unwritable cookie dropped %d",
               dropped);
      *err = msg;
      return 0;
    }
    nchecks++;
    for(int i = 0 ; i < 100 ; i++) {
      char name[16];

      snprintf(name, sizeof(name), "c%d", i);
      c.name = name;
      big.Add(c);
    }
    const CString text = browserLoginPasteText(big, &dropped);

    if (text.GetLength() >= BROWSER_PASTE_MAX || dropped == 0
        || text.GetLength() < BROWSER_PASTE_MAX - 3100) {
      snprintf(msg, sizeof(msg), "a 300 KB harvest gave a %d-byte paste, dropping %d",
               text.GetLength(), dropped);
      *err = msg;
      return 0;
    }
    nchecks++;
  }
  return nchecks;
}
