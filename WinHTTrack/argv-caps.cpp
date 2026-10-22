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
/*       the caps on what one option value may carry            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Split out of Shell.cpp so tools/argv-caps-test.cpp can compile the real checks off
   Windows. Nothing here needs MFC beyond CString, and the only engine call is the
   ANSI-to-UTF-8 conversion that decides how many bytes a value really costs. */

#ifdef ARGV_CAPS_TEST
#include "mfc-test-stubs.h"          /* CString, and the engine calls as the harness sees them */
#include "argv-caps-engine-macros.h" /* the caps, copied out of the engine by the harness */
#include "accept-rows.h"
#else
#include "stdafx.h"
#include "Shell.h"
extern "C" {
  #include "HTTrackInterface.h"
};
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

// see Shell.h
BOOL fitsEngineArgument(const CString &value, size_t maxBytes) {
  if (value.IsEmpty())
    return FALSE;
  // the engine measures the UTF-8 bytes strdupt_utf8() will hand it, not these characters
  char *utf8 = hts_convertStringSystemToUTF8(value, value.GetLength());  // freet() nulls it, so not const
  // strdupt_utf8() falls back to these same ANSI bytes when the conversion fails
  const size_t bytes = utf8 != NULL ? strlen(utf8) : (size_t) value.GetLength();
  if (utf8 != NULL)
    freet(utf8);
  return bytes < maxBytes;
}

// see Shell.h
BOOL isEngineArgument(const CString &value, size_t maxBytes) {
  return fitsEngineArgument(value, maxBytes) && value[0] != '-';
}

// Same, except the engine also refuses any argument that reaches HTS_CDLMAXSIZE bytes.
static BOOL isCappedArgument(const CString &value, size_t maxBytes) {
  const size_t ceiling = (size_t) HTS_CDLMAXSIZE;
  const size_t cap = maxBytes < ceiling ? maxBytes : ceiling;

  return isEngineArgument(value, cap);
}

// see Shell.h. A control character is refused for the same reason it is in an Accept
// value: the engine turns CR, LF and TAB into spaces, so the identity would go out
// mangled, and a browser login is what first sets this field from outside.
BOOL isUserAgentArgument(const CString &value) {
  for(int i=0 ; i<value.GetLength() ; i++) {
    if ((unsigned char) value[i] < ' ')
      return FALSE;
  }
  return isCappedArgument(value, HTS_CDLMAXSIZE);   // -F has no cap of its own
}

BOOL isFooterArgument(const CString &value) {
  return isCappedArgument(value, HTS_FOOTER_MAXSIZE);
}

BOOL isLangIsoArgument(const CString &value) {
  return isCappedArgument(value, HTS_LANGISO_MAXSIZE);
}

BOOL isRefererArgument(const CString &value) {
  return isCappedArgument(value, HTS_REFERER_MAXSIZE);
}

// A kind row glues "kind=" in front of its value, and the two ride one argv word.
static_assert(HTS_ACCEPT_MAXSIZE + ACCEPT_KIND_PREFIX_MAXSIZE < HTS_CDLMAXSIZE,
              "a kind row's argv word could pass the engine's ceiling");

// see Shell.h. A control character is refused too. The engine turns CR, LF and TAB into
// spaces in every argument (htscoremain.c), so such a value would otherwise go out
// mangled, under a kind the user never meant for the newline case.
BOOL isAcceptArgument(const CString &value) {
  for(int i=0 ; i<value.GetLength() ; i++) {
    if ((unsigned char) value[i] < ' ')
      return FALSE;
  }
  return isEngineArgument(value, HTS_ACCEPT_MAXSIZE);
}

// see Shell.h
BOOL isAcceptDefaultArgument(const CString &value) {
  static const char *const kinds[] = { ACCEPT_ROWS(ACCEPT_ROW_KIND) };
  const int eq = value.Find('=');

  if (!isAcceptArgument(value))
    return FALSE;
  for(int row=0 ; eq > 0 && row<ACCEPT_ROW_COUNT ; row++) {
    if (kinds[row] != NULL && (int) strlen(kinds[row]) == eq
        && strncmp((LPCSTR) value, kinds[row], (size_t) eq) == 0)
      return FALSE;
  }
  return TRUE;
}

// see Shell.h
CString acceptBoxText(const CString &value, const CString &automatic) {
  return value.IsEmpty() ? automatic : value;
}

// see Shell.h
CString acceptBoxValue(const CString &text, const CString &automatic) {
  return text == automatic ? CString() : text;
}

// see Shell.h
CString optionValue(const CString &text, BOOL (*fits)(const CString &)) {
  return fits(text) ? text : CString();
}

static BOOL isAllDigits(const CString &value) {
  for(int i = 0 ; i < value.GetLength() ; i++) {
    if (value[i] < '0' || value[i] > '9')
      return FALSE;
  }
  return !value.IsEmpty();
}

// see Shell.h
BOOL isSingleFileMaxArgument(const CString &value) {
  char *end;
  LLint v;

  if (!isAllDigits(value))   // strtoll would otherwise take a sign or leading spaces
    return FALSE;
  errno = 0;
  v = strtoll((LPCSTR) value, &end, 10);
  // leading zeros keep the value small however long the string is, so argv still caps it
  return *end == '\0' && errno != ERANGE && v > 0 && fitsEngineArgument(value, HTS_CDLMAXSIZE);
}

// see Shell.h
BOOL isMaxRetryAfterArgument(const CString &value) {
  // strtol takes a sign and saturates silently at LONG_MAX, so digits come first
  if (!isAllDigits(value))
    return FALSE;
  return strtol((LPCSTR) value, NULL, 10) <= HTS_MAX_RETRY_AFTER_LIMIT
         && fitsEngineArgument(value, HTS_MAXRETRYAFTER_MAXBYTES);
}

/* A value the engine refuses costs the whole mirror, and these fields are only reachable
   by typing into a page or by loading a profile. Both --selftest and
   tools/argv-caps-test.cpp call this, and both pin the count. */
int argvCapsCheckCases(CString *err, int *nskipped) {
  char msg[1024];
  int nchecks = 0;

  *nskipped = 0;

  /* Each field has its own cap, and optionValue() must hand the value through unwrapped. */
  {
    static const struct { BOOL (*ok)(const CString &); const char* field;
                          const char* value; int repeat; BOOL want; } quoted[] = {
      { isFooterArgument, "footer", HTS_DEFAULT_FOOTER, 1, TRUE },
      { isFooterArgument, "footer", HTS_NOPARAM, 1, TRUE },   /* asks for no footer at all */
      { isFooterArgument, "footer", "", 1, FALSE },
      { isFooterArgument, "footer", "-<!-- x -->", 1, FALSE },  /* reads as the argument being missing */
      { isFooterArgument, "footer", "\"<!-- x -->\"", 1, TRUE },   /* a quote is footer text, so it must pass */
      /* padding is the user's own text, so optionValue() must not trim it away */
      { isFooterArgument, "footer", " <!-- x --> ", 1, TRUE },
      { isFooterArgument, "footer", "x", HTS_FOOTER_MAXSIZE - 1, TRUE },  /* one under the cap fits */
      { isFooterArgument, "footer", "x", HTS_FOOTER_MAXSIZE, FALSE },
      { isLangIsoArgument, "accept-language", "en, fr", 1, TRUE },
      { isLangIsoArgument, "accept-language", "x", HTS_LANGISO_MAXSIZE - 1, TRUE },
      { isLangIsoArgument, "accept-language", "x", HTS_LANGISO_MAXSIZE, FALSE },
      { isRefererArgument, "referer", "x", HTS_REFERER_MAXSIZE - 1, TRUE },
      { isRefererArgument, "referer", "x", HTS_REFERER_MAXSIZE, FALSE },
      /* the user-agent has no cap of its own, only the engine's ceiling on one argument */
      { isUserAgentArgument, "user-agent", "x", HTS_CDLMAXSIZE - 1, TRUE },
      { isUserAgentArgument, "user-agent", "x", HTS_CDLMAXSIZE, FALSE },
      /* what a browser login can hand over, and what the engine would flatten */
      { isUserAgentArgument, "user-agent", "Mozilla/5.0 (Windows NT 10.0)", 1, TRUE },
      { isUserAgentArgument, "user-agent", "Mozilla/5.0\nUser-Agent: other", 1, FALSE },
      { isUserAgentArgument, "user-agent", "Mozilla/5.0\rx", 1, FALSE },
      { isUserAgentArgument, "user-agent", "Mozilla/5.0\tx", 1, FALSE },
      { isAcceptArgument, "accept", "text/html,image/avif,*/*;q=0.8", 1, TRUE },
      { isAcceptArgument, "accept", "*/*", 1, TRUE },
      { isAcceptArgument, "accept", "text/html, */*", 1, TRUE },   /* a space is legal */
      { isAcceptArgument, "accept", "x", HTS_ACCEPT_MAXSIZE - 1, TRUE },
      { isAcceptArgument, "accept", "x", HTS_ACCEPT_MAXSIZE, FALSE },
      /* the engine would turn each of these into a space and send the value mangled */
      { isAcceptArgument, "accept", "*/*\nscript=*/*", 1, FALSE },
      { isAcceptArgument, "accept", "*/*\rscript=*/*", 1, FALSE },
      { isAcceptArgument, "accept", "*/*\tq=0.1", 1, FALSE },
      /* a kind row's value may hold anything, since its own kind word goes in front */
      { isAcceptArgument, "accept", "image=*/*", 1, TRUE },
      /* the default row sends its value alone, so the engine would read these two as the
         image kind: one would apply the default to images, the other would send images no
         Accept header at all */
      { isAcceptDefaultArgument, "accept default", "image=*/*", 1, FALSE },
      { isAcceptDefaultArgument, "accept default", "image=", 1, FALSE },
      { isAcceptDefaultArgument, "accept default", "text/html,*/*;q=0.8", 1, TRUE },
      { isAcceptDefaultArgument, "accept default", "imagex=y", 1, TRUE },   /* no kind word */
      { NULL, NULL, NULL, 0, FALSE }
    };

    for(int k=0 ; quoted[k].field != NULL ; k++) {
      CString value;
      for(int n=0 ; n<quoted[k].repeat ; n++)
        value += quoted[k].value;
      const char *why = NULL;

      if (quoted[k].ok(value) != quoted[k].want)
        why = quoted[k].want ? "refused, expected accepted" : "accepted, expected refused";
      else if (quoted[k].ok(CString()))   /* for every option, not only the footer rows above */
        why = "accepted an empty value";
      else if (quoted[k].ok("-" + value))
        why = "accepted a leading dash";
      else if (optionValue(value, quoted[k].ok) != (quoted[k].want ? value : CString()))
        why = "wrapped or dropped the value";
      if (why != NULL) {
        snprintf(msg, sizeof(msg), "%s argument '%s' (%d chars) %s",
                 quoted[k].field, (LPCSTR) value.Left(40), (int) value.GetLength(), why);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
    /* The caps count the UTF-8 bytes the engine will see: 200 accented characters are 400 of
       them. Under a UTF-8 ANSI codepage the conversion is a copy, and 200 stay 200. */
    if (GetACP() == CP_UTF8) {
      (*nskipped)++;
    } else {
      if (isFooterArgument(CString('\xE9', 200))) {
        *err = "a 400-byte accented footer was judged short enough";
        return 0;
      }
      nchecks++;
    }
  }
  /* The single-file cap rides argv as its own token, so its length costs the mirror as
     surely as its value: the engine refuses an argument of HTS_CDLMAXSIZE bytes before
     parsing it. */
  {
    static const struct { const char *lead; int repeat; const char *tail; BOOL want; } caps[] = {
      { "", 0, "1000", TRUE },
      { "", 0, "+1000", FALSE },   /* strtoll would take the sign, the engine will not */
      { "", 0, "3000000000", TRUE },   /* over 2 GB, so a 32-bit parse cannot pass this */
      { "9", 19, "", FALSE },   /* overflows LLint at a length the rows below still allow */
      { "0", 4, "", FALSE },   /* zero, however it is written */
      /* leading zeros keep the value at 1, so only the length decides these two */
      { "0", HTS_CDLMAXSIZE - 2, "1", TRUE },
      { "0", HTS_CDLMAXSIZE - 1, "1", FALSE },
      { NULL, 0, NULL, FALSE }
    };

    for(int k=0 ; caps[k].lead != NULL ; k++) {
      CString value;

      for(int n=0 ; n<caps[k].repeat ; n++)
        value += caps[k].lead;
      value += caps[k].tail;
      if (isSingleFileMaxArgument(value) != caps[k].want) {
        snprintf(msg, sizeof(msg), "single-file cap '%s' (%d chars) judged %s",
                 (LPCSTR) value.Left(40), (int) value.GetLength(),
                 caps[k].want ? "bad, expected good" : "good, expected bad");
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }
  /* The box shows an empty value as a word, and that word has to come back as an empty
     value under every language, or a profile would carry it into a request. */
  {
    static const struct { const char *automatic; const char *shown; const char *value; } rows[] = {
      { "Automatic", "Automatic", "" },
      { "Automatique", "Automatique", "" },        /* the same row under another language */
      { "Automatic", "*/*", "*/*" },
      { "Automatic", "Automatic ", "Automatic " },   /* the user's own text, near the word */
      { "", "", "" },   /* a catalog without the key: the word is empty, and so is the value */
      { NULL, NULL, NULL }
    };

    for(int k=0 ; rows[k].automatic != NULL ; k++) {
      const CString automatic = rows[k].automatic;
      const char *why = NULL;

      if (acceptBoxValue(rows[k].shown, automatic) != CString(rows[k].value))
        why = "stored the wrong value";
      else if (acceptBoxText(rows[k].value, automatic) != CString(rows[k].shown))
        why = "showed the wrong text";
      if (why != NULL) {
        snprintf(msg, sizeof(msg), "the accept box showing '%s' under '%s' %s",
                 rows[k].shown, rows[k].automatic, why);
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }
  /* The retry-after cap is HTS_MAXRETRYAFTER_MAXBYTES, not HTS_CDLMAXSIZE (see Shell.h). */
  {
    static const struct { const char *lead; int repeat; const char *tail; BOOL want; } delays[] = {
      { "", 0, "0", TRUE },   /* 0 is a value, not an empty box */
      { "", 0, "60", TRUE },
      { "", 0, "3600", TRUE },   /* HTS_MAX_RETRY_AFTER_LIMIT, which the engine accepts */
      { "", 0, "3601", FALSE },
      { "", 0, "", FALSE },   /* no value at all, so no option */
      { "", 0, "+5", FALSE },   /* the engine's %d would take the sign, we will not */
      { "", 0, " 5", FALSE },
      { "", 0, "5s", FALSE },
      { "", 0, "-1", FALSE },
      { "0", 4, "60", TRUE },   /* leading zeros keep it in range however it is written */
      /* zeros and out of range together: reading a prefix of the digits passes every
         other row here, and accepts this one */
      { "0", 4, "3601", FALSE },
      { "9", 12, "", FALSE },   /* far out of range, but short enough that the length never decides it */
      /* value 1 either way, so only the glued argv length decides these two */
      { "0", HTS_MAXRETRYAFTER_MAXBYTES - 2, "1", TRUE },
      { "0", HTS_MAXRETRYAFTER_MAXBYTES - 1, "1", FALSE },
      { NULL, 0, NULL, FALSE }
    };

    for(int k=0 ; delays[k].lead != NULL ; k++) {
      CString value;

      for(int n=0 ; n<delays[k].repeat ; n++)
        value += delays[k].lead;
      value += delays[k].tail;
      if (isMaxRetryAfterArgument(value) != delays[k].want) {
        snprintf(msg, sizeof(msg), "max retry-after '%s' (%d chars) judged %s",
                 (LPCSTR) value.Left(40), (int) value.GetLength(),
                 delays[k].want ? "bad, expected good" : "good, expected bad");
        *err = msg;
        return 0;
      }
      nchecks++;
    }
  }
  return nchecks;
}
