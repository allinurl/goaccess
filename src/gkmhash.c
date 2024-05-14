/**
 * gkhash.c -- default hash table functions
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2024 Gerardo Orellana <hello @ goaccess.io>
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

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "gkmhash.h"

#include "error.h"
#include "gkhash.h"
#include "persistence.h"
#include "sort.h"
#include "util.h"
#include "xmalloc.h"

/* *INDENT-OFF* */
/* Per module - These metrics are not dated */
const GKHashMetric global_metrics[] = {
  { .metric.storem=MTRC_UNIQUE_KEYS , MTRC_TYPE_SI32 , new_si32_ht , des_si32_free , del_si32_free , 1 , NULL , "SI32_UNIQUE_KEYS.db" } ,
  { .metric.storem=MTRC_AGENT_KEYS  , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , del_ii32      , 0 , NULL , "II32_AGENT_KEYS.db"  } ,
  { .metric.storem=MTRC_AGENT_VALS  , MTRC_TYPE_IS32 , new_is32_ht , des_is32_free , del_is32_free , 1 , NULL , "IS32_AGENT_VALS.db"  } ,
  { .metric.storem=MTRC_CNT_VALID   , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , del_ii32      , 1 , NULL , "II32_CNT_VALID.db"   } ,
  { .metric.storem=MTRC_CNT_BW      , MTRC_TYPE_IU64 , new_iu64_ht , des_iu64      , del_iu64      , 1 , NULL , "IU64_CNT_BW.db"      } ,
};

/* Per module & per date */
const GKHashMetric module_metrics[] = {
  { .metric.storem=MTRC_KEYMAP    , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , del_ii32      , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_ROOTMAP   , MTRC_TYPE_IS32 , new_is32_ht , des_is32_free , del_is32_free , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_DATAMAP   , MTRC_TYPE_IS32 , new_is32_ht , des_is32_free , del_is32_free , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_UNIQMAP   , MTRC_TYPE_U648 , new_u648_ht , des_u648      , del_u648      , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_ROOT      , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , del_ii32      , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_HITS      , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , del_ii32      , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_VISITORS  , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , del_ii32      , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_BW        , MTRC_TYPE_IU64 , new_iu64_ht , des_iu64      , del_iu64      , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_CUMTS     , MTRC_TYPE_IU64 , new_iu64_ht , des_iu64      , del_iu64      , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_MAXTS     , MTRC_TYPE_IU64 , new_iu64_ht , des_iu64      , del_iu64      , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_METHODS   , MTRC_TYPE_II08 , new_ii08_ht , des_ii08      , del_ii08      , 0 , NULL , NULL } ,
  { .metric.storem=MTRC_PROTOCOLS , MTRC_TYPE_II08 , new_ii08_ht , des_ii08      , del_ii08      , 0 , NULL , NULL } ,
  { .metric.storem=MTRC_AGENTS    , MTRC_TYPE_IGSL , new_igsl_ht , des_igsl_free , del_igsl_free , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_METADATA  , MTRC_TYPE_SU64 , new_su64_ht , des_su64_free , del_su64_free , 1 , NULL , NULL } ,
};
const size_t module_metrics_len = ARRAY_SIZE (module_metrics);
const size_t global_metrics_len = ARRAY_SIZE (global_metrics);
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

/* Initialize a global hash structure.
 *
 * On success, a pointer to that hash structure is returned. */
static GKHashGlobal *
init_gkhashglobal (void) {
  GKHashGlobal *storage = NULL;

  int n = 0, i;

  storage = new_gkhglobal ();
  n = global_metrics_len;
  for (i = 0; i < n; i++) {
    storage->metrics[i] = global_metrics[i];
    storage->metrics[i].hash = global_metrics[i].alloc ();
  }

  return storage;
}

/* Initialize module metrics and mallocs its hash structure */
static void
init_tables (GModule module, GKHashModule *storage) {
  int n = 0, i;

  n = module_metrics_len;
  for (i = 0; i < n; i++) {
    storage[module].metrics[i] = module_metrics[i];
    storage[module].metrics[i].hash = module_metrics[i].alloc ();
  }
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

/* Destroys malloc'd global metrics */
static void
free_global_metrics (GKHashGlobal *ghash) {
  int i, n = 0;
  GKHashMetric mtrc;

  if (!ghash)
    return;

  n = global_metrics_len;
  for (i = 0; i < n; i++) {
    mtrc = ghash->metrics[i];
    mtrc.des (mtrc.hash, mtrc.free_data);
  }
}

/* Destroys malloc'd module metrics */
static void
free_module_metrics (GKHashModule *mhash, GModule module, uint8_t free_data) {
  int i, n = 0;
  GKHashMetric mtrc;

  if (!mhash)
    return;

  n = module_metrics_len;
  for (i = 0; i < n; i++) {
    mtrc = mhash[module].metrics[i];
    mtrc.des (mtrc.hash, free_data ? mtrc.free_data : 0);
  }
}

/* For each module metric, deletes all entries from the hash table */
static void
del_module_metrics (GKHashModule *mhash, GModule module, uint8_t free_data) {
  int i, n = 0;
  GKHashMetric mtrc;

  n = module_metrics_len;
  for (i = 0; i < n; i++) {
    mtrc = mhash[module].metrics[i];
    mtrc.del (mtrc.hash, free_data);
  }
}

/* Destroys all hash tables and possibly all the malloc'd data within */
static void
free_stores (GKHashStorage *store) {
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

/* Insert an uint32_t key (date) and a GKHashStorage payload
 *
 * On error, -1 is returned.
 * On key found, 1 is returned.
 * On success 0 is returned */
static int
ins_igkh (khash_t (igkh) *hash, uint32_t key) {
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

/* Given a hash and a key (date), get the relevant store
 *
 * On error or not found, NULL is returned.
 * On success, a pointer to that store is returned. */
static void *
get_store (khash_t (igkh) *hash, uint32_t key) {
  GKHashStorage *store = NULL;
  khint_t k;

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
get_hash_from_store (GKHashStorage *store, int module, GSMetric metric) {
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
void *
get_hash (int module, uint64_t key, GSMetric metric) {
  GKHashStorage *store = NULL;
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * hash = get_hdb (db, MTRC_DATES);

  if ((store = get_store (hash, key)) == NULL)
    return NULL;
  return get_hash_from_store (store, module, metric);
}

/* Given a module and a metric, get the cache hash table
 *
 * On success, a pointer to that hash table is returned. */
static void *
get_hash_from_cache (GModule module, GSMetric metric) {
  GKDB *db = get_db_instance (DB_INSTANCE);

  return db->cache[module].metrics[metric].hash;
}

GSLList *
ht_get_keymap_list_from_key (GModule module, uint32_t key) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  GSLList *list = NULL;
  khiter_t kv;
  khint_t k;
  khash_t (ii32) * hash = NULL;

  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);

  if (!dates)
    return NULL;

  for (k = kh_begin (dates); k != kh_end (dates); ++k) {
    if (!kh_exist (dates, k))
      continue;
    if (!(hash = get_hash (module, kh_key (dates, k), MTRC_KEYMAP)))
      continue;
    if ((kv = kh_get (ii32, hash, key)) == kh_end (hash))
      continue;
    list = list_insert_prepend (list, i322ptr (kh_val (hash, kv)));
  }

  return list;
}

/* Insert a unique visitor key string (IP/DATE/UA), mapped to an auto
 * incremented value.
 *
 * If the given key exists, its value is returned.
 * On error, 0 is returned.
 * On success the value of the key inserted is returned */
uint32_t
ht_insert_unique_key (uint32_t date, const char *key) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * seqs = get_hdb (db, MTRC_SEQS);
  khash_t (si32) * hash = get_hash (-1, date, MTRC_UNIQUE_KEYS);
  uint32_t val = 0;
  char *dupkey = NULL;

  if (!hash)
    return 0;

  if ((val = get_si32 (hash, key)) != 0)
    return val;

  dupkey = xstrdup (key);
  if ((val = ins_si32_inc (hash, dupkey, ht_ins_seq, seqs, "ht_unique_keys")) == 0)
    free (dupkey);
  return val;
}

/* Insert a user agent key string, mapped to an auto incremented value.
 *
 * If the given key exists, its value is returned.
 * On error, 0 is returned.
 * On success the value of the key inserted is returned */
uint32_t
ht_insert_agent_key (uint32_t date, uint32_t key) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * seqs = get_hdb (db, MTRC_SEQS);
  khash_t (ii32) * hash = get_hash (-1, date, MTRC_AGENT_KEYS);
  uint32_t val = 0;

  if (!hash)
    return 0;

  if ((val = get_ii32 (hash, key)) != 0)
    return val;

  return ins_ii32_inc (hash, key, ht_ins_seq, seqs, "ht_agent_keys");
}

/* Insert a user agent uint32_t key, mapped to a user agent string value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_agent_value (uint32_t date, uint32_t key, char *value) {
  khash_t (is32) * hash = get_hash (-1, date, MTRC_AGENT_VALS);
  char *dupval = NULL;

  if (!hash)
    return -1;

  if ((kh_get (is32, hash, key)) != kh_end (hash))
    return 0;

  dupval = xstrdup (value);
  if (ins_is32 (hash, key, dupval) != 0)
    free (dupval);
  return 0;
}

/* Insert a keymap string key.
 *
 * If the given key exists, its value is returned.
 * On error, 0 is returned.
 * On success the value of the key inserted is returned */
uint32_t
ht_insert_keymap (GModule module, uint32_t date, uint32_t key, uint32_t *ckey) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * seqs = get_hdb (db, MTRC_SEQS);
  khash_t (ii32) * hash = get_hash (module, date, MTRC_KEYMAP);
  khash_t (ii32) * cache = get_hash_from_cache (module, MTRC_KEYMAP);

  uint32_t val = 0;
  const char *modstr;

  if (!hash)
    return 0;

  if ((val = get_ii32 (hash, key)) != 0) {
    *ckey = get_ii32 (cache, key);
    return val;
  }

  modstr = get_module_str (module);
  if ((val = ins_ii32_inc (hash, key, ht_ins_seq, seqs, modstr)) == 0) {
    return val;
  }
  *ckey = ins_ii32_ai (cache, key);

  return val;
}

/* Insert a rootmap uint32_t key from the keymap store mapped to its string
 * value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_rootmap (GModule module, uint32_t date, uint32_t key, const char *value, uint32_t ckey) {
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

/* Insert a datamap uint32_t key and string value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_datamap (GModule module, uint32_t date, uint32_t key, const char *value, uint32_t ckey) {
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

  if (get_iu64 (cache, ckey) < value)
    ins_iu64 (cache, ckey, value);
  if (get_iu64 (hash, key) < value)
    ins_iu64 (hash, key, value);

  return 0;
}

/* Insert a method given an uint32_t key and string value.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
int
ht_insert_method (GModule module, uint32_t date, uint32_t key, const char *value, uint32_t ckey) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (ii08) * hash = get_hash (module, date, MTRC_METHODS);
  khash_t (ii08) * cache = get_hash_from_cache (module, MTRC_METHODS);
  khash_t (si08) * mtpr = get_hdb (db, MTRC_METH_PROTO);
  int ret = 0;
  uint8_t val = 0;

  if (!hash)
    return -1;

  if (!(val = get_si08 (mtpr, value)))
    return -1;

  if ((ret = ins_ii08 (hash, key, val)) == 0)
    ins_ii08 (cache, ckey, val);

  return ret;
}

/* Insert a protocol given an uint32_t key and string value.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
int
ht_insert_protocol (GModule module, uint32_t date, uint32_t key, const char *value, uint32_t ckey) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (ii08) * hash = get_hash (module, date, MTRC_PROTOCOLS);
  khash_t (ii08) * cache = get_hash_from_cache (module, MTRC_PROTOCOLS);
  khash_t (si08) * mtpr = get_hdb (db, MTRC_METH_PROTO);
  int ret = 0;
  uint8_t val = 0;

  if (!hash)
    return -1;

  if (!(val = get_si08 (mtpr, value)))
    return -1;

  if ((ret = ins_ii08 (hash, key, val)) == 0)
    ins_ii08 (cache, ckey, val);

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

int
ht_insert_date (uint32_t key) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * hash = get_hdb (db, MTRC_DATES);

  if (!hash)
    return -1;

  return ins_igkh (hash, key);
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

uint32_t
ht_sum_valid (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (ii32) * hash = NULL;
  uint32_t k = 0;
  uint32_t sum = 0;

  if (!dates)
    return 0;

  /* *INDENT-OFF* */
  HT_SUM_VAL (dates, k, {
    if ((hash = get_hash (-1, k, MTRC_CNT_VALID)))
      sum += get_ii32 (hash, 1);
  });
  /* *INDENT-ON* */

  return sum;
}

uint64_t
ht_sum_bw (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (iu64) * hash = NULL;
  uint32_t k = 0;
  uint64_t sum = 0;

  if (!dates)
    return 0;

  /* *INDENT-OFF* */
  HT_SUM_VAL (dates, k, {
    if ((hash = get_hash (-1, k, MTRC_CNT_BW)))
      sum += get_iu64 (hash, 1);
  });
  /* *INDENT-ON* */

  return sum;
}

/* Get the number of elements in a dates hash.
 *
 * Return 0 if the operation fails, else number of elements. */
uint32_t
ht_get_size_dates (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * hash = get_hdb (db, MTRC_DATES);

  if (!hash)
    return 0;

  return kh_size (hash);
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
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (u648) * hash = NULL;
  uint32_t k = 0;
  uint32_t sum = 0;

  if (!dates)
    return 0;

  /* *INDENT-OFF* */
  HT_SUM_VAL (dates, k, {
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

uint8_t
get_method_proto (const char *value) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si08) * mtpr = get_hdb (db, MTRC_METH_PROTO);
  uint8_t val = 0;

  if (!mtpr)
    return 0;

  if ((val = get_si08 (mtpr, value)) != 0)
    return val;
  return 0;
}

/* Get the string value from MTRC_METHODS given an uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_method (GModule module, uint32_t key) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (ii08) * cache = get_hash_from_cache (module, MTRC_METHODS);
  khash_t (si08) * mtpr = get_hdb (db, MTRC_METH_PROTO);
  uint8_t val = 0;
  khint_t k;

  if (!(val = get_ii08 (cache, key)))
    return NULL;

  for (k = kh_begin (mtpr); k != kh_end (mtpr); ++k) {
    if (kh_exist (mtpr, k) && kh_val (mtpr, k) == val)
      return xstrdup (kh_key (mtpr, k));
  }
  return NULL;
}

/* Get the string value from MTRC_PROTOCOLS given an uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_protocol (GModule module, uint32_t key) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (ii08) * cache = get_hash_from_cache (module, MTRC_PROTOCOLS);
  khash_t (si08) * mtpr = get_hdb (db, MTRC_METH_PROTO);
  uint8_t val = 0;
  khint_t k;

  if (!(val = get_ii08 (cache, key)))
    return NULL;

  for (k = kh_begin (mtpr); k != kh_end (mtpr); ++k) {
    if (kh_exist (mtpr, k) && kh_val (mtpr, k) == val)
      return xstrdup (kh_key (mtpr, k));
  }
  return NULL;
}

/* Get the string value from ht_agent_vals (user agent) given an uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_host_agent_val (uint32_t key) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (is32) * hash = NULL;
  char *data = NULL;
  uint32_t k = 0;

  if (!dates)
    return NULL;

  /* *INDENT-OFF* */
  HT_FIRST_VAL (dates, k, {
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
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);

  GSLList *res = NULL, *list = NULL;
  khiter_t kv;
  khint_t k;
  khash_t (igsl) * hash = NULL;
  void *data = NULL;

  if (!dates)
    return NULL;

  for (k = kh_begin (dates); k != kh_end (dates); ++k) {
    if (!kh_exist (dates, k))
      continue;
    if (!(hash = get_hash (module, kh_key (dates, k), MTRC_AGENTS)))
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
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (su64) * hash = NULL;
  uint32_t k = 0;
  uint64_t sum = 0;

  /* *INDENT-OFF* */
  HT_SUM_VAL (dates, k, {
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
ht_get_hits_min_max (GModule module, uint32_t *min, uint32_t *max) {
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
ht_get_visitors_min_max (GModule module, uint32_t *min, uint32_t *max) {
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
ht_get_bw_min_max (GModule module, uint64_t *min, uint64_t *max) {
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
ht_get_cumts_min_max (GModule module, uint64_t *min, uint64_t *max) {
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
ht_get_maxts_min_max (GModule module, uint64_t *min, uint64_t *max) {
  khash_t (iu64) * cache = get_hash_from_cache (module, MTRC_MAXTS);

  if (!cache)
    return;

  get_iu64_min_max (cache, min, max);
}

static void
destroy_date_stores (int date) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * hash = get_hdb (db, MTRC_DATES);
  khiter_t k;

  k = kh_get (igkh, hash, date);
  free_stores (kh_value (hash, k));
  kh_del (igkh, hash, k);
}

int
invalidate_date (int date) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * hash = get_hdb (db, MTRC_DATES);
  GModule module;
  size_t idx = 0;

  if (!hash)
    return -1;

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    del_module_metrics (db->cache, module, 0);
  }

  destroy_date_stores (date);

  return 0;
}

static uint32_t
ins_cache_map (GModule module, GSMetric metric, uint32_t key) {
  khash_t (ii32) * cache = get_hash_from_cache (module, metric);

  if (!cache)
    return 0;
  return ins_ii32_ai (cache, key);
}

static int
ins_cache_ii08 (GKHashStorage *store, GModule module, GSMetric metric, uint32_t key, uint32_t ckey) {
  khash_t (ii08) * hash = get_hash_from_store (store, module, metric);
  khash_t (ii08) * cache = get_hash_from_cache (module, metric);
  khint_t k;

  if ((k = kh_get (ii08, hash, key)) == kh_end (hash))
    return -1;
  return ins_ii08 (cache, ckey, kh_val (hash, k));
}

static int
ins_cache_is32 (GKHashStorage *store, GModule module, GSMetric metric, uint32_t key, uint32_t ckey) {
  khash_t (is32) * hash = get_hash_from_store (store, module, metric);
  khash_t (is32) * cache = get_hash_from_cache (module, metric);
  khint_t k;

  if ((k = kh_get (is32, hash, key)) == kh_end (hash))
    return -1;
  return ins_is32 (cache, ckey, kh_val (hash, k));
}

static int
inc_cache_ii32 (GKHashStorage *store, GModule module, GSMetric metric, uint32_t key, uint32_t ckey) {
  khash_t (ii32) * hash = get_hash_from_store (store, module, metric);
  khash_t (ii32) * cache = get_hash_from_cache (module, metric);
  khint_t k;

  if ((k = kh_get (ii32, hash, key)) == kh_end (hash))
    return -1;
  return inc_ii32 (cache, ckey, kh_val (hash, k));
}

static int
max_cache_iu64 (GKHashStorage *store, GModule module, GSMetric metric, uint32_t key, uint32_t ckey) {
  khash_t (iu64) * hash = get_hash_from_store (store, module, metric);
  khash_t (iu64) * cache = get_hash_from_cache (module, metric);
  khint_t k;

  if ((k = kh_get (iu64, hash, key)) == kh_end (hash))
    return -1;

  if (get_iu64 (cache, ckey) < kh_val (hash, k))
    return ins_iu64 (cache, ckey, kh_val (hash, k));
  return -1;
}

static int
inc_cache_iu64 (GKHashStorage *store, GModule module, GSMetric metric, uint32_t key, uint32_t ckey) {
  khash_t (iu64) * hash = get_hash_from_store (store, module, metric);
  khash_t (iu64) * cache = get_hash_from_cache (module, metric);
  khint_t k;

  if ((k = kh_get (iu64, hash, key)) == kh_end (hash))
    return -1;
  return inc_iu64 (cache, ckey, kh_val (hash, k));
}

static int
ins_raw_num_data (GModule module, uint32_t date) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * hash = get_hdb (db, MTRC_DATES);
  GKHashStorage *store = get_store (hash, date);
  khiter_t k, kr;
  uint32_t ckey = 0, rkey = 0, nrkey = 0;
  char *val = NULL;

  khash_t (ii32) * kmap = get_hash_from_store (store, module, MTRC_KEYMAP);
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
        nrkey = ins_cache_map (module, MTRC_KEYMAP, djb2 ((unsigned char *) val));
        ins_cache_is32 (store, module, MTRC_ROOTMAP, rkey, nrkey);
        ins_ii32 (cache, ckey, nrkey);
      }
    }

    ins_cache_is32 (store, module, MTRC_DATAMAP, kh_val (kmap, k), ckey);
    inc_cache_ii32 (store, module, MTRC_HITS, kh_val (kmap, k), ckey);
    inc_cache_ii32 (store, module, MTRC_VISITORS, kh_val (kmap, k), ckey);
    inc_cache_iu64 (store, module, MTRC_BW, kh_val (kmap, k), ckey);
    inc_cache_iu64 (store, module, MTRC_CUMTS, kh_val (kmap, k), ckey);
    max_cache_iu64 (store, module, MTRC_MAXTS, kh_val (kmap, k), ckey);
    ins_cache_ii08 (store, module, MTRC_METHODS, kh_val (kmap, k), ckey);
    ins_cache_ii08 (store, module, MTRC_PROTOCOLS, kh_val (kmap, k), ckey);
  }

  return 0;
}

static int
set_raw_num_data_date (GModule module) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * hash = get_hdb (db, MTRC_DATES);
  khiter_t k;

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

/* Initialize hash tables */
void
init_storage (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  db->cache = init_gkhashmodule ();

  if (conf.restore)
    restore_data ();
}

/* Destroys the hash structure */
void
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

void
free_cache (GKHashModule *cache) {
  GModule module;
  size_t idx = 0;

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    free_module_metrics (cache, module, 0);
  }
  free (cache);
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

#ifdef _DEBUG
  clock_t begin = clock ();
  double taken;
  const char *modstr = NULL;
  LOG_DEBUG (("== parse_raw_data ==\n"));
#endif

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

#ifdef _DEBUG
  modstr = get_module_str (module);
  taken = (double) (clock () - begin) / CLOCKS_PER_SEC;
  LOG_DEBUG (("== %-30s%f\n\n", modstr, taken));
#endif

  return raw_data;
}
