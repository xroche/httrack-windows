/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Xavier Roche and other contributors

   Replays a COPY of the catalog join in WinHTTrack/newlang.cpp, once with the symbol-key
   fallback and once without, and holds the two resolved tables against each other.
   LangKeyIsSymbol() is the only shipped code here, because the line reader below is
   copied out of HTTrackInterface.c and std::map stands in for coucal. So an equal pair
   of tables supports the no-op rather than proving it. What proves it is that no shipped
   key has the symbol shape, checked at 2 below and again over every catalog in
   tools/test-newlang-key.py, which compiles and drives this. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "newlang-key.h"

/* The engine's reader, copied from WinHTTrack/HTTrackInterface.c because including it
   would pull in htsopt.h. It decides where a key ends, so both modes need the real one:
   CR, TAB and FF are dropped, space and TAB are trimmed off both ends, and a trailing
   backslash joins the next physical line. */
static int linput(FILE *fp, char *s, int max) {
  int c, j = 0;

  do {
    c = fgetc(fp);
    if (c != EOF) {
      switch(c) {
      case 13: case 9: case 12:
        break;
      case 10:
        c = -1;
        break;
      default:
        s[j++] = (char) c;
        break;
      }
    }
  } while((c != -1) && (c != EOF) && (j < (max - 1)));
  s[j] = '\0';
  return j;
}

static int linput_trim(FILE *fp, char *s, int max) {
  std::vector<char> ls(max + 2);
  int rlen;

  s[0] = '\0';
  rlen = linput(fp, &ls[0], max);
  if (rlen > 0) {
    char *a;

    while((rlen > 0) && ((ls[rlen - 1] == ' ') || (ls[rlen - 1] == '\t')))
      ls[--rlen] = '\0';
    a = &ls[0];
    while((rlen > 0) && ((*a == ' ') || (*a == '\t'))) {
      a++;
      rlen--;
    }
    if (rlen > 0) {
      memcpy(s, a, rlen);
      s[rlen] = '\0';
    }
  }
  return rlen;
}

static int linput_cpp(FILE *fp, char *s, int max) {
  int rlen = 0;

  s[0] = '\0';
  do {
    int ret;

    if (rlen > 0)
      if (s[rlen - 1] == '\\')
        s[--rlen] = '\0';
    ret = linput_trim(fp, s + rlen, max - rlen);
    if (ret > 0)
      rlen += ret;
  } while((s[rlen > 0 ? rlen - 1 : 0] == '\\') && (rlen < max));
  return rlen;
}

typedef std::map<std::string, std::string> Table;

static const char *lookup(const Table &t, const std::string &key) {
  const Table::const_iterator i = t.find(key);

  return (i == t.end()) ? "" : i->second.c_str();
}

/* lang.def, read as newlang.cpp reads it: the first line of a pair is stored, the second
   is what it is looked up by, and a repeated lookup key takes a 1, 2... suffix. */
static bool loadLangDef(const char *path, Table &keys) {
  FILE *fp = fopen(path, "rb");
  char intkey[8192], key[8192];

  if (fp == NULL)
    return false;
  while(!feof(fp)) {
    linput_cpp(fp, intkey, 8000);
    linput_cpp(fp, key, 8000);
    if (intkey[0] != '\0' && key[0] != '\0') {
      std::string k(key);

      for(int increment = 1 ; keys.find(k) != keys.end() ; increment++) {
        char suffix[32];

        sprintf(suffix, "%d", increment);
        k = std::string(key) + suffix;
      }
      keys[k] = intkey;
    }
  }
  fclose(fp);
  return true;
}

/* One catalog pass, as newlang.cpp's loop body does it. englishPass is its loops>0: a
   symbol already filled by the selected language is left alone. fallback switches the
   one line this change adds, so the two runs differ in nothing else. */
static bool loadCatalog(const char *path, const Table &keys, Table &str,
                        bool englishPass, bool fallback, long *fallbackHits) {
  FILE *fp = fopen(path, "rb");
  char extkey[8192], value[8192];

  if (fp == NULL)
    return false;
  while(!feof(fp)) {
    linput_cpp(fp, extkey, 8000);
    linput_cpp(fp, value, 8000);
    if (extkey[0] != '\0' && value[0] != '\0') {
      std::string intkey(lookup(keys, extkey));

      if (fallback && intkey.empty() && LangKeyIsSymbol(extkey)) {
        intkey = extkey;
        (*fallbackHits)++;
      }
      if (!intkey.empty()) {
        std::string test(lookup(str, intkey));

        if (!test.empty()) {
          if (!englishPass) {
            for(int increment = 1 ; !test.empty() ; increment++) {
              char suffix[32];

              sprintf(suffix, "%d", increment);
              intkey = lookup(keys, std::string(extkey) + suffix);
              test = intkey.empty() ? "" : lookup(str, intkey);
            }
          } else
            intkey = "";
        }
        if (!intkey.empty())
          str[intkey] = value;
      }
    }
  }
  fclose(fp);
  return true;
}

/* The two-pass load of newlang.cpp: the selected catalog, then English for what it left
   unset. ConvertCatalogValue() is left out, because it maps a value to a value that this
   change cannot reach, so equal raw tables stay equal through it. */
static Table resolve(const std::string &engine, const Table &keys,
                     const std::string &language, bool fallback, long *fallbackHits) {
  const char *const names[2] = { language.c_str(), "English" };
  Table str;

  for(int pass = 0 ; pass < 2 ; pass++) {
    const std::string path = engine + "/lang/" + names[pass] + ".txt";

    if (!loadCatalog(path.c_str(), keys, str, pass > 0, fallback, fallbackHits)) {
      fprintf(stderr, "cannot open %s\n", path.c_str());
      exit(1);
    }
  }
  return str;
}

static int failures = 0;

static void expectSymbol(const char *key, int want) {
  const int got = LangKeyIsSymbol(key);

  if (got != want) {
    fprintf(stderr, "LangKeyIsSymbol(\"%s\") is %d, expected %d\n", key, got, want);
    failures++;
  }
}

/* Every language lang.def names, so the comparison covers all 30 catalogs. */
static std::vector<std::string> languages(const Table &keys) {
  std::vector<std::string> out;

  for(int i = 1 ; ; i++) {
    char name[64];

    sprintf(name, "LANGUAGE_%d", i);
    const std::string file(lookup(keys, name));
    if (file.empty())
      break;
    out.push_back(file);
  }
  return out;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <engine checkout>\n", argv[0]);
    return 1;
  }
  const std::string engine(argv[1]);
  Table keys;

  if (!loadLangDef((engine + "/lang.def").c_str(), keys)) {
    fprintf(stderr, "cannot open %s/lang.def\n", engine.c_str());
    return 1;
  }

  /* 1. The shape test itself. A key lang.def does not list reaches it, so the English
     texts below are the ones that must keep being read as English text. */
  expectSymbol("LANG_OK", 1);
  expectSymbol("LANG_F11b", 1);
  expectSymbol("LANG_I1g2", 1);
  expectSymbol("LISTDEF_1", 1);
  expectSymbol("LANGUAGE_NAME", 0);   /* lang.def's own identity rows stay text keys */
  expectSymbol("LANGUAGE_1", 0);
  expectSymbol("LANG_", 0);
  expectSymbol("LISTDEF_", 0);
  expectSymbol("LANG_OK ", 0);        /* linput_cpp trims, so a space means real text */
  expectSymbol("LANG_OK!", 0);
  expectSymbol("LANG_ OK", 0);
  expectSymbol("LANGUAGE pack", 0);
  expectSymbol("OK", 0);
  expectSymbol("No storage media (SDCARD)", 0);
  expectSymbol("View License", 0);
  expectSymbol("", 0);
  expectSymbol(NULL, 0);

  /* 2. No key either file carries has the symbol shape, so the branch is unreachable. */
  long shaped = 0;

  for(Table::const_iterator i = keys.begin() ; i != keys.end() ; ++i) {
    if (LangKeyIsSymbol(i->first.c_str())) {
      fprintf(stderr, "lang.def looks up %s, which has the symbol shape\n",
              i->first.c_str());
      shaped++;
    }
  }

  /* 3. Every catalog resolves byte-identically with the fallback and without it. */
  const std::vector<std::string> langs = languages(keys);
  long compared = 0, hits = 0;

  if (langs.size() < 30) {
    fprintf(stderr, "lang.def names %d languages, expected at least 30\n",
            (int) langs.size());
    return 1;
  }
  for(size_t i = 0 ; i < langs.size() ; i++) {
    long before = 0, after = 0;
    const Table old = resolve(engine, keys, langs[i], false, &before);
    const Table now = resolve(engine, keys, langs[i], true, &after);

    if (before != 0) {
      fprintf(stderr, "%s: the fallback ran with it switched off\n", langs[i].c_str());
      failures++;
    }
    hits += after;
    if (old.size() != now.size()) {
      fprintf(stderr, "%s: %d symbols resolve, %d with the fallback\n",
              langs[i].c_str(), (int) old.size(), (int) now.size());
      failures++;
    }
    for(Table::const_iterator k = old.begin() ; k != old.end() ; ++k) {
      const char *const got = lookup(now, k->first);

      if (k->second != got) {
        fprintf(stderr, "%s: %s was \"%s\", is now \"%s\"\n",
                langs[i].c_str(), k->first.c_str(), k->second.c_str(), got);
        failures++;
      }
      compared++;
    }
  }
  if (hits != 0) {
    fprintf(stderr, "the fallback resolved %ld rows: a shipped catalog is symbol-keyed "
            "and the change is no longer a no-op\n", hits);
    failures++;
  }

  /* 4. The control: the comparison above has to be able to see a difference at all.
     A catalog holding one symbol-keyed row must resolve only with the fallback. */
  const char *probe = getenv("NEWLANG_KEY_PROBE");

  if (probe != NULL) {
    Table off, on;
    long ignored = 0, probed = 0;

    loadCatalog(probe, keys, off, false, false, &ignored);
    loadCatalog(probe, keys, on, false, true, &probed);
    if (probed != 1 || !off.empty() || on.size() != 1
        || lookup(on, "LANG_OK") != std::string("probe value")) {
      fprintf(stderr, "the probe catalog resolved the same either way: the comparison "
              "above cannot see a symbol-keyed row\n");
      failures++;
    }
  } else {
    fprintf(stderr, "NEWLANG_KEY_PROBE unset: the comparison is unproven\n");
    failures++;
  }

  printf("%ld of the %d lang.def keys have the symbol shape and the fallback resolved "
         "%ld rows, so the %ld resolved strings of %d catalogs match byte for byte\n",
         shaped, (int) keys.size(), hits, compared, (int) langs.size());
  return (failures == 0 && shaped == 0) ? 0 : 1;
}
