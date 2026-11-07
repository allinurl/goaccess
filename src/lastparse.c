/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2026 Gerardo Orellana <hello @ goaccess.io>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Persistence of log resume markers, including migration of legacy snippets. */

#include <stdlib.h>
#include <string.h>

#include "lastparse.h"

#include "error.h"

/* Version 1 stores SHA-256 digests of the recorded prefix length. */
#define LAST_PARSE_VERSION 1
#define LAST_PARSE_FORMAT "uA(US(uIUvc#))"
#define LAST_PARSE_LEGACY_FORMAT "A(US(uIUvc#))"

/* Exact layout of the historical marker, used only while loading old files. */
typedef struct GLastParseLegacy_ {
  uint32_t line;
  int64_t ts;
  uint64_t size;
  uint16_t snippetlen;
  char snippet[READ_BYTES + 1];
} GLastParseLegacy;

/* Load old resume metadata, replacing its raw snippet with a fingerprint. */
static void
load_legacy_last_parse (khash_t (iglp) *hash, const char *path) {
  GLastParseLegacy old = { 0 };
  GLastParse val = { 0 };
  tpl_node *tn = NULL;
  char fmt[] = LAST_PARSE_LEGACY_FORMAT;
  uint64_t key = 0;
  int ret = 0;

  tn = tpl_map (fmt, &key, &old, READ_BYTES);
  if (!tn || tpl_load (tn, TPL_FILE, path) != 0)
    FATAL ("Unable to load log resume markers from '%s'.", path);

  while ((ret = tpl_unpack (tn, 1)) > 0) {
    if (old.snippetlen > READ_BYTES)
      FATAL ("Invalid log prefix length in '%s'.", path);
    val.line = old.line;
    val.ts = old.ts;
    val.size = old.size;
    val.snippetlen = old.snippetlen;
    sha256_digest (old.snippet, old.snippetlen, val.fingerprint);
    if (ins_iglp (hash, key, &val) != 0)
      FATAL ("Unable to restore log resume markers from '%s'.", path);
  }
  tpl_free (tn);
  if (ret < 0)
    FATAL ("Invalid log resume markers in '%s'.", path);
}

/* Load fingerprinted resume metadata, accepting the original snippet format. */
void
load_last_parse (khash_t (iglp) *hash, const char *path) {
  GLastParse val = { 0 };
  tpl_node *tn = NULL;
  char *fmt = NULL, format[] = LAST_PARSE_FORMAT;
  uint64_t key = 0;
  uint32_t version = 0;
  int legacy = 0, ret = 0;

  fmt = tpl_peek (TPL_FILE, path);
  if (!fmt)
    FATAL ("Unable to read log resume markers from '%s'.", path);
  legacy = strcmp (fmt, LAST_PARSE_LEGACY_FORMAT) == 0;
  if (!legacy && strcmp (fmt, LAST_PARSE_FORMAT) != 0)
    FATAL ("Unsupported log resume marker format in '%s'.", path);
  free (fmt);

  if (legacy) {
    load_legacy_last_parse (hash, path);
    return;
  }

  tn = tpl_map (format, &version, &key, &val, SHA256_DIGEST_SIZE);
  if (!tn || tpl_load (tn, TPL_FILE, path) != 0 || tpl_unpack (tn, 0) <= 0)
    FATAL ("Unable to load log resume markers from '%s'.", path);
  if (version != LAST_PARSE_VERSION)
    FATAL ("Unsupported log resume marker version in '%s'.", path);

  while ((ret = tpl_unpack (tn, 1)) > 0) {
    if (val.snippetlen > READ_BYTES)
      FATAL ("Invalid log prefix length in '%s'.", path);
    if (ins_iglp (hash, key, &val) != 0)
      FATAL ("Unable to restore log resume markers from '%s'.", path);
  }
  tpl_free (tn);
  if (ret < 0)
    FATAL ("Invalid log resume markers in '%s'.", path);
}

/* Pack resume metadata without raw log text, including an empty marker set.
 *
 * On success, the packed node is returned for the caller to save and free.
 * On failure, a fatal error is raised. */
tpl_node *
pack_last_parse (khash_t (iglp) *hash) {
  GLastParse val = { 0 };
  tpl_node *tn = NULL;
  char fmt[] = LAST_PARSE_FORMAT;
  khint_t k;
  uint64_t key = 0;
  uint32_t version = LAST_PARSE_VERSION;

  tn = tpl_map (fmt, &version, &key, &val, SHA256_DIGEST_SIZE);
  if (!tn || tpl_pack (tn, 0) != 0)
    FATAL ("Unable to pack log resume markers.");

  for (k = 0; k < kh_end (hash); k++) {
    if (!kh_exist (hash, k))
      continue;
    key = kh_key (hash, k);
    val = kh_val (hash, k);
    if (tpl_pack (tn, 1) != 0)
      FATAL ("Unable to pack log resume markers.");
  }
  return tn;
}
