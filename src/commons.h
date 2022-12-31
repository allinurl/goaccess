/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2022 Gerardo Orellana <hello @ goaccess.io>
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

#ifndef COMMONS_H_INCLUDED
#define COMMONS_H_INCLUDED

#include <time.h>
#include <stdint.h>
#include "gslist.h"

/* Remove the __attribute__ stuff when the compiler is not GCC. */
#if !__GNUC__
#define __attribute__(x) /**/
#endif
#define GO_UNUSED __attribute__((unused))
#define GO_VERSION 		"1.7"
#define GO_WEBSITE 		"https://goaccess.io/"
extern struct tm now_tm;

/* common char array buffer size */
#define INIT_BUF_SIZE 1024

/* total number of modules */
#ifdef HAVE_GEOLOCATION
#define TOTAL_MODULES    19
#else
#define TOTAL_MODULES    17
#endif

/* maximum number of items within a panel */
#define MAX_CHOICES          366
/* real-time */
#define MAX_CHOICES_RT        50
/* max default items when date-spec = min */
#define MAX_CHOICES_MINUTE  1440        /* 24hrs */

/* date and time length - e.g., 2016/12/12 12:12:12 -0600 */
#define DATE_TIME     25 + 1
/* date length -  e.g., 2016/12/12 */
#define DATE_LEN      10 + 1
/* date length -  e.g., 12:12:12 */
#define TIME_LEN       8 + 1
/* hour + ':' + min length - e.g., 12:12 */
#define HRMI_LEN   4 + 1 + 1

#define YR_FMT "%Y"
#define MO_FMT "%M"
#define DT_FMT "%d"

/* maximum protocol string length */
#define REQ_PROTO_LEN     9

#define IGNORE_LEVEL_PANEL 1
#define IGNORE_LEVEL_REQ 2

/* Type of IP */
typedef enum {
  TYPE_IPINV,
  TYPE_IPV4,
  TYPE_IPV6
} GTypeIP;

/* Type of Modules */
typedef enum MODULES {
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
  STATUS_CODES,
  REMOTE_USER,
  CACHE_STATUS,
#ifdef HAVE_GEOLOCATION
  GEO_LOCATION,
  ASN,
#endif
  MIME_TYPE,
  TLS_TYPE,
} GModule;

/* Metric totals. These are metrics that have a percent value and are
 * calculated values. */
typedef struct GPercTotals_ {
  uint32_t hits;                /* total valid hits */
  uint32_t visitors;            /* total visitors */
  uint64_t bw;                  /* total bandwidth */
} GPercTotals;

/* Metrics within GHolder or GDashData */
typedef struct GMetrics {
  /* metric id can be used to identify
   * a specific data field */
  uint8_t id;
  char *data;
  char *method;
  char *protocol;

  float hits_perc;
  float visitors_perc;
  float bw_perc;

  uint64_t hits;
  uint64_t visitors;

  /* holder has a numeric value, while
   * dashboard has a displayable string value */
  union {
    char *sbw;
    uint64_t nbw;
  } bw;

  /* holder has a numeric value, while
   * dashboard has a displayable string value */
  union {
    char *sts;
    uint64_t nts;
  } avgts;

  /* holder has a numeric value, while
   * dashboard has a displayable string value */
  union {
    char *sts;
    uint64_t nts;
  } cumts;

  /* holder has a numeric value, while
   * dashboard has a displayable string value */
  union {
    char *sts;
    uint64_t nts;
  } maxts;
} GMetrics;

/* Holder sub item */
typedef struct GSubItem_ {
  GModule module;
  GMetrics *metrics;
  struct GSubItem_ *prev;
  struct GSubItem_ *next;
} GSubItem;

/* Double linked-list of sub items */
typedef struct GSubList_ {
  int size;
  struct GSubItem_ *head;
  struct GSubItem_ *tail;
} GSubList;

/* Holder item */
typedef struct GHolderItem_ {
  GSubList *sub_list;
  GMetrics *metrics;
} GHolderItem;

/* Holder of GRawData */
typedef struct GHolder_ {
  GHolderItem *items;           /* holder items */
  GModule module;               /* current module  */
  int idx;                      /* holder index  */
  int holder_size;              /* number of allocated items */
  uint32_t ht_size;             /* size of the hash table/store */
  int sub_items_size;           /* number of sub items  */
} GHolder;

/* Enum-to-string */
typedef struct GEnum_ {
  const char *str;
  int idx;
} GEnum;

/* A metric can contain a root/data/uniq node id */
typedef struct GDataMap_ {
  int data;
  int root;
} GDataMap;

typedef struct GAgentItem_ {
  char *agent;
} GAgentItem;

typedef struct GAgents_ {
  int size;
  int idx;
  struct GAgentItem_ *items;
} GAgents;

#define FOREACH_MODULE(item, array) \
  for (; (item < ARRAY_SIZE(array)) && array[item] != -1; ++item)

/* Processing time */
extern time_t end_proc;
extern time_t timestamp;
extern time_t start_proc;

/* list of available modules/panels */
extern int module_list[TOTAL_MODULES];

/* *INDENT-OFF* */
GAgents *new_gagents (uint32_t size);
void free_agents_array (GAgents *agents);

char *enum2str (const GEnum map[], int len, int idx);
char *get_module_str (GModule module);
float get_percentage (unsigned long long total, unsigned long long hit);
int get_max_choices (void);
int get_module_enum (const char *str);
int has_timestamp (const char *fmt);
int str2enum (const GEnum map[], int len, const char *str);

int enable_panel (GModule mod);
int get_module_index (int module);
int get_next_module (GModule module);
int get_prev_module (GModule module);
int ignore_panel (GModule mod);
int init_modules (void);
int remove_module(GModule module);
uint32_t get_num_modules (void);
void verify_panels (void);

char *get_log_source_str (int max_len);
intmax_t get_log_sizes (void);

void display_default_config_file (void);
void display_storage (void);
void display_version (void);
/* *INDENT-ON* */

#endif
