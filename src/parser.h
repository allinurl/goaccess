/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2016 Gerardo Orellana <hello @ goaccess.io>
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

#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED

#define KEY_FOUND       1
#define KEY_NOT_FOUND  -1
#define LINE_BUFFER     4096    /* read at most this num of chars */
#define NUM_TESTS       20      /* test this many lines from the log */
#define MAX_LOG_ERRORS  20

#define LINE_LEN        23
#define ERROR_LEN       255
#define REF_SITE_LEN    511     /* maximum length of a referring site */
#define CACHE_STATUS_LEN 7

#define SPEC_TOKN_SET   0x1
#define SPEC_TOKN_NUL   0x2
#define SPEC_TOKN_INV   0x3
#define SPEC_SFMT_MIS   0x4

#include "commons.h"

/* Log properties. Note: This is per line parsed */
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
  char *qstr;
  char *ref;
  char *req;
  char *req_key;
  char *status;
  char *time;
  char *uniq_key;
  char *vhost;
  char *userid;
  char *cache_status;

  char site[REF_SITE_LEN + 1];

  uint64_t resp_size;
  uint64_t serve_time;

  int type_ip;
  int is_404;
  int is_static;
  int uniq_nkey;
  int agent_nkey;

  char *errstr;
} GLogItem;

/* Overall parsed log properties */
typedef struct GLog_
{
  unsigned int excluded_ip;
  unsigned int invalid;
  unsigned int offset;
  unsigned int processed;
  unsigned int valid;
  unsigned long long resp_size;
  unsigned short load_from_disk_only;
  unsigned short piping;
  GLogItem *items;

  unsigned short log_erridx;
  char **errors;

  FILE *pipe;
} GLog;

/* Raw data field type */
typedef enum
{
  INTEGER,
  STRING
} GRawDataType;

/* Raw Data extracted from table stores */
typedef struct GRawDataItem_
{
  int key;
  union
  {
    char *svalue;
    int ivalue;
  } value;
} GRawDataItem;

/* Raw Data per module */
typedef struct GRawData_
{
  GRawDataItem *items;          /* data */
  GModule module;               /* current module */
  GRawDataType type;            /* raw data items type */
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
  int (*key_data) (GKeyData * kdata, GLogItem * logitem);

  /* data field */
  void (*datamap) (int data_nkey, const char *data, GModule module);
  void (*rootmap) (int root_nkey, const char *root, GModule module);

  /* metrics */
  void (*hits) (int data_nkey, GModule module);
  void (*visitor) (int uniq_nkey, GModule module);
  void (*bw) (int data_nkey, uint64_t size, GModule module);
  void (*cumts) (int data_nkey, uint64_t ts, GModule module);
  void (*maxts) (int data_nkey, uint64_t ts, GModule module);
  void (*method) (int data_nkey, const char *method, GModule module);
  void (*protocol) (int data_nkey, const char *proto, GModule module);
  void (*agent) (int data_nkey, int agent_nkey, GModule module);
} GParse;

char *fgetline (FILE * fp);
char **test_format (GLog * glog, int *len);
GLog *init_log (void);
GLogItem *init_log_item (GLog * glog);
GRawDataItem *new_grawdata_item (unsigned int size);
GRawData *new_grawdata (void);
int parse_log (GLog ** glog, char *tail, int dry_run);
void free_logerrors (GLog * glog);
void free_raw_data (GRawData * raw_data);
void output_logerrors (GLog * glog);
void reset_struct (GLog * glog);

#endif
