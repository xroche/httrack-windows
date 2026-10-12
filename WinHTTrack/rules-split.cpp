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
  /* both copied in by tools/test-rules-split.py, out of the engine's own source */
  int hts_rule_would_be_read_as_opt(const char *arg);
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

/* The dialog's own ' ' and ',', plus every byte the engine's hts_scan_token() ends a
   token on, so one keyword can never reach the engine as two argv tokens. Checked
   against the engine's HTS_REALSPACES by tools/test-rules-split.py. */
#define FILTER_KEYWORD_SEPARATORS " ,\t\n\v\f\r"

/* Copies the next keyword of *FIELD into KEYWORD and advances *FIELD past it, which it
   does even on FALSE. Returns FALSE, KEYWORD left alone, once the field holds no more,
   and for a keyword too long for KEYWORD, so the rest of that field is not read. */
static BOOL nextFilterKeyword(const char **field, char *keyword, size_t keywordsize) {
  const char *const start = *field + strspn(*field, FILTER_KEYWORD_SEPARATORS);
  const size_t len = strcspn(start, FILTER_KEYWORD_SEPARATORS);

  *field = start[len] != '\0' ? start + len + 1 : start + len;
  if (len == 0 || len >= keywordsize)
    return FALSE;
  memcpy(keyword, start, len);
  keyword[len] = '\0';
  return TRUE;
}

/* Builds into OUT the rule the dialog's template TYPE makes of KEYWORD, with no leading
   sign. Returns FALSE with OUT empty when TYPE cannot use KEYWORD, which only the exact
   link (9) does, and when the rule does not fit OUT. */
static BOOL filterRuleForKeyword(int type, const char *keyword, char *out, size_t outsize) {
  int n;

  out[0] = '\0';
  switch (type) {
  case 0:                      /* extension */
    n = snprintf(out, outsize, "*.%s", keyword);
    break;
  case 1:                      /* name holds */
    n = snprintf(out, outsize, "*/*%s*", keyword);
    break;
  case 2:                      /* this file */
    n = snprintf(out, outsize, "*/%s", keyword);
    break;
  case 3:                      /* folder name holds */
    n = snprintf(out, outsize, "*/*%s*/*", keyword);
    break;
  case 4:                      /* this folder */
    n = snprintf(out, outsize, "*/%s/*", keyword);
    break;
  case 5:                      /* domain */
    n = snprintf(out, outsize, "*[name].%s/*", keyword);
    break;
  case 6:                      /* domain name holds */
    n = snprintf(out, outsize, "*[name].*[name]%s*[name].*[name]/*", keyword);
    break;
  case 7:                      /* host */
    n = snprintf(out, outsize, "%s/*", keyword);
    break;
  case 8:                      /* link holds */
    n = snprintf(out, outsize, "*%s*", keyword);
    break;
  case 9:
    /* The engine's own answer, asked about the token a forbidding rule would carry.
       Such a keyword turns on options instead of filtering, and a pattern anchored at
       both ends never matches it anyway. */
    {
      char forbidding[1 + 1024 + 1];   /* the sign, the field's own cap, the NUL */

      /* The dialog caps its field at 1024, so this refuses nothing it can produce. It
         is here because asking the engine about a truncated token would answer about
         a different rule, and no test can tell that apart from this refusal. */
      if ((size_t) snprintf(forbidding, sizeof(forbidding), "-%s", keyword)
          >= sizeof(forbidding))
        return FALSE;
      if (hts_rule_would_be_read_as_opt(forbidding))
        return FALSE;
    }
    n = snprintf(out, outsize, "%s", keyword);
    break;
  default:                     /* every link, and no selection: no keyword used */
    return FALSE;
  }
  if (n < 0 || (size_t) n >= outsize) {
    out[0] = '\0';             /* a rule that does not fit is refused, not truncated */
    return FALSE;
  }
  return TRUE;
}

// see Shell.h
BOOL filterRulesForField(int type, BOOL accept, const char *field,
                         char *out, size_t outsize) {
  const char *cursor = field;
  char keyword[2048], rule[2100];      /* as the dialog's own buffers */
  BOOL every = TRUE;

  out[0] = '\0';
  if (type == 10) {            /* every link: the keyword field is not read */
    snprintf(out, outsize, "%s*", accept ? "+" : "-");
    return TRUE;
  }
  while (nextFilterKeyword(&cursor, keyword, sizeof(keyword))) {
    const size_t used = strlen(out);

    if (!filterRuleForKeyword(type, keyword, rule, sizeof(rule))) {
      every = FALSE;
      continue;
    }
    /* room for the sign, the CRLF and the NUL, then the rule itself */
    if (used + 4 >= outsize || strlen(rule) >= outsize - used - 4)
      break;
    snprintf(out + used, outsize - used, "%s%s\x0d\x0a", accept ? "+" : "-", rule);
  }
  return every;
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
  /* nextFilterKeyword(): the keywords the Add Filter dialog reads out of its field. */
  {
    static const struct { const char* field; const char* want; } fields[] = {
      { "gif", "gif" },
      { "gif jpg", "gif|jpg" },
      { "gif,jpg", "gif|jpg" },
      { "  gif ,, jpg  ", "gif|jpg" },
      { " , ", "" },
      { "", "" },
      { NULL, NULL }
    };

    for(int k=0 ; fields[k].field != NULL ; k++) {
      char keyword[2048];
      const char *cursor = fields[k].field;
      CString joined;

      while(nextFilterKeyword(&cursor, keyword, sizeof(keyword))) {
        if (!joined.IsEmpty())
          joined += "|";
        joined += keyword;
      }
      if (joined != fields[k].want) {
        snprintf(msg, sizeof(msg), "keyword field '%s' read as '%s', expected '%s'",
                 fields[k].field, (LPCSTR) joined, fields[k].want);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }
  /* hts_rule_would_be_read_as_opt(): the engine's own answer, which the exact-link
     refusal rests on. The engine tests it too (its test 584), and it is asserted here
     as well because the dialog's refusal and its "address and path" help text mean
     something different the day that answer changes. */
  {
    static const struct { const char* pattern; int reads; } patterns[] = {
      { "localhost", 1 },
      { "cgi-bin", 1 },
      { "blue", 1 },
      { "www.test.com", 0 },
      { "*.gif", 0 },
      { "a/b", 0 },
      /* a '%' makes the engine read the '.' as an option's parameter, so the dot
         stops saving the rule */
      { "www.test.com%2Ftest.html", 1 },
      { "a.b%20c", 1 },
      { "a.b%20c/", 0 },
      { "", 1 },
      { NULL, 0 }
    };

    for(int k=0 ; patterns[k].pattern != NULL ; k++) {
      char forbidding[64];
      BOOL got;

      snprintf(forbidding, sizeof(forbidding), "-%s", patterns[k].pattern);
      got = hts_rule_would_be_read_as_opt(forbidding) ? TRUE : FALSE;

      if (got != (patterns[k].reads != 0)) {
        snprintf(msg, sizeof(msg), "'-%s' reads as %s, expected %s", patterns[k].pattern,
                 got ? "options" : "a rule", patterns[k].reads ? "options" : "a rule");
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }
  /* Every byte the engine ends a token on separates keywords here too, or one keyword
     would reach the engine as two argv tokens, the second one unsigned. */
  {
    static const char seps[] = " ,\t\n\v\f\r";

    for(size_t k = 0 ; k < sizeof(seps) - 1 ; k++) {
      char field[16], keyword[16];
      const char *cursor = field;

      snprintf(field, sizeof(field), "gif%cjpg", seps[k]);
      if (!nextFilterKeyword(&cursor, keyword, sizeof(keyword))
          || strcmp(keyword, "gif") != 0
          || !nextFilterKeyword(&cursor, keyword, sizeof(keyword))
          || strcmp(keyword, "jpg") != 0) {
        snprintf(msg, sizeof(msg), "byte 0x%02x does not separate two keywords",
                 (unsigned char) seps[k]);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }
  /* A keyword too long for the buffer ends the walk, so the field's tail is dropped
     rather than half a keyword taken. The 4-byte keyword is the boundary: it needs the
     buffer's last byte for its NUL. */
  {
    static const char* const fields[] = { "aaaaa bb", "aaaa bb", NULL };

    for(int k = 0 ; fields[k] != NULL ; k++) {
      char keyword[4];
      const char *cursor = fields[k];

      if (nextFilterKeyword(&cursor, keyword, sizeof(keyword))) {
        snprintf(msg, sizeof(msg), "'%s' gave a keyword in %d bytes", fields[k],
                 (int) sizeof(keyword));
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }
  /* filterRuleForKeyword(): the rule each Add Filter template builds. The dialog's own
     example text is the contract, so each keyword here is the one the dialog shows. */
  {
    static const struct { int type; const char* keyword; const char* want; } templates[] = {
      { 0, "gif", "*.gif" },
      { 1, "blue", "*/*blue*" },
      { 2, "bigfile.mov", "*/bigfile.mov" },
      { 3, "cgi", "*/*cgi*/*" },
      { 4, "cgi-bin", "*/cgi-bin/*" },
      { 5, "myweb.com", "*[name].myweb.com/*" },
      { 6, "myweb", "*[name].*[name]myweb*[name].*[name]/*" },
      { 7, "www.myweb.com", "www.myweb.com/*" },
      { 8, "myweb", "*myweb*" },
      { 9, "www.test.com/test/myweb.html", "www.test.com/test/myweb.html" },
      /* the exact link passes the keyword through, so it is the one that can emit
         a rule the engine reads as options */
      { 9, "localhost", NULL },
      { 9, "", NULL },
      /* every link takes no keyword, and neither does an empty selection */
      { 10, "gif", NULL },
      { -1, "gif", NULL },                   /* -1 is the combo box's CB_ERR */
      { 0, NULL, NULL }
    };

    for(int k=0 ; templates[k].keyword != NULL ; k++) {
      char rule[256];
      const BOOL got = filterRuleForKeyword(templates[k].type, templates[k].keyword,
                                            rule, sizeof(rule));

      /* OUT comes back empty on a refusal, which is what lets a caller skip the
         keyword without reading a half-built rule */
      if (got != (templates[k].want != NULL)
          || strcmp(rule, got ? templates[k].want : "") != 0) {
        snprintf(msg, sizeof(msg), "filter type %d on '%s' built %s, expected %s",
                 templates[k].type, templates[k].keyword,
                 got ? rule : "nothing",
                 templates[k].want != NULL ? templates[k].want : "nothing");
        *err = msg;
        return 0;
      }
      nchecks++;
    }
    /* No template may build a rule the engine reads as options. The condition is
       restated here rather than asked of the engine, so a change on either side of
       that contract cannot hide behind the other. */
    static const char* const risky[] = { "blue", "cgi-bin", "localhost",
                                         "a.b%20c", "x%2Fy", NULL };

    for(int type = 0 ; type <= 10 ; type++) {
      for(int k = 0 ; risky[k] != NULL ; k++) {
        char rule[256];

        if (filterRuleForKeyword(type, risky[k], rule, sizeof(rule))
            && !(strchr(rule, '/') != NULL || strchr(rule, '*') != NULL
                 || (strchr(rule, '.') != NULL && strchr(rule, '%') == NULL))) {
          snprintf(msg, sizeof(msg), "filter type %d on '%s' built '%s', which the engine "
                   "reads as options", type, risky[k], rule);
          *err = msg;
          return 0;
        }
        nchecks++;
      }
    }
    /* A rule too long for the buffer comes back refused, never truncated. */
    {
      char keyword[300], rule[256];

      memset(keyword, 'x', sizeof(keyword) - 1);
      keyword[sizeof(keyword) - 1] = '\0';
      if (filterRuleForKeyword(0, keyword, rule, sizeof(rule)) || rule[0] != '\0') {
        snprintf(msg, sizeof(msg), "a %d-byte keyword built '%.80s' in %d bytes",
                 (int) strlen(keyword), rule, (int) sizeof(rule));
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }
  /* filterRulesForField(): the lines the Add Filter dialog adds to the box, which is
     also the verdict OnOK() refuses on. */
  {
    static const struct { int type; int accept; const char* field; const char* want; int every; } fields[] = {
      { 0, 0, "gif", "-*.gif\x0d\x0a", 1 },
      { 0, 1, "gif", "+*.gif\x0d\x0a", 1 },
      { 0, 0, "gif jpg", "-*.gif\x0d\x0a-*.jpg\x0d\x0a", 1 },
      /* every link takes no keyword, so the field is not read */
      { 10, 0, "", "-*", 1 },
      { 10, 1, "gif", "+*", 1 },
      /* the exact link refuses a keyword the engine would read as options, and the
         refusal is the whole field's, so the dialog can keep its box open on it */
      { 9, 0, "localhost", "", 0 },
      { 9, 0, "www.test.com/a.html", "-www.test.com/a.html\x0d\x0a", 1 },
      /* a bad keyword is refused wherever it sits, and the good ones still build */
      { 9, 0, "www.test.com/a.html localhost", "-www.test.com/a.html\x0d\x0a", 0 },
      { 9, 0, "localhost www.test.com/a.html", "-www.test.com/a.html\x0d\x0a", 0 },
      /* a whitespace byte inside a keyword would reach the engine as a second,
         unsigned argv token, so it separates keywords here */
      { 0, 0, "gif\tjpg", "-*.gif\x0d\x0a-*.jpg\x0d\x0a", 1 },
      { 0, 0, "", "", 1 },
      { -1, 0, "gif", "", 0 },
      { 0, 0, NULL, NULL, 0 }
    };

    for(int k=0 ; fields[k].field != NULL ; k++) {
      char out[1024];              /* FILTER_RULES_MAXSIZE, which lives in Shell.h */
      const BOOL every = filterRulesForField(fields[k].type, fields[k].accept ? TRUE : FALSE,
                                             fields[k].field, out, sizeof(out));

      if (strcmp(out, fields[k].want) != 0 || every != (fields[k].every != 0)) {
        snprintf(msg, sizeof(msg), "type %d on '%.80s' gave '%.200s' (%s), expected "
                 "'%.200s' (%s)", fields[k].type, fields[k].field, out,
                 every ? "all" : "not all", fields[k].want,
                 fields[k].every ? "all" : "not all");
        *err = msg;
        return 0;
      }
      nchecks++;
    }
    /* More rules than the box has room for: it stops on a whole rule and still reports
       every keyword usable, because the dialog must not refuse the field over room. */
    {
      char room[32];
      const BOOL every = filterRulesForField(0, FALSE,
                                             "aaaaaaaaaa bbbbbbbbbb cccccccccc",
                                             room, sizeof(room));

      if (every != TRUE
          || strcmp(room, "-*.aaaaaaaaaa\x0d\x0a-*.bbbbbbbbbb\x0d\x0a") != 0) {
        snprintf(msg, sizeof(msg), "three rules in %d bytes gave '%.200s' (%s)",
                 (int) sizeof(room), room, every ? "all" : "not all");
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
