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

#include "error.h"
#include "parser.h"
#include "settings.h"
#include "util.h"
#include "xmalloc.h"

GHashTable *ht_browsers = NULL;
GHashTable *ht_countries = NULL;
GHashTable *ht_date_bw = NULL;
GHashTable *ht_file_bw = NULL;
GHashTable *ht_file_serve_usecs = NULL;
GHashTable *ht_host_bw = NULL;
GHashTable *ht_hostnames = NULL;
GHashTable *ht_hosts_agents = NULL;
GHashTable *ht_host_serve_usecs = NULL;
GHashTable *ht_hosts = NULL;
GHashTable *ht_keyphrases = NULL;
GHashTable *ht_monthly = NULL;
GHashTable *ht_not_found_requests = NULL;
GHashTable *ht_os = NULL;
GHashTable *ht_referrers = NULL;
GHashTable *ht_referring_sites = NULL;
GHashTable *ht_request_keys = NULL;
GHashTable *ht_request_methods = NULL;
GHashTable *ht_request_protocols = NULL;
GHashTable *ht_requests = NULL;
GHashTable *ht_requests_static = NULL;
GHashTable *ht_status_code = NULL;
GHashTable *ht_unique_visitors = NULL;
GHashTable *ht_unique_vis = NULL;

static GHashTable *
new_ht (GDestroyNotify d1, GDestroyNotify d2)
{
  return g_hash_table_new_full (g_str_hash, g_str_equal, d1, d2);
}

/* Initialize GLib hash tables */
void
init_storage (void)
{
  ht_date_bw = new_ht (g_free, g_free);
  ht_file_bw = new_ht (g_free, g_free);
  ht_file_serve_usecs = new_ht (g_free, g_free);
  ht_host_bw = new_ht (g_free, g_free);
  ht_hostnames = new_ht (g_free, g_free);
  ht_hosts_agents = new_ht (g_free, g_free);
  ht_host_serve_usecs = new_ht (g_free, g_free);
  ht_not_found_requests = new_ht (g_free, g_free);
  ht_request_keys = new_ht (g_free, g_free);
  ht_request_methods = new_ht (g_free, g_free);
  ht_request_protocols = new_ht (g_free, g_free);
  ht_requests = new_ht (g_free, g_free);
  ht_requests_static = new_ht (g_free, g_free);
  ht_keyphrases = new_ht (g_free, g_free);
  ht_referring_sites = new_ht (g_free, g_free);
  ht_status_code = new_ht (g_free, g_free);
  ht_hosts = new_ht (g_free, g_free);
  ht_unique_vis = new_ht (g_free, g_free);
  ht_referrers = new_ht (g_free, g_free);
  ht_unique_visitors = new_ht (g_free, g_free);

  /* The following tables contain a structure as their value, thus we
     use a special iterator to free its value */
  ht_browsers = new_ht (g_free, NULL);
  ht_countries = new_ht (g_free, NULL);
  ht_os = new_ht (g_free, NULL);
}

#ifdef HAVE_LIBGEOIP
void
free_countries (GO_UNUSED gpointer old_key, gpointer old_value,
                GO_UNUSED gpointer user_data)
{
  GLocation *loc = old_value;
  free (loc);
}
#endif

void
free_os (GO_UNUSED gpointer old_key, gpointer old_value,
         GO_UNUSED gpointer user_data)
{
  GOpeSys *opesys = old_value;
  free (opesys);
}

void
free_browser (GO_UNUSED gpointer old_key, gpointer old_value,
              GO_UNUSED gpointer user_data)
{
  GBrowser *browser = old_value;
  free (browser);
}

/* free memory allocated by g_hash_table_new_full() function */
void
free_key_value (gpointer old_key, GO_UNUSED gpointer old_value,
                GO_UNUSED gpointer user_data)
{
  g_free (old_key);
}

uint32_t
get_ht_size (GHashTable * ht)
{
  return g_hash_table_size (ht);
}

int
process_request_meta (GHashTable * ht, char *key, uint64_t size)
{
  gpointer value_ptr;
  uint64_t add_value;
  uint64_t *ptr_value;

  if ((ht == NULL) || (key == NULL))
    return (EINVAL);

  value_ptr = g_hash_table_lookup (ht, key);
  if (value_ptr != NULL) {
    ptr_value = (uint64_t *) value_ptr;
    add_value = *ptr_value + size;
  } else {
    add_value = 0 + size;
  }

  ptr_value = xmalloc (sizeof (uint64_t));
  *ptr_value = add_value;

  g_hash_table_replace (ht, g_strdup (key), ptr_value);

  return 0;
}

int
process_opesys (GHashTable * ht, const char *key, const char *os_type)
{
  GOpeSys *opesys;
  if (ht == NULL)
    return (EINVAL);

  opesys = g_hash_table_lookup (ht, key);
  if (opesys != NULL) {
    opesys->hits++;
  } else {
    opesys = xcalloc (1, sizeof (GOpeSys));
    strcpy (opesys->os_type, os_type);
    opesys->hits = 1;
  }

  /* replace the entry. old key will be freed by "free_os" */
  g_hash_table_replace (ht, g_strdup (key), opesys);

  return 0;
}

int
process_browser (GHashTable * ht, const char *key, const char *browser_type)
{
  GBrowser *browser;
  if (ht == NULL)
    return (EINVAL);

  browser = g_hash_table_lookup (ht, key);
  if (browser != NULL) {
    browser->hits++;
  } else {
    browser = xcalloc (1, sizeof (GBrowser));
    strcpy (browser->browser_type, browser_type);
    browser->hits = 1;
  }

  /* replace the entry. old key will be freed by "free_browser" */
  g_hash_table_replace (ht, g_strdup (key), browser);

  return 0;
}

static void
process_request_protocol (const char *key, const char *protocol)
{
  g_hash_table_replace (ht_request_protocols, g_strdup (key),
                        g_strdup (protocol));
}

static void
process_request_method (const char *key, const char *method)
{
  g_hash_table_replace (ht_request_methods, g_strdup (key), g_strdup (method));
}

static void
process_request_keys (const char *key, const char *request)
{
  g_hash_table_replace (ht_request_keys, g_strdup (key), g_strdup (request));
}

int
process_request (GHashTable * ht, const char *key, const GLogItem * glog)
{
  if ((ht == NULL) || (key == NULL))
    return (EINVAL);

  process_generic_data (ht, key);
  if (conf.append_protocol && glog->protocol)
    process_request_protocol (key, glog->protocol);
  if (conf.append_method && glog->method)
    process_request_method (key, glog->method);
  process_request_keys (key, glog->req);

  return 0;
}

#ifdef HAVE_LIBGEOIP
int
process_geolocation (GHashTable * ht, const char *ctry, const char *cont,
                     const char *city)
{
  GLocation *location;
  if (ht == NULL)
    return (EINVAL);

  location = g_hash_table_lookup (ht, ctry);
  if (location != NULL) {
    location->hits++;
  } else {
    location = xcalloc (1, sizeof (GLocation));
    xstrncpy (location->continent, cont, CONTINENT_LEN);
    if (city[0] != '\0')
      xstrncpy (location->city, city, CITY_LEN);
    location->hits = 1;
  }

  /* replace the entry. old key will be freed by "free_countries" */
  g_hash_table_replace (ht, g_strdup (ctry), location);

  return 0;
}
#endif

/* store generic data into the given hash table */
int
process_generic_data (GHashTable * ht, const char *key)
{
  gpointer value_ptr;
  int add_value, first = 0;
  int *ptr_value;

  if ((ht == NULL) || (key == NULL))
    return (EINVAL);

  value_ptr = g_hash_table_lookup (ht, key);
  if (value_ptr != NULL) {
    ptr_value = (int *) value_ptr;
    add_value = *ptr_value + 1;
  } else {
    first = add_value = 1;
  }

  ptr_value = xmalloc (sizeof (int));
  *ptr_value = add_value;

  /* replace the entry. old key will be freed by "free_key_value" */
  g_hash_table_replace (ht, g_strdup (key), ptr_value);

  return first ? KEY_NOT_FOUND : KEY_FOUND;
}

GHashTable *
get_ht_by_module (GModule module)
{
  GHashTable *ht;

  switch (module) {
   case VISITORS:
     ht = ht_unique_vis;
     break;
   case REQUESTS:
     ht = ht_requests;
     break;
   case REQUESTS_STATIC:
     ht = ht_requests_static;
     break;
   case NOT_FOUND:
     ht = ht_not_found_requests;
     break;
   case HOSTS:
     ht = ht_hosts;
     break;
   case OS:
     ht = ht_os;
     break;
   case BROWSERS:
     ht = ht_browsers;
     break;
   case REFERRERS:
     ht = ht_referrers;
     break;
   case REFERRING_SITES:
     ht = ht_referring_sites;
     break;
   case KEYPHRASES:
     ht = ht_keyphrases;
     break;
#ifdef HAVE_LIBGEOIP
   case GEO_LOCATION:
     ht = ht_countries;
     break;
#endif
   case STATUS_CODES:
     ht = ht_status_code;
     break;
   default:
     return NULL;
  }

  return ht;
}

/* process host agent strings */
int
process_host_agents (char *host, char *agent)
{
  char *ptr_value, *tmp, *a;
  GHashTable *ht = ht_hosts_agents;
  gpointer value_ptr;
  size_t len1, len2;

  if ((ht == NULL) || (host == NULL) || (agent == NULL))
    return (EINVAL);

  a = xstrdup (agent);

  value_ptr = g_hash_table_lookup (ht, host);
  if (value_ptr != NULL) {
    ptr_value = (char *) value_ptr;
    if (strstr (ptr_value, a)) {
      if (a != NULL)
        free (a);
      return 0;
    }

    len1 = strlen (ptr_value);
    len2 = strlen (a);

    tmp = xmalloc (len1 + len2 + 2);
    memcpy (tmp, ptr_value, len1);
    tmp[len1] = '|';
    /*
     * NUL-terminated
     */
    memcpy (tmp + len1 + 1, a, len2 + 1);
  } else
    tmp = alloc_string (a);

  g_hash_table_replace (ht, g_strdup (host), tmp);
  if (a != NULL)
    free (a);
  return 0;
}

char *
get_request_meta (const char *k, GReqMeta meta)
{
  GHashTable *ht = NULL;
  gpointer value_ptr;

  switch (meta) {
   case REQUEST:
     ht = ht_request_keys;
     break;
   case REQUEST_METHOD:
     ht = ht_request_methods;
     break;
   case REQUEST_PROTOCOL:
     ht = ht_request_protocols;
     break;
   default:
     ht = NULL;
  }

  if (ht == NULL)
    return 0;

  value_ptr = g_hash_table_lookup (ht, k);
  if (value_ptr != NULL)
    return xstrdup ((char *) value_ptr);

  return alloc_string ("---");
}

uint64_t
get_bandwidth (const char *k, GModule module)
{
  gpointer value_ptr;
  GHashTable *ht = NULL;

  /* bandwidth modules */
  switch (module) {
   case VISITORS:
     ht = ht_date_bw;
     break;
   case REQUESTS:
   case REQUESTS_STATIC:
   case NOT_FOUND:
     ht = ht_file_bw;
     break;
   case HOSTS:
     ht = ht_host_bw;
     break;
   default:
     ht = NULL;
  }

  if (ht == NULL)
    return 0;

  value_ptr = g_hash_table_lookup (ht, k);
  if (value_ptr != NULL)
    return (*(uint64_t *) value_ptr);
  return 0;
}

/* get time taken to serve the request, in microseconds for given key */
uint64_t
get_serve_time (const char *key, GModule module)
{
  gpointer value_ptr;

  /* bandwidth modules */
  GHashTable *ht = NULL;
  switch (module) {
   case HOSTS:
     ht = ht_host_serve_usecs;
     break;
   case REQUESTS:
   case REQUESTS_STATIC:
   case NOT_FOUND:
     ht = ht_file_serve_usecs;
     break;
   default:
     ht = NULL;
  }

  if (ht == NULL)
    return 0;

  value_ptr = g_hash_table_lookup (ht, key);
  if (value_ptr != NULL)
    return *(uint64_t *) value_ptr;
  return 0;
}

/* iterate over the key/value pairs in the hash table */
static void
raw_data_iter (gpointer k, gpointer v, gpointer data_ptr)
{
  GRawData *raw_data = data_ptr;
  raw_data->items[raw_data->idx].key = (gchar *) k;
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
  sort_raw_data (raw_data, module, ht_size);

  return raw_data;
}
