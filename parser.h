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

#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED

#define UKEY_BUFFER 	   2048
#define LINE_BUFFER 	   4096
#define BW_HASHTABLES   3
#define KEY_FOUND       1
#define KEY_NOT_FOUND  -1

#ifdef HAVE_LIBGLIB_2_0
#include <glib.h>
#endif

#include "commons.h"

extern GHashTable *ht_browsers;
extern GHashTable *ht_countries;
extern GHashTable *ht_date_bw;
extern GHashTable *ht_file_bw;
extern GHashTable *ht_file_serve_usecs;
extern GHashTable *ht_host_bw;
extern GHashTable *ht_hostnames;
extern GHashTable *ht_hosts;
extern GHashTable *ht_hosts_agents;
extern GHashTable *ht_host_serve_usecs;
extern GHashTable *ht_keyphrases;
extern GHashTable *ht_not_found_requests;
extern GHashTable *ht_os;
extern GHashTable *ht_referrers;
extern GHashTable *ht_referring_sites;
extern GHashTable *ht_requests;
extern GHashTable *ht_requests_static;
extern GHashTable *ht_status_code;
extern GHashTable *ht_unique_vis;
extern GHashTable *ht_unique_visitors;

typedef struct GLogItem_
{
  char *agent;
  char *date;
  char *host;
  char *ref;
  char *method;
  char *protocol;
  char *req;
  char *status;
  unsigned long long resp_size;
  unsigned long long serve_time;
} GLogItem;

typedef struct GLog_
{
  unsigned int invalid;
  unsigned int offset;
  unsigned int process;
  unsigned long long resp_size;
  unsigned short piping;
  GLogItem *items;
} GLog;

typedef struct GRawDataItem_
{
  void *key;
  void *value;
} GRawDataItem;

typedef struct GRawData_
{
  GRawDataItem *items;          /* data                     */
  GModule module;               /* current module           */
  int idx;                      /* first level index        */
  int size;                     /* total num of items on ht */
} GRawData;

GLog *init_log (void);
GLogItem *init_log_item (GLog * logger);
GRawDataItem *new_grawdata_item (unsigned int size);
GRawData *new_grawdata (void);
int cmp_bw_asc (const void *a, const void *b);
int cmp_bw_desc (const void *a, const void *b);
int cmp_data_asc (const void *a, const void *b);
int cmp_data_desc (const void *a, const void *b);
int cmp_mthd_asc (const void *a, const void *b);
int cmp_mthd_desc (const void *a, const void *b);
int cmp_num_asc (const void *a, const void *b);
int cmp_num_desc (const void *a, const void *b);
int cmp_proto_asc (const void *a, const void *b);
int cmp_proto_desc (const void *a, const void *b);
int cmp_raw_browser_num_desc (const void *a, const void *b);
int cmp_raw_data_desc (const void *a, const void *b);
int cmp_raw_geo_num_desc (const void *a, const void *b);
int cmp_raw_num_desc (const void *a, const void *b);
int cmp_raw_os_num_desc (const void *a, const void *b);
int cmp_raw_req_num_desc (const void *a, const void *b);
int cmp_usec_asc (const void *a, const void *b);
int cmp_usec_desc (const void *a, const void *b);
int parse_log (GLog ** logger, char *tail, int n);
int test_format (GLog * logger);
void free_raw_data (GRawData * raw_data);
void reset_struct (GLog * logger);

#endif
