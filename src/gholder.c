/**
 * gholder.c -- data structure to hold raw data
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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "gholder.h"

#include "error.h"
#include "gdns.h"
#include "gkhash.h"
#include "gstorage.h"
#include "util.h"
#include "xmalloc.h"

#ifdef HAVE_GEOLOCATION
#include "geoip1.h"
#endif

typedef struct GPanel_ {
  GModule module;
  void (*insert) (GRawDataItem item, GHolder * h, datatype type, const struct GPanel_ *);
  void (*holder_callback) (GHolder * h);
} GPanel;


/* *INDENT-OFF* */
static void add_data_to_holder (GRawDataItem item, GHolder * h, datatype type, const GPanel * panel);
static void add_host_to_holder (GRawDataItem item, GHolder * h, datatype type, const GPanel * panel);
static void add_root_to_holder (GRawDataItem item, GHolder * h, datatype type, const GPanel * panel);
static void add_host_child_to_holder (GHolder * h);
#ifdef HAVE_GEOLOCATION
static void add_geo_to_holder (GRawDataItem item, GHolder * h, datatype type, const GPanel * panel);
#endif

static const GPanel paneling[] = {
  {VISITORS        , add_data_to_holder , NULL},
  {REQUESTS        , add_data_to_holder , NULL},
  {REQUESTS_STATIC , add_data_to_holder , NULL},
  {NOT_FOUND       , add_data_to_holder , NULL},
  {HOSTS           , add_host_to_holder , add_host_child_to_holder} ,
  {OS              , add_root_to_holder , NULL},
  {BROWSERS        , add_root_to_holder , NULL},
  {VISIT_TIMES     , add_data_to_holder , NULL},
  {VIRTUAL_HOSTS   , add_data_to_holder , NULL},
  {REFERRERS       , add_data_to_holder , NULL},
  {REFERRING_SITES , add_data_to_holder , NULL},
  {KEYPHRASES      , add_data_to_holder , NULL},
  {STATUS_CODES    , add_root_to_holder , NULL},
  {REMOTE_USER     , add_data_to_holder , NULL},
  {CACHE_STATUS    , add_data_to_holder , NULL},
#ifdef HAVE_GEOLOCATION
  {GEO_LOCATION    , add_geo_to_holder  , NULL},
  {ASN             , add_data_to_holder , NULL} ,
#endif
  {MIME_TYPE       , add_root_to_holder , NULL} ,
  {TLS_TYPE        , add_root_to_holder , NULL} ,
};
/* *INDENT-ON* */


/* Get a panel from the GPanel structure given a module.
 *
 * On error, or if not found, NULL is returned.
 * On success, the panel value is returned. */
static const GPanel *
panel_lookup (GModule module) {
  int i, num_panels = ARRAY_SIZE (paneling);

  for (i = 0; i < num_panels; i++) {
    if (paneling[i].module == module)
      return &paneling[i];
  }
  return NULL;
}

/* Allocate memory for a new GHolder instance.
 *
 * On success, the newly allocated GHolder is returned . */
GHolder *
new_gholder (uint32_t size) {
  GHolder *holder = xmalloc (size * sizeof (GHolder));
  memset (holder, 0, size * sizeof *holder);

  return holder;
}

/* Allocate memory for a new GHolderItem instance.
 *
 * On success, the newly allocated GHolderItem is returned . */
static GHolderItem *
new_gholder_item (uint32_t size) {
  GHolderItem *item = xcalloc (size, sizeof (GHolderItem));

  return item;
}

/* Allocate memory for a new double linked-list GSubList instance.
 *
 * On success, the newly allocated GSubList is returned . */
static GSubList *
new_gsublist (void) {
  GSubList *sub_list = xmalloc (sizeof (GSubList));
  sub_list->head = NULL;
  sub_list->tail = NULL;
  sub_list->size = 0;

  return sub_list;
}

/* Allocate memory for a new double linked-list GSubItem node.
 *
 * On success, the newly allocated GSubItem is returned . */
static GSubItem *
new_gsubitem (GModule module, GMetrics *nmetrics) {
  GSubItem *sub_item = xmalloc (sizeof (GSubItem));

  sub_item->metrics = nmetrics;
  sub_item->module = module;
  sub_item->sub_list = NULL;
  sub_item->prev = NULL;
  sub_item->next = NULL;

  return sub_item;
}

/* Add an item to the end of a given sub list. */
static void
add_sub_item_back (GSubList *sub_list, GModule module, GMetrics *nmetrics) {
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

/* Delete the entire given sub list. */
static void
delete_sub_list (GSubList *sub_list) {
  GSubItem *item = NULL;
  GSubItem *next = NULL;

  if (sub_list->size == 0)
    goto clear;

  for (item = sub_list->head; item; item = next) {
    next = item->next;
    if (item->sub_list != NULL)
      delete_sub_list (item->sub_list);
    free (item->metrics->data);
    free (item->metrics);
    free (item);
  }
clear:
  sub_list->head = NULL;
  sub_list->size = 0;
  free (sub_list);
}

/* Free malloc'd holder fields. */
static void
free_holder_data (GHolderItem item) {
  if (item.sub_list != NULL)
    delete_sub_list (item.sub_list);
  free_gmetrics (item.metrics);
}

/* Free all memory allocated in holder for a given module. */
void
free_holder_by_module (GHolder **holder, GModule module) {
  uint32_t j;

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

/* Free all memory allocated in holder for all modules. */
void
free_holder (GHolder **holder) {
  GModule module;
  uint32_t j;
  size_t idx = 0;

  if ((*holder) == NULL)
    return;

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    for (j = 0; j < (*holder)[module].idx; j++) {
      free_holder_data ((*holder)[module].items[j]);
    }
    free ((*holder)[module].items);
  }
  free (*holder);
  (*holder) = NULL;
}

/* Iterate over holder and get the key index.
 *
 * If the key does not exist, -1 is returned.
 * On success, the key in holder is returned . */
static int
get_item_idx_in_holder (GHolder *holder, const char *k) {
  uint32_t i;
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

/* Sort a single GSubList in place: copy to array, sort, rebuild list.
 * Preserves each sub-item's own sub_list for recursive nesting. */
static void
sort_single_sub_list (GSubList **sl_ptr, GModule module, GSort sort, int max_choices_sub) {
  GHolderItem *arr;
  GSubItem *iter;
  GSubList *sub_list = *sl_ptr;
  int j, k, limit;

  if (sub_list == NULL || sub_list->size == 0)
    return;

  arr = new_gholder_item (sub_list->size);

  /* copy items from the linked-list into an array, preserving sub_list */
  for (j = 0, iter = sub_list->head; iter; iter = iter->next, j++) {
    arr[j].metrics = new_gmetrics ();
    arr[j].metrics->nbw = iter->metrics->nbw;
    arr[j].metrics->data = xstrdup (iter->metrics->data);
    arr[j].metrics->hits = iter->metrics->hits;
    arr[j].metrics->id = iter->metrics->id;
    arr[j].metrics->visitors = iter->metrics->visitors;
    arr[j].sub_list = iter->sub_list;
    iter->sub_list = NULL; /* prevent double-free */
    if (conf.serve_usecs) {
      arr[j].metrics->avgts.nts = iter->metrics->avgts.nts;
      arr[j].metrics->cumts.nts = iter->metrics->cumts.nts;
      arr[j].metrics->maxts.nts = iter->metrics->maxts.nts;
    }
  }
  sort_holder_items (arr, j, sort);
  delete_sub_list (sub_list);

  /* Only rebuild up to max_choices_sub items */
  limit = (j < max_choices_sub) ? j : max_choices_sub;
  sub_list = new_gsublist ();
  for (k = 0; k < limit; k++) {
    if (k > 0)
      sub_list = *sl_ptr;

    add_sub_item_back (sub_list, module, arr[k].metrics);
    sub_list->tail->sub_list = arr[k].sub_list;
    *sl_ptr = sub_list;
    sub_list = NULL;
  }

  /* Free items beyond max_choices_sub */
  for (k = limit; k < j; k++) {
    if (arr[k].sub_list != NULL)
      delete_sub_list (arr[k].sub_list);
    free (arr[k].metrics->data);
    free (arr[k].metrics);
  }

  free (arr);
  if (sub_list) {
    delete_sub_list (sub_list);
    sub_list = NULL;
  }

  /* recursively sort nested sub-lists */
  sub_list = *sl_ptr;
  if (sub_list != NULL) {
    for (iter = sub_list->head; iter; iter = iter->next) {
      if (iter->sub_list != NULL)
        sort_single_sub_list (&iter->sub_list, module, sort, max_choices_sub);
    }
  }
}

/* Copy linked-list items to an array, sort, and move them back to the
 * list. Should be faster than sorting the list */
static void
sort_sub_list (GHolder *h, GSort sort) {
  uint32_t i;

  /* iterate over root-level nodes */
  for (i = 0; i < h->idx; i++) {
    sort_single_sub_list (&h->items[i].sub_list, h->module, sort, h->max_choices_sub);
  }
}

/* Set the data metric field for the host panel.
 *
 * On success, the data field/metric is set. */
static int
set_host_child_metrics (char *data, uint8_t id, GMetrics **nmetrics) {
  GMetrics *metrics;

  metrics = new_gmetrics ();
  metrics->data = xstrdup (data);
  metrics->id = id;
  *nmetrics = metrics;

  return 0;
}

/* Set host panel data, including sub items.
 *
 * On success, the host panel data is set. */
static void
set_host_sub_list (GHolder *h, GSubList *sub_list) {
  GMetrics *nmetrics;
#ifdef HAVE_GEOLOCATION
  char city[CITY_LEN] = "";
  char continent[CONTINENT_LEN] = "";
  char country[ASN_LEN] = "";
  char asn[ASN_LEN] = "";
#endif

  char *host = h->items[h->idx].metrics->data, *hostname = NULL;
#ifdef HAVE_GEOLOCATION
  /* add geolocation child nodes */
  set_geolocation (host, continent, country, city, asn);

  /* country */
  if (country[0] != '\0' && sub_list->size < h->max_choices_sub) {
    set_host_child_metrics (country, MTRC_ID_COUNTRY, &nmetrics);
    add_sub_item_back (sub_list, h->module, nmetrics);
    h->items[h->idx].sub_list = sub_list;
    h->sub_items_size++;

    /* flag only */
    conf.has_geocountry = 1;
  }

  /* city */
  if (city[0] != '\0' && sub_list->size < h->max_choices_sub) {
    set_host_child_metrics (city, MTRC_ID_CITY, &nmetrics);
    add_sub_item_back (sub_list, h->module, nmetrics);
    h->items[h->idx].sub_list = sub_list;
    h->sub_items_size++;

    /* flag only */
    conf.has_geocity = 1;
  }

  /* ASN */
  if (asn[0] != '\0' && sub_list->size < h->max_choices_sub) {
    set_host_child_metrics (asn, MTRC_ID_ASN, &nmetrics);
    add_sub_item_back (sub_list, h->module, nmetrics);
    h->items[h->idx].sub_list = sub_list;
    h->sub_items_size++;

    /* flag only */
    conf.has_geoasn = 1;
  }
#endif

  /* hostname */
  if (conf.enable_html_resolver && conf.output_stdout && !conf.no_ip_validation &&
      !conf.real_time_html && sub_list->size < h->max_choices_sub) {
    hostname = reverse_ip (host);
    set_host_child_metrics (hostname, MTRC_ID_HOSTNAME, &nmetrics);
    add_sub_item_back (sub_list, h->module, nmetrics);
    h->items[h->idx].sub_list = sub_list;
    h->sub_items_size++;
    free (hostname);
  }
}

/* Set host panel data, including sub items.
 *
 * On success, the host panel data is set. */
static void
add_host_child_to_holder (GHolder *h) {
  GMetrics *nmetrics;
  GSubList *sub_list = new_gsublist ();

  char *ip = h->items[h->idx].metrics->data;
  char *hostname = NULL;
  uint32_t n = h->sub_items_size;

  /* add child nodes */
  set_host_sub_list (h, sub_list);

  pthread_mutex_lock (&gdns_thread.mutex);
  hostname = ht_get_hostname (ip);
  pthread_mutex_unlock (&gdns_thread.mutex);

  /* determine if we have the IP's hostname */
  if (!hostname) {
    dns_resolver (ip);
  } else if (hostname && sub_list->size < h->max_choices_sub) {
    set_host_child_metrics (hostname, MTRC_ID_HOSTNAME, &nmetrics);
    add_sub_item_back (sub_list, h->module, nmetrics);
    h->items[h->idx].sub_list = sub_list;
    h->sub_items_size++;
    free (hostname);
  } else if (hostname) {
    free (hostname);
  }

  /* did not add any items */
  if (n == h->sub_items_size)
    free (sub_list);
}

/* Given a GRawDataType, set the data and hits value.
 *
 * On error, no values are set and 1 is returned.
 * On success, the data and hits values are set and 0 is returned. */
static int
map_data (GModule module, GRawDataItem item, datatype type, char **data, uint32_t *hits) {
  switch (type) {
  case U32:
    if (!(*data = ht_get_datamap (module, item.nkey)))
      return 1;
    if (!item.hits)
      return 1;
    *hits = item.hits;
    break;
  case STR:
    if (!(*hits = ht_get_hits (module, item.nkey)))
      return 1;
    *data = xstrdup (item.data);
    break;
  }
  return 0;
}

/* Given a data item, store it into a holder structure. */
static void
set_single_metrics (GRawDataItem item, GHolder *h, char *data, uint32_t hits) {
  uint32_t visitors = 0;
  uint64_t bw = 0, cumts = 0, maxts = 0;

  bw = ht_get_bw (h->module, item.nkey);
  cumts = ht_get_cumts (h->module, item.nkey);
  maxts = ht_get_maxts (h->module, item.nkey);
  visitors = ht_get_visitors (h->module, item.nkey);

  h->items[h->idx].metrics = new_gmetrics ();
  h->items[h->idx].metrics->hits = hits;
  h->items[h->idx].metrics->data = data;
  h->items[h->idx].metrics->visitors = visitors;
  h->items[h->idx].metrics->nbw = bw;
  h->items[h->idx].metrics->avgts.nts = cumts / hits;
  h->items[h->idx].metrics->cumts.nts = cumts;
  h->items[h->idx].metrics->maxts.nts = maxts;

  if (bw && !conf.bandwidth)
    conf.bandwidth = 1;
  if (cumts && !conf.serve_usecs)
    conf.serve_usecs = 1;

  if (conf.append_method) {
    h->items[h->idx].metrics->method = ht_get_method (h->module, item.nkey);
  }

  if (conf.append_protocol) {
    h->items[h->idx].metrics->protocol = ht_get_protocol (h->module, item.nkey);
  }
}

/* Set all panel data. This will set data for panels that do not
 * contain sub items. A function pointer is used for post data set. */
static void
add_data_to_holder (GRawDataItem item, GHolder *h, datatype type, const GPanel *panel) {
  char *data = NULL;
  uint32_t hits = 0;

  if (map_data (h->module, item, type, &data, &hits) == 1)
    return;

  set_single_metrics (item, h, data, hits);
  if (panel->holder_callback)
    panel->holder_callback (h);

  h->idx++;
}

/* A wrapper to set a host item */
static void
set_host (GRawDataItem item, GHolder *h, const GPanel *panel, char *data, uint32_t hits) {
  set_single_metrics (item, h, xstrdup (data), hits);
  if (panel->holder_callback)
    panel->holder_callback (h);
  h->idx++;
}

/* Set all panel data. This will set data for panels that do not
 * contain sub items. A function pointer is used for post data set. */
static void
add_host_to_holder (GRawDataItem item, GHolder *h, datatype type, const GPanel *panel) {
  char buf4[INET_ADDRSTRLEN];
  char buf6[INET6_ADDRSTRLEN];
  char *data = NULL;
  uint32_t hits = 0;
  unsigned i;

  struct in6_addr addr6, mask6, nwork6;
  struct in_addr addr4, mask4, nwork4;

  const char *arr4[] = { "255.255.255.0", "255.255.0.0", "255.0.0.0" };
  const char *arr6[] = {
    "ffff:ffff:ffff:ffff:0000:0000:0000:0000",
    "ffff:ffff:ffff:0000:0000:0000:0000:0000",
    "ffff:ffff:0000:0000:0000:0000:0000:0000"
  };

  const char *m4 = arr4[0];
  const char *m6 = arr6[0];

  if (map_data (h->module, item, type, &data, &hits) == 1)
    return;

  if (!conf.anonymize_ip) {
    add_data_to_holder (item, h, type, panel);
    free (data);
    return;
  }

  if (conf.anonymize_level == ANONYMIZE_STRONG || conf.anonymize_level == ANONYMIZE_PEDANTIC) {
    m4 = arr4[conf.anonymize_level - 1];
    m6 = arr6[conf.anonymize_level - 1];
  }

  if (1 == inet_pton (AF_INET, data, &addr4)) {
    if (1 == inet_pton (AF_INET, m4, &mask4)) {
      memset (buf4, 0, sizeof *buf4);
      nwork4.s_addr = addr4.s_addr & mask4.s_addr;

      if (inet_ntop (AF_INET, &nwork4, buf4, INET_ADDRSTRLEN) != NULL) {
        set_host (item, h, panel, buf4, hits);
        free (data);
      }
    }
  } else if (1 == inet_pton (AF_INET6, data, &addr6)) {
    if (1 == inet_pton (AF_INET6, m6, &mask6)) {
      memset (buf6, 0, sizeof *buf6);
      for (i = 0; i < 16; i++) {
        nwork6.s6_addr[i] = addr6.s6_addr[i] & mask6.s6_addr[i];
      }

      if (inet_ntop (AF_INET6, &nwork6, buf6, INET6_ADDRSTRLEN) != NULL) {
        set_host (item, h, panel, buf6, hits);
        free (data);
      }
    }
  }
}

/* Set all root panel data. This will set the root nodes. */
static int
set_root_metrics (GRawDataItem item, GModule module, datatype type, GMetrics **nmetrics) {
  GMetrics *metrics;
  char *data = NULL;
  uint64_t bw = 0, cumts = 0, maxts = 0;
  uint32_t hits = 0, visitors = 0;

  if (map_data (module, item, type, &data, &hits) == 1)
    return 1;

  bw = ht_get_bw (module, item.nkey);
  cumts = ht_get_cumts (module, item.nkey);
  maxts = ht_get_maxts (module, item.nkey);
  visitors = ht_get_visitors (module, item.nkey);

  metrics = new_gmetrics ();
  metrics->avgts.nts = cumts / hits;
  metrics->cumts.nts = cumts;
  metrics->maxts.nts = maxts;
  metrics->nbw = bw;
  metrics->data = data;
  metrics->hits = hits;
  metrics->visitors = visitors;
  *nmetrics = metrics;

  return 0;
}

/* Set all root panel data, including sub list items. */
static void
add_root_to_holder (GRawDataItem item, GHolder *h, datatype type, GO_UNUSED const GPanel *panel) {
  GSubList *sub_list;
  GMetrics *metrics, *nmetrics;
  char *root = NULL;
  int root_idx = KEY_NOT_FOUND, idx = 0;

  if (set_root_metrics (item, h->module, type, &nmetrics) == 1)
    return;

  if (!(root = ht_get_root (h->module, item.nkey))) {
    free_gmetrics (nmetrics);
    return;
  }

  /* add data as a child node into holder */
  if (KEY_NOT_FOUND == (root_idx = get_item_idx_in_holder (h, root))) {
    /* Check if we've reached max_choices for root items */
    if (h->idx >= h->max_choices) {
      free (root);
      free_gmetrics (nmetrics);
      return;
    }
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

  /* Accumulate metrics into parent regardless of whether we add the sub-item */
  h->items[idx].metrics = metrics;
  h->items[idx].metrics->cumts.nts += nmetrics->cumts.nts;
  h->items[idx].metrics->nbw += nmetrics->nbw;
  h->items[idx].metrics->hits += nmetrics->hits;
  h->items[idx].metrics->visitors += nmetrics->visitors;
  h->items[idx].metrics->avgts.nts = h->items[idx].metrics->cumts.nts / h->items[idx].metrics->hits;

  if (nmetrics->maxts.nts > h->items[idx].metrics->maxts.nts)
    h->items[idx].metrics->maxts.nts = nmetrics->maxts.nts;

  /* Only add sub-item if we haven't reached max_choices_sub for this sub-list */
  if (sub_list->size < h->max_choices_sub) {
    add_sub_item_back (sub_list, h->module, nmetrics);
    h->items[idx].sub_list = sub_list;
    h->sub_items_size++;
  } else {
    free_gmetrics (nmetrics);
  }
}

#ifdef HAVE_GEOLOCATION
/* Find a sub-item in a GSubList by its data string.
 * Returns the matching GSubItem, or NULL if not found. */
static GSubItem *
find_sub_item_by_data (GSubList *sl, const char *data) {
  GSubItem *iter;
  if (sl == NULL || data == NULL)
    return NULL;
  for (iter = sl->head; iter; iter = iter->next) {
    if (strcmp (iter->metrics->data, data) == 0)
      return iter;
  }
  return NULL;
}

/* Build 3-level GEO_LOCATION hierarchy: Continent > Country > City.
 * Falls back to add_root_to_holder (2-level) when has_geocity is false. */
static void
add_geo_to_holder (GRawDataItem item, GHolder *h, datatype type, GO_UNUSED const GPanel *panel) {
  GMetrics *nmetrics;
  GSubList *sub_list;
  GSubItem *country_sub;
  GMetrics *cont_metrics, *country_metrics;
  char *root = NULL;
  const char *continent = NULL;
  int root_idx = KEY_NOT_FOUND, idx = 0;

  if (!conf.has_geocity) {
    add_root_to_holder (item, h, type, panel);
    return;
  }

  /* city metrics from storage (city is the "data" key) */
  if (set_root_metrics (item, h->module, type, &nmetrics) == 1)
    return;

  /* country is the "root" of the city in storage */
  if (!(root = ht_get_root (h->module, item.nkey))) {
    free_gmetrics (nmetrics);
    return;
  }

  /* look up the continent for this country */
  continent = get_continent_for_country (root);
  if (continent == NULL) {
    /* fallback: use country as root directly (2-level) */
    free (root);
    add_root_to_holder (item, h, type, panel);
    free_gmetrics (nmetrics);
    return;
  }

  /* Find or create continent as root GHolderItem */
  if (KEY_NOT_FOUND == (root_idx = get_item_idx_in_holder (h, continent))) {
    /* Check if we've reached max_choices for root items (continents) */
    if (h->idx >= h->max_choices) {
      free (root);
      free_gmetrics (nmetrics);
      return;
    }
    idx = h->idx;
    cont_metrics = new_gmetrics ();
    cont_metrics->data = xstrdup (continent);
    h->items[idx].metrics = cont_metrics;
    h->items[idx].sub_list = new_gsublist ();
    h->idx++;
  } else {
    idx = root_idx;
    cont_metrics = h->items[idx].metrics;
  }

  /* Find or create country as sub-item under continent */
  sub_list = h->items[idx].sub_list;
  country_sub = find_sub_item_by_data (sub_list, root);
  if (country_sub == NULL) {
    /* Only add country if continent's sub-list hasn't reached max_choices_sub */
    if (sub_list->size < h->max_choices_sub) {
      country_metrics = new_gmetrics ();
      country_metrics->data = xstrdup (root);
      add_sub_item_back (sub_list, h->module, country_metrics);
      country_sub = sub_list->tail;
      country_sub->sub_list = new_gsublist ();
      h->sub_items_size++;
    } else {
      /* Continent sub-list is full, accumulate to continent and skip country/city */
      cont_metrics->hits += nmetrics->hits;
      cont_metrics->visitors += nmetrics->visitors;
      cont_metrics->nbw += nmetrics->nbw;
      cont_metrics->cumts.nts += nmetrics->cumts.nts;
      if (nmetrics->maxts.nts > cont_metrics->maxts.nts)
        cont_metrics->maxts.nts = nmetrics->maxts.nts;
      if (cont_metrics->hits > 0)
        cont_metrics->avgts.nts = cont_metrics->cumts.nts / cont_metrics->hits;
      free_gmetrics (nmetrics);
      free (root);
      return;
    }
  } else {
    country_metrics = country_sub->metrics;
    /* Handle mixed 2-level (persisted) and 3-level (live) data:
     * older entries won't have a city sub-list. */
    if (country_sub->sub_list == NULL)
      country_sub->sub_list = new_gsublist ();
  }

  /* Accumulate metrics upward: city -> country -> continent */
  country_metrics->hits += nmetrics->hits;
  country_metrics->visitors += nmetrics->visitors;
  country_metrics->nbw += nmetrics->nbw;
  country_metrics->cumts.nts += nmetrics->cumts.nts;
  if (nmetrics->maxts.nts > country_metrics->maxts.nts)
    country_metrics->maxts.nts = nmetrics->maxts.nts;
  if (country_metrics->hits > 0)
    country_metrics->avgts.nts = country_metrics->cumts.nts / country_metrics->hits;

  cont_metrics->hits += nmetrics->hits;
  cont_metrics->visitors += nmetrics->visitors;
  cont_metrics->nbw += nmetrics->nbw;
  cont_metrics->cumts.nts += nmetrics->cumts.nts;
  if (nmetrics->maxts.nts > cont_metrics->maxts.nts)
    cont_metrics->maxts.nts = nmetrics->maxts.nts;
  if (cont_metrics->hits > 0)
    cont_metrics->avgts.nts = cont_metrics->cumts.nts / cont_metrics->hits;

  /* Add city as sub-item under country (only if country's sub-list hasn't reached max_choices_sub) */
  if (country_sub->sub_list->size < h->max_choices_sub) {
    add_sub_item_back (country_sub->sub_list, h->module, nmetrics);
    h->sub_items_size++;
  } else {
    free_gmetrics (nmetrics);
  }

  free (root);
}
#endif

/* Load raw data into our holder structure */
void
load_holder_data (GRawData *raw_data, GHolder *h, GModule module, GSort sort, uint32_t max_choices, uint32_t max_choices_sub) {
  uint32_t i;
  uint32_t size = 0;
  uint32_t alloc_size = 0;
  const GPanel *panel = panel_lookup (module);
  /* Hierarchical panels group multiple raw items under fewer root items */
  int is_hierarchical = (panel->insert == add_root_to_holder ||
#ifdef HAVE_GEOLOCATION
                         panel->insert == add_geo_to_holder ||
#endif
                         0);

#ifdef _DEBUG
  clock_t begin = clock ();
  double taken;
  const char *modstr = NULL;
  LOG_DEBUG (("== load_holder_data ==\n"));
#endif

  size = raw_data->size;
  /* For hierarchical data, we don't know how many root items we'll create,
   * but it can't be more than size or max_choices */
  alloc_size = size > max_choices ? max_choices : size;
  h->holder_size = is_hierarchical ? size : (size > max_choices ? max_choices : size);
  h->ht_size = size;
  h->idx = 0;
  h->module = module;
  h->sub_items_size = 0;
  h->max_choices = max_choices;
  h->max_choices_sub = max_choices_sub;
  h->items = new_gholder_item (alloc_size);

  for (i = 0; i < h->holder_size; i++) {
    panel->insert (raw_data->items[i], h, raw_data->type, panel);
  }
  sort_holder_items (h->items, h->idx, sort);
  if (h->sub_items_size)
    sort_sub_list (h, sort);
  free_raw_data (raw_data);

#ifdef _DEBUG
  modstr = get_module_str (module);
  taken = (double) (clock () - begin) / CLOCKS_PER_SEC;
  LOG_DEBUG (("== %-30s%f\n\n", modstr, taken));
#endif
}
