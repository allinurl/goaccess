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
#define REF_SITE_LEN    512

#include "commons.h"

typedef struct GLogItem_
{
  char *agent;
  char *date;
  char *host;
  char *method;
  char *protocol;
  char *ref;
  char *req;
  char *req_key;
  char *status;
  char date_key[DATE_LEN];      /* Ymd */
  char site[REF_SITE_LEN];
  unsigned long long resp_size;
  unsigned long long serve_time;
} GLogItem;

typedef struct GLog_
{
  unsigned int exclude_ip;
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

/* *INDENT-OFF* */
GLog *init_log (void);
GLogItem *init_log_item (GLog * logger);
GRawData *new_grawdata (void);
GRawDataItem *new_grawdata_item (unsigned int size);
int parse_log (GLog ** logger, char *tail, int n);
int test_format (GLog * logger);
void free_raw_data (GRawData * raw_data);
void reset_struct (GLog * logger);
/* *INDENT-ON* */

#endif
