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
#define GO_VERSION 		"0.9"
#define GO_WEBSITE 		"http://goaccess.io/"
struct tm *now_tm;

#define INT_TO_PTR(i) ((void *) (long) (i))
#define PTR_TO_INT(p) ((int) (long) (p))

/* Processing time */
extern time_t end_proc;
extern time_t timestamp;
extern time_t start_proc;

/* resizing */
extern size_t real_size_y;
extern size_t term_h;
extern size_t term_w;

#ifdef DEBUG
#define LOG_DEBUG(x, ...) do { dbg_fprintf x; } while (0)
#else
#define LOG_DEBUG(x, ...) do { } while (0)
#endif

#ifdef HAVE_LIBGEOIP
#define TOTAL_MODULES    13
#else
#define TOTAL_MODULES    12
#endif

#define DATE_TIME        20
#define DATE_LEN         12     /* date length */

#define REQ_PROTO_LEN     9
#define REQ_METHOD_LEN    8

typedef enum
{
  TYPE_IPINV,
  TYPE_IPV4,
  TYPE_IPV6
} GTypeIP;

typedef enum
{
  REQUEST,
  REQUEST_METHOD,
  REQUEST_PROTOCOL
} GReqMeta;

typedef enum METRICS
{
  MTRC_KEYMAP,
  MTRC_ROOTMAP,
  MTRC_DATAMAP,
  MTRC_UNIQMAP,
  MTRC_HITS,
  MTRC_VISITORS,
  MTRC_BW,
  MTRC_TIME_SERVED,
  MTRC_METHODS,
  MTRC_PROTOCOLS,
  MTRC_AGENTS,
} GMetric;

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
  REFERRERS,
  REFERRING_SITES,
  KEYPHRASES,
#ifdef HAVE_LIBGEOIP
  GEO_LOCATION,
#endif
  STATUS_CODES,
} GModule;

typedef struct GMetrics
{
  /* metric id can be used to identified
   * a specific data field */
  uint8_t id;
  char *data;
  char *method;
  char *protocol;

  float percent;
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
} GMetrics;

typedef struct GSubItem_
{
  GModule module;
  GMetrics *metrics;
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
  GSubList *sub_list;
  GMetrics *metrics;
} GHolderItem;

typedef struct GHolder_
{
  GHolderItem *items;           /* items */
  GModule module;               /* current module  */
  int idx;                      /* index  */
  int holder_size;              /* total number of allocated items */
  int ht_size;                  /* total number of data items */
  int sub_items_size;           /* total number of sub items  */
} GHolder;

typedef struct GEnum_
{
  const char *str;
  int idx;
} GEnum;

typedef struct GDataMap_
{
  int data;
  int uniq;
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

/* single linked-list */
typedef struct GSLList_
{
  void *data;
  struct GSLList_ *next;
} GSLList;

/* *INDENT-OFF* */
GAgents *new_gagents (void);
GAgentItem *new_gagent_item (uint32_t size);

float get_percentage (unsigned long long total, unsigned long long hit);
void display_storage (void);
void display_version (void);
int get_module_enum (const char *str);
int str2enum (const GEnum map[], int len, const char *str);

/* single linked-list */
GSLList *list_create (void *data);
GSLList *list_find (GSLList * node, int (*func) (void *, void *), void *data);
GSLList *list_insert_append (GSLList * node, void *data);
GSLList *list_insert_prepend (GSLList * list, void *data);
int list_count (GSLList * list);
int list_foreach (GSLList * node, int (*func) (void *, void *), void *user_data);
int list_remove_nodes (GSLList * list);
void format_date_visitors (GMetrics * metrics);
/* *INDENT-ON* */

#endif
