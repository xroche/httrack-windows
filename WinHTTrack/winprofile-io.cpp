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
/*       winprofile.ini codec, line I/O and binding check       */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Split out of Shell.cpp so tools/winprofile-bind-test.cpp can compile the real writer.
   What that test asserts is the bytes the other front ends read. Nothing here needs MFC
   beyond CString, and linput() is the only engine call. */

#ifdef WINPROFILE_IO_TEST
#include "winprofile-io-test.h"     /* CString, linput() and strcatbuff() for the harness */
#else
#include "stdafx.h"
#include "Shell.h"
extern "C" {
  #include "HTTrackInterface.h"
};
#endif
#include "winprofile-bind.h"

// Ecriture profiles
CString profile_code(const char* from) {
  int i;
  CString result;
  for(i = 0 ; from[i] != '\0' ; i++) {
    switch(from[i]) {
    case '%': 
      result += '%';
      result += '%';
      break;
    case '=': 
      result += '%';
      result += '3';
      result += 'd';
      break;
    case 13:
      result += '%';
      result += '0';
      result += 'd';
      break;
    case 10:
      result += '%';
      result += '0';
      result += 'a';
      break;
    case 9:
      result += '%';
      result += '0';
      result += '9';
      break;
    default:
      result += from[i];
      break;
    }
  }
  return result;
}
CString profile_decode(const char* from) {
  int j;
  CString result;
  for(j = 0 ; from[j] != '\0' ; ) {  // oui oui
    if (from[j]=='%') {
      if (from[j + 1] == '%') {
        result += '%';
        j+=2;
      } else if (from[j + 1] == '\0' || from[j + 2] == '\0') {
        result += ' ';    // a truncated escape has no second digit to step over
        break;
      } else {
        if (strncmp(from+j+1,"0d", 2)==0)
          result += (char) 13;
        else if (strncmp(from+j+1,"0a", 2)==0)
          result += (char) 10;
        else if (strncmp(from+j+1,"09", 2)==0)
          result += (char) 9;
        else if (strncmp(from+j+1,"3d", 2)==0)
          result += '=';
        else
          result += ' ';
        j+=3;
      }
    } else
      result += from[j++];
  }
  return result;
}
int MyWriteProfileIntFile(FILE* fp,CString dummy,CString name,int value) {
  if (fp) {
    fprintf(fp,"%s=%d\x0d\x0a", (LPCTSTR)name, value);
  }
  return 0;
}
int MyWriteProfileStringFile(FILE* fp,CString dummy,CString name,CString value) {
  if (fp) {
    fprintf(fp,"%s=%s\x0d\x0a", (LPCTSTR)name, profile_code(value.GetBuffer(0)).GetBuffer(0));
  }
  return 0;
}
int MyGetProfileIntFile(FILE* fp,CString dummy,CString name,int value) {
  if (fp) {
    char srch[256];
    fseek(fp,0,SEEK_SET);
    sprintf(srch,"%s=",(LPCTSTR)name);
    while(!feof(fp)) {
      char s[2048]; s[0]='\0';
      linput(fp,s,2000);
      if (strlen(s)==0)     // EOF
        return value;
      if (strncmp(s,srch,strlen(srch)) == 0) {    // ligne reconnue
        int val;
        if (sscanf(s+strlen(srch),"%d",&val) == 1)
          return val;
        else
          return value;
      }
    }
    return value;
  } else return value;
}
CString MyGetProfileStringFile(FILE* fp,CString dummy,CString name,CString value) {
  if (fp) {
    char srch[256];
    fseek(fp,0,SEEK_SET);
    sprintf(srch,"%s",(LPCTSTR)name);
    strcatbuff(srch,"=");
    while(!feof(fp)) {
      char s[32768]; s[0]='\0';
      linput(fp,s,32000);
      if (strlen(s)==0)     // EOF
        return value;
      if (strncmp(s,srch,strlen(srch)) == 0) {    // ligne reconnue
        return profile_decode(s+strlen(srch));
      }
    }
    return value;
  } else return value;
}
/* VALUE when the shared table offers KEY that entry, DFLT otherwise. Another front end, or
   a catalog short enough for DDX to store -1, can put an index in the file no combo has. */
int winprofileListValue(const char *key, int value, int dflt) {
  int base, count;

  if (!winprofileListRange(key, &base, &count))
    return value;
  return (value >= base && value < base + count) ? value : dflt;
}


/* One row of WINPROFILE_BINDINGS as data: the key, the kind it claims, and the value an
   absent key means, held as text for NUMBER and TEXT and as a number for the other two. */
typedef struct {
  const char *key, *kind, *text;
  int num;
} winprofile_bound_t;

#define WP_BOUND_CHECKBOX(key, member, dflt) { key, "checkbox", NULL, dflt },
#define WP_BOUND_LIST(key, member, dflt)     { key, "list",     NULL, dflt },
#define WP_BOUND_NUMBER(key, member, dflt)   { key, "number",   dflt, 0 },
#define WP_BOUND_TEXT(key, member, dflt)     { key, "string",   dflt, 0 },
static const winprofile_bound_t winprofile_bound[] = {
  WINPROFILE_BINDINGS(WP_BOUND_CHECKBOX, WP_BOUND_LIST, WP_BOUND_NUMBER, WP_BOUND_TEXT)
};
#undef WP_BOUND_CHECKBOX
#undef WP_BOUND_LIST
#undef WP_BOUND_NUMBER
#undef WP_BOUND_TEXT

#define WP_HAND_KEY(key) key,
static const char *const winprofile_byhand[] = { WINPROFILE_HANDWRITTEN(WP_HAND_KEY) };
#undef WP_HAND_KEY

#define WP_COMBO_KEY(key, listdef) key,
static const char *const winprofile_combos[] = { WINPROFILE_LIST_COMBOS(WP_COMBO_KEY) };
#undef WP_COMBO_KEY

#define WP_NBOUND  ((int) (sizeof(winprofile_bound) / sizeof(winprofile_bound[0])))
#define WP_NHAND   ((int) (sizeof(winprofile_byhand) / sizeof(winprofile_byhand[0])))
#define WP_NCOMBO  ((int) (sizeof(winprofile_combos) / sizeof(winprofile_combos[0])))

/* The read fallback's own cases, with the values that must pass through untouched,
   because a filter dropping every value would satisfy the out-of-range rows on its own. */
typedef struct {
  const char *key;
  int value, dflt, want;
} winprofile_fallback_t;

static const winprofile_fallback_t winprofile_fallbacks[] = {
  { "CheckType", 0, 1, 0 }, { "CheckType", 2, 1, 2 },
  { "CheckType", 3, 1, 1 }, { "CheckType", -1, 1, 1 },
  { "ProxyType", 2, 0, 2 }, { "ProxyType", 3, 0, 0 },
  { "Build", 14, 0, 14 }, { "Build", 15, 0, 0 },
  /* winprofileListValue leaves any other key's value unchanged. */
  { "Near", 7, 0, 7 }, { "MaxRate", 7, 0, 7 }, { "Nosuchkey", 7, 0, 7 },
};

static BOOL wpNamed(const char *const *list, int n, const char *key) {
  for(int i=0 ; i<n ; i++) {
    if (strcmp(list[i], key) == 0)
      return TRUE;
  }
  return FALSE;
}

static const winprofile_bound_t *wpBound(const char *key) {
  for(int i=0 ; i<WP_NBOUND ; i++) {
    if (strcmp(winprofile_bound[i].key, key) == 0)
      return &winprofile_bound[i];
  }
  return NULL;
}

int winprofileCheckBindings(CString *err, int *nskipped) {
  char msg[1024];
  int nchecks = 0;

  *nskipped = 0;
  for(int k=0 ; k<WP_NBOUND ; k++) {
    const winprofile_bound_t *const b = &winprofile_bound[k];
    const winprofile_key_t *const row = winprofileTableKey(b->key);
    const BOOL isList = strcmp(b->kind, "list") == 0;
    int base = 0, count = 0;
    CString mine;

    if (row == NULL || !winprofileOwnedBy(row->owners, "win")) {
      snprintf(msg, sizeof(msg), "the engine's table does not give '%s' to this GUI", b->key);
      *err = msg;
      return 0;
    }
    /* The kind is what decides how a 0 reads, so a disagreement loses a setting. */
    if (isList ? (!winprofileListRange(b->key, &base, &count) || base != 0)
               : strcmp(b->kind, row->kind) != 0) {
      snprintf(msg, sizeof(msg), "'%s' is bound as %s, the table types it '%s'",
               b->key, b->kind, row->kind);
      *err = msg;
      return 0;
    }
    nchecks++;
    /* A LIST row has to name the catalog list its combo is filled from, because the count
       the table states goes unchecked otherwise. */
    if (isList && !wpNamed(winprofile_combos, WP_NCOMBO, b->key)
        && strcmp(b->key, WINPROFILE_COMBO_FROM_RC) != 0) {
      snprintf(msg, sizeof(msg), "the list key '%s' names no catalog list", b->key);
      *err = msg;
      return 0;
    }
    nchecks++;
    /* A key in both lists would be counted twice and still sweep clean. */
    if (wpNamed(winprofile_byhand, WP_NHAND, b->key)) {
      snprintf(msg, sizeof(msg), "'%s' is both bound and hand-written", b->key);
      *err = msg;
      return 0;
    }
    nchecks++;
    if (b->text != NULL)
      mine = b->text;
    else
      mine.Format("%d", b->num);
    if (strcmp(row->default_state, "agreed") == 0) {
      /* A default that disagrees reopens the project on a setting nobody chose. */
      if (mine != row->default_value) {
        snprintf(msg, sizeof(msg), "an absent '%s' reads as '%s' here, '%s' in the table",
                 b->key, (LPCSTR) mine, row->default_value);
        *err = msg;
        return 0;
      }
    } else if (strcmp(row->default_state, "none") == 0) {
      /* Nothing to substitute, because a filled-in value cannot be told from a typed
         one. */
      if (b->text == NULL || b->text[0] != '\0') {
        snprintf(msg, sizeof(msg), "'%s' takes no default, this GUI fills in '%s'",
                 b->key, (LPCSTR) mine);
        *err = msg;
        return 0;
      }
    } else {
      /* A run-time default cannot sit in the list, so such a key is written by hand. */
      snprintf(msg, sizeof(msg), "'%s' defaults %s, which the binding list cannot state",
               b->key, row->default_state);
      *err = msg;
      return 0;
    }
    nchecks++;
  }
  /* And the other way round. A combo list has to belong to a key the table types as a
     list and that this GUI saves, bound here or hand-written the way CurrentAction is.
     Otherwise it holds a catalog count against a key nothing reads that way. */
  for(int k=0 ; k<WP_NCOMBO ; k++) {
    const char *const key = winprofile_combos[k];
    const winprofile_bound_t *const b = wpBound(key);
    int base = 0, count = 0;

    if (!winprofileListRange(key, &base, &count)
        || (b != NULL ? strcmp(b->kind, "list") != 0
                      : !wpNamed(winprofile_byhand, WP_NHAND, key))) {
      snprintf(msg, sizeof(msg), "the combo list names '%s', which no list key here uses",
               key);
      *err = msg;
      return 0;
    }
    nchecks++;
  }
  /* The other direction, counting the skips, because a sweep over a list that quietly
     dropped what it could not bind proves nothing. That is how ProxyType survived one. */
  for(int i=0 ; i<WINPROFILE_KEY_COUNT ; i++) {
    const winprofile_key_t *const row = &winprofile_keys[i];

    if (!winprofileOwnedBy(row->owners, "win")   /* another front end's key */
        || row->legacy_of[0] != '\0'             /* an old spelling of one of ours */
        || strcmp(row->scope, "read_only") == 0) {
      (*nskipped)++;
      continue;
    }
    if (wpBound(row->key) == NULL && !wpNamed(winprofile_byhand, WP_NHAND, row->key)) {
      snprintf(msg, sizeof(msg), "the table gives '%s' to this GUI, which saves it nowhere",
               row->key);
      *err = msg;
      return 0;
    }
    nchecks++;
  }
  for(int k=0 ; k<(int) (sizeof(winprofile_fallbacks)/sizeof(winprofile_fallbacks[0])) ; k++) {
    const winprofile_fallback_t *const f = &winprofile_fallbacks[k];
    const int got = winprofileListValue(f->key, f->value, f->dflt);

    if (got != f->want) {
      snprintf(msg, sizeof(msg), "%s=%d read as %d, expected %d",
               f->key, f->value, got, f->want);
      *err = msg;
      return 0;
    }
    nchecks++;
  }
  if (WP_NBOUND != WINPROFILE_BOUND_KEYS || WP_NHAND != WINPROFILE_HANDWRITTEN_KEYS
      || nchecks != WINPROFILE_BINDING_CHECKS || *nskipped != WINPROFILE_SKIPPED_ROWS) {
    snprintf(msg, sizeof(msg), "ran %d checks over %d bound and %d hand-written keys, "
             "skipping %d table rows; expected %d, %d, %d and %d",
             nchecks, WP_NBOUND, WP_NHAND, *nskipped, WINPROFILE_BINDING_CHECKS,
             WINPROFILE_BOUND_KEYS, WINPROFILE_HANDWRITTEN_KEYS, WINPROFILE_SKIPPED_ROWS);
    *err = msg;
    return 0;
  }
  return nchecks;
}
