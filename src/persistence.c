/**
 * persistence.c -- on-disk persistence functionality
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

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "persistence.h"

#include "error.h"
#include "gkhash.h"
#include "sort.h"
#include "tpl.h"
#include "util.h"
#include "xmalloc.h"

static uint32_t *persisted_dates = NULL;
static uint32_t persisted_dates_len = 0;

/* Determine the path for the given database file.
 *
 * On error, a fatal error is thrown.
 * On success, the databases path string is returned. */
static char *
set_db_path (const char *fn) {
  struct stat info;
  char *rpath = NULL, *path = NULL;
  const char *dbpath = NULL;

  if (!conf.db_path)
    dbpath = DB_PATH;
  else
    dbpath = conf.db_path;

  rpath = realpath (dbpath, NULL);
  if (rpath == NULL)
    FATAL ("Unable to open the specified db path/file '%s'. %s", dbpath, strerror (errno));

  /* sanity check: Is db_path accessible and a directory? */
  if (stat (rpath, &info) != 0)
    FATAL ("Unable to access database path: %s", strerror (errno));
  else if (!(info.st_mode & S_IFDIR))
    FATAL ("Database path is not a directory.");

  path = xmalloc (snprintf (NULL, 0, "%s/%s", rpath, fn) + 1);
  sprintf (path, "%s/%s", rpath, fn);
  free (rpath);

  return path;
}

/* Dump to disk the database file and frees its memory. The data is written
 * to a temporary file first and renamed into place so an interrupted write
 * never truncates an existing database. */
/* Set when any database write of the current persist pass fails; the version
 * metadata is then withheld so the dataset is not marked complete. */
static int persist_error = 0;

static void
close_tpl (tpl_node *tn, const char *fn) {
  char *tmp = NULL;

  tmp = xmalloc (snprintf (NULL, 0, "%s.tmp", fn) + 1);
  sprintf (tmp, "%s.tmp", fn);

  if (tpl_dump (tn, TPL_FILE, tmp) != 0 || rename (tmp, fn) != 0) {
    persist_error = 1;
    unlink (tmp);
  }

  tpl_free (tn);
  free (tmp);
}

/* Databases consumed by a migration; removed only after the migrated data
 * has been persisted in the current format. */
static char **migrated_files = NULL;
static uint32_t migrated_files_len = 0;

/* Queue a legacy database file for removal once the migrated data has been
 * persisted in the current format. */
static void
defer_migrated_unlink (const char *path) {
  migrated_files = xrealloc (migrated_files, (migrated_files_len + 1) * sizeof (char *));
  migrated_files[migrated_files_len++] = xstrdup (path);
}

/* Remove all legacy database files consumed by a completed migration. */
static void
unlink_migrated_files (void) {
  uint32_t i;

  for (i = 0; i < migrated_files_len; ++i) {
    unlink (migrated_files[i]);
    free (migrated_files[i]);
  }
  free (migrated_files);
  migrated_files = NULL;
  migrated_files_len = 0;
}

/* Get the on-disk database version of the restored dataset.
 *
 * If no version was persisted, 1 is returned (pre-versioning database).
 * On success, the persisted version is returned. */
static uint32_t
get_db_version (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * db_props = get_hdb (db, MTRC_DB_PROPS);
  khint_t k;

  k = kh_get (si32, db_props, "version");
  if (k == kh_end (db_props))
    return 1;
  return kh_val (db_props, k);
}

/* Given a database filename, restore a string key, uint32_t value back to the
 * storage */
static void
restore_global_si08 (khash_t (si08) *hash, const char *fn) {
  tpl_node *tn;
  char *key = NULL;
  char fmt[] = "A(sv)";
  uint16_t val;

  tn = tpl_map (fmt, &key, &val);
  tpl_load (tn, TPL_FILE, fn);
  while (tpl_unpack (tn, 1) > 0) {
    ins_si08 (hash, key, val);
    free (key);
  }
  tpl_free (tn);
}

/* Given a hash and a filename, persist to disk a string key, uint32_t value */
static void
persist_global_si08 (khash_t (si08) *hash, const char *fn) {
  tpl_node *tn;
  khint_t k;
  const char *key = NULL;
  char fmt[] = "A(sv)";
  uint16_t val;

  if (!hash || kh_size (hash) == 0)
    return;

  tn = tpl_map (fmt, &key, &val);
  for (k = 0; k < kh_end (hash); ++k) {
    if (!kh_exist (hash, k) || (!(key = kh_key (hash, k))))
      continue;
    val = kh_value (hash, k);
    tpl_pack (tn, 1);
  }

  close_tpl (tn, fn);
}

/* Given a database filename, restore a string key, uint32_t value back to the
 * storage */
static void
restore_global_si32 (khash_t (si32) *hash, const char *fn) {
  tpl_node *tn;
  char *key = NULL;
  char fmt[] = "A(su)";
  uint32_t val;

  tn = tpl_map (fmt, &key, &val);
  tpl_load (tn, TPL_FILE, fn);
  while (tpl_unpack (tn, 1) > 0) {
    ins_si32 (hash, key, val);
    free (key);
  }
  tpl_free (tn);
}

/* Given a hash and a filename, persist to disk a string key, uint32_t value */
static void
persist_global_si32 (khash_t (si32) *hash, const char *fn) {
  tpl_node *tn;
  khint_t k;
  const char *key = NULL;
  char fmt[] = "A(su)";
  uint32_t val;

  if (!hash || kh_size (hash) == 0)
    return;

  tn = tpl_map (fmt, &key, &val);
  for (k = 0; k < kh_end (hash); ++k) {
    if (!kh_exist (hash, k) || (!(key = kh_key (hash, k))))
      continue;
    val = kh_value (hash, k);
    tpl_pack (tn, 1);
  }

  close_tpl (tn, fn);
}

/* Restore a string key, string value database back to the storage. */
static void
restore_global_ss32 (khash_t (ss32) *hash, const char *fn) {
  tpl_node *tn;
  char *key = NULL, *val = NULL;
  char fmt[] = "A(ss)";
  khint_t k;
  int ret = 0;

  tn = tpl_map (fmt, &key, &val);
  tpl_load (tn, TPL_FILE, fn);
  while (tpl_unpack (tn, 1) > 0) {
    k = kh_put (ss32, hash, key, &ret);
    if (ret > 0) {
      kh_key (hash, k) = xstrdup (key);
      kh_val (hash, k) = xstrdup (val);
    }
    free (key);
    free (val);
  }
  tpl_free (tn);
}

/* Persist to disk a string key, string value hash. */
static void
persist_global_ss32 (khash_t (ss32) *hash, const char *fn) {
  tpl_node *tn;
  khint_t k;
  const char *key = NULL, *val = NULL;
  char fmt[] = "A(ss)";

  if (!hash || kh_size (hash) == 0)
    return;

  tn = tpl_map (fmt, &key, &val);
  for (k = 0; k < kh_end (hash); ++k) {
    if (!kh_exist (hash, k) || !(key = kh_key (hash, k)) || !(val = kh_value (hash, k)))
      continue;
    tpl_pack (tn, 1);
  }

  close_tpl (tn, fn);
}

/* Given a database filename, restore a uint64_t key, GLastParse value back to
 * the storage */
static void
restore_global_iglp (khash_t (iglp) *hash, const char *fn) {
  tpl_node *tn;
  uint64_t key;
  GLastParse val = { 0 };
  char fmt[] = "A(US(uIUvc#))";

  tn = tpl_map (fmt, &key, &val, READ_BYTES);
  tpl_load (tn, TPL_FILE, fn);
  while (tpl_unpack (tn, 1) > 0) {
    ins_iglp (hash, key, &val);
  }
  tpl_free (tn);
}

/* Given a hash and a filename, persist to disk a uint64_t key, uint32_t value */
static void
persist_global_iglp (khash_t (iglp) *hash, const char *fn) {
  tpl_node *tn;
  khint_t k;
  uint64_t key;
  GLastParse val = { 0 };
  char fmt[] = "A(US(uIUvc#))";

  if (!hash || kh_size (hash) == 0)
    return;

  tn = tpl_map (fmt, &key, &val, READ_BYTES);
  for (k = 0; k < kh_end (hash); ++k) {
    if (!kh_exist (hash, k))
      continue;
    key = kh_key (hash, k);
    val = kh_value (hash, k);
    tpl_pack (tn, 1);
  }

  close_tpl (tn, fn);
}

/* Given a filename, ensure we have a valid return path
 *
 * On error, NULL is returned.
 * On success, the valid path is returned */
static char *
check_restore_path (const char *fn) {
  char *path = set_db_path (fn);
  if (access (path, F_OK) != -1)
    return path;

  LOG_DEBUG (("DB file %s doesn't exist. %s\n", path, strerror (errno)));
  free (path);
  return NULL;
}

static char *
build_filename (const char *type, const char *modstr, const char *mtrstr) {
  char *fn = xmalloc (snprintf (NULL, 0, "%s_%s_%s.db", type, modstr, mtrstr) + 1);
  sprintf (fn, "%s_%s_%s.db", type, modstr, mtrstr);
  return fn;
}

/* Get the database filename given a module and a metric.
 *
 * On error, a fatal error is triggered.
 * On success, the filename is returned */
static char *
get_filename (GModule module, GKHashMetric mtrc) {
  const char *mtrstr, *modstr, *type;
  char *fn = NULL;

  if (!(mtrstr = get_mtr_str (mtrc.metric.storem)))
    FATAL ("Unable to allocate metric name.");
  if (!(modstr = get_module_str (module)))
    FATAL ("Unable to allocate module name.");
  if (!(type = get_mtr_type_str (mtrc.type)))
    FATAL ("Unable to allocate module name.");

  fn = build_filename (type, modstr, mtrstr);

  return fn;
}

/* Check if the given date can be inserted based on how many dates we need to
 * keep conf.keep_last.
 *
 * Returns -1 if it fails to insert the date.
 * Returns 1 if the date exists.
 * Returns 2 if the date shouldn't be inserted.
 * On success or if the date is inserted 0 is returned */
static int
insert_restored_date (uint32_t date) {
  uint32_t i, len = 0;

  /* no keep last, simply insert the restored date to our storage */
  if (!conf.keep_last || persisted_dates_len < conf.keep_last)
    return ht_insert_date (date);

  len = MIN (persisted_dates_len, conf.keep_last);
  for (i = 0; i < len; ++i)
    if (persisted_dates[i] == date)
      return ht_insert_date (date);
  return 2;
}

/* Given a database filename, restore a string key, uint32_t value back to
 * the storage */
static int
restore_si32 (GSMetric metric, const char *path, int module) {
  khash_t (si32) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(su))";
  int date = 0, ret = 0;
  char *key = NULL;
  uint32_t val = 0;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    if ((ret = insert_restored_date (date)) == 2)
      continue;
    if (ret == -1 || !(hash = get_hash (module, date, metric)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      ins_si32 (hash, key, val);
      free (key);
    }
  }
  tpl_free (tn);

  return 0;
}

/* Given a database filename, restore a string key, uint32_t value back to
 * the storage */
static int
migrate_si32_to_ii32 (GSMetric metric, const char *path, int module) {
  khash_t (ii32) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(su))";
  int date = 0, ret = 0;
  char *key = NULL;
  uint32_t val = 0;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    if ((ret = insert_restored_date (date)) == 2)
      continue;
    if (ret == -1 || !(hash = get_hash (module, date, metric)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      ins_ii32 (hash, djb2 ((const unsigned char *) key), val);
      free (key);
    }
  }
  tpl_free (tn);

  return 0;
}

/* Parse a legacy "DATE|HOST|AGENT" unique visitor key into a fingerprint.
 * v1 keys carry the raw user agent while v2 keys carry the agent hash as a
 * hex string.
 *
 * On error, 0 is returned.
 * On success, the visitor fingerprint is returned. */
static uint64_t
legacy_uniq_key_fingerprint (const char *key, uint32_t dbver) {
  const char *host = NULL, *agent = NULL;
  char *hostcp = NULL;
  size_t len = 0;
  uint32_t agent_hash = 0;
  uint64_t fp = 0;

  if (!key || !(host = strchr (key, '|')) || !(agent = strchr (host + 1, '|')))
    return 0;
  host++;

  if (dbver >= 2)
    agent_hash = (uint32_t) strtoul (agent + 1, NULL, 16);
  else
    agent_hash = djb2 ((const unsigned char *) (agent + 1));

  len = agent - host;
  hostcp = xmalloc (len + 1);
  memcpy (hostcp, host, len);
  hostcp[len] = '\0';

  fp = visitor_fingerprint (hostcp, agent_hash);
  free (hostcp);

  return fp;
}

/* Load the set of VISITORS data keys per date straight from the legacy
 * keymap database, keyed as (date << 32 | data key). Used to tell the data
 * key member of a legacy uniqmap pair from the visitor member.
 *
 * If there is nothing to read, NULL is returned.
 * On success, the newly allocated set is returned. */
static
khash_t (u648) *
load_visitors_data_keys (void) {
  tpl_node *tn = NULL;
  char *fn = NULL, *path = NULL;
  char fmt[] = "A(iA(uu))";
  const char *modstr = NULL;
  int date = 0;
  uint32_t key = 0, val = 0;
  khash_t (u648) * dkeys = NULL;

  if (!(modstr = get_module_str (VISITORS)))
    FATAL ("Unable to allocate module name.");

  fn = build_filename ("II32", modstr, "MTRC_KEYMAP");
  path = check_restore_path (fn);
  free (fn);
  if (!path)
    return NULL;

  if (!(tn = tpl_map (fmt, &date, &key, &val))) {
    free (path);
    return NULL;
  }

  dkeys = kh_init (u648);
  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    while (tpl_unpack (tn, 2) > 0)
      ins_u648 (dkeys, ((uint64_t) date << 32) | val);
  }
  tpl_free (tn);
  free (path);

  return dkeys;
}

/* Determine whether a legacy uniqmap pair member can be a VISITORS data key
 * for the given date, using the restored keymap when available and the
 * sequence bound otherwise.
 *
 * If the member can be a data key, non-zero is returned.
 * Otherwise, 0 is returned. */
static int
is_visitors_dkey (khash_t (u648) *dkeys, uint32_t date, uint32_t member, uint32_t vseq) {
  if (dkeys)
    return kh_get (u648, dkeys, ((uint64_t) date << 32) | member) != kh_end (dkeys);
  return member <= vseq;
}

/* Build the set of counted visitor sequences per date from the legacy
 * VISITORS module uniqmap. Each of its pairs holds one of the date's data
 * keys and a counted visitor sequence; any pair member that cannot be a
 * data key identifies a counted visitor, and when both members could be
 * either, both are marked to stay on the safe side.
 *
 * If there is nothing to read, NULL is returned.
 * On success, a set keyed by (date << 32 | visitor sequence) is returned. */
static
khash_t (u648) *
load_counted_visitors (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * seqs = get_hdb (db, MTRC_SEQS);
  khash_t (u648) * counted = NULL, *dkeys = NULL;
  tpl_node *tn = NULL;
  char *fn = NULL, *path = NULL;
  char fmt[] = "A(iA(Uv))";
  const char *modstr = NULL;
  int date = 0, hi_data = 0, lo_data = 0;
  uint64_t key;
  uint16_t val = 0;
  uint32_t hi = 0, lo = 0, vseq = 0;

  if (!(modstr = get_module_str (VISITORS)))
    FATAL ("Unable to allocate module name.");

  fn = build_filename ("U648", modstr, "MTRC_UNIQMAP");
  path = check_restore_path (fn);
  free (fn);
  if (!path)
    return NULL;

  if (!(tn = tpl_map (fmt, &date, &key, &val))) {
    free (path);
    return NULL;
  }

  vseq = get_si32 (seqs, modstr);
  dkeys = load_visitors_data_keys ();
  counted = kh_init (u648);

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    while (tpl_unpack (tn, 2) > 0) {
      u64decode (key, &hi, &lo);
      hi_data = is_visitors_dkey (dkeys, date, hi, vseq);
      lo_data = is_visitors_dkey (dkeys, date, lo, vseq);

      if (hi_data && !lo_data) {
        ins_u648 (counted, ((uint64_t) date << 32) | lo);
      } else if (lo_data && !hi_data) {
        ins_u648 (counted, ((uint64_t) date << 32) | hi);
      } else {
        /* either member could be the visitor, mark both */
        ins_u648 (counted, ((uint64_t) date << 32) | hi);
        ins_u648 (counted, ((uint64_t) date << 32) | lo);
      }
    }
  }
  tpl_free (tn);
  free (path);

  if (dkeys)
    kh_destroy (u648, dkeys);

  return counted;
}

/* Migrate a legacy string keyed unique visitors database into fingerprint
 * keyed storage, marking visitors as counted according to the legacy
 * VISITORS uniqmap; the per-date visitors counters are rebuilt separately
 * by the VISITORS uniqmap migration.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
static int
migrate_unique_keys (const char *path, uint32_t dbver) {
  khash_t (u6432) * hash = NULL;
  khash_t (u648) * counted = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(su))";
  int date = 0, ret = 0, is_new = 0;
  char *key = NULL;
  khint_t k;
  uint64_t fp = 0;
  uint32_t val = 0, sval = 0;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  counted = load_counted_visitors ();

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    if ((ret = insert_restored_date (date)) == 2)
      continue;
    if (ret == -1 || !(hash = get_hash (-1, date, MTRC_UNIQUE_KEYS)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      if ((fp = legacy_uniq_key_fingerprint (key, dbver)) != 0) {
        sval = val;
        /* conservative: a phantom mark only suppresses a future first
         * count, matching what the legacy encoding would have done */
        if (counted && kh_get (u648, counted, ((uint64_t) date << 32) | val) != kh_end (counted))
          sval |= VISITOR_COUNTED_BIT;

        k = kh_put (u6432, hash, fp, &is_new);
        if (is_new > 0)
          kh_val (hash, k) = sval;
      }
      free (key);
    }
  }
  tpl_free (tn);

  if (counted)
    kh_destroy (u648, counted);

  return 0;
}

/* Migrate a legacy order-normalized uniqmap database into the ordered pair
 * encoding. The legacy encoding put the larger member in the high word, so
 * when both members are plausible for either position, both orientations
 * are kept; this avoids recounting a visitor that was already counted.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
static int
migrate_uniqmap (GModule module, const char *path) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * seqs = get_hdb (db, MTRC_SEQS);
  khash_t (u648) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(Uv))";
  const char *modstr = NULL;
  int date = 0, ret = 0;
  uint64_t key;
  uint16_t val = 0;
  uint32_t hi = 0, lo = 0, mseq = 0, useq = 0;

  if (!(modstr = get_module_str (module)))
    FATAL ("Unable to allocate module name.");

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  mseq = get_si32 (seqs, modstr);
  useq = get_si32 (seqs, "ht_unique_keys");

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    if ((ret = insert_restored_date (date)) == 2)
      continue;
    if (ret == -1 || !(hash = get_hash (module, date, MTRC_UNIQMAP)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      u64decode (key, &hi, &lo);
      /* ordered encoding holds the data key in the high word and the
       * visitor sequence in the low word */
      if (hi <= mseq && lo <= useq)
        ins_u648 (hash, u64encode (hi, lo));
      if (lo <= mseq && hi <= useq && hi != lo)
        ins_u648 (hash, u64encode (lo, hi));
    }
  }
  tpl_free (tn);

  return 0;
}

/* Migrate the legacy VISITORS uniqmap. The reported visitors total is
 * decoupled from the pair set: the per-date counters take the exact legacy
 * pair count, while the pair set itself (kept only under an hour/minute
 * date specificity) holds every plausible orientation so an already seen
 * pair is never recounted, even when a visitor sequence numerically equals
 * another valid data key.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
static int
migrate_visitors_uniqmap (const char *path) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * seqs = get_hdb (db, MTRC_SEQS);
  khash_t (u648) * hash = NULL;
  khash_t (ii32) * cnt = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(Uv))";
  const char *modstr = NULL;
  int date = 0, ret = 0, keep_pairs = 0;
  uint64_t key;
  uint16_t val = 0;
  uint32_t hi = 0, lo = 0, mseq = 0, useq = 0, entries = 0;

  if (!(modstr = get_module_str (VISITORS)))
    FATAL ("Unable to allocate module name.");

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  mseq = get_si32 (seqs, modstr);
  useq = get_si32 (seqs, "ht_unique_keys");
  keep_pairs = !module_vkey_data (VISITORS);

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    if ((ret = insert_restored_date (date)) == 2)
      continue;
    if (ret == -1 || !(cnt = get_hash (-1, date, MTRC_CNT_VISITORS)))
      break;
    hash = get_hash (VISITORS, date, MTRC_UNIQMAP);

    entries = 0;
    while (tpl_unpack (tn, 2) > 0) {
      entries++;
      if (!keep_pairs || !hash)
        continue;

      u64decode (key, &hi, &lo);
      if (hi <= mseq && lo <= useq)
        ins_u648 (hash, u64encode (hi, lo));
      if (lo <= mseq && hi <= useq && hi != lo)
        ins_u648 (hash, u64encode (lo, hi));
    }

    /* each legacy pair incremented exactly one visitors row, so the exact
     * legacy pair count is the date's visitors total */
    ins_ii32 (cnt, 1, entries);
  }
  tpl_free (tn);

  return 0;
}

/* Given a hash and a filename, persist to disk a string key, uint32_t value */
static int
persist_si32 (GSMetric metric, const char *path, int module) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);

  khash_t (si32) * hash = NULL;
  tpl_node *tn = NULL;
  int date = 0;
  char fmt[] = "A(iA(su))";
  uint32_t val = 0;
  const char *key = NULL;

  if (!dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (dates, date, {
    if (!(hash = get_hash (module, date, metric)))
      return -1;
    kh_foreach (hash, key, val, { tpl_pack (tn, 2); });
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */
  close_tpl (tn, path);

  return 0;
}

/* Field width classes for projecting the merged metrics table onto the
 * legacy per-metric database files. */
enum {
  IMTV_FIELD_U32,
  IMTV_FIELD_U64,
  IMTV_FIELD_U08,
};

/* *INDENT-OFF* */
/* Legacy per-metric database files backing the merged MTRC_METRICS table.
 * Keeping this projection preserves on-disk compatibility with databases
 * persisted before the per-metric tables were merged. */
static const struct {
  GSMetric metric;
  const char *type;
  int kind;
} imtv_fields[] = {
  { MTRC_HITS      , "II32" , IMTV_FIELD_U32 } ,
  { MTRC_VISITORS  , "II32" , IMTV_FIELD_U32 } ,
  { MTRC_ROOT      , "II32" , IMTV_FIELD_U32 } ,
  { MTRC_BW        , "IU64" , IMTV_FIELD_U64 } ,
  { MTRC_CUMTS     , "IU64" , IMTV_FIELD_U64 } ,
  { MTRC_MAXTS     , "IU64" , IMTV_FIELD_U64 } ,
  { MTRC_METHODS   , "II08" , IMTV_FIELD_U08 } ,
  { MTRC_PROTOCOLS , "II08" , IMTV_FIELD_U08 } ,
};
/* *INDENT-ON* */

/* Extract the uint32_t field for the given metric from a merged entry.
 *
 * If the field is present, non-zero is returned and the value is set.
 * If the field is absent, 0 is returned. */
static int
imtv_u32_field (const GKMetricVals *mv, GSMetric metric, uint32_t *val) {
  switch (metric) {
  case MTRC_HITS:
    *val = mv->hits;
    break;
  case MTRC_VISITORS:
    *val = mv->visitors;
    break;
  case MTRC_ROOT:
    *val = mv->root;
    break;
  default:
    return 0;
  }
  return *val != 0;
}

/* Extract the uint64_t field for the given metric from a merged entry.
 * Bandwidth and cumulative time can hold a present zero value, hence their
 * presence is carried by the touched bits.
 *
 * If the field is present, non-zero is returned and the value is set.
 * If the field is absent, 0 is returned. */
static int
imtv_u64_field (const GKMetricVals *mv, GSMetric metric, uint64_t *val) {
  switch (metric) {
  case MTRC_BW:
    *val = mv->bw;
    return (mv->touched & METRIC_TOUCHED_BW) != 0;
  case MTRC_CUMTS:
    *val = mv->cumts;
    return (mv->touched & METRIC_TOUCHED_CUMTS) != 0;
  case MTRC_MAXTS:
    *val = mv->maxts;
    return mv->maxts != 0;
  default:
    return 0;
  }
}

/* Extract the uint8_t field for the given metric from a merged entry.
 *
 * If the field is present, non-zero is returned and the value is set.
 * If the field is absent, 0 is returned. */
static int
imtv_u08_field (const GKMetricVals *mv, GSMetric metric, uint16_t *val) {
  switch (metric) {
  case MTRC_METHODS:
    *val = mv->meth;
    break;
  case MTRC_PROTOCOLS:
    *val = mv->proto;
    break;
  default:
    return 0;
  }
  return *val != 0;
}

/* Set the uint32_t field for the given metric on a merged entry. */
static void
imtv_set_u32_field (GKMetricVals *mv, GSMetric metric, uint32_t val) {
  switch (metric) {
  case MTRC_HITS:
    mv->hits = val;
    break;
  case MTRC_VISITORS:
    mv->visitors = val;
    break;
  case MTRC_ROOT:
    mv->root = val;
    break;
  default:
    break;
  }
}

/* Set the uint64_t field for the given metric on a merged entry. */
static void
imtv_set_u64_field (GKMetricVals *mv, GSMetric metric, uint64_t val) {
  switch (metric) {
  case MTRC_BW:
    mv->bw = val;
    mv->touched |= METRIC_TOUCHED_BW;
    break;
  case MTRC_CUMTS:
    mv->cumts = val;
    mv->touched |= METRIC_TOUCHED_CUMTS;
    break;
  case MTRC_MAXTS:
    mv->maxts = val;
    break;
  default:
    break;
  }
}

/* Set the uint8_t field for the given metric on a merged entry. */
static void
imtv_set_u08_field (GKMetricVals *mv, GSMetric metric, uint8_t val) {
  switch (metric) {
  case MTRC_METHODS:
    mv->meth = val;
    break;
  case MTRC_PROTOCOLS:
    mv->proto = val;
    break;
  default:
    break;
  }
}

/* Given a database filename, restore a uint32_t key, string value back to
 * the merged metrics storage */
static int
migrate_is32_to_ii08 (GSMetric metric, const char *path, int module) {
  khash_t (imtv) * hash = NULL;
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si08) * mtpr = get_hdb (db, MTRC_METH_PROTO);
  GKMetricVals *mv = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(us))";
  int date = 0, ret = 0;
  uint32_t key = 0;
  char *val = NULL;
  khint_t k;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    if ((ret = insert_restored_date (date)) == 2)
      continue;
    if (ret == -1 || !(hash = get_hash (module, date, MTRC_METRICS)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      k = kh_get (si08, mtpr, val);
      /* key found, return current value */
      if (k == kh_end (mtpr)) {
        free (val);
        continue;
      }
      if ((mv = ins_imtv (hash, key)))
        imtv_set_u08_field (mv, metric, kh_val (mtpr, k));
      free (val);
    }
  }
  tpl_free (tn);

  return 0;
}

/* Given a database filename, restore a uint32_t key, string value back to
 * the storage */
static int
restore_is32 (GSMetric metric, const char *path, int module) {
  khash_t (is32) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(us))";
  int date = 0, ret = 0;
  uint32_t key = 0;
  char *val = NULL, *dupval = NULL;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    if ((ret = insert_restored_date (date)) == 2)
      continue;
    if (ret == -1 || !(hash = get_hash (module, date, metric)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      dupval = xstrdup (val);
      if (ins_is32 (hash, key, dupval) != 0)
        free (dupval);
      free (val);
    }
  }
  tpl_free (tn);

  return 0;
}

/* Given a hash and a filename, persist to disk a uint32_t key, string value */
static int
persist_is32 (GSMetric metric, const char *path, int module) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (is32) * hash = NULL;
  tpl_node *tn = NULL;
  int date = 0;
  char fmt[] = "A(iA(us))";
  char *val = NULL;
  uint32_t key = 0;

  if (!dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (dates, date, {
    if (!(hash = get_hash (module, date, metric)))
      return -1;
    kh_foreach (hash, key, val, { tpl_pack (tn, 2); });
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */
  close_tpl (tn, path);

  return 0;
}


/* Given a database filename, restore a uint32_t key, uint32_t value back to
 * the storage */
static int
restore_ii32 (GSMetric metric, const char *path, int module) {
  khash_t (ii32) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(uu))";
  int date = 0, ret = 0;
  uint32_t key = 0, val = 0;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    if ((ret = insert_restored_date (date)) == 2)
      continue;
    if (ret == -1 || !(hash = get_hash (module, date, metric)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      ins_ii32 (hash, key, val);
    }
  }
  tpl_free (tn);

  return 0;
}

/* Given a hash and a filename, persist to disk a uint32_t key, uint32_t value */
static int
persist_ii32 (GSMetric metric, const char *path, int module) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (ii32) * hash = NULL;
  tpl_node *tn = NULL;
  int date = 0;
  char fmt[] = "A(iA(uu))";
  uint32_t key = 0, val = 0;

  if (!dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (dates, date, {
    if (!(hash = get_hash (module, date, metric)))
      return -1;
    kh_foreach (hash, key, val, { tpl_pack (tn, 2); });
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */
  close_tpl (tn, path);

  return 0;
}

/* Given a database filename, restore a uint64_t key set back to the storage.
 * The on-disk value byte is legacy filler and is ignored. */
static int
restore_u648 (GSMetric metric, const char *path, int module) {
  khash_t (u648) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(Uv))";
  int date = 0, ret = 0;
  uint64_t key;
  uint16_t val = 0;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    if ((ret = insert_restored_date (date)) == 2)
      continue;
    if (ret == -1 || !(hash = get_hash (module, date, metric)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      ins_u648 (hash, key);
    }
  }
  tpl_free (tn);

  return 0;
}

/* Given a hash and a filename, persist to disk a uint64_t key set. A constant
 * value byte of 1 is written to keep the legacy on-disk format. */
static int
persist_u648 (GSMetric metric, const char *path, int module) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (u648) * hash = NULL;
  tpl_node *tn = NULL;
  khint_t k;
  int date = 0;
  uint32_t entries = 0;
  char fmt[] = "A(iA(Uv))";
  uint64_t key;
  uint16_t val = 1;

  if (!dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (dates, date, {
    if (!(hash = get_hash (module, date, metric)))
      return -1;
    for (k = kh_begin (hash); k != kh_end (hash); ++k) {
      if (!kh_exist (hash, k))
        continue;
      key = kh_key (hash, k);
      entries++;
      tpl_pack (tn, 2);
    }
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */

  /* modules that count visitors off the visitor key hold no uniqmap entries;
   * skip writing an empty database file for them */
  if (entries == 0) {
    tpl_free (tn);
    return 0;
  }
  close_tpl (tn, path);

  return 0;
}

/* Given a legacy database filename, restore uint32_t key/value pairs back
 * into the corresponding merged metrics field.
 *
 * On error, 1 is returned.
 * On success, 0 is returned */
static int
restore_imtv_u32 (GSMetric metric, const char *path, int module) {
  khash_t (imtv) * hash = NULL;
  GKMetricVals *mv = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(uu))";
  int date = 0, ret = 0;
  uint32_t key = 0, val = 0;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    if ((ret = insert_restored_date (date)) == 2)
      continue;
    if (ret == -1 || !(hash = get_hash (module, date, MTRC_METRICS)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      if ((mv = ins_imtv (hash, key)))
        imtv_set_u32_field (mv, metric, val);
    }
  }
  tpl_free (tn);

  return 0;
}

/* Given a legacy database filename, restore uint32_t key, uint64_t value
 * pairs back into the corresponding merged metrics field.
 *
 * On error, 1 is returned.
 * On success, 0 is returned */
static int
restore_imtv_u64 (GSMetric metric, const char *path, int module) {
  khash_t (imtv) * hash = NULL;
  GKMetricVals *mv = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(uU))";
  int date = 0, ret = 0;
  uint32_t key = 0;
  uint64_t val = 0;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    if ((ret = insert_restored_date (date)) == 2)
      continue;
    if (ret == -1 || !(hash = get_hash (module, date, MTRC_METRICS)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      if ((mv = ins_imtv (hash, key)))
        imtv_set_u64_field (mv, metric, val);
    }
  }
  tpl_free (tn);

  return 0;
}

/* Given a legacy database filename, restore uint32_t key, uint8_t value
 * pairs back into the corresponding merged metrics field.
 *
 * On error, 1 is returned.
 * On success, 0 is returned */
static int
restore_imtv_u08 (GSMetric metric, const char *path, int module) {
  khash_t (imtv) * hash = NULL;
  GKMetricVals *mv = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(uv))";
  int date = 0, ret = 0;
  uint32_t key = 0;
  uint16_t val = 0;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    if ((ret = insert_restored_date (date)) == 2)
      continue;
    if (ret == -1 || !(hash = get_hash (module, date, MTRC_METRICS)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      if ((mv = ins_imtv (hash, key)))
        imtv_set_u08_field (mv, metric, val);
    }
  }
  tpl_free (tn);

  return 0;
}

/* Persist a uint32_t field of the merged metrics table in the legacy
 * per-metric file format.
 *
 * On error, 1 or -1 is returned.
 * On success, 0 is returned */
static int
persist_imtv_u32 (GSMetric metric, const char *path, int module) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (imtv) * hash = NULL;
  tpl_node *tn = NULL;
  khint_t k;
  int date = 0;
  char fmt[] = "A(iA(uu))";
  uint32_t key = 0, val = 0;

  if (!dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (dates, date, {
    if (!(hash = get_hash (module, date, MTRC_METRICS)))
      return -1;
    for (k = kh_begin (hash); k != kh_end (hash); ++k) {
      if (!kh_exist (hash, k) || !imtv_u32_field (&kh_val (hash, k), metric, &val))
        continue;
      key = kh_key (hash, k);
      tpl_pack (tn, 2);
    }
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */
  close_tpl (tn, path);

  return 0;
}

/* Persist a uint64_t field of the merged metrics table in the legacy
 * per-metric file format.
 *
 * On error, 1 or -1 is returned.
 * On success, 0 is returned */
static int
persist_imtv_u64 (GSMetric metric, const char *path, int module) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (imtv) * hash = NULL;
  tpl_node *tn = NULL;
  khint_t k;
  int date = 0;
  char fmt[] = "A(iA(uU))";
  uint32_t key = 0;
  uint64_t val = 0;

  if (!dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (dates, date, {
    if (!(hash = get_hash (module, date, MTRC_METRICS)))
      return -1;
    for (k = kh_begin (hash); k != kh_end (hash); ++k) {
      if (!kh_exist (hash, k) || !imtv_u64_field (&kh_val (hash, k), metric, &val))
        continue;
      key = kh_key (hash, k);
      tpl_pack (tn, 2);
    }
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */
  close_tpl (tn, path);

  return 0;
}

/* Persist a uint8_t field of the merged metrics table in the legacy
 * per-metric file format.
 *
 * On error, 1 or -1 is returned.
 * On success, 0 is returned */
static int
persist_imtv_u08 (GSMetric metric, const char *path, int module) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (imtv) * hash = NULL;
  tpl_node *tn = NULL;
  khint_t k;
  int date = 0;
  char fmt[] = "A(iA(uv))";
  uint32_t key = 0;
  uint16_t val = 0;

  if (!dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (dates, date, {
    if (!(hash = get_hash (module, date, MTRC_METRICS)))
      return -1;
    for (k = kh_begin (hash); k != kh_end (hash); ++k) {
      if (!kh_exist (hash, k) || !imtv_u08_field (&kh_val (hash, k), metric, &val))
        continue;
      key = kh_key (hash, k);
      tpl_pack (tn, 2);
    }
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */
  close_tpl (tn, path);

  return 0;
}

/* Restore the merged metrics table from the legacy per-metric database
 * files. */
static void
restore_imtv (GModule module) {
  const char *modstr = NULL, *mtrstr = NULL;
  char *fn = NULL, *path = NULL;
  size_t i = 0;

  if (!(modstr = get_module_str (module)))
    FATAL ("Unable to allocate module name.");

  for (i = 0; i < ARRAY_SIZE (imtv_fields); ++i) {
    if (!(mtrstr = get_mtr_str (imtv_fields[i].metric)))
      FATAL ("Unable to allocate metric name.");

    fn = build_filename (imtv_fields[i].type, modstr, mtrstr);
    path = check_restore_path (fn);
    free (fn);
    if (!path)
      continue;

    switch (imtv_fields[i].kind) {
    case IMTV_FIELD_U32:
      restore_imtv_u32 (imtv_fields[i].metric, path, module);
      break;
    case IMTV_FIELD_U64:
      restore_imtv_u64 (imtv_fields[i].metric, path, module);
      break;
    case IMTV_FIELD_U08:
      restore_imtv_u08 (imtv_fields[i].metric, path, module);
      break;
    }
    free (path);
  }
}

/* Persist the merged metrics table as the legacy per-metric database
 * files. */
static void
persist_imtv (GModule module) {
  const char *modstr = NULL, *mtrstr = NULL;
  char *fn = NULL, *path = NULL;
  size_t i = 0;
  int ret = 0;

  if (!(modstr = get_module_str (module)))
    FATAL ("Unable to allocate module name.");

  for (i = 0; i < ARRAY_SIZE (imtv_fields); ++i) {
    if (!(mtrstr = get_mtr_str (imtv_fields[i].metric)))
      FATAL ("Unable to allocate metric name.");

    fn = build_filename (imtv_fields[i].type, modstr, mtrstr);
    path = set_db_path (fn);
    free (fn);

    switch (imtv_fields[i].kind) {
    case IMTV_FIELD_U32:
      ret = persist_imtv_u32 (imtv_fields[i].metric, path, module);
      break;
    case IMTV_FIELD_U64:
      ret = persist_imtv_u64 (imtv_fields[i].metric, path, module);
      break;
    case IMTV_FIELD_U08:
      ret = persist_imtv_u08 (imtv_fields[i].metric, path, module);
      break;
    }

    if (ret != 0)
      persist_error = 1;
    free (path);
  }
}

/* Given a database filename, restore a uint64_t key, uint32_t value back to
 * the storage */
static int
restore_u6432 (GSMetric metric, const char *path, int module) {
  khash_t (u6432) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(Uu))";
  int date = 0, ret = 0;
  uint64_t key;
  uint32_t val = 0;
  khint_t k;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    if ((ret = insert_restored_date (date)) == 2)
      continue;
    if (ret == -1 || !(hash = get_hash (module, date, metric)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      k = kh_put (u6432, hash, key, &ret);
      if (ret > 0)
        kh_val (hash, k) = val;
    }
  }
  tpl_free (tn);

  return 0;
}

/* Given a hash and a filename, persist to disk a uint64_t key, uint32_t
 * value */
static int
persist_u6432 (GSMetric metric, const char *path, int module) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (u6432) * hash = NULL;
  tpl_node *tn = NULL;
  khint_t k;
  int date = 0;
  char fmt[] = "A(iA(Uu))";
  uint64_t key;
  uint32_t val = 0;

  if (!dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (dates, date, {
    if (!(hash = get_hash (module, date, metric)))
      return -1;
    for (k = kh_begin (hash); k != kh_end (hash); ++k) {
      if (!kh_exist (hash, k))
        continue;
      key = kh_key (hash, k);
      val = kh_val (hash, k);
      tpl_pack (tn, 2);
    }
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */
  close_tpl (tn, path);

  return 0;
}

/* Given a database filename, restore a uint32_t key, uint64_t value back to
 * the storage */
static int
restore_iu64 (GSMetric metric, const char *path, int module) {
  khash_t (iu64) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(uU))";
  int date = 0, ret = 0;
  uint32_t key;
  uint64_t val;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    if ((ret = insert_restored_date (date)) == 2)
      continue;
    if (ret == -1 || !(hash = get_hash (module, date, metric)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      ins_iu64 (hash, key, val);
    }
  }
  tpl_free (tn);

  return 0;
}

/* Given a hash and a filename, persist to disk a uint32_t key, uint64_t value */
static int
persist_iu64 (GSMetric metric, const char *path, int module) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (iu64) * hash = NULL;
  tpl_node *tn = NULL;
  int date = 0;
  char fmt[] = "A(iA(uU))";
  uint32_t key;
  uint64_t val;

  if (!dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (dates, date, {
    if (!(hash = get_hash (module, date, metric)))
      return -1;
    kh_foreach (hash, key, val, { tpl_pack (tn, 2); });
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */
  close_tpl (tn, path);

  return 0;
}

/* Given a database filename, restore a string key, uint64_t value back to
 * the storage */
static int
restore_su64 (GSMetric metric, const char *path, int module) {
  khash_t (su64) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(sU))";
  int date = 0, ret = 0;
  char *key = NULL;
  uint64_t val;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    if ((ret = insert_restored_date (date)) == 2)
      continue;
    if (ret == -1 || !(hash = get_hash (module, date, metric)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      ins_su64 (hash, key, val);
      free (key);
    }
  }
  tpl_free (tn);

  return 0;
}

/* Given a hash and a filename, persist to disk a string key, uint64_t value */
static int
persist_su64 (GSMetric metric, const char *path, int module) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (su64) * hash = NULL;
  tpl_node *tn = NULL;
  int date = 0;
  char fmt[] = "A(iA(sU))";
  const char *key = NULL;
  uint64_t val;

  if (!dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (dates, date, {
    if (!(hash = get_hash (module, date, metric)))
      return -1;
    kh_foreach (hash, key, val, { tpl_pack (tn, 2); });
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */
  close_tpl (tn, path);

  return 0;
}

/* Given a database filename, restore a uint32_t key, GSLList value back to the
 * storage */
static int
restore_igsl (GSMetric metric, const char *path, int module) {
  khash_t (igsl) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(uu))";
  int date = 0, ret = 0;
  uint32_t key, val;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    if ((ret = insert_restored_date (date)) == 2)
      continue;
    if (ret == -1 || !(hash = get_hash (module, date, metric)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      ins_igsl (hash, key, val);
    }
  }
  tpl_free (tn);

  return 0;
}

/* Given a hash and a filename, persist to disk a uint32_t key, GSLList value */
static int
persist_igsl (GSMetric metric, const char *path, int module) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (igsl) * hash = NULL;
  GSLList *node;
  tpl_node *tn = NULL;
  int date = 0;
  char fmt[] = "A(iA(uu))";
  uint32_t key, val;

  if (!dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (dates, date, {
    if (!(hash = get_hash (module, date, metric)))
      return -1;
    kh_foreach (hash, key, node, {
      while (node) {
        val = (*(uint32_t *) node->data);
        node = node->next;
      }
      tpl_pack (tn, 2);
    });
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */
  close_tpl (tn, path);

  return 0;
}

/* Entry function to restore hash data by type */
static void
restore_by_type (GKHashMetric mtrc, const char *fn, int module) {
  char *path = NULL;

  if (!(path = check_restore_path (fn)))
    goto clean;

  switch (mtrc.type) {
  case MTRC_TYPE_SI32:
    restore_si32 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_IS32:
    restore_is32 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_II32:
    restore_ii32 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_U648:
    restore_u648 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_U6432:
    restore_u6432 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_IU64:
    restore_iu64 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_SU64:
    restore_su64 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_IGSL:
    restore_igsl (mtrc.metric.storem, path, module);
    break;
  default:
    break;
  }
clean:
  free (path);
}

/* Entry function to restore hash data by metric type */
static void
restore_metric_type (GModule module, GKHashMetric mtrc) {
  char *fn = NULL;

  /* the merged metrics table is backed by multiple legacy files */
  if (mtrc.type == MTRC_TYPE_IMTV) {
    restore_imtv (module);
    return;
  }

  fn = get_filename (module, mtrc);
  restore_by_type (mtrc, fn, module);
  free (fn);
}

/* Migrate legacy v1 method/protocol databases into the merged metrics
 * table.
 *
 * On success, the number of migrated databases is returned.
 * On failure or nothing to migrate, 0 is returned. */
static int
migrate_imtv (GModule module) {
  static const GSMetric metrics[] = { MTRC_METHODS, MTRC_PROTOCOLS };
  const char *modstr = NULL, *mtrstr = NULL;
  char *fn = NULL, *path = NULL;
  size_t i = 0;
  int ret = 0;

  if (!(modstr = get_module_str (module)))
    FATAL ("Unable to allocate module name.");

  for (i = 0; i < ARRAY_SIZE (metrics); ++i) {
    if (!(mtrstr = get_mtr_str (metrics[i])))
      FATAL ("Unable to allocate metric name.");

    fn = build_filename ("IS32", modstr, mtrstr);
    path = check_restore_path (fn);
    free (fn);
    if (!path)
      continue;

    if (migrate_is32_to_ii08 (metrics[i], path, module) == 0) {
      defer_migrated_unlink (path);
      ret++;
    }
    free (path);
  }

  return ret;
}

/* Migrate legacy databases for the given metric into the current storage
 * format. Consumed legacy files are queued for removal but kept on disk
 * until the migrated data has been persisted.
 *
 * Sets skip_restore when the migration replaces the regular restore for
 * this metric.
 * Returns the number of migrated databases. */
static int
migrate_metric (GModule module, GKHashMetric mtrc, int *skip_restore) {
  uint32_t dbver = get_db_version ();
  int ret = 0;
  char *fn = NULL, *path = NULL;
  const char *modstr;

  *skip_restore = 0;
  /* db is up-to-date, thus no need to migrate anything */
  if (dbver == DB_VERSION)
    return 0;

  switch (mtrc.metric.storem) {
  case MTRC_UNIQUE_KEYS:
    if (!(path = check_restore_path ("SI32_UNIQUE_KEYS.db")))
      break;
    if (migrate_unique_keys (path, dbver) != 0)
      break;
    /* the migration loaded the fingerprint table directly; skip the regular
     * restore for it */
    *skip_restore = 1;
    defer_migrated_unlink (path);
    ret++;
    break;
  case MTRC_KEYMAP:
    if (dbver >= 2)
      break;
    if (!(modstr = get_module_str (module)))
      FATAL ("Unable to allocate module name.");
    fn = build_filename ("SI32", modstr, "MTRC_KEYMAP");
    if (!(path = check_restore_path (fn)))
      break;
    if (migrate_si32_to_ii32 (mtrc.metric.storem, path, module) != 0)
      break;
    defer_migrated_unlink (path);
    ret++;
    break;
  case MTRC_METRICS:
    if (dbver >= 2)
      break;
    ret = migrate_imtv (module);
    break;
  case MTRC_AGENT_KEYS:
    if (dbver >= 2)
      break;
    if (!(path = check_restore_path ("SI32_AGENT_KEYS.db")))
      break;
    if (migrate_si32_to_ii32 (mtrc.metric.storem, path, -1) != 0)
      break;
    defer_migrated_unlink (path);
    ret++;
    break;
  case MTRC_UNIQMAP:
    if (dbver < 2)
      break;
    if (!(modstr = get_module_str (module)))
      FATAL ("Unable to allocate module name.");
    fn = build_filename ("U648", modstr, "MTRC_UNIQMAP");
    if (!(path = check_restore_path (fn)))
      break;

    /* the legacy file uses the normalized pair encoding either way; never
     * restore it directly */
    *skip_restore = 1;
    if (module == VISITORS) {
      if (migrate_visitors_uniqmap (path) != 0)
        break;
      /* the file name is only retired when the panel counts visitors off
       * the visitor key; under an hour/minute date specificity the persist
       * rewrites it in place */
      if (module_vkey_data (VISITORS))
        defer_migrated_unlink (path);
      ret++;
      break;
    }
    if (module_vkey_data (module)) {
      /* visitor counting for this module now keys off the visitor key; its
       * legacy uniqmap is dropped */
      defer_migrated_unlink (path);
      ret++;
      break;
    }
    /* the file name is reused by the current format; the migration persist
     * rewrites it in place, so it must not be queued for deletion */
    if (migrate_uniqmap (module, path) != 0)
      break;
    ret++;
    break;
  default:
    break;
  }

  free (fn);
  free (path);

  return ret;
}

static void
persist_by_type (GKHashMetric mtrc, const char *fn, int module) {
  char *path = NULL;
  int ret = 0;
  path = set_db_path (fn);

  switch (mtrc.type) {
  case MTRC_TYPE_SI32:
    ret = persist_si32 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_IS32:
    ret = persist_is32 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_II32:
    ret = persist_ii32 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_U648:
    ret = persist_u648 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_U6432:
    ret = persist_u6432 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_IU64:
    ret = persist_iu64 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_SU64:
    ret = persist_su64 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_IGSL:
    ret = persist_igsl (mtrc.metric.storem, path, module);
    break;
  default:
    break;
  }

  if (ret != 0)
    persist_error = 1;
  free (path);
}

static void
persist_metric_type (GModule module, GKHashMetric mtrc) {
  char *fn = NULL;

  /* the merged metrics table is backed by multiple legacy files */
  if (mtrc.type == MTRC_TYPE_IMTV) {
    persist_imtv (module);
    return;
  }

  fn = get_filename (module, mtrc);
  persist_by_type (mtrc, fn, module);
  free (fn);
}

/* Given all the dates that we have processed, persist to disk a copy of them. */
static void
persist_dates (void) {
  tpl_node *tn;
  char *path = NULL;
  uint32_t *dates = NULL, len = 0, i, date = 0;
  char fmt[] = "A(u)";

  if (!(path = set_db_path ("I32_DATES.db")))
    return;

  dates = get_sorted_dates (&len);

  tn = tpl_map (fmt, &date);
  for (i = 0; i < len; ++i) {
    date = dates[i];
    tpl_pack (tn, 1);
  }
  close_tpl (tn, path);

  free (path);
  free (dates);
}

/* Restore all the processed dates from our last dataset */
static void
restore_dates (void) {
  tpl_node *tn;
  char *path = NULL;
  uint32_t date, idx = 0;
  char fmt[] = "A(u)";
  int len;

  if (!(path = check_restore_path ("I32_DATES.db")))
    return;

  tn = tpl_map (fmt, &date);
  tpl_load (tn, TPL_FILE, path);

  len = tpl_Alen (tn, 1);
  if (len < 0)
    return;
  persisted_dates_len = len;
  persisted_dates = xcalloc (persisted_dates_len, sizeof (uint32_t));
  while (tpl_unpack (tn, 1) > 0)
    persisted_dates[idx++] = date;

  qsort (persisted_dates, idx, sizeof (uint32_t), cmp_ui32_desc);
  tpl_free (tn);
  free (path);
}

/* Entry function to restore a global hashes */
static void
restore_global (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * overall = get_hdb (db, MTRC_CNT_OVERALL);
  khash_t (si32) * seqs = get_hdb (db, MTRC_SEQS);
  khash_t (iglp) * last_parse = get_hdb (db, MTRC_LAST_PARSE);
  khash_t (si32) * db_props = get_hdb (db, MTRC_DB_PROPS);
  khash_t (si08) * meth_proto = get_hdb (db, MTRC_METH_PROTO);

  char *path = NULL;

  if ((path = check_restore_path ("SI32_DB_PROPS.db"))) {
    restore_global_si32 (db_props, path);
    free (path);
  }

  restore_dates ();
  if ((path = check_restore_path ("SI32_CNT_OVERALL.db"))) {
    restore_global_si32 (overall, path);
    free (path);
  }
  if ((path = check_restore_path ("SI32_SEQS.db"))) {
    restore_global_si32 (seqs, path);
    free (path);
  }
  if ((path = check_restore_path ("SI08_METH_PROTO.db"))) {
    restore_global_si08 (meth_proto, path);
    free (path);
  }
  if ((path = check_restore_path ("IGLP_LAST_PARSE.db"))) {
    restore_global_iglp (last_parse, path);
    free (path);
  }
  if ((path = check_restore_path ("SS32_COUNTRY_CONTINENT.db"))) {
    restore_global_ss32 (get_hdb (db, MTRC_COUNTRY_CONTINENT), path);
    free (path);
  }
}

static void
persist_global (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * overall = get_hdb (db, MTRC_CNT_OVERALL);
  khash_t (si32) * seqs = get_hdb (db, MTRC_SEQS);
  khash_t (iglp) * last_parse = get_hdb (db, MTRC_LAST_PARSE);
  khash_t (si08) * meth_proto = get_hdb (db, MTRC_METH_PROTO);
  char *path = NULL;

  persist_dates ();
  if ((path = set_db_path ("SI32_CNT_OVERALL.db"))) {
    persist_global_si32 (overall, path);
    free (path);
  }
  if ((path = set_db_path ("SI32_SEQS.db"))) {
    persist_global_si32 (seqs, path);
    free (path);
  }
  if ((path = set_db_path ("IGLP_LAST_PARSE.db"))) {
    persist_global_iglp (last_parse, path);
    free (path);
  }
  if ((path = set_db_path ("SI08_METH_PROTO.db"))) {
    persist_global_si08 (meth_proto, path);
    free (path);
  }
  if ((path = set_db_path ("SS32_COUNTRY_CONTINENT.db"))) {
    persist_global_ss32 (get_hdb (db, MTRC_COUNTRY_CONTINENT), path);
    free (path);
  }
}

/* Persist the database properties, marking the on-disk dataset as complete
 * for the current version. Must be written only after every other database
 * write has succeeded. */
static void
persist_db_props (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * db_props = get_hdb (db, MTRC_DB_PROPS);
  char *path = NULL;
  khint_t k;
  int ret;

  /* upsert: the restored props may carry an older version value */
  k = kh_put (si32, db_props, "version", &ret);
  if (ret > 0)
    kh_key (db_props, k) = xstrdup ("version");
  kh_val (db_props, k) = DB_VERSION;

  if ((path = set_db_path ("SI32_DB_PROPS.db"))) {
    persist_global_si32 (db_props, path);
    free (path);
  }
}

void
persist_data (void) {
  GModule module;
  int i, n = 0;
  size_t idx = 0;

  persist_error = 0;
  persist_global ();

  n = global_metrics_len;
  for (i = 0; i < n; ++i)
    persist_by_type (global_metrics[i], global_metrics[i].filename, -1);

  n = module_metrics_len;
  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    for (i = 0; i < n; ++i) {
      persist_metric_type (module, module_metrics[i]);
    }
  }

  /* the version metadata is written last so an interrupted or failed persist
   * never marks an incomplete dataset as current */
  if (persist_error == 0)
    persist_db_props ();
  else
    LOG_DEBUG (("Failed to write one or more database files; version metadata withheld\n"));
}

/* Entry function to restore hashes */
void
restore_data (void) {
  int migrated = 0, skip = 0;
  GModule module;
  int i, n = 0;
  size_t idx = 0;

  restore_global ();

  n = global_metrics_len;
  for (i = 0; i < n; ++i) {
    migrated += migrate_metric (-1, global_metrics[i], &skip);
    if (!skip)
      restore_by_type (global_metrics[i], global_metrics[i].filename, -1);
  }

  n = module_metrics_len;
  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    for (i = 0; i < n; ++i) {
      migrated += migrate_metric (module, module_metrics[i], &skip);
      if (!skip)
        restore_metric_type (module, module_metrics[i]);
    }
  }

  if (migrated) {
    /* persist the migrated data in the current format before removing the
     * legacy files, so an interrupted or failed migration simply runs
     * again */
    persist_data ();
    if (persist_error == 0)
      unlink_migrated_files ();
    if (!conf.persist)
      conf.persist = 1;
  }
}

void
free_persisted_data (void) {
  free (persisted_dates);
}
