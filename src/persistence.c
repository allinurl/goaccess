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

  tpl_dump (tn, TPL_FILE, fn);
  tpl_free (tn);
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

  tpl_dump (tn, TPL_FILE, fn);
  tpl_free (tn);
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

  tpl_dump (tn, TPL_FILE, fn);
  tpl_free (tn);
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

/* Dump to disk the database file and frees its memory */
static void
close_tpl (tpl_node *tn, const char *fn) {
  tpl_dump (tn, TPL_FILE, fn);
  tpl_free (tn);
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

static char *
migrate_unique_key (const char *key) {
  char *nkey = NULL, *token = NULL, *ptr = NULL;
  char agent_hex[64] = { 0 };
  uint32_t delims = 0;

  if (!key || count_matches (key, '|') < 2)
    return NULL;

  nkey = xstrdup ("");
  while ((ptr = strchr (key, '|'))) {
    if (!(token = extract_by_delim (&key, "|"))) {
      free (nkey);
      return NULL;
    }

    append_str (&nkey, token);
    append_str (&nkey, "|");
    free (token);
    key++;
    delims++;
  }
  if (delims == 2) {
    sprintf (agent_hex, "%" PRIx32, djb2 ((const unsigned char *) key));
    append_str (&nkey, agent_hex);
  }

  return nkey;
}


/* Given a database filename, restore a string key, uint32_t value back to
 * the storage */
static int
migrate_si32_to_ii32_unique_keys (GSMetric metric, const char *path, int module) {
  khash_t (si32) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(su))";
  int date = 0, ret = 0;
  char *key = NULL, *nkey = NULL;
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
      if ((nkey = migrate_unique_key (key)))
        ins_si32 (hash, nkey, val);
      free (key);
      free (nkey);
    }
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

/* Given a database filename, restore a uint32_t key, string value back to
 * the storage */
static int
migrate_is32_to_ii08 (GSMetric metric, const char *path, int module) {
  khash_t (ii08) * hash = NULL;
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si08) * mtpr = get_hdb (db, MTRC_METH_PROTO);
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
    if (ret == -1 || !(hash = get_hash (module, date, metric)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      k = kh_get (si08, mtpr, val);
      /* key found, return current value */
      if (k == kh_end (mtpr)) {
        free (val);
        continue;
      }
      ins_ii08 (hash, key, kh_val (mtpr, k));
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
restore_ii08 (GSMetric metric, const char *path, int module) {
  khash_t (ii08) * hash = NULL;
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
    if (ret == -1 || !(hash = get_hash (module, date, metric)))
      break;

    while (tpl_unpack (tn, 2) > 0) {
      ins_ii08 (hash, key, val);
    }
  }
  tpl_free (tn);

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

/* Given a hash and a filename, persist to disk a uint32_t key, uint32_t value */
static int
persist_ii08 (GSMetric metric, const char *path, int module) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (ii08) * hash = NULL;
  tpl_node *tn = NULL;
  int date = 0;
  char fmt[] = "A(iA(uv))";
  uint32_t key = 0;
  uint16_t val = 0;

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

/* Given a database filename, restore a uint64_t key, uint8_t value back to
 * the storage */
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
      ins_u648 (hash, key, val);
    }
  }
  tpl_free (tn);

  return 0;
}

/* Given a hash and a filename, persist to disk a uint64_t key, uint8_t value */
static int
persist_u648 (GSMetric metric, const char *path, int module) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (u648) * hash = NULL;
  tpl_node *tn = NULL;
  int date = 0;
  char fmt[] = "A(iA(Uv))";
  uint64_t key;
  uint16_t val = 0;

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
  case MTRC_TYPE_II08:
    restore_ii08 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_II32:
    restore_ii32 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_U648:
    restore_u648 (mtrc.metric.storem, path, module);
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

  fn = get_filename (module, mtrc);
  restore_by_type (mtrc, fn, module);
  free (fn);
}

static int
migrate_metric (GModule module, GKHashMetric mtrc) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * db_props = get_hdb (db, MTRC_DB_PROPS);

  int ret = 0;
  char *fn = NULL, *path = NULL;
  const char *modstr, *mtrstr;
  khint_t k;

  k = kh_get (si32, db_props, "version");
  /* db is up-to-date, thus no need to migrate anything */
  if (k != kh_end (db_props) && kh_val (db_props, k) == DB_VERSION)
    return 0;

  switch (mtrc.metric.storem) {
  case MTRC_UNIQUE_KEYS:
    if (!(path = check_restore_path ("SI32_UNIQUE_KEYS.db")))
      break;
    if (migrate_si32_to_ii32_unique_keys (mtrc.metric.storem, path, -1) != 0)
      break;
    unlink (path);
    ret++;
    break;
  case MTRC_KEYMAP:
    if (!(modstr = get_module_str (module)))
      FATAL ("Unable to allocate module name.");
    fn = build_filename ("SI32", modstr, "MTRC_KEYMAP");
    if (!(path = check_restore_path (fn)))
      break;
    if (migrate_si32_to_ii32 (mtrc.metric.storem, path, module) != 0)
      break;
    unlink (path);
    ret++;
    break;
  case MTRC_METHODS:
  case MTRC_PROTOCOLS:
    if (!(mtrstr = get_mtr_str (mtrc.metric.storem)))
      FATAL ("Unable to allocate metric name.");
    if (!(modstr = get_module_str (module)))
      FATAL ("Unable to allocate module name.");

    fn = build_filename ("IS32", modstr, mtrstr);
    if (!(path = check_restore_path (fn)))
      break;
    if (migrate_is32_to_ii08 (mtrc.metric.storem, path, module) != 0)
      break;
    unlink (path);
    ret++;
    break;
  case MTRC_AGENT_KEYS:
    if (!(path = check_restore_path ("SI32_AGENT_KEYS.db")))
      break;
    if (migrate_si32_to_ii32 (mtrc.metric.storem, path, -1) != 0)
      break;
    unlink (path);
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
  path = set_db_path (fn);

  switch (mtrc.type) {
  case MTRC_TYPE_SI32:
    persist_si32 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_IS32:
    persist_is32 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_II32:
    persist_ii32 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_II08:
    persist_ii08 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_U648:
    persist_u648 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_IU64:
    persist_iu64 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_SU64:
    persist_su64 (mtrc.metric.storem, path, module);
    break;
  case MTRC_TYPE_IGSL:
    persist_igsl (mtrc.metric.storem, path, module);
    break;
  default:
    break;
  }
  free (path);
}

static void
persist_metric_type (GModule module, GKHashMetric mtrc) {
  char *fn = NULL;
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
  tpl_dump (tn, TPL_FILE, path);

  tpl_free (tn);
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
}

static void
persist_global (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * overall = get_hdb (db, MTRC_CNT_OVERALL);
  khash_t (si32) * seqs = get_hdb (db, MTRC_SEQS);
  khash_t (iglp) * last_parse = get_hdb (db, MTRC_LAST_PARSE);
  khash_t (si32) * db_props = get_hdb (db, MTRC_DB_PROPS);
  khash_t (si08) * meth_proto = get_hdb (db, MTRC_METH_PROTO);
  char *path = NULL;

  ins_si32 (db_props, "version", DB_VERSION);

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
}

/* Entry function to restore hashes */
void
restore_data (void) {
  int migrated = 0;
  GModule module;
  int i, n = 0;
  size_t idx = 0;

  restore_global ();

  n = global_metrics_len;
  for (i = 0; i < n; ++i) {
    migrated += migrate_metric (-1, global_metrics[i]);
    restore_by_type (global_metrics[i], global_metrics[i].filename, -1);
  }

  n = module_metrics_len;
  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    for (i = 0; i < n; ++i) {
      migrated += migrate_metric (module, module_metrics[i]);
      restore_metric_type (module, module_metrics[i]);
    }
  }

  if (migrated && !conf.persist)
    conf.persist = 1;
}

void
free_persisted_data (void) {
  free (persisted_dates);
}
