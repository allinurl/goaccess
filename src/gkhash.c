/**
 * gkhash.c -- default hash table functions
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2025 Gerardo Orellana <hello @ goaccess.io>
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "gkhash.h"

#include "error.h"
#include "persistence.h"
#include "sort.h"
#include "util.h"
#include "xmalloc.h"

/* *INDENT-OFF* */
/* Hash table that holds DB instances */
static khash_t (igdb) * ht_db = NULL;
/* *INDENT-ON* */

/* Allocate memory for a new global GKHashDB instance.
 *
 * On success, the newly allocated GKHashDB is returned . */
static GKHashDB *
new_gkhdb (void) {
  GKHashDB *storage = xcalloc (1, sizeof (GKHashDB));
  return storage;
}

/* Allocate memory for a new GKDB instance.
 *
 * On success, the newly allocated GKHashDB is returned . */
static GKDB *
new_gkdb (void) {
  GKDB *db = xcalloc (1, sizeof (GKDB));
  return db;
}

/* Get the module string value given a metric enum value.
 *
 * On error, NULL is returned.
 * On success, the string module value is returned. */
const char *
get_mtr_type_str (GSMetricType type) {
  static const GEnum enum_metric_types[] = {
    {"II32", MTRC_TYPE_II32},
    {"IS32", MTRC_TYPE_IS32},
    {"IU64", MTRC_TYPE_IU64},
    {"SI32", MTRC_TYPE_SI32},
    {"SI08", MTRC_TYPE_SI08},
    {"II08", MTRC_TYPE_II08},
    {"SS32", MTRC_TYPE_SS32},
    {"IGSL", MTRC_TYPE_IGSL},
    {"SU64", MTRC_TYPE_SU64},
    {"IGKH", MTRC_TYPE_IGKH},
    {"U648", MTRC_TYPE_U648},
    {"IGLP", MTRC_TYPE_IGLP},
  };
  return enum2str (enum_metric_types, ARRAY_SIZE (enum_metric_types), type);
}

/* Initialize a new uint32_t key - GSLList value hash table */
void *
new_igsl_ht (void) {
  khash_t (igsl) * h = kh_init (igsl);
  return h;
}

/* Initialize a new string key - uint32_t value hash table */
void *
new_ii08_ht (void) {
  khash_t (ii08) * h = kh_init (ii08);
  return h;
}

/* Initialize a new uint32_t key - uint32_t value hash table */
void *
new_ii32_ht (void) {
  khash_t (ii32) * h = kh_init (ii32);
  return h;
}

/* Initialize a new uint32_t key - string value hash table */
void *
new_is32_ht (void) {
  khash_t (is32) * h = kh_init (is32);
  return h;
}

/* Initialize a new uint32_t key - uint64_t value hash table */
void *
new_iu64_ht (void) {
  khash_t (iu64) * h = kh_init (iu64);
  return h;
}

/* Initialize a new string key - uint32_t value hash table */
void *
new_si32_ht (void) {
  khash_t (si32) * h = kh_init (si32);
  return h;
}

/* Initialize a new string key - uint64_t value hash table */
void *
new_su64_ht (void) {
  khash_t (su64) * h = kh_init (su64);
  return h;
}

/* Initialize a new uint64_t key - uint8_t value hash table */
void *
new_u648_ht (void) {
  khash_t (u648) * h = kh_init (u648);
  return h;
}

/* Initialize a new uint64_t key - GLastParse value hash table */
static void *
new_iglp_ht (void) {
  khash_t (iglp) * h = kh_init (iglp);
  return h;
}

/* Initialize a new uint32_t key - GKHashStorage value hash table */
static void *
new_igdb_ht (void) {
  khash_t (igdb) * h = kh_init (igdb);
  return h;
}

/* Initialize a new uint32_t key - GKHashStorage value hash table */
static void *
new_igkh_ht (void) {
  khash_t (igkh) * h = kh_init (igkh);
  return h;
}

/* Initialize a new string key - uint32_t value hash table */
static void *
new_si08_ht (void) {
  khash_t (si08) * h = kh_init (si08);
  return h;
}

/* Initialize a new string key - string value hash table */
static void *
new_ss32_ht (void) {
  khash_t (ss32) * h = kh_init (ss32);
  return h;
}

/* Deletes all entries from the hash table and optionally frees its GSLList */
void
del_igsl_free (void *h, uint8_t free_data) {
  khint_t k;
  khash_t (igsl) * hash = h;
  void *list = NULL;
  if (!hash)
    return;

  for (k = 0; k < kh_end (hash); ++k) {
    if (!kh_exist (hash, k))
      continue;

    if (free_data) {
      list = kh_value (hash, k);
      list_remove_nodes (list);
    }
    kh_del (igsl, hash, k);
  }
}

/* Deletes all entries from the hash table */
void
del_ii08 (void *h, GO_UNUSED uint8_t free_data) {
  khint_t k;
  khash_t (ii08) * hash = h;
  if (!hash)
    return;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      kh_del (ii08, hash, k);
    }
  }
}

/* Deletes all entries from the hash table */
void
del_ii32 (void *h, GO_UNUSED uint8_t free_data) {
  khint_t k;
  khash_t (ii32) * hash = h;
  if (!hash)
    return;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      kh_del (ii32, hash, k);
    }
  }
}

/* Deletes both the hash entry and its string values */
void
del_is32_free (void *h, uint8_t free_data) {
  khint_t k;
  khash_t (is32) * hash = h;
  if (!hash)
    return;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      if (free_data)
        free ((char *) kh_value (hash, k));
      kh_del (is32, hash, k);
    }
  }
}

/* Deletes all entries from the hash table */
void
del_iu64 (void *h, GO_UNUSED uint8_t free_data) {
  khint_t k;
  khash_t (iu64) * hash = h;
  if (!hash)
    return;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      kh_del (iu64, hash, k);
    }
  }
}

/* Deletes an entry from the hash table and optionally the keys for a string
 * key - uint32_t value hash */
void
del_si32_free (void *h, uint8_t free_data) {
  khint_t k;
  khash_t (si32) * hash = h;
  if (!hash)
    return;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      if (free_data)
        free ((char *) kh_key (hash, k));
      kh_del (si32, hash, k);
    }
  }
}

/* Deletes all entries from the hash table and optionally frees its string key */
void
del_su64_free (void *h, uint8_t free_data) {
  khint_t k;
  khash_t (su64) * hash = h;
  if (!hash)
    return;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      if (free_data)
        free ((char *) kh_key (hash, k));
      kh_del (su64, hash, k);
    }
  }
}

/* Deletes all entries from the hash table */
void
del_u648 (void *h, GO_UNUSED uint8_t free_data) {
  khint_t k;
  khash_t (u648) * hash = h;
  if (!hash)
    return;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      kh_del (u648, hash, k);
    }
  }
}

/* Destroys both the hash structure and its GSLList
 * values */
void
des_igsl_free (void *h, uint8_t free_data) {
  khash_t (igsl) * hash = h;
  khint_t k;
  void *list = NULL;
  if (!hash)
    return;

  if (!free_data)
    goto des;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k) && (list = kh_value (hash, k))) {
      list_remove_nodes (list);
    }
  }
des:
  kh_destroy (igsl, hash);
}

/* Destroys the hash structure */
void
des_ii08 (void *h, GO_UNUSED uint8_t free_data) {
  khash_t (ii08) * hash = h;
  if (!hash)
    return;
  kh_destroy (ii08, hash);
}

/* Destroys the hash structure */
void
des_ii32 (void *h, GO_UNUSED uint8_t free_data) {
  khash_t (ii32) * hash = h;
  if (!hash)
    return;
  kh_destroy (ii32, hash);
}

/* Destroys both the hash structure and its string values */
void
des_is32_free (void *h, uint8_t free_data) {
  khint_t k;
  khash_t (is32) * hash = h;
  if (!hash)
    return;

  if (!free_data)
    goto des;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      free ((char *) kh_value (hash, k));
    }
  }
des:
  kh_destroy (is32, hash);
}

/* Destroys the hash structure */
void
des_iu64 (void *h, GO_UNUSED uint8_t free_data) {
  khash_t (iu64) * hash = h;
  if (!hash)
    return;
  kh_destroy (iu64, hash);
}

/* Destroys both the hash structure and the keys for a
 * string key - uint32_t value hash */
void
des_si32_free (void *h, uint8_t free_data) {
  khint_t k;
  khash_t (si32) * hash = h;
  if (!hash)
    return;

  if (!free_data)
    goto des;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      free ((char *) kh_key (hash, k));
    }
  }

des:
  kh_destroy (si32, hash);
}

/* Destroys both the hash structure and the keys for a
 * string key - uint64_t value hash */
void
des_su64_free (void *h, uint8_t free_data) {
  khash_t (su64) * hash = h;
  khint_t k;
  if (!hash)
    return;

  if (!free_data)
    goto des;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      free ((char *) kh_key (hash, k));
    }
  }

des:
  kh_destroy (su64, hash);
}

/* Destroys the hash structure */
void
des_u648 (void *h, GO_UNUSED uint8_t free_data) {
  khash_t (u648) * hash = h;
  if (!hash)
    return;
  kh_destroy (u648, hash);
}

/* Destroys both the hash structure and the keys for a
 * string key - uint32_t value hash */
static void
des_si08_free (void *h, uint8_t free_data) {
  khint_t k;
  khash_t (si08) * hash = h;
  if (!hash)
    return;

  if (!free_data)
    goto des;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      free ((char *) kh_key (hash, k));
    }
  }

des:
  kh_destroy (si08, hash);
}

/* Deletes an entry from the hash table and optionally the keys for a string
 * key - uint32_t value hash */
static void
del_si08_free (void *h, uint8_t free_data) {
  khint_t k;
  khash_t (si08) * hash = h;
  if (!hash)
    return;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      if (free_data)
        free ((char *) kh_key (hash, k));
      kh_del (si08, hash, k);
    }
  }
}

/* Deletes an entry from the hash table and optionally the keys for a string
 * keys and string values */
static void
del_ss32_free (void *h, uint8_t free_data) {
  khint_t k;
  khash_t (ss32) * hash = h;
  if (!hash)
    return;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      if (free_data) {
        free ((char *) kh_key (hash, k));
        free ((char *) kh_value (hash, k));
      }
      kh_del (ss32, hash, k);
    }
  }
}

/* Destroys both the hash structure and its string
 * keys and string values */
static void
des_ss32_free (void *h, uint8_t free_data) {
  khint_t k;
  khash_t (ss32) * hash = h;
  if (!hash)
    return;

  if (!free_data)
    goto des;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      free ((char *) kh_key (hash, k));
      free ((char *) kh_value (hash, k));
    }
  }

des:
  kh_destroy (ss32, hash);
}

/* Destroys the hash structure */
static void
des_iglp (void *h, GO_UNUSED uint8_t free_data) {
  khash_t (iglp) * hash = h;
  if (!hash)
    return;
  kh_destroy (iglp, hash);
}

/* *INDENT-OFF* */
/* Whole application */
const GKHashMetric app_metrics[] = {
  { .metric.dbm=MTRC_DATES       , MTRC_TYPE_IGKH , new_igkh_ht , NULL          , NULL          , 1 , NULL , NULL                  } ,
  { .metric.dbm=MTRC_SEQS        , MTRC_TYPE_SI32 , new_si32_ht , des_si32_free , del_si32_free , 1 , NULL , "SI32_SEQS.db"        } ,
  { .metric.dbm=MTRC_CNT_OVERALL , MTRC_TYPE_SI32 , new_si32_ht , des_si32_free , del_si32_free , 1 , NULL , "SI32_CNT_OVERALL.db" } ,
  { .metric.dbm=MTRC_HOSTNAMES   , MTRC_TYPE_SS32 , new_ss32_ht , des_ss32_free , del_ss32_free , 1 , NULL , NULL                  } ,
  { .metric.dbm=MTRC_LAST_PARSE  , MTRC_TYPE_IGLP , new_iglp_ht , des_iglp      , NULL          , 1 , NULL , "IGLP_LAST_PARSE.db"  } ,
  { .metric.dbm=MTRC_JSON_LOGFMT , MTRC_TYPE_SS32 , new_ss32_ht , des_ss32_free , del_ss32_free , 1 , NULL , NULL                  } ,
  { .metric.dbm=MTRC_METH_PROTO  , MTRC_TYPE_SI08 , new_si08_ht , des_si08_free , del_si08_free , 1 , NULL , "SI08_METH_PROTO.db"  } ,
  { .metric.dbm=MTRC_DB_PROPS    , MTRC_TYPE_SI32 , new_si32_ht , des_si32_free , del_si32_free , 1 , NULL , "SI32_DB_PROPS.db"    } ,
};

const size_t app_metrics_len = ARRAY_SIZE (app_metrics);
/* *INDENT-ON* */

/* Destroys malloc'd module metrics */
static void
free_app_metrics (GKHashDB *storage) {
  int i, n = 0;
  GKHashMetric mtrc;

  if (!storage)
    return;

  n = app_metrics_len;
  for (i = 0; i < n; i++) {
    mtrc = storage->metrics[i];
    if (mtrc.des) {
      mtrc.des (mtrc.hash, mtrc.free_data);
    }
  }
  free (storage);
}

/* Given a key (date), get the relevant store
 *
 * On error or not found, NULL is returned.
 * On success, a pointer to that store is returned. */
void *
get_db_instance (uint32_t key) {
  GKDB *db = NULL;
  khint_t k;

  khash_t (igdb) * hash = ht_db;

  k = kh_get (igdb, hash, key);
  /* key not found, return NULL */
  if (k == kh_end (hash))
    return NULL;

  db = kh_val (hash, k);
  return db;
}

/* Get an app hash table given a DB instance and a GAMetric
 *
 * On success, a pointer to that store is returned. */
void *
get_hdb (GKDB *db, GAMetric mtrc) {
  return db->hdb->metrics[mtrc].hash;
}

Logs *
get_db_logs (uint32_t instance) {
  GKDB *db = get_db_instance (instance);
  return db->logs;
}


/* Initialize a global hash structure.
 *
 * On success, a pointer to that hash structure is returned. */
static GKHashDB *
init_gkhashdb (void) {
  GKHashDB *storage = NULL;

  int n = 0, i;

  storage = new_gkhdb ();
  n = app_metrics_len;
  for (i = 0; i < n; i++) {
    storage->metrics[i] = app_metrics[i];
    storage->metrics[i].hash = app_metrics[i].alloc ();
  }

  return storage;
}

/* Insert an uint32_t key and an GLastParse value
 * Note: If the key exists, its value is replaced by the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ins_iglp (khash_t (iglp) *hash, uint64_t key, const GLastParse *lp) {
  khint_t k;
  int ret;

  if (!hash)
    return -1;

  k = kh_put (iglp, hash, key, &ret);
  if (ret == -1)
    return -1;

  kh_val (hash, k) = *lp;

  return 0;
}

/* Create a new GKDB instance given a uint32_t key
 *
 * On error, -1 is returned.
 * On key found, 1 is returned.
 * On success 0 is returned */
static GKDB *
new_db (khash_t (igdb) *hash, uint32_t key) {
  GKDB *db = NULL;
  khint_t k;
  int ret;

  if (!hash)
    return NULL;

  k = kh_put (igdb, hash, key, &ret);
  /* operation failed */
  if (ret == -1)
    return NULL;
  /* the key is present in the hash table */
  if (ret == 0)
    return kh_val (hash, k);

  db = new_gkdb ();
  db->hdb = init_gkhashdb ();
  db->cache = NULL;
  db->store = NULL;
  db->logs = NULL;
  kh_val (hash, k) = db;

  return db;
}

uint32_t *
get_sorted_dates (uint32_t *len) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * hash = get_hdb (db, MTRC_DATES);
  khiter_t key;
  uint32_t *dates = NULL;
  int i = 0;
  uint32_t size = 0;

  if (!hash)
    return NULL;

  size = kh_size (hash);
  dates = xcalloc (size, sizeof (uint32_t));
  for (key = kh_begin (hash); key != kh_end (hash); ++key)
    if (kh_exist (hash, key))
      dates[i++] = kh_key (hash, key);
  qsort (dates, i, sizeof (uint32_t), cmp_ui32_asc);
  *len = i;

  return dates;
}

/* Insert a string key and the corresponding uint8_t value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
int
ins_si08 (khash_t (si08) *hash, const char *key, uint8_t value) {
  khint_t k;
  int ret;
  char *dupkey = NULL;

  if (!hash)
    return -1;

  dupkey = xstrdup (key);
  k = kh_put (si08, hash, dupkey, &ret);
  /* operation failed, or key exists */
  if (ret == -1 || ret == 0) {
    free (dupkey);
    return -1;
  }

  kh_val (hash, k) = value;

  return 0;
}

/* Increment a string key and with the corresponding incremental uint32_t value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, 0 is returned.
 * On success or if the key exists, the value is returned */
static uint8_t
ins_si08_ai (khash_t (si08) *hash, const char *key) {
  uint8_t size = 0, value = 0;

  if (!hash)
    return 0;

  size = kh_size (hash);
  /* the auto increment value starts at SIZE (hash table) + 1 */
  value = size > 0 ? size + 1 : 1;

  return ins_si08 (hash, key, value) == 0 ? value : 0;
}

/* Insert a string key and the corresponding uint32_t value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
int
ins_si32 (khash_t (si32) *hash, const char *key, uint32_t value) {
  khint_t k;
  int ret;
  char *dupkey = NULL;

  if (!hash)
    return -1;

  dupkey = xstrdup (key);
  k = kh_put (si32, hash, dupkey, &ret);
  /* operation failed, or key exists */
  if (ret == -1 || ret == 0) {
    free (dupkey);
    return -1;
  }

  kh_val (hash, k) = value;

  return 0;
}

/* Increment a string key and with the corresponding incremental uint32_t value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, 0 is returned.
 * On success or if the key exists, the value is returned */
uint32_t
ins_si32_inc (khash_t (si32) *hash, const char *key,
              uint32_t (*cb) (khash_t (si32) *, const char *), khash_t (si32) *seqs,
              const char *seqk) {
  khint_t k;
  int ret;
  uint32_t value = 0;

  if (!hash)
    return 0;

  k = kh_put (si32, hash, key, &ret);
  /* operation failed, or key exists */
  if (ret == -1 || ret == 0)
    return 0;

  if ((value = cb (seqs, seqk)) == 0)
    return 0;
  kh_val (hash, k) = value;

  return value;
}

/* Increment a uint32_t key and with the corresponding incremental uint32_t value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, 0 is returned.
 * On success or if the key exists, the value is returned */
uint32_t
ins_ii32_inc (khash_t (ii32) *hash, uint32_t key,
              uint32_t (*cb) (khash_t (si32) *, const char *), khash_t (si32) *seqs,
              const char *seqk) {
  khint_t k;
  int ret;
  uint32_t value = 0;

  if (!hash)
    return 0;

  k = kh_put (ii32, hash, key, &ret);
  /* operation failed, or key exists */
  if (ret == -1 || ret == 0)
    return 0;

  if ((value = cb (seqs, seqk)) == 0)
    return 0;
  kh_val (hash, k) = value;

  return value;
}

/* Insert an uint32_t key and the corresponding string value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
int
ins_is32 (khash_t (is32) *hash, uint32_t key, char *value) {
  khint_t k;
  int ret;

  if (!hash)
    return -1;

  k = kh_put (is32, hash, key, &ret);
  if (ret == -1 || ret == 0)
    return -1;

  kh_val (hash, k) = value;

  return 0;
}

/* Insert a string key and the corresponding string value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, -1 is returned.
 * If key exists, 1 is returned.
 * On success 0 is returned */
static int
ins_ss32 (khash_t (ss32) *hash, const char *key, const char *value) {
  khint_t k;
  int ret;
  char *dupkey = NULL;

  if (!hash)
    return -1;

  dupkey = xstrdup (key);
  k = kh_put (ss32, hash, dupkey, &ret);
  /* operation failed */
  if (ret == -1) {
    free (dupkey);
    return -1;
  }
  /* key exists */
  if (ret == 0) {
    free (dupkey);
    return 1;
  }

  kh_val (hash, k) = xstrdup (value);

  return 0;
}

/* Insert an uint32_t key and an uint32_t value
 * Note: If the key exists, its value is replaced by the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ins_ii32 (khash_t (ii32) *hash, uint32_t key, uint32_t value) {
  khint_t k;
  int ret;

  if (!hash)
    return -1;

  k = kh_put (ii32, hash, key, &ret);
  if (ret == -1)
    return -1;

  kh_val (hash, k) = value;

  return 0;
}

/* Insert an uint32_t key and an uint8_t value
 * Note: If the key exists, its value is replaced by the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ins_ii08 (khash_t (ii08) *hash, uint32_t key, uint8_t value) {
  khint_t k;
  int ret;

  if (!hash)
    return -1;

  k = kh_put (ii08, hash, key, &ret);
  if (ret == -1)
    return -1;

  kh_val (hash, k) = value;

  return 0;
}

/* Insert a uint32_t key and a uint64_t value
 * Note: If the key exists, its value is replaced by the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ins_iu64 (khash_t (iu64) *hash, uint32_t key, uint64_t value) {
  khint_t k;
  int ret;

  if (!hash)
    return -1;

  k = kh_put (iu64, hash, key, &ret);
  if (ret == -1)
    return -1;

  kh_val (hash, k) = value;

  return 0;
}

/* Insert a string key and a uint64_t value
 * Note: If the key exists, the value is not replaced.
 *
 * On error or key exists, -1 is returned.
 * On success 0 is returned */
int
ins_su64 (khash_t (su64) *hash, const char *key, uint64_t value) {
  khint_t k;
  int ret;
  char *dupkey = NULL;

  if (!hash)
    return -1;

  dupkey = xstrdup (key);
  k = kh_put (su64, hash, dupkey, &ret);
  /* operation failed, or key exists */
  if (ret == -1 || ret == 0) {
    free (dupkey);
    return -1;
  }

  kh_val (hash, k) = value;

  return 0;
}

/* Insert a uint64_t key and a uint8_t value
 *
 * On error or key exists, -1 is returned.
 * On key exists, 1 is returned.
 * On success 0 is returned */
int
ins_u648 (khash_t (u648) *hash, uint64_t key, uint8_t value) {
  khint_t k;
  int ret;

  if (!hash)
    return -1;

  k = kh_put (u648, hash, key, &ret);
  if (ret == -1)
    return -1;
  if (ret == 0)
    return 1;

  kh_val (hash, k) = value;

  return 0;
}

/* Increase an uint32_t value given an uint32_t key.
 *
 * On error, 0 is returned.
 * On success the increased value is returned */
uint32_t
inc_ii32 (khash_t (ii32) *hash, uint32_t key, uint32_t inc) {
  khint_t k;
  int ret;

  if (!hash)
    return 0;

  k = kh_get (ii32, hash, key);
  /* key not found, put a new hash with val=0 */
  if (k == kh_end (hash)) {
    k = kh_put (ii32, hash, key, &ret);
    /* operation failed */
    if (ret == -1)
      return 0;
    kh_val (hash, k) = 0;
  }

  return __sync_add_and_fetch (&kh_val (hash, k), inc);
}

/* Increase a uint64_t value given a string key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
inc_su64 (khash_t (su64) *hash, const char *key, uint64_t inc) {
  khint_t k;
  int ret;
  uint64_t value = inc;
  char *dupkey = NULL;

  if (!hash)
    return -1;

  k = kh_get (su64, hash, key);
  /* key not found, set new value to the given `inc` */
  if (k == kh_end (hash)) {
    dupkey = xstrdup (key);
    k = kh_put (su64, hash, dupkey, &ret);
    /* operation failed */
    if (ret == -1) {
      free (dupkey);
      return -1;
    }
  } else {
    value = kh_val (hash, k) + inc;
  }

  kh_val (hash, k) = value;

  return 0;
}

/* Increase a uint64_t value given a uint32_t key.
 * Note: If the key exists, its value is increased by the given inc.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
inc_iu64 (khash_t (iu64) *hash, uint32_t key, uint64_t inc) {
  khint_t k;
  int ret;
  uint64_t value = inc;

  if (!hash)
    return -1;

  k = kh_get (iu64, hash, key);
  /* key found, increment current value by the given `inc` */
  if (k != kh_end (hash))
    value = (uint64_t) kh_val (hash, k) + inc;

  k = kh_put (iu64, hash, key, &ret);
  if (ret == -1)
    return -1;

  kh_val (hash, k) = value;

  return 0;
}

/* Increase an uint32_t value given a string key.
 *
 * On error, 0 is returned.
 * On success the increased value is returned */
static uint32_t
inc_si32 (khash_t (si32) *hash, const char *key, uint32_t inc) {
  khint_t k;
  int ret;
  char *dupkey = NULL;

  if (!hash)
    return 0;

  k = kh_get (si32, hash, key);
  /* key not found, put a new hash with val=0 */
  if (k == kh_end (hash)) {
    dupkey = xstrdup (key);
    k = kh_put (si32, hash, dupkey, &ret);
    /* operation failed */
    if (ret == -1) {
      free (dupkey);
      return 0;
    }
    /* concurrently added */
    if (ret == 0)
      free (dupkey);
    kh_val (hash, k) = 0;
  }

  return __sync_add_and_fetch (&kh_val (hash, k), inc);
}

/* Insert a string key and auto increment int value.
 *
 * On error, 0 is returned.
 * On key found, the stored value is returned
 * On success the value of the key inserted is returned */
uint32_t
ins_ii32_ai (khash_t (ii32) *hash, uint32_t key) {
  int size = 0, value = 0;
  int ret;
  khint_t k;

  if (!hash)
    return 0;

  size = kh_size (hash);
  /* the auto increment value starts at SIZE (hash table) + 1 */
  value = size > 0 ? size + 1 : 1;

  k = kh_put (ii32, hash, key, &ret);
  /* operation failed */
  if (ret == -1)
    return 0;
  /* key exists */
  if (ret == 0)
    return kh_val (hash, k);

  kh_val (hash, k) = value;

  return value;
}

/* Compare if the given needle is in the haystack
 *
 * if equal, 1 is returned, else 0 */
static int
find_int_key_in_list (void *data, void *needle) {
  return (*(uint32_t *) data) == (*(uint32_t *) needle) ? 1 : 0;
}

/* Insert an int key and the corresponding GSLList (Single linked-list) value.
 * Note: If the key exists within the list, the value is not appended.
 *
 * On error, -1 is returned.
 * On success or if key is found, 0 is returned */
int
ins_igsl (khash_t (igsl) *hash, uint32_t key, uint32_t value) {
  khint_t k;
  GSLList *list;
  int ret;

  if (!hash)
    return -1;

  k = kh_get (igsl, hash, key);
  /* key found, check if key exists within the list */
  if (k != kh_end (hash) && (list = kh_val (hash, k))) {
    if (list_find (list, find_int_key_in_list, &value))
      return 0;
    list = list_insert_prepend (list, i322ptr (value));
  } else {
    list = list_create (i322ptr (value));
  }

  k = kh_put (igsl, hash, key, &ret);
  if (ret == -1) {
    list_remove_nodes (list);
    return -1;
  }

  kh_val (hash, k) = list;

  return 0;
}


/* Get the uint32_t value of a given string key.
 *
 * On error, 0 is returned.
 * On success the uint32_t value for the given key is returned */
uint32_t
get_si32 (khash_t (si32) *hash, const char *key) {
  khint_t k;

  if (!hash)
    return 0;

  k = kh_get (si32, hash, key);
  /* key found, return current value */
  if (k != kh_end (hash))
    return __sync_add_and_fetch (&kh_val (hash, k), 0);

  return 0;
}

/* Get the uint8_t value of a given string key.
 *
 * On error, 0 is returned.
 * On success the uint8_t value for the given key is returned */
uint8_t
get_si08 (khash_t (si08) *hash, const char *key) {
  khint_t k;

  if (!hash)
    return 0;

  k = kh_get (si08, hash, key);
  /* key found, return current value */
  if (k != kh_end (hash))
    return kh_val (hash, k);

  return 0;
}

/* Get the uint8_t value of a given string key.
 *
 * On error, 0 is returned.
 * On success the uint8_t value for the given key is returned */
uint8_t
get_ii08 (khash_t (ii08) *hash, uint32_t key) {
  khint_t k;

  if (!hash)
    return 0;

  k = kh_get (ii08, hash, key);
  /* key found, return current value */
  if (k != kh_end (hash))
    return kh_val (hash, k);

  return 0;
}

/* Get the string value of a given uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
get_is32 (khash_t (is32) *hash, uint32_t key) {
  khint_t k;
  char *value = NULL;

  if (!hash)
    return NULL;

  k = kh_get (is32, hash, key);
  /* key found, return current value */
  if (k != kh_end (hash) && (value = kh_val (hash, k)))
    return xstrdup (value);

  return NULL;
}

/* Get the string value of a given string key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
static char *
get_ss32 (khash_t (ss32) *hash, const char *key) {
  khint_t k;
  char *value = NULL;

  if (!hash)
    return NULL;

  k = kh_get (ss32, hash, key);
  /* key found, return current value */
  if (k != kh_end (hash) && (value = kh_val (hash, k)))
    return xstrdup (value);

  return NULL;
}

/* Get the uint32_t value of a given uint32_t key.
 *
 * If key is not found, 0 is returned.
 * On error, -1 is returned.
 * On success the uint32_t value for the given key is returned */
uint32_t
get_ii32 (khash_t (ii32) *hash, uint32_t key) {
  khint_t k;

  if (!hash)
    return 0;

  k = kh_get (ii32, hash, key);
  /* key found, return current value */
  if (k != kh_end (hash))
    return __sync_add_and_fetch (&kh_val (hash, k), 0);

  return 0;
}

/* Get the uint64_t value of a given uint32_t key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
get_iu64 (khash_t (iu64) *hash, uint32_t key) {
  khint_t k;
  uint64_t value = 0;

  if (!hash)
    return 0;

  k = kh_get (iu64, hash, key);
  /* key found, return current value */
  if (k != kh_end (hash) && (value = kh_val (hash, k)))
    return value;

  return 0;
}

/* Get the uint64_t value of a given string key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
get_su64 (khash_t (su64) *hash, const char *key) {
  khint_t k;
  uint64_t val = 0;

  if (!hash)
    return 0;

  k = kh_get (su64, hash, key);
  /* key found, return current value */
  if (k != kh_end (hash) && (val = kh_val (hash, k)))
    return val;

  return 0;
}

/* Get the GLastParse value of a given uint32_t key.
 *
 * If key is not found, {0} is returned.
 * On error, -1 is returned.
 * On success the GLastParse value for the given key is returned */
static GLastParse
get_iglp (khash_t (iglp) *hash, uint64_t key) {
  khint_t k;
  GLastParse lp = { 0 };

  if (!hash)
    return lp;

  k = kh_get (iglp, hash, key);
  /* key found, return current value */
  if (k != kh_end (hash)) {
    lp = kh_val (hash, k);
    return lp;
  }

  return lp;
}

/* Iterate over all the key/value pairs for the given hash structure
 * and set the maximum and minimum values found on an integer key and
 * integer value.
 *
 * Note: These are expensive calls since it has to iterate over all
 * key-value pairs
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
get_ii32_min_max (khash_t (ii32) *hash, uint32_t *min, uint32_t *max) {
  khint_t k;
  uint32_t curvalue = 0;
  int i;

  for (i = 0, k = kh_begin (hash); k != kh_end (hash); ++k) {
    if (!kh_exist (hash, k))
      continue;

    curvalue = kh_value (hash, k);
    if (i++ == 0)
      *min = curvalue;
    if (curvalue > *max)
      *max = curvalue;
    if (curvalue < *min)
      *min = curvalue;
  }
}

/* Iterate over all the key/value pairs for the given hash structure
 * and set the maximum and minimum values found on an integer key and
 * a uint64_t value.
 *
 * Note: These are expensive calls since it has to iterate over all
 * key-value pairs
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
get_iu64_min_max (khash_t (iu64) *hash, uint64_t *min, uint64_t *max) {
  khint_t k;
  uint64_t curvalue = 0;
  int i;

  for (i = 0, k = kh_begin (hash); k != kh_end (hash); ++k) {
    if (!kh_exist (hash, k))
      continue;

    curvalue = kh_value (hash, k);
    if (i++ == 0)
      *min = curvalue;
    if (curvalue > *max)
      *max = curvalue;
    if (curvalue < *min)
      *min = curvalue;
  }
}

uint32_t
ht_get_excluded_ips (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * hash = get_hdb (db, MTRC_CNT_OVERALL);

  if (!hash)
    return 0;

  return get_si32 (hash, "excluded_ip");
}

uint32_t
ht_get_invalid (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * hash = get_hdb (db, MTRC_CNT_OVERALL);

  if (!hash)
    return 0;

  return get_si32 (hash, "failed_requests");
}

uint32_t
ht_get_processed (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * hash = get_hdb (db, MTRC_CNT_OVERALL);

  if (!hash)
    return 0;

  return get_si32 (hash, "total_requests");
}

uint32_t
ht_get_processing_time (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * hash = get_hdb (db, MTRC_CNT_OVERALL);

  if (!hash)
    return 0;

  return get_si32 (hash, "processing_time");
}

uint8_t
ht_insert_meth_proto (const char *key) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si08) * hash = get_hdb (db, MTRC_METH_PROTO);
  uint8_t val = 0;

  if (!hash)
    return 0;

  if ((val = get_si08 (hash, key)) != 0)
    return val;

  return ins_si08_ai (hash, key);
}

uint32_t
ht_inc_cnt_overall (const char *key, uint32_t val) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * hash = get_hdb (db, MTRC_CNT_OVERALL);

  if (!hash)
    return 0;

  return inc_si32 (hash, key, val);
}

int
ht_insert_last_parse (uint64_t key, const GLastParse *lp) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (iglp) * hash = get_hdb (db, MTRC_LAST_PARSE);

  if (!hash)
    return 0;

  return ins_iglp (hash, key, lp);
}

/* Increases the unique key counter from a uint32_t key.
 *
 * On error, 0 is returned.
 * On success the inserted key is returned */
uint32_t
ht_ins_seq (khash_t (si32) *hash, const char *key) {
  if (!hash)
    return 0;

  return inc_si32 (hash, key, 1);
}

/* Insert an IP hostname mapped to the corresponding hostname.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
int
ht_insert_hostname (const char *ip, const char *host) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (ss32) * hash = get_hdb (db, MTRC_HOSTNAMES);

  if (!hash)
    return -1;

  return ins_ss32 (hash, ip, host);
}

/* Insert a JSON log format specification such as request.method => %m.
 *
 * On error -1 is returned.
 * On success or if key exists, 0 is returned */
int
ht_insert_json_logfmt (GO_UNUSED void *userdata, char *key, char *spec) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (ss32) * hash = get_hdb (db, MTRC_JSON_LOGFMT);
  khint_t k;
  int ret;
  char *dupkey = NULL;

  if (!hash)
    return -1;

  k = kh_get (ss32, hash, key);
  /* key found, free it then to insert */
  if (k != kh_end (hash))
    free (kh_val (hash, k));
  else {
    dupkey = xstrdup (key);
    k = kh_put (ss32, hash, dupkey, &ret);
    /* operation failed */
    if (ret == -1) {
      free (dupkey);
      return -1;
    }
  }
  kh_val (hash, k) = xstrdup (spec);

  return 0;
}

GLastParse
ht_get_last_parse (uint64_t key) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (iglp) * hash = get_hdb (db, MTRC_LAST_PARSE);
  return get_iglp (hash, key);
}

/* Get the string value from ht_hostnames given a string key (IP).
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_hostname (const char *host) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (ss32) * hash = get_hdb (db, MTRC_HOSTNAMES);

  if (!hash)
    return NULL;

  return get_ss32 (hash, host);
}

/* Get the string value from ht_json_logfmt given a JSON specifier key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_json_logfmt (const char *key) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (ss32) * hash = get_hdb (db, MTRC_JSON_LOGFMT);

  if (!hash)
    return NULL;

  return get_ss32 (hash, key);
}

void
init_pre_storage (Logs *logs) {
  GKDB *db = NULL;
  ht_db = (khash_t (igdb) *) new_igdb_ht ();
  db = new_db (ht_db, DB_INSTANCE);
  db->logs = logs;
}

static void
free_igdb (khash_t (igdb) *hash, khint_t k) {
  GKDB *db = NULL;
  db = kh_val (hash, k);

  des_igkh (get_hdb (db, MTRC_DATES));
  free_logs (db->logs);
  free_cache (db->cache);
  free_app_metrics (db->hdb);

  kh_del (igdb, hash, k);
  free (kh_val (hash, k));

}

/* Destroys the hash structure */
static void
des_igdb (void *h) {
  khint_t k;
  khash_t (igdb) * hash = h;

  if (!hash)
    return;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k))
      free_igdb (hash, k);
  }
  kh_destroy (igdb, hash);
}

/* Destroys the hash structure and its content */
void
free_storage (void) {
  if (conf.persist)
    persist_data ();
  des_igdb (ht_db);
  free_persisted_data ();
}
