/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Xavier Roche and other contributors

   This holds every language's combo lists against the entry count the engine's key
   table states, which --selftest does on Windows and a reviewer cannot rerun there.
   tools/test-winprofile-bind.py drives it. */

#include "htslines.h"

/* For BOOL, which winprofile-bind.h's helpers are declared with. */
#include "winprofile-io-test.h"
#include "winprofile-bind.h"

#include <map>
#include <string>
#include <vector>

/* How long a line does LANG_LOAD() give linput_cpp()? */
#define CATALOG_LINE_MAX 8000
#define CATALOG_FLAGS (HTS_LINE_DROP_TAB | HTS_LINE_DROP_FF | HTS_LINE_DROP_NUL)

typedef std::map<std::string, std::string> Catalog;

/* LANGINTKEY() and LANGSEL() both answer "" for a key they do not hold. */
static const char *look(const Catalog &c, const std::string &key) {
  const Catalog::const_iterator i = c.find(key);

  return (i == c.end()) ? "" : i->second.c_str();
}

/* This reads every line of PATH, as LANG_LOAD()'s feof() loop does, and answers FALSE
   when it cannot open the file, which the caller must report rather than treat as an
   empty catalog. hts_readline_cpp() is the engine's own reader, because a line pattern of
   our own mis-pairs keys and values at a continuation line. */
static bool catalogLines(const std::string &path, std::vector<std::string> &out) {
  FILE *const fp = fopen(path.c_str(), "rb");
  char line[CATALOG_LINE_MAX + 1];

  if (fp == NULL)
    return false;
  while (!feof(fp)) {
    hts_readline_cpp(fp, line, CATALOG_LINE_MAX, CATALOG_FLAGS);
    out.push_back(line);
  }
  fclose(fp);
  return true;
}

/* This appends the suffix LANG_LOAD() gives a repeated definition. */
static std::string suffixed(const std::string &key, int n) {
  char num[32];

  sprintf(num, "%d", n);
  return key + num;
}

/* This reads lang.def into NewLangStrKeys. The lookup key is the second line of each
   pair, and the internal key is the first. */
static bool loadKeys(const std::string &path, Catalog &keys) {
  std::vector<std::string> lines;

  if (!catalogLines(path, lines))
    return false;
  for (size_t i = 0; i + 1 < lines.size(); i += 2) {
    const std::string intkey = lines[i];
    std::string key = lines[i + 1];

    if (intkey.empty() || key.empty())
      continue;
    for (int n = 1; *look(keys, key) != '\0'; n++)
      key = suffixed(lines[i + 1], n);
    keys[key] = intkey;
  }
  return true;
}

/* This reads one lang/<name>.txt into NewLangStr. LOOPS is LANG_LOAD()'s pass, so 0 is
   the chosen language and 1 the English file it backfills every unset key from. */
static bool loadStrings(const std::string &path, const Catalog &keys, Catalog &strs,
                        int loops) {
  std::vector<std::string> lines;

  if (!catalogLines(path, lines))
    return false;
  for (size_t i = 0; i + 1 < lines.size(); i += 2) {
    const std::string extkey = lines[i];
    const std::string value = lines[i + 1];
    std::string intkey;

    if (extkey.empty() || value.empty())
      continue;
    intkey = look(keys, extkey);
    if (intkey.empty())
      continue;
    if (*look(strs, intkey) != '\0') {
      if (loops != 0)
        continue;
      for (int n = 1;; n++) {
        intkey = look(keys, suffixed(extkey, n));
        if (intkey.empty() || *look(strs, intkey) == '\0')
          break;
      }
      if (intkey.empty())
        continue;
    }
    strs[intkey] = value;
  }
  return true;
}

/* This applies conv_printf()'s escapes, so '\n' becomes the separator
   countComboEntries() splits on. Its DBCS pairing needs the catalog's codepage and is
   left out, so an entry whose trail byte is 0x5c is counted right only by --selftest. */
static std::string unescape(const std::string &value) {
  std::string out;

  for (size_t i = 0; i < value.size(); i++) {
    if (value[i] != '\\' || i + 1 == value.size()) {
      out += value[i];
      continue;
    }
    switch (value[++i]) {
    case 'a': out += '\a'; break;
    case 'b': out += '\b'; break;
    case 'f': out += '\f'; break;
    case 'n': out += '\n'; break;
    case 'r': out += '\r'; break;
    case 't': out += '\t'; break;
    case 'v': out += '\v'; break;
    default:  out += value[i]; break;
    }
  }
  return out;
}

int main(int argc, char **argv) {
  const std::string engine = (argc > 1) ? argv[1] : ".";
  Catalog keys;
  std::vector<std::string> english;
  int nlangs = 0, nchecks = 0, ntranslated = 0;

  /* The list's name, not its text: cpp_lang.h is not included here, so LISTDEF_n is still
     the token --selftest expands into a catalog lookup. */
#define WP_COMBO_ROW(key, listdef) { key, #listdef },
  static const struct { const char *key, *listdef; } combos[] = {
    WINPROFILE_LIST_COMBOS(WP_COMBO_ROW)
  };
#undef WP_COMBO_ROW
  const int ncombos = (int) (sizeof(combos) / sizeof(combos[0]));

  if (!loadKeys(engine + "/lang.def", keys)) {
    fprintf(stderr, "FAIL: no %s/lang.def, so there is no catalog to check\n",
            engine.c_str());
    return 2;
  }
  if (ncombos != WINPROFILE_LIST_COMBO_COUNT) {
    fprintf(stderr, "FAIL: the combo list names %d lists, WINPROFILE_LIST_COMBO_COUNT"
            " says %d\n", ncombos, WINPROFILE_LIST_COMBO_COUNT);
    return 1;
  }
  for (int l = 1;; l++) {
    char slot[32];
    std::string name;
    Catalog strs;

    sprintf(slot, "LANGUAGE_%d", l);
    name = look(keys, slot);
    if (name.empty())
      break;
    if (!loadStrings(engine + "/lang/" + name + ".txt", keys, strs, 0)
        || !loadStrings(engine + "/lang/" + std::string(look(keys, "LANGUAGE_1")) + ".txt",
                        keys, strs, 1)) {
      fprintf(stderr, "FAIL: cannot read the catalog of %s\n", name.c_str());
      return 2;
    }
    for (int k = 0; k < ncombos; k++) {
      const std::string value = unescape(look(strs, combos[k].listdef));
      const int offered = countComboEntries(value.c_str());
      int base = 0, count = 0;

      if (value.empty()) {
        fprintf(stderr, "FAIL: %s holds no %s\n", name.c_str(), combos[k].listdef);
        return 1;
      }
      if (!winprofileListRange(combos[k].key, &base, &count) || count != offered) {
        fprintf(stderr, "FAIL: the %s combo offers %d entries from %s in %s,"
                " the table names %d\n", combos[k].key, offered, combos[k].listdef,
                name.c_str(), count);
        return 1;
      }
      /* A loader that silently found nothing would leave every language reading as
         English and every count agreeing. Translated text is what shows it did not. */
      if (nlangs == 0)
        english.push_back(value);
      else if (english[k] != value)
        ntranslated++;
      nchecks++;
    }
    nlangs++;
  }
  if (nlangs < 2) {
    fprintf(stderr, "FAIL: found %d languages in %s/lang.def\n", nlangs, engine.c_str());
    return 2;
  }
  if (ntranslated == 0) {
    fprintf(stderr, "FAIL: no language carries a list of its own, so the catalogs were"
            " never really read\n");
    return 2;
  }
  printf("%d catalog lists agree with the engine's table over %d languages"
         " (%d lists translated)\n", nchecks, nlangs, ntranslated);
  return 0;
}
