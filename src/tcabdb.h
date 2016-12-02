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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef TCABDB_H_INCLUDED
#define TCABDB_H_INCLUDED

#include <tcadb.h>

#include "gslist.h"
#include "gstorage.h"
#include "parser.h"

#define DB_PARAMS 256

/* Metrics Storage */

/* Maps keys (string) to numeric values (integer).
 * This mitigates the issue of having multiple stores
 * with the same string key, and therefore, avoids unnecessary
 * memory usage (in most cases).
 *
 * HEAD|/index.php  -> 1
 * POST|/index.php  -> 2
 * Windows XP       -> 3
 * Ubuntu 10.10     -> 4
 * GET|Ubuntu 10.10 -> 5
 * Linux            -> 6
 * 26/Dec/2014      -> 7
 * Windows          -> 8
 */
/* MTRC_KEYMAP */

/* Maps integer keys of root elements from the keymap hash
 * to actual string values.
 *
 * 6 -> Linux
 * 8 -> Windows
 */
/* MTRC_ROOTMAP */

/* Maps integer keys of data elements from the keymap hash
 * to actual string values.
 *
 * 1 -> /index.php
 * 2 -> /index.php
 * 3 -> Windows XP
 * 4 -> Ubuntu 10.10
 * 5 -> Ubuntu 10.10
 * 7 -> 26/Dec/2014
 */
/* MTRC_DATAMAP */

/* Maps a string key made from the integer key of the
 * IP/date/UA and the integer key from the data field of
 * each module to numeric autoincremented values. e.g., "1|4"
 * => 1 -> unique visitor key (concatenated) with 4 -> data
 * key.
 *
 * "1|4" -> 1
 * "1|5" -> 2
 */
/* MTRC_UNIQMAP */

/* Maps integer key from the keymap hash to the number of
 * hits.
 *
 * 1 -> 10934
 * 2 -> 3231
 * 3 -> 500
 * 4 -> 201
 * 5 -> 206
 */
/* MTRC_HITS */

/* Maps numeric keys made from the uniqmap store to autoincremented values
 * (counter).
 * 10 -> 100
 * 40 -> 56
 */
/* MTRC_VISITORS */

/* Maps numeric data keys to bandwidth (in bytes).
 * 1 -> 1024
 * 2 -> 2048
 */
/* MTRC_BW */

/* Maps numeric data keys to cumulative time served (in usecs/msecs).
 * 1 -> 187
 * 2 -> 208
 */
/* MTRC_CUMTS */

/* Maps numeric data keys to max time served (in usecs/msecs).
 * 1 -> 1287
 * 2 -> 2308
 */
/* MTRC_MAXTS */

/* Maps numeric data keys to string values.
 * 1 -> GET
 * 2 -> POST
 */
/* MTRC_METHODS */

/* Maps numeric data keys to string values.
 * 1 -> HTTP/1.1
 * 2 -> HTTP/1.0
 */
/* MTRC_PROTOCOLS */

/* Maps numeric unique user-agent keys to the
 * corresponding numeric value.
 * 1 -> 3
 * 2 -> 4
 */
/* MTRC_AGENTS */

/* Enumerated Storage Metrics */
typedef struct GTCStorageMetric_
{
  GSMetric metric;
  const char *dbname;
  void *store;
} GTCStorageMetric;

/* Data Storage per module */
typedef struct GTCStorage_
{
  GModule module;
  GTCStorageMetric metrics[GSMTRC_TOTAL];
} GTCStorage;

void free_agent_list (void);
void free_storage (void);
void init_storage (void);

int ht_insert_agent_key (const char *key);
int ht_insert_agent_value (int key, const char *value);
int ht_insert_unique_key (const char *key);

int ht_insert_agent (GModule module, int key, int value);
int ht_insert_bw (GModule module, int key, uint64_t inc);
int ht_insert_cumts (GModule module, int key, uint64_t inc);
int ht_insert_datamap (GModule module, int key, const char *value);
int ht_insert_genstats_bw (const char *key, uint64_t inc);
int ht_insert_genstats (const char *key, int inc);
int ht_insert_hits (GModule module, int key, int inc);
int ht_insert_hostname (const char *ip, const char *host);
int ht_insert_keymap (GModule module, const char *key);
int ht_insert_maxts (GModule module, int key, uint64_t value);
int ht_insert_meta_data (GModule module, const char *key, uint64_t value);
int ht_insert_method (GModule module, int key, const char *value);
int ht_insert_protocol (GModule module, int key, const char *value);
int ht_insert_root (GModule module, int key, int value);
int ht_insert_rootmap (GModule module, int key, const char *value);
int ht_insert_uniqmap (GModule module, const char *key);
int ht_insert_visitor (GModule module, int key, int inc);
int ht_replace_genstats (const char *key, int value);

uint32_t ht_get_size_datamap (GModule module);
uint32_t ht_get_size_uniqmap (GModule module);

char *ht_get_datamap (GModule module, int key);
char *ht_get_host_agent_val (int key);
char *ht_get_hostname (const char *host);
char *ht_get_method (GModule module, int key);
char *ht_get_protocol (GModule module, int key);
char *ht_get_root (GModule module, int key);
int ht_get_hits (GModule module, int key);
int ht_get_keymap (GModule module, const char *key);
int ht_get_uniqmap (GModule module, const char *key);
int ht_get_visitors (GModule module, int key);
uint32_t ht_get_genstats (const char *key);
uint64_t ht_get_bw (GModule module, int key);
uint64_t ht_get_cumts (GModule module, int key);
uint64_t ht_get_genstats_bw (const char *key);
uint64_t ht_get_maxts (GModule module, int key);
uint64_t ht_get_meta_data (GModule module, const char *key);
void ht_get_bw_min_max (GModule module, uint64_t * min, uint64_t * max);
void ht_get_cumts_min_max (GModule module, uint64_t * min, uint64_t * max);
void ht_get_hits_min_max (GModule module, int *min, int *max);
void ht_get_maxts_min_max (GModule module, uint64_t * min, uint64_t * max);
void ht_get_visitors_min_max (GModule module, int *min, int *max);

GSLList *tclist_to_gsllist (TCLIST * tclist);
GSLList *ht_get_host_agent_list (GModule module, int key);
TCLIST *ht_get_host_agent_tclist (GModule module, int key);

GRawData *parse_raw_data (GModule module);

/* *INDENT-ON* */

#endif
