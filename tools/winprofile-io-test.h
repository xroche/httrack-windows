/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Xavier Roche and other contributors

   Just enough of MFC and of the engine for WinHTTrack/winprofile-io.cpp to compile off
   Windows, so tools/winprofile-bind-test.cpp exercises the real writer. Only CString,
   linput() and strcatbuff() are needed, and each is reproduced to its real contract.
   See tools/test-winprofile-bind.py. */

#ifndef WINPROFILE_IO_TEST_H
#define WINPROFILE_IO_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

/* The engine macros the binding list names. htsglobal.h cannot be included here, because
   it pulls in htsfeatures.h, which only the engine's own build generates.
   tools/test-winprofile-bind.py copies the #define lines out of it, so the value under
   test is the engine's rather than a copy kept here. */
#include "winprofile-engine-macros.h"

#define BOOL int
#define TRUE 1
#define FALSE 0
typedef const char *LPCTSTR;
typedef const char *LPCSTR;

/* MFC's CString answers to far more than this. What winprofile-io.cpp uses is here. */
class CString {
public:
  CString() {}
  CString(const char *p) : s(p != NULL ? p : "") {}
  CString& operator=(const char *p) { s = (p != NULL ? p : ""); return *this; }
  CString& operator+=(char c) { s += c; return *this; }
  CString& operator+=(const char *p) { s += (p != NULL ? p : ""); return *this; }
  bool operator==(const char *p) const { return s == (p != NULL ? p : ""); }
  bool operator!=(const char *p) const { return !(*this == p); }
  bool operator==(const CString &o) const { return s == o.s; }
  bool operator!=(const CString &o) const { return s != o.s; }
  operator const char*() const { return s.c_str(); }
  const char *GetBuffer(int) const { return s.c_str(); }
  int GetLength() const { return (int) s.size(); }
  bool IsEmpty() const { return s.empty(); }
  void Format(const char *f, int n) { char b[32]; sprintf(b, f, n); s = b; }
  int Find(char c) const {
    const size_t i = s.find(c);
    return (i == std::string::npos) ? -1 : (int) i;
  }
  CString Left(int n) const { return CString(s.substr(0, n).c_str()); }
  CString Mid(int n) const { return CString(s.substr(n).c_str()); }
  void TrimLeft() {
    const size_t i = s.find_first_not_of(" \t\r\n");
    s = (i == std::string::npos) ? "" : s.substr(i);
  }
  void TrimRight() {
    const size_t i = s.find_last_not_of(" \t\r\n");
    s = (i == std::string::npos) ? "" : s.substr(0, i + 1);
  }
  std::string s;
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

#endif
