/**
 * gholder.c -- data structure to hold raw data
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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabdb.h"
#else
#include "gkhash.h"
#endif

#ifdef HAVE_LIBGEOIP
#include "geolocation.h"
#endif

#include "gholder.h"

#include "error.h"
#include "gdns.h"
#include "util.h"
#include "xmalloc.h"

typedef struct GPanel_
{
  GModule module;
  void (*insert) (GRawDataItem item, GHolder * h, const struct GPanel_ *);
  void (*holder_callback) (GHolder * h);
  void (*lookup) (GRawDataItem item);
} GPanel;

static void add_data_to_holder (GRawDataItem item, GHolder * h,
                                const GPanel * panel);
static void add_host_to_holder (GRawDataItem item, GHolder * h,
                                const GPanel * panel);
static void add_root_to_holder (GRawDataItem item, GHolder * h,
                                const GPanel * panel);
static void add_host_child_to_holder (GHolder * h);
static void data_visitors (GHolder * h);

/* *INDENT-OFF* */
static GPanel paneling[] = {
  {VISITORS        , add_data_to_holder, data_visitors} ,
  {REQUESTS        , add_data_to_holder, NULL} ,
  {REQUESTS_STATIC , add_data_to_holder, NULL} ,
  {NOT_FOUND       , add_data_to_holder, NULL} ,
  {HOSTS           , add_host_to_holder, add_host_child_to_holder} ,
  {OS              , add_root_to_holder, NULL} ,
  {BROWSERS        , add_root_to_holder, NULL} ,
  {VISIT_TIMES     , add_data_to_holder, NULL} ,
  {VIRTUAL_HOSTS   , add_data_to_holder, NULL} ,
  {REFERRERS       , add_data_to_holder, NULL} ,
  {REFERRING_SITES , add_data_to_holder, NULL} ,
  {KEYPHRASES      , add_data_to_holder, NULL} ,
#ifdef HAVE_LIBGEOIP
  {GEO_LOCATION    , add_root_to_holder, NULL} ,
#endif
  {STATUS_CODES    , add_root_to_holder, NULL} ,
};
/* *INDENT-ON* */

static GPanel *
panel_lookup (GModule module)
{
  int i, num_panels = ARRAY_SIZE (paneling);

  for (i = 0; i < num_panels; i++) {
    if (paneling[i].module == module)
      return &paneling[i];
  }
  return NULL;
}

/* allocate memory for holder */
GHolder *
new_gholder (uint32_t size)
{
  GHolder *holder = xmalloc (size * sizeof (GHolder));
  memset (holder, 0, size * sizeof *holder);

  return holder;
}

/* allocate memory for holder items */
static GHolderItem *
new_gholder_item (uint32_t size)
{
  GHolderItem *item = xcalloc (size, sizeof (GHolderItem));

  return item;
}

/* allocate memory for a sub list */
static GSubList *
new_gsublist (void)
{
  GSubList *sub_list = xmalloc (sizeof (GSubList));
  sub_list->head = NULL;
  sub_list->tail = NULL;
  sub_list->size = 0;

  return sub_list;
}

/* allocate memory for a sub list item */
static GSubItem *
new_gsubitem (GModule module, GMetrics * nmetrics)
{
  GSubItem *sub_item = xmalloc (sizeof (GSubItem));

  sub_item->metrics = nmetrics;
  sub_item->module = module;
  sub_item->prev = NULL;
  sub_item->next = NULL;

  return sub_item;
}

/* add an item to the end of a given sub list */
static void
add_sub_item_back (GSubList * sub_list, GModule module, GMetrics * nmetrics)
{
  GSubItem *sub_item = new_gsubitem (module, nmetrics);
  if (sub_list->tail) {
    sub_list->tail->next = sub_item;
    sub_item->prev = sub_list->tail;
    sub_list->tail = sub_item;
  } else {
    sub_list->head = sub_item;
    sub_list->tail = sub_item;
  }
  sub_list->size++;
}

/* delete entire given sub list */
static void
delete_sub_list (GSubList * sub_list)
{
  GSubItem *item = NULL;
  GSubItem *next = NULL;

  if (sub_list != NULL && sub_list->size == 0)
    goto clear;
  if (sub_list->size == 0)
    return;

  for (item = sub_list->head; item; item = next) {
    next = item->next;
    free (item->metrics->data);
    free (item->metrics);
    free (item);
  }
clear:
  sub_list->head = NULL;
  sub_list->size = 0;
  free (sub_list);
}

/* dynamically allocated holder fields */
static void
free_holder_data (GHolderItem item)
{
  if (item.sub_list != NULL)
    delete_sub_list (item.sub_list);
  if (item.metrics->data != NULL)
    free (item.metrics->data);
  if (item.metrics->method != NULL)
    free (item.metrics->method);
  if (item.metrics->protocol != NULL)
    free (item.metrics->protocol);
  if (item.metrics != NULL)
    free (item.metrics);
}

/* free memory allocated in holder for specific module */
void
free_holder_by_module (GHolder ** holder, GModule module)
{
  int j;

  if ((*holder) == NULL)
    return;

  for (j = 0; j < (*holder)[module].idx; j++) {
    free_holder_data ((*holder)[module].items[j]);
  }
  free ((*holder)[module].items);

  (*holder)[module].holder_size = 0;
  (*holder)[module].idx = 0;
  (*holder)[module].sub_items_size = 0;
}

/* free memory allocated in holder */
void
free_holder (GHolder ** holder)
{
  GModule module;
  int j;

  if ((*holder) == NULL)
    return;

  for (module = 0; module < TOTAL_MODULES; module++) {
    for (j = 0; j < (*holder)[module].idx; j++) {
      free_holder_data ((*holder)[module].items[j]);
    }
    free ((*holder)[module].items);
  }
  free (*holder);
  (*holder) = NULL;
}

/* iterate over holder and get the key index.
 * return -1 if not found */
static int
get_item_idx_in_holder (GHolder * holder, const char *k)
{
  int i;
  if (holder == NULL)
    return KEY_NOT_FOUND;
  if (holder->idx == 0)
    return KEY_NOT_FOUND;
  if (k == NULL || *k == '\0')
    return KEY_NOT_FOUND;

  for (i = 0; i < holder->idx; i++) {
    if (strcmp (k, holder->items[i].metrics->data) == 0)
      return i;
  }

  return KEY_NOT_FOUND;
}

/* Copy linked-list items to an array, sort, and move them back
 * to the list. Should be faster than sorting the list */
static void
sort_sub_list (GHolder * h, GSort sort)
{
  GHolderItem *arr;
  GSubItem *iter;
  GSubList *sub_list;
  int i, j, k;

  /* iterate over root-level nodes */
  for (i = 0; i < h->idx; i++) {
    sub_list = h->items[i].sub_list;
    if (sub_list == NULL)
      continue;

    arr = new_gholder_item (sub_list->size);

    /* copy items from the linked-list into an array */
    for (j = 0, iter = sub_list->head; iter; iter = iter->next, j++) {
      arr[j].metrics = new_gmetrics ();

      arr[j].metrics->bw.nbw = iter->metrics->bw.nbw;
      arr[j].metrics->data = xstrdup (iter->metrics->data);
      arr[j].metrics->hits = iter->metrics->hits;
      arr[j].metrics->id = iter->metrics->id;
      arr[j].metrics->visitors = iter->metrics->visitors;
      if (conf.serve_usecs) {
        arr[j].metrics->avgts.nts = iter->metrics->avgts.nts;
        arr[j].metrics->cumts.nts = iter->metrics->cumts.nts;
        arr[j].metrics->maxts.nts = iter->metrics->maxts.nts;
      }
    }
    sort_holder_items (arr, j, sort);
    delete_sub_list (sub_list);

    sub_list = new_gsublist ();
    for (k = 0; k < j; k++) {
      if (k > 0)
        sub_list = h->items[i].sub_list;

      add_sub_item_back (sub_list, h->module, arr[k].metrics);
      h->items[i].sub_list = sub_list;
    }
    free (arr);
  }
}

static void
data_visitors (GHolder * h)
{
  char date[DATE_LEN] = "";     /* Ymd */
  char *datum = h->items[h->idx].metrics->data;

  memset (date, 0, sizeof *date);
  /* verify we have a valid date conversion */
  if (convert_date (date, datum, conf.date_format, "%Y%m%d", DATE_LEN) == 0) {
    free (datum);
    h->items[h->idx].metrics->data = xstrdup (date);
    return;
  }
  LOG_DEBUG (("invalid date: %s", datum));

  free (datum);
  h->items[h->idx].metrics->data = xstrdup ("---");
}

static int
set_host_child_metrics (char *data, uint8_t id, GMetrics ** nmetrics)
{
  GMetrics *metrics;

  metrics = new_gmetrics ();
  metrics->data = xstrdup (data);
  metrics->id = id;
  *nmetrics = metrics;

  return 0;
}

static void
set_host_sub_list (GHolder * h, GSubList * sub_list)
{
  GMetrics *nmetrics;
#ifdef HAVE_LIBGEOIP
  char city[CITY_LEN] = "";
  char continent[CONTINENT_LEN] = "";
  char country[COUNTRY_LEN] = "";
#endif

  char *host = h->items[h->idx].metrics->data, *hostname = NULL;
#ifdef HAVE_LIBGEOIP
  /* add geolocation child nodes */
  set_geolocation (host, continent, country, city);

  /* country */
  if (country[0] != '\0') {
    set_host_child_metrics (country, MTRC_ID_COUNTRY, &nmetrics);
    add_sub_item_back (sub_list, h->module, nmetrics);
    h->items[h->idx].sub_list = sub_list;
    h->sub_items_size++;
  }

  /* city */
  if (city[0] != '\0') {
    set_host_child_metrics (city, MTRC_ID_CITY, &nmetrics);
    add_sub_item_back (sub_list, h->module, nmetrics);
    h->items[h->idx].sub_list = sub_list;
    h->sub_items_size++;
  }
#endif

  /* hostname */
  if (conf.enable_html_resolver && conf.output_html) {
    hostname = reverse_ip (host);
    set_host_child_metrics (hostname, MTRC_ID_HOSTNAME, &nmetrics);
    add_sub_item_back (sub_list, h->module, nmetrics);
    h->items[h->idx].sub_list = sub_list;
    h->sub_items_size++;
    free (hostname);
  }
}

static void
add_host_child_to_holder (GHolder * h)
{
  GMetrics *nmetrics;
  GSubList *sub_list = new_gsublist ();

  char *ip = h->items[h->idx].metrics->data;
  char *hostname = NULL;
  int n = h->sub_items_size;

  /* add child nodes */
  set_host_sub_list (h, sub_list);

  pthread_mutex_lock (&gdns_thread.mutex);
  hostname = ht_get_hostname (ip);
  pthread_mutex_unlock (&gdns_thread.mutex);

  /* determine if we have the IP's hostname */
  if (!hostname) {
    dns_resolver (ip);
  } else if (hostname) {
    set_host_child_metrics (hostname, MTRC_ID_HOSTNAME, &nmetrics);
    add_sub_item_back (sub_list, h->module, nmetrics);
    h->items[h->idx].sub_list = sub_list;
    h->sub_items_size++;
    free (hostname);
  }

  /* did not add any items */
  if (n == h->sub_items_size)
    free (sub_list);
}

static void
add_host_to_holder (GRawDataItem item, GHolder * h, const GPanel * panel)
{
  add_data_to_holder (item, h, panel);
}

static void
add_data_to_holder (GRawDataItem item, GHolder * h, const GPanel * panel)
{
  char *data = NULL, *method = NULL, *protocol = NULL;
  int visitors = 0;
  uint64_t bw = 0, cumts = 0, maxts = 0;

  if (!(data = ht_get_datamap (h->module, item.key)))
    return;

  bw = ht_get_bw (h->module, item.key);
  cumts = ht_get_cumts (h->module, item.key);
  maxts = ht_get_maxts (h->module, item.key);
  visitors = ht_get_visitors (h->module, item.key);

  h->items[h->idx].metrics = new_gmetrics ();
  h->items[h->idx].metrics->hits = item.value;
  h->items[h->idx].metrics->visitors = visitors;
  h->items[h->idx].metrics->data = data;
  h->items[h->idx].metrics->bw.nbw = bw;
  h->items[h->idx].metrics->avgts.nts = cumts / item.value;
  h->items[h->idx].metrics->cumts.nts = cumts;
  h->items[h->idx].metrics->maxts.nts = maxts;

  if (conf.append_method) {
    method = ht_get_method (h->module, item.key);
    h->items[h->idx].metrics->method = method;
  }

  if (conf.append_protocol) {
    protocol = ht_get_protocol (h->module, item.key);
    h->items[h->idx].metrics->protocol = protocol;
  }

  if (panel->holder_callback)
    panel->holder_callback (h);

  h->idx++;
}

static int
set_root_metrics (int key, int hits, GModule module, GMetrics ** nmetrics)
{
  GMetrics *metrics;
  char *data = NULL;
  uint64_t bw = 0, cumts = 0, maxts = 0;
  int visitors = 0;

  if (!(data = ht_get_datamap (module, key)))
    return 1;

  bw = ht_get_bw (module, key);
  cumts = ht_get_cumts (module, key);
  maxts = ht_get_maxts (module, key);
  visitors = ht_get_visitors (module, key);

  metrics = new_gmetrics ();
  metrics->avgts.nts = cumts / hits;
  metrics->cumts.nts = cumts;
  metrics->maxts.nts = maxts;
  metrics->bw.nbw = bw;
  metrics->data = data;
  metrics->hits = hits;
  metrics->visitors = visitors;
  *nmetrics = metrics;

  return 0;
}

static void
add_root_to_holder (GRawDataItem item, GHolder * h,
                    GO_UNUSED const GPanel * panel)
{
  GSubList *sub_list;
  GMetrics *metrics, *nmetrics;
  char *root = NULL;
  int root_idx = KEY_NOT_FOUND, idx = 0;

  if (set_root_metrics (item.key, item.value, h->module, &nmetrics) == 1)
    return;

  if (!(root = (ht_get_root (h->module, item.key))))
    return;

  /* add data as a child node into holder */
  if (KEY_NOT_FOUND == (root_idx = get_item_idx_in_holder (h, root))) {
    idx = h->idx;
    sub_list = new_gsublist ();
    metrics = new_gmetrics ();

    h->items[idx].metrics = metrics;
    h->items[idx].metrics->data = root;
    h->idx++;
  } else {
    sub_list = h->items[root_idx].sub_list;
    metrics = h->items[root_idx].metrics;

    idx = root_idx;
    free (root);
  }

  add_sub_item_back (sub_list, h->module, nmetrics);
  h->items[idx].sub_list = sub_list;

  h->items[idx].metrics = metrics;
  h->items[idx].metrics->avgts.nts += nmetrics->avgts.nts;
  h->items[idx].metrics->cumts.nts += nmetrics->cumts.nts;
  h->items[idx].metrics->maxts.nts += nmetrics->maxts.nts;
  h->items[idx].metrics->bw.nbw += nmetrics->bw.nbw;
  h->items[idx].metrics->hits += nmetrics->hits;
  h->items[idx].metrics->visitors += nmetrics->visitors;

  h->sub_items_size++;
}

/* Load raw data into our holder structure */
void
load_holder_data (GRawData * raw_data, GHolder * h, GModule module, GSort sort)
{
  int i, size = 0;
  const GPanel *panel = panel_lookup (module);

  size = raw_data->size;
  h->holder_size = size > MAX_CHOICES ? MAX_CHOICES : size;
  h->ht_size = size;
  h->idx = 0;
  h->module = module;
  h->sub_items_size = 0;
  h->items = new_gholder_item (h->holder_size);

  for (i = 0; i < h->holder_size; i++) {
    panel->insert (raw_data->items[i], h, panel);
  }
  sort_holder_items (h->items, h->idx, sort);
  if (h->sub_items_size)
    sort_sub_list (h, sort);
  free_raw_data (raw_data);
}
