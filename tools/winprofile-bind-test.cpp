/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Xavier Roche and other contributors

   What the option pages save, tested off Windows. The GUI itself only builds under MSVC,
   so --selftest cannot be rerun by a reviewer and cannot be mutated in a pull request.
   This compiles WinHTTrack/winprofile-io.cpp, the real writer, against a stub CString and
   runs five things:

     - winprofileCheckBindings(), the same function --selftest calls;
     - the bytes WINPROFILE_BINDINGS writes, against a key-to-member table written out
       again by hand below. That second table is the whole point. A member swapped between
       two binding rows cancels out in any round trip, because the write and the read both
       follow the swap, and only an expectation that does not come from the binding list
       can see it;
     - a round trip, which catches a read row disagreeing with its write row;
     - the escaped bytes of a value holding '%', '=', TAB, CR and LF.

   Driven by tools/test-winprofile-bind.py. */

#include "winprofile-io-test.h"

/* The option pages, with the members WINPROFILE_BINDINGS names. Declared here rather than
   included, because OptionTab*.h is MFC. */
struct WpStub_m_option1 {
  int      m_link;
  int      m_testall;
  int      m_parseall;
  int      m_htmlfirst;
  int      m_keepwww;
  int      m_keepslashes;
  int      m_keepqueryorder;
};
struct WpStub_m_option3 {
  int      m_cache;
  int      m_filter;
  int      m_travel;
  int      m_travel2;
  int      m_travel3;
  CString  m_stripquery;
};
struct WpStub_m_option9 {
  int      m_norecatch;
  int      m_index;
  int      m_index2;
  int      m_index_mail;
  int      m_logf;
  int      m_warc;
  int      m_warccdx;
  int      m_wacz;
  int      m_singlefile;
  CString  m_singlefilemax;
  int      m_changes;
  int      m_Cache2;
  int      m_logtype;
};
struct WpStub_m_option4 {
  int      m_remt;
  int      m_rems;
  int      m_ka;
  CString  m_pausefiles;
  CString  m_maxretryafter;
  CString  m_connexion;
  CString  m_retry;
  CString  m_timeout;
  CString  m_rate;
};
struct WpStub_m_option8 {
  int      m_robots;
  int      m_sitemap;
  CString  m_sitemapurl;
  CString  m_hostalias;
  int      m_cookies;
  int      m_checktype;
  int      m_parsejava;
  int      m_http10;
  int      m_toler;
  int      m_updhack;
  int      m_urlhack;
  CString  m_cookiesfile;
};
struct WpStub_m_option2 {
  int      m_errpage;
  int      m_external;
  int      m_hidepwd;
  int      m_hidequery;
  int      m_nopurge;
  int      m_build;
  struct { CString m_BuildString; } Bopt;
};
struct WpStub_m_option10 {
  int      m_ftpprox;
  CString  m_proxy;
  CString  m_port;
  int      m_proxytype;
};
struct WpStub_m_option5 {
  CString  m_maxhtml;
  CString  m_othermax;
  CString  m_sizemax;
  CString  m_pausebytes;
  CString  m_maxtime;
  CString  m_maxrate;
  CString  m_depth;
  CString  m_depth2;
  CString  m_maxconn;
  CString  m_maxlinks;
};
struct WpStub_m_option6 {
  CString  m_footer;
  CString  m_other_headers;
  CString  m_default_referer;
};
struct WpStub_m_option7 {
  CString  m_url2;
};
struct WpStub_m_option11 {
  CString  m_ext1;
  CString  m_ext2;
  CString  m_ext3;
  CString  m_ext4;
  CString  m_ext5;
  CString  m_ext6;
  CString  m_ext7;
  CString  m_ext8;
  CString  m_mime1;
  CString  m_mime2;
  CString  m_mime3;
  CString  m_mime4;
  CString  m_mime5;
  CString  m_mime6;
  CString  m_mime7;
  CString  m_mime8;
};

struct WpStubTab {
  WpStub_m_option1 m_option1;
  WpStub_m_option3 m_option3;
  WpStub_m_option9 m_option9;
  WpStub_m_option4 m_option4;
  WpStub_m_option8 m_option8;
  WpStub_m_option2 m_option2;
  WpStub_m_option10 m_option10;
  WpStub_m_option5 m_option5;
  WpStub_m_option6 m_option6;
  WpStub_m_option7 m_option7;
  WpStub_m_option11 m_option11;
};

static WpStubTab stubTab;
static WpStubTab *const maintab = &stubTab;

#include "winprofile-bind.h"

/* Declared by Shell.h on Windows; the harness cannot include that. */
int winprofileListValue(const char *key, int value, int dflt);
int MyWriteProfileIntFile(FILE* fp,CString dummy,CString name,int value);
int MyWriteProfileStringFile(FILE* fp,CString dummy,CString name,CString value);
int MyGetProfileIntFile(FILE* fp,CString dummy,CString name,int value);
CString MyGetProfileStringFile(FILE* fp,CString dummy,CString name,CString value);
CString profile_code(const char* from);
int winprofileCheckBindings(CString *err, int *nskipped);

static int failures = 0;

static void fail(const char *what, const char *got, const char *want) {
  fprintf(stderr, "FAIL: %s\n  got:  %s\n  want: %s\n", what, got, want);
  failures++;
}

/* A member's own name folded to a small number. An int row cannot carry its name, and a
   value taken from the row's key or its position would follow a swap instead of showing
   it. tools/test-winprofile-bind.py reproduces this fold. */
static int wpMemberId(const char *member) {
  int h = 0;

  for(int i=0 ; member[i] != '\0' ; i++)
    h = (h * 31 + (unsigned char) member[i]) % 9973;
  return h;
}

/* Key to member, written out by hand. It is NOT derived from WINPROFILE_BINDINGS, and
   that is what lets it disagree. */
static const struct { const char *key, *member; } wpWitness[] = {
  { "Near", "maintab->m_option1.m_link" },
  { "Test", "maintab->m_option1.m_testall" },
  { "ParseAll", "maintab->m_option1.m_parseall" },
  { "HTMLFirst", "maintab->m_option1.m_htmlfirst" },
  { "KeepWww", "maintab->m_option1.m_keepwww" },
  { "KeepSlashes", "maintab->m_option1.m_keepslashes" },
  { "KeepQueryOrder", "maintab->m_option1.m_keepqueryorder" },
  { "Cache", "maintab->m_option3.m_cache" },
  { "NoRecatch", "maintab->m_option9.m_norecatch" },
  { "Index", "maintab->m_option9.m_index" },
  { "WordIndex", "maintab->m_option9.m_index2" },
  { "MailIndex", "maintab->m_option9.m_index_mail" },
  { "Log", "maintab->m_option9.m_logf" },
  { "RemoveTimeout", "maintab->m_option4.m_remt" },
  { "RemoveRateout", "maintab->m_option4.m_rems" },
  { "KeepAlive", "maintab->m_option4.m_ka" },
  { "FollowRobotsTxt", "maintab->m_option8.m_robots" },
  { "NoErrorPages", "maintab->m_option2.m_errpage" },
  { "NoExternalPages", "maintab->m_option2.m_external" },
  { "NoPwdInPages", "maintab->m_option2.m_hidepwd" },
  { "NoQueryStrings", "maintab->m_option2.m_hidequery" },
  { "NoPurgeOldFiles", "maintab->m_option2.m_nopurge" },
  { "Warc", "maintab->m_option9.m_warc" },
  { "WarcCdx", "maintab->m_option9.m_warccdx" },
  { "Wacz", "maintab->m_option9.m_wacz" },
  { "Sitemap", "maintab->m_option8.m_sitemap" },
  { "SitemapUrl", "maintab->m_option8.m_sitemapurl" },
  { "HostAlias", "maintab->m_option8.m_hostalias" },
  { "SingleFile", "maintab->m_option9.m_singlefile" },
  { "SingleFileMaxSize", "maintab->m_option9.m_singlefilemax" },
  { "Changes", "maintab->m_option9.m_changes" },
  { "Cookies", "maintab->m_option8.m_cookies" },
  { "CheckType", "maintab->m_option8.m_checktype" },
  { "ParseJava", "maintab->m_option8.m_parsejava" },
  { "HTTP10", "maintab->m_option8.m_http10" },
  { "TolerantRequests", "maintab->m_option8.m_toler" },
  { "UpdateHack", "maintab->m_option8.m_updhack" },
  { "URLHack", "maintab->m_option8.m_urlhack" },
  { "CookiesFile", "maintab->m_option8.m_cookiesfile" },
  { "PauseFiles", "maintab->m_option4.m_pausefiles" },
  { "MaxRetryAfter", "maintab->m_option4.m_maxretryafter" },
  { "StoreAllInCache", "maintab->m_option9.m_Cache2" },
  { "LogType", "maintab->m_option9.m_logtype" },
  { "UseHTTPProxyForFTP", "maintab->m_option10.m_ftpprox" },
  { "Build", "maintab->m_option2.m_build" },
  { "PrimaryScan", "maintab->m_option3.m_filter" },
  { "Travel", "maintab->m_option3.m_travel" },
  { "GlobalTravel", "maintab->m_option3.m_travel2" },
  { "RewriteLinks", "maintab->m_option3.m_travel3" },
  { "StripQuery", "maintab->m_option3.m_stripquery" },
  { "BuildString", "maintab->m_option2.Bopt.m_BuildString" },
  { "MaxHtml", "maintab->m_option5.m_maxhtml" },
  { "MaxOther", "maintab->m_option5.m_othermax" },
  { "MaxAll", "maintab->m_option5.m_sizemax" },
  { "MaxWait", "maintab->m_option5.m_pausebytes" },
  { "Sockets", "maintab->m_option4.m_connexion" },
  { "Retry", "maintab->m_option4.m_retry" },
  { "MaxTime", "maintab->m_option5.m_maxtime" },
  { "TimeOut", "maintab->m_option4.m_timeout" },
  { "RateOut", "maintab->m_option4.m_rate" },
  { "Footer", "maintab->m_option6.m_footer" },
  { "OtherHeaders", "maintab->m_option6.m_other_headers" },
  { "DefaultReferer", "maintab->m_option6.m_default_referer" },
  { "MaxRate", "maintab->m_option5.m_maxrate" },
  { "WildCardFilters", "maintab->m_option7.m_url2" },
  { "Proxy", "maintab->m_option10.m_proxy" },
  { "Port", "maintab->m_option10.m_port" },
  { "ProxyType", "maintab->m_option10.m_proxytype" },
  { "Depth", "maintab->m_option5.m_depth" },
  { "ExtDepth", "maintab->m_option5.m_depth2" },
  { "MaxConn", "maintab->m_option5.m_maxconn" },
  { "MaxLinks", "maintab->m_option5.m_maxlinks" },
  { "MIMEDefsExt1", "maintab->m_option11.m_ext1" },
  { "MIMEDefsExt2", "maintab->m_option11.m_ext2" },
  { "MIMEDefsExt3", "maintab->m_option11.m_ext3" },
  { "MIMEDefsExt4", "maintab->m_option11.m_ext4" },
  { "MIMEDefsExt5", "maintab->m_option11.m_ext5" },
  { "MIMEDefsExt6", "maintab->m_option11.m_ext6" },
  { "MIMEDefsExt7", "maintab->m_option11.m_ext7" },
  { "MIMEDefsExt8", "maintab->m_option11.m_ext8" },
  { "MIMEDefsMime1", "maintab->m_option11.m_mime1" },
  { "MIMEDefsMime2", "maintab->m_option11.m_mime2" },
  { "MIMEDefsMime3", "maintab->m_option11.m_mime3" },
  { "MIMEDefsMime4", "maintab->m_option11.m_mime4" },
  { "MIMEDefsMime5", "maintab->m_option11.m_mime5" },
  { "MIMEDefsMime6", "maintab->m_option11.m_mime6" },
  { "MIMEDefsMime7", "maintab->m_option11.m_mime7" },
  { "MIMEDefsMime8", "maintab->m_option11.m_mime8" },
};

#define WP_NWITNESS ((int) (sizeof(wpWitness) / sizeof(wpWitness[0])))

/* Give every member a value naming itself. */
#define WP_ID_INT(key, member, dflt)  member = wpMemberId(#member);
#define WP_ID_TEXT(key, member, dflt) member = #member;

/* Representative values for the round trip, every list index inside its own range. */
#define WP_SET_INT(key, member, dflt)  member = winprofileListValue(key, 1, 1);
#define WP_SET_TEXT(key, member, dflt) member = "rt:" key;

#define WP_ZERO_INT(key, member, dflt)  member = -999;
#define WP_ZERO_TEXT(key, member, dflt) member = "";

#define WP_WRITE_INT(key, member, dflt)  MyWriteProfileIntFile(fp, "", key, member);
#define WP_WRITE_TEXT(key, member, dflt) MyWriteProfileStringFile(fp, "", key, member);

#define WP_READ_INT(key, member, dflt)  member = MyGetProfileIntFile(fp, "", key, dflt);
#define WP_READ_LIST(key, member, dflt) member = \
    winprofileListValue(key, MyGetProfileIntFile(fp, "", key, dflt), dflt);
#define WP_READ_TEXT(key, member, dflt) member = MyGetProfileStringFile(fp, "", key, dflt);

#define WP_CMP_INT(key, member, dflt)  wpCmpInt(key, member, winprofileListValue(key, 1, 1));
#define WP_CMP_TEXT(key, member, dflt) wpCmpText(key, member, CString("rt:" key));

static void wpCmpInt(const char *key, int got, int want) {
  if (got != want) {
    char g[32], w[32];
    sprintf(g, "%d", got);
    sprintf(w, "%d", want);
    fail(key, g, w);
  }
}

static void wpCmpText(const char *key, const CString &got, const CString &want) {
  if (got != want)
    fail(key, (const char *) got, (const char *) want);
}

static CString slurp(const char *path) {
  FILE *const fp = fopen(path, "rb");
  CString out;
  int c;

  if (fp == NULL)
    return out;
  while ((c = fgetc(fp)) != EOF)
    out += (char) c;
  fclose(fp);
  return out;
}

int main(int argc, char **argv) {
  const char *const tmp = (argc > 1) ? argv[1] : "winprofile-bind-test.ini";
  int nskipped = 0, nchecks;
  CString err;

  /* 1. The check --selftest runs, on the same code. */
  nchecks = winprofileCheckBindings(&err, &nskipped);
  if (nchecks == 0) {
    fprintf(stderr, "FAIL: winprofileCheckBindings: %s\n", (const char *) err);
    failures++;
  } else {
    printf("bindings agree with the engine's table on %d checks over %d bound keys"
           " (%d table rows skipped)\n", nchecks, WINPROFILE_BOUND_KEYS, nskipped);
  }

  /* 2. The owners column is comma-separated, and a substring test would read a future
     owner spelled "winrt" or "nowin" as this GUI. No table the engine ships today can show
     that, so the matcher is held against its own cases. */
  {
    static const struct { const char *owners; int want; } owned[] = {
      { "win", 1 }, { "win,web,droid", 1 }, { "web,win", 1 }, { "web,win,droid", 1 },
      { "winrt", 0 }, { "nowin", 0 }, { "winrt,nowin", 0 }, { "web,droid", 0 }, { "", 0 },
      { NULL, 0 }
    };
    int n = 0;

    for(int k=0 ; owned[k].owners != NULL ; k++) {
      if (winprofileOwnedBy(owned[k].owners, "win") != owned[k].want)
        fail("owners", owned[k].owners, owned[k].want ? "win" : "not win");
      else
        n++;
    }
    if (n == 9)
      printf("the owners column reads as a comma-separated list on %d cases\n", n);
  }

  /* 3. The bytes, against the hand-written witness. */
  {
    FILE *fp = fopen(tmp, "wb");
    CString want;

    if (fp == NULL) {
      fprintf(stderr, "FAIL: cannot write %s\n", tmp);
      return 2;
    }
    WINPROFILE_BINDINGS(WP_ID_INT, WP_ID_INT, WP_ID_TEXT, WP_ID_TEXT)
    WINPROFILE_BINDINGS(WP_WRITE_INT, WP_WRITE_INT, WP_WRITE_TEXT, WP_WRITE_TEXT)
    fclose(fp);

    for(int i=0 ; i<WP_NWITNESS ; i++) {
      const winprofile_key_t *const row = winprofileTableKey(wpWitness[i].key);
      char line[512];

      if (row == NULL) {
        fail("witness names a key the table does not", wpWitness[i].key, "a table row");
        continue;
      }
      /* An int row carries the fold of its member name, a text row the name itself. */
      if (strcmp(row->kind, "checkbox") == 0 || strncmp(row->kind, "list:", 5) == 0)
        snprintf(line, sizeof(line), "%s=%d\r\n", wpWitness[i].key,
                 wpMemberId(wpWitness[i].member));
      else
        snprintf(line, sizeof(line), "%s=%s\r\n", wpWitness[i].key,
                 (const char *) profile_code(wpWitness[i].member));
      want += line;
    }
    if (WP_NWITNESS != WINPROFILE_BOUND_KEYS) {
      char g[32];
      sprintf(g, "%d rows", WP_NWITNESS);
      fail("the witness table covers a different number of keys", g, "WINPROFILE_BOUND_KEYS");
    }
    {
      const CString got = slurp(tmp);

      if (got != want) {
        /* Name the first differing line, because a 90-line diff on one stream is unreadable. */
        int at = 0, line = 1;
        while (at < got.GetLength() && at < want.GetLength()
               && got.s[at] == want.s[at]) {
          if (got.s[at] == '\n')
            line++;
          at++;
        }
        fprintf(stderr, "FAIL: the written bytes differ from the witness at line %d,"
                " byte %d\n  got:  %.80s\n  want: %.80s\n", line, at,
                got.s.c_str() + (at > 40 ? at - 40 : 0),
                want.s.c_str() + (at > 40 ? at - 40 : 0));
        failures++;
      } else
        printf("the written bytes match the hand-written witness over %d keys\n",
               WP_NWITNESS);
    }
  }

  /* 4. The round trip: a read row that disagrees with its write row shows up here. */
  {
    FILE *fp = fopen(tmp, "wb");

    if (fp == NULL)
      return 2;
    WINPROFILE_BINDINGS(WP_SET_INT, WP_SET_INT, WP_SET_TEXT, WP_SET_TEXT)
    WINPROFILE_BINDINGS(WP_WRITE_INT, WP_WRITE_INT, WP_WRITE_TEXT, WP_WRITE_TEXT)
    fclose(fp);
    WINPROFILE_BINDINGS(WP_ZERO_INT, WP_ZERO_INT, WP_ZERO_TEXT, WP_ZERO_TEXT)
    fp = fopen(tmp, "rb");
    if (fp == NULL)
      return 2;
    WINPROFILE_BINDINGS(WP_READ_INT, WP_READ_LIST, WP_READ_TEXT, WP_READ_TEXT)
    fclose(fp);
    WINPROFILE_BINDINGS(WP_CMP_INT, WP_CMP_INT, WP_CMP_TEXT, WP_CMP_TEXT)
    if (failures == 0)
      printf("every bound member survives a write and a read back\n");
  }

  /* 5. The escaped bytes. A round trip cannot see an under-escaping writer, because it
     reads back its own output. */
  {
    static const struct { const char *value, *want; } esc[] = {
      { "a=b\r\n\tc%d", "K=a%3db%0d%0a%09c%%d\r\n" },
      { "100% =", "K=100%% %3d\r\n" },
      { "", "K=\r\n" },
      { "plain", "K=plain\r\n" },
      { NULL, NULL }
    };
    int n = 0;

    for(int k=0 ; esc[k].value != NULL ; k++) {
      FILE *fp = fopen(tmp, "wb");
      CString got;

      if (fp == NULL)
        return 2;
      MyWriteProfileStringFile(fp, "", "K", esc[k].value);
      fclose(fp);
      got = slurp(tmp);
      if (got != esc[k].want)
        fail("escaped bytes", (const char *) got, esc[k].want);
      else
        n++;
    }
    if (n == 4)
      printf("escaping writes the expected bytes on %d values\n", n);
  }

  remove(tmp);
  if (failures != 0) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  printf("winprofile bindings ok\n");
  return 0;
}
