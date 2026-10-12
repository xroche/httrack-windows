/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Xavier Roche and other contributors

   Runs the catalog loader of WinHTTrack/newlang-key.cpp over the engine's catalogs, then
   over the fixtures the engine's tests/576_webhttrack-lang-symbol-keys.test re-keys, and
   requires the same strings out of each. The loader is the shipped one, so the five
   ordering rules it follows are checked here and not in a copy: std::map stands in for
   coucal, the line reader below for the engine's, and tools/test-newlang-key.py mutates
   each rule in turn to prove a broken one reds this harness. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "newlang-key.h"

/* The engine's reader, copied from WinHTTrack/HTTrackInterface.c because including it
   would pull in htsopt.h. It decides where a key ends, so this is the real one: CR, TAB
   and FF are dropped, space and TAB are trimmed off both ends, and a trailing backslash
   joins the next physical line. */
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

/* What the loader reads and writes through, in place of a catalog FILE* and coucal. */
struct Catalogs {
  std::string paths[2];
  const Table *keys;
  std::set<std::string> named;   /* every internal key lang.def names */
  Table str;
  long unnamed;                  /* strings resolved under a key lang.def never names */

  Catalogs(const std::string &selected, const std::string &english, const Table &k)
      : keys(&k), unnamed(0) {
    paths[0] = selected;
    paths[1] = english;
    for(Table::const_iterator i = k.begin() ; i != k.end() ; ++i)
      named.insert(i->second);
  }
};

static FILE *catalogOpen(void *ctx, int index) {
  Catalogs *const c = (Catalogs *) ctx;
  FILE *const fp = fopen(c->paths[index].c_str(), "rb");

  if (fp == NULL) {
    fprintf(stderr, "cannot open %s\n", c->paths[index].c_str());
    exit(1);
  }
  return fp;
}

static const char *catalogInternalKey(void *ctx, const char *key) {
  return lookup(*((Catalogs *) ctx)->keys, key);
}

static const char *catalogResolved(void *ctx, const char *intkey) {
  return lookup(((Catalogs *) ctx)->str, intkey);
}

static void catalogStore(void *ctx, const char *intkey, const char *value) {
  Catalogs *const c = (Catalogs *) ctx;

  c->str[intkey] = value;
  if (c->named.find(intkey) == c->named.end())
    c->unnamed++;
}

/* One resolved table, built by the shipped loader. ConvertCatalogValue() is left out,
   because it maps a value to a value no key rule can reach, so equal raw tables stay
   equal through it. */
static Table resolveFiles(const std::string &selected, const std::string &english,
                          const Table &keys, long *unnamed) {
  Catalogs c(selected, english, keys);
  const LangCatalogIO io = { &c, catalogOpen, linput_cpp, catalogInternalKey,
                             catalogResolved, catalogStore };

  LangLoadCatalogs(&io);
  *unnamed += c.unnamed;
  return c.str;
}

static Table resolve(const std::string &engine, const Table &keys,
                     const std::string &language, long *unnamed) {
  return resolveFiles(engine + "/lang/" + language + ".txt",
                      engine + "/lang/English.txt", keys, unnamed);
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

  /* 3. Every catalog resolves the same slots, each under a key lang.def names, which is
     what leaves the symbol branch unreachable on shipped data. A catalog short of a
     string the others carry shows up here as a slot nothing filled. */
  const std::vector<std::string> langs = languages(keys);
  long compared = 0, unnamed = 0, strings = 0;
  Table reference;

  if ((long) langs.size() < kMinLanguages) {
    fprintf(stderr, "lang.def names %d languages, under the %ld this check needs to "
            "prove anything\n", (int) langs.size(), kMinLanguages);
    return 1;
  }
  for(size_t i = 0 ; i < langs.size() ; i++) {
    const Table now = resolve(engine, keys, langs[i], &unnamed);

    if (i == 0) {
      reference = now;
      strings = (long) now.size();
    }
    if ((long) now.size() != strings) {
      fprintf(stderr, "lang/%s.txt resolves %d strings where lang/%s.txt resolves %ld: "
              "one of the two is short of a string the other carries\n",
              langs[i].c_str(), (int) now.size(), langs[0].c_str(), strings);
      failures++;
    }
    for(Table::const_iterator k = reference.begin() ; k != reference.end() ; ++k) {
      if (!lookup(now, k->first)[0]) {
        fprintf(stderr, "%s: %s resolves to nothing, where %s resolves it\n",
                langs[i].c_str(), k->first.c_str(), langs[0].c_str());
        failures++;
      }
      compared++;
    }
  }
  if (unnamed != 0) {
    fprintf(stderr, "%ld strings resolved under a key lang.def never names: a shipped "
            "catalog is symbol-keyed and the symbol path is no longer unreachable\n",
            unnamed);
    failures++;
  }

  /* 4. The control: the walk above finds no line taking the symbol path, so this is what
     says the path is there to be taken. Only that path can key a string on the catalog's
     own key, because lang.def looks nothing up by LANG_OK. */
  const char *probe = getenv("NEWLANG_KEY_PROBE");

  if (probe != NULL) {
    long ignored = 0;
    const Table on = resolveFiles(probe, probe, keys, &ignored);

    if (on.size() != 1 || lookup(on, "LANG_OK") != std::string("probe value")) {
      fprintf(stderr, "the probe catalog resolved %d strings and LANG_OK to \"%s\": the "
              "loader never takes the symbol path\n", (int) on.size(),
              lookup(on, "LANG_OK"));
      failures++;
    }
  } else {
    fprintf(stderr, "NEWLANG_KEY_PROBE unset: the symbol path is unproven\n");
    failures++;
  }

  /* 5. The guard's direction. The loader reaches the symbol path only where the text
     lookup missed, and no shipped key is both listed in lang.def and symbol-shaped, so no
     real catalog can tell that order from the inverted one. This synthetic row is both,
     and the key it resolves under says which one ran. */
  const char *guard = getenv("NEWLANG_KEY_GUARD");

  if (guard != NULL) {
    Table synthetic;
    long claimed = 0;

    synthetic["LANG_PROBE"] = "LANG_REAL";
    const Table resolvedGuard = resolveFiles(guard, guard, synthetic, &claimed);
    if (claimed != 0 || resolvedGuard.size() != 1
        || lookup(resolvedGuard, "LANG_REAL") != std::string("via lang.def")) {
      fprintf(stderr, "a key lang.def lists and the shape test accepts resolved %ld rows "
              "under itself: the shape test is deciding before the text lookup\n",
              claimed);
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
                                    keys, &ignored);

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
      const Table got = resolveFiles(dir + "/" + rekeyed[v].french,
                                     dir + "/" + rekeyed[v].english, keys, &ignored);

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

    /* 7. The one string the symbol key moves. fr-gap.txt drops the second copy of
       "Start", which a text-keyed English cannot reach, because its pass never
       increments. A symbol-keyed English names that slot outright and fills it. */
    {
      const Table text = resolveFiles(dir + "/fr-gap.txt", dir + "/en-none.txt",
                                      keys, &ignored);
      const Table symbol = resolveFiles(dir + "/fr-gap.txt", dir + "/en-all.txt",
                                        keys, &ignored);

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

  printf("%ld of the %d lang.def keys have the symbol shape and %ld of the %ld resolved "
         "strings of %d catalogs key on anything else\n",
         shaped, (int) keys.size(), unnamed, compared, (int) langs.size());
  printf("%ld re-keyed catalog pairs resolve the same %ld strings, %ld of them symbols, "
         "on %ld comparisons, and a symbol-keyed English fills %ld string a text-keyed "
         "one cannot\n", variants, baselineStrings, resolvedSymbols, parityCompared,
         gapFilled);
  return (failures == 0 && shaped == 0) ? 0 : 1;
}
