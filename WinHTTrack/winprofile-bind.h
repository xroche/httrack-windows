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
/*       hts-cache/winprofile.ini field bindings                */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef WINPROFILE_BIND_H
#define WINPROFILE_BIND_H

#include <ctype.h>
#include <string.h>

/* The engine's generated key table, from winprofile-keys.tsv. It states the contract
   WebHTTrack and HTTrack for Android read this file under. */
#include "winprofile-keys.h"

/* WINPROFILE_BINDINGS lists every saved key, its member, and the value an absent key
   means. Write_profile, Read_profile and --selftest all expand it, so each key is named,
   typed and defaulted once. A row's macro is its kind in the table above, which --selftest
   holds this list against. NUMBER and TEXT both keep their value as text, because a
   substituted number cannot be told from a typed one. */
#define WINPROFILE_BINDINGS(CHECKBOX, LIST, NUMBER, TEXT)                      \
  CHECKBOX("Near", maintab->m_option1.m_link, 0)                               \
  CHECKBOX("Test", maintab->m_option1.m_testall, 0)                            \
  CHECKBOX("ParseAll", maintab->m_option1.m_parseall, 1)                       \
  CHECKBOX("HTMLFirst", maintab->m_option1.m_htmlfirst, 0)                     \
  CHECKBOX("KeepWww", maintab->m_option1.m_keepwww, 0)                         \
  CHECKBOX("KeepSlashes", maintab->m_option1.m_keepslashes, 0)                 \
  CHECKBOX("KeepQueryOrder", maintab->m_option1.m_keepqueryorder, 0)           \
  CHECKBOX("Cache", maintab->m_option3.m_cache, 1)                             \
  CHECKBOX("NoRecatch", maintab->m_option9.m_norecatch, 0)                     \
  CHECKBOX("Index", maintab->m_option9.m_index, 1)                             \
  CHECKBOX("WordIndex", maintab->m_option9.m_index2, 0)                        \
  CHECKBOX("MailIndex", maintab->m_option9.m_index_mail, 0)                    \
  CHECKBOX("Log", maintab->m_option9.m_logf, 1)                                \
  CHECKBOX("RemoveTimeout", maintab->m_option4.m_remt, 0)                      \
  CHECKBOX("RemoveRateout", maintab->m_option4.m_rems, 0)                      \
  CHECKBOX("KeepAlive", maintab->m_option4.m_ka, 1)                            \
  LIST("FollowRobotsTxt", maintab->m_option8.m_robots, 2)                      \
  CHECKBOX("NoErrorPages", maintab->m_option2.m_errpage, 0)                    \
  CHECKBOX("NoExternalPages", maintab->m_option2.m_external, 0)                \
  CHECKBOX("NoPwdInPages", maintab->m_option2.m_hidepwd, 0)                    \
  CHECKBOX("NoQueryStrings", maintab->m_option2.m_hidequery, 0)                \
  CHECKBOX("NoPurgeOldFiles", maintab->m_option2.m_nopurge, 0)                 \
  CHECKBOX("Warc", maintab->m_option9.m_warc, 0)                               \
  CHECKBOX("WarcCdx", maintab->m_option9.m_warccdx, 0)                         \
  CHECKBOX("Wacz", maintab->m_option9.m_wacz, 0)                               \
  CHECKBOX("Sitemap", maintab->m_option8.m_sitemap, 0)                         \
  TEXT("SitemapUrl", maintab->m_option8.m_sitemapurl, "")                      \
  TEXT("HostAlias", maintab->m_option8.m_hostalias, "")                        \
  CHECKBOX("SingleFile", maintab->m_option9.m_singlefile, 0)                   \
  TEXT("SingleFileMaxSize", maintab->m_option9.m_singlefilemax, "")            \
  CHECKBOX("Changes", maintab->m_option9.m_changes, 0)                         \
  CHECKBOX("Cookies", maintab->m_option8.m_cookies, 1)                         \
  LIST("CheckType", maintab->m_option8.m_checktype, 1)                         \
  CHECKBOX("ParseJava", maintab->m_option8.m_parsejava, 1)                     \
  CHECKBOX("HTTP10", maintab->m_option8.m_http10, 0)                           \
  CHECKBOX("TolerantRequests", maintab->m_option8.m_toler, 0)                  \
  CHECKBOX("UpdateHack", maintab->m_option8.m_updhack, 1)                      \
  CHECKBOX("URLHack", maintab->m_option8.m_urlhack, 1)                         \
  TEXT("CookiesFile", maintab->m_option8.m_cookiesfile, "")                    \
  TEXT("PauseFiles", maintab->m_option4.m_pausefiles, "")                      \
  TEXT("MaxRetryAfter", maintab->m_option4.m_maxretryafter, "")                \
  CHECKBOX("StoreAllInCache", maintab->m_option9.m_Cache2, 0)                  \
  LIST("LogType", maintab->m_option9.m_logtype, 0)                             \
  CHECKBOX("UseHTTPProxyForFTP", maintab->m_option10.m_ftpprox, 1)             \
  LIST("Build", maintab->m_option2.m_build, 0)                                 \
  LIST("PrimaryScan", maintab->m_option3.m_filter, 3)                          \
  LIST("Travel", maintab->m_option3.m_travel, 1)                               \
  LIST("GlobalTravel", maintab->m_option3.m_travel2, 0)                        \
  LIST("RewriteLinks", maintab->m_option3.m_travel3, 0)                        \
  TEXT("StripQuery", maintab->m_option3.m_stripquery, "")                      \
  TEXT("BuildString", maintab->m_option2.Bopt.m_BuildString, "%h%p/%n%q.%t")   \
  TEXT("MaxHtml", maintab->m_option5.m_maxhtml, "")                            \
  TEXT("MaxOther", maintab->m_option5.m_othermax, "")                          \
  TEXT("MaxAll", maintab->m_option5.m_sizemax, "")                             \
  TEXT("MaxWait", maintab->m_option5.m_pausebytes, "")                         \
  NUMBER("Sockets", maintab->m_option4.m_connexion, "")                        \
  TEXT("Retry", maintab->m_option4.m_retry, "")                                \
  TEXT("MaxTime", maintab->m_option5.m_maxtime, "")                            \
  TEXT("TimeOut", maintab->m_option4.m_timeout, "")                            \
  TEXT("RateOut", maintab->m_option4.m_rate, "")                               \
  TEXT("Footer", maintab->m_option6.m_footer, HTS_DEFAULT_FOOTER)              \
  TEXT("OtherHeaders", maintab->m_option6.m_other_headers, "")                 \
  TEXT("DefaultReferer", maintab->m_option6.m_default_referer, "")             \
  NUMBER("MaxRate", maintab->m_option5.m_maxrate, "")                          \
  TEXT("WildCardFilters", maintab->m_option7.m_url2,                           \
       "+*.png +*.gif +*.jpg +*.jpeg +*.css +*.js"                             \
       " -ad.doubleclick.net/* -mime:application/foobar")                      \
  TEXT("Proxy", maintab->m_option10.m_proxy, "")                               \
  TEXT("Port", maintab->m_option10.m_port, "")                                 \
  LIST("ProxyType", maintab->m_option10.m_proxytype, 0)                        \
  TEXT("Depth", maintab->m_option5.m_depth, "")                                \
  TEXT("ExtDepth", maintab->m_option5.m_depth2, "")                            \
  TEXT("MaxConn", maintab->m_option5.m_maxconn, "")                            \
  TEXT("MaxLinks", maintab->m_option5.m_maxlinks, "")                          \
  TEXT("MIMEDefsExt1", maintab->m_option11.m_ext1, "")                         \
  TEXT("MIMEDefsExt2", maintab->m_option11.m_ext2, "")                         \
  TEXT("MIMEDefsExt3", maintab->m_option11.m_ext3, "")                         \
  TEXT("MIMEDefsExt4", maintab->m_option11.m_ext4, "")                         \
  TEXT("MIMEDefsExt5", maintab->m_option11.m_ext5, "")                         \
  TEXT("MIMEDefsExt6", maintab->m_option11.m_ext6, "")                         \
  TEXT("MIMEDefsExt7", maintab->m_option11.m_ext7, "")                         \
  TEXT("MIMEDefsExt8", maintab->m_option11.m_ext8, "")                         \
  TEXT("MIMEDefsMime1", maintab->m_option11.m_mime1, "")                       \
  TEXT("MIMEDefsMime2", maintab->m_option11.m_mime2, "")                       \
  TEXT("MIMEDefsMime3", maintab->m_option11.m_mime3, "")                       \
  TEXT("MIMEDefsMime4", maintab->m_option11.m_mime4, "")                       \
  TEXT("MIMEDefsMime5", maintab->m_option11.m_mime5, "")                       \
  TEXT("MIMEDefsMime6", maintab->m_option11.m_mime6, "")                       \
  TEXT("MIMEDefsMime7", maintab->m_option11.m_mime7, "")                       \
  TEXT("MIMEDefsMime8", maintab->m_option11.m_mime8, "")

/* Pinned, never floored, because a count with slack is how a loop stops running and
   still prints "ok". The engine's table is checked out fresh, so a row it gains for
   another front end moves WINPROFILE_SKIPPED_ROWS and reds this on purpose. The three key
   counts add up to WINPROFILE_KEY_COUNT. */
#define WINPROFILE_BOUND_KEYS 88
#define WINPROFILE_HANDWRITTEN_KEYS 8
#define WINPROFILE_SKIPPED_ROWS 11
#define WINPROFILE_BINDING_CHECKS 468
#define WINPROFILE_LIST_COMBO_COUNT 9

/* The keys no binding can carry. These stay hand-written: ProfileFormat is a constant,
   Dos packs two checkboxes into one value, Category reads and writes different members,
   AcceptLanguage and UserID default to a run-time value, and the three Current* keys
   belong to the first dialog, not a page. tools/test-winprofile-bind.py holds each one
   against both profile functions, because nothing here can see whether a key named below
   is really saved. */
#define WINPROFILE_HANDWRITTEN(KEY)                                            \
  KEY("ProfileFormat")                                                         \
  KEY("Dos")                                                                   \
  KEY("Category")                                                              \
  KEY("AcceptLanguage")                                                        \
  KEY("UserID")                                                                \
  KEY("CurrentUrl")                                                            \
  KEY("CurrentAction")                                                         \
  KEY("CurrentURLList")

/* Each LIST key above and the catalog list SetCombo() fills its combo from. The entry
   count reaches the table through nothing the compiler sees, so --selftest counts it. */
#define WINPROFILE_LIST_COMBOS(COMBO)                                          \
  COMBO("Build", LISTDEF_3)                                                    \
  COMBO("PrimaryScan", LISTDEF_4)                                              \
  COMBO("Travel", LISTDEF_5)                                                   \
  COMBO("GlobalTravel", LISTDEF_6)                                             \
  COMBO("RewriteLinks", LISTDEF_11)                                            \
  COMBO("CheckType", LISTDEF_7)                                                \
  COMBO("FollowRobotsTxt", LISTDEF_8)                                          \
  COMBO("LogType", LISTDEF_9)                                                  \
  COMBO("CurrentAction", LISTDEF_10)

/* The one LIST key with no catalog list, because its three entries come from the .rc's
   DLGINIT. Named here so the completeness check counts it instead of passing over it. */
#define WINPROFILE_COMBO_FROM_RC "ProxyType"

/* How many entries SetCombo() would put in a combo from LANG_STRING. SetCombo() in
   Shell.cpp holds the twin split that fills the combo, so keep the two the same, or this
   count stops matching what the combo shows. isspace(), because that is what the
   CString::Trim calls over there drop. Here rather than beside it, because
   tools/catalog-lists-test.cpp counts the same entries off Windows. */
static inline int countComboEntries(const char *lang_string) {
  const char *p = lang_string;
  int n = 0;

  while (*p != '\0') {
    const char *const nl = strchr(p, '\n');
    const char *const stop = (nl != NULL) ? nl : p + strlen(p);
    const char *start = p;
    const char *end = stop;

    while (start < end && isspace((unsigned char) *start))
      start++;
    while (end > start && isspace((unsigned char) end[-1]))
      end--;
    if (end > start)
      n++;
    p = (nl != NULL) ? nl + 1 : stop;
  }
  return n;
}

/* The table's row for KEY, or NULL when it states none. */
static inline const winprofile_key_t *winprofileTableKey(const char *key) {
  for(int i=0 ; i<WINPROFILE_KEY_COUNT ; i++) {
    if (strcmp(winprofile_keys[i].key, key) == 0)
      return &winprofile_keys[i];
  }
  return NULL;
}

/* TRUE when OWNERS, a comma-separated list, carries WHO as a whole entry. A substring
   test would also answer yes to a future owner spelled "winrt" or "nowin". */
static inline BOOL winprofileOwnedBy(const char *owners, const char *who) {
  const size_t len = strlen(who);
  const char *p = owners;

  while (*p != '\0') {
    const char *const comma = strchr(p, ',');
    const size_t span = (comma != NULL) ? (size_t) (comma - p) : strlen(p);

    if (span == len && strncmp(p, who, len) == 0)
      return TRUE;
    if (comma == NULL)
      break;
    p = comma + 1;
  }
  return FALSE;
}

/* TRUE when the table types KEY as a list, filling in the index of its first entry and
   how many entries it has. The kind column spells that as "list:<base>:<count>". */
static inline BOOL winprofileListRange(const char *key, int *base, int *count) {
  const winprofile_key_t *const row = winprofileTableKey(key);
  const char *const kind = (row != NULL) ? row->kind : "";
  const char *sep;

  if (strncmp(kind, "list:", 5) != 0)
    return FALSE;
  *base = atoi(kind + 5);
  sep = strchr(kind + 5, ':');
  *count = (sep != NULL) ? atoi(sep + 1) : 0;
  return *count > 0;
}

#endif
