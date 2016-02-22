/**
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

#ifndef COMMONS_H_INCLUDED
#define COMMONS_H_INCLUDED

#ifdef HAVE_LIBGEOIP
#include <GeoIP.h>
#endif

#include <time.h>
#include <stdint.h>

/* Remove the __attribute__ stuff when the compiler is not GCC. */
#if !__GNUC__
#define __attribute__(x) /**/
#endif
#define GO_UNUSED __attribute__((unused))
#define GO_VERSION 		"0.9.7"
#define GO_WEBSITE 		"http://goaccess.io/"
struct tm *now_tm;

/* debug log */
#define LOG_DEBUG(x, ...) do { dbg_fprintf x; } while (0)
/* invalid requests log */
#define LOG_INVALID(x, ...) do { invalid_fprintf x; } while (0)

/* total number of modules */
#ifdef HAVE_LIBGEOIP
#define TOTAL_MODULES    14
#else
#define TOTAL_MODULES    13
#endif

/* maximum number of items within a panel */
#define MAX_CHOICES      366

/* date and time length */
#define DATE_TIME        20
/* date length */
#define DATE_LEN         12
/* hour length */
#define HOUR_LEN          3

/* maximum protocol string length */
#define REQ_PROTO_LEN     9

/* Type of IP */
typedef enum
{
  TYPE_IPINV,
  TYPE_IPV4,
  TYPE_IPV6
} GTypeIP;

/* Type of Modules */
typedef enum MODULES
{
  VISITORS,
  REQUESTS,
  REQUESTS_STATIC,
  NOT_FOUND,
  HOSTS,
  OS,
  BROWSERS,
  VISIT_TIMES,
  VIRTUAL_HOSTS,
  REFERRERS,
  REFERRING_SITES,
  KEYPHRASES,
#ifdef HAVE_LIBGEOIP
  GEO_LOCATION,
#endif
  STATUS_CODES,
} GModule;

/* Metric totals. These are metrics that have a percent value and are
 * calculated values. */
typedef struct GPercTotals_
{
  int hits;                     /* total valid hits */
  int visitors;                 /* total visitors */
  uint64_t bw;                  /* total bandwidth */
} GPercTotals;

/* Metrics within GHolder or GDashData */
typedef struct GMetrics
{
  /* metric id can be used to identify
   * a specific data field */
  uint8_t id;
  char *data;
  char *method;
  char *protocol;

  float hits_perc;
  float visitors_perc;
  float bw_perc;

  int hits;
  int visitors;

  /* holder has a numeric value, while
   * dashboard has a displayable string value */
  union
  {
    char *sbw;
    uint64_t nbw;
  } bw;

  /* holder has a numeric value, while
   * dashboard has a displayable string value */
  union
  {
    char *sts;
    uint64_t nts;
  } avgts;

  /* holder has a numeric value, while
   * dashboard has a displayable string value */
  union
  {
    char *sts;
    uint64_t nts;
  } cumts;

  /* holder has a numeric value, while
   * dashboard has a displayable string value */
  union
  {
    char *sts;
    uint64_t nts;
  } maxts;
} GMetrics;

/* Holder sub item */
typedef struct GSubItem_
{
  GModule module;
  GMetrics *metrics;
  struct GSubItem_ *prev;
  struct GSubItem_ *next;
} GSubItem;

/* Double linked-list of sub items */
typedef struct GSubList_
{
  int size;
  struct GSubItem_ *head;
  struct GSubItem_ *tail;
} GSubList;

/* Holder item */
typedef struct GHolderItem_
{
  GSubList *sub_list;
  GMetrics *metrics;
} GHolderItem;

/* Holder of GRawData */
typedef struct GHolder_
{
  GHolderItem *items;           /* items */
  GModule module;               /* current module  */
  int idx;                      /* index  */
  int holder_size;              /* total number of allocated items */
  int ht_size;                  /* total number of data items */
  int sub_items_size;           /* total number of sub items  */
} GHolder;

/* Enum-to-string */
typedef struct GEnum_
{
  const char *str;
  int idx;
} GEnum;

/* A metric can contain a root/data/uniq node id */
typedef struct GDataMap_
{
  int data;
  int root;
} GDataMap;

typedef struct GAgentItem_
{
  char *agent;
} GAgentItem;

typedef struct GAgents_
{
  int size;
  struct GAgentItem_ *items;
} GAgents;

/* Generic Single linked-list */
typedef struct GSLList_
{
  void *data;
  struct GSLList_ *next;
} GSLList;

#define FOREACH_MODULE(item, array) \
  for (item = item; (item < ARRAY_SIZE(array)) && array[item] != -1; ++item)

/* Processing time */
extern time_t end_proc;
extern time_t timestamp;
extern time_t start_proc;

/* list of available modules/panels */
extern int module_list[TOTAL_MODULES];

/* *INDENT-OFF* */
GAgents *new_gagents (void);
GAgentItem *new_gagent_item (uint32_t size);

float get_percentage (unsigned long long total, unsigned long long hit);
int get_module_enum (const char *str);
int has_timestamp (const char *fmt);
int str2enum (const GEnum map[], int len, const char *str);

int get_module_index (int module);
int get_next_module(GModule module);
int get_prev_module(GModule module);
int ignore_panel (GModule mod);
int init_modules (void);
int remove_module(GModule module);
uint32_t get_num_modules(void);

void display_default_config_file (void);
void display_storage (void);
void display_version (void);

/* single linked-list */
GSLList *list_create (void *data);
GSLList *list_find (GSLList * node, int (*func) (void *, void *), void *data);
GSLList *list_insert_append (GSLList * node, void *data);
GSLList *list_insert_prepend (GSLList * list, void *data);
int list_count (GSLList * list);
int list_foreach (GSLList * node, int (*func) (void *, void *), void *user_data);
int list_remove_nodes (GSLList * list);
/* *INDENT-ON* */

#endif
