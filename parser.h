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

#define UKEY_BUFFER 	  2048
#define LINE_BUFFER 	  4096
#define BW_HASHTABLES   3
#define KEY_FOUND       1
#define KEY_NOT_FOUND  -1

#include "commons.h"

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
