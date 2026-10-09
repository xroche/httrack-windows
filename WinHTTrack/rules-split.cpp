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
/*       scan-rule splitting and the preset checkboxes          */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Split out of Shell.cpp so tools/rules-split-test.cpp can compile the real splitter off
   Windows. Nothing here needs MFC beyond CString and its two arrays, and hts_scan_token()
   is the only engine call. */

#ifdef RULES_SPLIT_TEST
#include "mfc-test-stubs.h"          /* CString and the two arrays for the harness */
extern "C" {
  int hts_scan_token(char **ptr, char *dest, size_t destsize);
};
#else
#include "stdafx.h"
#include "Shell.h"
extern "C" {
  #include "HTTrackInterface.h"
};
#endif

// Returns the token at *PTR and advances *PTR past it and the whitespace after it.
// SCRATCH holds the whole string being split, so hts_scan_token() never truncates a
// token and one buffer serves them all.
static CString scanRuleToken(char **ptr, CString &scratch) {
  char *const dest = scratch.GetBuffer();

  (void) hts_scan_token(ptr, dest, (size_t) scratch.GetLength() + 1);
  return CString(dest);
}

// Split STR with hts_scan_token(), so a rule ends where the engine itself ends it.
void splitTokensInArray(CSimpleArray<CString> &args, const CString &str) {
  CString source(str), scratch;              // hts_scan_token() walks a pointer into source
  char *cur = source.GetBuffer();

  scratch.GetBufferSetLength(str.GetLength());
  while (*cur != '\0') {
    const CString token = scanRuleToken(&cur, scratch);

    if (!token.IsEmpty())                    // leading whitespace yields an empty token
      args.Add(token);
  }
}

// see Shell.h
void splitLinesInArray(CSimpleArray<CString> &args,
                       const CString &str,
                       const CString &separator) {
  const int size = str.GetLength();
  int i, last;
  for(i = 0, last = 0 ; i <= size; i++) {
    if (i == size || str[i] == '\n') {
      if (last != i) {
        CString sub = str.Mid(last, i - last);
        sub.Trim(_T(" \t\r\n"));
        if (sub.GetLength() != 0) {
          if (separator.GetLength() !=0) {
            args.Add(separator);
          }
          args.Add(sub);
        }
      }
      last = i + 1;
    }
  }
}

// Split a rule field the way WebHTTrack does; both read the same winprofile.ini.
// Whitespace splits rules, except beside a ',' or '=': "a , b = c" is one "a,b=c".
void splitRulesInArray(CStringArray &rules, const CString &str) {
  CString source(str), scratch;
  char *cur = source.GetBuffer();

  scratch.GetBufferSetLength(str.GetLength());
  while (*cur != '\0') {
    CString rule;
    BOOL more = TRUE;

    while (more) {
      const CString token = scanRuleToken(&cur, scratch);

      rule += token;
      // nothing follows the last token, so nothing glues to it and the loop ends
      more = *cur != '\0' &&
        (*cur == ',' || *cur == '=' ||
         (!token.IsEmpty() && (token[token.GetLength() - 1] == ',' ||
                               token[token.GetLength() - 1] == '=')));
    }
    if (!rule.IsEmpty())                     // a field of whitespace carries no rule
      rules.Add(rule);
  }
}

// see Shell.h
BOOL ruleListHoldsRule(const CString &list, const CString &rule) {
  CSimpleArray<CString> rules;

  splitTokensInArray(rules, list);
  for(int i = 0 ; i < rules.GetSize() ; i++) {
    if (rules[i] == rule)
      return TRUE;
  }
  return FALSE;
}

// Returns LINE with PRESET's rules gone, spacing kept around what stays.
static CString keepRulesInLine(const CString &line, const CString &preset) {
  CString source(line), scratch;
  char *const base = source.GetBuffer();
  char *cur = base;
  CString kept, gap;
  BOOL first = TRUE;

  scratch.GetBufferSetLength(line.GetLength());
  while (*cur != '\0') {
    const int start = (int) (cur - base);
    const CString rule = scanRuleToken(&cur, scratch);
    const int end = start + rule.GetLength();

    if (!rule.IsEmpty()) {
      if (!ruleListHoldsRule(preset, rule)) {
        // the spacing the user typed, the line's indent only before the first rule
        if (first || !kept.IsEmpty())
          kept += gap;
        kept += rule;
      }
      first = FALSE;
    }
    gap = line.Mid(end, (int) (cur - base) - end);   // this is the next rule's gap
  }
  return kept;
}

// see Shell.h
extern const char rulePresetImages[] = "+*.gif +*.jpg +*.jpeg +*.png +*.tif +*.bmp";
extern const char rulePresetArchives[] = "+*.zip +*.tar +*.tgz +*.gz +*.rar +*.z +*.exe";
extern const char rulePresetMovies[] = "+*.mov +*.mpg +*.mpeg +*.avi +*.asf +*.mp3 +*.mp2 "
  "+*.rm +*.wav +*.vob +*.qt +*.vid +*.ac3 +*.wma +*.wmv";

// see Shell.h
BOOL ruleListHoldsPreset(const CString &box, const CString &preset) {
  CSimpleArray<CString> rules;

  splitTokensInArray(rules, preset);
  if (rules.GetSize() == 0)
    return FALSE;
  for(int i = 0 ; i < rules.GetSize() ; i++) {
    if (!ruleListHoldsRule(box, rules[i]))
      return FALSE;
  }
  return TRUE;
}

// see Shell.h
CString applyRulePreset(const CString &box, const CString &preset, BOOL checked) {
  const int size = box.GetLength();
  CString out;
  int pos = 0;

  while (pos < size) {
    int eol = pos;

    while (eol < size && box[eol] != '\n')
      eol++;
    const CString line = box.Mid(pos, eol - pos);
    const CString kept = keepRulesInLine(line, preset);
    if (!kept.IsEmpty()) {
      if (!out.IsEmpty())
        out += "\r\n";
      out += kept;
    }
    pos = eol + 1;
  }
  if (checked) {
    if (!out.IsEmpty())
      out += "\r\n";
    out += preset;
  }
  return out;
}

/* These paths are only reachable by typing into the Experts page or clicking a preset
   checkbox, and a rule the engine cannot parse aborts the whole mirror. Both --selftest
   and tools/rules-split-test.cpp call this, and both pin the count. */
int rulesSplitCheckCases(CString *err) {
  char msg[1024];
  int nchecks = 0;

  /* splitRulesInArray(): what a repeatable option carries, one flag per rule. */
  {
    static const struct { const char* field; const char* want; } rules[] = {
      { "a=b\r\nc=d", "a=b|c=d" },
      { "a=b  c=d", "a=b|c=d" },
      { "a=b\tc=d", "a=b|c=d" },
      { "a=b\vc=d", "a=b|c=d" },
      { "a=b\fc=d", "a=b|c=d" },
      { "a=b \r\n\t c=d", "a=b|c=d" },
      { "  \r\n a=b \r\n  ", "a=b" },
      { "a.com  ,  b.com  =  c.com", "a.com,b.com=c.com" },
      /* the separator ending a line glues it to the next, wherever the rule sits */
      { "a.com,\r\nb.com=c.com", "a.com,b.com=c.com" },
      { "x=y\r\na.com , b.com = c.com", "x=y|a.com,b.com=c.com" },
      /* an empty field must not emit --host-alias "", which the engine refuses */
      { " \r\n\t\v\f ", "" },
      { "", "" },
      { NULL, NULL }
    };

    for(int k=0 ; rules[k].field != NULL ; k++) {
      CStringArray got;
      CString joined;

      splitRulesInArray(got, rules[k].field);
      for(INT_PTR j=0 ; j<got.GetSize() ; j++) {
        if (j != 0)
          joined += "|";
        joined += got[j];
      }
      if (joined != rules[k].want) {
        snprintf(msg, sizeof(msg), "rule field '%s' split into '%s', expected '%s'",
                 rules[k].field, (LPCSTR) joined, rules[k].want);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
    /* The splitter gives the engine room for the whole field, so a rule longer than
       any engine cap comes back whole instead of truncated. */
    {
      const CString longRule('x', 2000);
      CStringArray got;

      splitRulesInArray(got, longRule + " " + longRule);
      if (got.GetSize() != 2 || got[0] != longRule || got[1] != longRule) {
        snprintf(msg, sizeof(msg),
                 "two %d-byte rules split into %d rules, the first %d bytes",
                 longRule.GetLength(), (int) got.GetSize(),
                 got.GetSize() != 0 ? got[0].GetLength() : 0);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }
  /* applyRulePreset(): what a preset checkbox leaves in the box. */
  {
    static const char preset[] = "+*.gif +*.jpg";
    static const struct { const char* box; int checked; const char* want; } boxes[] = {
      { "+*.gif", 0, "" },
      { "+*.gif +*.jpg +*.zip", 0, "+*.zip" },
      /* a rule holding a preset rule is not that rule */
      { "+*.gifx", 0, "+*.gifx" },
      /* matching is case-sensitive, so an upper-case rule is the user's own */
      { "+*.GIF", 0, "+*.GIF" },
      /* a ',' separates nothing in what we send, so this is one rule, not ours to split */
      { "+*.gif,+*.jpg", 0, "+*.gif,+*.jpg" },
      /* the sign is part of the rule */
      { "-*.gif", 0, "-*.gif" },
      { "+*.gif\t+*.zip", 0, "+*.zip" },
      { "+*.gif   +*.zip", 0, "+*.zip" },
      { "+*.zip   +*.gif", 0, "+*.zip" },
      /* an untouched line keeps the spacing the user typed */
      { "+*.zip  +*.htm", 0, "+*.zip  +*.htm" },
      /* a rule removed from the middle keeps the indent and both neighbours */
      { "  +*.zip +*.gif +*.htm", 0, "  +*.zip +*.htm" },
      /* the engine splits on any isspace() byte, so none of these glue two rules */
      { "+*.gif\r+*.zip", 0, "+*.zip" },
      { "+*.gif\v+*.zip", 0, "+*.zip" },
      { "+*.gif\f+*.zip", 0, "+*.zip" },
      { "+*.htm\r\n+*.gif\r\n+*.zip", 0, "+*.htm\r\n+*.zip" },
      { "", 0, "" },
      { " \r\n\t ", 0, "" },
      { "", 1, "+*.gif +*.jpg" },
      { "+*.gif +*.zip", 1, "+*.zip\r\n+*.gif +*.jpg" },
      /* the preset is removed before it is added back, so nothing doubles */
      { "+*.gif +*.jpg", 1, "+*.gif +*.jpg" },
      { "+*.gifx", 1, "+*.gifx\r\n+*.gif +*.jpg" },
      { NULL, 0, NULL }
    };

    for(int k=0 ; boxes[k].box != NULL ; k++) {
      const CString got = applyRulePreset(boxes[k].box, preset, boxes[k].checked);

      if (got != boxes[k].want) {
        snprintf(msg, sizeof(msg), "rules box '%s' with the preset %s gave '%s', expected '%s'",
                 boxes[k].box, boxes[k].checked ? "on" : "off", (LPCSTR) got, boxes[k].want);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }
  /* ruleListHoldsPreset(): only a whole preset checks its box, because the click that
     unchecks it takes out every rule of the preset. */
  {
    static const char preset[] = "+*.gif +*.jpg";
    static const struct { const char* box; int want; } holds[] = {
      { "+*.gif +*.jpg", 1 },
      { "+*.jpg +*.gif", 1 },               /* order is not part of the preset */
      { "+*.gif\r\n+*.jpg", 1 },            /* the control's own line breaks */
      { "  +*.gif\t+*.zip +*.jpg  ", 1 },   /* the user's own rules sit among them */
      { "+*.gif", 0 },                      /* one rule of it is not the preset */
      { "+*.jpg", 0 },
      { "", 0 },
      { "+*.gifx +*.jpgx", 0 },             /* a longer rule is not that rule */
      { "+*.GIF +*.JPG", 0 },               /* matching is case-sensitive */
      { "-*.gif -*.jpg", 0 },               /* the sign is part of the rule */
      { "+*.gif,+*.jpg", 0 },               /* one rule, since ',' separates nothing */
      { NULL, 0 }
    };

    for(int k=0 ; holds[k].box != NULL ; k++) {
      const int got = ruleListHoldsPreset(holds[k].box, preset) ? 1 : 0;

      if (got != holds[k].want) {
        snprintf(msg, sizeof(msg), "box '%s' read the preset as %s, expected %s",
                 holds[k].box, got ? "applied" : "not applied",
                 holds[k].want ? "applied" : "not applied");
        *err = msg;
        return 0;
      }
      nchecks++;
    }
    /* A preset holding no rule is held by nothing. Three boxes lead with whitespace,
       because the empty token the splitter drops would otherwise read as a rule. */
    {
      static const struct { const char* box; const char* preset; } none[] = {
        { "+*.gif", "" },
        { "  +*.gif", "" },
        { "  +*.gif", " " },
        { "  +*.gif", " \r\n\t " },
        { NULL, NULL }
      };

      for(int k=0 ; none[k].box != NULL ; k++) {
        if (ruleListHoldsPreset(none[k].box, none[k].preset)) {
          snprintf(msg, sizeof(msg), "box '%s' read the empty preset '%s' as applied",
                   none[k].box, none[k].preset);
          *err = msg;
          return 0;
        }
        nchecks++;
      }
    }
  }
  return nchecks;
}
