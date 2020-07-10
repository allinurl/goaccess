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
static khash_t (igkh) * ht_dates       = NULL;
static khash_t (si32) * ht_seqs        = NULL;
static khash_t (ss32) * ht_hostnames   = NULL;
static khash_t (si32) * ht_cnt_overall = NULL;
static khash_t (ii32) * ht_last_parse  = NULL;

static GKHashModule *cache_storage = NULL;
/* *INDENT-ON* */

/* Allocate memory for a new store container GKHashStorage instance.
 *
 * On success, the newly allocated GKHashStorage is returned . */
static GKHashStorage *
new_gkhstorage (void) {
  GKHashStorage *storage = xcalloc (1, sizeof (GKHashStorage));
  return storage;
}

/* Allocate memory for a new module GKHashModule instance.
 *
 * On success, the newly allocated GKHashStorage is returned . */
static GKHashModule *
new_gkhmodule (uint32_t size) {
  GKHashModule *storage = xcalloc (size, sizeof (GKHashModule));
  return storage;
}

/* Allocate memory for a new global GKHashGlobal instance.
 *
 * On success, the newly allocated GKHashGlobal is returned . */
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

/* Initialize a new uint32_t key - GKHashStorage value hash table */
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

/* Initialize a new uint64_t key - uint8_t value hash table */
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

/* Deletes an entry from the hash table and optionally the keys for a string
 * key - uint32_t value hash */
static void
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

/* Destroys both the hash structure and its string values */
static void
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

/* Deletes both the hash entry and its string values */
static void
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
des_ii32 (void *h, GO_UNUSED uint8_t free_data) {
  khash_t (ii32) * hash = h;
  if (!hash)
    return;
  kh_destroy (ii32, hash);
}

/* Deletes all entries from the hash table */
static void
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

/* Destroys the hash structure */
static void
des_u648 (void *h, GO_UNUSED uint8_t free_data) {
  khash_t (u648) * hash = h;
  if (!hash)
    return;
  kh_destroy (u648, hash);
}

/* Deletes all entries from the hash table */
static void
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
static void
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

/* Deletes all entries from the hash table and optionally frees its GSLList */
static void
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

/* Destroys both the hash structure and the keys for a
 * string key - uint64_t value hash */
static void
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

/* Deletes all entries from the hash table and optionally frees its string key */
static void
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

/* Destroys the hash structure */
static void
des_iu64 (void *h, GO_UNUSED uint8_t free_data) {
  khash_t (iu64) * hash = h;
  if (!hash)
    return;
  kh_destroy (iu64, hash);
}

/* Deletes all entries from the hash table */
static void
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

/* *INDENT-OFF* */
static const GKHashMetric global_metrics[] = {
  { MTRC_UNIQUE_KEYS, MTRC_TYPE_SI32 , new_si32_ht , des_si32_free , del_si32_free , NULL , "SI32_UNIQUE_KEYS.db" } ,
  { MTRC_AGENT_KEYS , MTRC_TYPE_SI32 , new_si32_ht , des_si32_free , del_si32_free , NULL , "SI32_AGENT_KEYS.db"  } ,
  { MTRC_AGENT_VALS , MTRC_TYPE_IS32 , new_is32_ht , des_is32_free , del_is32_free , NULL , "IS32_AGENT_VALS.db"  } ,
  { MTRC_CNT_VALID  , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , del_ii32      , NULL , "II32_CNT_VALID.db"   } ,
  { MTRC_CNT_BW     , MTRC_TYPE_IU64 , new_iu64_ht , des_iu64      , del_iu64      , NULL , "IU64_CNT_BW.db"      } ,
};

static GKHashMetric module_metrics[] = {
  { MTRC_KEYMAP     , MTRC_TYPE_SI32 , new_si32_ht , des_si32_free , del_si32_free , NULL , NULL}                   ,
  { MTRC_ROOTMAP    , MTRC_TYPE_IS32 , new_is32_ht , des_is32_free , del_is32_free , NULL , NULL}                   ,
  { MTRC_DATAMAP    , MTRC_TYPE_IS32 , new_is32_ht , des_is32_free , del_is32_free , NULL , NULL}                   ,
  { MTRC_UNIQMAP    , MTRC_TYPE_U648 , new_u648_ht , des_u648      , del_u648      , NULL , NULL}                   ,
  { MTRC_ROOT       , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , del_ii32      , NULL , NULL}                   ,
  { MTRC_HITS       , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , del_ii32      , NULL , NULL}                   ,
  { MTRC_VISITORS   , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , del_ii32      , NULL , NULL}                   ,
  { MTRC_BW         , MTRC_TYPE_IU64 , new_iu64_ht , des_iu64      , del_iu64      , NULL , NULL}                   ,
  { MTRC_CUMTS      , MTRC_TYPE_IU64 , new_iu64_ht , des_iu64      , del_iu64      , NULL , NULL}                   ,
  { MTRC_MAXTS      , MTRC_TYPE_IU64 , new_iu64_ht , des_iu64      , del_iu64      , NULL , NULL}                   ,
  { MTRC_METHODS    , MTRC_TYPE_IS32 , new_is32_ht , des_is32_free , del_is32_free , NULL , NULL}                   ,
  { MTRC_PROTOCOLS  , MTRC_TYPE_IS32 , new_is32_ht , des_is32_free , del_is32_free , NULL , NULL}                   ,
  { MTRC_AGENTS     , MTRC_TYPE_IGSL , new_igsl_ht , des_igsl_free , del_igsl_free , NULL , NULL}                   ,
  { MTRC_METADATA   , MTRC_TYPE_SU64 , new_su64_ht , des_su64_free , del_su64_free , NULL , NULL}                   ,
};
/* *INDENT-ON* */

/* Initialize module metrics and mallocs its hash structure */
static void
init_tables (GModule module, GKHashModule * storage) {
  int n = 0, i;

  n = ARRAY_SIZE (module_metrics);
  for (i = 0; i < n; i++) {
    storage[module].metrics[i] = module_metrics[i];
    storage[module].metrics[i].hash = module_metrics[i].alloc ();
  }
}

/* Destroys malloc'd global metrics */
static void
free_global_metrics (GKHashGlobal * ghash) {
  int i, n = 0;
  GKHashMetric mtrc;

  if (!ghash)
    return;

  n = ARRAY_SIZE (global_metrics);
  for (i = 0; i < n; i++) {
    mtrc = ghash->metrics[i];
    mtrc.des (mtrc.hash, 1);
  }
}

/* Destroys malloc'd mdule metrics */
static void
free_module_metrics (GKHashModule * mhash, GModule module, uint8_t free_data) {
  int i, n = 0;
  GKHashMetric mtrc;

  if (!mhash)
    return;

  n = ARRAY_SIZE (module_metrics);
  for (i = 0; i < n; i++) {
    mtrc = mhash[module].metrics[i];
    mtrc.des (mtrc.hash, free_data);
  }
}

/* For each module metric, deletes all entries from the hash table */
static void
del_module_metrics (GKHashModule * mhash, GModule module, uint8_t free_data) {
  int i, n = 0;
  GKHashMetric mtrc;

  n = ARRAY_SIZE (module_metrics);
  for (i = 0; i < n; i++) {
    mtrc = mhash[module].metrics[i];
    mtrc.del (mtrc.hash, free_data);
  }
}

/* Destroys all hash tables and possibly all the malloc'd data within */
static void
free_stores (GKHashStorage * store) {
  GModule module;
  size_t idx = 0;

  free_global_metrics (store->ghash);
  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    free_module_metrics (store->mhash, module, 1);
  }

  free (store->ghash);
  free (store->mhash);
  free (store);
}

/* Given a key (date), get the relevant store
 *
 * On error or not found, NULL is returned.
 * On success, a pointer to that store is returned. */
static void *
get_store (uint32_t key) {
  GKHashStorage *store = NULL;
  khint_t k;

  khash_t (igkh) * hash = ht_dates;

  k = kh_get (igkh, hash, key);
  /* key not found, return NULL */
  if (k == kh_end (hash))
    return NULL;

  store = kh_val (hash, k);
  return store;
}

/* Given a store, a module and the metric, get the hash table
 *
 * On error or not found, NULL is returned.
 * On success, a pointer to that hash table is returned. */
static void *
get_hash_from_store (GKHashStorage * store, int module, GSMetric metric) {
  int mtrc = 0, cnt = 0;
  if (!store)
    return NULL;

  if (module == -1) {
    mtrc = metric - MTRC_METADATA - 1;
    cnt = MTRC_CNT_BW - MTRC_UNIQUE_KEYS + 1;
    if (mtrc >= cnt) {
      LOG_DEBUG (("Out of bounds when attempting to get hash %d\n", metric));
      return NULL;
    }
  }

  /* ###NOTE: BE CAREFUL here, to avoid the almost unnecessary loop, we simply
   * use the index from the enum to make it O(1). The metrics array has to be
   * created in the same order as the GSMetric enum */
  if (module < 0)
    return store->ghash->metrics[mtrc].hash;
  return store->mhash[module].metrics[metric].hash;
}

/* Given a module a key (date) and the metric, get the hash table
 *
 * On error or not found, NULL is returned.
 * On success, a pointer to that hash table is returned. */
static void *
get_hash (int module, uint64_t key, GSMetric metric) {
  GKHashStorage *store = NULL;

  if ((store = get_store (key)) == NULL)
    return NULL;
  return get_hash_from_store (store, module, metric);
}

/* Given a module and a metric, get the cache hash table
 *
 * On success, a pointer to that hash table is returned. */
static void *
get_hash_from_cache (GModule module, GSMetric metric) {
  return cache_storage[module].metrics[metric].hash;
}

/* Initialize a global hash structure.
 *
 * On success, a pointer to that hash structure is returned. */
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

/* Initialize a module hash structure.
 *
 * On success, a pointer to that hash structure is returned. */
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

/* Insert an uint32_t key (date) and a GKHashStorage payload
 *
 * On error, -1 is returned.
 * On key found, 1 is returned.
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

/* Increment a string key and with the corresponding incremental uint32_t value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, 0 is returned.
 * On success or if the key exists, the value is returned */
static uint32_t
ins_si32_inc (khash_t (si32) * hash, const char *key, uint32_t (*cb) (const char *),
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
ins_is32 (khash_t (is32) * hash, uint32_t key, char *value) {
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
 * Note: If the key exists, the value is not replaced.
 *
 * On error or key exists, -1 is returned.
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

/* Insert a uint64_t key and a uint8_t value
 *
 * On error or key exists, -1 is returned.
 * On key exists, 1 is returned.
 * On success 0 is returned */
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
 * On error, 0 is returned.
 * On success the increased value is returned */
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

/* Insert a string key and auto increment int value.
 *
 * On error, 0 is returned.
 * On key found, the stored value is returned
 * On success the value of the key inserted is returned */
static uint32_t
ins_si32_ai (khash_t (si32) * hash, const char *key) {
  int size = 0, value = 0;
  int ret;
  khint_t k;

  if (!hash)
    return 0;

  size = kh_size (hash);
  /* the auto increment value starts at SIZE (hash table) + 1 */
  value = size > 0 ? size + 1 : 1;

  k = kh_put (si32, hash, key, &ret);
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
static int
ins_igsl (khash_t (igsl) * hash, uint32_t key, uint32_t value) {
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
  if (ret == -1)
    return -1;

  kh_val (hash, k) = list;

  return 0;
}

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

/* Get the database filename given a module and a metric.
 *
 * On error, a fatal error is triggered.
 * On success, the filename is returned */
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

/* Dump to disk the database file and frees its memory */
static void
close_tpl (tpl_node * tn, const char *fn) {
  tpl_dump (tn, TPL_FILE, fn);
  tpl_free (tn);
}

/* Given a database filename, restore a string key, uint32_t value back to the
 * storage */
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
  }
  free (key);
  tpl_free (tn);
}

/* Given a hash and a filename, persist to disk a string key, uint32_t value */
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

/* Given a database filename, restore a uint32_t key, uint32_t value back to
 * the storage */
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

/* Given a hash and a filename, persist to disk a uint32_t key, uint32_t value */
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

/* Given a database filename, restore a string key, uint32_t value back to
 * the storage */
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
    if ((ht_insert_date (date) == -1) || !(hash = get_hash (module, date, metric)))
      return 1;
    while (tpl_unpack (tn, 2) > 0) {
      ins_si32 (hash, key, val);
    }
  }
  free (key);
  tpl_free (tn);

  return 0;
}

/* Given a hash and a filename, persist to disk a string key, uint32_t value */
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

/* Given a database filename, restore a uint32_t key, string value back to
 * the storage */
static int
restore_is32 (GSMetric metric, const char *path, int module) {
  khash_t (is32) * hash = NULL;
  tpl_node *tn;
  char fmt[] = "A(iA(us))";
  int date = 0;
  uint32_t key = 0;
  char *val = NULL, *dupval = NULL;

  if (!(tn = tpl_map (fmt, &date, &key, &val)))
    return 1;

  tpl_load (tn, TPL_FILE, path);
  while (tpl_unpack (tn, 1) > 0) {
    /* if operation fails */
    if ((ht_insert_date (date) == -1) || !(hash = get_hash (module, date, metric)))
      return 1;
    while (tpl_unpack (tn, 2) > 0) {
      dupval = xstrdup (val);
      if (ins_is32 (hash, key, dupval) != 0)
        free (dupval);
    }
  }
  free (val);
  tpl_free (tn);

  return 0;
}

/* Given a hash and a filename, persist to disk a uint32_t key, string value */
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

/* Given a database filename, restore a uint32_t key, uint32_t value back to
 * the storage */
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
    if ((ht_insert_date (date) == -1) || !(hash = get_hash (module, date, metric)))
      return 1;
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

/* Given a database filename, restore a uint64_t key, uint8_t value back to
 * the storage */
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
    if ((ht_insert_date (date) == -1) || !(hash = get_hash (module, date, metric)))
      return 1;
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

/* Given a database filename, restore a uint32_t key, uint64_t value back to
 * the storage */
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
    if ((ht_insert_date (date) == -1) || !(hash = get_hash (module, date, metric)))
      return 1;
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

/* Given a database filename, restore a string key, uint64_t value back to
 * the storage */
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
    if ((ht_insert_date (date) == -1) || !(hash = get_hash (module, date, metric)))
      return 1;
    while (tpl_unpack (tn, 2) > 0) {
      ins_su64 (hash, key, val);
    }
  }
  free (key);
  tpl_free (tn);

  return 0;
}

/* Given a hash and a filename, persist to disk a string key, uint64_t value */
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

/* Given a database filename, restore a uint32_t key, GSLList value back to the
 * storage */
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
    if ((ht_insert_date (date) == -1) || !(hash = get_hash (module, date, metric)))
      return 1;
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

GSLList *
ht_get_keymap_list_from_key (GModule module, const char *key) {
  GSLList *list = NULL;
  khiter_t kv;
  khint_t k;
  khash_t (si32) * hash = NULL;

  if (!ht_dates)
    return NULL;

  for (k = kh_begin (ht_dates); k != kh_end (ht_dates); ++k) {
    if (!kh_exist (ht_dates, k))
      continue;
    if (!(hash = get_hash (module, kh_key (ht_dates, k), MTRC_KEYMAP)))
      continue;
    if ((kv = kh_get (si32, hash, key)) == kh_end (hash))
      continue;
    list = list_insert_prepend (list, i322ptr (kh_val (hash, kv)));
  }

  return list;
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
      *min = curvalue;
    if (curvalue > *max)
      *max = curvalue;
    if (curvalue < *min)
      *min = curvalue;
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
ht_get_processing_time (void) {
  khash_t (si32) * hash = ht_cnt_overall;

  if (!hash)
    return 0;

  return get_si32 (hash, "processing_time");
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


/* Encode a data key and a unique visitor's key to a new uint64_t key
  *
  * ###NOTE: THIS LIMITS THE MAX VALUE OF A DATA TABLE TO uint32_t
  * WILL NEED TO CHANGE THIS IF WE GO OVER uint32_t
  */
static uint64_t
u64encode (uint32_t x, uint32_t y) {
  return x > y ? (uint32_t) y | ((uint64_t) x << 32) : (uint32_t) x | ((uint64_t) y << 32);
}

/* Decode a uint64_t number into the original two uint32_t  */
void
u64decode (uint64_t n, uint32_t * x, uint32_t * y) {
  *x = (uint64_t) n >> 32;
  *y = (uint64_t) n & 0xFFFFFFFF;
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
  char *dupkey = NULL;
  khash_t (si32) * hash = get_hash (-1, date, MTRC_UNIQUE_KEYS);

  if (!hash)
    return 0;

  if ((val = get_si32 (hash, key)) != 0)
    return val;

  dupkey = xstrdup (key);
  if ((val = ins_si32_inc (hash, dupkey, ht_ins_seq, "ht_unique_keys")) == 0)
    free (dupkey);
  return val;
}

/* Insert a user agent key string, mapped to an auto incremented value.
 *
 * If the given key exists, its value is returned.
 * On error, 0 is returned.
 * On success the value of the key inserted is returned */
uint32_t
ht_insert_agent_key (uint32_t date, const char *key) {
  char *dupkey = NULL;
  uint32_t val = 0;
  khash_t (si32) * hash = get_hash (-1, date, MTRC_AGENT_KEYS);

  if (!hash)
    return 0;

  if ((val = get_si32 (hash, key)) != 0)
    return val;

  dupkey = xstrdup (key);
  if ((val = ins_si32_inc (hash, dupkey, ht_ins_seq, "ht_agent_keys")) == 0)
    free (dupkey);
  return val;
}

/* Insert a user agent uint32_t key, mapped to a user agent string value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_agent_value (uint32_t date, uint32_t key, const char *value) {
  khash_t (is32) * hash = get_hash (-1, date, MTRC_AGENT_VALS);
  char *dupval = NULL;
  int ret = 0;

  if (!hash)
    return -1;

  dupval = xstrdup (value);
  if ((ret = ins_is32 (hash, key, dupval)) != 0)
    free (dupval);
  return ret;
}

/* Insert a keymap string key.
 *
 * If the given key exists, its value is returned.
 * On error, 0 is returned.
 * On success the value of the key inserted is returned */
uint32_t
ht_insert_keymap (GModule module, uint32_t date, const char *key, uint32_t * ckey) {
  khash_t (si32) * hash = get_hash (module, date, MTRC_KEYMAP);
  khash_t (si32) * cache = get_hash_from_cache (module, MTRC_KEYMAP);

  uint32_t val = 0;
  char *modstr = NULL, *dupkey = NULL;

  if (!hash)
    return 0;

  if ((val = get_si32 (hash, key)) != 0) {
    *ckey = get_si32 (cache, key);
    return val;
  }

  modstr = get_module_str (module);
  dupkey = xstrdup (key);

  if ((val = ins_si32_inc (hash, dupkey, ht_ins_seq, modstr)) == 0) {
    free (dupkey);
    free (modstr);
    return val;
  }
  *ckey = ins_si32_ai (cache, dupkey);
  free (modstr);

  return val;
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
ht_insert_datamap (GModule module, uint32_t date, uint32_t key, const char *value,
                   uint32_t ckey) {
  khash_t (is32) * hash = get_hash (module, date, MTRC_DATAMAP);
  khash_t (is32) * cache = get_hash_from_cache (module, MTRC_DATAMAP);
  char *dupval = NULL;
  int ret = 0;

  if (!hash)
    return -1;

  dupval = xstrdup (value);
  if ((ret = ins_is32 (hash, key, dupval)) == 0)
    ins_is32 (cache, ckey, dupval);
  else
    free (dupval);

  return ret;
}

/* Insert a rootmap uint32_t key from the keymap store mapped to its string
 * value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_rootmap (GModule module, uint32_t date, uint32_t key, const char *value,
                   uint32_t ckey) {
  khash_t (is32) * hash = get_hash (module, date, MTRC_ROOTMAP);
  khash_t (is32) * cache = get_hash_from_cache (module, MTRC_ROOTMAP);
  char *dupval = NULL;
  int ret = 0;

  if (!hash)
    return -1;

  dupval = xstrdup (value);
  if ((ret = ins_is32 (hash, key, dupval)) == 0)
    ins_is32 (cache, ckey, dupval);
  else
    free (dupval);

  return ret;
}

/* Insert a data uint32_t key mapped to the corresponding uint32_t root key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_root (GModule module, uint32_t date, uint32_t key, uint32_t value, uint32_t dkey,
                uint32_t rkey) {
  khash_t (ii32) * hash = get_hash (module, date, MTRC_ROOT);
  khash_t (ii32) * cache = get_hash_from_cache (module, MTRC_ROOT);

  if (!hash)
    return -1;

  ins_ii32 (cache, dkey, rkey);
  return ins_ii32 (hash, key, value);
}

/* Increases hits counter from a uint32_t key.
 *
 * On error, 0 is returned.
 * On success the inserted value is returned */
uint32_t
ht_insert_hits (GModule module, uint32_t date, uint32_t key, uint32_t inc, uint32_t ckey) {
  khash_t (ii32) * hash = get_hash (module, date, MTRC_HITS);
  khash_t (ii32) * cache = get_hash_from_cache (module, MTRC_HITS);

  if (!hash)
    return 0;

  inc_ii32 (cache, ckey, inc);
  return inc_ii32 (hash, key, inc);
}

/* Increases visitors counter from a uint32_t key.
 *
 * On error, 0 is returned.
 * On success the inserted value is returned */
uint32_t
ht_insert_visitor (GModule module, uint32_t date, uint32_t key, uint32_t inc, uint32_t ckey) {
  khash_t (ii32) * hash = get_hash (module, date, MTRC_VISITORS);
  khash_t (ii32) * cache = get_hash_from_cache (module, MTRC_VISITORS);

  if (!hash)
    return 0;

  inc_ii32 (cache, ckey, inc);
  return inc_ii32 (hash, key, inc);
}

/* Increases bandwidth counter from a uint32_t key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_bw (GModule module, uint32_t date, uint32_t key, uint64_t inc, uint32_t ckey) {
  khash_t (iu64) * hash = get_hash (module, date, MTRC_BW);
  khash_t (iu64) * cache = get_hash_from_cache (module, MTRC_BW);

  if (!hash)
    return -1;

  inc_iu64 (cache, ckey, inc);
  return inc_iu64 (hash, key, inc);
}

/* Increases cumulative time served counter from a uint32_t key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_cumts (GModule module, uint32_t date, uint32_t key, uint64_t inc, uint32_t ckey) {
  khash_t (iu64) * hash = get_hash (module, date, MTRC_CUMTS);
  khash_t (iu64) * cache = get_hash_from_cache (module, MTRC_CUMTS);

  if (!hash)
    return -1;

  inc_iu64 (cache, ckey, inc);
  return inc_iu64 (hash, key, inc);
}

/* Insert the maximum time served counter from a uint32_t key.
 * Note: it compares the current value with the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_maxts (GModule module, uint32_t date, uint32_t key, uint64_t value, uint32_t ckey) {
  khash_t (iu64) * hash = get_hash (module, date, MTRC_MAXTS);
  khash_t (iu64) * cache = get_hash_from_cache (module, MTRC_MAXTS);

  if (!hash)
    return -1;

  if (get_iu64 (hash, key) < value) {
    ins_iu64 (cache, ckey, value);
    ins_iu64 (hash, key, value);
  }

  return 0;
}

/* Insert a method given an uint32_t key and string value.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
int
ht_insert_method (GModule module, uint32_t date, uint32_t key, const char *value,
                  uint32_t ckey) {
  khash_t (is32) * hash = get_hash (module, date, MTRC_METHODS);
  khash_t (is32) * cache = get_hash_from_cache (module, MTRC_METHODS);
  char *dupval = NULL;
  int ret = 0;

  if (!hash)
    return -1;

  dupval = xstrdup (value);
  if ((ret = ins_is32 (hash, key, dupval)) == 0)
    ins_is32 (cache, ckey, dupval);
  else
    free (dupval);

  return ret;
}

/* Insert a protocol given an uint32_t key and string value.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
int
ht_insert_protocol (GModule module, uint32_t date, uint32_t key, const char *value,
                    uint32_t ckey) {
  khash_t (is32) * hash = get_hash (module, date, MTRC_PROTOCOLS);
  khash_t (is32) * cache = get_hash_from_cache (module, MTRC_PROTOCOLS);
  char *dupval = NULL;
  int ret = 0;

  if (!hash)
    return -1;

  dupval = xstrdup (value);
  if ((ret = ins_is32 (hash, key, dupval)) == 0)
    ins_is32 (cache, ckey, dupval);
  else
    free (dupval);

  return ret;
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
ht_insert_meta_data (GModule module, uint32_t date, const char *key, uint64_t value) {
  khash_t (su64) * hash = get_hash (module, date, MTRC_METADATA);

  if (!hash)
    return -1;

  return inc_su64 (hash, key, value);
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
  khash_t (is32) * cache = get_hash_from_cache (module, MTRC_DATAMAP);

  if (!cache)
    return 0;

  return kh_size (cache);
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
  khash_t (is32) * cache = get_hash_from_cache (module, MTRC_DATAMAP);

  if (!cache)
    return NULL;

  return get_is32 (cache, key);
}

/* Get the string root from MTRC_ROOTMAP given an uint32_t data key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_root (GModule module, uint32_t key) {
  int root_key = 0;
  khash_t (ii32) * hashroot = get_hash_from_cache (module, MTRC_ROOT);
  khash_t (is32) * hashrootmap = get_hash_from_cache (module, MTRC_ROOTMAP);

  if (!hashroot || !hashrootmap)
    return NULL;

  /* not found */
  if ((root_key = get_ii32 (hashroot, key)) == 0)
    return NULL;

  return get_is32 (hashrootmap, root_key);
}


/* Get the int visitors value from MTRC_VISITORS given an int key.
 *
 * If key is not found, 0 is returned.
 * On error, -1 is returned.
 * On success the int value for the given key is returned */
uint32_t
ht_get_hits (GModule module, int key) {
  khash_t (ii32) * cache = get_hash_from_cache (module, MTRC_HITS);

  if (!cache)
    return 0;

  return get_ii32 (cache, key);
}

/* Get the uint32_t visitors value from MTRC_VISITORS given an uint32_t key.
 *
 * If key is not found, 0 is returned.
 * On error, -1 is returned.
 * On success the uint32_t value for the given key is returned */
uint32_t
ht_get_visitors (GModule module, uint32_t key) {
  khash_t (ii32) * cache = get_hash_from_cache (module, MTRC_VISITORS);

  if (!cache)
    return 0;

  return get_ii32 (cache, key);
}

/* Get the uint64_t value from MTRC_BW given an uint32_t key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_bw (GModule module, uint32_t key) {
  khash_t (iu64) * cache = get_hash_from_cache (module, MTRC_BW);

  if (!cache)
    return 0;

  return get_iu64 (cache, key);
}

/* Get the uint64_t value from MTRC_CUMTS given an uint32_t key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_cumts (GModule module, uint32_t key) {
  khash_t (iu64) * cache = get_hash_from_cache (module, MTRC_CUMTS);

  if (!cache)
    return 0;

  return get_iu64 (cache, key);
}

/* Get the uint64_t value from MTRC_MAXTS given an uint32_t key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_maxts (GModule module, uint32_t key) {
  khash_t (iu64) * cache = get_hash_from_cache (module, MTRC_MAXTS);

  if (!cache)
    return 0;

  return get_iu64 (cache, key);
}

/* Get the string value from MTRC_METHODS given an uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_method (GModule module, uint32_t key) {
  khash_t (is32) * cache = get_hash_from_cache (module, MTRC_METHODS);

  if (!cache)
    return NULL;

  return get_is32 (cache, key);
}

/* Get the string value from MTRC_PROTOCOLS given an uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_protocol (GModule module, uint32_t key) {
  khash_t (is32) * cache = get_hash_from_cache (module, MTRC_PROTOCOLS);

  if (!cache)
    return NULL;

  return get_is32 (cache, key);
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
  void *data = NULL;

  if (!ht_dates)
    return NULL;

  for (k = kh_begin (ht_dates); k != kh_end (ht_dates); ++k) {
    if (!kh_exist (ht_dates, k))
      continue;
    if (!(hash = get_hash (module, kh_key (ht_dates, k), MTRC_AGENTS)))
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

uint32_t
ht_get_keymap (GModule module, const char *key) {
  khash_t (si32) * cache = get_hash_from_cache (module, MTRC_KEYMAP);

  if (!cache)
    return 0;

  return get_si32 (cache, key);
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
  khash_t (ii32) * cache = get_hash_from_cache (module, MTRC_HITS);

  if (!cache)
    return;

  get_ii32_min_max (cache, min, max);
}

/* Set the maximum and minimum values found on an integer key and
 * integer value found on the MTRC_VISITORS hash structure.
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_visitors_min_max (GModule module, uint32_t * min, uint32_t * max) {
  khash_t (ii32) * cache = get_hash_from_cache (module, MTRC_VISITORS);

  if (!cache)
    return;

  get_ii32_min_max (cache, min, max);
}

/* Set the maximum and minimum values found on an integer key and
 * a uint64_t value found on the MTRC_BW hash structure.
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_bw_min_max (GModule module, uint64_t * min, uint64_t * max) {
  khash_t (iu64) * cache = get_hash_from_cache (module, MTRC_BW);

  if (!cache)
    return;

  get_iu64_min_max (cache, min, max);
}

/* Set the maximum and minimum values found on an integer key and
 * a uint64_t value found on the MTRC_CUMTS hash structure.
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_cumts_min_max (GModule module, uint64_t * min, uint64_t * max) {
  khash_t (iu64) * cache = get_hash_from_cache (module, MTRC_CUMTS);

  if (!cache)
    return;

  get_iu64_min_max (cache, min, max);
}

/* Set the maximum and minimum values found on an integer key and
 * a uint64_t value found on the MTRC_MAXTS hash structure.
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_maxts_min_max (GModule module, uint64_t * min, uint64_t * max) {
  khash_t (iu64) * cache = get_hash_from_cache (module, MTRC_MAXTS);

  if (!cache)
    return;

  get_iu64_min_max (cache, min, max);
}

uint32_t *
get_sorted_dates (uint32_t * len) {
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
  *len = i;

  return dates;
}

int
invalidate_date (int date) {
  khash_t (igkh) * hash = ht_dates;
  khiter_t k;
  GModule module;
  size_t idx = 0;

  if (!hash)
    return -1;

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    del_module_metrics (cache_storage, module, 0);
  }

  k = kh_get (igkh, hash, date);
  free_stores (kh_value (hash, k));
  kh_del (igkh, hash, k);

  return 0;
}

static uint32_t
ins_cache_map (GModule module, GSMetric metric, const char *key) {
  khash_t (si32) * cache = get_hash_from_cache (module, metric);

  if (!cache)
    return 0;
  return ins_si32_ai (cache, key);
}

static int
ins_cache_is32 (GKHashStorage * store, GModule module, GSMetric metric, uint32_t key,
                uint32_t ckey) {
  khash_t (is32) * hash = get_hash_from_store (store, module, metric);
  khash_t (is32) * cache = get_hash_from_cache (module, metric);
  khint_t k;

  if ((k = kh_get (is32, hash, key)) == kh_end (hash))
    return -1;
  return ins_is32 (cache, ckey, kh_val (hash, k));
}

static int
inc_cache_ii32 (GKHashStorage * store, GModule module, GSMetric metric, uint32_t key,
                uint32_t ckey) {
  khash_t (ii32) * hash = get_hash_from_store (store, module, metric);
  khash_t (ii32) * cache = get_hash_from_cache (module, metric);
  khint_t k;

  if ((k = kh_get (ii32, hash, key)) == kh_end (hash))
    return -1;
  return inc_ii32 (cache, ckey, kh_val (hash, k));
}

static int
inc_cache_iu64 (GKHashStorage * store, GModule module, GSMetric metric, uint32_t key,
                uint32_t ckey) {
  khash_t (iu64) * hash = get_hash_from_store (store, module, metric);
  khash_t (iu64) * cache = get_hash_from_cache (module, metric);
  khint_t k;

  if ((k = kh_get (iu64, hash, key)) == kh_end (hash))
    return -1;
  return inc_iu64 (cache, ckey, kh_val (hash, k));
}

static int
ins_raw_num_data (GModule module, uint32_t date) {
  khiter_t k, kr;
  GKHashStorage *store = get_store (date);
  uint32_t ckey = 0, rkey = 0, nrkey = 0;
  char *val = NULL;

  khash_t (si32) * kmap = get_hash_from_store (store, module, MTRC_KEYMAP);
  khash_t (ii32) * root = get_hash_from_store (store, module, MTRC_ROOT);
  khash_t (is32) * rmap = get_hash_from_store (store, module, MTRC_ROOTMAP);
  khash_t (ii32) * cache = get_hash_from_cache (module, MTRC_ROOT);

  if (!kmap)
    return -1;

  for (k = kh_begin (kmap); k != kh_end (kmap); ++k) {
    if (!kh_exist (kmap, k))
      continue;
    if ((ckey = ins_cache_map (module, MTRC_KEYMAP, kh_key (kmap, k))) == 0)
      continue;

    if ((rkey = get_ii32 (root, kh_val (kmap, k)))) {
      kr = kh_get (is32, rmap, rkey);
      if (kr != kh_end (rmap) && (val = kh_val (rmap, kr))) {
        nrkey = ins_cache_map (module, MTRC_KEYMAP, val);
        ins_cache_is32 (store, module, MTRC_ROOTMAP, rkey, nrkey);
        ins_ii32 (cache, ckey, nrkey);
      }
    }

    ins_cache_is32 (store, module, MTRC_DATAMAP, kh_val (kmap, k), ckey);
    inc_cache_ii32 (store, module, MTRC_HITS, kh_val (kmap, k), ckey);
    inc_cache_ii32 (store, module, MTRC_VISITORS, kh_val (kmap, k), ckey);
    inc_cache_iu64 (store, module, MTRC_BW, kh_val (kmap, k), ckey);
    inc_cache_iu64 (store, module, MTRC_CUMTS, kh_val (kmap, k), ckey);
    inc_cache_iu64 (store, module, MTRC_MAXTS, kh_val (kmap, k), ckey);
    ins_cache_is32 (store, module, MTRC_METHODS, kh_val (kmap, k), ckey);
    ins_cache_is32 (store, module, MTRC_PROTOCOLS, kh_val (kmap, k), ckey);
  }

  return 0;
}

static int
set_raw_num_data_date (GModule module) {
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

int
rebuild_rawdata_cache (void) {
  GModule module;
  size_t idx = 0;

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    set_raw_num_data_date (module);
  }

  return 2;
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

static GRawData *
get_u32_raw_data (GModule module) {
  khash_t (ii32) * hash = get_hash_from_cache (module, MTRC_HITS);
  GRawData *raw_data;
  khiter_t key;
  uint32_t ht_size = 0;

  if (!hash)
    return NULL;

  ht_size = kh_size (hash);
  raw_data = init_new_raw_data (module, ht_size);
  raw_data->type = U32;

  for (key = kh_begin (hash); key != kh_end (hash); ++key) {
    if (!kh_exist (hash, key))
      continue;
    raw_data->items[raw_data->idx].nkey = kh_key (hash, key);
    raw_data->items[raw_data->idx].hits = kh_val (hash, key);
    raw_data->idx++;
  }

  return raw_data;
}

/* Store the key/value pairs from a hash table into raw_data and sorts
 * the hits (numeric) value.
 *
 * On error, NULL is returned.
 * On success the GRawData sorted is returned */
static GRawData *
get_str_raw_data (GModule module) {
  khash_t (is32) * hash = get_hash_from_cache (module, MTRC_DATAMAP);
  GRawData *raw_data;
  khiter_t key;
  uint32_t ht_size = 0;

  if (!hash)
    return NULL;

  ht_size = kh_size (hash);
  raw_data = init_new_raw_data (module, ht_size);
  raw_data->type = STR;

  for (key = kh_begin (hash); key != kh_end (hash); ++key) {
    if (!kh_exist (hash, key))
      continue;
    raw_data->items[raw_data->idx].nkey = kh_key (hash, key);
    raw_data->items[raw_data->idx].data = kh_val (hash, key);
    raw_data->idx++;
  }

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
  clock_t begin = clock ();
  double taken;
  char *modstr = NULL;

  switch (module) {
  case VISITORS:
    raw_data = get_str_raw_data (module);
    if (raw_data)
      sort_raw_str_data (raw_data, raw_data->idx);
    break;
  default:
    raw_data = get_u32_raw_data (module);
    if (raw_data)
      sort_raw_num_data (raw_data, raw_data->idx);
  }

  modstr = get_module_str (module);
  taken = (double) (clock () - begin) / CLOCKS_PER_SEC;
  LOG_DEBUG (("== %s\t\t\t%f\n", modstr, taken));
  free (modstr);

  return raw_data;
}

/* Initialize hash tables */
void
init_storage (void) {
  /* *INDENT-OFF* */
  ht_hostnames   = (khash_t (ss32) *) new_ss32_ht ();
  ht_dates       = (khash_t (igkh) *) new_igkh_ht ();
  ht_seqs        = (khash_t (si32) *) new_si32_ht ();
  ht_cnt_overall = (khash_t (si32) *) new_si32_ht ();
  ht_last_parse  = (khash_t (ii32) *) new_ii32_ht ();
  /* *INDENT-ON* */

  cache_storage = init_gkhashmodule ();

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
  GModule module;
  size_t idx = 0;

  if (conf.persist)
    persist_data ();

  des_igkh (ht_dates);
  des_si32_free (ht_seqs, 1);
  des_ss32_free (ht_hostnames, 1);

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    free_module_metrics (cache_storage, module, 0);
  }
  free (cache_storage);

  des_si32_free (ht_cnt_overall, 1);
  des_ii32 (ht_last_parse, 1);
}
