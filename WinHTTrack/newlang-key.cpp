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
/*       catalog keys and the catalog loader                    */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Split out of newlang.cpp so tools/newlang-key-test.cpp runs the real key resolution over
   the engine's catalogs. Nothing here needs MFC. */

#include <stdio.h>
#include <string.h>

#include "newlang-key.h"

int LangKeyIsSymbol(const char *key) {
  static const char *const prefixes[] = { "LANG_", "LISTDEF_" };
  unsigned int i;

  if (key == NULL)
    return 0;
  for(i = 0 ; i < sizeof(prefixes) / sizeof(prefixes[0]) ; i++) {
    const size_t len = strlen(prefixes[i]);

    if (strncmp(key, prefixes[i], len) == 0 && key[len] != '\0') {
      const char *p;

      /* Every remaining byte has to be an identifier byte, so no English sentence
         starting with those letters can pass. cpp_lang.h mixes case (LANG_F11b).
         The engine's lang_is_symbol() tests the prefix alone, and a key only one of
         the two accepts resolves to a name no call site looks up either way. */
      for(p = key + len ; *p != '\0' ; p++) {
        if (!(*p == '_'
              || (*p >= '0' && *p <= '9')
              || (*p >= 'A' && *p <= 'Z')
              || (*p >= 'a' && *p <= 'z')))
          return 0;
      }
      return 1;
    }
  }
  return 0;
}

/* The engine's strnotempty(), whose header this unit cannot include. */
static int notempty(const char *s) {
  return (s != NULL && s[0] != '\0') ? 1 : 0;
}

/* One pass over a catalog, taking either its symbol-keyed lines or its text-keyed ones.
   fallback is the English pass, which fills only what the selected language left unset. */
static void LangLoadCatalogPass(const LangCatalogIO *io, FILE *fp, int fallback,
                                int symbols) {
  char extkey[8192];
  char value[8192];

  rewind(fp);
  while(!feof(fp)) {
    const char *intkey;
    int is_symbol;

    io->readLine(fp, extkey, 8000);
    io->readLine(fp, value, 8000);
    if (!notempty(extkey) || !notempty(value))
      continue;
    intkey = io->internalKey(io->ctx, extkey);

    /* A row may key on its LANG_ symbol instead of the English text, so a reworded
       string keeps its translations. Reached only where lang.def has no row. */
    is_symbol = (!notempty(intkey) && LangKeyIsSymbol(extkey)) ? 1 : 0;

    if (is_symbol != symbols)
      continue;
    if (symbols) {
      /* The first symbol line wins, and no symbol line ever increments. */
      if (notempty(io->resolved(io->ctx, extkey)))
        continue;
      intkey = extkey;
    } else {
      if (!notempty(intkey))
        continue;
      if (notempty(io->resolved(io->ctx, intkey))) {
        size_t pos = strlen(extkey);
        int increment = 0;

        if (fallback)
          continue;
        /* Repeated English text: lang.def keys its Nth copy as text + N. */
        do {
          increment++;
          sprintf(extkey + pos, "%d", increment);
          intkey = io->internalKey(io->ctx, extkey);
        }  while(notempty(intkey) && notempty(io->resolved(io->ctx, intkey)));
        if (!notempty(intkey))
          continue;
      }
    }
    io->store(io->ctx, intkey, value);
  }
}

void LangLoadCatalogs(const LangCatalogIO *io) {
  int index;

  for(index = 0 ; index < 2 ; index++) {
    FILE *const fp = io->open(io->ctx, index);

    if (fp == NULL)
      return;
    /* Symbol lines first, so a repeated text skips the slots they own. Inside this loop
       and never around it: English's symbols must not preempt the selected language's
       text lines. */
    LangLoadCatalogPass(io, fp, index > 0, 1);
    LangLoadCatalogPass(io, fp, index > 0, 0);
    fclose(fp);
  }
}
