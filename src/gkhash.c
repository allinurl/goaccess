/**
 * gkhash.c -- default hash table functions
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2016 Gerardo Orellana <hello @ goaccess.io>
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

#include "gkhash.h"

#include "error.h"
#include "sort.h"
#include "util.h"
#include "xmalloc.h"

/* Hash tables storage */
static GKHashStorage *gkh_storage;

/* *INDENT-OFF* */
/* Hash tables used across the whole app */
static khash_t (is32) *ht_agent_vals  = NULL;
static khash_t (si32) *ht_agent_keys  = NULL;
static khash_t (si32) *ht_unique_keys = NULL;
static khash_t (ss32) *ht_hostnames   = NULL;
/* *INDENT-ON* */

/* Instantiate a new store */
static GKHashStorage *
new_gkhstorage (uint32_t size)
{
  GKHashStorage *storage = xcalloc (size, sizeof (GKHashStorage));
  return storage;
}

/* Initialize a new int key - int value hash table */
static
khash_t (ii32) *
new_ii32_ht (void)
{
  khash_t (ii32) * h = kh_init (ii32);
  return h;
}

/* Initialize a new int key - string value hash table */
static
khash_t (is32) *
new_is32_ht (void)
{
  khash_t (is32) * h = kh_init (is32);
  return h;
}

/* Initialize a new int key - uint64_t value hash table */
static
khash_t (iu64) *
new_iu64_ht (void)
{
  khash_t (iu64) * h = kh_init (iu64);
  return h;
}

/* Initialize a new string key - int value hash table */
static
khash_t (si32) *
new_si32_ht (void)
{
  khash_t (si32) * h = kh_init (si32);
  return h;
}

/* Initialize a new string key - string value hash table */
static
khash_t (ss32) *
new_ss32_ht (void)
{
  khash_t (ss32) * h = kh_init (ss32);
  return h;
}

/* Initialize a new int key - int value hash table */
static
khash_t (igsl) *
new_igsl_ht (void)
{
  khash_t (igsl) * h = kh_init (igsl);
  return h;
}

/* Initialize a new int key - uint64_t value hash table */
static
khash_t (su64) *
new_su64_ht (void)
{
  khash_t (su64) * h = kh_init (su64);
  return h;
}

/* Destroys both the hash structure and the keys for a
 * string key - int value hash */
static void
des_si32_free (khash_t (si32) * hash)
{
  khint_t k;
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
des_is32_free (khash_t (is32) * hash)
{
  khint_t k;
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
des_ss32_free (khash_t (ss32) * hash)
{
  khint_t k;
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
des_ii32 (khash_t (ii32) * hash)
{
  if (!hash)
    return;
  kh_destroy (ii32, hash);
}

/* Destroys both the hash structure and its GSLList
 * values */
static void
des_igsl_free (khash_t (igsl) * hash)
{
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
des_su64_free (khash_t (su64) * hash)
{
  if (!hash)
    return;

  kh_destroy (su64, hash);
}

/* Destroys the hash structure */
static void
des_iu64 (khash_t (iu64) * hash)
{
  if (!hash)
    return;
  kh_destroy (iu64, hash);
}

/* Initialize map & metric hashes */
static void
init_tables (GModule module)
{
  int n = 0, i;
  /* *INDENT-OFF* */
  GKHashMetric metrics[] = {
    {MTRC_KEYMAP    , MTRC_TYPE_SI32 , {.si32 = new_si32_ht ()}} ,
    {MTRC_ROOTMAP   , MTRC_TYPE_IS32 , {.is32 = new_is32_ht ()}} ,
    {MTRC_DATAMAP   , MTRC_TYPE_IS32 , {.is32 = new_is32_ht ()}} ,
    {MTRC_UNIQMAP   , MTRC_TYPE_SI32 , {.si32 = new_si32_ht ()}} ,
    {MTRC_ROOT      , MTRC_TYPE_II32 , {.ii32 = new_ii32_ht ()}} ,
    {MTRC_HITS      , MTRC_TYPE_II32 , {.ii32 = new_ii32_ht ()}} ,
    {MTRC_VISITORS  , MTRC_TYPE_II32 , {.ii32 = new_ii32_ht ()}} ,
    {MTRC_BW        , MTRC_TYPE_IU64 , {.iu64 = new_iu64_ht ()}} ,
    {MTRC_CUMTS     , MTRC_TYPE_IU64 , {.iu64 = new_iu64_ht ()}} ,
    {MTRC_MAXTS     , MTRC_TYPE_IU64 , {.iu64 = new_iu64_ht ()}} ,
    {MTRC_METHODS   , MTRC_TYPE_IS32 , {.is32 = new_is32_ht ()}} ,
    {MTRC_PROTOCOLS , MTRC_TYPE_IS32 , {.is32 = new_is32_ht ()}} ,
    {MTRC_AGENTS    , MTRC_TYPE_IGSL , {.igsl = new_igsl_ht ()}} ,
    {MTRC_METADATA  , MTRC_TYPE_SU64 , {.su64 = new_su64_ht ()}} ,
  };
  /* *INDENT-ON* */

  n = ARRAY_SIZE (metrics);
  for (i = 0; i < n; i++) {
    gkh_storage[module].metrics[i] = metrics[i];
  }
}

/* Initialize hash tables */
void
init_storage (void)
{
  GModule module;
  size_t idx = 0;

  /* Hashes used across the whole app (not per module) */
  ht_agent_keys = (khash_t (si32) *) new_si32_ht ();
  ht_agent_vals = (khash_t (is32) *) new_is32_ht ();
  ht_hostnames = (khash_t (ss32) *) new_ss32_ht ();
  ht_unique_keys = (khash_t (si32) *) new_si32_ht ();

  gkh_storage = new_gkhstorage (TOTAL_MODULES);
  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    gkh_storage[module].module = module;
    init_tables (module);
  }
}

static void
free_metric_type (GKHashMetric mtrc)
{
  /* Determine the hash structure type */
  switch (mtrc.type) {
  case MTRC_TYPE_II32:
    des_ii32 (mtrc.ii32);
    break;
  case MTRC_TYPE_IS32:
    des_is32_free (mtrc.is32);
    break;
  case MTRC_TYPE_IU64:
    des_iu64 (mtrc.iu64);
    break;
  case MTRC_TYPE_SI32:
    des_si32_free (mtrc.si32);
    break;
  case MTRC_TYPE_SS32:
    des_ss32_free (mtrc.ss32);
    break;
  case MTRC_TYPE_IGSL:
    des_igsl_free (mtrc.igsl);
    break;
  case MTRC_TYPE_SU64:
    des_su64_free (mtrc.su64);
    break;
  }
}

/* Destroys the hash structure allocated metrics */
static void
free_metrics (GModule module)
{
  int i;
  GKHashMetric mtrc;

  for (i = 0; i < GSMTRC_TOTAL; i++) {
    mtrc = gkh_storage[module].metrics[i];
    free_metric_type (mtrc);
  }
}

/* Destroys the hash structure and its content */
void
free_storage (void)
{
  size_t idx = 0;

  des_is32_free (ht_agent_vals);
  des_si32_free (ht_agent_keys);
  des_si32_free (ht_unique_keys);
  des_ss32_free (ht_hostnames);

  if (!gkh_storage)
    return;

  FOREACH_MODULE (idx, module_list) {
    free_metrics (module_list[idx]);
  }
  free (gkh_storage);
}

/* Given a module and a metric, get the hash table
 *
 * On error, or if table is not found, NULL is returned.
 * On success the hash structure pointer is returned. */
static void *
get_hash (GModule module, GSMetric metric)
{
  void *hash = NULL;
  int i;
  GKHashMetric mtrc;

  for (i = 0; i < GSMTRC_TOTAL; i++) {
    if (hash != NULL)
      break;

    mtrc = gkh_storage[module].metrics[i];
    if (mtrc.metric != metric)
      continue;

    /* Determine the hash structure type */
    switch (mtrc.type) {
    case MTRC_TYPE_II32:
      hash = mtrc.ii32;
      break;
    case MTRC_TYPE_IS32:
      hash = mtrc.is32;
      break;
    case MTRC_TYPE_IU64:
      hash = mtrc.iu64;
      break;
    case MTRC_TYPE_SI32:
      hash = mtrc.si32;
      break;
    case MTRC_TYPE_SS32:
      hash = mtrc.ss32;
      break;
    case MTRC_TYPE_IGSL:
      hash = mtrc.igsl;
      break;
    case MTRC_TYPE_SU64:
      hash = mtrc.su64;
      break;
    }
  }

  return hash;
}

/* Insert a string key and the corresponding int value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
static int
ins_si32 (khash_t (si32) * hash, const char *key, int value)
{
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

/* Insert an int key and the corresponding string value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
static int
ins_is32 (khash_t (is32) * hash, int key, const char *value)
{
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
ins_ss32 (khash_t (ss32) * hash, const char *key, const char *value)
{
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

/* Insert an int key and an int value
 * Note: If the key exists, its value is replaced by the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
ins_ii32 (khash_t (ii32) * hash, int key, int value)
{
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

/* Insert an int key and a uint64_t value
 * Note: If the key exists, its value is replaced by the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
ins_iu64 (khash_t (iu64) * hash, int key, uint64_t value)
{
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

/* Increase an int value given an int key.
 * Note: If the key exists, its value is increased by the given inc.
 *
 * On error, -1 is returned.
 * On success the inserted value is returned */
static int
inc_ii32 (khash_t (ii32) * hash, int key, int inc)
{
  khint_t k;
  int ret, value = inc;

  if (!hash)
    return -1;

  k = kh_get (ii32, hash, key);
  /* key found, increment current value by the given `inc` */
  if (k != kh_end (hash))
    value = kh_val (hash, k) + inc;

  k = kh_put (ii32, hash, key, &ret);
  if (ret == -1)
    return -1;

  kh_val (hash, k) = value;

  return value;
}

/* Increase a uint64_t value given a string key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
inc_su64 (khash_t (su64) * hash, const char *key, uint64_t inc)
{
  khint_t k;
  int ret;
  uint64_t value = inc;

  if (!hash)
    return -1;

  k = kh_get (su64, hash, key);
  /* key not found, set new value to the given `inc` */
  if (k == kh_end (hash)) {
    k = kh_put (su64, hash, key, &ret);
    /* operation failed */
    if (ret == -1)
      return -1;
  } else {
    value = kh_val (hash, k) + inc;
  }

  kh_val (hash, k) = value;

  return 0;
}

/* Increase a uint64_t value given an int key.
 * Note: If the key exists, its value is increased by the given inc.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
inc_iu64 (khash_t (iu64) * hash, int key, uint64_t inc)
{
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

/* Insert a string key and auto increment int value.
 *
 * On error, -1 is returned.
 * On success the value of the key inserted is returned */
static int
ins_si32_ai (khash_t (si32) * hash, const char *key)
{
  int size = 0, value = 0;

  if (!hash)
    return -1;

  size = kh_size (hash);
  /* the auto increment value starts at SIZE (hash table) + 1 */
  value = size > 0 ? size + 1 : 1;

  if (ins_si32 (hash, key, value) == -1)
    return -1;

  return value;
}

/* Compare if the given needle is in the haystack
 *
 * if equal, 1 is returned, else 0 */
static int
find_int_key_in_list (void *data, void *needle)
{
  return (*(int *) data) == (*(int *) needle) ? 1 : 0;
}

/* Insert an int key and the corresponding GSLList (Single linked-list) value.
 * Note: If the key exists within the list, the value is not appended.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
ins_igsl (khash_t (igsl) * hash, int key, int value)
{
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
    list = list_insert_prepend (list, int2ptr (value));
  } else {
    list = list_create (int2ptr (value));
  }

  k = kh_put (igsl, hash, key, &ret);
  if (ret == -1)
    return -1;

  kh_val (hash, k) = list;

  return 0;
}

/* Get the int value of a given string key.
 *
 * On error, -1 is returned.
 * On success the int value for the given key is returned */
static int
get_si32 (khash_t (si32) * hash, const char *key)
{
  khint_t k;

  if (!hash)
    return -1;

  k = kh_get (si32, hash, key);
  /* key found, return current value */
  if (k != kh_end (hash))
    return kh_val (hash, k);

  return -1;
}

/* Get the string value of a given int key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
static char *
get_is32 (khash_t (is32) * hash, int key)
{
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
get_ss32 (khash_t (ss32) * hash, const char *key)
{
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

/* Get the int value of a given int key.
 *
 * If key is not found, 0 is returned.
 * On error, -1 is returned.
 * On success the int value for the given key is returned */
static int
get_ii32 (khash_t (ii32) * hash, int key)
{
  khint_t k;
  int value = 0;

  if (!hash)
    return -1;

  k = kh_get (ii32, hash, key);
  /* key found, return current value */
  if (k != kh_end (hash) && (value = kh_val (hash, k)))
    return value;

  return 0;
}

/* Get the uint64_t value of a given int key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
static uint64_t
get_iu64 (khash_t (iu64) * hash, int key)
{
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

/* Get the GSLList value of a given int key.
 *
 * On error, or if key is not found, NULL is returned.
 * On success the GSLList value for the given key is returned */
static GSLList *
get_igsl (khash_t (igsl) * hash, int key)
{
  khint_t k;
  GSLList *list = NULL;

  if (!hash)
    return NULL;

  k = kh_get (igsl, hash, key);
  /* key found, return current value */
  if (k != kh_end (hash) && (list = kh_val (hash, k)))
    return list;

  return NULL;
}

/* Get the uint64_t value of a given string key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
static uint64_t
get_su64 (khash_t (su64) * hash, const char *key)
{
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
get_ii32_min_max (khash_t (ii32) * hash, int *min, int *max)
{
  khint_t k;
  int curvalue = 0, i;

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
get_iu64_min_max (khash_t (iu64) * hash, uint64_t * min, uint64_t * max)
{
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

/* Insert a unique visitor key string (IP/DATE/UA), mapped to an auto
 * incremented value.
 *
 * If the given key exists, its value is returned.
 * On error, -1 is returned.
 * On success the value of the key inserted is returned */
int
ht_insert_unique_key (const char *key)
{
  int value = -1;
  khash_t (si32) * hash = ht_unique_keys;

  if (!hash)
    return -1;

  if ((value = get_si32 (hash, key)) != -1)
    return value;

  return ins_si32_ai (hash, key);
}

/* Insert a user agent key string, mapped to an auto incremented value.
 *
 * If the given key exists, its value is returned.
 * On error, -1 is returned.
 * On success the value of the key inserted is returned */
int
ht_insert_agent_key (const char *key)
{
  int value = -1;
  khash_t (si32) * hash = ht_agent_keys;

  if (!hash)
    return -1;

  if ((value = get_si32 (hash, key)) != -1)
    return value;

  return ins_si32_ai (hash, key);
}

/* Insert a user agent int key, mapped to a user agent string value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_agent_value (int key, const char *value)
{
  khash_t (is32) * hash = ht_agent_vals;

  if (!hash)
    return -1;

  return ins_is32 (hash, key, value);
}

/* Insert a keymap string key.
 *
 * If the given key exists, its value is returned.
 * On error, -1 is returned.
 * On success the value of the key inserted is returned */
int
ht_insert_keymap (GModule module, const char *key)
{
  int value = -1;
  khash_t (si32) * hash = get_hash (module, MTRC_KEYMAP);

  if (!hash)
    return -1;

  if ((value = get_si32 (hash, key)) != -1)
    return value;

  return ins_si32_ai (hash, key);
}

/* Insert a datamap int key and string value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_datamap (GModule module, int key, const char *value)
{
  khash_t (is32) * hash = get_hash (module, MTRC_DATAMAP);

  if (!hash)
    return -1;

  return ins_is32 (hash, key, value);
}

/* Insert a rootmap int key from the keymap store mapped to its string value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_rootmap (GModule module, int key, const char *value)
{
  khash_t (is32) * hash = get_hash (module, MTRC_ROOTMAP);

  if (!hash)
    return -1;

  return ins_is32 (hash, key, value);
}

/* Insert a uniqmap string key.
 *
 * If the given key exists, 0 is returned.
 * On error, -1 is returned.
 * On success the value of the key inserted is returned */
int
ht_insert_uniqmap (GModule module, const char *key)
{
  int value = -1;
  khash_t (si32) * hash = get_hash (module, MTRC_UNIQMAP);

  if (!hash)
    return -1;

  if ((value = get_si32 (hash, key)) != -1)
    return 0;

  return ins_si32_ai (hash, key);
}

/* Insert a data int key mapped to the corresponding int root key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_root (GModule module, int key, int value)
{
  khash_t (ii32) * hash = get_hash (module, MTRC_ROOT);

  if (!hash)
    return -1;

  return ins_ii32 (hash, key, value);
}

/* Insert meta data counters from a string key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_meta_data (GModule module, const char *key, uint64_t value)
{
  khash_t (su64) * hash = get_hash (module, MTRC_METADATA);

  if (!hash)
    return -1;

  return inc_su64 (hash, key, value);
}

/* Increases hits counter from an int key.
 *
 * On error, -1 is returned.
 * On success the inserted value is returned */
int
ht_insert_hits (GModule module, int key, int inc)
{
  khash_t (ii32) * hash = get_hash (module, MTRC_HITS);

  if (!hash)
    return -1;

  return inc_ii32 (hash, key, inc);
}

/* Increases visitors counter from an int key.
 *
 * On error, -1 is returned.
 * On success the inserted value is returned */
int
ht_insert_visitor (GModule module, int key, int inc)
{
  khash_t (ii32) * hash = get_hash (module, MTRC_VISITORS);

  if (!hash)
    return -1;

  return inc_ii32 (hash, key, inc);
}

/* Increases bandwidth counter from an int key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_bw (GModule module, int key, uint64_t inc)
{
  khash_t (iu64) * hash = get_hash (module, MTRC_BW);

  if (!hash)
    return -1;

  return inc_iu64 (hash, key, inc);
}

/* Increases cumulative time served counter from an int key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_cumts (GModule module, int key, uint64_t inc)
{
  khash_t (iu64) * hash = get_hash (module, MTRC_CUMTS);

  if (!hash)
    return -1;

  return inc_iu64 (hash, key, inc);
}

/* Insert the maximum time served counter from an int key.
 * Note: it compares the current value with the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_maxts (GModule module, int key, uint64_t value)
{
  uint64_t curvalue = 0;
  khash_t (iu64) * hash = get_hash (module, MTRC_MAXTS);

  if (!hash)
    return -1;

  if ((curvalue = get_iu64 (hash, key)) < value)
    ins_iu64 (hash, key, value);

  return 0;
}

/* Insert a method given an int key and string value.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
int
ht_insert_method (GModule module, int key, const char *value)
{
  khash_t (is32) * hash = get_hash (module, MTRC_METHODS);

  if (!hash)
    return -1;

  return ins_is32 (hash, key, value);
}

/* Insert a protocol given an int key and string value.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
int
ht_insert_protocol (GModule module, int key, const char *value)
{
  khash_t (is32) * hash = get_hash (module, MTRC_PROTOCOLS);

  if (!hash)
    return -1;

  return ins_is32 (hash, key, value);
}

/* Insert an agent for a hostname given an int key and int value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_agent (GModule module, int key, int value)
{
  khash_t (igsl) * hash = get_hash (module, MTRC_AGENTS);

  if (!hash)
    return -1;

  return ins_igsl (hash, key, value);
}

/* Insert an IP hostname mapped to the corresponding hostname.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
int
ht_insert_hostname (const char *ip, const char *host)
{
  khash_t (ss32) * hash = ht_hostnames;

  if (!hash)
    return -1;

  return ins_ss32 (hash, ip, host);
}

/* Get the number of elements in a datamap.
 *
 * Return -1 if the operation fails, else number of elements. */
uint32_t
ht_get_size_datamap (GModule module)
{
  khash_t (is32) * hash = get_hash (module, MTRC_DATAMAP);

  if (!hash)
    return 0;

  return kh_size (hash);
}

/* Get the number of elements in a uniqmap.
 *
 * On error, 0 is returned.
 * On success the number of elements in MTRC_UNIQMAP is returned */
uint32_t
ht_get_size_uniqmap (GModule module)
{
  khash_t (is32) * hash = get_hash (module, MTRC_UNIQMAP);

  if (!hash)
    return 0;

  return kh_size (hash);
}

/* Get the string data value of a given int key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_datamap (GModule module, int key)
{
  khash_t (is32) * hash = get_hash (module, MTRC_DATAMAP);

  if (!hash)
    return NULL;

  return get_is32 (hash, key);
}

/* Get the int value from MTRC_KEYMAP given a string key.
 *
 * On error, -1 is returned.
 * On success the int value for the given key is returned */
int
ht_get_keymap (GModule module, const char *key)
{
  khash_t (si32) * hash = get_hash (module, MTRC_KEYMAP);

  if (!hash)
    return -1;

  return get_si32 (hash, key);
}

/* Get the int value from MTRC_UNIQMAP given a string key.
 *
 * On error, -1 is returned.
 * On success the int value for the given key is returned */
int
ht_get_uniqmap (GModule module, const char *key)
{
  khash_t (si32) * hash = get_hash (module, MTRC_UNIQMAP);

  if (!hash)
    return -1;

  return get_si32 (hash, key);
}

/* Get the string root from MTRC_ROOTMAP given an int data key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_root (GModule module, int key)
{
  int root_key = 0;
  khash_t (ii32) * hashroot = get_hash (module, MTRC_ROOT);
  khash_t (is32) * hashrootmap = get_hash (module, MTRC_ROOTMAP);

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
int
ht_get_visitors (GModule module, int key)
{
  khash_t (ii32) * hash = get_hash (module, MTRC_VISITORS);

  if (!hash)
    return -1;

  return get_ii32 (hash, key);
}

/* Get the int visitors value from MTRC_VISITORS given an int key.
 *
 * If key is not found, 0 is returned.
 * On error, -1 is returned.
 * On success the int value for the given key is returned */
int
ht_get_hits (GModule module, int key)
{
  khash_t (ii32) * hash = get_hash (module, MTRC_HITS);

  if (!hash)
    return -1;

  return get_ii32 (hash, key);
}

/* Get the uint64_t value from MTRC_BW given an int key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_bw (GModule module, int key)
{
  khash_t (iu64) * hash = get_hash (module, MTRC_BW);

  if (!hash)
    return 0;

  return get_iu64 (hash, key);
}

/* Get the uint64_t value from MTRC_CUMTS given an int key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_cumts (GModule module, int key)
{
  khash_t (iu64) * hash = get_hash (module, MTRC_CUMTS);

  if (!hash)
    return 0;

  return get_iu64 (hash, key);
}

/* Get the uint64_t value from MTRC_MAXTS given an int key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_maxts (GModule module, int key)
{
  khash_t (iu64) * hash = get_hash (module, MTRC_MAXTS);

  if (!hash)
    return 0;

  return get_iu64 (hash, key);
}

/* Get the string value from MTRC_METHODS given an int key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_method (GModule module, int key)
{
  khash_t (is32) * hash = get_hash (module, MTRC_METHODS);

  if (!hash)
    return NULL;

  return get_is32 (hash, key);
}

/* Get the string value from MTRC_PROTOCOLS given an int key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_protocol (GModule module, int key)
{
  khash_t (is32) * hash = get_hash (module, MTRC_PROTOCOLS);

  if (!hash)
    return NULL;

  return get_is32 (hash, key);
}

/* Get the string value from ht_hostnames given a string key (IP).
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_hostname (const char *host)
{
  khash_t (ss32) * hash = ht_hostnames;

  if (!hash)
    return NULL;

  return get_ss32 (hash, host);
}

/* Get the string value from ht_agent_vals (user agent) given an int key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_host_agent_val (int key)
{
  khash_t (is32) * hash = ht_agent_vals;

  if (!hash)
    return NULL;

  return get_is32 (hash, key);
}

/* Get the list value from MTRC_AGENTS given an int key.
 *
 * On error, or if key is not found, NULL is returned.
 * On success the GSLList value for the given key is returned */
GSLList *
ht_get_host_agent_list (GModule module, int key)
{
  khash_t (igsl) * hash = get_hash (module, MTRC_AGENTS);
  GSLList *list;

  if ((list = get_igsl (hash, key)))
    return list;
  return NULL;
}

/* Get the meta data uint64_t from MTRC_METADATA given a string key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_meta_data (GModule module, const char *key)
{
  khash_t (su64) * hash = get_hash (module, MTRC_METADATA);

  return get_su64 (hash, key);
}

/* Set the maximum and minimum values found on an integer key and
 * integer value found on the MTRC_VISITORS hash structure.
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_hits_min_max (GModule module, int *min, int *max)
{
  khash_t (ii32) * hash = get_hash (module, MTRC_HITS);

  if (!hash)
    return;

  get_ii32_min_max (hash, min, max);
}

/* Set the maximum and minimum values found on an integer key and
 * integer value found on the MTRC_VISITORS hash structure.
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_visitors_min_max (GModule module, int *min, int *max)
{
  khash_t (ii32) * hash = get_hash (module, MTRC_VISITORS);

  if (!hash)
    return;

  get_ii32_min_max (hash, min, max);
}

/* Set the maximum and minimum values found on an integer key and
 * a uint64_t value found on the MTRC_BW hash structure.
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_bw_min_max (GModule module, uint64_t * min, uint64_t * max)
{
  khash_t (iu64) * hash = get_hash (module, MTRC_BW);

  if (!hash)
    return;

  get_iu64_min_max (hash, min, max);
}

/* Set the maximum and minimum values found on an integer key and
 * a uint64_t value found on the MTRC_CUMTS hash structure.
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_cumts_min_max (GModule module, uint64_t * min, uint64_t * max)
{
  khash_t (iu64) * hash = get_hash (module, MTRC_CUMTS);

  if (!hash)
    return;

  get_iu64_min_max (hash, min, max);
}

/* Set the maximum and minimum values found on an integer key and
 * a uint64_t value found on the MTRC_MAXTS hash structure.
 *
 * If the hash structure is empty, no values are set.
 * On success the minimum and maximum values are set. */
void
ht_get_maxts_min_max (GModule module, uint64_t * min, uint64_t * max)
{
  khash_t (iu64) * hash = get_hash (module, MTRC_MAXTS);

  if (!hash)
    return;

  get_iu64_min_max (hash, min, max);
}

/* A wrapper to initialize a raw data structure.
 *
 * On success a GRawData structure is returned. */
static GRawData *
init_new_raw_data (GModule module, uint32_t ht_size)
{
  GRawData *raw_data;

  raw_data = new_grawdata ();
  raw_data->idx = 0;
  raw_data->module = module;
  raw_data->size = ht_size;
  raw_data->items = new_grawdata_item (ht_size);

  return raw_data;
}

/* Store the key/value pairs from a hash table into raw_data and sorts
 * the hits (numeric) value.
 *
 * On error, NULL is returned.
 * On success the GRawData sorted is returned */
static GRawData *
parse_raw_num_data (GModule module)
{
  GRawData *raw_data;
  khiter_t key;
  uint32_t ht_size = 0;

  khash_t (ii32) * hash = get_hash (module, MTRC_HITS);
  if (!hash)
    return NULL;

  ht_size = kh_size (hash);
  raw_data = init_new_raw_data (module, ht_size);
  raw_data->type = INTEGER;
  for (key = kh_begin (hash); key != kh_end (hash); ++key) {
    if (!kh_exist (hash, key))
      continue;

    raw_data->items[raw_data->idx].key = kh_key (hash, key);
    raw_data->items[raw_data->idx].value.ivalue = kh_value (hash, key);
    raw_data->idx++;
  }

  sort_raw_num_data (raw_data, raw_data->idx);

  return raw_data;
}

/* Store the key/value pairs from a hash table into raw_data and sorts
 * the data (string) value.
 *
 * On error, NULL is returned.
 * On success the GRawData sorted is returned */
static GRawData *
parse_raw_str_data (GModule module)
{
  GRawData *raw_data;
  khiter_t key;
  uint32_t ht_size = 0;

  khash_t (is32) * hash = get_hash (module, MTRC_DATAMAP);
  if (!hash)
    return NULL;

  ht_size = kh_size (hash);
  raw_data = init_new_raw_data (module, ht_size);
  raw_data->type = STRING;
  for (key = kh_begin (hash); key != kh_end (hash); ++key) {
    if (!kh_exist (hash, key))
      continue;

    raw_data->items[raw_data->idx].key = kh_key (hash, key);
    raw_data->items[raw_data->idx].value.svalue = kh_value (hash, key);
    raw_data->idx++;
  }

  sort_raw_str_data (raw_data, raw_data->idx);

  return raw_data;
}

/* Entry point to load the raw data from the data store into our
 * GRawData structure.
 *
 * On error, NULL is returned.
 * On success the GRawData sorted is returned */
GRawData *
parse_raw_data (GModule module)
{
  GRawData *raw_data;

  switch (module) {
  case VISITORS:
    raw_data = parse_raw_str_data (module);
    break;
  default:
    raw_data = parse_raw_num_data (module);
  }
  return raw_data;
}
