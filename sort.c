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

#ifdef HAVE_LIBGEOIP
#include "geolocation.h"
#endif
#include "browsers.h"
#include "opesys.h"
#include "settings.h"
#include "util.h"

#include "sort.h"

/* *INDENT-OFF* */
const int sort_choices[][SORT_MAX_OPTS] = {
  {SORT_BY_HITS, SORT_BY_DATA, SORT_BY_BW, -1},
  {SORT_BY_HITS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, SORT_BY_PROT, SORT_BY_MTHD, -1},
  {SORT_BY_HITS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, SORT_BY_PROT, SORT_BY_MTHD, -1},
  {SORT_BY_HITS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, SORT_BY_PROT, SORT_BY_MTHD, -1},
  {SORT_BY_HITS, SORT_BY_DATA, SORT_BY_BW, SORT_BY_USEC, -1},
  {SORT_BY_HITS, SORT_BY_DATA, -1},
  {SORT_BY_HITS, SORT_BY_DATA, -1},
  {SORT_BY_HITS, SORT_BY_DATA, -1},
  {SORT_BY_HITS, SORT_BY_DATA, -1},
  {SORT_BY_HITS, SORT_BY_DATA, -1},
#ifdef HAVE_LIBGEOIP
  {SORT_BY_HITS, SORT_BY_DATA, -1},
#endif
  {SORT_BY_HITS, SORT_BY_DATA, -1},
};

static GEnum FIELD[] = {
  {"BY_HITS" , SORT_BY_HITS } ,
  {"BY_DATA" , SORT_BY_DATA } ,
  {"BY_BW"   , SORT_BY_BW   } ,
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
int
cmp_data_asc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ia->data, ib->data);
}

/* sort data descending */
int
cmp_data_desc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ib->data, ia->data);
}

/* sort raw data descending */
int
cmp_raw_data_desc (const void *a, const void *b)
{
  const GRawDataItem *ia = a;
  const GRawDataItem *ib = b;
  return strcmp ((const char *) ib->key, (const char *) ia->key);
}

/* sort numeric descending */
int
cmp_num_desc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return (int) (ib->hits - ia->hits);
}

/* sort raw numeric descending */
int
cmp_raw_num_desc (const void *a, const void *b)
{
  const GRawDataItem *ia = a;
  const GRawDataItem *ib = b;
  return (int) (*(int *) ib->value - *(int *) ia->value);
}

/* sort raw numeric descending */
int
cmp_raw_os_num_desc (const void *a, const void *b)
{
  const GRawDataItem *ia = a;
  const GRawDataItem *ib = b;
  GOpeSys *aos = ia->value;
  GOpeSys *bos = ib->value;
  return (int) (bos->hits - aos->hits);
}

/* sort raw numeric descending */
int
cmp_raw_browser_num_desc (const void *a, const void *b)
{
  const GRawDataItem *ia = a;
  const GRawDataItem *ib = b;
  GBrowser *abro = ia->value;
  GBrowser *bbro = ib->value;
  return (int) (bbro->hits - abro->hits);
}

/* sort raw numeric descending */
#ifdef HAVE_LIBGEOIP
int
cmp_raw_geo_num_desc (const void *a, const void *b)
{
  const GRawDataItem *ia = a;
  const GRawDataItem *ib = b;
  GLocation *aloc = ia->value;
  GLocation *bloc = ib->value;
  return (int) (bloc->hits - aloc->hits);
}
#endif

/* sort numeric ascending */
int
cmp_num_asc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return (int) (ia->hits - ib->hits);
}

/* sort bandwidth descending */
int
cmp_bw_desc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return (unsigned long long) (ib->bw - ia->bw);
}

/* sort bandwidth ascending */
int
cmp_bw_asc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return (unsigned long long) (ia->bw - ib->bw);
}

/* sort usec descending */
int
cmp_usec_desc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return (unsigned long long) (ib->usecs - ia->usecs);
}

/* sort usec ascending */
int
cmp_usec_asc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return (unsigned long long) (ia->usecs - ib->usecs);
}

/* sort protocol ascending */
int
cmp_proto_asc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ia->protocol, ib->protocol);
}

/* sort protocol descending */
int
cmp_proto_desc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ib->protocol, ia->protocol);
}

/* sort method ascending */
int
cmp_mthd_asc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ia->method, ib->method);
}

/* sort method descending */
int
cmp_mthd_desc (const void *a, const void *b)
{
  const GHolderItem *ia = a;
  const GHolderItem *ib = b;
  return strcmp (ib->method, ia->method);
}

/* sort raw data for the first run (default sort) */
GRawData *
sort_raw_data (GRawData * raw, GModule module, int ht_size)
{
  switch (module) {
   case VISITORS:
     qsort (raw->items, ht_size, sizeof (GRawDataItem), cmp_raw_data_desc);
     break;
   case OS:
     qsort (raw->items, ht_size, sizeof (GRawDataItem), cmp_raw_os_num_desc);
     break;
   case BROWSERS:
     qsort (raw->items, ht_size, sizeof (GRawDataItem),
            cmp_raw_browser_num_desc);
     break;
#ifdef HAVE_LIBGEOIP
   case GEO_LOCATION:
     qsort (raw->items, ht_size, sizeof (GRawDataItem), cmp_raw_geo_num_desc);
     break;
#endif
   default:
     qsort (raw->items, ht_size, sizeof (GRawDataItem), cmp_raw_num_desc);
  }
  return raw;
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
