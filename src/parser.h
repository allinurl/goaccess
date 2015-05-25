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

#define LINE_BUFFER 	  4096
#define BW_HASHTABLES   3
#define KEY_FOUND       1
#define KEY_NOT_FOUND  -1
#define REF_SITE_LEN    512

#include "commons.h"

typedef struct GLogItem_
{
  char *agent;
  char *browser;
  char *browser_type;
  char *continent;
  char *country;
  char *date;
  char *host;
  char *keyphrase;
  char *method;
  char *os;
  char *os_type;
  char *protocol;
  char *ref;
  char *req;
  char *req_key;
  char *status;
  char *time;
  char *uniq_key;

  char site[REF_SITE_LEN];

  uint64_t resp_size;
  uint64_t serve_time;

  int type_ip;
  int is_404;
  int is_static;
  int uniq_nkey;
  int agent_nkey;
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
  GRawDataItem *items;          /* data */
  GModule module;               /* current module */
  int idx;                      /* first level index */
  int size;                     /* total num of items on ht */
} GRawData;

/* Each record contains a data value, i.e., Windows XP, and it may contain a
 * root value, i.e., Windows, and a unique key which is the combination of
 * date, IP and user agent */
typedef struct GKeyData_
{
  void *data;
  void *data_key;
  int data_nkey;

  void *root;
  void *root_key;
  int root_nkey;

  void *uniq_key;
  int uniq_nkey;
} GKeyData;

typedef struct GParse_
{
  GModule module;
  int (*key_data) (GKeyData * kdata, GLogItem * glog);

  /* data field */
  void (*datamap) (int data_nkey, const char *data, GModule module);
  void (*rootmap) (int root_nkey, const char *root, GModule module);

  /* metrics */
  void (*hits) (int data_nkey, int uniq_nkey, int root_nkey, GModule module);
  void (*visitor) (int uniq_nkey, GModule module);
  void (*bw) (int data_nkey, uint64_t size, GModule module);
  void (*avgts) (int data_nkey, uint64_t ts, GModule module);
  void (*method) (int data_nkey, const char *method, GModule module);
  void (*protocol) (int data_nkey, const char *proto, GModule module);
  void (*agent) (int data_nkey, int agent_nkey, GModule module);
} GParse;

GLog *init_log (void);
GLogItem *init_log_item (GLog * logger);
GRawDataItem *new_grawdata_item (unsigned int size);
GRawData *new_grawdata (void);
int parse_log (GLog ** logger, char *tail, int n);
int test_format (GLog * logger);
void free_raw_data (GRawData * raw_data);
void reset_struct (GLog * logger);
void verify_formats (void);

#endif
