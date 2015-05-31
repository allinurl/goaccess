/**
 * sort.c -- functions related to sort functionality
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "settings.h"
#include "util.h"

#include "sort.h"

/* *INDENT-OFF* */
const int sort_choices[][SORT_MAX_OPTS] = {
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, -1},
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, SORT_BY_PROT, SORT_BY_MTHD, -1},
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, SORT_BY_PROT, SORT_BY_MTHD, -1},
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, SORT_BY_PROT, SORT_BY_MTHD, -1},
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, -1},
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, -1},
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, -1},
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, -1},
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, -1},
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, -1},
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, -1},
#ifdef HAVE_LIBGEOIP
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, -1},
#endif
  {SORT_BY_HITS, SORT_BY_VISITORS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, -1},
};

static GEnum FIELD[] = {
  {"BY_HITS" , SORT_BY_HITS } ,
  {"BY_VISITORS"  , SORT_BY_VISITORS } ,
  {"BY_DATA" , SORT_BY_DATA } ,
  {"BY_BW"   , SORT_BY_BW } ,
  {"BY_USEC" , SORT_BY_USEC } ,
  {"BY_PROT" , SORT_BY_PROT } ,
  {"BY_MTHD" , SORT_BY_MTHD } ,
};

static GEnum ORDER[] = {
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
  {REFERRERS           , SORT_BY_HITS , SORT_DESC } ,
  {REFERRING_SITES     , SORT_BY_HITS , SORT_DESC } ,
  {KEYPHRASES          , SORT_BY_HITS , SORT_DESC } ,
#ifdef HAVE_LIBGEOIP
  {GEO_LOCATION        , SORT_BY_HITS , SORT_DESC } ,
#endif
  {STATUS_CODES        , SORT_BY_HITS , SORT_DESC } ,
};
/* *INDENT-ON* */

/* sort data ascending */
static int
cmp_data_asc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ia->metrics->data, ib->metrics->data);
}

/* sort data descending */
static int
cmp_data_desc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ib->metrics->data, ia->metrics->data);
}

/* sort numeric descending */
static int
cmp_num_desc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  int va = ia->metrics->hits;
  int vb = ib->metrics->hits;

  return (va < vb) - (va > vb);
}

/* sort numeric ascending */
static int
cmp_num_asc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  int va = ia->metrics->hits;
  int vb = ib->metrics->hits;

  return (va > vb) - (va < vb);
}

/* sort numeric descending */
static int
cmp_vis_desc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  int va = ia->metrics->visitors;
  int vb = ib->metrics->visitors;

  return (va < vb) - (va > vb);
}

/* sort numeric ascending */
static int
cmp_vis_asc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  int va = ia->metrics->visitors;
  int vb = ib->metrics->visitors;

  return (va > vb) - (va < vb);
}

/* sort raw numeric descending */
static int
cmp_raw_num_desc (const void *a, const void *b)
{
  const GRawDataItem *ia = a;
  const GRawDataItem *ib = b;

  GDataMap *amap = ia->value;
  GDataMap *bmap = ib->value;

  int va = amap->data;
  int vb = bmap->data;

  return (va < vb) - (va > vb);
}

/* sort bandwidth descending */
static int
cmp_bw_desc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  uint64_t va = ia->metrics->bw.nbw;
  uint64_t vb = ib->metrics->bw.nbw;

  return (va < vb) - (va > vb);
}

/* sort bandwidth ascending */
static int
cmp_bw_asc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  uint64_t va = ia->metrics->bw.nbw;
  uint64_t vb = ib->metrics->bw.nbw;

  return (va > vb) - (va < vb);
}

/* sort usec descending */
static int
cmp_usec_desc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  uint64_t va = ia->metrics->avgts.nts;
  uint64_t vb = ib->metrics->avgts.nts;

  return (va < vb) - (va > vb);
}

/* sort usec ascending */
static int
cmp_usec_asc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;

  uint64_t va = ia->metrics->avgts.nts;
  uint64_t vb = ib->metrics->avgts.nts;

  return (va > vb) - (va < vb);
}

/* sort protocol ascending */
static int
cmp_proto_asc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ia->metrics->protocol, ib->metrics->protocol);
}

/* sort protocol descending */
static int
cmp_proto_desc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ib->metrics->protocol, ia->metrics->protocol);
}

/* sort method ascending */
static int
cmp_mthd_asc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ia->metrics->method, ib->metrics->method);
}

/* sort method descending */
static int
cmp_mthd_desc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ib->metrics->method, ia->metrics->method);
}

int
get_sort_field_enum (const char *str)
{
  return str2enum (FIELD, ARRAY_SIZE (FIELD), str);
}

int
get_sort_order_enum (const char *str)
{
  return str2enum (ORDER, ARRAY_SIZE (ORDER), str);
}

void
set_initial_sort (const char *smod, const char *sfield, const char *ssort)
{
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

int
can_sort_module (GModule module, int field)
{
  int i, can_sort = 0;
  for (i = 0; -1 != sort_choices[module][i]; i++) {
    if (sort_choices[module][i] != field)
      continue;
    if (SORT_BY_USEC == field && !conf.serve_usecs)
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

void
parse_initial_sort (void)
{
  int i;
  char *view;
  char module[SORT_MODULE_LEN], field[SORT_FIELD_LEN], order[SORT_ORDER_LEN];
  for (i = 0; i < conf.sort_panel_idx; ++i) {
    view = conf.sort_panels[i];
    if (sscanf (view, "%8[^','],%7[^','],%4s", module, field, order) != 3)
      continue;
    set_initial_sort (module, field, order);
  }
}

/* apply user defined sort */
void
sort_holder_items (GHolderItem * items, int size, GSort sort)
{
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
  case SORT_BY_USEC:
    if (sort.sort == SORT_DESC)
      qsort (items, size, sizeof (GHolderItem), cmp_usec_desc);
    else
      qsort (items, size, sizeof (GHolderItem), cmp_usec_asc);
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

/* sort raw data for the first run (default sort) */
GRawData *
sort_raw_data (GRawData * raw, int ht_size)
{
  qsort (raw->items, ht_size, sizeof (GRawDataItem), cmp_raw_num_desc);
  return raw;
}
