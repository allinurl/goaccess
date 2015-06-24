/**
 * tcabdb.c -- Tokyo Cabinet database functions
 * Copyright (C) 2009-2014 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An Ncurses apache weblog analyzer & interactive viewer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * A copy of the GNU General Public License is attached to this
 * source distribution for its full text.
 *
 * Visit http://goaccess.prosoftcorp.com for new releases.
 */

#include <errno.h>
#include <tcutil.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "tcabdb.h"
#include "tcbtdb.h"

#ifdef HAVE_LIBGEOIP
#include "geolocation.h"
#endif

#include "error.h"
#include "sort.h"
#include "util.h"
#include "xmalloc.h"

GStorage *ht_storage;

/* tables for the whole app */
TCADB *ht_agent_keys = NULL;
TCADB *ht_agent_vals = NULL;
TCADB *ht_general_stats = NULL;
TCADB *ht_hostnames = NULL;
TCADB *ht_unique_keys = NULL;

static int
tc_adb_open (TCADB * adb, const char *params)
{
  /* attempt to open the database */
  if (!tcadbopen (adb, params))
    return 1;
  return 0;
}

/* Close the database handle */
static int
tc_db_close (TCADB * adb, char *dbname)
{
  if (adb == NULL)
    return 1;

  /* close the database */
  if (!tcadbclose (adb))
    FATAL ("Unable to close DB: %s", dbname);

  /* delete the object */
  tcadbdel (adb);

#ifdef TCB_BTREE
  /* remove database file */
  if (!conf.keep_db_files && !conf.load_from_disk && !tcremovelink (dbname))
    LOG_DEBUG (("Unable to remove DB: %s\n", dbname));
#endif
  free (dbname);

  return 0;
}

static TCADB *
tc_adb_create (char *path)
{
  char params[DB_PARAMS] = "";
  TCADB *adb = tcadbnew ();

#ifdef TCB_MEMHASH
  xstrncpy (params, path, DB_PARAMS);
#endif
#ifdef TCB_BTREE
  tc_db_get_params (params, path);
#endif

  if (tc_adb_open (adb, params)) {
    free (path);
    LOG_DEBUG (("params: %s", params));
    FATAL ("Unable to open an abstract database: %s", params);
  }

  free (path);

  return adb;
}

static char *
get_dbname (const char *dbname, int module)
{
  char *path = NULL;
#ifdef TCB_MEMHASH
  (void) dbname;
  (void) module;
  path = alloc_string ("*");
#endif

#ifdef TCB_BTREE
  char *db = NULL;
  path = tc_db_set_path (dbname, module);

  if (module >= 0) {
    db = xmalloc (snprintf (NULL, 0, "%s", path) + 1);
    sprintf (db, "%s", path);
    free (path);
    return db;
  }
#endif

  return path;
}

static void
init_tables (GModule module)
{
  ht_storage[module].module = module;
  ht_storage[module].metrics = new_ht_metrics ();

  /* Initialize metrics hash tables */
  ht_storage[module].metrics->keymap =
    tc_adb_create (get_dbname (DB_KEYMAP, module));
  ht_storage[module].metrics->datamap =
    tc_adb_create (get_dbname (DB_DATAMAP, module));
  ht_storage[module].metrics->rootmap =
    tc_adb_create (get_dbname (DB_ROOTMAP, module));
  ht_storage[module].metrics->uniqmap =
    tc_adb_create (get_dbname (DB_UNIQMAP, module));
  ht_storage[module].metrics->hits =
    tc_adb_create (get_dbname (DB_HITS, module));
  ht_storage[module].metrics->visitors =
    tc_adb_create (get_dbname (DB_VISITORS, module));
  ht_storage[module].metrics->bw = tc_adb_create (get_dbname (DB_BW, module));
  ht_storage[module].metrics->time_served =
    tc_adb_create (get_dbname (DB_AVGTS, module));
  ht_storage[module].metrics->methods =
    tc_adb_create (get_dbname (DB_METHODS, module));
  ht_storage[module].metrics->protocols =
    tc_adb_create (get_dbname (DB_PROTOCOLS, module));
#ifdef TCB_MEMHASH
  ht_storage[module].metrics->agents =
    tc_adb_create (get_dbname (DB_AGENTS, module));
#endif
#ifdef TCB_BTREE
  /* allow for duplicate keys */
  ht_storage[module].metrics->agents = tc_bdb_create (DB_AGENTS, module);
#endif
}

/* Initialize Tokyo Cabinet storage tables */
void
init_storage (void)
{
  GModule module;

  ht_agent_keys = tc_adb_create (get_dbname (DB_AGENT_KEYS, -1));
  ht_agent_vals = tc_adb_create (get_dbname (DB_AGENT_VALS, -1));
  ht_general_stats = tc_adb_create (get_dbname (DB_GEN_STATS, -1));
  ht_hostnames = tc_adb_create (get_dbname (DB_HOSTNAMES, -1));
  ht_unique_keys = tc_adb_create (get_dbname (DB_UNIQUE_KEYS, -1));

  ht_storage = new_gstorage (TOTAL_MODULES);
  for (module = 0; module < TOTAL_MODULES; ++module) {
    init_tables (module);
  }
}

static void
free_tables (GStorageMetrics * metrics, GModule module)
{
  /* Initialize metrics hash tables */
  tc_db_close (metrics->keymap, get_dbname (DB_KEYMAP, module));
  tc_db_close (metrics->datamap, get_dbname (DB_DATAMAP, module));
  tc_db_close (metrics->rootmap, get_dbname (DB_ROOTMAP, module));
  tc_db_close (metrics->uniqmap, get_dbname (DB_UNIQMAP, module));
  tc_db_close (metrics->hits, get_dbname (DB_HITS, module));
  tc_db_close (metrics->visitors, get_dbname (DB_VISITORS, module));
  tc_db_close (metrics->bw, get_dbname (DB_BW, module));
  tc_db_close (metrics->time_served, get_dbname (DB_AVGTS, module));
  tc_db_close (metrics->methods, get_dbname (DB_METHODS, module));
  tc_db_close (metrics->protocols, get_dbname (DB_PROTOCOLS, module));
#ifdef TCB_MEMHASH
  tc_db_close (metrics->agents, get_dbname (DB_AGENTS, module));
#endif
#ifdef TCB_BTREE
  tc_bdb_close (metrics->agents, get_dbname (DB_AGENTS, module));
#endif
}

void
free_storage (void)
{
  GModule module;

  tc_db_close (ht_agent_keys, get_dbname (DB_AGENT_KEYS, -1));
  tc_db_close (ht_agent_vals, get_dbname (DB_AGENT_VALS, -1));
  tc_db_close (ht_general_stats, get_dbname (DB_GEN_STATS, -1));
  tc_db_close (ht_hostnames, get_dbname (DB_HOSTNAMES, -1));
  tc_db_close (ht_unique_keys, get_dbname (DB_UNIQUE_KEYS, -1));

  for (module = 0; module < TOTAL_MODULES; ++module) {
    free_tables (ht_storage[module].metrics, module);
  }
}

uint32_t
get_ht_size (TCADB * adb)
{
  return tcadbrnum (adb);
}

uint32_t
get_ht_size_by_metric (GModule module, GMetric metric)
{
  TCADB *adb = get_storage_metric (module, metric);

  return get_ht_size (adb);
}

int
ht_insert_keymap (TCADB * adb, const char *value)
{
  void *value_ptr;
  int nkey = 0, size = 0, ret = 0;

  if ((adb == NULL) || (value == NULL))
    return (EINVAL);

  if ((value_ptr = tcadbget2 (adb, value)) != NULL) {
    ret = (*(int *) value_ptr);
    free (value_ptr);
    return ret;
  }

  size = get_ht_size (adb);
  /* the auto increment value starts at SIZE (hash table) + 1 */
  nkey = size > 0 ? size + 1 : 1;

  tcadbput (adb, value, strlen (value), &nkey, sizeof (int));

  return nkey;
}

int
ht_insert_uniqmap (TCADB * adb, char *uniq_key)
{
  void *value_ptr;
  int nkey = 0, size = 0;

  if ((adb == NULL) || (uniq_key == NULL))
    return (EINVAL);

  if ((value_ptr = tcadbget2 (adb, uniq_key)) != NULL) {
    free (value_ptr);
    return 0;
  }

  size = get_ht_size (adb);
  /* the auto increment value starts at SIZE (hash table) + 1 */
  nkey = size > 0 ? size + 1 : 1;

  tcadbput (adb, uniq_key, strlen (uniq_key), &nkey, sizeof (int));
  free (uniq_key);

  return nkey;
}

int
ht_insert_nkey_nval (TCADB * adb, int nkey, int nval)
{
  int sp = 0;

  if (adb == NULL)
    return (EINVAL);

  if (tcadbget (adb, &nkey, sizeof (int), &sp) != NULL)
    return 1;

  tcadbput (adb, &nkey, sizeof (int), &nval, sizeof (int));

  return 0;
}

int
ht_insert_hit (TCADB * adb, int data_nkey, int uniq_nkey, int root_nkey)
{
  int sp = 0;
  GDataMap *map;

  if (adb == NULL)
    return (EINVAL);

  if ((map = tcadbget (adb, &data_nkey, sizeof (int), &sp)) != NULL) {
    map->data++;
  } else {
    map = xcalloc (1, sizeof (GDataMap));
    map->data = 1;
    map->root = root_nkey;
    map->uniq = uniq_nkey;
  }
  tcadbput (adb, &data_nkey, sizeof (int), map, sizeof (GDataMap));
  if (map)
    free (map);

  return 0;
}

int
find_host_agent_in_list (void *data, void *needle)
{
  return (*(int *) data) == (*(int *) needle) ? 1 : 0;
}

#ifdef TCB_MEMHASH
int
ht_insert_host_agent (TCADB * adb, int data_nkey, int agent_nkey)
{
  GSLList *list, *match;
  int sp = 0;

  if (adb == NULL)
    return (EINVAL);

  if ((list = tcadbget (adb, &data_nkey, sizeof (int), &sp)) != NULL) {
    if ((match = list_find (list, find_host_agent_in_list, &agent_nkey)))
      goto out;
    list = list_insert_prepend (list, int2ptr (agent_nkey));
  } else {
    list = list_create (int2ptr (agent_nkey));
  }
  tcadbput (adb, &data_nkey, sizeof (int), list, sizeof (GSLList));
out:
  free (list);

  return 0;
}
#endif

int
ht_insert_str_from_int_key (TCADB * adb, int nkey, const char *value)
{
  if (adb == NULL)
    return (EINVAL);

  tcadbput (adb, &nkey, sizeof (int), value, strlen (value));

  return 0;
}

static int
ht_inc_int_from_key (TCADB * adb, const void *key, size_t ksize, int inc)
{
  int sp = 0;
  void *value_ptr;
  int add_value;

  if (adb == NULL)
    return (EINVAL);

  if ((value_ptr = tcadbget (adb, key, ksize, &sp)) != NULL) {
    add_value = (*(int *) value_ptr) + inc;
    free (value_ptr);
  } else {
    add_value = 0 + inc;
  }

  tcadbput (adb, key, ksize, &add_value, sizeof (int));

  return 0;
}

static int
ht_inc_u64_from_key (TCADB * adb, const void *key, size_t ksize, uint64_t inc)
{
  int sp = 0;
  void *value_ptr;
  uint64_t add_value;

  if (adb == NULL)
    return (EINVAL);

  if ((value_ptr = tcadbget (adb, key, ksize, &sp)) != NULL) {
    add_value = (*(uint64_t *) value_ptr) + inc;
    free (value_ptr);
  } else {
    add_value = 0 + inc;
  }

  tcadbput (adb, key, ksize, &add_value, sizeof (uint64_t));

  return 0;
}

int
ht_inc_int_from_int_key (TCADB * adb, int data_nkey, int inc)
{
  if (adb == NULL)
    return (EINVAL);

  return ht_inc_int_from_key (adb, &data_nkey, sizeof (data_nkey), inc);
}

int
ht_inc_u64_from_int_key (TCADB * adb, int data_nkey, uint64_t inc)
{
  if (adb == NULL)
    return (EINVAL);

  return ht_inc_u64_from_key (adb, &data_nkey, sizeof (data_nkey), inc);
}

int
ht_inc_int_from_str_key (TCADB * adb, const char *key, int inc)
{
  if (adb == NULL)
    return (EINVAL);

  return ht_inc_int_from_key (adb, key, strlen (key), inc);
}

int
ht_inc_u64_from_str_key (TCADB * adb, const char *key, uint64_t inc)
{
  if (adb == NULL)
    return (EINVAL);

  return ht_inc_u64_from_key (adb, key, strlen (key), inc);
}

int
ht_insert_unique_key (const char *key)
{
  return ht_insert_keymap (ht_unique_keys, key);
}

int
ht_insert_agent_key (const char *key)
{
  return ht_insert_keymap (ht_agent_keys, key);
}

int
ht_insert_agent_val (int nkey, const char *key)
{
  return ht_insert_str_from_int_key (ht_agent_vals, nkey, key);
}

static int
get_int_from_int_key (TCADB * adb, int nkey)
{
  void *value_ptr;
  int sp = 0, ret = 0;

  if (adb == NULL)
    return (EINVAL);

  value_ptr = tcadbget (adb, &nkey, sizeof (int), &sp);
  if (value_ptr != NULL) {
    ret = (*(int *) value_ptr);
    free (value_ptr);
  }

  return ret;
}

int
get_int_from_str_key (TCADB * adb, const char *key)
{
  void *value_ptr;
  int sp = 0, ret = 0;

  if (adb == NULL)
    return (EINVAL);

  value_ptr = tcadbget (adb, key, strlen (key), &sp);
  if (value_ptr != NULL) {
    ret = (*(int *) value_ptr);
    free (value_ptr);
  }

  return ret;
}

unsigned int
get_uint_from_str_key (TCADB * adb, const char *key)
{
  void *value_ptr;
  int sp = 0, ret = 0;

  if (adb == NULL)
    return (EINVAL);

  value_ptr = tcadbget (adb, key, strlen (key), &sp);
  if (value_ptr != NULL) {
    ret = (*(unsigned int *) value_ptr);
    free (value_ptr);
  }

  return ret;
}

char *
get_str_from_int_key (TCADB * adb, int nkey)
{
  void *value_ptr;
  int sp = 0;

  if (adb == NULL)
    return NULL;

  value_ptr = tcadbget (adb, &nkey, sizeof (int), &sp);
  if (value_ptr != NULL)
    return (char *) value_ptr;

  return NULL;
}

char *
get_root_from_key (int root_nkey, GModule module)
{
  TCADB *adb = NULL;
  void *value_ptr;
  int sp = 0;

  adb = get_storage_metric (module, MTRC_ROOTMAP);
  if (adb == NULL)
    return NULL;

  value_ptr = tcadbget (adb, &root_nkey, sizeof (int), &sp);
  if (value_ptr != NULL)
    return (char *) value_ptr;

  return NULL;
}

char *
get_node_from_key (int data_nkey, GModule module, GMetric metric)
{
  TCADB *adb = NULL;
  GStorageMetrics *metrics;

  metrics = get_storage_metrics_by_module (module);
  /* bandwidth modules */
  switch (metric) {
  case MTRC_DATAMAP:
    adb = metrics->datamap;
    break;
  case MTRC_METHODS:
    adb = metrics->methods;
    break;
  case MTRC_PROTOCOLS:
    adb = metrics->protocols;
    break;
  default:
    adb = NULL;
  }

  if (adb == NULL)
    return NULL;

  return get_str_from_int_key (adb, data_nkey);
}

uint64_t
get_cumulative_from_key (int data_nkey, GModule module, GMetric metric)
{
  TCADB *adb = NULL;
  GStorageMetrics *metrics;
  void *value_ptr;
  uint64_t ret = 0;
  int sp = 0;

  metrics = get_storage_metrics_by_module (module);
  /* bandwidth modules */
  switch (metric) {
  case MTRC_BW:
    adb = metrics->bw;
    break;
  case MTRC_TIME_SERVED:
    adb = metrics->time_served;
    break;
  default:
    adb = NULL;
  }

  if (adb == NULL)
    return 0;

  value_ptr = tcadbget (adb, &data_nkey, sizeof (int), &sp);
  if (value_ptr != NULL) {
    ret = (*(uint64_t *) value_ptr);
    free (value_ptr);
  }

  return ret;
}

int
get_num_from_key (int data_nkey, GModule module, GMetric metric)
{
  TCADB *adb = NULL;
  GStorageMetrics *metrics;

  metrics = get_storage_metrics_by_module (module);
  /* bandwidth modules */
  switch (metric) {
  case MTRC_HITS:
    adb = metrics->hits;
    break;
  case MTRC_VISITORS:
    adb = metrics->visitors;
    break;
  default:
    adb = NULL;
  }

  if (adb == NULL)
    return 0;

  return get_int_from_int_key (adb, data_nkey);
}

char *
get_hostname (const char *host)
{
  void *value_ptr;

  value_ptr = tcadbget2 (ht_hostnames, host);
  if (value_ptr)
    return value_ptr;
  return NULL;
}

int
get_int_from_keymap (const char *key, GModule module)
{
  TCADB *adb = get_storage_metric (module, MTRC_KEYMAP);

  return get_int_from_str_key (adb, key);
}

char *
get_host_agent_val (int agent_nkey)
{
  return get_str_from_int_key (ht_agent_vals, agent_nkey);
}

GSLList *
tclist_to_gsllist (TCLIST * tclist)
{
  GSLList *list = NULL;
  int i, sz;
  int *val;

  for (i = 0; i < tclistnum (tclist); ++i) {
    val = (int *) tclistval (tclist, i, &sz);
    if (list == NULL)
      list = list_create (int2ptr (*val));
    else
      list = list_insert_prepend (list, int2ptr (*val));
  }

  return list;
}

void *
get_host_agent_list (int data_nkey)
{
#ifdef TCB_MEMHASH
  TCADB *db;
  void *list;
  int sp = 0;
#endif
#ifdef TCB_BTREE
  TCBDB *db;
  TCLIST *tclist;
#endif

  db = get_storage_metric (HOSTS, MTRC_AGENTS);
#ifdef TCB_MEMHASH
  if ((list = tcadbget (db, &data_nkey, sizeof (data_nkey), &sp)))
    return list;
#endif
#ifdef TCB_BTREE
  if ((tclist = tcbdbget4 (db, &data_nkey, sizeof (int))) != NULL) {
    return tclist;
  }
#endif

  return NULL;
}

/* Calls the given function for each of the key/value pairs */
static void
tc_db_foreach (void *db, void (*fp) (TCADB * m, void *k, int s, void *u),
               void *user_data)
{
  TCADB *adb = db;
  int ksize = 0;
  void *key;

  tcadbiterinit (adb);
  while ((key = tcadbiternext (adb, &ksize)) != NULL)
    (*fp) (adb, key, ksize, user_data);
}

static void
free_key (TCADB * adb, void *key, int ksize, GO_UNUSED void *user_data)
{
  void *value;
  int sp = 0;

  value = tcadbget (adb, key, ksize, &sp);
  if (value)
    free (value);
  free (key);
}

static void
free_agent_values (TCADB * adb, void *key, int ksize, GO_UNUSED void *user_data)
{
  void *list;
  int sp = 0;

  list = tcadbget (adb, key, ksize, &sp);
  if (list)
    list_remove_nodes (list);
  free (key);
}

void
free_db_key (TCADB * adb)
{
  tc_db_foreach (adb, free_key, NULL);
}

void
free_agent_list (void)
{
  TCADB *adb = get_storage_metric (HOSTS, MTRC_AGENTS);

  tc_db_foreach (adb, free_agent_values, NULL);
}

static void
set_raw_data (void *key, void *value, GRawData * raw_data)
{
  raw_data->items[raw_data->idx].key = key;
  raw_data->items[raw_data->idx].value = value;
  raw_data->idx++;
}

static void
data_iter_generic (TCADB * adb, void *key, int ksize, void *user_data)
{
  GRawData *raw_data = user_data;
  void *value;
  int sp = 0;

  value = tcadbget (adb, key, ksize, &sp);
  if (value)
    set_raw_data (key, value, raw_data);
}

GRawData *
parse_raw_data (void *db, int ht_size, GModule module)
{
  GRawData *raw_data;

  raw_data = new_grawdata ();
  raw_data->size = ht_size;
  raw_data->module = module;
  raw_data->idx = 0;
  raw_data->items = new_grawdata_item (ht_size);

  tc_db_foreach (db, data_iter_generic, raw_data);
  sort_raw_data (raw_data, ht_size);

  return raw_data;
}
