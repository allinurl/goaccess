/**
 * sort.c -- functions related to sort functionality
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "settings.h"
#include "util.h"

#include "sort.h"

/* *INDENT-OFF* */
const int sort_choices[][SORT_MAX_OPTS] = {
  /* VISITORS */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, -1},
  /* REQUESTS */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, SORT_BY_PROT, SORT_BY_MTHD, -1},
  /* REQUESTS_STATIC */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, SORT_BY_PROT, SORT_BY_MTHD, -1},
  /* NOT_FOUND */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, SORT_BY_PROT, SORT_BY_MTHD, -1},
  /* HOSTS */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, -1},
  /* OS */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, -1},
  /* BROWSERS */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, -1},
  /* VISIT_TIMES */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, -1},
  /* VIRTUAL_HOSTS */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, -1},
  /* REFERRERS */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, -1},
  /* REFERRING_SITES */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, -1},
  /* KEYPHRASES */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, -1},
  /* STATUS_CODES */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, -1},
  /* REMOTE_USER */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, -1},
  /* CACHE_STATUS */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, -1},
#ifdef HAVE_GEOLOCATION
  /* GEO_LOCATION */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, -1},
  /* ASN */
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, -1},
#endif
  /* MIME_TYPE */
  {SORT_BY_HITS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_AVGTS, SORT_BY_CUMTS, SORT_BY_MAXTS, -1},
  /* TLS_TYPE */
  {SORT_BY_HITS, SORT_BY_DATA, SORT_BY_VISITORS, SORT_BY_BW, -1},
};

static const GEnum FIELD[] = {
  {"BY_HITS"     , SORT_BY_HITS     } ,
  {"BY_VISITORS" , SORT_BY_VISITORS } ,
  {"BY_DATA"     , SORT_BY_DATA     } ,
  {"BY_BW"       , SORT_BY_BW       } ,
  {"BY_AVGTS"    , SORT_BY_AVGTS    } ,
  {"BY_CUMTS"    , SORT_BY_CUMTS    } ,
  {"BY_MAXTS"    , SORT_BY_MAXTS    } ,
  {"BY_PROT"     , SORT_BY_PROT     } ,
  {"BY_MTHD"     , SORT_BY_MTHD     } ,
};

static const GEnum ORDER[] = {
  {"ASC"  , SORT_ASC  } ,
  {"DESC" , SORT_DESC } ,
};

GSort module_sort[TOTAL_MODULES] = {
  {VISITORS            , SORT_BY_DATA , SORT_DESC } ,
  {REQUESTS            , SORT_BY_HITS , SORT_DESC } ,
  {REQUESTS_STATIC     , SORT_BY_HITS , SORT_DESC } ,
  {NOT_FOUND           , SORT_BY_HITS , SORT_DESC } ,
  {HOSTS               , SORT_BY_HITS , SORT_DESC } ,
  {OS                  , SORT_BY_HITS , SORT_DESC } ,
  {BROWSERS            , SORT_BY_HITS , SORT_DESC } ,
  {VISIT_TIMES         , SORT_BY_DATA , SORT_ASC  } ,
  {VIRTUAL_HOSTS       , SORT_BY_HITS , SORT_DESC } ,
  {REFERRERS           , SORT_BY_HITS , SORT_DESC } ,
  {REFERRING_SITES     , SORT_BY_HITS , SORT_DESC } ,
  {KEYPHRASES          , SORT_BY_HITS , SORT_DESC } ,
  {STATUS_CODES        , SORT_BY_HITS , SORT_DESC } ,
  {REMOTE_USER         , SORT_BY_HITS , SORT_DESC } ,
  {CACHE_STATUS        , SORT_BY_HITS , SORT_DESC } ,
#ifdef HAVE_GEOLOCATION
  {GEO_LOCATION        , SORT_BY_HITS , SORT_DESC } ,
  {ASN                 , SORT_BY_HITS , SORT_DESC } ,
#endif
  {MIME_TYPE           , SORT_BY_HITS , SORT_DESC } ,
  {TLS_TYPE            , SORT_BY_VISITORS , SORT_DESC } ,
};
/* *INDENT-ON* */

/* Sort an array of strings ascending */
int
strcmp_asc (const void *a, const void *b) {
  return strcmp (*((char *const *) a), *((char *const *) b));
}

/* Sort 'data' metric ascending */
static int
cmp_data_asc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ia->metrics->data, ib->metrics->data);
}

/* Sort 'data' metric descending */
static int
cmp_data_desc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ib->metrics->data, ia->metrics->data);
}

/* Sort 'hits' metric descending */
static int
cmp_num_desc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  uint64_t va = ia->metrics->hits;
  uint64_t vb = ib->metrics->hits;

  return (va < vb) - (va > vb);
}

/* Sort 'hits' metric ascending */
static int
cmp_num_asc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  uint64_t va = ia->metrics->hits;
  uint64_t vb = ib->metrics->hits;

  return (va > vb) - (va < vb);
}

/* Sort 'visitors' metric descending */
static int
cmp_vis_desc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  uint64_t va = ia->metrics->visitors;
  uint64_t vb = ib->metrics->visitors;

  return (va < vb) - (va > vb);
}

/* Sort 'visitors' metric ascending */
static int
cmp_vis_asc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  uint64_t va = ia->metrics->visitors;
  uint64_t vb = ib->metrics->visitors;

  return (va > vb) - (va < vb);
}

/* Sort GRawDataItem value descending */
static int
cmp_raw_num_desc (const void *a, const void *b) {
  const GRawDataItem *ia = a;
  const GRawDataItem *ib = b;

  uint64_t va = ia->hits;
  uint64_t vb = ib->hits;

  return (va < vb) - (va > vb);
}

/* Sort GRawDataItem value descending */
static int
cmp_raw_str_desc (const void *a, const void *b) {
  const GRawDataItem *ia = a;
  const GRawDataItem *ib = b;

  return strcmp (ib->data, ia->data);
}

/* Sort 'bandwidth' metric descending */
static int
cmp_bw_desc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  uint64_t va = ia->metrics->nbw;
  uint64_t vb = ib->metrics->nbw;

  return (va < vb) - (va > vb);
}

/* Sort 'bandwidth' metric ascending */
static int
cmp_bw_asc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  uint64_t va = ia->metrics->nbw;
  uint64_t vb = ib->metrics->nbw;

  return (va > vb) - (va < vb);
}

/* Sort 'avgts' metric descending */
static int
cmp_avgts_desc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  uint64_t va = ia->metrics->avgts.nts;
  uint64_t vb = ib->metrics->avgts.nts;

  return (va < vb) - (va > vb);
}

/* Sort 'avgts' metric ascending */
static int
cmp_avgts_asc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  uint64_t va = ia->metrics->avgts.nts;
  uint64_t vb = ib->metrics->avgts.nts;

  return (va > vb) - (va < vb);
}

/* Sort 'cumts' metric descending */
static int
cmp_cumts_desc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  uint64_t va = ia->metrics->cumts.nts;
  uint64_t vb = ib->metrics->cumts.nts;

  return (va < vb) - (va > vb);
}

/* Sort 'cumts' metric ascending */
static int
cmp_cumts_asc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  uint64_t va = ia->metrics->cumts.nts;
  uint64_t vb = ib->metrics->cumts.nts;

  return (va > vb) - (va < vb);
}

/* Sort 'maxts' metric descending */
static int
cmp_maxts_desc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  uint64_t va = ia->metrics->maxts.nts;
  uint64_t vb = ib->metrics->maxts.nts;

  return (va < vb) - (va > vb);
}

/* Sort 'maxts' metric ascending */
static int
cmp_maxts_asc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  uint64_t va = ia->metrics->maxts.nts;
  uint64_t vb = ib->metrics->maxts.nts;

  return (va > vb) - (va < vb);
}

/* Sort 'protocol' metric ascending */
static int
cmp_proto_asc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ia->metrics->protocol, ib->metrics->protocol);
}

/* Sort 'protocol' metric descending */
static int
cmp_proto_desc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ib->metrics->protocol, ia->metrics->protocol);
}

/* Sort 'method' metric ascending */
static int
cmp_mthd_asc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ia->metrics->method, ib->metrics->method);
}

/* Sort 'method' metric descending */
static int
cmp_mthd_desc (const void *a, const void *b) {
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ib->metrics->method, ia->metrics->method);
}

/* Sort ascending */
#if defined(__clang__) && defined(__clang_major__) && (__clang_major__ >= 4)
__attribute__((no_sanitize ("implicit-conversion", "unsigned-integer-overflow")))
#endif
int
cmp_ui32_asc (const void *a, const void *b) {
  const uint32_t *ia = (const uint32_t *) a; // casting pointer types
  const uint32_t *ib = (const uint32_t *) b;
  return *ia - *ib;
}

int
cmp_ui32_desc (const void *a, const void *b) {
  const uint32_t *ia = (const uint32_t *) a; // casting pointer types
  const uint32_t *ib = (const uint32_t *) b;
  return *ib - *ia;
}

/* Given a string sort field, get the enum field value.
 *
 * On error, -1 is returned.
 * On success, the enumerated field value is returned. */
int
get_sort_field_enum (const char *str) {
  return str2enum (FIELD, ARRAY_SIZE (FIELD), str);
}

/* Given a string sort order, get the enum order value.
 *
 * On error, -1 is returned.
 * On success, the enumerated order value is returned. */
int
get_sort_order_enum (const char *str) {
  return str2enum (ORDER, ARRAY_SIZE (ORDER), str);
}

/* Given a GSortOrder enum value, return the corresponding string.
 *
 * The string corresponding to the enumerated order value is returned. */
const char *
get_sort_order_str (GSortOrder order) {
  return ORDER[order].str;
}

/* Given a GSortField enum value, return the corresponding string.
 *
 * The string corresponding to the enumerated field value is returned. */
const char *
get_sort_field_str (GSortField field) {
  return FIELD[field].str;
}

/* Given a GSortField enum value, return the corresponding key.
 *
 * The key corresponding to the enumerated field value is returned. */
const char *
get_sort_field_key (GSortField field) {
  static const char *const field2key[][2] = {
    {"BY_HITS", "hits"},
    {"BY_VISITORS", "visitors"},
    {"BY_DATA", "data"},
    {"BY_BW", "bytes"},
    {"BY_AVGTS", "avgts"},
    {"BY_CUMTS", "cumts"},
    {"BY_MAXTS", "maxts"},
    {"BY_PROT", "protocol"},
    {"BY_MTHD", "method"},
  };

  return field2key[field][1];
}

/* Set the initial metric sort per module/panel.
 *
 * On error, function returns.
 * On success, panel metrics are sorted. */
void
set_initial_sort (const char *smod, const char *sfield, const char *ssort) {
  int module, field, order;
  if ((module = get_module_enum (smod)) == -1)
    return;

  if ((field = get_sort_field_enum (sfield)) == -1)
    return;
  if ((order = get_sort_order_enum (ssort)) == -1)
    return;
  if (!can_sort_module (module, field))
    return;

  module_sort[module].field = field;
  module_sort[module].sort = order;
}

/* Determine if module/panel metric can be sorted.
 *
 * On error or if metric can't be sorted, 0 is returned.
 * On success, 1 is returned. */
int
can_sort_module (GModule module, int field) {
  int i, can_sort = 0;
  for (i = 0; -1 != sort_choices[module][i]; i++) {
    if (sort_choices[module][i] != field)
      continue;
    if (SORT_BY_AVGTS == field && !conf.serve_usecs)
      continue;
    if (SORT_BY_CUMTS == field && !conf.serve_usecs)
      continue;
    if (SORT_BY_MAXTS == field && !conf.serve_usecs)
      continue;
    else if (SORT_BY_BW == field && !conf.bandwidth)
      continue;
    else if (SORT_BY_PROT == field && !conf.append_protocol)
      continue;
    else if (SORT_BY_MTHD == field && !conf.append_method)
      continue;

    can_sort = 1;
    break;
  }

  return can_sort;
}

/* Parse all initial sort options from the config file.
 *
 * On error, function returns.
 * On success, panel metrics are sorted. */
void
parse_initial_sort (void) {
  int i;
  char module[SORT_MODULE_LEN], field[SORT_FIELD_LEN], order[SORT_ORDER_LEN];
  for (i = 0; i < conf.sort_panel_idx; ++i) {
    if (sscanf (conf.sort_panels[i], "%15[^','],%11[^','],%4s", module, field, order) != 3)
      continue;
    set_initial_sort (module, field, order);
  }
}

/* Apply user defined sort */
void
sort_holder_items (GHolderItem *items, int size, GSort sort) {
  switch (sort.field) {
  case SORT_BY_HITS:
    if (sort.sort == SORT_DESC)
      qsort (items, size, sizeof (GHolderItem), cmp_num_desc);
    else
      qsort (items, size, sizeof (GHolderItem), cmp_num_asc);
    break;
  case SORT_BY_VISITORS:
    if (sort.sort == SORT_DESC)
      qsort (items, size, sizeof (GHolderItem), cmp_vis_desc);
    else
      qsort (items, size, sizeof (GHolderItem), cmp_vis_asc);
    break;
  case SORT_BY_DATA:
    if (sort.sort == SORT_DESC)
      qsort (items, size, sizeof (GHolderItem), cmp_data_desc);
    else
      qsort (items, size, sizeof (GHolderItem), cmp_data_asc);
    break;
  case SORT_BY_BW:
    if (sort.sort == SORT_DESC)
      qsort (items, size, sizeof (GHolderItem), cmp_bw_desc);
    else
      qsort (items, size, sizeof (GHolderItem), cmp_bw_asc);
    break;
  case SORT_BY_AVGTS:
    if (sort.sort == SORT_DESC)
      qsort (items, size, sizeof (GHolderItem), cmp_avgts_desc);
    else
      qsort (items, size, sizeof (GHolderItem), cmp_avgts_asc);
    break;
  case SORT_BY_CUMTS:
    if (sort.sort == SORT_DESC)
      qsort (items, size, sizeof (GHolderItem), cmp_cumts_desc);
    else
      qsort (items, size, sizeof (GHolderItem), cmp_cumts_asc);
    break;
  case SORT_BY_MAXTS:
    if (sort.sort == SORT_DESC)
      qsort (items, size, sizeof (GHolderItem), cmp_maxts_desc);
    else
      qsort (items, size, sizeof (GHolderItem), cmp_maxts_asc);
    break;
  case SORT_BY_PROT:
    if (sort.sort == SORT_DESC)
      qsort (items, size, sizeof (GHolderItem), cmp_proto_desc);
    else
      qsort (items, size, sizeof (GHolderItem), cmp_proto_asc);
    break;
  case SORT_BY_MTHD:
    if (sort.sort == SORT_DESC)
      qsort (items, size, sizeof (GHolderItem), cmp_mthd_desc);
    else
      qsort (items, size, sizeof (GHolderItem), cmp_mthd_asc);
    break;
  }
}

/* Sort raw numeric data in a descending order for the first run
 * (default sort)
 *
 * On success, raw data sorted in a descending order. */
GRawData *
sort_raw_num_data (GRawData *raw_data, int ht_size) {
  qsort (raw_data->items, ht_size, sizeof *(raw_data->items), cmp_raw_num_desc);
  return raw_data;
}

/* Sort raw string data in a descending order for the first run.
 *
 * On success, raw data sorted in a descending order. */
GRawData *
sort_raw_str_data (GRawData *raw_data, int ht_size) {
  qsort (raw_data->items, ht_size, sizeof *(raw_data->items), cmp_raw_str_desc);
  return raw_data;
}
