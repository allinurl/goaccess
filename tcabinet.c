/**
 * tcabinet.c -- Tokyo Cabinet database functions
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
#include <tcbdb.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "tcabinet.h"

#include "commons.h"
#include "error.h"
#include "parser.h"
#include "settings.h"
#include "util.h"
#include "xmalloc.h"

TCBDB *ht_browsers = NULL;
TCBDB *ht_countries = NULL;
TCBDB *ht_date_bw = NULL;
TCBDB *ht_file_bw = NULL;
TCBDB *ht_file_serve_usecs = NULL;
TCBDB *ht_host_bw = NULL;
TCBDB *ht_hostnames = NULL;
TCBDB *ht_hosts = NULL;
TCBDB *ht_host_serve_usecs = NULL;
TCBDB *ht_keyphrases = NULL;
TCBDB *ht_not_found_requests = NULL;
TCBDB *ht_os = NULL;
TCBDB *ht_hosts_agents = NULL;
TCBDB *ht_referrers = NULL;
TCBDB *ht_referring_sites = NULL;
TCBDB *ht_requests = NULL;
TCBDB *ht_requests_static = NULL;
TCBDB *ht_status_code = NULL;
TCBDB *ht_unique_vis = NULL;
TCBDB *ht_unique_visitors = NULL;

static char *
tc_db_set_path (const char *dbname)
{
  char *path;

  if (conf.db_path != NULL) {
    path = xmalloc (snprintf (NULL, 0, "%s%s", conf.db_path, dbname) + 1);
    sprintf (path, "%s%s", conf.db_path, dbname);
  } else {
    path = xmalloc (snprintf (NULL, 0, "%s%s", TC_DBPATH, dbname) + 1);
    sprintf (path, "%s%s", TC_DBPATH, dbname);
  }
  return path;
}

/* Open the database handle */
TCBDB *
tc_db_create (const char *dbname)
{
  char *path = NULL;
  TCBDB *bdb;
  int ecode;
  uint32_t lcnum, ncnum, lmemb, nmemb, bnum;

  path = tc_db_set_path (dbname);

  bdb = tcbdbnew ();

  lcnum = conf.cache_lcnum > 0 ? conf.cache_lcnum : TC_LCNUM;
  ncnum = conf.cache_ncnum > 0 ? conf.cache_ncnum : TC_NCNUM;

  LOG_DEBUG (("%s\n", path));
  LOG_DEBUG (("lcnum, ncnum: %d, %d\n", lcnum, ncnum));

  /* set the caching parameters of a B+ tree database object */
  if (!tcbdbsetcache (bdb, lcnum, ncnum)) {
    free (path);
    LOG_DEBUG (("Unable to set TCB cache.\n\n"));
    error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                   "Unable to set TCB cache");
  }

  lmemb = conf.tune_lmemb > 0 ? conf.tune_lmemb : TC_LMEMB;
  nmemb = conf.tune_nmemb > 0 ? conf.tune_nmemb : TC_NMEMB;
  bnum = conf.tune_bnum > 0 ? conf.tune_bnum : TC_BNUM;

  LOG_DEBUG (("lmemb, nmemb, bnum: %d, %d, %d\n\n", lmemb, nmemb, bnum));
  tcbdbtune (bdb, lmemb, nmemb, bnum, 8, 10, BDBTBZIP);

  /* attempt to open the database */
  if (!tcbdbopen (bdb, path, BDBOWRITER | BDBOCREAT | BDBOTRUNC)) {
    free (path);
    ecode = tcbdbecode (bdb);

    LOG_DEBUG (("%s\n\n", tcbdberrmsg (ecode)));
    error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                   tcbdberrmsg (ecode));
  }
  free (path);

  return bdb;
}

/* Close the database handle */
int
tc_db_close (TCBDB * bdb, const char *dbname)
{
  int ecode;

  if (bdb == NULL)
    return 1;

  /* close the database */
  if (!tcbdbclose (bdb)) {
    ecode = tcbdbecode (bdb);
    error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                   tcbdberrmsg (ecode));
  }
  /* delete the object */
  tcbdbdel (bdb);
  /* remove database file */
  tcremovelink (dbname);

  return 0;
}

/* Calls the given function for each of the key/value pairs */
void
tc_db_foreach (TCBDB * bdb,
               void (*fp) (BDBCUR * cur, char *k, int s, void *u),
               void *user_data)
{
  BDBCUR *cur;
  int ksize;
  char *key = NULL;

  cur = tcbdbcurnew (bdb);
  tcbdbcurfirst (cur);
  while ((key = tcbdbcurkey (cur, &ksize)) != NULL)
    (*fp) (cur, key, ksize, user_data);

  tcbdbcurdel (cur);
}

/* Return number of records of a hash database */
unsigned int
get_ht_size (TCBDB * bdb)
{
  if (bdb == NULL)
    return 0;
  return tcbdbrnum (bdb);
}

/* Store generic data into the given hash table */
int
process_generic_data (TCBDB * bdb, const char *k)
{
  return tcbdbaddint (bdb, k, strlen (k), 1) == 1 ? KEY_NOT_FOUND : KEY_FOUND;
}

int
process_request (TCBDB * bdb, const char *k, const GLogItem * glog)
{
  GRequest *request;
  int sp = 0, ecode;
  void *value;

  if ((bdb == NULL) || (k == NULL))
    return (EINVAL);

  if ((value = tcbdbget (bdb, k, strlen (k), &sp)) != NULL) {
    request = value;
    request->hits++;
  } else {
    request = xcalloc (1, sizeof (GRequest));

    if (conf.append_protocol && glog->protocol)
      xstrncpy (request->protocol, glog->protocol, REQ_PROTO_LEN);
    if (conf.append_method && glog->method)
      xstrncpy (request->method, glog->method, REQ_METHOD_LEN);

    request->request = alloc_string (glog->req);
    request->hits = 1;
  }

  if (!tcbdbput (bdb, k, strlen (k), request, sizeof (GRequest))) {
    ecode = tcbdbecode (bdb);
    error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                   tcbdberrmsg (ecode));
  }
  if (request)
    free (request);

  return 0;
}

int
process_request_meta (TCBDB * bdb, const char *k, uint64_t size)
{
  int sp = 0, ecode;
  void *value;
  uint64_t add_value;

  if ((bdb == NULL) || (k == NULL))
    return (EINVAL);

  if ((value = tcbdbget (bdb, k, strlen (k), &sp)) != NULL) {
    add_value = (*(uint64_t *) value) + size;
  } else {
    add_value = 0 + size;
  }
  if (!tcbdbput (bdb, k, strlen (k), &add_value, sizeof (uint64_t))) {
    ecode = tcbdbecode (bdb);
    error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                   tcbdberrmsg (ecode));
  }
  free (value);

  return 0;
}

int
process_opesys (TCBDB * bdb, const char *k, const char *os_type)
{
  GOpeSys *opesys;
  int sp = 0, ecode;

  if ((bdb == NULL) || (k == NULL))
    return (EINVAL);

  if ((opesys = tcbdbget (bdb, k, strlen (k), &sp)) != NULL) {
    opesys->hits++;
  } else {
    opesys = xcalloc (1, sizeof (GOpeSys));
    xstrncpy (opesys->os_type, os_type, OPESYS_TYPE_LEN);
    opesys->hits = 1;
  }
  if (!tcbdbput (bdb, k, strlen (k), opesys, sizeof (GOpeSys))) {
    ecode = tcbdbecode (bdb);
    error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                   tcbdberrmsg (ecode));
  }
  if (opesys)
    free (opesys);

  return 0;
}

int
process_browser (TCBDB * bdb, const char *k, const char *browser_type)
{
  GBrowser *browser;
  int sp = 0, ecode;

  if ((bdb == NULL) || (k == NULL))
    return (EINVAL);

  if ((browser = tcbdbget (bdb, k, strlen (k), &sp)) != NULL) {
    browser->hits++;
  } else {
    browser = xcalloc (1, sizeof (GOpeSys));
    xstrncpy (browser->browser_type, browser_type, BROWSER_TYPE_LEN);
    browser->hits = 1;
  }
  if (!tcbdbput (bdb, k, strlen (k), browser, sizeof (GBrowser))) {
    ecode = tcbdbecode (bdb);
    error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                   tcbdberrmsg (ecode));
  }
  if (browser)
    free (browser);

  return 0;
}

int
process_geolocation (TCBDB * bdb, const char *cntry, const char *cont)
{
  GLocation *location;
  int sp = 0, ecode;

  if ((bdb == NULL) || (cntry == NULL))
    return (EINVAL);

  if ((location = tcbdbget (bdb, cntry, strlen (cntry), &sp)) != NULL) {
    location->hits++;
  } else {
    location = xcalloc (1, sizeof (GLocation));
    xstrncpy (location->continent, cont, CONTINENT_LEN);
    location->hits = 1;
  }
  if (!tcbdbput (bdb, cntry, strlen (cntry), location, sizeof (GLocation))) {
    ecode = tcbdbecode (bdb);
    error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                   tcbdberrmsg (ecode));
  }
  if (location)
    free (location);

  return 0;
}

/* process host agent strings */
int
process_host_agents (char *host, char *agent)
{
  int ecode;
  char *ptr_value = NULL, *tmp = NULL, *a = NULL;
  TCBDB *bdb = ht_hosts_agents;
  void *value_ptr;
  size_t len1, len2;

  if ((bdb == NULL) || (host == NULL) || (agent == NULL))
    return (EINVAL);

  a = xstrdup (agent);

  if ((value_ptr = tcbdbget2 (bdb, host)) != NULL) {
    ptr_value = (char *) value_ptr;
    if (strstr (ptr_value, a)) {
      if (a != NULL)
        goto out;
    }

    len1 = strlen (ptr_value);
    len2 = strlen (a);

    tmp = xmalloc (len1 + len2 + 2);
    memcpy (tmp, ptr_value, len1);
    tmp[len1] = '|';
    /* NUL-terminated */
    memcpy (tmp + len1 + 1, a, len2 + 1);
  } else
    tmp = alloc_string (a);

  if (!tcbdbput2 (bdb, host, tmp)) {
    ecode = tcbdbecode (bdb);
    error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                   tcbdberrmsg (ecode));
  }

out:
  if (a != NULL)
    free (a);
  if (tmp != NULL)
    free (tmp);
  if (value_ptr != NULL)
    free (value_ptr);

  return 0;
}

uint64_t
get_serve_time (const char *k, GModule module)
{
  int sp = 0;
  TCBDB *bdb = NULL;
  uint64_t serve_time = 0;
  void *value;

  /* serve time modules */
  switch (module) {
   case HOSTS:
     bdb = ht_host_serve_usecs;
     break;
   case REQUESTS:
   case REQUESTS_STATIC:
   case NOT_FOUND:
     bdb = ht_file_serve_usecs;
     break;
   default:
     bdb = NULL;
  }

  if (bdb == NULL)
    return 0;

  if ((value = tcbdbget (bdb, k, strlen (k), &sp)) != NULL) {
    serve_time = (*(uint64_t *) value);
    free (value);
  }

  return serve_time;
}

uint64_t
get_bandwidth (char *k, GModule module)
{
  int sp = 0;
  TCBDB *bdb = NULL;
  uint64_t bw = 0;
  void *value;

  /* bandwidth modules */
  switch (module) {
   case VISITORS:
     bdb = ht_date_bw;
     break;
   case REQUESTS:
   case REQUESTS_STATIC:
   case NOT_FOUND:
     bdb = ht_file_bw;
     break;
   case HOSTS:
     bdb = ht_host_bw;
     break;
   default:
     bdb = NULL;
  }

  if (bdb == NULL)
    return 0;

  if ((value = tcbdbget (bdb, k, strlen (k), &sp)) != NULL) {
    bw = (*(uint64_t *) value);
    free (value);
  }

  return bw;
}

TCBDB *
get_ht_by_module (GModule module)
{
  TCBDB *bdb;

  switch (module) {
   case VISITORS:
     bdb = ht_unique_vis;
     break;
   case REQUESTS:
     bdb = ht_requests;
     break;
   case REQUESTS_STATIC:
     bdb = ht_requests_static;
     break;
   case NOT_FOUND:
     bdb = ht_not_found_requests;
     break;
   case HOSTS:
     bdb = ht_hosts;
     break;
   case OS:
     bdb = ht_os;
     break;
   case BROWSERS:
     bdb = ht_browsers;
     break;
   case REFERRERS:
     bdb = ht_referrers;
     break;
   case REFERRING_SITES:
     bdb = ht_referring_sites;
     break;
   case KEYPHRASES:
     bdb = ht_keyphrases;
     break;
#ifdef HAVE_LIBGEOIP
   case GEO_LOCATION:
     bdb = ht_countries;
     break;
#endif
   case STATUS_CODES:
     bdb = ht_status_code;
     break;
   default:
     return NULL;
  }

  return bdb;
}

/* This function frees all the requests fields from GRequest.
 *
 * Note: we need to go over all of them before exiting the program
 * since TokyoCabinet keeps the same pointer at all times.
 */
void
free_requests (BDBCUR * cur, char *key, GO_UNUSED int ksize,
               GO_UNUSED void *user_data)
{
  GRequest *request;
  int vsize = 0;

  request = tcbdbcurval (cur, &vsize);
  if (request) {
    if (request->request)
      free (request->request);
    free (request);
  }
  free (key);
  tcbdbcurnext (cur);
}

static void
data_iter_generic (BDBCUR * cur, char *key, GO_UNUSED int ksize,
                   void *user_data)
{
  GRawData *raw_data = user_data;
  void *value;
  int vsize = 0;

  value = tcbdbcurval (cur, &vsize);
  if (value) {
    raw_data->items[raw_data->idx].key = key;
    raw_data->items[raw_data->idx].value = value;
    raw_data->idx++;
  }
  tcbdbcurnext (cur);
}

GRawData *
parse_raw_data (TCBDB * bdb, int ht_size, GModule module)
{
  GRawData *raw_data;

  raw_data = new_grawdata ();
  raw_data->size = ht_size;
  raw_data->module = module;
  raw_data->idx = 0;
  raw_data->items = new_grawdata_item (ht_size);

  tc_db_foreach (bdb, data_iter_generic, raw_data);
  switch (module) {
   case VISITORS:
     qsort (raw_data->items, ht_size, sizeof (GRawDataItem), cmp_raw_data_desc);
     break;
   case REQUESTS:
   case REQUESTS_STATIC:
   case NOT_FOUND:
     qsort (raw_data->items, ht_size, sizeof (GRawDataItem),
            cmp_raw_req_num_desc);
     break;
   case OS:
     qsort (raw_data->items, ht_size, sizeof (GRawDataItem),
            cmp_raw_os_num_desc);
     break;
   case BROWSERS:
     qsort (raw_data->items, ht_size, sizeof (GRawDataItem),
            cmp_raw_browser_num_desc);
     break;
#ifdef HAVE_LIBGEOIP
   case GEO_LOCATION:
     qsort (raw_data->items, ht_size, sizeof (GRawDataItem),
            cmp_raw_geo_num_desc);
     break;
#endif
   default:
     qsort (raw_data->items, ht_size, sizeof (GRawDataItem), cmp_raw_num_desc);
  }

  return raw_data;
}
