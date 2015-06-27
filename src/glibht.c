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

#include "sort.h"
#include "xmalloc.h"

GStorage *ht_storage;

/* tables for the whole app */
GHashTable *ht_agent_keys = NULL;
GHashTable *ht_agent_vals = NULL;
GHashTable *ht_hostnames = NULL;
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
free_hits (GO_UNUSED gpointer old_key, gpointer old_value,
           GO_UNUSED gpointer user_data)
{
  GDataMap *map = old_value;
  free (map);
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
  ht_storage[module].metrics->agents = new_int_ht (g_free, NULL);
}

/* Initialize GLib hash tables */
void
init_storage (void)
{
  GModule module;

  ht_agent_keys = new_str_ht (g_free, g_free);
  ht_agent_vals = new_str_ht (g_free, g_free);
  ht_hostnames = new_str_ht (g_free, g_free);
  ht_unique_keys = new_str_ht (g_free, g_free);

  ht_storage = new_gstorage (TOTAL_MODULES);
  for (module = 0; module < TOTAL_MODULES; ++module) {
    init_tables (module);
  }
}

static void
free_tables (GStorageMetrics * metrics)
{
  g_hash_table_foreach (metrics->hits, free_hits, NULL);

  /* Initialize metrics hash tables */
  g_hash_table_destroy (metrics->keymap);
  g_hash_table_destroy (metrics->datamap);
  g_hash_table_destroy (metrics->rootmap);
  g_hash_table_destroy (metrics->uniqmap);
  g_hash_table_destroy (metrics->hits);
  g_hash_table_destroy (metrics->visitors);
  g_hash_table_destroy (metrics->bw);
  g_hash_table_destroy (metrics->time_served);
  g_hash_table_destroy (metrics->methods);
  g_hash_table_destroy (metrics->protocols);
  g_hash_table_destroy (metrics->agents);
}

void
free_storage (void)
{
  GModule module;

  g_hash_table_destroy (ht_agent_keys);
  g_hash_table_destroy (ht_agent_vals);
  g_hash_table_destroy (ht_hostnames);
  g_hash_table_destroy (ht_unique_keys);

  for (module = 0; module < TOTAL_MODULES; ++module) {
    free_tables (ht_storage[module].metrics);
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

static int
find_host_agent_in_list (void *data, void *needle)
{
  return (*(int *) data) == (*(int *) needle) ? 1 : 0;
}

int
ht_insert_host_agent (GHashTable * ht, int data_nkey, int agent_nkey)
{
  GSLList *list, *match;

  if (ht == NULL)
    return (EINVAL);

  list = g_hash_table_lookup (ht, &data_nkey);
  if (list != NULL) {
    if ((match = list_find (list, find_host_agent_in_list, &agent_nkey)))
      goto out;
    list = list_insert_prepend (list, int2ptr (agent_nkey));
  } else {
    list = list_create (int2ptr (agent_nkey));
  }
  g_hash_table_replace (ht, int2ptr (data_nkey), list);
out:

  return 0;
}

int
ht_insert_str_from_int_key (GHashTable * ht, int nkey, const char *value)
{
  if (ht == NULL)
    return (EINVAL);

  g_hash_table_replace (ht, int2ptr (nkey), g_strdup (value));

  return 0;
}

static int
ht_inc_int_from_key (GHashTable * ht, void *key, int inc)
{
  gpointer value_ptr;
  int add_value;

  if (ht == NULL)
    return (EINVAL);

  value_ptr = g_hash_table_lookup (ht, key);
  if (value_ptr != NULL) {
    add_value = (*(int *) value_ptr) + inc;
  } else {
    add_value = 0 + inc;
  }
  g_hash_table_replace (ht, key, int2ptr (add_value));

  return 0;
}

static int
ht_inc_u64_from_key (GHashTable * ht, void *key, uint64_t inc)
{
  gpointer value_ptr;
  uint64_t add_value;

  if (ht == NULL)
    return (EINVAL);

  value_ptr = g_hash_table_lookup (ht, key);
  if (value_ptr != NULL) {
    add_value = (*(uint64_t *) value_ptr) + inc;
  } else {
    add_value = 0 + inc;
  }
  g_hash_table_replace (ht, key, uint642ptr (add_value));

  return 0;
}

int
ht_inc_int_from_int_key (GHashTable * ht, int data_nkey, int inc)
{
  int ret, *key;
  if (ht == NULL)
    return (EINVAL);

  key = int2ptr (data_nkey);
  if ((ret = ht_inc_int_from_key (ht, key, inc)) != 0)
    free (key);

  return ret;
}

int
ht_inc_u64_from_int_key (GHashTable * ht, int data_nkey, uint64_t inc)
{
  int ret, *key;
  if (ht == NULL)
    return (EINVAL);

  key = int2ptr (data_nkey);
  if ((ret = ht_inc_u64_from_key (ht, key, inc)) != 0)
    free (key);

  return ret;
}

int
ht_inc_int_from_str_key (GHashTable * ht, char *key, int inc)
{
  if (ht == NULL)
    return (EINVAL);

  return ht_inc_int_from_key (ht, key, inc);
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
get_int_from_int_key (GHashTable * ht, int nkey)
{
  gpointer value_ptr;

  if (ht == NULL)
    return (EINVAL);

  value_ptr = g_hash_table_lookup (ht, &nkey);
  if (value_ptr != NULL)
    return (*(int *) value_ptr);

  return 0;
}

int
get_int_from_str_key (GHashTable * ht, const char *key)
{
  gpointer value_ptr;

  if (ht == NULL)
    return (EINVAL);

  value_ptr = g_hash_table_lookup (ht, key);
  if (value_ptr != NULL)
    return (*(int *) value_ptr);

  return 0;
}

unsigned int
get_uint_from_str_key (GHashTable * ht, const char *key)
{
  gpointer value_ptr;

  if (ht == NULL)
    return (EINVAL);

  value_ptr = g_hash_table_lookup (ht, key);
  if (value_ptr != NULL)
    return (*(unsigned int *) value_ptr);

  return 0;
}

char *
get_str_from_int_key (GHashTable * ht, int nkey)
{
  gpointer value_ptr;

  if (ht == NULL)
    return NULL;

  value_ptr = g_hash_table_lookup (ht, &nkey);
  if (value_ptr != NULL)
    return xstrdup ((char *) value_ptr);

  return NULL;
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

  return get_str_from_int_key (ht, data_nkey);
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

  return get_int_from_int_key (ht, data_nkey);
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

int
get_int_from_keymap (const char *key, GModule module)
{
  GHashTable *ht = get_storage_metric (module, MTRC_KEYMAP);

  return get_int_from_str_key (ht, key);
}

char *
get_host_agent_val (int agent_nkey)
{
  return get_str_from_int_key (ht_agent_vals, agent_nkey);
}

void *
get_host_agent_list (int data_nkey)
{
  GHashTable *ht;
  void *list;

  ht = get_storage_metric (HOSTS, MTRC_AGENTS);
  if ((list = g_hash_table_lookup (ht, &data_nkey)))
    return list;
  return NULL;
}

static void
free_agent_values (GO_UNUSED gpointer k, gpointer v,
                   GO_UNUSED gpointer data_ptr)
{
  void *list = v;

  if (list != NULL)
    list_remove_nodes (list);
}

void
free_agent_list (void)
{
  GHashTable *ht = get_storage_metric (HOSTS, MTRC_AGENTS);

  g_hash_table_foreach (ht, (GHFunc) free_agent_values, NULL);
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
