/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Xavier Roche and other contributors

   Replays a COPY of the catalog join in WinHTTrack/newlang.cpp, once with the symbol-key
   fallback and once without, and holds the two resolved tables against each other. Then
   it re-keys the French and English catalogs the way the engine's
   tests/576_webhttrack-lang-symbol-keys.test does, and requires the same strings out.
   LangKeyIsSymbol() is the only shipped code here, because the line reader below is
   copied out of HTTrackInterface.c and std::map stands in for coucal. So an equal pair
   of tables supports the no-op rather than proving it. What proves it is that no shipped
   key has the symbol shape, checked at 2 below and again over every catalog in
   tools/test-newlang-key.py, which compiles and drives this and holds the loader this
   copies against the rules it follows. */

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

/* Which form of the one added line loadCatalog() applies. GUARDED is what newlang.cpp
   ships. UNGUARDED drops the missed-lookup test, so check 5 can see the guard's
   direction, which no shipped key is both listed and symbol-shaped enough to show. */
enum Fallback { OFF, GUARDED, UNGUARDED };

/* One catalog pass, as LANG_LOAD_CATALOG() in newlang.cpp does it. englishPass is its
   fallback: a slot the selected language filled is left alone. symbols takes the
   symbol-keyed lines, and the two modes below are the only other thing that can move
   the resolved table. */
static bool loadCatalog(const char *path, const Table &keys, Table &str,
                        bool englishPass, Fallback mode, long *fallbackHits,
                        bool symbols) {
  FILE *fp = fopen(path, "rb");
  char extkey[8192], value[8192];

  if (fp == NULL)
    return false;
  while(!feof(fp)) {
    linput_cpp(fp, extkey, 8000);
    linput_cpp(fp, value, 8000);
    if (extkey[0] != '\0' && value[0] != '\0') {
      std::string intkey(lookup(keys, extkey));
      /* The text lookup decides first, which is what keeps the LANGUAGE_* keys text. */
      const bool isSymbol = mode != OFF && LangKeyIsSymbol(extkey)
                            && (mode == UNGUARDED || intkey.empty());

      if (isSymbol != symbols)
        continue;
      if (symbols) {
        if (!lookup(str, extkey)[0])     /* the first symbol line wins */
          str[extkey] = value;
        (*fallbackHits)++;
        continue;
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

/* One catalog, read twice: its symbol lines, then its text lines. Nested inside the
   two-language loop below, never around it. */
static bool loadBoth(const char *path, const Table &keys, Table &str,
                     bool englishPass, Fallback mode, long *fallbackHits) {
  return loadCatalog(path, keys, str, englishPass, mode, fallbackHits, true)
         && loadCatalog(path, keys, str, englishPass, mode, fallbackHits, false);
}

/* The two-pass load of newlang.cpp: the selected catalog, then English for what it left
   unset. ConvertCatalogValue() is left out, because it maps a value to a value that this
   change cannot reach, so equal raw tables stay equal through it. */
static Table resolveFiles(const std::string &selected, const std::string &english,
                          const Table &keys, Fallback mode, long *fallbackHits) {
  const std::string paths[2] = { selected, english };
  Table str;

  for(int pass = 0 ; pass < 2 ; pass++) {
    if (!loadBoth(paths[pass].c_str(), keys, str, pass > 0, mode, fallbackHits)) {
      fprintf(stderr, "cannot open %s\n", paths[pass].c_str());
      exit(1);
    }
  }
  return str;
}

static Table resolve(const std::string &engine, const Table &keys,
                     const std::string &language, Fallback mode, long *fallbackHits) {
  return resolveFiles(engine + "/lang/" + language + ".txt",
                      engine + "/lang/English.txt", keys, mode, fallbackHits);
}

/* Floors, not counts. The engine owns the catalogs and adds a string every few weeks, so
   every expected total below is derived from lang.def in the same run and held against
   another count of the same thing. These only catch a loop that read nothing at all. */
static const long kMinLanguages = 30;
static const long kMinStrings = 400;
static const long kMinSymbols = 480;

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

  /* 2. No key either file carries has the symbol shape, so the branch is unreachable.
     The same walk counts the symbols lang.def names, which is what a catalog has to
     resolve and what every total below is measured against. */
  long shaped = 0, named = 0;

  for(Table::const_iterator i = keys.begin() ; i != keys.end() ; ++i) {
    if (LangKeyIsSymbol(i->first.c_str())) {
      fprintf(stderr, "lang.def looks up %s, which has the symbol shape\n",
              i->first.c_str());
      shaped++;
    }
    if (LangKeyIsSymbol(i->second.c_str()))
      named++;
  }
  if (named < kMinSymbols) {
    fprintf(stderr, "lang.def names %ld symbols, under the %ld this check needs to prove "
            "anything\n", named, kMinSymbols);
    return 1;
  }

  /* 3. Every catalog resolves byte-identically with the fallback and without it. The
     English pass fills whatever a catalog lacks, so all of them resolve the same count,
     and that count is the engine's to change. */
  const std::vector<std::string> langs = languages(keys);
  long compared = 0, hits = 0, strings = 0;

  if ((long) langs.size() < kMinLanguages) {
    fprintf(stderr, "lang.def names %d languages, under the %ld this check needs to "
            "prove anything\n", (int) langs.size(), kMinLanguages);
    return 1;
  }
  for(size_t i = 0 ; i < langs.size() ; i++) {
    long before = 0, after = 0;
    const Table old = resolve(engine, keys, langs[i], OFF, &before);
    const Table now = resolve(engine, keys, langs[i], GUARDED, &after);

    if (i == 0)
      strings = (long) old.size();
    if (before != 0) {
      fprintf(stderr, "%s: the fallback ran with it switched off\n", langs[i].c_str());
      failures++;
    }
    hits += after;
    if ((long) old.size() != strings) {
      fprintf(stderr, "lang/%s.txt resolves %d strings where lang/%s.txt resolves %ld: "
              "one of the two is short of a string the other carries\n",
              langs[i].c_str(), (int) old.size(), langs[0].c_str(), strings);
      failures++;
    }
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

    loadBoth(probe, keys, off, false, OFF, &ignored);
    loadBoth(probe, keys, on, false, GUARDED, &probed);
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

  /* 5. The guard's direction. newlang.cpp reaches the fallback only where the text
     lookup missed, and no shipped key is both listed in lang.def and symbol-shaped, so
     no real catalog can tell the guarded form from the inverted one. This synthetic row
     is both, and the symbol it resolves to says which form ran. */
  const char *guard = getenv("NEWLANG_KEY_GUARD");

  if (guard != NULL) {
    Table synthetic, guarded, inverted;
    long onHit = 0, offHit = 0;

    synthetic["LANG_PROBE"] = "LANG_REAL";
    loadBoth(guard, synthetic, guarded, false, GUARDED, &onHit);
    loadBoth(guard, synthetic, inverted, false, UNGUARDED, &offHit);
    /* Guarded, the listed key wins and the fallback never runs. */
    if (onHit != 0 || guarded.size() != 1
        || lookup(guarded, "LANG_REAL") != std::string("via lang.def")) {
      fprintf(stderr, "the guarded fallback did not resolve LANG_PROBE through lang.def, "
              "so it is preempting a listed key\n");
      failures++;
    }
    /* Inverted, it overwrites that symbol with the key, which is the whole difference. */
    if (offHit != 1 || inverted.size() != 1
        || lookup(inverted, "LANG_PROBE") != std::string("via lang.def")) {
      fprintf(stderr, "dropping the missed-lookup test changed nothing, so no check here "
              "can see the guard's direction\n");
      failures++;
    }
  } else {
    fprintf(stderr, "NEWLANG_KEY_GUARD unset: the guard's direction is unproven\n");
    failures++;
  }

  /* 6. Parity with the engine's tests/576_webhttrack-lang-symbol-keys.test, over the
     fixtures it builds: re-keying any subset of a catalog resolves every string to the
     same bytes. The last variant also distinguishes the nested pass order from a flat
     one, where English's symbols would run before French's text. */
  const char *parity = getenv("NEWLANG_PARITY_DIR");
  long controls = 0, variants = 0, parityCompared = 0, resolvedSymbols = 0;
  long baselineStrings = 0, gapChecks = 0, gapFilled = 0;

  if (parity != NULL) {
    const std::string dir(parity);
    /* The two copies of "Start" hold different French values, so a lost increment shows
       up as a wrong string and not only as a count. */
    static const struct { const char *symbol, *value; } control[] = {
      { "LANG_J9", "D\xc3\xa9" "marrer..." },
      { "LANG_J13", "DEMARRER!" },
      { NULL, NULL }
    };
    static const struct { const char *french, *english, *what; } rekeyed[] = {
      { "fr-all.txt", "en-none.txt", "every French line" },
      { "fr-odd-last.txt", "en-none.txt", "the odd French lines, moved to the end" },
      { "fr-all.txt", "en-all.txt", "both catalogs" },
      { "fr-none.txt", "en-all.txt", "English alone" },
      { NULL, NULL, NULL }
    };
    long ignored = 0;
    const Table want = resolveFiles(dir + "/fr-none.txt", dir + "/en-none.txt",
                                    keys, GUARDED, &ignored);

    baselineStrings = (long) want.size();
    for(Table::const_iterator k = want.begin() ; k != want.end() ; ++k) {
      if (LangKeyIsSymbol(k->first.c_str()))
        resolvedSymbols++;
    }
    for(int k = 0 ; control[k].symbol != NULL ; k++) {
      const char *const got = lookup(want, control[k].symbol);

      if (strcmp(got, control[k].value) != 0) {
        fprintf(stderr, "the text-keyed French catalog resolved %s to \"%s\", expected "
                "\"%s\"\n", control[k].symbol, got, control[k].value);
        failures++;
      } else
        controls++;
    }
    for(int v = 0 ; rekeyed[v].french != NULL ; v++) {
      long hit = 0;
      const Table got = resolveFiles(dir + "/" + rekeyed[v].french,
                                     dir + "/" + rekeyed[v].english, keys, GUARDED, &hit);

      if (got.size() != want.size()) {
        fprintf(stderr, "re-keying %s resolves %d strings, expected %d\n",
                rekeyed[v].what, (int) got.size(), (int) want.size());
        failures++;
      }
      for(Table::const_iterator k = want.begin() ; k != want.end() ; ++k) {
        if (k->second != lookup(got, k->first)) {
          fprintf(stderr, "re-keying %s: %s was \"%s\", is now \"%s\"\n",
                  rekeyed[v].what, k->first.c_str(), k->second.c_str(),
                  lookup(got, k->first));
          failures++;
        }
        parityCompared++;
      }
      variants++;
    }

    /* 7. The one string this change moves. fr-gap.txt drops the second copy of "Start",
       which a text-keyed English cannot reach, because its pass never increments. A
       symbol-keyed English names that slot outright and fills it. */
    {
      long hit = 0;
      const Table text = resolveFiles(dir + "/fr-gap.txt", dir + "/en-none.txt",
                                      keys, GUARDED, &hit);
      const Table symbol = resolveFiles(dir + "/fr-gap.txt", dir + "/en-all.txt",
                                        keys, GUARDED, &hit);

      if (*lookup(text, "LANG_J13") != '\0') {
        fprintf(stderr, "fr-gap.txt still resolves LANG_J13, so it holds no gap and the "
                "two loads below cannot differ\n");
        failures++;
      } else
        gapChecks++;
      if (strcmp(lookup(symbol, "LANG_J13"), "Start") != 0) {
        fprintf(stderr, "a symbol-keyed English left LANG_J13 as \"%s\", expected the "
                "English \"Start\"\n", lookup(symbol, "LANG_J13"));
        failures++;
      } else
        gapChecks++;
      for(Table::const_iterator k = symbol.begin() ; k != symbol.end() ; ++k) {
        if (k->second != lookup(text, k->first))
          gapFilled++;
      }
      if (gapFilled != 1 || symbol.size() != text.size() + 1) {
        fprintf(stderr, "a symbol-keyed English moved %ld strings and %d slots, expected "
                "the one gap\n", gapFilled, (int) (symbol.size() - text.size()));
        failures++;
      } else
        gapChecks++;
    }
  } else {
    fprintf(stderr, "NEWLANG_PARITY_DIR unset: the engine's fixtures never ran\n");
    failures++;
  }

  /* Every total is held against another count of the same thing, one check per cause, so
     a loop that stopped running cannot hide behind another one's figure. */
  if (compared != (long) langs.size() * strings || strings < kMinStrings) {
    fprintf(stderr, "compared %ld strings over %ld catalogs of %ld each: the loop over "
            "the catalogs stopped short\n", compared, (long) langs.size(), strings);
    failures++;
  }
  if (baselineStrings != strings) {
    fprintf(stderr, "the rebuilt French fixture resolves %ld strings where the shipped "
            "catalog resolves %ld: the builder pairs the lines up differently from the "
            "loader\n", baselineStrings, strings);
    failures++;
  }
  if (resolvedSymbols != named) {
    fprintf(stderr, "French resolves %ld of the %ld symbols lang.def names: a catalog "
            "line for one of them went missing upstream\n", resolvedSymbols, named);
    failures++;
  }
  if (parityCompared != variants * baselineStrings) {
    fprintf(stderr, "compared %ld strings over %ld re-keyed variants of %ld each: the "
            "loop over the fixtures stopped short\n",
            parityCompared, variants, baselineStrings);
    failures++;
  }
  /* Our own fixtures, so these stay exact. */
  if (controls != 2 || variants != 4 || gapChecks != 3 || gapFilled != 1) {
    fprintf(stderr, "ran %ld controls, %ld variants, %ld gap checks and filled %ld gap "
            "strings, expected 2, 4, 3 and 1\n",
            controls, variants, gapChecks, gapFilled);
    failures++;
  }

  printf("%ld of the %d lang.def keys have the symbol shape and the fallback resolved "
         "%ld rows, so the %ld resolved strings of %d catalogs match byte for byte\n",
         shaped, (int) keys.size(), hits, compared, (int) langs.size());
  printf("%ld re-keyed catalog pairs resolve the same %ld strings, %ld of them symbols, "
         "on %ld comparisons, and a symbol-keyed English fills %ld string a text-keyed "
         "one cannot\n", variants, baselineStrings, resolvedSymbols, parityCompared,
         gapFilled);
  return (failures == 0 && shaped == 0) ? 0 : 1;
}
