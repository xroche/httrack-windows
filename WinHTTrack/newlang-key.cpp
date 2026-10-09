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
/*       catalog key shape test                                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Split out of newlang.cpp so tools/newlang-key-test.cpp can replay the real key
   resolution over the engine's catalogs. Nothing here needs MFC. */

#include <string.h>

#include "newlang-key.h"

int LangKeyIsSymbol(const char *key) {
  /* The underscore keeps the five LANGUAGE_* metadata keys out. Loosen this to a bare
     LANG prefix and LANGUAGE_NAME turns eligible, so a lookup stores a catalog filename
     where a translated string belongs. */
  static const char *const prefixes[] = { "LANG_", "LISTDEF_" };
  unsigned int i;

  if (key == NULL)
    return 0;
  for(i = 0 ; i < sizeof(prefixes) / sizeof(prefixes[0]) ; i++) {
    const size_t len = strlen(prefixes[i]);

    if (strncmp(key, prefixes[i], len) == 0 && key[len] != '\0') {
      const char *p;

      /* Every remaining byte has to be an identifier byte, so no English sentence
         starting with those letters can pass. cpp_lang.h mixes case (LANG_F11b). */
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
