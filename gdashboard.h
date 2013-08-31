/**
 * Copyright (C) 2009-2013 by Gerardo Orellana <goaccess@prosoftcorp.com>
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

#ifndef GDASHBOARD_H_INCLUDED
#define GDASHBOARD_H_INCLUDED

#include "ui.h"
#include "parser.h"

/* *INDENT-OFF* */
#define DASH_HEAD_POS   0  /* header line pos           */
#define DASH_DESC_POS   1  /* description line pos      */
#define DASH_EMPTY_POS  2  /* empty line pos            */
#define DASH_DATA_POS   3  /* empty line pos            */

#define DASH_COLLAPSED  11 /* total items per module    */
#define DASH_EXPANDED   32 /* total items when expanded */
#define DASH_NON_DATA   4  /* items without stats       */

#define DASH_INIT_X     2  /* x-axis offset             */
#define DASH_BW_LEN     11 /* max bandwidth length      */
#define DASH_SRV_TM_LEN 9  /* max served time length    */
#define DASH_SPACE      1  /* space between data        */

#define VISIT_HEAD "Unique visitors per day - Including spiders"
#define VISIT_DESC "Hits having the same IP, date and agent are a unique visit"
#define REQUE_HEAD "Requested files (Pages-URL)"
#define REQUE_DESC "Top requested files - hits, percent, [bandwidth, time served]"
#define STATI_HEAD "Requested static files (e.g., png, js, css, etc)"
#define STATI_DESC "Top requested static files - hits, percent, [bandwidth, time served]"
#define REFER_HEAD "Referrers URLs"
#define REFER_DESC "Top requested referrers - hits, percent"
#define FOUND_HEAD "HTTP 404 not found URLs"
#define FOUND_DESC "Top 404 not found URLs - hits, percent, [bandwidth, time served]"
#define OPERA_HEAD "Operating Systems"
#define OPERA_DESC "Top operating systems - hits, percent"
#define BROWS_HEAD "Browsers"
#define BROWS_DESC "Top browsers - hits, percent"
#define HOSTS_HEAD "Hosts"
#define HOSTS_DESC "Top hosts - hits, percent, [bandwidth, time served]"
#define CODES_HEAD "HTTP status codes"
#define CODES_DESC "Top HTTP status codes - hits, percent"
#define SITES_HEAD "Referring Sites"
#define SITES_DESC "Top referring sites - hits, percent"
#define KEYPH_HEAD "Keyphrases from Google's search engine"
#define KEYPH_DESC "Top keyphrases - hits, percent"

typedef struct GDashStyle_
{
   int color_hits;
   int color_data;
   int color_bw;
   int color_percent;
   int color_bars;
   int color_usecs;
} GDashStyle;

typedef struct GDashData_
{
   char *serve_time;
   char *bandwidth;
   char *data;
   float percent;
   int hits;
   unsigned long long bw;
   unsigned long long usecs;
   short is_subitem;
} GDashData;

typedef struct GDashModule_
{
   GDashData *data;
   GModule module;
   char *desc;
   char *head;
   int alloc_data;  /* alloc data items */
   int dash_size;   /* dashboard size   */
   int data_len;
   int hits_len;
   int holder_size; /* hash table size  */
   int ht_size;     /* hash table size  */
   int idx_data;    /* idx data         */
   int max_hits;
   int perc_len;
} GDashModule;

typedef struct GDash_
{
   int total_alloc;
   GDashModule module[TOTAL_MODULES];
} GDash;

typedef struct GSubItem_
{
   GModule module;
   const char *data;
   int hits;
   unsigned long long bw;
   struct GSubItem_ *prev;
   struct GSubItem_ *next;
} GSubItem;

typedef struct GSubList_
{
   int size;
   struct GSubItem_ *head;
   struct GSubItem_ *tail;
} GSubList;

typedef struct GHolderItem_
{
   char *data;
   int hits;
   unsigned long long bw;
   unsigned long long usecs;
   GSubList *sub_list;
} GHolderItem;

typedef struct GHolder_
{
   GHolderItem *items; /* 1st level data    */
   GModule module;     /* current module    */
   int idx;            /* 1st level index   */
   int holder_size;    /* total items on ht */
   int sub_items_size;
} GHolder;

typedef struct GRawDataItem_
{
   char *data;
   int hits;
   unsigned long long bw;
   unsigned long long usecs;
} GRawDataItem;

typedef struct GRawData_
{
   GRawDataItem *items; /* 1st level data    */
   GModule module;      /* current module    */
   int idx;             /* 1st level index   */
   int size;            /* total items on ht */
} GRawData;

float get_percentage (unsigned long long total, unsigned long long hit);
GDashData *new_gdata (unsigned int size);
GDash *new_gdash ();
GHolder *new_gholder (unsigned int size);
GRawData *parse_raw_data (GHashTable * ht, int ht_size, GModule module);
int get_item_idx_in_holder (GHolder * holder, char *k);
int perform_next_find (GHolder * h, GScrolling * scrolling);
int render_find_dialog (WINDOW * main_win, GScrolling * scrolling);
void *add_hostname_node (void *ptr_holder);
void add_sub_item_back (GSubList * sub_list, GModule module, const char *data, int hits, unsigned long long bw);
void display_content (WINDOW * win, GLog * logger, GDash * dash, GScrolling * scrolling);
void free_dashboard (GDash * dash);
void free_holder_by_module (GHolder ** holder, GModule module);
void free_holder (GHolder ** holder);
void load_data_to_dash (GHolder * h, GDash * dash, GModule module, GScrolling * scrolling);
void load_data_to_holder (GRawData * raw_data, GHolder * h, GModule module, GSort sort);
void load_host_to_holder (GHolder * h, char *ip);
void reset_find ();
void reset_scroll_offsets (GScrolling * scrolling);
/* *INDENT-ON* */

#endif
