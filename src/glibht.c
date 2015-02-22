/**
 * glibc.c -- GLib functions
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LIBGLIB_2_0
#include <glib.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glibht.h"

#ifdef HAVE_LIBGEOIP
#include "geolocation.h"
#endif

#include "browsers.h"
#include "error.h"
#include "opesys.h"
#include "gstorage.h"
#include "parser.h"
#include "settings.h"
#include "sort.h"
#include "util.h"
#include "xmalloc.h"

GStorage *ht_storage;

/* tables for the whole app */
GHashTable *ht_hostnames = NULL;
GHashTable *ht_hosts_agents = NULL;
GHashTable *ht_unique_keys = NULL;

static GHashTable *
new_str_ht (GDestroyNotify d1, GDestroyNotify d2)
{
  return g_hash_table_new_full (g_str_hash, g_str_equal, d1, d2);
}

static GHashTable *
new_int_ht (GDestroyNotify d1, GDestroyNotify d2)
{
  return g_hash_table_new_full (g_int_hash, g_int_equal, d1, d2);
}

static void
init_tables (GModule module)
{
  ht_storage[module].module = module;
  ht_storage[module].metrics = new_ht_metrics ();

  /* Initialize metrics hash tables */
  ht_storage[module].metrics->keymap = new_str_ht (g_free, g_free);
  ht_storage[module].metrics->datamap = new_int_ht (g_free, g_free);
  ht_storage[module].metrics->rootmap = new_int_ht (g_free, g_free);
  ht_storage[module].metrics->uniqmap = new_str_ht (g_free, g_free);

  ht_storage[module].metrics->hits = new_int_ht (g_free, NULL);
  ht_storage[module].metrics->visitors = new_int_ht (g_free, g_free);
  ht_storage[module].metrics->bw = new_int_ht (g_free, g_free);
  ht_storage[module].metrics->time_served = new_int_ht (g_free, g_free);
  ht_storage[module].metrics->methods = new_int_ht (g_free, g_free);
  ht_storage[module].metrics->protocols = new_int_ht (g_free, g_free);
}

/* Initialize GLib hash tables */
void
init_storage (void)
{
  GModule module;

  ht_hostnames = new_str_ht (g_free, g_free);
  ht_hosts_agents = new_str_ht (g_free, g_free);
  ht_unique_keys = new_str_ht (g_free, g_free);

  ht_storage = new_gstorage (TOTAL_MODULES);
  for (module = 0; module < TOTAL_MODULES; ++module) {
    init_tables (module);
  }
}

uint32_t
get_ht_size (GHashTable * ht)
{
  return g_hash_table_size (ht);
}

uint32_t
get_ht_size_by_metric (GModule module, GMetric metric)
{
  GHashTable *ht = get_storage_metric (module, metric);

  return get_ht_size (ht);
}

int
ht_insert_keymap (GHashTable * ht, const char *value)
{
  gpointer value_ptr;
  int nkey = 0, size = 0;

  if (ht == NULL)
    return (EINVAL);

  value_ptr = g_hash_table_lookup (ht, value);
  if (value_ptr != NULL)
    return (*(int *) value_ptr);

  size = get_ht_size (ht);
  /* the auto increment value starts at SIZE (hash table) + 1 */
  nkey = size > 0 ? size + 1 : 1;

  g_hash_table_replace (ht, g_strdup (value), int2ptr (nkey));

  return nkey;
}

int
ht_insert_uniqmap (GHashTable * ht, char *uniq_key)
{
  int nkey = 0, size = 0;

  if (ht == NULL)
    return (EINVAL);

  if ((g_hash_table_lookup (ht, uniq_key)) != NULL)
    return 0;

  size = get_ht_size (ht);
  /* the auto increment value starts at SIZE (hash table) + 1 */
  nkey = size > 0 ? size + 1 : 1;

  g_hash_table_replace (ht, uniq_key, int2ptr (nkey));

  return nkey;
}

int
ht_insert_nkey_nval (GHashTable * ht, int nkey, int nval)
{
  if (ht == NULL)
    return (EINVAL);

  if ((g_hash_table_lookup (ht, &nkey)) != NULL)
    return 1;

  g_hash_table_replace (ht, int2ptr (nkey), int2ptr (nval));

  return 0;
}

int
ht_insert_unique_key (const char *key)
{
  return ht_insert_keymap (ht_unique_keys, key);
}

int
ht_insert_agent (const char *key)
{
  return ht_insert_keymap (ht_hosts_agents, key);
}

int
ht_insert_nodemap (GHashTable * ht, int nkey, const char *value)
{
  if (ht == NULL)
    return (EINVAL);

  g_hash_table_replace (ht, int2ptr (nkey), g_strdup (value));

  return 0;
}

/* store generic data into the given hash table */
int
ht_insert_hit (GHashTable * ht, int data_nkey, int uniq_nkey, int root_nkey)
{
  GDataMap *map;

  if (ht == NULL)
    return (EINVAL);

  map = g_hash_table_lookup (ht, &data_nkey);
  if (map != NULL) {
    map->data++;
  } else {
    map = xcalloc (1, sizeof (GDataMap));
    map->data = 1;
    map->root = root_nkey;
    map->uniq = uniq_nkey;
  }
  g_hash_table_replace (ht, int2ptr (data_nkey), map);

  return 0;
}

int
ht_insert_num (GHashTable * ht, int data_nkey)
{
  gpointer value_ptr;
  int add_value;

  if (ht == NULL)
    return (EINVAL);

  value_ptr = g_hash_table_lookup (ht, &data_nkey);
  if (value_ptr != NULL) {
    add_value = (*(int *) value_ptr) + 1;
  } else {
    add_value = 1;
  }
  g_hash_table_replace (ht, int2ptr (data_nkey), int2ptr (add_value));

  return 0;
}

int
ht_insert_cumulative (GHashTable * ht, int data_nkey, uint64_t size)
{
  gpointer value_ptr;
  uint64_t add_value;

  if (ht == NULL)
    return (EINVAL);

  value_ptr = g_hash_table_lookup (ht, &data_nkey);
  if (value_ptr != NULL) {
    add_value = (*(uint64_t *) value_ptr) + size;
  } else {
    add_value = 0 + size;
  }
  g_hash_table_replace (ht, int2ptr (data_nkey), uint642ptr (add_value));

  return 0;
}

char *
get_root_from_key (int root_nkey, GModule module)
{
  GHashTable *ht = NULL;
  gpointer value_ptr;

  ht = get_storage_metric (module, MTRC_ROOTMAP);
  if (ht == NULL)
    return NULL;

  value_ptr = g_hash_table_lookup (ht, &root_nkey);
  if (value_ptr != NULL)
    return xstrdup ((char *) value_ptr);

  return NULL;
}

char *
get_node_from_key (int data_nkey, GModule module, GMetric metric)
{
  GHashTable *ht = NULL;
  GStorageMetrics *metrics;
  gpointer value_ptr;

  metrics = get_storage_metrics_by_module (module);
  /* bandwidth modules */
  switch (metric) {
  case MTRC_DATAMAP:
    ht = metrics->datamap;
    break;
  case MTRC_METHODS:
    ht = metrics->methods;
    break;
  case MTRC_PROTOCOLS:
    ht = metrics->protocols;
    break;
  default:
    ht = NULL;
  }

  if (ht == NULL)
    return NULL;

  value_ptr = g_hash_table_lookup (ht, &data_nkey);
  if (value_ptr != NULL)
    return xstrdup ((char *) value_ptr);

  return NULL;
}

uint64_t
get_cumulative_from_key (int data_nkey, GModule module, GMetric metric)
{
  gpointer value_ptr;
  GHashTable *ht = NULL;
  GStorageMetrics *metrics;

  metrics = get_storage_metrics_by_module (module);
  /* bandwidth modules */
  switch (metric) {
  case MTRC_BW:
    ht = metrics->bw;
    break;
  case MTRC_TIME_SERVED:
    ht = metrics->time_served;
    break;
  default:
    ht = NULL;
  }

  if (ht == NULL)
    return 0;

  value_ptr = g_hash_table_lookup (ht, &data_nkey);
  if (value_ptr != NULL)
    return (*(uint64_t *) value_ptr);
  return 0;
}

int
get_num_from_key (int data_nkey, GModule module, GMetric metric)
{
  gpointer value_ptr;
  GHashTable *ht = NULL;
  GStorageMetrics *metrics;

  metrics = get_storage_metrics_by_module (module);
  /* bandwidth modules */
  switch (metric) {
  case MTRC_HITS:
    ht = metrics->hits;
    break;
  case MTRC_VISITORS:
    ht = metrics->visitors;
    break;
  default:
    ht = NULL;
  }

  if (ht == NULL)
    return 0;

  value_ptr = g_hash_table_lookup (ht, &data_nkey);
  if (value_ptr != NULL)
    return (*(int *) value_ptr);
  return 0;
}

char *
get_hostname (const char *host)
{
  int found;
  void *value_ptr;

  found = g_hash_table_lookup_extended (ht_hostnames, host, NULL, &value_ptr);
  if (found && value_ptr)
    return xstrdup (value_ptr);
  return NULL;
}


/* iterate over the key/value pairs in the hash table */
static void
raw_data_iter (gpointer k, gpointer v, gpointer data_ptr)
{
  GRawData *raw_data = data_ptr;
  raw_data->items[raw_data->idx].key = k;
  raw_data->items[raw_data->idx].value = v;
  raw_data->idx++;
}

/* store the key/value pairs from a hash table into raw_data */
GRawData *
parse_raw_data (GHashTable * ht, int ht_size, GModule module)
{
  GRawData *raw_data;
  raw_data = new_grawdata ();

  raw_data->size = ht_size;
  raw_data->module = module;
  raw_data->idx = 0;
  raw_data->items = new_grawdata_item (ht_size);

  g_hash_table_foreach (ht, (GHFunc) raw_data_iter, raw_data);
  sort_raw_data (raw_data, ht_size);

  return raw_data;
}
