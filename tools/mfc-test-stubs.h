/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Xavier Roche and other contributors

   Just enough of MFC and of the engine for WinHTTrack/winprofile-io.cpp,
   WinHTTrack/rules-split.cpp and WinHTTrack/argv-caps.cpp to compile off Windows, so the
   harnesses in this directory exercise the real code. Each stub is reproduced to its real
   contract. See tools/test-winprofile-bind.py, tools/test-rules-split.py and
   tools/test-argv-caps.py. */

#ifndef MFC_TEST_STUBS_H
#define MFC_TEST_STUBS_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#ifdef WINPROFILE_IO_TEST
/* The engine macros the binding list names. htsglobal.h cannot be included here, because
   it pulls in htsfeatures.h, which only the engine's own build generates.
   tools/test-winprofile-bind.py copies the #define lines out of it, so the value under
   test is the engine's rather than a copy kept here. */
#include "winprofile-engine-macros.h"
#endif

#define BOOL int
#define TRUE 1
#define FALSE 0
#define _T(x) x
typedef const char *LPCTSTR;
typedef const char *LPCSTR;
typedef ptrdiff_t INT_PTR;

/* MFC's CString answers to far more than this. What the two units use is here. */
class CString {
public:
  CString() {}
  CString(const char *p) : s(p != NULL ? p : "") {}
  CString(char c, int n) : s((size_t) n, c) {}
  CString& operator=(const char *p) { s = (p != NULL ? p : ""); return *this; }
  CString& operator+=(char c) { s += c; return *this; }
  CString& operator+=(const char *p) { s += (p != NULL ? p : ""); return *this; }
  CString& operator+=(const CString &o) { s += o.s; return *this; }
  bool operator==(const char *p) const { return s == (p != NULL ? p : ""); }
  bool operator!=(const char *p) const { return !(*this == p); }
  bool operator==(const CString &o) const { return s == o.s; }
  bool operator!=(const CString &o) const { return s != o.s; }
  char operator[](int i) const { return s[(size_t) i]; }
  operator const char*() const { return s.c_str(); }
  int GetLength() const { return (int) s.size(); }
  bool IsEmpty() const { return s.empty(); }
  void Format(const char *f, int n) { char b[32]; sprintf(b, f, n); s = b; }
  int Find(char c) const {
    const size_t i = s.find(c);
    return (i == std::string::npos) ? -1 : (int) i;
  }
  CString Left(int n) const { return CString(s.substr(0, n).c_str()); }
  CString Mid(int n) const { return CString(s.substr(n).c_str()); }
  CString Mid(int n, int len) const { return CString(s.substr(n, len).c_str()); }
  CString Right(int n) const { return CString(s.substr(s.size() - (size_t) n).c_str()); }
  void TrimLeft() {
    const size_t i = s.find_first_not_of(" \t\r\n");
    s = (i == std::string::npos) ? "" : s.substr(i);
  }
  void TrimRight() {
    const size_t i = s.find_last_not_of(" \t\r\n");
    s = (i == std::string::npos) ? "" : s.substr(0, i + 1);
  }
  void Trim(const char *set) {
    const size_t i = s.find_first_not_of(set);
    s = (i == std::string::npos) ? "" : s.substr(i);
    const size_t j = s.find_last_not_of(set);
    s = (j == std::string::npos) ? "" : s.substr(0, j + 1);
  }
  /* MFC hands out the live buffer and leaves GetLength() alone until ReleaseBuffer().
     A copy serves here, because neither unit writes through it and reads back a CString. */
  char *GetBuffer(int = 0) { buf.assign(s.begin(), s.end()); buf.push_back('\0'); return &buf[0]; }
  const char *GetBuffer(int) const { return s.c_str(); }
  char *GetBufferSetLength(int n) { s.assign((size_t) n, '\0'); return GetBuffer(); }
  std::string s;
private:
  std::vector<char> buf;
};

inline CString operator+(const CString &a, const char *b) { CString r(a); r += b; return r; }
inline CString operator+(const CString &a, const CString &b) { CString r(a); r += b; return r; }

/* CSimpleArray counts in int, CStringArray in INT_PTR, and both are indexed by the moved
   code and by the cases it carries. */
template<class T> class CSimpleArray {
public:
  void Add(const T &v) { v_.push_back(v); }
  int GetSize() const { return (int) v_.size(); }
  const T& operator[](int i) const { return v_[(size_t) i]; }
private:
  std::vector<T> v_;
};

class CStringArray {
public:
  void Add(const CString &v) { v_.push_back(v); }
  INT_PTR GetSize() const { return (INT_PTR) v_.size(); }
  const CString& operator[](INT_PTR i) const { return v_[(size_t) i]; }
private:
  std::vector<CString> v_;
};

/* The engine's htslib.c reader: CR, TAB and FF go, LF ends the line. Dropping a raw TAB
   is why the writer escapes one. */
static inline int linput(FILE *fp, char *s, int max) {
  int c, j = 0;

  do {
    c = fgetc(fp);
    if (c != EOF) {
      switch (c) {
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

#define strcatbuff(a, b) strcat(a, b)

#ifdef ARGV_CAPS_TEST
/* A legacy codepage, which is what makes the accented case decidable, and what the
   Windows runner reports too. */
#define CP_UTF8 65001u
static inline unsigned int GetACP(void) { return 1252u; }

/* The engine's converter under that codepage, over the Latin-1 range where CP1252 agrees
   with ISO-8859-1. A case using 0x80 to 0x9F would cost three bytes on Windows, two here. */
static inline char *hts_convertStringSystemToUTF8(const char *s, size_t size) {
  char *const out = (char *) malloc(size * 2 + 1);
  size_t j = 0;

  if (out == NULL)
    return NULL;
  for(size_t i = 0 ; i < size ; i++) {
    const unsigned char c = (unsigned char) s[i];

    if (c < 0x80) {
      out[j++] = (char) c;
    } else {
      out[j++] = (char) (0xc0 | (c >> 6));
      out[j++] = (char) (0x80 | (c & 0x3f));
    }
  }
  out[j] = '\0';
  return out;
}

/* The engine's freet() nulls what it frees, so its callers hold a non-const pointer. */
#define freet(p) do { free(p); (p) = NULL; } while(0)
#endif

#endif
