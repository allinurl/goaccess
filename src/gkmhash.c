/**
 * gkmhash.c -- module metric hash tables
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

/* Number of cache entries to allocate when a module cache first grows. Kept
 * small since arrays double on growth and most modules hold few entries. */
#define CACHE_INIT_CAPACITY 256

/* Per-module cache backed by dense arrays indexed by cache key (ckey).
 * ckeys are auto-incremented starting at 1, so every metric can live in a
 * plain array instead of a hash table; only the data-hash to ckey keymap
 * requires an actual hash table. String values are borrowed from the dated
 * stores and are never owned by the cache. */
struct GKCacheModule_ {
  khash_t (ii32) * keymap;      /* data hash -> ckey */
  char **datamap;               /* data ckey -> data string */
  char **rootmap;               /* root ckey -> root string */
  uint32_t *root;               /* data ckey -> root ckey */
  uint32_t *hits;
  uint32_t *visitors;
  uint64_t *bw;
  uint64_t *cumts;
  uint64_t *maxts;
  uint8_t *meth;
  uint8_t *proto;
  uint32_t size;                /* highest assigned ckey */
  uint32_t capacity;            /* allocated entries per metric array */
  uint32_t datamap_size;        /* number of ckeys holding a data string */
  uint8_t has_bw;               /* bw metrics have been recorded */
  uint8_t has_cumts;            /* cumts metrics have been recorded */
};

/* *INDENT-OFF* */
/* Per module - These metrics are not dated */
const GKHashMetric global_metrics[] = {
  { .metric.storem=MTRC_UNIQUE_KEYS  , MTRC_TYPE_U6432 , new_u6432_ht , des_u6432    , del_u6432     , 0 , NULL , "U6432_UNIQUE_KEYS.db" } ,
  { .metric.storem=MTRC_AGENT_KEYS   , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , del_ii32      , 0 , NULL , "II32_AGENT_KEYS.db"   } ,
  { .metric.storem=MTRC_AGENT_VALS   , MTRC_TYPE_IS32 , new_is32_ht , des_is32_free , del_is32_free , 1 , NULL , "IS32_AGENT_VALS.db"   } ,
  { .metric.storem=MTRC_CNT_VALID    , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , del_ii32      , 1 , NULL , "II32_CNT_VALID.db"    } ,
  { .metric.storem=MTRC_CNT_BW       , MTRC_TYPE_IU64 , new_iu64_ht , des_iu64      , del_iu64      , 1 , NULL , "IU64_CNT_BW.db"       } ,
  { .metric.storem=MTRC_CNT_VISITORS , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , del_ii32      , 1 , NULL , "II32_CNT_VISITORS.db" } ,
};

/* Per module & per date - The order must match the GSMetric enum */
const GKHashMetric module_metrics[] = {
  { .metric.storem=MTRC_KEYMAP   , MTRC_TYPE_II32 , new_ii32_ht , des_ii32      , del_ii32      , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_ROOTMAP  , MTRC_TYPE_IS32 , new_is32_ht , des_is32_free , del_is32_free , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_DATAMAP  , MTRC_TYPE_IS32 , new_is32_ht , des_is32_free , del_is32_free , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_UNIQMAP  , MTRC_TYPE_U648 , new_u648_ht , des_u648      , del_u648      , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_METRICS  , MTRC_TYPE_IMTV , new_imtv_ht , des_imtv      , del_imtv      , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_AGENTS   , MTRC_TYPE_IGSL , new_igsl_ht , des_igsl_free , del_igsl_free , 1 , NULL , NULL } ,
  { .metric.storem=MTRC_METADATA , MTRC_TYPE_SU64 , new_su64_ht , des_su64_free , del_su64_free , 1 , NULL , NULL } ,
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
    mtrc = metric - MTRC_UNIQUE_KEYS;
    cnt = MTRC_CNT_VISITORS - MTRC_UNIQUE_KEYS + 1;
    if (mtrc < 0 || mtrc >= cnt) {
      LOG_DEBUG (("Out of bounds when attempting to get hash %d\n", metric));
      return NULL;
    }
    return store->ghash->metrics[mtrc].hash;
  }

  /* the packed numeric metrics all live in the merged MTRC_METRICS table */
  if (metric >= MTRC_ROOT && metric <= MTRC_PROTOCOLS)
    metric = MTRC_METRICS;

  if (metric >= GSMTRC_TOTAL) {
    LOG_DEBUG (("Out of bounds when attempting to get hash %d\n", metric));
    return NULL;
  }

  /* ###NOTE: BE CAREFUL here, to avoid the almost unnecessary loop, we simply
   * use the index from the enum to make it O(1). The metrics array has to be
   * created in the same order as the GSMetric enum */
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

/* Given a module, get its cache
 *
 * On error, NULL is returned.
 * On success, a pointer to the module cache is returned. */
static GKCacheModule *
get_cache_module (GModule module) {
  GKDB *db = get_db_instance (DB_INSTANCE);

  if (!db || !db->cache)
    return NULL;
  return &db->cache[module];
}

/* Reallocate a cache metric array to the new capacity and zero the newly
 * allocated range.
 *
 * On success, the newly allocated array is returned. */
static void *
cache_grow_arr (void *arr, uint32_t oldcap, uint32_t newcap, size_t isize) {
  char *tmp = xrealloc (arr, (size_t) newcap * isize);
  memset (tmp + (size_t) oldcap * isize, 0, ((size_t) newcap - oldcap) * isize);
  return tmp;
}

/* Ensure the cache metric arrays can hold the given ckey index. */
static void
cache_grow (GKCacheModule *cache, uint32_t ckey) {
  uint32_t newcap = 0, oldcap = cache->capacity;

  if (ckey < oldcap)
    return;

  newcap = oldcap == 0 ? CACHE_INIT_CAPACITY : oldcap;
  while (newcap <= ckey)
    newcap <<= 1;

  cache->datamap = cache_grow_arr (cache->datamap, oldcap, newcap, sizeof (char *));
  cache->rootmap = cache_grow_arr (cache->rootmap, oldcap, newcap, sizeof (char *));
  cache->root = cache_grow_arr (cache->root, oldcap, newcap, sizeof (uint32_t));
  cache->hits = cache_grow_arr (cache->hits, oldcap, newcap, sizeof (uint32_t));
  cache->visitors = cache_grow_arr (cache->visitors, oldcap, newcap, sizeof (uint32_t));
  cache->bw = cache_grow_arr (cache->bw, oldcap, newcap, sizeof (uint64_t));
  cache->cumts = cache_grow_arr (cache->cumts, oldcap, newcap, sizeof (uint64_t));
  cache->maxts = cache_grow_arr (cache->maxts, oldcap, newcap, sizeof (uint64_t));
  cache->meth = cache_grow_arr (cache->meth, oldcap, newcap, sizeof (uint8_t));
  cache->proto = cache_grow_arr (cache->proto, oldcap, newcap, sizeof (uint8_t));
  cache->capacity = newcap;
}

/* Insert a data hash key into the cache keymap, assigning a new dense cache
 * key if not present, and ensure the metric arrays can hold it.
 *
 * On error, 0 is returned.
 * On success or if the key exists, its cache key is returned */
static uint32_t
cache_ins_ckey (GKCacheModule *cache, uint32_t dhash) {
  uint32_t ckey = 0;

  if (!cache || !cache->keymap)
    return 0;

  if ((ckey = ins_ii32_ai (cache->keymap, dhash)) == 0)
    return 0;

  if (ckey > cache->size) {
    cache->size = ckey;
    cache_grow (cache, ckey);
  }

  return ckey;
}

/* Look up the cache key assigned to the given data hash key.
 *
 * If not found, 0 is returned.
 * On success, the cache key is returned */
static uint32_t
cache_get_ckey (GKCacheModule *cache, uint32_t dhash) {
  if (!cache)
    return 0;
  return get_ii32 (cache->keymap, dhash);
}

/* Determine whether the given ckey indexes a valid cache entry.
 *
 * On success, non-zero is returned.
 * On failure, 0 is returned. */
static int
cache_valid_ckey (const GKCacheModule *cache, uint32_t ckey) {
  return cache && cache->capacity && ckey >= 1 && ckey <= cache->size;
}

/* Borrow a data string into the cache datamap. The value is only set once
 * per ckey, mirroring the non-replacing insert semantics of the store. */
static void
cache_set_datamap (GKCacheModule *cache, uint32_t ckey, char *value) {
  if (!cache_valid_ckey (cache, ckey) || cache->datamap[ckey])
    return;

  cache->datamap[ckey] = value;
  cache->datamap_size++;
}

/* Borrow a root string into the cache rootmap. The value is only set once
 * per ckey, mirroring the non-replacing insert semantics of the store. */
static void
cache_set_rootmap (GKCacheModule *cache, uint32_t ckey, char *value) {
  if (!cache_valid_ckey (cache, ckey) || cache->rootmap[ckey])
    return;

  cache->rootmap[ckey] = value;
}

/* Map a data ckey to its root ckey. */
static void
cache_set_root (GKCacheModule *cache, uint32_t dkey, uint32_t rkey) {
  if (!cache_valid_ckey (cache, dkey))
    return;

  cache->root[dkey] = rkey;
}

/* Clear all cache entries for a module, keeping the allocated arrays for
 * later reuse. Borrowed strings belong to the dated stores and must not be
 * freed here. */
static void
cache_reset (GKCacheModule *cache) {
  if (!cache || !cache->capacity)
    return;

  del_ii32 (cache->keymap, 0);
  memset (cache->datamap, 0, cache->capacity * sizeof (char *));
  memset (cache->rootmap, 0, cache->capacity * sizeof (char *));
  memset (cache->root, 0, cache->capacity * sizeof (uint32_t));
  memset (cache->hits, 0, cache->capacity * sizeof (uint32_t));
  memset (cache->visitors, 0, cache->capacity * sizeof (uint32_t));
  memset (cache->bw, 0, cache->capacity * sizeof (uint64_t));
  memset (cache->cumts, 0, cache->capacity * sizeof (uint64_t));
  memset (cache->maxts, 0, cache->capacity * sizeof (uint64_t));
  memset (cache->meth, 0, cache->capacity * sizeof (uint8_t));
  memset (cache->proto, 0, cache->capacity * sizeof (uint8_t));
  cache->size = 0;
  cache->datamap_size = 0;
  cache->has_bw = 0;
  cache->has_cumts = 0;
}

/* Initialize the cache for all enabled modules.
 *
 * On success, a pointer to the newly allocated module cache array is
 * returned. */
static GKCacheModule *
init_cache_modules (void) {
  GKCacheModule *cache = xcalloc (TOTAL_MODULES, sizeof (GKCacheModule));
  GModule module;
  size_t idx = 0;

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    cache[module].keymap = new_ii32_ht ();
  }

  return cache;
}

/* Free the module cache and all of its metric arrays. */
void
free_cache (GKCacheModule *cache) {
  GKCacheModule *c = NULL;
  GModule module;
  size_t idx = 0;

  if (!cache)
    return;

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    c = &cache[module];

    des_ii32 (c->keymap, 0);
    free (c->datamap);
    free (c->rootmap);
    free (c->root);
    free (c->hits);
    free (c->visitors);
    free (c->bw);
    free (c->cumts);
    free (c->maxts);
    free (c->meth);
    free (c->proto);
  }
  free (cache);
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

/* Increases the unique visitors counter for the given date. */
void
ht_inc_cnt_visitors (uint32_t date) {
  khash_t (ii32) * hash = get_hash (-1, date, MTRC_CNT_VISITORS);

  if (!hash)
    return;

  inc_ii32 (hash, 1, 1);
}

/* Insert a unique visitor fingerprint (IP/UA), mapped to an auto incremented
 * value. On a visitor's first countable request, the value's counted bit is
 * set and first_count is raised for the panels to act on.
 *
 * If the given key exists, its sequence value is returned.
 * On error, 0 is returned.
 * On success the sequence value of the key inserted is returned */
uint32_t
ht_insert_unique_key (uint32_t date, uint64_t key, int countable, int *first_count) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si32) * seqs = get_hdb (db, MTRC_SEQS);
  khash_t (u6432) * hash = get_hash (-1, date, MTRC_UNIQUE_KEYS);
  uint32_t val = 0;
  khint_t k;
  int ret;

  *first_count = 0;
  if (!hash)
    return 0;

  k = kh_put (u6432, hash, key, &ret);
  if (ret == -1)
    return 0;

  if (ret == 0) {
    val = kh_val (hash, k);
  } else {
    val = ht_ins_seq (seqs, "ht_unique_keys");
    /* the counted bit occupies the top sequence bit; reject an overflowing
     * sequence rather than corrupting counted state */
    if (val == 0 || (val & VISITOR_COUNTED_BIT)) {
      kh_del (u6432, hash, k);
      return 0;
    }
    kh_val (hash, k) = val;
  }

  if (countable && !(val & VISITOR_COUNTED_BIT)) {
    kh_val (hash, k) = val | VISITOR_COUNTED_BIT;
    *first_count = 1;
  }

  return val & ~VISITOR_COUNTED_BIT;
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
  GKCacheModule *cache = get_cache_module (module);

  uint32_t val = 0;
  const char *modstr;

  if (!hash)
    return 0;

  if ((val = get_ii32 (hash, key)) != 0) {
    *ckey = cache_get_ckey (cache, key);
    return val;
  }

  modstr = get_module_str (module);
  if ((val = ins_ii32_inc (hash, key, ht_ins_seq, seqs, modstr)) == 0) {
    return val;
  }
  *ckey = cache_ins_ckey (cache, key);

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
  GKCacheModule *cache = get_cache_module (module);
  char *dupval = NULL;
  int ret = 0;

  if (!hash)
    return -1;

  dupval = xstrdup (value);
  if ((ret = ins_is32 (hash, key, dupval)) == 0)
    cache_set_rootmap (cache, ckey, dupval);
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
  GKCacheModule *cache = get_cache_module (module);
  char *dupval = NULL;
  int ret = 0;

  if (!hash)
    return -1;

  dupval = xstrdup (value);
  if ((ret = ins_is32 (hash, key, dupval)) == 0)
    cache_set_datamap (cache, ckey, dupval);
  else
    free (dupval);

  return ret;
}

/* Insert a uniqmap uint64_t key into the set.
 *
 * If the given key exists, 0 is returned.
 * On error, 0 is returned.
 * On success 1 is returned */
int
ht_insert_uniqmap (GModule module, uint32_t date, uint32_t key, uint32_t value) {
  khash_t (u648) * hash = get_hash (module, date, MTRC_UNIQMAP);
  uint64_t k = 0;

  if (!hash)
    return 0;

  k = u64encode (key, value);

  return ins_u648 (hash, k) == 0 ? 1 : 0;
}

/* Insert a data uint32_t key mapped to the corresponding uint32_t root key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_root (GModule module, uint32_t date, uint32_t key, uint32_t value, uint32_t dkey,
                uint32_t rkey) {
  khash_t (imtv) * hash = get_hash (module, date, MTRC_METRICS);
  GKCacheModule *cache = get_cache_module (module);
  GKMetricVals *mv = NULL;

  if (!hash)
    return -1;

  cache_set_root (cache, dkey, rkey);
  if (!(mv = ins_imtv (hash, key)))
    return -1;
  mv->root = value;

  return 0;
}

/* Increases hits counter from a uint32_t key.
 *
 * On error, 0 is returned.
 * On success the inserted value is returned */
uint32_t
ht_insert_hits (GModule module, uint32_t date, uint32_t key, uint32_t inc, uint32_t ckey) {
  khash_t (imtv) * hash = get_hash (module, date, MTRC_METRICS);
  GKCacheModule *cache = get_cache_module (module);
  GKMetricVals *mv = NULL;

  if (!hash)
    return 0;

  if (cache_valid_ckey (cache, ckey))
    __atomic_add_fetch (&cache->hits[ckey], inc, __ATOMIC_SEQ_CST);
  if (!(mv = ins_imtv (hash, key)))
    return 0;
  return __atomic_add_fetch (&mv->hits, inc, __ATOMIC_SEQ_CST);
}

/* Increases visitors counter from a uint32_t key.
 *
 * On error, 0 is returned.
 * On success the inserted value is returned */
uint32_t
ht_insert_visitor (GModule module, uint32_t date, uint32_t key, uint32_t inc, uint32_t ckey) {
  khash_t (imtv) * hash = get_hash (module, date, MTRC_METRICS);
  GKCacheModule *cache = get_cache_module (module);
  GKMetricVals *mv = NULL;

  if (!hash)
    return 0;

  if (cache_valid_ckey (cache, ckey))
    __atomic_add_fetch (&cache->visitors[ckey], inc, __ATOMIC_SEQ_CST);
  if (!(mv = ins_imtv (hash, key)))
    return 0;
  return __atomic_add_fetch (&mv->visitors, inc, __ATOMIC_SEQ_CST);
}

/* Increases bandwidth counter from a uint32_t key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_bw (GModule module, uint32_t date, uint32_t key, uint64_t inc, uint32_t ckey) {
  khash_t (imtv) * hash = get_hash (module, date, MTRC_METRICS);
  GKCacheModule *cache = get_cache_module (module);
  GKMetricVals *mv = NULL;

  if (!hash)
    return -1;

  if (cache_valid_ckey (cache, ckey)) {
    cache->bw[ckey] += inc;
    cache->has_bw = 1;
  }
  if (!(mv = ins_imtv (hash, key)))
    return -1;
  mv->bw += inc;
  mv->touched |= METRIC_TOUCHED_BW;

  return 0;
}

/* Increases cumulative time served counter from a uint32_t key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_cumts (GModule module, uint32_t date, uint32_t key, uint64_t inc, uint32_t ckey) {
  khash_t (imtv) * hash = get_hash (module, date, MTRC_METRICS);
  GKCacheModule *cache = get_cache_module (module);
  GKMetricVals *mv = NULL;

  if (!hash)
    return -1;

  if (cache_valid_ckey (cache, ckey)) {
    cache->cumts[ckey] += inc;
    cache->has_cumts = 1;
  }
  if (!(mv = ins_imtv (hash, key)))
    return -1;
  mv->cumts += inc;
  mv->touched |= METRIC_TOUCHED_CUMTS;

  return 0;
}

/* Insert the maximum time served counter from a uint32_t key.
 * Note: it compares the current value with the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_maxts (GModule module, uint32_t date, uint32_t key, uint64_t value, uint32_t ckey) {
  khash_t (imtv) * hash = get_hash (module, date, MTRC_METRICS);
  GKCacheModule *cache = get_cache_module (module);
  GKMetricVals *mv = NULL;

  if (!hash)
    return -1;

  if (cache_valid_ckey (cache, ckey) && cache->maxts[ckey] < value)
    cache->maxts[ckey] = value;
  if (!(mv = ins_imtv (hash, key)))
    return -1;
  if (mv->maxts < value)
    mv->maxts = value;

  return 0;
}

/* Insert a method given an uint32_t key and string value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_method (GModule module, uint32_t date, uint32_t key, const char *value, uint32_t ckey) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (imtv) * hash = get_hash (module, date, MTRC_METRICS);
  GKCacheModule *cache = get_cache_module (module);
  khash_t (si08) * mtpr = get_hdb (db, MTRC_METH_PROTO);
  GKMetricVals *mv = NULL;
  uint8_t val = 0;

  if (!hash)
    return -1;

  if (!(val = get_si08 (mtpr, value)))
    return -1;

  if (!(mv = ins_imtv (hash, key)))
    return -1;
  mv->meth = val;
  if (cache_valid_ckey (cache, ckey))
    cache->meth[ckey] = val;

  return 0;
}

/* Insert a protocol given an uint32_t key and string value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_protocol (GModule module, uint32_t date, uint32_t key, const char *value, uint32_t ckey) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (imtv) * hash = get_hash (module, date, MTRC_METRICS);
  GKCacheModule *cache = get_cache_module (module);
  khash_t (si08) * mtpr = get_hdb (db, MTRC_METH_PROTO);
  GKMetricVals *mv = NULL;
  uint8_t val = 0;

  if (!hash)
    return -1;

  if (!(val = get_si08 (mtpr, value)))
    return -1;

  if (!(mv = ins_imtv (hash, key)))
    return -1;
  mv->proto = val;
  if (cache_valid_ckey (cache, ckey))
    cache->proto[ckey] = val;

  return 0;
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
 * Return 0 if the operation fails, else number of elements. */
uint32_t
ht_get_size_datamap (GModule module) {
  GKCacheModule *cache = get_cache_module (module);

  if (!cache)
    return 0;

  return cache->datamap_size;
}

/* Get the total number of unique visitors across all dates. The per-date
 * counters are authoritative under every date specificity; the VISITORS
 * uniqmap is membership state only and may hold conservative extra pairs
 * after a migration.
 *
 * On error, 0 is returned.
 * On success the number of unique visitors is returned */
uint32_t
ht_sum_uniq_visitors (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (ii32) * hash = NULL;
  uint32_t k = 0;
  uint32_t sum = 0;

  if (!dates)
    return 0;

  /* *INDENT-OFF* */
  HT_SUM_VAL (dates, k, {
    if ((hash = get_hash (-1, k, MTRC_CNT_VISITORS)))
      sum += get_ii32 (hash, 1);
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
  GKCacheModule *cache = get_cache_module (module);

  if (!cache_valid_ckey (cache, key) || !cache->datamap[key])
    return NULL;

  return xstrdup (cache->datamap[key]);
}

/* Get the string root from MTRC_ROOTMAP given an uint32_t data key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_root (GModule module, uint32_t key) {
  GKCacheModule *cache = get_cache_module (module);
  uint32_t root_key = 0;

  if (!cache_valid_ckey (cache, key))
    return NULL;

  /* not found */
  if ((root_key = cache->root[key]) == 0)
    return NULL;

  if (!cache_valid_ckey (cache, root_key) || !cache->rootmap[root_key])
    return NULL;

  return xstrdup (cache->rootmap[root_key]);
}

/* Get the int visitors value from MTRC_HITS given an int key.
 *
 * If key is not found, 0 is returned.
 * On error, -1 is returned.
 * On success the int value for the given key is returned */
uint32_t
ht_get_hits (GModule module, int key) {
  GKCacheModule *cache = get_cache_module (module);

  if (key < 1 || !cache_valid_ckey (cache, key))
    return 0;

  return __atomic_load_n (&cache->hits[key], __ATOMIC_SEQ_CST);
}

/* Get the uint32_t visitors value from MTRC_VISITORS given an uint32_t key.
 *
 * If key is not found, 0 is returned.
 * On error, -1 is returned.
 * On success the uint32_t value for the given key is returned */
uint32_t
ht_get_visitors (GModule module, uint32_t key) {
  GKCacheModule *cache = get_cache_module (module);

  if (!cache_valid_ckey (cache, key))
    return 0;

  return __atomic_load_n (&cache->visitors[key], __ATOMIC_SEQ_CST);
}

/* Get the uint64_t value from MTRC_BW given an uint32_t key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_bw (GModule module, uint32_t key) {
  GKCacheModule *cache = get_cache_module (module);

  if (!cache_valid_ckey (cache, key))
    return 0;

  return cache->bw[key];
}

/* Get the uint64_t value from MTRC_CUMTS given an uint32_t key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_cumts (GModule module, uint32_t key) {
  GKCacheModule *cache = get_cache_module (module);

  if (!cache_valid_ckey (cache, key))
    return 0;

  return cache->cumts[key];
}

/* Get the uint64_t value from MTRC_MAXTS given an uint32_t key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_maxts (GModule module, uint32_t key) {
  GKCacheModule *cache = get_cache_module (module);

  if (!cache_valid_ckey (cache, key))
    return 0;

  return cache->maxts[key];
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

/* Look up the string for a method/protocol numeric value.
 *
 * On error, NULL is returned.
 * On success the string value for the given id is returned */
static char *
get_meth_proto_str (uint8_t val) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (si08) * mtpr = get_hdb (db, MTRC_METH_PROTO);
  khint_t k;

  for (k = kh_begin (mtpr); k != kh_end (mtpr); ++k) {
    if (kh_exist (mtpr, k) && kh_val (mtpr, k) == val)
      return xstrdup (kh_key (mtpr, k));
  }
  return NULL;
}

/* Get the string value from MTRC_METHODS given an uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_method (GModule module, uint32_t key) {
  GKCacheModule *cache = get_cache_module (module);

  if (!cache_valid_ckey (cache, key) || !cache->meth[key])
    return NULL;

  return get_meth_proto_str (cache->meth[key]);
}

/* Get the string value from MTRC_PROTOCOLS given an uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_protocol (GModule module, uint32_t key) {
  GKCacheModule *cache = get_cache_module (module);

  if (!cache_valid_ckey (cache, key) || !cache->proto[key])
    return NULL;

  return get_meth_proto_str (cache->proto[key]);
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

/* Set the maximum and minimum values found on a uint32_t cache metric array.
 * Only assigned entries (non-zero values) are considered, matching the
 * presence semantics of the former per-metric hash tables.
 *
 * If no entries are present, no values are set.
 * On success the minimum and maximum values are set. */
static void
cache_min_max_u32 (const GKCacheModule *cache, const uint32_t *arr, uint32_t *min, uint32_t *max) {
  uint32_t i, curvalue = 0;
  int j = 0;

  for (i = 1; i <= cache->size; ++i) {
    if ((curvalue = arr[i]) == 0)
      continue;
    if (j++ == 0)
      *min = curvalue;
    if (curvalue > *max)
      *max = curvalue;
    if (curvalue < *min)
      *min = curvalue;
  }
}

/* Set the maximum and minimum values found on a uint64_t cache metric array.
 * All data entries (hits > 0) are considered, matching the presence
 * semantics of the former bw/cumts hash tables where a zero value was still
 * a present entry.
 *
 * If no entries are present, no values are set.
 * On success the minimum and maximum values are set. */
static void
cache_min_max_u64_data (const GKCacheModule *cache, const uint64_t *arr, uint64_t *min,
                        uint64_t *max) {
  uint64_t curvalue = 0;
  uint32_t i;
  int j = 0;

  for (i = 1; i <= cache->size; ++i) {
    if (cache->hits[i] == 0)
      continue;
    curvalue = arr[i];
    if (j++ == 0)
      *min = curvalue;
    if (curvalue > *max)
      *max = curvalue;
    if (curvalue < *min)
      *min = curvalue;
  }
}

/* Set the maximum and minimum values found on a uint64_t cache metric array.
 * Only assigned entries (non-zero values) are considered, matching the
 * presence semantics of the former maxts hash tables.
 *
 * If no entries are present, no values are set.
 * On success the minimum and maximum values are set. */
static void
cache_min_max_u64_nonzero (const GKCacheModule *cache, const uint64_t *arr, uint64_t *min,
                           uint64_t *max) {
  uint64_t curvalue = 0;
  uint32_t i;
  int j = 0;

  for (i = 1; i <= cache->size; ++i) {
    if ((curvalue = arr[i]) == 0)
      continue;
    if (j++ == 0)
      *min = curvalue;
    if (curvalue > *max)
      *max = curvalue;
    if (curvalue < *min)
      *min = curvalue;
  }
}

/* Set the maximum and minimum values found on an integer key and
 * integer value found on the MTRC_HITS metric.
 *
 * If the metric is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_hits_min_max (GModule module, uint32_t *min, uint32_t *max) {
  GKCacheModule *cache = get_cache_module (module);

  if (!cache || !cache->capacity)
    return;

  cache_min_max_u32 (cache, cache->hits, min, max);
}

/* Set the maximum and minimum values found on an integer key and
 * integer value found on the MTRC_VISITORS metric.
 *
 * If the metric is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_visitors_min_max (GModule module, uint32_t *min, uint32_t *max) {
  GKCacheModule *cache = get_cache_module (module);

  if (!cache || !cache->capacity)
    return;

  cache_min_max_u32 (cache, cache->visitors, min, max);
}

/* Set the maximum and minimum values found on an integer key and
 * a uint64_t value found on the MTRC_BW metric.
 *
 * If the metric is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_bw_min_max (GModule module, uint64_t *min, uint64_t *max) {
  GKCacheModule *cache = get_cache_module (module);

  if (!cache || !cache->capacity || !cache->has_bw)
    return;

  cache_min_max_u64_data (cache, cache->bw, min, max);
}

/* Set the maximum and minimum values found on an integer key and
 * a uint64_t value found on the MTRC_CUMTS metric.
 *
 * If the metric is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_cumts_min_max (GModule module, uint64_t *min, uint64_t *max) {
  GKCacheModule *cache = get_cache_module (module);

  if (!cache || !cache->capacity || !cache->has_cumts)
    return;

  cache_min_max_u64_data (cache, cache->cumts, min, max);
}

/* Set the maximum and minimum values found on an integer key and
 * a uint64_t value found on the MTRC_MAXTS metric.
 *
 * If the metric is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_maxts_min_max (GModule module, uint64_t *min, uint64_t *max) {
  GKCacheModule *cache = get_cache_module (module);

  if (!cache || !cache->capacity)
    return;

  cache_min_max_u64_nonzero (cache, cache->maxts, min, max);
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
    cache_reset (get_cache_module (module));
  }

  destroy_date_stores (date);

  return 0;
}

/* Rebuild the cache for a module from the given date store. */
static int
ins_raw_num_data (GModule module, uint32_t date) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * hash = get_hdb (db, MTRC_DATES);
  GKHashStorage *store = get_store (hash, date);
  GKCacheModule *cache = get_cache_module (module);
  khiter_t k, kr;
  uint32_t ckey = 0, nrkey = 0;
  char *val = NULL;
  GKMetricVals *mv = NULL;

  khash_t (ii32) * kmap = get_hash_from_store (store, module, MTRC_KEYMAP);
  khash_t (imtv) * metrics = get_hash_from_store (store, module, MTRC_METRICS);
  khash_t (is32) * dmap = get_hash_from_store (store, module, MTRC_DATAMAP);
  khash_t (is32) * rmap = get_hash_from_store (store, module, MTRC_ROOTMAP);

  if (!kmap)
    return -1;

  for (k = kh_begin (kmap); k != kh_end (kmap); ++k) {
    if (!kh_exist (kmap, k))
      continue;
    if ((ckey = cache_ins_ckey (cache, kh_key (kmap, k))) == 0)
      continue;

    mv = get_imtv (metrics, kh_val (kmap, k));

    if (mv && mv->root) {
      kr = kh_get (is32, rmap, mv->root);
      if (kr != kh_end (rmap) && (val = kh_val (rmap, kr))) {
        nrkey = cache_ins_ckey (cache, djb2 ((unsigned char *) val));
        cache_set_rootmap (cache, nrkey, val);
        cache_set_root (cache, ckey, nrkey);
      }
    }

    if ((kr = kh_get (is32, dmap, kh_val (kmap, k))) != kh_end (dmap))
      cache_set_datamap (cache, ckey, kh_val (dmap, kr));

    /* root-only keys hold no metrics of their own */
    if (!mv)
      continue;

    if (mv->hits)
      __atomic_add_fetch (&cache->hits[ckey], mv->hits, __ATOMIC_SEQ_CST);
    if (mv->visitors)
      __atomic_add_fetch (&cache->visitors[ckey], mv->visitors, __ATOMIC_SEQ_CST);
    if (mv->touched & METRIC_TOUCHED_BW) {
      cache->bw[ckey] += mv->bw;
      cache->has_bw = 1;
    }
    if (mv->touched & METRIC_TOUCHED_CUMTS) {
      cache->cumts[ckey] += mv->cumts;
      cache->has_cumts = 1;
    }
    if (mv->maxts && cache->maxts[ckey] < mv->maxts)
      cache->maxts[ckey] = mv->maxts;
    if (mv->meth)
      cache->meth[ckey] = mv->meth;
    if (mv->proto)
      cache->proto[ckey] = mv->proto;
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
  db->cache = init_cache_modules ();

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

/* A wrapper to initialize a raw data structure that retains at most `limit`
 * of a table's `ht_size` keys, or all of them given RAW_DATA_ALL.
 *
 * On success a GRawData structure is returned. */
static GRawData *
init_new_raw_data (GModule module, datatype type, uint32_t ht_size, uint32_t limit) {
  GRawData *raw_data;

  raw_data = new_grawdata ();
  raw_data->idx = 0;
  raw_data->module = module;
  raw_data->type = type;
  raw_data->size = ht_size;
  raw_data->capacity = (limit == RAW_DATA_ALL || limit > ht_size) ? ht_size : limit;
  raw_data->items = new_grawdata_item (raw_data->capacity);

  return raw_data;
}

/* Store the top `limit` cache hits into raw_data.
 *
 * On error, NULL is returned.
 * On success the GRawData is returned */
static GRawData *
get_u32_raw_data (GModule module, uint32_t limit) {
  GKCacheModule *cache = get_cache_module (module);
  GRawData *raw_data;
  GRawDataItem item = { 0 };
  uint32_t i, ht_size = 0;

  if (!cache || !cache->keymap)
    return NULL;

  for (i = 1; i <= cache->size; ++i)
    ht_size += cache->hits[i] > 0;

  raw_data = init_new_raw_data (module, U32, ht_size, limit);

  for (i = 1; i <= cache->size; ++i) {
    if (cache->hits[i] == 0)
      continue;
    item.nkey = i;
    item.hits = cache->hits[i];
    retain_raw_item (raw_data, item);
  }

  return raw_data;
}

/* Store the top `limit` cache data strings into raw_data.
 *
 * On error, NULL is returned.
 * On success the GRawData is returned */
static GRawData *
get_str_raw_data (GModule module, uint32_t limit) {
  GKCacheModule *cache = get_cache_module (module);
  GRawData *raw_data;
  GRawDataItem item = { 0 };
  uint32_t i;

  if (!cache || !cache->keymap)
    return NULL;

  raw_data = init_new_raw_data (module, STR, cache->datamap_size, limit);

  for (i = 1; i <= cache->size; ++i) {
    if (!cache->datamap[i])
      continue;
    item.nkey = i;
    item.data = cache->datamap[i];
    retain_raw_item (raw_data, item);
  }

  return raw_data;
}

/* Entry point to load the raw data from the data store into our
 * GRawData structure, keeping only the top `limit` keys unless RAW_DATA_ALL
 * is given.
 *
 * On error, NULL is returned.
 * On success the GRawData sorted is returned */
GRawData *
parse_raw_data (GModule module, uint32_t limit) {
  GRawData *raw_data = NULL;

#ifdef _DEBUG
  clock_t begin = clock ();
  double taken;
  const char *modstr = NULL;
  LOG_DEBUG (("== parse_raw_data ==\n"));
#endif

  switch (module) {
  case VISITORS:
    raw_data = get_str_raw_data (module, limit);
    if (raw_data)
      sort_raw_str_data (raw_data, raw_data->idx);
    break;
  default:
    raw_data = get_u32_raw_data (module, limit);
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
