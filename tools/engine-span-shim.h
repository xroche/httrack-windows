/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Xavier Roche and other contributors

   The two engine span helpers hts_scan_token() calls, written to their contract in
   the engine's src/htsurlport.h, so a harness can link the copied-out tokenizer. The
   GUI sizes every token buffer to its whole input, so the clip never runs. */
#ifndef ENGINE_SPAN_SHIM_H
#define ENGINE_SPAN_SHIM_H

#include <stddef.h>
#include <string.h>

typedef struct {
  const char *p;
  size_t len;
} hts_span;

static hts_span hts_span_at(const char *p, size_t len) {
  hts_span r;

  r.p = p;
  r.len = len;
  return r;
}

/* Copy S into DST clipped to SIZE - 1 bytes. FALSE if clipped, or if SIZE is 0. */
static int hts_span_copy(hts_span s, char *dst, size_t size) {
  size_t n;

  if (size == 0)
    return 0;
  n = s.len < size - 1 ? s.len : size - 1;
  memcpy(dst, s.p, n);
  dst[n] = '\0';
  return n == s.len;
}

#endif
