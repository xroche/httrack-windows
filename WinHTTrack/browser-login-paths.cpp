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
typedef int (*BrowserLoginNamesLister)(const char *urls, const char *list_file,
                                       char ***names, char *error, size_t error_size);
typedef void (*BrowserLoginNamesFree)(char **names);
typedef BOOL (*BrowserLoginWriter)(void *ctx, int index);
BOOL browserLoginWriteHeld(const CSimpleArray<CString> &names, const CString &box,
                           const CString &listFile, BrowserLoginNamesLister list,
                           BrowserLoginNamesFree release, BrowserLoginWriter write,
                           void *ctx, CString *refused);
CString browserLoginPasteText(const CSimpleArray<CookieRecord> &cookies, int *dropped);
unsigned int browserLoginPasteFlags(BOOL dryRun);
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

// see browser-login.h
CString browserLoginUtf8(const CString &text) {
  char *converted = strdupt_utf8(text);  // freet() nulls it, so not const
  const CString out(converted != NULL ? converted : (LPCSTR) text);

  freet(converted);
  return out;
}

// see browser-login.h
BOOL browserLoginWriteHeld(const CSimpleArray<CString> &names, const CString &box,
                           const CString &listFile, BrowserLoginNamesLister list,
                           BrowserLoginNamesFree release, BrowserLoginWriter write,
                           void *ctx, CString *refused) {
  char **inUse = NULL;
  char why[256];
  BOOL ok = TRUE;

  *refused = CString();
  /* So a user without a browser login never meets the engine's refusal. */
  if (names.GetSize() == 0)
    return TRUE;

  /* The bytes argv would carry for the same box and -%L path. */
  const CString urls = browserLoginUtf8(box);
  const CString path = browserLoginUtf8(listFile);

  why[0] = '\0';
  if (list(urls, path.IsEmpty() ? NULL : (LPCSTR) path, &inUse, why, sizeof(why)) < 0) {
    *refused = why[0] != '\0' ? CString(why) : CString("the addresses were refused");
    return FALSE;
  }
  /* Held order, so a later login under the same name is written last and wins. */
  for(int k = 0 ; ok && k < names.GetSize() ; k++) {
    for(int i = 0 ; inUse[i] != NULL ; i++) {
      if (names[k] == inUse[i]) {
        ok = write(ctx, k);
        break;
      }
    }
  }
  release(inUse);
  return ok;
}

/* From the engine's htsbundle.c, which refuses a paste of this many bytes or more. */
#define BROWSER_PASTE_MAX (256 * 1024)

/* A comment first, because a paste starting with "curl" reads as a cURL command. */
#define BROWSER_PASTE_HEAD "# Netscape HTTP Cookie File\n"

// see browser-login.h
unsigned int browserLoginPasteFlags(BOOL dryRun) {
  /* A login with no cookie still carries the address and the identity. */
  return HTS_PASTE_KEEP_EMPTY | (dryRun ? HTS_PASTE_DRY_RUN : 0u);
}

// see browser-login.h
CString browserLoginPasteText(const CSimpleArray<CookieRecord> &cookies, int *dropped) {
  CString text = BROWSER_PASTE_HEAD;

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

/* The cases' stand-in for hts_bundle_names_in_use(), and what it was asked. */
static struct {
  const char *const *inUse;
  const char *refuse;
  int calls, frees;
  CString urls, list;
  BOOL listNull;
} fakeNames;

static void fakeNamesReset(const char *const *inUse, const char *refuse) {
  fakeNames.inUse = inUse;
  fakeNames.refuse = refuse;
  fakeNames.calls = fakeNames.frees = 0;
  fakeNames.urls = fakeNames.list = CString();
  fakeNames.listNull = FALSE;
}

static int fakeNamesInUse(const char *urls, const char *list_file, char ***names,
                          char *error, size_t error_size) {
  int n = 0;

  fakeNames.calls++;
  fakeNames.urls = urls;
  fakeNames.list = list_file != NULL ? list_file : "";
  fakeNames.listNull = list_file == NULL;
  *names = NULL;
  if (fakeNames.refuse != NULL) {
    snprintf(error, error_size, "%s", fakeNames.refuse);
    return -1;
  }
  while (n < 3 && fakeNames.inUse[n] != NULL)
    n++;
  /* Three slots and a NULL, as the engine's array ends. */
  static const char *list[4];

  for(int i = 0 ; i < 4 ; i++)
    list[i] = i < n ? fakeNames.inUse[i] : NULL;
  *names = (char **) list;
  return n;
}

static void fakeNamesFree(char **names) {
  if (names != NULL)
    fakeNames.frees++;
}

/* Records which held logins the finish wrote, and fails the write of FAIL. */
struct BrowserLoginRecorder {
  int fail;
  CString calls;
  static BOOL write(void *ctx, int index) {
    BrowserLoginRecorder *const r = (BrowserLoginRecorder *) ctx;
    char one[16];

    snprintf(one, sizeof(one), r->calls.IsEmpty() ? "%d" : " %d", index);
    r->calls += one;
    return index != r->fail;
  }
};

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
    /* Which held logins the finish writes, from the names the engine says are in use.
       Held: example.com, other.org, then example.com again. */
    static const struct {
      const char *inUse[3], *refuse;
      int fail;
      const char *want;
      BOOL ok;
    } writes[] = {
      { { "example.com" }, NULL, -1, "0 2", TRUE },
      /* held order, not the engine's */
      { { "other.org", "example.com" }, NULL, -1, "0 1 2", TRUE },
      { { "elsewhere.net" }, NULL, -1, "", TRUE },
      { { NULL }, NULL, -1, "", TRUE },
      /* a name matches whole or not at all */
      { { "example.com.au", "example" }, NULL, -1, "", TRUE },
      /* the first failed write stops the finish */
      { { "example.com", "other.org" }, NULL, 1, "0 1", FALSE },
      { { "other.org" }, NULL, 1, "1", FALSE },
      /* a refusal writes nothing and stops the save */
      { { "example.com" }, "address list holds an option", -1, "", FALSE },
      { { NULL }, NULL, -1, NULL, FALSE }
    };
    CSimpleArray<CString> names;

    names.Add("example.com");
    names.Add("other.org");
    names.Add("example.com");
    for(int k = 0 ; writes[k].want != NULL ; k++) {
      BrowserLoginRecorder rec;
      CString refused;

      rec.fail = writes[k].fail;
      fakeNamesReset(writes[k].inUse, writes[k].refuse);
      const BOOL ok = browserLoginWriteHeld(names, "https://example.com/", "",
                                            fakeNamesInUse, fakeNamesFree,
                                            BrowserLoginRecorder::write, &rec, &refused);

      if (ok != writes[k].ok || rec.calls != writes[k].want
          || refused != (writes[k].refuse != NULL ? writes[k].refuse : "")
          || fakeNames.calls != 1 || fakeNames.frees != (writes[k].refuse != NULL ? 0 : 1)) {
        snprintf(msg, sizeof(msg), "names case %d wrote '%s' ok=%d refused '%s' asked %d "
                 "freed %d, not '%s' ok=%d", k, (LPCSTR) rec.calls, (int) ok,
                 (LPCSTR) refused, fakeNames.calls, fakeNames.frees, writes[k].want,
                 (int) writes[k].ok);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
    /* No held login, no question, so the engine's refusal cannot block a plain save. */
    {
      static const char *const none[3] = { NULL };
      CSimpleArray<CString> noLogin;
      BrowserLoginRecorder rec;
      CString refused;

      rec.fail = -1;
      fakeNamesReset(none, "address list holds an option");
      if (!browserLoginWriteHeld(noLogin, "-O", "", fakeNamesInUse, fakeNamesFree,
                                 BrowserLoginRecorder::write, &rec, &refused)
          || fakeNames.calls != 0 || !refused.IsEmpty() || !rec.calls.IsEmpty()) {
        *err = "a save with no held login asked the engine for bundle names";
        return 0;
      }
      nchecks++;
    }
    /* The box and the -%L path reach the engine as the UTF-8 argv carries, and an empty
       path as no list at all. */
    {
      static const char *const none[3] = { NULL };
      const CString box = "https://caf\xE9.test/#httrack-bundle=caf\xE9.test -*.gif";
      const CString path = "C:\\caf\xE9\\urls.txt";
      BrowserLoginRecorder rec;
      CString refused;

      rec.fail = -1;
      fakeNamesReset(none, NULL);
      browserLoginWriteHeld(names, box, path, fakeNamesInUse, fakeNamesFree,
                            BrowserLoginRecorder::write, &rec, &refused);
      if (fakeNames.urls != browserLoginUtf8(box) || fakeNames.urls == box
          || fakeNames.list != browserLoginUtf8(path) || fakeNames.list == path) {
        *err = "the engine was asked about the addresses in the wrong encoding";
        return 0;
      }
      nchecks++;
      fakeNamesReset(none, NULL);
      browserLoginWriteHeld(names, box, "", fakeNamesInUse, fakeNamesFree,
                            BrowserLoginRecorder::write, &rec, &refused);
      if (!fakeNames.listNull) {
        *err = "an empty -%L path reached the engine as a file name";
        return 0;
      }
      nchecks++;
    }
  }
  {
    /* The engine scopes the paste to the site and caps its own jar, so the paste carries
       every cookie it can, past the jar's 30000 bytes, and stops only at the paste cap. */
    CSimpleArray<CookieRecord> few, many;
    CookieRecord c;
    int dropped = -1;

    c.domain = ".example.com";
    c.value = CString('v', 3000);
    for(int i = 0 ; i < 20 ; i++) {
      char name[16];

      snprintf(name, sizeof(name), "c%d", i);
      c.name = name;
      few.Add(c);
    }
    c.name = "bad\tname";
    few.Add(c);
    if (browserLoginPasteText(few, &dropped).GetLength() <= COOKIE_JAR_MAXLEN
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
      many.Add(c);
    }
    const CString text = browserLoginPasteText(many, &dropped);

    if (text.GetLength() >= BROWSER_PASTE_MAX || dropped == 0
        || text.GetLength() < BROWSER_PASTE_MAX - 3100) {
      snprintf(msg, sizeof(msg), "a 300 KB harvest gave a %d-byte paste, dropping %d",
               text.GetLength(), dropped);
      *err = msg;
      return 0;
    }
    nchecks++;
  }
  {
    /* A paste one byte under the cap fits, one at the cap drops the cookie that overflows,
       and a later cookie that still fits is kept. */
    CookieRecord fill, mid, tiny;
    CString line;

    fill.domain = mid.domain = tiny.domain = ".example.com";
    fill.value = CString('f', 2000);
    mid.name = "mid";
    mid.value = CString('m', 50);
    tiny.name = "t";
    if (cookieFileLine(mid, line) != CookieWritable) {
      *err = "the paste cap cases cannot write their own cookie";
      return 0;
    }
    const int midLen = line.GetLength();

    cookieFileLine(tiny, line);
    const int tinyLen = line.GetLength();
    CSimpleArray<CookieRecord> head;
    int used = (int) strlen(BROWSER_PASTE_HEAD);

    for(int i = 0 ; ; i++) {
      char name[16];

      snprintf(name, sizeof(name), "f%d", i);
      fill.name = name;
      cookieFileLine(fill, line);
      if (used + line.GetLength() > BROWSER_PASTE_MAX - 1 - midLen - 200)
        break;
      head.Add(fill);
      used += line.GetLength();
    }
    /* PAD tops HEAD up so the cookies through MID sum to one byte under the cap. */
    CookieRecord pad;

    pad.domain = ".example.com";
    pad.name = "pad";
    cookieFileLine(pad, line);
    const int padValue = BROWSER_PASTE_MAX - 1 - used - midLen - line.GetLength();
    const struct { int extra, after, want, dropped; } edges[] = {
      /* EXTRA bytes on PAD, then AFTER: 1 is MID, 2 is MID MID TINY */
      { 0, 1, BROWSER_PASTE_MAX - 1, 0 },
      { 1, 1, BROWSER_PASTE_MAX - midLen, 1 },
      { 1, 2, BROWSER_PASTE_MAX - midLen + tinyLen, 2 },
      { 0, 0, 0, 0 }
    };

    for(int k = 0 ; edges[k].after != 0 ; k++) {
      CSimpleArray<CookieRecord> all = head;
      int dropped = -1;

      pad.value = CString('p', padValue + edges[k].extra);
      all.Add(pad);
      all.Add(mid);
      if (edges[k].after == 2) {
        all.Add(mid);
        all.Add(tiny);
      }
      const int got = browserLoginPasteText(all, &dropped).GetLength();

      if (got != edges[k].want || dropped != edges[k].dropped) {
        snprintf(msg, sizeof(msg), "paste cap case %d gave %d bytes dropping %d, "
                 "expected %d dropping %d", k, got, dropped, edges[k].want, edges[k].dropped);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }
  {
    /* The engine reads a paste starting with "curl" as a cURL command. */
    CSimpleArray<CookieRecord> one;
    CookieRecord c;
    int dropped = -1;

    c.domain = "curl";
    c.name = "id";
    one.Add(c);
    const CString text = browserLoginPasteText(one, &dropped);

    if (text.Left(1) != "#" || text.Find("\ncurl\t") < 0 || dropped != 0) {
      *err = "a cookie for the host curl starts the paste, which then reads as cURL";
      return 0;
    }
    nchecks++;
  }
  /* A check-only login must write nothing, and every login keeps one with no cookie. */
  if (browserLoginPasteFlags(TRUE) != (HTS_PASTE_KEEP_EMPTY | HTS_PASTE_DRY_RUN)
      || browserLoginPasteFlags(FALSE) != HTS_PASTE_KEEP_EMPTY) {
    *err = "a login asks the engine for the wrong paste flags";
    return 0;
  }
  nchecks++;
  return nchecks;
}
