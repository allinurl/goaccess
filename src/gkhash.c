/**
 * gkhash.c -- default hash table functions
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2020 Gerardo Orellana <hello @ goaccess.io>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "gkhash.h"

#include "error.h"
#include "sort.h"
#include "tpl.h"
#include "util.h"
#include "xmalloc.h"

/* *INDENT-OFF* */
/* Hash tables used across the whole app */
static khash_t (igkh) * ht_dates     = NULL;
static khash_t (si32) * ht_seqs      = NULL;
static khash_t (ss32) * ht_hostnames = NULL;
static khash_t (sgsl) * ht_raw_hits  = NULL;
static khash_t (si32) * ht_cnt_overall = NULL;
static khash_t (ii32) * ht_last_parse  = NULL;
/* *INDENT-ON* */

static GKHashStorage *
new_gkhstorage (void) {
  GKHashStorage *storage = xcalloc (1, sizeof (GKHashStorage));
  return storage;
}

static GKHashModule *
new_gkhmodule (uint32_t size) {
  GKHashModule *storage = xcalloc (size, sizeof (GKHashModule));
  return storage;
}

static GKHashGlobal *
new_gkhglobal (void) {
  GKHashGlobal *storage = xcalloc (1, sizeof (GKHashGlobal));
  return storage;
}

/* Get the module string value given a metric enum value.
 *
 * On error, NULL is returned.
 * On success, the string module value is returned. */
static char *
get_mtr_type_str (GSMetricType type) {
  GEnum enum_metric_types[] = {
    {"II32", MTRC_TYPE_II32},
    {"IS32", MTRC_TYPE_IS32},
    {"IU64", MTRC_TYPE_IU64},
    {"SI32", MTRC_TYPE_SI32},
    {"SS32", MTRC_TYPE_SS32},
    {"IGSL", MTRC_TYPE_IGSL},
    {"SGSL", MTRC_TYPE_SGSL},
    {"SU64", MTRC_TYPE_SU64},
    {"IGKH", MTRC_TYPE_IGKH},
    {"U648", MTRC_TYPE_U648},
  };
  return enum2str (enum_metric_types, ARRAY_SIZE (enum_metric_types), type);
}

/* Initialize a new uint32_t key - uint32_t value hash table */
static void *
new_ii32_ht (void) {
  khash_t (ii32) * h = kh_init (ii32);
  return h;
}

/* Initialize a new uint32_t key - uint8_t value hash table */
static void *
new_igkh_ht (void) {
  khash_t (igkh) * h = kh_init (igkh);
  return h;
}

/* Initialize a new uint32_t key - string value hash table */
static void *
new_is32_ht (void) {
  khash_t (is32) * h = kh_init (is32);
  return h;
}

/* Initialize a new uint32_t key - uint64_t value hash table */
static void *
new_iu64_ht (void) {
  khash_t (iu64) * h = kh_init (iu64);
  return h;
}

/* Initialize a new uint64_t key - uint32_t value hash table */
static void *
new_u648_ht (void) {
  khash_t (u648) * h = kh_init (u648);
  return h;
}

/* Initialize a new string key - uint32_t value hash table */
static void *
new_si32_ht (void) {
  khash_t (si32) * h = kh_init (si32);
  return h;
}

/* Initialize a new string key - string value hash table */
static void *
new_ss32_ht (void) {
  khash_t (ss32) * h = kh_init (ss32);
  return h;
}

/* Initialize a new string key - GSLList value hash table */
static void *
new_sgsl_ht (void) {
  khash_t (sgsl) * h = kh_init (sgsl);
  return h;
}

/* Initialize a new uint32_t key - GSLList value hash table */
static void *
new_igsl_ht (void) {
  khash_t (igsl) * h = kh_init (igsl);
  return h;
}

/* Initialize a new string key - uint64_t value hash table */
static void *
new_su64_ht (void) {
  khash_t (su64) * h = kh_init (su64);
  return h;
}

/* Destroys both the hash structure and the keys for a
 * string key - uint32_t value hash */
static void
des_si32_free (void *h) {
  khint_t k;
  khash_t (si32) * hash = h;
  if (!hash)
    return;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      free ((char *) kh_key (hash, k));
    }
  }

  kh_destroy (si32, hash);
}

/* Destroys both the hash structure and its string values */
static void
des_is32_free (void *h) {
  khint_t k;
  khash_t (is32) * hash = h;
  if (!hash)
    return;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      free ((char *) kh_value (hash, k));
    }
  }

  kh_destroy (is32, hash);
}

/* Destroys both the hash structure and its string
 * keys and string values */
static void
des_ss32_free (void *h) {
  khint_t k;
  khash_t (ss32) * hash = h;
  if (!hash)
    return;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      free ((char *) kh_key (hash, k));
      free ((char *) kh_value (hash, k));
    }
  }

  kh_destroy (ss32, hash);
}

/* Destroys the hash structure */
static void
des_ii32 (void *h) {
  khash_t (ii32) * hash = h;
  if (!hash)
    return;
  kh_destroy (ii32, hash);
}

/* Destroys the hash structure */
static void
des_u648 (void *h) {
  khash_t (u648) * hash = h;
  if (!hash)
    return;
  kh_destroy (u648, hash);
}

/* Frees both the hash structure and its GSLList
 * values */
static void
del_sgsl_free (khash_t (sgsl) * hash) {
  khint_t k;
  GSLList *list = NULL;
  if (!hash)
    return;

  for (k = kh_begin (hash); k != kh_end (hash); ++k) {
    if (!kh_exist (hash, k) || !(list = kh_value (hash, k)))
      continue;
    list_remove_nodes (list);
    kh_del (sgsl, hash, k);
  }
}

/* Destroys both the hash structure and its GSLList
 * values */
static void
des_sgsl_free (void *h) {
  khash_t (sgsl) * hash = h;
  if (!hash)
    return;

  del_sgsl_free (hash);
  kh_destroy (sgsl, hash);
}

/* Destroys both the hash structure and its GSLList
 * values */
static void
des_igsl_free (void *h) {
  khash_t (igsl) * hash = h;
  khint_t k;
  void *list = NULL;
  if (!hash)
    return;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k) && (list = kh_value (hash, k))) {
      list_remove_nodes (list);
    }
  }

  kh_destroy (igsl, hash);
}

/* Destroys both the hash structure and the keys for a
 * string key - uint64_t value hash */
static void
des_su64_free (void *h) {
  khash_t (su64) * hash = h;
  khint_t k;
  if (!hash)
    return;

  for (k = 0; k < kh_end (hash); ++k) {
    if (kh_exist (hash, k)) {
      free ((char *) kh_key (hash, k));
    }
  }

  kh_destroy (su64, hash);
}

/* Destroys the hash structure */
static void
des_iu64 (void *h) {
  khash_t (iu64) * hash = h;
  if (!hash)
    return;
  kh_destroy (iu64, hash);
}

/* *INDENT-OFF* */
static const GKHashMetric global_metrics[] = {
  {MTRC_UNIQUE_KEYS , MTRC_TYPE_SI32 , new_si32_ht , des_si32_free , NULL , "SI32_UNIQUE_KEYS.db" } ,
  {MTRC_AGENT_VALS  , MTRC_TYPE_IS32 , new_is32_ht , des_is32_free , NULL , "IS32_AGENT_VALS.db"  } ,
  {MTRC_AGENT_KEYS  , MTRC_TYPE_SI32 , new_si32_ht , des_si32_free , NULL , "SI32_AGENT_KEYS.db"  } ,
  {MTRC_CNT_VALID   , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , NULL , "II32_CNT_VALID.db"   } ,
  {MTRC_CNT_BW      , MTRC_TYPE_IU64 , new_iu64_ht , des_iu64      , NULL , "IU64_CNT_BW.db"      } ,
};

static GKHashMetric module_metrics[] = {
  {MTRC_KEYMAP      , MTRC_TYPE_SI32 , new_si32_ht , des_si32_free , NULL, NULL}  ,
  {MTRC_ROOTMAP     , MTRC_TYPE_IS32 , new_is32_ht , des_is32_free , NULL, NULL}  ,
  {MTRC_DATAMAP     , MTRC_TYPE_IS32 , new_is32_ht , des_is32_free , NULL, NULL}  ,
  {MTRC_UNIQMAP     , MTRC_TYPE_U648 , new_u648_ht , des_u648      , NULL, NULL}  ,
  {MTRC_ROOT        , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , NULL, NULL}  ,
  {MTRC_HITS        , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , NULL, NULL}  ,
  {MTRC_VISITORS    , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , NULL, NULL}  ,
  {MTRC_BW          , MTRC_TYPE_IU64 , new_iu64_ht , des_iu64      , NULL, NULL}  ,
  {MTRC_CUMTS       , MTRC_TYPE_IU64 , new_iu64_ht , des_iu64      , NULL, NULL}  ,
  {MTRC_MAXTS       , MTRC_TYPE_IU64 , new_iu64_ht , des_iu64      , NULL, NULL}  ,
  {MTRC_METHODS     , MTRC_TYPE_IS32 , new_is32_ht , des_is32_free , NULL, NULL}  ,
  {MTRC_PROTOCOLS   , MTRC_TYPE_IS32 , new_is32_ht , des_is32_free , NULL, NULL}  ,
  {MTRC_AGENTS      , MTRC_TYPE_IGSL , new_igsl_ht , des_igsl_free , NULL, NULL}  ,
  {MTRC_METADATA    , MTRC_TYPE_SU64 , new_su64_ht , des_su64_free , NULL, NULL}  ,
};
/* *INDENT-ON* */

/* Initialize map & metric hashes */
static void
init_tables (GModule module, GKHashModule * storage) {
  int n = 0, i;

  n = ARRAY_SIZE (module_metrics);
  for (i = 0; i < n; i++) {
    storage[module].metrics[i] = module_metrics[i];
    storage[module].metrics[i].hash = module_metrics[i].alloc ();
  }
}

/* Destroys the hash structure allocated metrics */
static void
free_global_metrics (GKHashGlobal * ghash) {
  int i, n = 0;
  GKHashMetric mtrc;

  n = ARRAY_SIZE (global_metrics);
  for (i = 0; i < n; i++) {
    mtrc = ghash->metrics[i];
    mtrc.clean (mtrc.hash);
  }
}

/* Destroys the hash structure allocated metrics */
static void
free_module_metrics (GKHashModule * mhash, GModule module) {
  int i, n = 0;
  GKHashMetric mtrc;

  n = ARRAY_SIZE (module_metrics);
  for (i = 0; i < n; i++) {
    mtrc = mhash[module].metrics[i];
    mtrc.clean (mtrc.hash);
  }
}

static void
free_stores (GKHashStorage * store) {
  GModule module;
  size_t idx = 0;

  free_global_metrics (store->ghash);
  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    free_module_metrics (store->mhash, module);
  }

  free (store->ghash);
  free (store->mhash);
  free (store);
}


static void *
get_mod_hash (GKHashModule * hashmodule, GModule module, GSMetric metric) {
  GKHashMetric mtrc;
  int i, n = 0;

  n = ARRAY_SIZE (module_metrics);
  for (i = 0; i < n; i++) {
    mtrc = hashmodule[module].metrics[i];
    if (mtrc.metric != metric)
      continue;
    return mtrc.hash;
  }

  return NULL;
}

static void *
get_global_hash (GKHashGlobal * hashglobal, GSMetric metric) {
  GKHashMetric mtrc;
  int i, n = 0;

  n = ARRAY_SIZE (global_metrics);
  for (i = 0; i < n; i++) {
    mtrc = hashglobal->metrics[i];
    if (mtrc.metric != metric)
      continue;
    return mtrc.hash;
  }

  return NULL;
}

/* Given a module and a metric, get the hash table
 *
 * On error, or if table is not found, NULL is returned.
 * On success the hash structure pointer is returned. */
static void *
get_hash (int module, uint64_t key, GSMetric metric) {
  GKHashStorage *store = NULL;
  khint_t k;

  khash_t (igkh) * hash = ht_dates;

  k = kh_get (igkh, hash, key);
  /* key not found, return NULL */
  if (k == kh_end (hash))
    return NULL;

  store = kh_val (hash, k);
  if (module < 0)
    return get_global_hash (store->ghash, metric);
  return get_mod_hash (store->mhash, module, metric);
}

/* Destroys the hash structure allocated metrics on each render */
void
free_raw_hits (void) {
  if (!ht_raw_hits)
    return;
  del_sgsl_free (ht_raw_hits);
}

static GKHashGlobal *
init_gkhashglobal (void) {
  GKHashGlobal *storage = NULL;

  int n = 0, i;

  storage = new_gkhglobal ();
  n = ARRAY_SIZE (global_metrics);
  for (i = 0; i < n; i++) {
    storage->metrics[i] = global_metrics[i];
    storage->metrics[i].hash = global_metrics[i].alloc ();
  }

  return storage;
}

static GKHashModule *
init_gkhashmodule (void) {
  GKHashModule *storage = NULL;
  GModule module;
  size_t idx = 0;

  storage = new_gkhmodule (TOTAL_MODULES);
  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    storage[module].module = module;
    init_tables (module, storage);
  }

  return storage;
}

/* Insert an uint32_t key and an uint8_t value
 * Note: If the key exists, its value is replaced by the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
ins_igkh (khash_t (igkh) * hash, uint32_t key) {
  GKHashStorage *store = NULL;
  khint_t k;
  int ret;

  if (!hash)
    return -1;

  k = kh_put (igkh, hash, key, &ret);
  /* operation failed */
  if (ret == -1)
    return -1;
  /* the key is present in the hash table */
  if (ret == 0)
    return 1;

  store = new_gkhstorage ();
  store->mhash = init_gkhashmodule ();
  store->ghash = init_gkhashglobal ();

  kh_val (hash, k) = store;

  return 0;
}

/* Insert a string key and the corresponding uint32_t value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
static int
ins_si32 (khash_t (si32) * hash, const char *key, uint32_t value) {
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

/* Insert a string key and the corresponding uint32_t value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
static uint32_t
ins_si32_inc (khash_t (si32) * hash, const char *key,
              uint32_t (*cb) (const char *), const char *seqk) {
  khint_t k;
  int ret;
  char *dupkey = NULL;
  uint32_t value = 0;

  if (!hash)
    return 0;

  dupkey = xstrdup (key);
  k = kh_put (si32, hash, dupkey, &ret);
  /* operation failed, or key exists */
  if (ret == -1 || ret == 0) {
    free (dupkey);
    return 0;
  }

  if ((value = cb (seqk)) == 0)
    return 0;
  kh_val (hash, k) = value;

  return value;
}

/* Insert an uint32_t key and the corresponding string value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
static int
ins_is32 (khash_t (is32) * hash, uint32_t key, const char *value) {
  khint_t k;
  int ret;

  if (!hash)
    return -1;

  k = kh_put (is32, hash, key, &ret);
  if (ret == -1 || ret == 0)
    return -1;

  kh_val (hash, k) = xstrdup (value);

  return 0;
}

/* Insert a string key and the corresponding string value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
static int
ins_ss32 (khash_t (ss32) * hash, const char *key, const char *value) {
  khint_t k;
  int ret;
  char *dupkey = NULL;

  if (!hash)
    return -1;

  dupkey = xstrdup (key);
  k = kh_put (ss32, hash, dupkey, &ret);
  /* operation failed, or key exists */
  if (ret == -1 || ret == 0) {
    free (dupkey);
    return -1;
  }

  kh_val (hash, k) = xstrdup (value);

  return 0;
}

/* Insert an uint32_t key and an uint32_t value
 * Note: If the key exists, its value is replaced by the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
ins_ii32 (khash_t (ii32) * hash, uint32_t key, uint32_t value) {
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

/* Insert a uint32_t key and a uint64_t value
 * Note: If the key exists, its value is replaced by the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
ins_iu64 (khash_t (iu64) * hash, uint32_t key, uint64_t value) {
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
 * Note: If the key exists, its value is replaced by the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
ins_su64 (khash_t (su64) * hash, const char *key, uint64_t value) {
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

static int
ins_u648 (khash_t (u648) * hash, uint64_t key, uint8_t value) {
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
 * Note: If the key exists, its value is increased by the given inc.
 *
 * On error, 0 is returned.
 * On success the inserted value is returned */
static uint32_t
inc_ii32 (khash_t (ii32) * hash, uint32_t key, uint32_t inc) {
  khint_t k;
  int ret;
  uint32_t value = inc;

  if (!hash)
    return 0;

  k = kh_get (ii32, hash, key);
  /* key found, increment current value by the given `inc` */
  if (k != kh_end (hash))
    value = kh_val (hash, k) + inc;

  k = kh_put (ii32, hash, key, &ret);
  if (ret == -1)
    return 0;

  kh_val (hash, k) = value;

  return value;
}

/* Increase a uint64_t value given a string key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
inc_su64 (khash_t (su64) * hash, const char *key, uint64_t inc) {
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
    if (ret == -1)
      return -1;
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
static int
inc_iu64 (khash_t (iu64) * hash, uint32_t key, uint64_t inc) {
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

/* Increase a uint32_t value given a string key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static uint32_t
inc_si32 (khash_t (si32) * hash, const char *key, uint32_t inc) {
  khint_t k;
  int ret;
  uint32_t value = inc;

  if (!hash)
    return 0;

  k = kh_get (si32, hash, key);
  /* key not found, set new value to the given `inc` */
  if (k == kh_end (hash)) {
    k = kh_put (si32, hash, key, &ret);
    /* operation failed */
    if (ret == -1)
      return 0;
  } else {
    value = kh_val (hash, k) + inc;
  }

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
 * On success 0 is returned */
static int
ins_igsl (khash_t (igsl) * hash, uint32_t key, uint32_t value) {
  khint_t k;
  GSLList *list, *match;
  int ret;

  if (!hash)
    return -1;

  k = kh_get (igsl, hash, key);
  /* key found, check if key exists within the list */
  if (k != kh_end (hash) && (list = kh_val (hash, k))) {
    if ((match = list_find (list, find_int_key_in_list, &value)))
      return 0;
    list = list_insert_prepend (list, i322ptr (value));
  } else {
    list = list_create (i322ptr (value));
  }

  k = kh_put (igsl, hash, key, &ret);
  if (ret == -1)
    return -1;

  kh_val (hash, k) = list;

  return 0;
}

/* Insert a string key and the corresponding GSLList (Single linked-list) value.
 * Note: If the key exists within the list, the value is not appended.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
ins_sgsl (khash_t (sgsl) * hash, const char *key, uint32_t value) {
  khint_t k;
  GSLList *list, *match;
  int ret;

  if (!hash)
    return -1;

  k = kh_get (sgsl, hash, key);
  /* key found, check if key exists within the list */
  if (k != kh_end (hash) && (list = kh_val (hash, k))) {
    if ((match = list_find (list, find_int_key_in_list, &value)))
      return 0;
    list = list_insert_prepend (list, i322ptr (value));
  } else {
    list = list_create (i322ptr (value));
  }

  k = kh_put (sgsl, hash, key, &ret);
  if (ret == -1)
    return -1;

  kh_val (hash, k) = list;

  return 0;
}

/* Get the on-disk databases path.
 *
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
    FATAL ("Unable to open the specified config file. %s", strerror (errno));

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

static char *
get_filename (GModule module, GKHashMetric mtrc) {
  char *mtrstr = NULL, *modstr = NULL, *type = NULL, *fn = NULL;

  if (!(mtrstr = get_mtr_str (mtrc.metric)))
    FATAL ("Unable to allocate metric name.");
  if (!(modstr = get_module_str (module)))
    FATAL ("Unable to allocate module name.");
  if (!(type = get_mtr_type_str (mtrc.type)))
    FATAL ("Unable to allocate module name.");

  fn = xmalloc (snprintf (NULL, 0, "%s_%s_%s.db", type, modstr, mtrstr) + 1);
  sprintf (fn, "%s_%s_%s.db", type, modstr, mtrstr);

  free (mtrstr);
  free (type);
  free (modstr);

  return fn;
}

static void
close_tpl (tpl_node * tn, const char *fn) {
  tpl_dump (tn, TPL_FILE, fn);
  tpl_free (tn);
}

static void
restore_global_si32 (khash_t (si32) * hash, const char *fn) {
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

static void
persist_global_si32 (khash_t (si32) * hash, const char *fn) {
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


static void
restore_global_ii32 (khash_t (ii32) * hash, const char *fn) {
  tpl_node *tn;
  uint32_t key, val;
  char fmt[] = "A(uu)";

  tn = tpl_map (fmt, &key, &val);
  tpl_load (tn, TPL_FILE, fn);
  while (tpl_unpack (tn, 1) > 0) {
    ins_ii32 (hash, key, val);
  }
  tpl_free (tn);
}

static void
persist_global_ii32 (khash_t (ii32) * hash, const char *fn) {
  tpl_node *tn;
  khint_t k;
  uint32_t key, val;
  char fmt[] = "A(uu)";

  if (!hash || kh_size (hash) == 0)
    return;

  tn = tpl_map (fmt, &key, &val);
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

static int
restore_si32 (GSMetric metric, const char *path, int module) {
  khash_t (si32) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(su))";
  int date = 0;
  char *key = NULL;
  uint32_t val = 0;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    /* if operation fails */
    if ((ht_insert_date (date) == -1) ||
        !(hash = get_hash (module, date, metric)))
      return 1;
    while (tpl_unpack (tn, 2) > 0) {
      ins_si32 (hash, key, val);
      free (key);
    }
  }
  tpl_free (tn);

  return 0;
}

static int
persist_si32 (GSMetric metric, const char *path, int module) {
  khash_t (si32) * hash = NULL;
  tpl_node *tn = NULL;
  int date = 0;
  char fmt[] = "A(iA(su))";
  uint32_t val = 0;
  const char *key = NULL;

  if (!ht_dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (ht_dates, date, {
    if (!(hash = get_hash (module, date, metric)))
      return -1;
    kh_foreach (hash, key, val, { tpl_pack (tn, 2); });
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */
  close_tpl (tn, path);

  return 0;
}

static int
restore_is32 (GSMetric metric, const char *path, int module) {
  khash_t (is32) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(us))";
  int date = 0;
  uint32_t key = 0;
  char *val = 0;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    /* if operation fails */
    if ((ht_insert_date (date) == -1) ||
        !(hash = get_hash (module, date, metric)))
      return 1;
    while (tpl_unpack (tn, 2) > 0) {
      ins_is32 (hash, key, val);
      free (val);
    }
  }
  tpl_free (tn);

  return 0;
}

static int
persist_is32 (GSMetric metric, const char *path, int module) {
  khash_t (is32) * hash = NULL;
  tpl_node *tn = NULL;
  int date = 0;
  char fmt[] = "A(iA(us))";
  char *val = NULL;
  uint32_t key = 0;

  if (!ht_dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (ht_dates, date, {
    if (!(hash = get_hash (module, date, metric)))
      return -1;
    kh_foreach (hash, key, val, { tpl_pack (tn, 2); });
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */
  close_tpl (tn, path);

  return 0;
}

static int
restore_ii32 (GSMetric metric, const char *path, int module) {
  khash_t (ii32) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(uu))";
  int date = 0;
  uint32_t key = 0, val = 0;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    /* if operation fails */
    if ((ht_insert_date (date) == -1) ||
        !(hash = get_hash (module, date, metric)))
      return 1;
    while (tpl_unpack (tn, 2) > 0) {
      ins_ii32 (hash, key, val);
    }
  }
  tpl_free (tn);

  return 0;
}

static int
persist_ii32 (GSMetric metric, const char *path, int module) {
  khash_t (ii32) * hash = NULL;
  tpl_node *tn = NULL;
  int date = 0;
  char fmt[] = "A(iA(uu))";
  uint32_t key = 0, val = 0;

  if (!ht_dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (ht_dates, date, {
    if (!(hash = get_hash (module, date, metric)))
      return -1;
    kh_foreach (hash, key, val, { tpl_pack (tn, 2); });
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */
  close_tpl (tn, path);

  return 0;
}

static int
restore_u648 (GSMetric metric, const char *path, int module) {
  khash_t (u648) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(Uv))";
  int date = 0;
  uint64_t key;
  uint16_t val = 0;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    /* if operation fails */
    if ((ht_insert_date (date) == -1) ||
        !(hash = get_hash (module, date, metric)))
      return 1;
    while (tpl_unpack (tn, 2) > 0) {
      ins_u648 (hash, key, val);
    }
  }
  tpl_free (tn);

  return 0;
}

static int
persist_u648 (GSMetric metric, const char *path, int module) {
  khash_t (u648) * hash = NULL;
  tpl_node *tn = NULL;
  int date = 0;
  char fmt[] = "A(iA(Uv))";
  uint64_t key;
  uint16_t val = 0;

  if (!ht_dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (ht_dates, date, {
    if (!(hash = get_hash (module, date, metric)))
      return -1;
    kh_foreach (hash, key, val, { tpl_pack (tn, 2); });
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */
  close_tpl (tn, path);

  return 0;
}

static int
restore_iu64 (GSMetric metric, const char *path, int module) {
  khash_t (iu64) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(uU))";
  int date = 0;
  uint32_t key;
  uint64_t val;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    /* if operation fails */
    if ((ht_insert_date (date) == -1) ||
        !(hash = get_hash (module, date, metric)))
      return 1;
    while (tpl_unpack (tn, 2) > 0) {
      ins_iu64 (hash, key, val);
    }
  }
  tpl_free (tn);

  return 0;
}

static int
persist_iu64 (GSMetric metric, const char *path, int module) {
  khash_t (iu64) * hash = NULL;
  tpl_node *tn = NULL;
  int date = 0;
  char fmt[] = "A(iA(uU))";
  uint32_t key;
  uint64_t val;

  if (!ht_dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (ht_dates, date, {
    if (!(hash = get_hash (module, date, metric)))
      return -1;
    kh_foreach (hash, key, val, { tpl_pack (tn, 2); });
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */
  close_tpl (tn, path);

  return 0;
}

static int
restore_su64 (GSMetric metric, const char *path, int module) {
  khash_t (su64) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(sU))";
  int date = 0;
  char *key = NULL;
  uint64_t val;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    /* if operation fails */
    if ((ht_insert_date (date) == -1) ||
        !(hash = get_hash (module, date, metric)))
      return 1;
    while (tpl_unpack (tn, 2) > 0) {
      ins_su64 (hash, key, val);
      free (key);
    }
  }
  tpl_free (tn);

  return 0;
}

static int
persist_su64 (GSMetric metric, const char *path, int module) {
  khash_t (su64) * hash = NULL;
  tpl_node *tn = NULL;
  int date = 0;
  char fmt[] = "A(iA(sU))";
  const char *key = NULL;
  uint64_t val;

  if (!ht_dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (ht_dates, date, {
    if (!(hash = get_hash (module, date, metric)))
      return -1;
    kh_foreach (hash, key, val, { tpl_pack (tn, 2); });
    tpl_pack (tn, 1);
  });
  /* *INDENT-ON* */
  close_tpl (tn, path);

  return 0;
}

static int
restore_igsl (GSMetric metric, const char *path, int module) {
  khash_t (igsl) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(uu))";
  int date = 0;
  uint32_t key, val;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    /* if operation fails */
    if ((ht_insert_date (date) == -1) ||
        !(hash = get_hash (module, date, metric)))
      return 1;
    while (tpl_unpack (tn, 2) > 0) {
      ins_igsl (hash, key, val);
    }
  }
  tpl_free (tn);

  return 0;
}

static int
persist_igsl (GSMetric metric, const char *path, int module) {
  khash_t (igsl) * hash = NULL;
  GSLList *node;
  tpl_node *tn = NULL;
  int date = 0;
  char fmt[] = "A(iA(uu))";
  uint32_t key, val;

  if (!ht_dates || !(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  /* *INDENT-OFF* */
  HT_FOREACH_KEY (ht_dates, date, {
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

static char *
check_restore_path (const char *fn) {
  char *path = set_db_path (fn);
  if (access (path, F_OK) != -1)
    return path;

  LOG_DEBUG (("DB file %s doesn't exist. %s\n", path, strerror (errno)));
  free (path);
  return NULL;
}

static void
restore_by_type (GKHashMetric mtrc, const char *fn, int module) {
  char *path = NULL;

  if (!(path = check_restore_path (fn)))
    goto clean;

  switch (mtrc.type) {
  case MTRC_TYPE_SI32:
    restore_si32 (mtrc.metric, path, module);
    break;
  case MTRC_TYPE_IS32:
    restore_is32 (mtrc.metric, path, module);
    break;
  case MTRC_TYPE_II32:
    restore_ii32 (mtrc.metric, path, module);
    break;
  case MTRC_TYPE_U648:
    restore_u648 (mtrc.metric, path, module);
    break;
  case MTRC_TYPE_IU64:
    restore_iu64 (mtrc.metric, path, module);
    break;
  case MTRC_TYPE_SU64:
    restore_su64 (mtrc.metric, path, module);
    break;
  case MTRC_TYPE_IGSL:
    restore_igsl (mtrc.metric, path, module);
    break;
  default:
    break;
  }
clean:
  free (path);
}

static void
restore_metric_type (GModule module, GKHashMetric mtrc) {
  char *fn = NULL;

  fn = get_filename (module, mtrc);
  restore_by_type (mtrc, fn, module);
  free (fn);
}

static void
restore_global (void) {
  char *path = NULL;

  if ((path = check_restore_path ("SI32_CNT_OVERALL.db"))) {
    restore_global_si32 (ht_cnt_overall, path);
    free (path);
  }
  if ((path = check_restore_path ("II32_LAST_PARSE.db"))) {
    restore_global_ii32 (ht_last_parse, path);
    free (path);
  }
}

static void
restore_data (void) {
  GModule module;
  int i, n = 0;
  size_t idx = 0;

  restore_global ();

  n = ARRAY_SIZE (global_metrics);
  for (i = 0; i < n; ++i)
    restore_by_type (global_metrics[i], global_metrics[i].filename, -1);

  n = ARRAY_SIZE (module_metrics);
  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    for (i = 0; i < n; ++i) {
      restore_metric_type (module, module_metrics[i]);
    }
  }
}

static void
persist_by_type (GKHashMetric mtrc, const char *fn, int module) {
  char *path = NULL;
  path = set_db_path (fn);

  switch (mtrc.type) {
  case MTRC_TYPE_SI32:
    persist_si32 (mtrc.metric, path, module);
    break;
  case MTRC_TYPE_IS32:
    persist_is32 (mtrc.metric, path, module);
    break;
  case MTRC_TYPE_II32:
    persist_ii32 (mtrc.metric, path, module);
    break;
  case MTRC_TYPE_U648:
    persist_u648 (mtrc.metric, path, module);
    break;
  case MTRC_TYPE_IU64:
    persist_iu64 (mtrc.metric, path, module);
    break;
  case MTRC_TYPE_SU64:
    persist_su64 (mtrc.metric, path, module);
    break;
  case MTRC_TYPE_IGSL:
    persist_igsl (mtrc.metric, path, module);
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

static void
persist_global (void) {
  char *path = NULL;

  if ((path = set_db_path ("SI32_CNT_OVERALL.db"))) {
    persist_global_si32 (ht_cnt_overall, path);
    free (path);
  }
  if ((path = set_db_path ("II32_LAST_PARSE.db"))) {
    persist_global_ii32 (ht_last_parse, path);
    free (path);
  }
}

static void
persist_data (void) {
  GModule module;
  int i, n = 0;
  size_t idx = 0;

  persist_global ();

  n = ARRAY_SIZE (global_metrics);
  for (i = 0; i < n; ++i)
    persist_by_type (global_metrics[i], global_metrics[i].filename, -1);

  n = ARRAY_SIZE (module_metrics);
  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    for (i = 0; i < n; ++i) {
      persist_metric_type (module, module_metrics[i]);
    }
  }
}

/* Get the uint32_t value of a given string key.
 *
 * On error, 0 is returned.
 * On success the uint32_t value for the given key is returned */
static uint32_t
get_si32 (khash_t (si32) * hash, const char *key) {
  khint_t k;

  if (!hash)
    return 0;

  k = kh_get (si32, hash, key);
  /* key found, return current value */
  if (k != kh_end (hash))
    return kh_val (hash, k);

  return 0;
}

/* Get the string value of a given uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
static char *
get_is32 (khash_t (is32) * hash, uint32_t key) {
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
get_ss32 (khash_t (ss32) * hash, const char *key) {
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
static uint32_t
get_ii32 (khash_t (ii32) * hash, uint32_t key) {
  khint_t k;
  uint32_t value = 0;

  if (!hash)
    return 0;

  k = kh_get (ii32, hash, key);
  /* key found, return current value */
  if (k != kh_end (hash) && (value = kh_val (hash, k)))
    return value;

  return 0;
}

/* Get the uint64_t value of a given uint32_t key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
static uint64_t
get_iu64 (khash_t (iu64) * hash, uint32_t key) {
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
static uint64_t
get_su64 (khash_t (su64) * hash, const char *key) {
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

/* Iterate over all the key/value pairs for the given hash structure
 * and set the maximum and minimum values found on an integer key and
 * integer value.
 *
 * Note: This are expensive calls since it has to iterate over all
 * key-value pairs
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
static void
get_ii32_min_max (khash_t (ii32) * hash, uint32_t * min, uint32_t * max) {
  khint_t k;
  uint32_t curvalue = 0;
  int i;

  for (i = 0, k = kh_begin (hash); k != kh_end (hash); ++k) {
    if (!kh_exist (hash, k))
      continue;

    curvalue = kh_value (hash, k);
    if (i++ == 0)
      *min += curvalue;
    if (curvalue > *max)
      *max += curvalue;
    if (curvalue < *min)
      *min += curvalue;
  }
}

/* Iterate over all the key/value pairs for the given hash structure
 * and set the maximum and minimum values found on an integer key and
 * a uint64_t value.
 *
 * Note: This are expensive calls since it has to iterate over all
 * key-value pairs
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
static void
get_iu64_min_max (khash_t (iu64) * hash, uint64_t * min, uint64_t * max) {
  khint_t k;
  uint64_t curvalue = 0;
  int i;

  for (i = 0, k = kh_begin (hash); k != kh_end (hash); ++k) {
    if (!kh_exist (hash, k))
      continue;

    curvalue = kh_value (hash, k);
    if (i++ == 0)
      *min += curvalue;
    if (curvalue > *max)
      *max += curvalue;
    if (curvalue < *min)
      *min += curvalue;
  }
}

/* Get the number of elements in a dates hash.
 *
 * Return 0 if the operation fails, else number of elements. */
uint32_t
ht_get_size_dates (void) {
  khash_t (igkh) * hash = ht_dates;

  if (!hash)
    return 0;

  return kh_size (hash);
}

uint32_t
ht_get_excluded_ips (void) {
  khash_t (si32) * hash = ht_cnt_overall;

  if (!hash)
    return 0;

  return get_si32 (hash, "excluded_ip");
}

uint32_t
ht_get_invalid (void) {
  khash_t (si32) * hash = ht_cnt_overall;

  if (!hash)
    return 0;

  return get_si32 (hash, "failed_requests");
}

uint32_t
ht_get_processed (void) {
  khash_t (si32) * hash = ht_cnt_overall;

  if (!hash)
    return 0;

  return get_si32 (hash, "total_requests");
}

uint32_t
ht_sum_valid (void) {
  khash_t (ii32) * hash = NULL;
  uint32_t k = 0;
  uint32_t sum = 0;

  if (!ht_dates)
    return 0;

  /* *INDENT-OFF* */
  HT_SUM_VAL (ht_dates, k, {
    if ((hash = get_hash (-1, k, MTRC_CNT_VALID)))
      sum += get_ii32 (hash, 1);
  });
  /* *INDENT-ON* */

  return sum;
}

uint64_t
ht_sum_bw (void) {
  khash_t (iu64) * hash = NULL;
  uint32_t k = 0;
  uint64_t sum = 0;

  if (!ht_dates)
    return 0;

  /* *INDENT-OFF* */
  HT_SUM_VAL (ht_dates, k, {
    if ((hash = get_hash (-1, k, MTRC_CNT_BW)))
      sum += get_iu64 (hash, 1);
  });
  /* *INDENT-ON* */

  return sum;
}

int
ht_insert_last_parse (uint32_t key, uint32_t value) {
  khash_t (ii32) * hash = ht_last_parse;

  if (!hash)
    return 0;

  return ins_ii32 (hash, key, value);
}

int
ht_insert_date (uint32_t key) {
  khash_t (igkh) * hash = ht_dates;

  if (!hash)
    return 0;

  return ins_igkh (hash, key);
}

uint32_t
ht_inc_cnt_overall (const char *key, uint32_t val) {
  khash_t (si32) * hash = ht_cnt_overall;

  if (!hash)
    return 0;

  if (get_si32 (hash, key) != 0)
    return inc_si32 (hash, key, val);
  return inc_si32 (hash, xstrdup (key), val);
}

uint32_t
ht_inc_cnt_valid (uint32_t date, uint32_t inc) {
  khash_t (ii32) * hash = get_hash (-1, date, MTRC_CNT_VALID);

  if (!hash)
    return 0;

  return inc_ii32 (hash, 1, inc);
}

int
ht_inc_cnt_bw (uint32_t date, uint64_t inc) {
  khash_t (iu64) * hash = get_hash (-1, date, MTRC_CNT_BW);

  if (!hash)
    return 0;

  return inc_iu64 (hash, 1, inc);
}

/* Increases the unique key counter from a uint32_t key.
 *
 * On error, 0 is returned.
 * On success the inserted key is returned */
static uint32_t
ht_ins_seq (const char *key) {
  khash_t (si32) * hash = ht_seqs;

  if (!hash)
    return 0;

  if (get_si32 (hash, key) != 0)
    return inc_si32 (hash, key, 1);
  return inc_si32 (hash, xstrdup (key), 1);
}

/* Insert a unique visitor key string (IP/DATE/UA), mapped to an auto
 * incremented value.
 *
 * If the given key exists, its value is returned.
 * On error, 0 is returned.
 * On success the value of the key inserted is returned */
uint32_t
ht_insert_unique_key (uint32_t date, const char *key) {
  uint32_t val = 0;
  khash_t (si32) * hash = get_hash (-1, date, MTRC_UNIQUE_KEYS);

  if (!hash)
    return 0;

  if ((val = get_si32 (hash, key)) != 0)
    return val;

  return ins_si32_inc (hash, key, ht_ins_seq, "ht_unique_keys");
}

/* Insert a user agent key string, mapped to an auto incremented value.
 *
 * If the given key exists, its value is returned.
 * On error, 0 is returned.
 * On success the value of the key inserted is returned */
uint32_t
ht_insert_agent_key (uint32_t date, const char *key) {
  uint32_t val = 0;
  khash_t (si32) * hash = get_hash (-1, date, MTRC_AGENT_KEYS);

  if (!hash)
    return 0;

  if ((val = get_si32 (hash, key)) != 0)
    return val;

  return ins_si32_inc (hash, key, ht_ins_seq, "ht_agent_keys");
}

/* Insert a user agent uint32_t key, mapped to a user agent string value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_agent_value (uint32_t date, uint32_t key, const char *value) {
  khash_t (is32) * hash = get_hash (-1, date, MTRC_AGENT_VALS);

  if (!hash)
    return -1;

  return ins_is32 (hash, key, value);
}

/* Insert a keymap string key.
 *
 * If the given key exists, its value is returned.
 * On error, 0 is returned.
 * On success the value of the key inserted is returned */
uint32_t
ht_insert_keymap (GModule module, uint32_t date, const char *key) {
  khash_t (si32) * hash = get_hash (module, date, MTRC_KEYMAP);
  uint32_t value = 0;
  char *modstr = NULL;

  if (!hash)
    return 0;

  if ((value = get_si32 (hash, key)) != 0)
    return value;

  modstr = get_module_str (module);
  value = ins_si32_inc (hash, key, ht_ins_seq, modstr);
  free (modstr);
  return value;
}

/* Encode a data key and a unique visitor's key to a new uint64_t key
  *
  * ###NOTE: THIS LIMITS THE MAX VALUE OF A DATA TABLE TO uint32_t
  * WILL NEED TO CHANGE THIS IF WE GO OVER uint32_t
  */
static uint64_t
u64encode (uint32_t x, uint32_t y) {
  return x >
    y ? (uint32_t) y | ((uint64_t) x << 32) : (uint32_t) x | ((uint64_t) y <<
                                                              32);
}

void
u64decode (uint64_t n, uint32_t * x, uint32_t * y) {
  *x = (uint64_t) n >> 32;
  *y = (uint64_t) n & 0xFFFFFFFF;
}

/* Insert a uniqmap string key.
 *
 * If the given key exists, 0 is returned.
 * On error, 0 is returned.
 * On success the value of the key inserted is returned */
int
ht_insert_uniqmap (GModule module, uint32_t date, uint32_t key, uint32_t value) {
  khash_t (u648) * hash = get_hash (module, date, MTRC_UNIQMAP);
  uint64_t k = 0;

  if (!hash)
    return 0;

  k = u64encode (key, value);
  return ins_u648 (hash, k, 1) == 0 ? 1 : 0;
}

/* Insert a datamap uint32_t key and string value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_datamap (GModule module, uint32_t date, uint32_t key,
                   const char *value) {
  khash_t (is32) * hash = get_hash (module, date, MTRC_DATAMAP);

  if (!hash)
    return -1;

  return ins_is32 (hash, key, value);
}

/* Insert a rootmap uint32_t key from the keymap store mapped to its string
 * value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_rootmap (GModule module, uint32_t date, uint32_t key,
                   const char *value) {
  khash_t (is32) * hash = get_hash (module, date, MTRC_ROOTMAP);

  if (!hash)
    return -1;

  return ins_is32 (hash, key, value);
}

/* Insert a data uint32_t key mapped to the corresponding uint32_t root key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_root (GModule module, uint32_t date, uint32_t key, uint32_t value) {
  khash_t (ii32) * hash = get_hash (module, date, MTRC_ROOT);

  if (!hash)
    return -1;

  return ins_ii32 (hash, key, value);
}

/* Increases hits counter from a uint32_t key.
 *
 * On error, 0 is returned.
 * On success the inserted value is returned */
uint32_t
ht_insert_hits (GModule module, uint32_t date, uint32_t key, uint32_t inc) {
  khash_t (ii32) * hash = get_hash (module, date, MTRC_HITS);

  if (!hash)
    return 0;

  return inc_ii32 (hash, key, inc);
}

/* Increases visitors counter from a uint32_t key.
 *
 * On error, 0 is returned.
 * On success the inserted value is returned */
uint32_t
ht_insert_visitor (GModule module, uint32_t date, uint32_t key, uint32_t inc) {
  khash_t (ii32) * hash = get_hash (module, date, MTRC_VISITORS);

  if (!hash)
    return 0;

  return inc_ii32 (hash, key, inc);
}

/* Increases bandwidth counter from a uint32_t key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_bw (GModule module, uint32_t date, uint32_t key, uint64_t inc) {
  khash_t (iu64) * hash = get_hash (module, date, MTRC_BW);

  if (!hash)
    return -1;

  return inc_iu64 (hash, key, inc);
}

/* Increases cumulative time served counter from a uint32_t key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_cumts (GModule module, uint32_t date, uint32_t key, uint64_t inc) {
  khash_t (iu64) * hash = get_hash (module, date, MTRC_CUMTS);

  if (!hash)
    return -1;

  return inc_iu64 (hash, key, inc);
}

/* Insert the maximum time served counter from a uint32_t key.
 * Note: it compares the current value with the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_maxts (GModule module, uint32_t date, uint32_t key, uint64_t value) {
  uint64_t curvalue = 0;
  khash_t (iu64) * hash = get_hash (module, date, MTRC_MAXTS);

  if (!hash)
    return -1;

  if ((curvalue = get_iu64 (hash, key)) < value)
    ins_iu64 (hash, key, value);

  return 0;
}

/* Insert a method given an uint32_t key and string value.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
int
ht_insert_method (GModule module, uint32_t date, uint32_t key,
                  const char *value) {
  khash_t (is32) * hash = get_hash (module, date, MTRC_METHODS);

  if (!hash)
    return -1;

  return ins_is32 (hash, key, value);
}

/* Insert a protocol given an uint32_t key and string value.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
int
ht_insert_protocol (GModule module, uint32_t date, uint32_t key,
                    const char *value) {
  khash_t (is32) * hash = get_hash (module, date, MTRC_PROTOCOLS);

  if (!hash)
    return -1;

  return ins_is32 (hash, key, value);
}

/* Insert an agent for a hostname given an uint32_t key and uint32_t value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_agent (GModule module, uint32_t date, uint32_t key, uint32_t value) {
  khash_t (igsl) * hash = get_hash (module, date, MTRC_AGENTS);

  if (!hash)
    return -1;

  return ins_igsl (hash, key, value);
}

/* Insert meta data counters from a string key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_meta_data (GModule module, uint32_t date, const char *key,
                     uint64_t value) {
  khash_t (su64) * hash = get_hash (module, date, MTRC_METADATA);

  if (!hash)
    return -1;

  return inc_su64 (hash, key, value);
}

/* Insert a keymap string key mapped to the corresponding uint32_t value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
ht_insert_keymap_list (const char *key, uint32_t value) {
  khash_t (sgsl) * hash = ht_raw_hits;

  if (!hash)
    return -1;
  return ins_sgsl (hash, key, value);
}

/* Insert an IP hostname mapped to the corresponding hostname.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
int
ht_insert_hostname (const char *ip, const char *host) {
  khash_t (ss32) * hash = ht_hostnames;

  if (!hash)
    return -1;

  return ins_ss32 (hash, ip, host);
}

uint32_t
ht_get_last_parse (uint32_t key) {
  khash_t (ii32) * hash = ht_last_parse;

  if (!hash)
    return 0;

  return get_ii32 (hash, key);
}

/* Get the number of elements in a datamap.
 *
 * Return -1 if the operation fails, else number of elements. */
uint32_t
ht_get_size_datamap (GModule module) {
  khash_t (is32) * hash = NULL;
  uint32_t k = 0;
  uint32_t sum = 0;

  if (!ht_dates)
    return 0;

  /* *INDENT-OFF* */
  HT_SUM_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_DATAMAP)))
      sum += kh_size (hash);
  });
  /* *INDENT-ON* */

  return sum;
}

/* Get the number of elements in a uniqmap.
 *
 * On error, 0 is returned.
 * On success the number of elements in MTRC_UNIQMAP is returned */
uint32_t
ht_get_size_uniqmap (GModule module) {
  khash_t (u648) * hash = NULL;
  uint32_t k = 0;
  uint32_t sum = 0;

  if (!ht_dates)
    return 0;

  /* *INDENT-OFF* */
  HT_SUM_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_UNIQMAP)))
    sum += kh_size (hash);
  });
  /* *INDENT-ON* */

  return sum;
}

/* Get the string data value of a given uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_datamap (GModule module, uint32_t key) {
  khash_t (is32) * hash = NULL;
  char *data = NULL;
  uint32_t k = 0;

  if (!ht_dates)
    return NULL;

  /* *INDENT-OFF* */
  HT_FIRST_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_DATAMAP)))
      if ((data = get_is32 (hash, key)))
        return data;
  });
  /* *INDENT-ON* */

  return NULL;
}

/* Get the string root from MTRC_ROOTMAP given an uint32_t data key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
static uint32_t
ht_get_root_key (GModule module, uint32_t key) {
  khash_t (ii32) * hash = NULL;
  uint32_t k = 0, root_key = 0;

  if (!ht_dates)
    return 0;

  /* *INDENT-OFF* */
  HT_FIRST_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_ROOT)))
      if ((root_key = get_ii32 (hash, key)))
        return root_key;
  });
  /* *INDENT-ON* */

  return root_key;
}

/* Get the string root from MTRC_ROOTMAP given an uint32_t data key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_root (GModule module, uint32_t key) {
  khash_t (is32) * hash = NULL;
  char *data = NULL;
  uint32_t k = 0, root_key = 0;

  if (!ht_dates)
    return NULL;

  if ((root_key = ht_get_root_key (module, key)) == 0)
    return NULL;

  /* *INDENT-OFF* */
  HT_FIRST_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_ROOTMAP)))
      if ((data = get_is32 (hash, root_key)))
        return data;
  });
  /* *INDENT-ON* */

  return NULL;
}

/* Get the uint32_t visitors value from MTRC_VISITORS given an uint32_t key.
 *
 * If key is not found, 0 is returned.
 * On error, 0 is returned.
 * On success the uint32_t value for the given key is returned */
static uint32_t
ht_get_hits (GModule module, uint32_t key) {
  khash_t (ii32) * hash = NULL;
  uint32_t k = 0;
  uint32_t sum = 0;

  if (!ht_dates)
    return 0;

  /* *INDENT-OFF* */
  HT_SUM_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_HITS)))
      sum += get_ii32 (hash, key);
  });
  /* *INDENT-ON* */

  return sum;
}

/* Get the uint32_t visitors value from MTRC_VISITORS given an uint32_t key.
 *
 * If key is not found, 0 is returned.
 * On error, -1 is returned.
 * On success the uint32_t value for the given key is returned */
uint32_t
ht_get_visitors (GModule module, uint32_t key) {
  khash_t (ii32) * hash = NULL;
  uint32_t k = 0;
  uint32_t sum = 0;

  if (!ht_dates)
    return 0;

  /* *INDENT-OFF* */
  HT_SUM_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_VISITORS)))
      sum += get_ii32 (hash, key);
  });
  /* *INDENT-ON* */

  return sum;
}

/* Get the uint64_t value from MTRC_BW given an uint32_t key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_bw (GModule module, uint32_t key) {
  khash_t (iu64) * hash = NULL;
  uint32_t k = 0;
  uint64_t sum = 0;

  if (!ht_dates)
    return 0;

  /* *INDENT-OFF* */
  HT_SUM_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_BW)))
      sum += get_iu64 (hash, key);
  });
  /* *INDENT-ON* */

  return sum;
}

/* Get the uint64_t value from MTRC_CUMTS given an uint32_t key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_cumts (GModule module, uint32_t key) {
  khash_t (iu64) * hash = NULL;
  uint32_t k = 0;
  uint64_t sum = 0;

  if (!ht_dates)
    return 0;

  /* *INDENT-OFF* */
  HT_SUM_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_CUMTS)))
      sum += get_iu64 (hash, key);
  });
  /* *INDENT-ON* */

  return sum;
}

/* Get the uint64_t value from MTRC_MAXTS given an uint32_t key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_maxts (GModule module, uint32_t key) {
  khash_t (iu64) * hash = NULL;
  uint32_t k = 0;
  uint64_t sum = 0;

  if (!ht_dates)
    return 0;

  /* *INDENT-OFF* */
  HT_SUM_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_MAXTS)))
      sum += get_iu64 (hash, key);
  });
  /* *INDENT-ON* */

  return sum;
}

/* Get the string value from MTRC_METHODS given an uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_method (GModule module, uint32_t key) {
  khash_t (is32) * hash = NULL;
  char *data = NULL;
  uint32_t k = 0;

  if (!ht_dates)
    return NULL;

  /* *INDENT-OFF* */
  HT_FIRST_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_METHODS)))
      if ((data = get_is32 (hash, key)))
        return data;
  });
  /* *INDENT-ON* */

  return NULL;
}

/* Get the string value from MTRC_PROTOCOLS given an uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_protocol (GModule module, uint32_t key) {
  khash_t (is32) * hash = NULL;
  char *data = NULL;
  uint32_t k = 0;

  if (!ht_dates)
    return NULL;

  /* *INDENT-OFF* */
  HT_FIRST_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_PROTOCOLS)))
      if ((data = get_is32 (hash, key)))
        return data;
  });
  /* *INDENT-ON* */

  return NULL;
}

/* Get the string value from ht_hostnames given a string key (IP).
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_hostname (const char *host) {
  khash_t (ss32) * hash = ht_hostnames;

  if (!hash)
    return NULL;

  return get_ss32 (hash, host);
}

/* Get the string value from ht_agent_vals (user agent) given an uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_host_agent_val (uint32_t key) {
  khash_t (is32) * hash = NULL;
  char *data = NULL;
  uint32_t k = 0;

  if (!ht_dates)
    return NULL;

  /* *INDENT-OFF* */
  HT_FIRST_VAL (ht_dates, k, {
    if ((hash = get_hash (-1, k, MTRC_AGENT_VALS)))
      if ((data = get_is32 (hash, key)))
        return data;
  });
  /* *INDENT-ON* */

  return NULL;
}

/* Get the list value from MTRC_AGENTS given an uint32_t key.
 *
 * On error, or if key is not found, NULL is returned.
 * On success the GSLList value for the given key is returned */
GSLList *
ht_get_host_agent_list (GModule module, uint32_t key) {
  GSLList *res = NULL, *list = NULL;
  khiter_t kv;
  khint_t k;
  khash_t (igsl) * hash = NULL;
  uint32_t date = 0;
  void *data = NULL;

  if (!ht_dates)
    return NULL;

  for (k = kh_begin (ht_dates); k != kh_end (ht_dates); ++k) {
    if (!kh_exist (ht_dates, k))
      continue;
    date = kh_key (ht_dates, k);
    if (!(hash = get_hash (module, date, MTRC_AGENTS)))
      continue;
    if ((kv = kh_get (igsl, hash, key)) == kh_end (hash))
      continue;

    list = kh_val (hash, kv);
    /* *INDENT-OFF* */
    GSLIST_FOREACH (list, data, {
      res = list_insert_prepend (res, i322ptr ((*(uint32_t *) data)));
    });
    /* *INDENT-ON* */
  }

  return res;
}

/* Get the meta data uint64_t from MTRC_METADATA given a string key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_meta_data (GModule module, const char *key) {
  khash_t (su64) * hash = NULL;
  uint32_t k = 0;
  uint64_t sum = 0;

  /* *INDENT-OFF* */
  HT_SUM_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_METADATA)))
      sum += get_su64 (hash, key);
  });
  /* *INDENT-ON* */

  return sum;
}

/* Set the maximum and minimum values found on an integer key and
 * integer value found on the MTRC_VISITORS hash structure.
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_hits_min_max (GModule module, uint32_t * min, uint32_t * max) {
  uint32_t k = 0;
  khash_t (ii32) * hash = NULL;

  /* *INDENT-OFF* */
  HT_SUM_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_HITS)))
      get_ii32_min_max (hash, min, max);
  });
  /* *INDENT-ON* */
}

/* Set the maximum and minimum values found on an integer key and
 * integer value found on the MTRC_VISITORS hash structure.
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_visitors_min_max (GModule module, uint32_t * min, uint32_t * max) {
  uint32_t k = 0;
  khash_t (ii32) * hash = NULL;

  /* *INDENT-OFF* */
  HT_SUM_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_VISITORS)))
      get_ii32_min_max (hash, min, max);
  });
  /* *INDENT-ON* */
}

/* Set the maximum and minimum values found on an integer key and
 * a uint64_t value found on the MTRC_BW hash structure.
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_bw_min_max (GModule module, uint64_t * min, uint64_t * max) {
  uint32_t k = 0;
  khash_t (iu64) * hash = NULL;

  /* *INDENT-OFF* */
  HT_SUM_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_BW)))
      get_iu64_min_max (hash, min, max);
  });
  /* *INDENT-ON* */
}

/* Set the maximum and minimum values found on an integer key and
 * a uint64_t value found on the MTRC_CUMTS hash structure.
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_cumts_min_max (GModule module, uint64_t * min, uint64_t * max) {
  uint32_t k = 0;
  khash_t (iu64) * hash = NULL;

  /* *INDENT-OFF* */
  HT_SUM_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_CUMTS)))
      get_iu64_min_max (hash, min, max);
  });
  /* *INDENT-ON* */
}

/* Set the maximum and minimum values found on an integer key and
 * a uint64_t value found on the MTRC_MAXTS hash structure.
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_maxts_min_max (GModule module, uint64_t * min, uint64_t * max) {
  uint32_t k = 0;
  khash_t (iu64) * hash = NULL;

  /* *INDENT-OFF* */
  HT_SUM_VAL (ht_dates, k, {
    if ((hash = get_hash (module, k, MTRC_MAXTS)))
      get_iu64_min_max (hash, min, max);
  });
  /* *INDENT-ON* */
}

uint32_t *
get_sorted_dates (void) {
  khiter_t key;
  uint32_t *dates = NULL;
  int i = 0;
  uint32_t size = 0;

  khash_t (igkh) * hash = ht_dates;
  if (!hash)
    return NULL;

  size = kh_size (hash);
  dates = xcalloc (size, sizeof (uint32_t));
  for (key = kh_begin (hash); key != kh_end (hash); ++key)
    if (kh_exist (hash, key))
      dates[i++] = kh_key (hash, key);
  qsort (dates, i, sizeof (uint32_t), cmp_ui32_asc);

  return dates;
}

int
invalidate_date (int date) {
  khash_t (igkh) * hash = ht_dates;
  khiter_t k;

  if (!hash)
    return -1;

  k = kh_get (igkh, hash, date);
  free_stores (kh_value (hash, k));
  kh_del (igkh, hash, k);

  return 0;
}

/* A wrapper to initialize a raw data structure.
 *
 * On success a GRawData structure is returned. */
static GRawData *
init_new_raw_data (GModule module, uint32_t ht_size) {
  GRawData *raw_data;

  raw_data = new_grawdata ();
  raw_data->idx = 0;
  raw_data->module = module;
  raw_data->size = ht_size;
  raw_data->items = new_grawdata_item (ht_size);

  return raw_data;
}

static int
ins_raw_num_data (GModule module, uint32_t date) {
  khiter_t k;
  khash_t (si32) * hash = get_hash (module, date, MTRC_KEYMAP);

  if (!hash)
    return -1;

  for (k = kh_begin (hash); k != kh_end (hash); ++k) {
    if (kh_exist (hash, k))
      ht_insert_keymap_list (kh_key (hash, k), kh_val (hash, k));
  }

  return 0;
}

static int
set_raw_num_data_from_date (GModule module) {
  khiter_t k;
  khash_t (igkh) * hash = ht_dates;

  if (!hash)
    return -1;

  /* iterate over the stored dates */
  for (k = kh_begin (hash); k != kh_end (hash); ++k) {
    if (kh_exist (hash, k))
      ins_raw_num_data (module, kh_key (hash, k));
  }

  return 0;
}

uint64_t
sum_u64_list (uint64_t (*cb) (GModule, uint32_t), GModule module,
              GSLList * list) {
  /* values can be uint64_t after exec the callback, e.g., bw */
  uint64_t n = 0;
  uint32_t *data = 0;

  if (!list)
    return 0;

  /* *INDENT-OFF* */
  GSLIST_FOREACH (list, data, {
    n += cb (module, (*(uint32_t *) data));
  });
  /* *INDENT-ON* */

  return n;
}

uint32_t
sum_u32_list (uint32_t (*cb) (GModule, uint32_t), GModule module,
              GSLList * list) {
  uint32_t n = 0;
  void *data = NULL;

  if (!list)
    return 0;

  /* *INDENT-OFF* */
  GSLIST_FOREACH (list, data, {
    n += cb (module, (*(uint32_t *) data));
  });
  /* *INDENT-ON* */

  return n;
}

/* Store the key/value pairs from a hash table into raw_data and sorts
 * the hits (numeric) value.
 *
 * On error, NULL is returned.
 * On success the GRawData sorted is returned */
static GRawData *
parse_raw_num_data (GModule module) {
  GRawData *raw_data;
  GSLList *list = NULL;
  khiter_t key;
  uint32_t ht_size = 0;

  khash_t (sgsl) * hash = ht_raw_hits;
  if (!hash)
    return NULL;

  ht_size = kh_size (hash);
  raw_data = init_new_raw_data (module, ht_size);

  for (key = kh_begin (hash); key != kh_end (hash); ++key) {
    if (!kh_exist (hash, key))
      continue;

    list = kh_value (hash, key);
    raw_data->items[raw_data->idx].keys = list;
    raw_data->items[raw_data->idx].hits =
      sum_u32_list (ht_get_hits, module, list);
    raw_data->idx++;
  }

  // sort new list
  sort_raw_num_data (raw_data, raw_data->idx);

  return raw_data;
}

/* Entry point to load the raw data from the data store into our
 * GRawData structure.
 *
 * On error, NULL is returned.
 * On success the GRawData sorted is returned */
GRawData *
parse_raw_data (GModule module) {
  GRawData *raw_data = NULL;

  if (set_raw_num_data_from_date (module) == 0)
    raw_data = parse_raw_num_data (module);
  return raw_data;
}

/* Initialize hash tables */
void
init_storage (void) {
  ht_hostnames = (khash_t (ss32) *) new_ss32_ht ();
  ht_dates = (khash_t (igkh) *) new_igkh_ht ();
  ht_seqs = (khash_t (si32) *) new_si32_ht ();
  ht_raw_hits = (khash_t (sgsl) *) new_sgsl_ht ();
  ht_cnt_overall = (khash_t (si32) *) new_si32_ht ();
  ht_last_parse = (khash_t (ii32) *) new_ii32_ht ();

  if (conf.restore)
    restore_data ();
}

/* Destroys the hash structure */
static void
des_igkh (void *h) {
  khint_t k;
  khash_t (igkh) * hash = h;

  if (!hash)
    return;

  for (k = kh_begin (hash); k != kh_end (hash); ++k) {
    if (!kh_exist (hash, k))
      continue;
    free_stores (kh_value (hash, k));
  }
  kh_destroy (igkh, hash);
}

/* Destroys the hash structure and its content */
void
free_storage (void) {
  if (conf.persist)
    persist_data ();

  des_igkh (ht_dates);
  des_si32_free (ht_seqs);
  des_ss32_free (ht_hostnames);
  des_sgsl_free (ht_raw_hits);

  des_si32_free (ht_cnt_overall);
  des_ii32 (ht_last_parse);
}
