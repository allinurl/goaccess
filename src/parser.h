/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2023 Gerardo Orellana <hello @ goaccess.io>
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
#define READ_BYTES      4096u
#define MAX_BATCH_LINES 8192u   /* max number of lines to read per batch before a reflow */

#define LINE_LEN          23
#define ERROR_LEN        255
#define REF_SITE_LEN     511    /* maximum length of a referring site */
#define CACHE_STATUS_LEN   7
#define HASH_HEX          64

#define ERR_SPEC_TOKN_NUL          0x1
#define ERR_SPEC_TOKN_INV          0x2
#define ERR_SPEC_SFMT_MIS          0x3
#define ERR_SPEC_LINE_INV          0x4
#define ERR_LOG_NOT_FOUND          0x5
#define ERR_LOG_REALLOC_FAILURE    0x6


#include <stdio.h>
#include "commons.h"
#include "gslist.h"

typedef struct GLogProp_ {
  char *filename;               /* filename including path */
  char *fname;                  /* basename(filename) */
  uint64_t inode;               /* inode of the log */
  uint64_t size;                /* original size of log */
} GLogProp;

/* Log properties. Note: This is per line parsed */
typedef struct GLogItem_ {
  char *agent;
  char *browser;
  char *browser_type;
  char *continent;
  char *country;
  char *asn;
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
  char agent_hex[HASH_HEX];

  uint64_t resp_size;
  uint64_t serve_time;

  uint32_t numdate;
  uint32_t agent_hash;
  int ignorelevel;
  int type_ip;
  int is_404;
  int is_static;
  int uniq_nkey;
  int agent_nkey;

  /* UMS */
  char *mime_type;
  char *tls_type;
  char *tls_cypher;
  char *tls_type_cypher;

  char *errstr;
  struct tm dt;
} GLogItem;

typedef struct GLastParse_ {
  uint32_t line;
  int64_t ts;
  uint64_t size;
  uint16_t snippetlen;
  char snippet[READ_BYTES + 1];
} GLastParse;

/* Overall parsed log properties */
typedef struct GLog_ {
  uint8_t piping:1;
  uint8_t log_erridx;
  uint32_t read;                /* lines read/parsed */
  uint64_t bytes;               /* bytes read on each iteration */
  uint64_t length;              /* length read from the log so far */
  uint64_t invalid;             /* invalid lines for this log */
  uint64_t processed;           /* lines proceeded for this log */

  /* file test for persisted/restored data */
  uint16_t snippetlen;
  char snippet[READ_BYTES + 1];

  GLastParse lp;
  GLogProp props;
  struct tm start_time;

  char *fname_as_vhost;
  char **errors;

  FILE *pipe;
} GLog;

/* Container for all logs */
typedef struct Logs_ {
  uint8_t restored:1;
  uint8_t load_from_disk_only:1;
  uint64_t *processed;
  uint64_t offset;
  int size;                     /* num items */
  int idx;
  char *filename;
  GLog *glog;
} Logs;

/* Pthread jobs for multi-thread */
typedef struct GJob_ {
  int p, cnt, test, dry_run;
  GLog *glog;
  GLogItem **logitems;
  char **lines;
} GJob;

/* Raw data field type */
typedef enum {
  U32,
  STR
} datatype;

/* Raw Data extracted from table stores */
typedef struct GRawDataItem_ {
  uint32_t nkey;
  union {
    const char *data;
    uint32_t hits;
  };
} GRawDataItem;

/* Raw Data per module */
typedef struct GRawData_ {
  GRawDataItem *items;          /* data */
  GModule module;               /* current module */
  datatype type;
  int idx;                      /* first level index */
  int size;                     /* total num of items on ht */
} GRawData;


char *extract_by_delim (const char **str, const char *end);
char *fgetline (FILE * fp);
char **test_format (Logs * logs, int *len);
int parse_log (Logs * logs, int dry_run);
GLogItem *parse_line (GLog * glog, char *line, int dry_run);
void *read_lines_thread (void *arg);
void *process_lines_thread (void *arg);
int set_glog (Logs * logs, const char *filename);
int set_initial_persisted_data (GLog * glog, FILE * fp, const char *fn);
int set_log (Logs * logs, const char *value);
void free_logerrors (GLog * glog);
void free_logs (Logs * logs);
void free_glog (GLogItem *logitem);
void free_raw_data (GRawData * raw_data);
void output_logerrors (void);
void reset_struct (Logs * logs);

GLogItem *init_log_item (GLog * glog);
GRawDataItem *new_grawdata_item (unsigned int size);
GRawData *new_grawdata (void);
Logs *init_logs (int size);
Logs *new_logs (int size);

#endif
