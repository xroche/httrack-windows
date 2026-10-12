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

#ifndef HTS_DEFNEWLANGKEY
#define HTS_DEFNEWLANGKEY

#include <stdio.h>

/* Does this catalog key name a LANG_ or LISTDEF_ symbol, rather than the English text
   lang.def joins on? Only a key lang.def does not list is ever asked. */
int LangKeyIsSymbol(const char *key);

/* Where a catalog load reads its lines and how it reaches the two tables. The GUI binds
   these to coucal and the shipped reader, tools/newlang-key-test.cpp to its own map. */
typedef struct LangCatalogIO {
  void *ctx;
  /* Opens catalog index 0, the selected language, or 1, the English fallback. A binding
     returning NULL has already reported the failure. */
  FILE *(*open)(void *ctx, int index);
  /* linput_cpp(), which decides where a key ends. */
  int (*readLine)(FILE *fp, char *line, int max);
  /* The internal key lang.def gives this catalog key, or "" where it lists none. */
  const char *(*internalKey)(void *ctx, const char *key);
  /* What this internal key holds so far, or "" while its slot is unset. */
  const char *(*resolved)(void *ctx, const char *intkey);
  void (*store)(void *ctx, const char *intkey, const char *value);
} LangCatalogIO;

/* Resolves every string of the selected language, then English for what it left unset. */
void LangLoadCatalogs(const LangCatalogIO *io);

#endif
