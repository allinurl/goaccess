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

/* Hash tables storage */
static GTCStorage *tc_storage;

/* tables for the whole app */
static TCADB *ht_agent_keys = NULL;
static TCADB *ht_agent_vals = NULL;
static TCADB *ht_general_stats = NULL;
static TCADB *ht_hostnames = NULL;
static TCADB *ht_unique_keys = NULL;

/* Instantiate a new store */
static GTCStorage *
new_tcstorage (uint32_t size)
{
  GTCStorage *storage = xcalloc (size, sizeof (GTCStorage));
  return storage;
}

/* Open a concrete database */
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
  if (!conf.keep_db_files && !tcremovelink (dbname))
    LOG_DEBUG (("Unable to remove DB: %s\n", dbname));
#endif
  free (dbname);

  return 0;
}

/* Setup a database given the file path and open it up */
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

/* Get the file path for the given a database name */
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

/* Initialize map & metric hashes */
static void
init_tables (GModule module)
{
  GTCStorageMetric mtrc;
  int n = 0, i;

  /* *INDENT-OFF* */
  GTCStorageMetric metrics[] = {
    {MTRC_KEYMAP    , DB_KEYMAP    , NULL} ,
    {MTRC_ROOTMAP   , DB_ROOTMAP   , NULL} ,
    {MTRC_DATAMAP   , DB_DATAMAP   , NULL} ,
    {MTRC_UNIQMAP   , DB_UNIQMAP   , NULL} ,
    {MTRC_ROOT      , DB_ROOT      , NULL} ,
    {MTRC_HITS      , DB_HITS      , NULL} ,
    {MTRC_VISITORS  , DB_VISITORS  , NULL} ,
    {MTRC_BW        , DB_BW        , NULL} ,
    {MTRC_CUMTS     , DB_CUMTS     , NULL} ,
    {MTRC_MAXTS     , DB_MAXTS     , NULL} ,
    {MTRC_METHODS   , DB_METHODS   , NULL} ,
    {MTRC_PROTOCOLS , DB_PROTOCOLS , NULL} ,
    {MTRC_AGENTS    , DB_AGENTS    , NULL} ,
  };
  /* *INDENT-ON* */

  n = ARRAY_SIZE (metrics);
  for (i = 0; i < n; i++) {
    mtrc = metrics[i];
#ifdef TCB_MEMHASH
    mtrc.store = tc_adb_create (get_dbname (mtrc.dbname, module));
#endif
#ifdef TCB_BTREE
    /* allow for duplicate keys */
    if (mtrc.metric == MTRC_AGENTS)
      mtrc.store = tc_bdb_create (DB_AGENTS, module);
    else
      mtrc.store = tc_adb_create (get_dbname (mtrc.dbname, module));
#endif
    tc_storage[module].metrics[i] = mtrc;
  }
}

/* Initialize hash tables */
void
init_storage (void)
{
  GModule module;
  size_t idx = 0;

  /* Hashes used across the whole app (not per module) */
  ht_agent_keys = tc_adb_create (get_dbname (DB_AGENT_KEYS, -1));
  ht_agent_vals = tc_adb_create (get_dbname (DB_AGENT_VALS, -1));
  ht_general_stats = tc_adb_create (get_dbname (DB_GEN_STATS, -1));
  ht_hostnames = tc_adb_create (get_dbname (DB_HOSTNAMES, -1));
  ht_unique_keys = tc_adb_create (get_dbname (DB_UNIQUE_KEYS, -1));

  tc_storage = new_tcstorage (TOTAL_MODULES);

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    tc_storage[module].module = module;
    init_tables (module);
  }
}

/* Destroys the hash structure allocated metrics */
static void
free_metrics (GModule module)
{
  int i;
  GTCStorageMetric mtrc;

  for (i = 0; i < GSMTRC_TOTAL; i++) {
    mtrc = tc_storage[module].metrics[i];
#ifdef TCB_MEMHASH
    tc_db_close (mtrc.store, get_dbname (mtrc.dbname, module));
#endif
#ifdef TCB_BTREE
    if (mtrc.metric == MTRC_AGENTS)
      tc_bdb_close (mtrc.store, get_dbname (mtrc.dbname, module));
    else
      tc_db_close (mtrc.store, get_dbname (mtrc.dbname, module));
#endif
  }
}

/* Destroys the hash structure and its content */
void
free_storage (void)
{
  size_t idx = 0;

  tc_db_close (ht_agent_keys, get_dbname (DB_AGENT_KEYS, -1));
  tc_db_close (ht_agent_vals, get_dbname (DB_AGENT_VALS, -1));
  tc_db_close (ht_general_stats, get_dbname (DB_GEN_STATS, -1));
  tc_db_close (ht_hostnames, get_dbname (DB_HOSTNAMES, -1));
  tc_db_close (ht_unique_keys, get_dbname (DB_UNIQUE_KEYS, -1));

  FOREACH_MODULE (idx, module_list) {
    free_metrics (module_list[idx]);
  }
}

static uint32_t
ht_get_size (TCADB * adb)
{
  return tcadbrnum (adb);
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
  GTCStorageMetric mtrc;

  for (i = 0; i < GSMTRC_TOTAL; i++) {
    mtrc = tc_storage[module].metrics[i];
    if (mtrc.metric != metric)
      continue;
    hash = mtrc.store;
    break;
  }

  return hash;
}

/* Insert a string key and the corresponding int value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
static int
ins_si32 (void *hash, const char *key, int value)
{
  if (!hash)
    return -1;

  /* if key exists in the database, it is overwritten */
  if (!tcadbput (hash, key, strlen (key), &value, sizeof (int)))
    LOG_DEBUG (("Unable to tcadbput\n"));

  return 0;
}

/* Insert an int key and the corresponding string value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
static int
ins_is32 (void *hash, int key, const char *value)
{
  if (!hash)
    return -1;

  /* if key exists in the database, it is overwritten */
  if (!tcadbput (hash, &key, sizeof (int), value, strlen (value)))
    LOG_DEBUG (("Unable to tcadbput\n"));

  return 0;
}

/* Insert a string key and the corresponding string value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, or if key exists, -1 is returned.
 * On success 0 is returned */
static int
ins_ss32 (void *hash, const char *key, const char *value)
{
  if (!hash)
    return -1;

  /* if key exists in the database, it is overwritten */
  if (!tcadbput2 (hash, key, value))
    LOG_DEBUG (("Unable to tcadbput\n"));

  return 0;
}

/* Insert an int key and an int value
 * Note: If the key exists, its value is replaced by the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
ins_ii32 (void *hash, int key, int value)
{
  if (!hash)
    return -1;

  /* if key exists in the database, it is overwritten */
  if (!tcadbput (hash, &key, sizeof (int), &value, sizeof (int)))
    LOG_DEBUG (("Unable to tcadbput\n"));

  return 0;
}

/* Insert an int key and a uint64_t value
 * Note: If the key exists, its value is replaced by the given value.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
ins_iu64 (void *hash, int key, uint64_t value)
{
  if (!hash)
    return -1;

  /* if key exists in the database, it is overwritten */
  if (!tcadbput (hash, &key, sizeof (int), &value, sizeof (uint64_t)))
    LOG_DEBUG (("Unable to tcadbput\n"));

  return 0;
}

/* Increase an int value given an int key.
 * Note: If the key exists, its value is increased by the given inc.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
inc_ii32 (void *hash, int key, int inc)
{
  if (!hash)
    return -1;

  /* if key exists in the database, it is incremented */
  if (tcadbaddint (hash, &key, sizeof (int), inc) == INT_MIN)
    LOG_DEBUG (("Unable to tcadbput\n"));

  return 0;
}

/* Increase a uint64_t value given an int key.
 * Note: If the key exists, its value is increased by the given inc.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
static int
inc_iu64 (void *hash, int key, uint64_t inc)
{
  int sp;
  uint64_t value = inc;
  void *ptr;

  if (!hash)
    return -1;

  if ((ptr = tcadbget (hash, &key, sizeof (int), &sp)) != NULL) {
    value = (*(uint64_t *) ptr) + inc;
    free (ptr);
  }

  /* if key exists in the database, it is overwritten */
  if (!tcadbput (hash, &key, sizeof (int), &value, sizeof (uint64_t)))
    LOG_DEBUG (("Unable to tcadbput\n"));

  return 0;
}

/* Insert a string key and auto increment its value.
 *
 * On error, -1 is returned.
 * On success the value of the key inserted is returned */
static int
inc_si32 (void *hash, const char *key, int inc)
{
  int value = inc;
  void *ptr;

  if (!hash)
    return -1;

  if ((ptr = tcadbget2 (hash, key)) != NULL) {
    value = (*(int *) ptr) + inc;
    free (ptr);
  }

  /* if key exists in the database, it is overwritten */
  if (!tcadbput (hash, key, strlen (key), &value, sizeof (int)))
    LOG_DEBUG (("Unable to tcadbput\n"));

  return 0;
}

/* Insert a string key and auto increment by uint64_t value.
 *
 * On error, -1 is returned.
 * On success the value of the key inserted is returned */
static int
inc_su64 (void *hash, const char *key, uint64_t inc)
{
  uint64_t value = inc;
  void *ptr;

  if (!hash)
    return -1;

  if ((ptr = tcadbget2 (hash, key)) != NULL) {
    value = (*(uint64_t *) ptr) + inc;
    free (ptr);
  }

  /* if key exists in the database, it is overwritten */
  if (!tcadbput (hash, key, strlen (key), &value, sizeof (uint64_t)))
    LOG_DEBUG (("Unable to tcadbput\n"));

  return 0;
}

/* Insert a string key and auto increment int value.
 *
 * On error, -1 is returned.
 * On success the value of the key inserted is returned */
static int
ins_si32_ai (void *hash, const char *key)
{
  int size = 0, value = 0;

  if (!hash)
    return -1;

  size = ht_get_size (hash);
  /* the auto increment value starts at SIZE (hash table) + 1 */
  value = size > 0 ? size + 1 : 1;

  if (ins_si32 (hash, key, value) == -1)
    return -1;

  return value;
}

#ifdef TCB_MEMHASH

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
ins_igsl (void *hash, int key, int value)
{
  GSLList *list, *match;
  int sp;

  if (!hash)
    return -1;

  /* key found, check if key exists within the list */
  if ((list = tcadbget (hash, &key, sizeof (int), &sp)) != NULL) {
    if ((match = list_find (list, find_int_key_in_list, &value)))
      goto out;
    list = list_insert_prepend (list, int2ptr (value));
  } else {
    list = list_create (int2ptr (value));
  }

  if (!tcadbput (hash, &key, sizeof (int), list, sizeof (GSLList)))
    LOG_DEBUG (("Unable to tcadbput\n"));
out:
  free (list);

  return 0;
}

#endif

/* Get the int value of a given string key.
 *
 * On error, -1 is returned.
 * On success the int value for the given key is returned */
static int
get_si32 (void *hash, const char *key)
{
  int ret = 0;
  void *ptr;

  if (!hash)
    return -1;

  /* key found, return current value */
  if ((ptr = tcadbget2 (hash, key)) != NULL) {
    ret = (*(int *) ptr);
    free (ptr);
    return ret;
  }

  return -1;
}

/* Get the unsigned int value of a given string key.
 *
 * On error, -1 is returned.
 * On success the int value for the given key is returned */
static uint32_t
get_sui32 (void *hash, const char *key)
{
  int ret = 0;
  void *ptr;
  if (!hash)
    return 0;

  /* key found, return current value */
  if ((ptr = tcadbget2 (hash, key)) != NULL) {
    ret = (*(uint32_t *) ptr);
    free (ptr);
    return ret;
  }

  return 0;
}

/* Get the uint64_t value of a given string key.
 *
 * On error, 0 is returned.
 * On success the int value for the given key is returned */
static uint64_t
get_su64 (void *hash, const char *key)
{
  uint64_t ret = 0;
  void *ptr;
  if (!hash)
    return 0;

  /* key found, return current value */
  if ((ptr = tcadbget2 (hash, key)) != NULL) {
    ret = (*(uint64_t *) ptr);
    free (ptr);
    return ret;
  }

  return 0;
}

/* Get the string value of a given int key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
static char *
get_is32 (void *hash, int key)
{
  int sp;
  char *value = NULL;

  if (!hash)
    return NULL;

  if ((value = tcadbget (hash, &key, sizeof (int), &sp)) != NULL)
    return value;

  return NULL;
}

/* Get the string value of a given string key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
static char *
get_ss32 (void *hash, const char *key)
{
  char *value = NULL;

  if (!hash)
    return NULL;

  if ((value = tcadbget2 (hash, key)) != NULL)
    return value;

  return NULL;
}

/* Get the int value of a given int key.
 *
 * If key is not found, 0 is returned.
 * On error, -1 is returned.
 * On success the int value for the given key is returned */
static int
get_ii32 (void *hash, int key)
{
  int sp, ret = 0;
  void *ptr = 0;

  if (!hash)
    return -1;

  /* key found, return current value */
  if ((ptr = tcadbget (hash, &key, sizeof (int), &sp)) != NULL) {
    ret = (*(int *) ptr);
    free (ptr);
    return ret;
  }

  return 0;
}

/* Get the uint64_t value of a given int key.
 *
 * On error, or if key is not found, 0 is returned.
 * On success the uint64_t value for the given key is returned */
static uint64_t
get_iu64 (void *hash, int key)
{
  int sp;
  uint64_t ret = 0;
  void *ptr = 0;

  if (!hash)
    return 0;

  /* key found, return current value */
  if ((ptr = tcadbget (hash, &key, sizeof (int), &sp)) != NULL) {
    ret = (*(uint64_t *) ptr);
    free (ptr);
    return ret;
  }

  return 0;
}

/* Get the GSLList value of a given int key.
 *
 * On error, or if key is not found, NULL is returned.
 * On success the GSLList value for the given key is returned */
static GSLList *
get_igsl (void *hash, int key)
{
  int sp;
  GSLList *list = NULL;

  if (!hash)
    return NULL;

  /* key found, return current value */
  if ((list = tcadbget (hash, &key, sizeof (int), &sp)) != NULL)
    return list;

  return NULL;
}

/* Get the TCLIST value of a given int key.
 *
 * On error, or if key is not found, NULL is returned.
 * On success the GSLList value for the given key is returned */
static TCLIST *
get_itcli (void *hash, int key)
{
  TCLIST *list = NULL;

  if (!hash)
    return NULL;

  /* key found, return current value */
  if ((list = tcbdbget4 (hash, &key, sizeof (int))) != NULL)
    return list;

  return NULL;
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
  void *hash = ht_unique_keys;

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
  void *hash = ht_agent_keys;

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
  void *hash = ht_agent_vals;

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
  void *hash = get_hash (module, MTRC_KEYMAP);

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
  void *hash = get_hash (module, MTRC_DATAMAP);

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
  void *hash = get_hash (module, MTRC_ROOTMAP);

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
  void *hash = get_hash (module, MTRC_UNIQMAP);

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
  void *hash = get_hash (module, MTRC_ROOT);

  if (!hash)
    return -1;

  return ins_ii32 (hash, key, value);
}

/* Increases hits counter from an int key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_hits (GModule module, int key, int inc)
{
  void *hash = get_hash (module, MTRC_HITS);

  if (!hash)
    return -1;

  return inc_ii32 (hash, key, inc);
}

/* Increases visitors counter from an int key.
 *
 * On error, -1 is returned.
 * On success 0 is returned */
int
ht_insert_visitor (GModule module, int key, int inc)
{
  void *hash = get_hash (module, MTRC_VISITORS);

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
  void *hash = get_hash (module, MTRC_BW);

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
  void *hash = get_hash (module, MTRC_CUMTS);

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
  void *hash = get_hash (module, MTRC_MAXTS);

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
  void *hash = get_hash (module, MTRC_METHODS);

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
  void *hash = get_hash (module, MTRC_PROTOCOLS);

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
  void *hash = get_hash (module, MTRC_AGENTS);

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
  void *hash = ht_hostnames;

  if (!hash)
    return -1;

  return ins_ss32 (hash, ip, host);
}

/* Increases a general stats counter int from a string key.
 *
 * On error, -1 is returned.
 * On success the value of the key inserted is returned */
int
ht_insert_genstats (const char *key, int inc)
{
  void *hash = ht_general_stats;

  if (!hash)
    return -1;

  return inc_si32 (hash, key, inc);
}

/* Increases a general stats counter uint64_t from a string key.
 *
 * On error, -1 is returned.
 * On success the value of the key inserted is returned */
int
ht_insert_genstats_bw (const char *key, uint64_t inc)
{
  void *hash = ht_general_stats;

  if (!hash)
    return -1;

  return inc_su64 (hash, key, inc);
}

/* Get the number of elements in a datamap.
 *
 * Return -1 if the operation fails, else number of elements. */
uint32_t
ht_get_size_datamap (GModule module)
{
  void *hash = get_hash (module, MTRC_DATAMAP);

  if (!hash)
    return 0;

  return ht_get_size (hash);
}

/* Get the number of elements in a uniqmap.
 *
 * On error, 0 is returned.
 * On success the number of elements in MTRC_UNIQMAP is returned */
uint32_t
ht_get_size_uniqmap (GModule module)
{
  void *hash = get_hash (module, MTRC_UNIQMAP);

  if (!hash)
    return 0;

  return ht_get_size (hash);
}

/* Get the string data value of a given int key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_datamap (GModule module, int key)
{
  void *hash = get_hash (module, MTRC_DATAMAP);

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
  void *hash = get_hash (module, MTRC_KEYMAP);

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
  void *hash = get_hash (module, MTRC_UNIQMAP);

  if (!hash)
    return -1;

  return get_si32 (hash, key);
}

/* Get the uint32_t value from ht_general_stats given a string key.
 *
 * On error, 0 is returned.
 * On success the uint32_t value for the given key is returned */
uint32_t
ht_get_genstats (const char *key)
{
  void *hash = ht_general_stats;

  if (!hash)
    return 0;

  return get_sui32 (hash, key);
}

/* Get the uint64_t value from ht_general_stats given a string key.
 *
 * On error, 0 is returned.
 * On success the uint64_t value for the given key is returned */
uint64_t
ht_get_genstats_bw (const char *key)
{
  void *hash = ht_general_stats;

  if (!hash)
    return 0;

  return get_su64 (hash, key);
}

/* Get the string root from MTRC_ROOTMAP given an int data key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_root (GModule module, int key)
{
  int root_key = 0;
  void *hashroot = get_hash (module, MTRC_ROOT);
  void *hashrootmap = get_hash (module, MTRC_ROOTMAP);

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
  void *hash = get_hash (module, MTRC_VISITORS);

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
  void *hash = get_hash (module, MTRC_BW);

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
  void *hash = get_hash (module, MTRC_CUMTS);

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
  void *hash = get_hash (module, MTRC_MAXTS);

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
  void *hash = get_hash (module, MTRC_METHODS);

  if (!hash)
    return NULL;

  return get_is32 (hash, key);
}

char *
ht_get_protocol (GModule module, int key)
{
  void *hash = get_hash (module, MTRC_PROTOCOLS);

  if (!hash)
    return NULL;

  return get_is32 (hash, key);
}

/* Get the string value from MTRC_PROTOCOLS given an int key.
 *
 * On error, NULL is returned.
 * On success the string value for the given key is returned */
char *
ht_get_hostname (const char *host)
{
  void *hash = ht_hostnames;

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
  void *hash = ht_agent_vals;

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
  void *hash = get_hash (module, MTRC_AGENTS);
  GSLList *list;

  if ((list = get_igsl (hash, key)))
    return list;
  return NULL;
}

/* Get the list value from MTRC_AGENTS given an int key.
 *
 * On error, or if key is not found, NULL is returned.
 * On success the TCLIST value for the given key is returned */
TCLIST *
ht_get_host_agent_tclist (GModule module, int key)
{
  void *hash = get_hash (module, MTRC_AGENTS);
  TCLIST *list;

  if ((list = get_itcli (hash, key)))
    return list;
  return NULL;
}

/* Insert the values from a TCLIST into a GSLList.
 *
 * On success the GSLList is returned */
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

/* Calls the given function for each of the key/value pairs */
static void
tc_db_foreach (void *db, void (*fp) (TCADB * m, void *k, int s, void *u),
               void *user_data)
{
  TCADB *adb = db;
  int ksize = 0;
  void *key;

  tcadbiterinit (adb);
  while ((key = tcadbiternext (adb, &ksize)) != NULL) {
    (*fp) (adb, key, ksize, user_data);
    free (key);
  }
}

/* Free the key/value pair */
static void
free_agent_values (TCADB * adb, void *key, int ksize, GO_UNUSED void *user_data)
{
  void *list;
  int sp = 0;

  list = tcadbget (adb, key, ksize, &sp);
  if (list)
    list_remove_nodes (list);
}

/* Iterate over the each key/value pair under MTRC_AGENTS and free the and the
 * list of values */
void
free_agent_list (void)
{
  void *hash = get_hash (HOSTS, MTRC_AGENTS);
  if (!hash)
    return;

  tc_db_foreach (hash, free_agent_values, NULL);
}

/* For each key/value pair stored in MTRC_HITS, assign the key/value to a
 * GRawDataItem */
static void
set_raw_data (void *key, void *value, GRawData * raw_data)
{
  raw_data->items[raw_data->idx].key = (*(int *) key);
  raw_data->items[raw_data->idx].value = (*(int *) value);
  raw_data->idx++;
}

/* Get the value stored in MTRC_HITS */
static void
data_iter_generic (TCADB * adb, void *key, int ksize, void *user_data)
{
  GRawData *raw_data = user_data;
  void *value;
  int sp = 0;

  value = tcadbget (adb, key, ksize, &sp);
  if (value) {
    set_raw_data (key, value, raw_data);
    free (value);
  }
}

/* Store the key/value pairs from a hash table into raw_data and sorts the the
 * hits structure.
 *
 * On error, NULL is returned.
 * On success the GRawData sorted is returned */
GRawData *
parse_raw_data (GModule module)
{
  GRawData *raw_data;
  uint32_t ht_size = 0;

  void *hash = get_hash (module, MTRC_HITS);

  if (!hash)
    return NULL;

  raw_data = new_grawdata ();
  raw_data->idx = 0;
  raw_data->module = module;
  raw_data->size = ht_size = ht_get_size (hash);
  raw_data->items = new_grawdata_item (ht_size);

  tc_db_foreach (hash, data_iter_generic, raw_data);

  sort_raw_data (raw_data, raw_data->idx);

  return raw_data;
}
