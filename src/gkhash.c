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

/* Hash tables storage */
static GKHashStorage *gkh_storage;

/* *INDENT-OFF* */
/* Hash tables used across the whole app */
static khash_t (is32) *ht_agent_vals  = NULL;
static khash_t (iui8) *ht_dates       = NULL;
static khash_t (si32) *ht_agent_keys  = NULL;
static khash_t (si32) *ht_seqs        = NULL;
static khash_t (si32) *ht_unique_keys = NULL;
static khash_t (ss32) *ht_hostnames   = NULL;

/* overall counters */
static khash_t (si32) *ht_cnt_overall = NULL;
static khash_t (ii32) *ht_last_parse  = NULL;
static khash_t (ii32) *ht_cnt_valid   = NULL; /* date key 20200101 -> 10,000 */
static khash_t (iu64) *ht_cnt_bw      = NULL; /* date key 20200101 -> 45,200 */


/* String metric types to enumerated modules */
static GEnum enum_metric_types[] = {
  {"II32"  , MTRC_TYPE_II32}  ,
  {"IS32"  , MTRC_TYPE_IS32}  ,
  {"IU64"  , MTRC_TYPE_IU64}  ,
  {"SI32"  , MTRC_TYPE_SI32}  ,
  {"SS32"  , MTRC_TYPE_SS32}  ,
  {"IGSL"  , MTRC_TYPE_IGSL}  ,
  {"SGSL"  , MTRC_TYPE_SGSL}  ,
  {"SU64"  , MTRC_TYPE_SU64}  ,
  {"IUI8"  , MTRC_TYPE_IUI8}  ,
};
/* *INDENT-ON* */

static GKHashStorage *
new_gkhstorage (uint32_t size)
{
  GKHashStorage *storage = xcalloc (size, sizeof (GKHashStorage));
  return storage;
}

/* Get the module string value given a metric enum value.
 *
 * On error, NULL is returned.
 * On success, the string module value is returned. */
static char *
get_mtr_type_str (GSMetricType type)
{
  return enum2str (enum_metric_types, ARRAY_SIZE (enum_metric_types), type);
}

/* Initialize a new uint32_t key - uint32_t value hash table */
static
khash_t (ii32) *
new_ii32_ht (void)
{
  khash_t (ii32) * h = kh_init (ii32);
  return h;
}

/* Initialize a new uint32_t key - uint8_t value hash table */
static
khash_t (iui8) *
new_iui8_ht (void)
{
  khash_t (iui8) * h = kh_init (iui8);
  return h;
}

/* Initialize a new uint32_t key - string value hash table */
static
khash_t (is32) *
new_is32_ht (void)
{
  khash_t (is32) * h = kh_init (is32);
  return h;
}

/* Initialize a new uint32_t key - uint64_t value hash table */
static
khash_t (iu64) *
new_iu64_ht (void)
{
  khash_t (iu64) * h = kh_init (iu64);
  return h;
}

/* Initialize a new string key - uint32_t value hash table */
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

/* Initialize a new string key - GSLList value hash table */
static
khash_t (sgsl) *
new_sgsl_ht (void)
{
  khash_t (sgsl) * h = kh_init (sgsl);
  return h;
}

/* Initialize a new uint32_t key - GSLList value hash table */
static
khash_t (igsl) *
new_igsl_ht (void)
{
  khash_t (igsl) * h = kh_init (igsl);
  return h;
}

/* Initialize a new string key - uint64_t value hash table */
static
khash_t (su64) *
new_su64_ht (void)
{
  khash_t (su64) * h = kh_init (su64);
  return h;
}

/* Destroys both the hash structure and the keys for a
 * string key - uint32_t value hash */
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

/* Destroys the hash structure */
static void
des_iui8 (khash_t (iui8) * hash)
{
  if (!hash)
    return;
  kh_destroy (iui8, hash);
}

/* Frees both the hash structure and its GSLList
 * values */
static void
del_sgsl_free (khash_t (sgsl) * hash)
{
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
des_sgsl_free (khash_t (sgsl) * hash)
{
  if (!hash)
    return;

  del_sgsl_free (hash);
  kh_destroy (sgsl, hash);
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
    {MTRC_KEYMAP    , MTRC_TYPE_SI32 , {.si32 = new_si32_ht ()}, NULL} ,
    {MTRC_KEYMAPUQ  , MTRC_TYPE_SGSL , {.sgsl = new_sgsl_ht ()}, NULL} ,
    {MTRC_ROOTMAP   , MTRC_TYPE_IS32 , {.is32 = new_is32_ht ()}, NULL} ,
    {MTRC_DATAMAP   , MTRC_TYPE_IS32 , {.is32 = new_is32_ht ()}, NULL} ,
    {MTRC_UNIQMAP   , MTRC_TYPE_SI32 , {.si32 = new_si32_ht ()}, NULL} ,
    {MTRC_ROOT      , MTRC_TYPE_II32 , {.ii32 = new_ii32_ht ()}, NULL} ,
    {MTRC_HITS      , MTRC_TYPE_II32 , {.ii32 = new_ii32_ht ()}, NULL} ,
    {MTRC_VISITORS  , MTRC_TYPE_II32 , {.ii32 = new_ii32_ht ()}, NULL} ,
    {MTRC_BW        , MTRC_TYPE_IU64 , {.iu64 = new_iu64_ht ()}, NULL} ,
    {MTRC_CUMTS     , MTRC_TYPE_IU64 , {.iu64 = new_iu64_ht ()}, NULL} ,
    {MTRC_MAXTS     , MTRC_TYPE_IU64 , {.iu64 = new_iu64_ht ()}, NULL} ,
    {MTRC_METHODS   , MTRC_TYPE_IS32 , {.is32 = new_is32_ht ()}, NULL} ,
    {MTRC_PROTOCOLS , MTRC_TYPE_IS32 , {.is32 = new_is32_ht ()}, NULL} ,
    {MTRC_AGENTS    , MTRC_TYPE_IGSL , {.igsl = new_igsl_ht ()}, NULL} ,
    {MTRC_METADATA  , MTRC_TYPE_SU64 , {.su64 = new_su64_ht ()}, NULL} ,
  };
  /* *INDENT-ON* */

  n = ARRAY_SIZE (metrics);
  for (i = 0; i < n; i++) {
    gkh_storage[module].metrics[i] = metrics[i];
  }
}

static void
free_metric_type (GKHashMetric mtrc)
{
  /* Determine the hash structure type */
  switch (mtrc.type) {
  case MTRC_TYPE_IUI8:
    des_iui8 (mtrc.iui8);
    break;
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
  case MTRC_TYPE_SGSL:
    des_sgsl_free (mtrc.sgsl);
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
    case MTRC_TYPE_IUI8:
      hash = mtrc.iui8;
      break;
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
    case MTRC_TYPE_SGSL:
      hash = mtrc.sgsl;
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

/* Destroys the hash structure allocated metrics on each render */
void
free_sgls_metrics (GModule module)
{
  khash_t (sgsl) * hash = NULL;

  hash = get_hash (module, MTRC_KEYMAPUQ);
  del_sgsl_free (hash);
}

/* Insert a string key and the corresponding uint32_t value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
static int
ins_si32 (khash_t (si32) * hash, const char *key, uint32_t value)
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

/* Insert a string key and the corresponding uint32_t value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
static uint32_t
ins_si32_inc (khash_t (si32) * hash, const char *key,
              uint32_t (*cb) (const char *), const char *seqk)
{
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
ins_is32 (khash_t (is32) * hash, uint32_t key, const char *value)
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

/* Insert an uint32_t key and an uint32_t value
 * Note: If the key exists, its value is replaced by the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
ins_ii32 (khash_t (ii32) * hash, uint32_t key, uint32_t value)
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

/* Insert an uint32_t key and an uint8_t value
 * Note: If the key exists, its value is replaced by the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
ins_iui8 (khash_t (iui8) * hash, uint32_t key, uint8_t value)
{
  khint_t k;
  int ret;

  if (!hash)
    return -1;

  k = kh_put (iui8, hash, key, &ret);
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
ins_iu64 (khash_t (iu64) * hash, uint32_t key, uint64_t value)
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

/* Insert a string key and a uint64_t value
 * Note: If the key exists, its value is replaced by the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
ins_su64 (khash_t (su64) * hash, const char *key, uint64_t value)
{
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

/* Increase an uint32_t value given an uint32_t key.
 * Note: If the key exists, its value is increased by the given inc.
 *
 * On error, 0 is returned.
 * On success the inserted value is returned */
static uint32_t
inc_ii32 (khash_t (ii32) * hash, uint32_t key, uint32_t inc)
{
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
inc_su64 (khash_t (su64) * hash, const char *key, uint64_t inc)
{
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
inc_iu64 (khash_t (iu64) * hash, uint32_t key, uint64_t inc)
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

/* Increase a uint32_t value given a string key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static uint32_t
inc_si32 (khash_t (si32) * hash, const char *key, uint32_t inc)
{
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

/* Insert a string key and auto increment uint32_t value.
 *
 * On error, 0 is returned.
 * On success the value of the key inserted is returned */
static uint32_t
ins_si32_ai (khash_t (si32) * hash, const char *key)
{
  uint32_t size = 0, value = 0;

  if (!hash)
    return 0;

  size = kh_size (hash);
  /* the auto increment value starts at SIZE (hash table) + 1 */
  value = size > 0 ? size + 1 : 1;

  if (ins_si32 (hash, key, value) == -1)
    return 0;

  return value;
}

/* Compare if the given needle is in the haystack
 *
 * if equal, 1 is returned, else 0 */
static int
find_int_key_in_list (void *data, void *needle)
{
  return (*(uint32_t *) data) == (*(uint32_t *) needle) ? 1 : 0;
}

/* Insert an int key and the corresponding GSLList (Single linked-list) value.
 * Note: If the key exists within the list, the value is not appended.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
ins_igsl (khash_t (igsl) * hash, uint32_t key, uint32_t value)
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
ins_sgsl (khash_t (sgsl) * hash, const char *key, uint32_t value)
{
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
set_db_path (const char *fn)
{
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
get_filename (GModule module, GKHashMetric mtrc)
{
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
restore_si32 (khash_t (si32) * hash, const char *fn)
{
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
persist_si32 (khash_t (si32) * hash, const char *fn)
{
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
restore_is32 (khash_t (is32) * hash, const char *fn)
{
  tpl_node *tn;
  char *val = NULL;
  char fmt[] = "A(us)";
  uint32_t key;

  tn = tpl_map (fmt, &key, &val);
  tpl_load (tn, TPL_FILE, fn);
  while (tpl_unpack (tn, 1) > 0) {
    ins_is32 (hash, key, val);
    free (val);
  }
  tpl_free (tn);
}

static void
persist_is32 (khash_t (is32) * hash, const char *fn)
{
  tpl_node *tn;
  khint_t k;
  uint32_t key;
  char *val = NULL;
  char fmt[] = "A(us)";

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

static void
restore_ii32 (khash_t (ii32) * hash, const char *fn)
{
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
persist_ii32 (khash_t (ii32) * hash, const char *fn)
{
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

static void
restore_iu64 (khash_t (iu64) * hash, const char *fn)
{
  tpl_node *tn;
  uint32_t key;
  uint64_t val;
  char fmt[] = "A(uU)";

  tn = tpl_map (fmt, &key, &val);
  tpl_load (tn, TPL_FILE, fn);
  while (tpl_unpack (tn, 1) > 0) {
    ins_iu64 (hash, key, val);
  }
  tpl_free (tn);
}

static void
persist_iu64 (khash_t (iu64) * hash, const char *fn)
{
  tpl_node *tn;
  khint_t k;
  uint32_t key;
  uint64_t val;
  char fmt[] = "A(uU)";

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

static void
restore_su64 (khash_t (su64) * hash, const char *fn)
{
  tpl_node *tn;
  char *key = NULL;
  char fmt[] = "A(sU)";
  uint64_t val;

  tn = tpl_map (fmt, &key, &val);
  tpl_load (tn, TPL_FILE, fn);
  while (tpl_unpack (tn, 1) > 0) {
    ins_su64 (hash, key, val);
    free (key);
  }
  tpl_free (tn);
}

static void
persist_su64 (khash_t (su64) * hash, const char *fn)
{
  tpl_node *tn;
  khint_t k;
  const char *key = NULL;
  char fmt[] = "A(sU)";
  uint64_t val;

  if (!hash || kh_size (hash) == 0)
    return;

  tn = tpl_map (fmt, &key, &val);
  for (k = 0; k < kh_end (hash); ++k) {
    if (!kh_exist (hash, k) || !(key = kh_key (hash, k)))
      continue;
    val = kh_value (hash, k);
    tpl_pack (tn, 1);
  }

  tpl_dump (tn, TPL_FILE, fn);
  tpl_free (tn);
}

static void
restore_igsl (khash_t (igsl) * hash, const char *fn)
{
  tpl_node *tn;
  uint32_t key, val;
  char fmt[] = "A(uu)";

  tn = tpl_map (fmt, &key, &val);
  tpl_load (tn, TPL_FILE, fn);
  while (tpl_unpack (tn, 1) > 0) {
    ins_igsl (hash, key, val);
  }
  tpl_free (tn);
}


static void
persist_igsl (khash_t (igsl) * hash, const char *fn)
{
  GSLList *node;
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
    node = kh_value (hash, k);
    while (node) {
      val = (*(uint32_t *) node->data);
      node = node->next;
    }
    tpl_pack (tn, 1);
  }

  tpl_dump (tn, TPL_FILE, fn);
  tpl_free (tn);
}

static char *
check_restore_path (const char *fn)
{
  char *path = set_db_path (fn);
  if (access (path, F_OK) != -1)
    return path;

  LOG_DEBUG (("DB file %s doesn't exist. %s\n", path, strerror (errno)));
  free (path);
  return NULL;
}

static void
restore_by_type (GKHashMetric mtrc, const char *fn)
{
  char *path = NULL;

  if (!(path = check_restore_path (fn)))
    goto clean;

  switch (mtrc.type) {
  case MTRC_TYPE_II32:
    restore_ii32 (mtrc.ii32, path);
    break;
  case MTRC_TYPE_IS32:
    restore_is32 (mtrc.is32, path);
    break;
  case MTRC_TYPE_IU64:
    restore_iu64 (mtrc.iu64, path);
    break;
  case MTRC_TYPE_IGSL:
    restore_igsl (mtrc.igsl, path);
    break;
  case MTRC_TYPE_SU64:
    restore_su64 (mtrc.su64, path);
    break;
  case MTRC_TYPE_SI32:
    restore_si32 (mtrc.si32, path);
    break;
  default:
    break;
  }
clean:
  free (path);
}

static void
restore_metric_type (GModule module, GKHashMetric mtrc)
{
  char *fn = NULL;

  fn = get_filename (module, mtrc);
  restore_by_type (mtrc, fn);
  free (fn);
}

static void
restore_data (void)
{
  GModule module;
  int i, n = 0;
  size_t idx = 0;

  /* *INDENT-OFF* */
  GKHashMetric metrics[] = {
    {0 , MTRC_TYPE_SI32 , {.si32 = ht_unique_keys }, "SI32_UNIQUE_KEYS.db"} ,
    {0 , MTRC_TYPE_IS32 , {.is32 = ht_agent_vals  }, "IS32_AGENT_VALS.db"} ,
    {0 , MTRC_TYPE_SI32 , {.si32 = ht_agent_keys  }, "SI32_AGENT_KEYS.db"} ,
    {0 , MTRC_TYPE_SI32 , {.si32 = ht_seqs        }, "SI32_SEQS.db"} ,
    {0 , MTRC_TYPE_SI32 , {.si32 = ht_cnt_overall }, "SI32_CNT_OVERALL.db"} ,
    {0 , MTRC_TYPE_II32 , {.ii32 = ht_last_parse  }, "II32_LAST_PARSE.db"} ,
    {0 , MTRC_TYPE_II32 , {.ii32 = ht_cnt_valid   }, "II32_CNT_VALID.db"} ,
    {0 , MTRC_TYPE_IU64 , {.iu64 = ht_cnt_bw      }, "IU64_CNT_BW.db"} ,
  };
  /* *INDENT-ON* */

  n = ARRAY_SIZE (metrics);
  for (i = 0; i < n; i++) {
    restore_by_type (metrics[i], metrics[i].filename);
  }

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    for (i = 0; i < GSMTRC_TOTAL; i++) {
      restore_metric_type (module, gkh_storage[module].metrics[i]);
    }
  }
}

static void
persist_by_type (GKHashMetric mtrc, const char *fn)
{
  char *path = NULL;
  path = set_db_path (fn);

  switch (mtrc.type) {
  case MTRC_TYPE_II32:
    persist_ii32 (mtrc.ii32, path);
    break;
  case MTRC_TYPE_IS32:
    persist_is32 (mtrc.is32, path);
    break;
  case MTRC_TYPE_IU64:
    persist_iu64 (mtrc.iu64, path);
    break;
  case MTRC_TYPE_IGSL:
    persist_igsl (mtrc.igsl, path);
    break;
  case MTRC_TYPE_SU64:
    persist_su64 (mtrc.su64, path);
    break;
  case MTRC_TYPE_SI32:
    persist_si32 (mtrc.si32, path);
    break;
  default:
    break;
  }
  free (path);
}

static void
persist_metric_type (GModule module, GKHashMetric mtrc)
{
  char *fn = NULL;
  fn = get_filename (module, mtrc);
  persist_by_type (mtrc, fn);
  free (fn);
}

static void
persist_overall (void)
{
  int n = 0, i;
  /* *INDENT-OFF* */
  GKHashMetric metrics[] = {
    {0 , MTRC_TYPE_SI32 , {.si32 = ht_unique_keys } , "SI32_UNIQUE_KEYS.db" } ,
    {0 , MTRC_TYPE_IS32 , {.is32 = ht_agent_vals  } , "IS32_AGENT_VALS.db"  } ,
    {0 , MTRC_TYPE_SI32 , {.si32 = ht_agent_keys  } , "SI32_AGENT_KEYS.db"  } ,
    {0 , MTRC_TYPE_SI32 , {.si32 = ht_seqs        } , "SI32_SEQS.db"        } ,
    {0 , MTRC_TYPE_SI32 , {.si32 = ht_cnt_overall } , "SI32_CNT_OVERALL.db" } ,
    {0 , MTRC_TYPE_II32 , {.ii32 = ht_last_parse  } , "II32_LAST_PARSE.db"  } ,
    {0 , MTRC_TYPE_II32 , {.ii32 = ht_cnt_valid   } , "II32_CNT_VALID.db"   } ,
    {0 , MTRC_TYPE_IU64 , {.iu64 = ht_cnt_bw      } , "IU64_CNT_BW.db"      } ,
  };
  /* *INDENT-ON* */

  n = ARRAY_SIZE (metrics);
  for (i = 0; i < n; i++) {
    persist_by_type (metrics[i], metrics[i].filename);
  }
}

/* Destroys the hash structure allocated metrics */
static void
persist_data (void)
{
  GModule module;
  int i;
  size_t idx = 0;

  persist_overall ();

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    for (i = 0; i < GSMTRC_TOTAL; i++) {
      persist_metric_type (module, gkh_storage[module].metrics[i]);
    }
  }
}

/* Get the uint32_t value of a given string key.
 *
 * On error, 0 is returned.
 * On success the uint32_t value for the given key is returned */
static uint32_t
get_si32 (khash_t (si32) * hash, const char *key)
{
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
get_is32 (khash_t (is32) * hash, uint32_t key)
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

/* Get the uint32_t value of a given uint32_t key.
 *
 * If key is not found, 0 is returned.
 * On error, -1 is returned.
 * On success the uint32_t value for the given key is returned */
static uint32_t
get_ii32 (khash_t (ii32) * hash, uint32_t key)
{
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
get_iu64 (khash_t (iu64) * hash, uint32_t key)
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

/* Get the GSLList value of a given uint32_t key.
 *
 * On error, or if key is not found, NULL is returned.
 * On success the GSLList value for the given key is returned */
static GSLList *
get_igsl (khash_t (igsl) * hash, uint32_t key)
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
get_ii32_min_max (khash_t (ii32) * hash, uint32_t * min, uint32_t * max)
{
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

/* Get the uint32_t value of a given string key.
 *
 * On error, 0 is returned.
 * On success the uint32_t value for the given key is returned */
static uint32_t
get_iui8 (khash_t (iui8) * hash, uint32_t key)
{
  khint_t k;

  if (!hash)
    return 0;

  k = kh_get (iui8, hash, key);
  /* key found, return current value */
  if (k != kh_end (hash))
    return kh_val (hash, k);

  return 0;
}

uint32_t
ht_get_date (uint32_t date)
{
  return get_iui8 (ht_dates, date);
}

/* Get the number of elements in a dates hash.
 *
 * Return 0 if the operation fails, else number of elements. */
uint32_t
ht_get_size_dates (void)
{
  khash_t (iui8) * hash = ht_dates;

  if (!hash)
    return 0;

  return kh_size (hash);
}

uint32_t
ht_get_excluded_ips (void)
{
  khash_t (si32) * hash = ht_cnt_overall;

  if (!hash)
    return 0;

  return get_si32 (hash, "excluded_ip");
}

uint32_t
ht_get_invalid (void)
{
  khash_t (si32) * hash = ht_cnt_overall;

  if (!hash)
    return 0;

  return get_si32 (hash, "failed_requests");
}

uint32_t
ht_get_processed (void)
{
  khash_t (si32) * hash = ht_cnt_overall;

  if (!hash)
    return 0;

  return get_si32 (hash, "total_requests");
}

uint32_t
ht_sum_valid (void)
{
  khash_t (ii32) * hash = ht_cnt_valid;
  khint_t k;
  uint32_t val = 0;

  if (!hash)
    return 0;

  for (k = 0; k < kh_end (hash); ++k)
    if (kh_exist (hash, k))
      val += kh_value (hash, k);

  return val;
}

uint64_t
ht_sum_bw (void)
{
  khash_t (iu64) * hash = ht_cnt_bw;
  khint_t k;
  uint64_t val = 0;

  if (!hash)
    return 0;

  for (k = 0; k < kh_end (hash); ++k)
    if (kh_exist (hash, k))
      val += kh_value (hash, k);

  return val;
}

uint32_t
ht_insert_last_parse (uint32_t key, uint32_t value)
{
  khash_t (ii32) * hash = ht_last_parse;

  if (!hash)
    return 0;

  return ins_ii32 (hash, key, value);
}

uint32_t
ht_insert_date (uint32_t key)
{
  khash_t (iui8) * hash = ht_dates;

  if (!hash)
    return 0;

  if (ins_iui8 (hash, key, 1) == -1)
    return 0;
  return key;
}

uint32_t
ht_inc_cnt_overall (const char *key, uint32_t val)
{
  khash_t (si32) * hash = ht_cnt_overall;

  if (!hash)
    return 0;

  if (get_si32 (hash, key) != 0)
    return inc_si32 (hash, key, val);
  return inc_si32 (hash, xstrdup (key), val);
}

uint32_t
ht_inc_cnt_valid (uint32_t key, uint32_t inc)
{
  khash_t (ii32) * hash = ht_cnt_valid;

  if (!hash)
    return 0;

  return inc_ii32 (hash, key, inc);
}

uint32_t
ht_inc_cnt_bw (uint32_t key, uint64_t inc)
{
  khash_t (iu64) * hash = ht_cnt_bw;

  if (!hash)
    return 0;

  return inc_iu64 (hash, key, inc);
}

/* Increases the unique key counter from a uint32_t key.
 *
 * On error, 0 is returned.
 * On success the inserted key is returned */
static uint32_t
ht_ins_seq (const char *key)
{
  khash_t (si32) * hash = ht_seqs;

  if (!hash)
    return 0;

  if (get_si32 (hash, key) != 0)
    return inc_si32 (hash, key, 1);
  return inc_si32 (hash, xstrdup (key), 1);
}

/* Increases the unique agent counter from a uint32_t key.
 *
 * On error, 0 is returned.
 * On success the inserted key is returned */
uint32_t
ht_insert_agent_seq (const char *key)
{
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
ht_insert_unique_key (const char *key)
{
  uint32_t val = 0;
  khash_t (si32) * hash = ht_unique_keys;

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
ht_insert_agent_key (const char *key)
{
  uint32_t val = 0;
  khash_t (si32) * hash = ht_agent_keys;

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
ht_insert_agent_value (uint32_t key, const char *value)
{
  khash_t (is32) * hash = ht_agent_vals;

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
ht_insert_keymap (GModule module, const char *key)
{
  uint32_t value = 0;
  khash_t (si32) * hash = get_hash (module, MTRC_KEYMAP);
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

/* Insert a datamap uint32_t key and string value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_datamap (GModule module, uint32_t key, const char *value)
{
  khash_t (is32) * hash = get_hash (module, MTRC_DATAMAP);

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
ht_insert_rootmap (GModule module, uint32_t key, const char *value)
{
  khash_t (is32) * hash = get_hash (module, MTRC_ROOTMAP);

  if (!hash)
    return -1;

  return ins_is32 (hash, key, value);
}

/* Insert a uniqmap string key.
 *
 * If the given key exists, 0 is returned.
 * On error, 0 is returned.
 * On success the value of the key inserted is returned */
uint32_t
ht_insert_uniqmap (GModule module, const char *key)
{
  khash_t (si32) * hash = get_hash (module, MTRC_UNIQMAP);

  if (!hash)
    return 0;

  if (get_si32 (hash, key) != 0)
    return 0;

  return ins_si32_ai (hash, key);
}

/* Insert a data uint32_t key mapped to the corresponding uint32_t root key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_root (GModule module, uint32_t key, uint32_t value)
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

/* Increases hits counter from a uint32_t key.
 *
 * On error, 0 is returned.
 * On success the inserted value is returned */
uint32_t
ht_insert_hits (GModule module, uint32_t key, uint32_t inc)
{
  khash_t (ii32) * hash = get_hash (module, MTRC_HITS);

  if (!hash)
    return 0;

  return inc_ii32 (hash, key, inc);
}

/* Increases visitors counter from a uint32_t key.
 *
 * On error, 0 is returned.
 * On success the inserted value is returned */
uint32_t
ht_insert_visitor (GModule module, uint32_t key, uint32_t inc)
{
  khash_t (ii32) * hash = get_hash (module, MTRC_VISITORS);

  if (!hash)
    return 0;

  return inc_ii32 (hash, key, inc);
}

/* Increases bandwidth counter from a uint32_t key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_bw (GModule module, uint32_t key, uint64_t inc)
{
  khash_t (iu64) * hash = get_hash (module, MTRC_BW);

  if (!hash)
    return -1;

  return inc_iu64 (hash, key, inc);
}

/* Increases cumulative time served counter from a uint32_t key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_cumts (GModule module, uint32_t key, uint64_t inc)
{
  khash_t (iu64) * hash = get_hash (module, MTRC_CUMTS);

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
ht_insert_maxts (GModule module, uint32_t key, uint64_t value)
{
  uint64_t curvalue = 0;
  khash_t (iu64) * hash = get_hash (module, MTRC_MAXTS);

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
ht_insert_method (GModule module, uint32_t key, const char *value)
{
  khash_t (is32) * hash = get_hash (module, MTRC_METHODS);

  if (!hash)
    return -1;

  return ins_is32 (hash, key, value);
}

/* Insert a protocol given an uint32_t key and string value.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
int
ht_insert_protocol (GModule module, uint32_t key, const char *value)
{
  khash_t (is32) * hash = get_hash (module, MTRC_PROTOCOLS);

  if (!hash)
    return -1;

  return ins_is32 (hash, key, value);
}

/* Insert an agent for a hostname given an uint32_t key and uint32_t value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_agent (GModule module, uint32_t key, uint32_t value)
{
  khash_t (igsl) * hash = get_hash (module, MTRC_AGENTS);

  if (!hash)
    return -1;

  return ins_igsl (hash, key, value);
}

/* Insert a keymap string key mapped to the corresponding uint32_t value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
ht_insert_keymap_list (GModule module, const char *key, uint32_t value)
{
  khash_t (sgsl) * hash = get_hash (module, MTRC_KEYMAPUQ);

  if (!hash)
    return -1;

  return ins_sgsl (hash, key, value);
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

uint32_t
ht_get_last_parse (uint32_t key)
{
  khash_t (ii32) * hash = ht_last_parse;

  if (!hash)
    return 0;

  return get_ii32 (hash, key);
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

/* Get the string data value of a given uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_datamap (GModule module, uint32_t key)
{
  khash_t (is32) * hash = get_hash (module, MTRC_DATAMAP);

  if (!hash)
    return NULL;

  return get_is32 (hash, key);
}

/* Get the uint32_t value from MTRC_KEYMAP given a string key.
 *
 * On error, 0 is returned.
 * On success the uint32_t value for the given key is returned */
uint32_t
ht_get_keymap (GModule module, const char *key)
{
  khash_t (si32) * hash = get_hash (module, MTRC_KEYMAP);

  if (!hash)
    return 0;

  return get_si32 (hash, key);
}

/* Get the uint32_t value from MTRC_UNIQMAP given a string key.
 *
 * On error, 0 is returned.
 * On success the uint32_t value for the given key is returned */
uint32_t
ht_get_uniqmap (GModule module, const char *key)
{
  khash_t (si32) * hash = get_hash (module, MTRC_UNIQMAP);

  if (!hash)
    return 0;

  return get_si32 (hash, key);
}

/* Get the string root from MTRC_ROOTMAP given an uint32_t data key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_root (GModule module, uint32_t key)
{
  uint32_t root_key = 0;
  khash_t (ii32) * hashroot = get_hash (module, MTRC_ROOT);
  khash_t (is32) * hashrootmap = get_hash (module, MTRC_ROOTMAP);

  if (!hashroot || !hashrootmap)
    return NULL;

  /* not found */
  if ((root_key = get_ii32 (hashroot, key)) == 0)
    return NULL;

  return get_is32 (hashrootmap, root_key);
}

/* Get the uint32_t visitors value from MTRC_VISITORS given an uint32_t key.
 *
 * If key is not found, 0 is returned.
 * On error, -1 is returned.
 * On success the uint32_t value for the given key is returned */
uint32_t
ht_get_visitors (GModule module, uint32_t key)
{
  khash_t (ii32) * hash = get_hash (module, MTRC_VISITORS);

  if (!hash)
    return 0;

  return get_ii32 (hash, key);
}

/* Get the uint32_t visitors value from MTRC_VISITORS given an uint32_t key.
 *
 * If key is not found, 0 is returned.
 * On error, 0 is returned.
 * On success the uint32_t value for the given key is returned */
uint32_t
ht_get_hits (GModule module, uint32_t key)
{
  khash_t (ii32) * hash = get_hash (module, MTRC_HITS);

  if (!hash)
    return 0;

  return get_ii32 (hash, key);
}

/* Get the uint64_t value from MTRC_BW given an uint32_t key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_bw (GModule module, uint32_t key)
{
  khash_t (iu64) * hash = get_hash (module, MTRC_BW);

  if (!hash)
    return 0;

  return get_iu64 (hash, key);
}

/* Get the uint64_t value from MTRC_CUMTS given an uint32_t key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_cumts (GModule module, uint32_t key)
{
  khash_t (iu64) * hash = get_hash (module, MTRC_CUMTS);

  if (!hash)
    return 0;

  return get_iu64 (hash, key);
}

/* Get the uint64_t value from MTRC_MAXTS given an uint32_t key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_maxts (GModule module, uint32_t key)
{
  khash_t (iu64) * hash = get_hash (module, MTRC_MAXTS);

  if (!hash)
    return 0;

  return get_iu64 (hash, key);
}

/* Get the string value from MTRC_METHODS given an uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_method (GModule module, uint32_t key)
{
  khash_t (is32) * hash = get_hash (module, MTRC_METHODS);

  if (!hash)
    return NULL;

  return get_is32 (hash, key);
}

/* Get the string value from MTRC_PROTOCOLS given an uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_protocol (GModule module, uint32_t key)
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

/* Get the string value from ht_agent_vals (user agent) given an uint32_t key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_host_agent_val (uint32_t key)
{
  khash_t (is32) * hash = ht_agent_vals;

  if (!hash)
    return NULL;

  return get_is32 (hash, key);
}

/* Get the list value from MTRC_AGENTS given an uint32_t key.
 *
 * On error, or if key is not found, NULL is returned.
 * On success the GSLList value for the given key is returned */
GSLList *
ht_get_host_agent_list (GModule module, uint32_t key)
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
ht_get_hits_min_max (GModule module, uint32_t * min, uint32_t * max)
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
ht_get_visitors_min_max (GModule module, uint32_t * min, uint32_t * max)
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

uint32_t *
get_sorted_dates (void)
{
  khiter_t key;
  uint32_t *dates = NULL;
  int i = 0;
  uint32_t size = 0;

  khash_t (iui8) * hash = ht_dates;
  if (!hash)
    return NULL;

  size = kh_size (hash);
  dates = xcalloc (size, sizeof (uint32_t));
  for (key = kh_begin (hash); key != kh_end (hash); ++key) {
    if (!kh_exist (hash, key))
      continue;
    dates[i++] = kh_key (hash, key);
  }
  qsort (dates, i, sizeof (uint32_t), cmp_ui32_asc);

  return dates;
}

static int
find_user_agent_key (void *data, void *needle)
{
  return (*(uint32_t *) data) == (*(uint32_t *) needle) ? 1 : 0;
}

static int
free_agent_list (uint32_t agent_nkey)
{
  khiter_t k, kv;
  khash_t (si32) * hash = ht_agent_keys;
  khash_t (is32) * hval = ht_agent_vals;

  if (!hash || !hval)
    return -1;

  for (k = kh_begin (hash); k != kh_end (hash); ++k) {
    if (!kh_exist (hash, k) || kh_val (hash, k) != agent_nkey)
      continue;

    free ((char *) kh_key (hash, k));
    kh_del (si32, hash, k);
  }

  if ((kv = kh_get (is32, hval, agent_nkey))) {
    free ((char *) kh_val (hval, kv));
    kh_del (is32, hval, kv);
  }

  return 0;
}

static int
free_used_agents_per_host (void *val, GO_UNUSED void *user_data)
{
  khiter_t k;
  khash_t (igsl) * hash = get_hash (HOSTS, MTRC_AGENTS);
  GSLList *list = NULL, *match = NULL;
  uint32_t agent_nkey = (*(uint32_t *) val);    //24
  int found = 0;

  for (k = kh_begin (hash); k != kh_end (hash); ++k) {
    if (!kh_exist (hash, k) || (!(list = kh_val (hash, k))))
      continue;

    if ((match = list_find (list, find_user_agent_key, &agent_nkey)))
      found++;
    /* since the agent is used in multiple IPs, there's no need for deleting the
     * key-value pairs. */
    if (found > 1)
      return 0;
  }
  free_agent_list (agent_nkey);

  return 0;
}

static void
free_key_by_type (GKHashMetric mtrc, uint32_t key)
{
  khiter_t k;
  void *list = NULL;
  char *value = NULL;

  switch (mtrc.type) {
  case MTRC_TYPE_II32:
    k = kh_get (ii32, mtrc.ii32, key);
    kh_del (ii32, mtrc.ii32, k);
    break;
  case MTRC_TYPE_IS32:
    k = kh_get (is32, mtrc.is32, key);
    if (k != kh_end (mtrc.is32) && (value = kh_val (mtrc.is32, k))) {
      free (value);
      kh_del (is32, mtrc.is32, k);
    }
    break;
  case MTRC_TYPE_IU64:
    k = kh_get (iu64, mtrc.iu64, key);
    kh_del (iu64, mtrc.iu64, k);
    break;
  case MTRC_TYPE_IGSL:
    k = kh_get (igsl, mtrc.igsl, key);
    if (k == kh_end (mtrc.igsl) || !(list = kh_val (mtrc.igsl, k)))
      break;

    list = kh_value (mtrc.igsl, k);
    if (mtrc.metric == MTRC_AGENTS)
      list_foreach (list, free_used_agents_per_host, NULL);
    list_remove_nodes (list);   /* remove list of current host */
    kh_del (igsl, mtrc.igsl, k);        /* remove host key from hash */
    break;
  default:
    break;
  }
}

static void
free_by_num_key (GModule module, uint32_t key)
{
  int i;
  GKHashMetric mtrc;

  for (i = 0; i < GSMTRC_TOTAL; ++i) {
    mtrc = gkh_storage[module].metrics[i];
    free_key_by_type (mtrc, key);
  }
}

static int
free_record_from_partial_key (GModule module, const char *key)
{
  khiter_t k;
  khash_t (si32) * hash = get_hash (module, MTRC_KEYMAP);
  char *p = NULL;
  int len = 0;

  if (!hash)
    return -1;

  len = strlen (key);
  for (k = kh_begin (hash); k != kh_end (hash); ++k) {
    if (!kh_exist (hash, k) || (!(p = strstr (kh_key (hash, k), key))))
      continue;
    p += len;
    if (*p != '|')
      continue;

    free_by_num_key (module, kh_value (hash, k));
    free ((char *) kh_key (hash, k));
    kh_del (si32, hash, k);
  }

  return 0;
}

static int
free_partial_match_uniqmap (GModule module, uint32_t uniq_nkey)
{
  khiter_t k;
  khash_t (si32) * hash = get_hash (module, MTRC_UNIQMAP);
  char *key = NULL, *p = NULL;
  int len = 0;

  if (!hash)
    return -1;

  key = u322str (uniq_nkey, 0);
  len = strlen (key);

  for (k = kh_begin (hash); k != kh_end (hash); ++k) {
    if (!kh_exist (hash, k) || (!(p = strstr (kh_key (hash, k), key))))
      continue;
    p += len;
    if (*p != '|')
      continue;
    free ((char *) kh_key (hash, k));
    kh_del (si32, hash, k);
  }
  free (key);

  return 0;
}

int
clean_partial_match_hashes (int date)
{
  GModule module;
  khash_t (si32) * hash = ht_unique_keys;
  khiter_t k;
  char *p = NULL, *key = NULL;
  int len = 0;
  size_t idx = 0;

  if (!hash)
    return -1;

  key = int2str (date, 0);
  len = strlen (key);
  for (k = kh_begin (hash); k != kh_end (hash); ++k) {
    if (!kh_exist (hash, k) || (!(p = strstr (kh_key (hash, k), key))))
      continue;
    p += len;
    if (*p != '|')
      continue;

    FOREACH_MODULE (idx, module_list) {
      module = module_list[idx];
      free_partial_match_uniqmap (module, kh_value (hash, k));
    }

    free ((char *) kh_key (hash, k));
    kh_del (si32, hash, k);
  }
  free (key);

  return 0;
}

int
clean_full_match_hashes (int date)
{
  khiter_t k;

  k = kh_get (ii32, ht_cnt_valid, date);
  kh_del (ii32, ht_cnt_valid, k);

  k = kh_get (iu64, ht_cnt_bw, date);
  kh_del (iu64, ht_cnt_bw, k);

  return 0;
}

int
invalidate_date (int date)
{
  GModule module;
  khash_t (iui8) * hash = ht_dates;
  khiter_t k;
  size_t idx = 0;
  char *key = NULL;

  if (!hash)
    return -1;

  key = int2str (date, 0);
  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    free_record_from_partial_key (module, key);
  }

  k = kh_get (iui8, hash, date);
  kh_del (iui8, hash, k);

  free (key);

  return 0;
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

static int
sum_hits_from_list (void *val, void *user_data)
{
  GRawData *raw_data = user_data;
  uint32_t hits = 0;

  hits = ht_get_hits (raw_data->module, (*(uint32_t *) val));
  raw_data->items[raw_data->idx].value.u32value += hits;

  return 0;
}

static int
filter_raw_num_data (GModule module)
{
  khiter_t key;
  char *pch = NULL;

  khash_t (si32) * hash = get_hash (module, MTRC_KEYMAP);
  if (!hash)
    return -1;

  for (key = kh_begin (hash); key != kh_end (hash); ++key) {
    if (!kh_exist (hash, key) || !(pch = strchr (kh_key (hash, key), '|')))
      continue;
    pch++;
    ht_insert_keymap_list (module, pch, kh_value (hash, key));
  }

  return 0;
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
  GSLList *list = NULL;
  uint32_t ht_size = 0;

  khash_t (sgsl) * hash = get_hash (module, MTRC_KEYMAPUQ);
  if (!hash)
    return NULL;

  ht_size = kh_size (hash);
  raw_data = init_new_raw_data (module, ht_size);

  for (key = kh_begin (hash); key != kh_end (hash); ++key) {
    if (!kh_exist (hash, key))
      continue;

    list = kh_value (hash, key);
    // add up all hits and set them as the value
    list_foreach (list, sum_hits_from_list, raw_data);
    // GSLList of keys
    raw_data->items[raw_data->idx].key.lkeys = list;
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
parse_raw_data (GModule module)
{
  GRawData *raw_data = NULL;

  if (filter_raw_num_data (module) == 0)
    raw_data = parse_raw_num_data (module);
  return raw_data;
}

/* Initialize hash tables */
void
init_storage (void)
{
  GModule module;
  size_t idx = 0;

  /* Hashes used across the whole app (not per module) */
  /* *INDENT-OFF* */
  ht_agent_keys  = (khash_t (si32) *) new_si32_ht ();
  ht_agent_vals  = (khash_t (is32) *) new_is32_ht ();
  ht_dates       = (khash_t (iui8) *) new_iui8_ht ();
  ht_hostnames   = (khash_t (ss32) *) new_ss32_ht ();
  ht_seqs        = (khash_t (si32) *) new_si32_ht ();
  ht_unique_keys = (khash_t (si32) *) new_si32_ht ();

  ht_cnt_overall = (khash_t (si32) *) new_si32_ht ();
  ht_last_parse  = (khash_t (ii32) *) new_ii32_ht ();
  ht_cnt_valid   = (khash_t (ii32) *) new_ii32_ht ();
  ht_cnt_bw      = (khash_t (iu64) *) new_iu64_ht ();
  /* *INDENT-ON* */

  gkh_storage = new_gkhstorage (TOTAL_MODULES);
  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    gkh_storage[module].module = module;
    init_tables (module);
  }

  restore_data ();
}

/* Destroys the hash structure and its content */
void
free_storage (void)
{
  size_t idx = 0;

  persist_data ();

  des_si32_free (ht_unique_keys);
  des_is32_free (ht_agent_vals);
  des_si32_free (ht_agent_keys);
  des_ss32_free (ht_hostnames);
  des_si32_free (ht_seqs);
  des_iui8 (ht_dates);

  des_si32_free (ht_cnt_overall);
  des_ii32 (ht_last_parse);
  des_ii32 (ht_cnt_valid);
  des_iu64 (ht_cnt_bw);

  if (!gkh_storage)
    return;

  FOREACH_MODULE (idx, module_list) {
    free_metrics (module_list[idx]);
  }
  free (gkh_storage);
}
