/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2020 Gerardo Orellana <hello @ goaccess.io>
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

#ifndef GKHASH_H_INCLUDED
#define GKHASH_H_INCLUDED

#include <stdint.h>

#include "gslist.h"
#include "gstorage.h"
#include "khash.h"
#include "parser.h"

/* Enumerated Storage Metrics */
typedef enum GSMetricType_ {
  /* uint32_t key - uint32_t val */
  MTRC_TYPE_II32,
  /* uint32_t key - string val */
  MTRC_TYPE_IS32,
  /* uint32_t key - uint64_t val */
  MTRC_TYPE_IU64,
  /* string key   - uint32_t val */
  MTRC_TYPE_SI32,
  /* string key   - string val */
  MTRC_TYPE_SS32,
  /* uint32_t key - GSLList val */
  MTRC_TYPE_IGSL,
  /* string key   - GSLList val */
  MTRC_TYPE_SGSL,
  /* string key   - uint64_t val */
  MTRC_TYPE_SU64,
  /* uint32_t key - GKHashStorage_ val */
  MTRC_TYPE_IGKH,
  /* uint64_t key - uint32_t val */
  MTRC_TYPE_U648,
} GSMetricType;

typedef struct GKHashStorage_ GKHashStorage;

/* uint32_t keys           , GKHashStorage payload */
KHASH_MAP_INIT_INT (igkh, GKHashStorage *);
/* uint32_t keys           , uint32_t payload */
KHASH_MAP_INIT_INT (ii32, uint32_t);
/* uint32_t keys           , string payload */
KHASH_MAP_INIT_INT (is32, char *);
/* uint32_t keys           , uint64_t payload */
KHASH_MAP_INIT_INT (iu64, uint64_t);
/* string keys             , uint32_t payload */
KHASH_MAP_INIT_STR (si32, uint32_t);
/* string keys             , string payload */
KHASH_MAP_INIT_STR (ss32, char *);
/* string keys             , GSLList payload */
KHASH_MAP_INIT_STR (sgsl, GSLList *);
/* uint32_t keys           , GSLList payload */
KHASH_MAP_INIT_INT (igsl, GSLList *);
/* string keys             , uint64_t payload */
KHASH_MAP_INIT_STR (su64, uint64_t);
/* uint64_t key            , uint8_t payload */
KHASH_MAP_INIT_INT64 (u648, uint8_t);

typedef struct GKHashMetric_ {
  GSMetric metric;
  GSMetricType type;
  void *(*alloc) (void);
  void (*clean) (void *);
  void *hash;
  const char *filename;
} GKHashMetric;

/* Data store per module */
typedef struct GKHashModule_ {
  GModule module;
  GKHashMetric metrics[GSMTRC_TOTAL];
} GKHashModule;

/* Data store global */
typedef struct GKHashGlobal_ {
  GKHashMetric metrics[GSMTRC_TOTAL];
} GKHashGlobal;

struct GKHashStorage_ {
  GKHashModule *mhash;          /* modules */
  GKHashGlobal *ghash;          /* global */
};

#define HT_FIRST_VAL(h, kvar, code) { khint_t __k;    \
  for (__k = kh_begin(h); __k != kh_end(h); ++__k) {  \
    if (!kh_exist(h,__k)) continue;                   \
    (kvar) = kh_key(h,__k);                           \
    code;                                             \
  } }

#define HT_SUM_VAL(h, kvar, code) { khint_t __k;      \
  for (__k = kh_begin(h); __k != kh_end(h); ++__k) {  \
    if (!kh_exist(h,__k)) continue;                   \
    (kvar) = kh_key(h,__k);                           \
    code;                                             \
  } }

#define HT_FOREACH_KEY(h, kvar, code) { khint_t __k; \
  for (__k = kh_begin(h); __k != kh_end(h); ++__k) {  \
    if (!kh_exist(h,__k)) continue;                   \
    (kvar) = kh_key(h,__k);                           \
    code;                                             \
  } }

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
/*khash_t(si32) MTRC_KEYMAP */

/* Maps integer keys of root elements from the keymap hash
 * to actual string values.
 *
 * 6 -> Linux
 * 8 -> Windows
 */
/*khash_t(is32) MTRC_ROOTMAP */

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
/*khash_t(is32) MTRC_DATAMAP */

/* Maps a string key made from the integer key of the
 * IP/date/UA and the integer key from the data field of
 * each module to numeric autoincremented values. e.g., "1|4"
 * => 1 -> unique visitor key (concatenated) with 4 -> data
 * key.
 *
 * "1|4" -> 1
 * "1|5" -> 2
 */
/*khash_t(si32) MTRC_UNIQMAP */

/* Maps integer key from the keymap hash to the number of
 * hits.
 *
 * 1 -> 10934
 * 2 -> 3231
 * 3 -> 500
 * 4 -> 201
 * 5 -> 206
 */
/*khash_t(ii32) MTRC_HITS */

/* Maps numeric keys made from the uniqmap store to autoincremented values
 * (counter).
 * 10 -> 100
 * 40 -> 56
 */
/*khash_t(ii32) MTRC_VISITORS */

/* Maps numeric data keys to bandwidth (in bytes).
 * 1 -> 1024
 * 2 -> 2048
 */
/*khash_t(iu64) MTRC_BW */

/* Maps numeric data keys to cumulative time served (in usecs/msecs).
 * 1 -> 187
 * 2 -> 208
 */
/*khash_t(iu64) MTRC_CUMTS */

/* Maps numeric data keys to max time served (in usecs/msecs).
 * 1 -> 1287
 * 2 -> 2308
 */
/*khash_t(iu64) MTRC_MAXTS */

/* Maps numeric data keys to string values.
 * 1 -> GET
 * 2 -> POST
 */
/*khash_t(is32) MTRC_METHODS */

/* Maps numeric data keys to string values.
 * 1 -> HTTP/1.1
 * 2 -> HTTP/1.0
 */
/*khash_t(is32) MTRC_PROTOCOLS */

/* Maps numeric unique data keys (e.g., 192.168.0.1 => 1) to the unique user
 * agent key. Therefore, 1 IP can contain multiple user agents
 * 1 -> 3,5
 * 2 -> 4,5,6,8
 */
/*khash_t(igsl) MTRC_AGENTS */

char *ht_get_datamap (GModule module, uint32_t key);
char *ht_get_host_agent_val (uint32_t key);
char *ht_get_hostname (const char *host);
char *ht_get_method (GModule module, uint32_t key);
char *ht_get_protocol (GModule module, uint32_t key);
char *ht_get_root (GModule module, uint32_t key);
int clean_full_match_hashes (int date);
int clean_partial_match_hashes (int date);
int ht_inc_cnt_bw (uint32_t date, uint64_t inc);
int ht_insert_agent (GModule module, uint32_t date, uint32_t key,
                     uint32_t value);
int ht_insert_agent_value (uint32_t date, uint32_t key, const char *value);
int ht_insert_bw (GModule module, uint32_t date, uint32_t key, uint64_t inc);
int ht_insert_cumts (GModule module, uint32_t date, uint32_t key, uint64_t inc);
int ht_insert_datamap (GModule module, uint32_t date, uint32_t key,
                       const char *value);
int ht_insert_date (uint32_t key);
int ht_insert_hostname (const char *ip, const char *host);
int ht_insert_last_parse (uint32_t key, uint32_t value);
int ht_insert_maxts (GModule module, uint32_t date, uint32_t key,
                     uint64_t value);
int ht_insert_meta_data (GModule module, uint32_t date, const char *key,
                         uint64_t value);
int ht_insert_method (GModule module, uint32_t date, uint32_t key,
                      const char *value);
int ht_insert_protocol (GModule module, uint32_t date, uint32_t key,
                        const char *value);
int ht_insert_root (GModule module, uint32_t date, uint32_t key,
                    uint32_t value);
int ht_insert_rootmap (GModule module, uint32_t date, uint32_t key,
                       const char *value);
int ht_insert_uniqmap (GModule module, uint32_t date, uint32_t key,
                       uint32_t value);
int invalidate_date (int date);
uint32_t *get_sorted_dates (void);
uint32_t ht_get_excluded_ips (void);
uint32_t ht_get_invalid (void);
uint32_t ht_get_last_parse (uint32_t key);
uint32_t ht_get_processed (void);
uint32_t ht_get_size_datamap (GModule module);
uint32_t ht_get_size_dates (void);
uint32_t ht_get_size_uniqmap (GModule module);
uint32_t ht_get_visitors (GModule module, uint32_t key);
uint32_t ht_inc_cnt_overall (const char *key, uint32_t val);
uint32_t ht_inc_cnt_valid (uint32_t date, uint32_t inc);
uint32_t ht_insert_agent_key (uint32_t date, const char *key);
uint32_t ht_insert_hits (GModule module, uint32_t date, uint32_t key,
                         uint32_t inc);
uint32_t ht_insert_keymap (GModule module, uint32_t date, const char *key);
uint32_t ht_insert_unique_key (uint32_t date, const char *key);
uint32_t ht_insert_unique_seq (const char *key);
uint32_t ht_insert_visitor (GModule module, uint32_t date, uint32_t key,
                            uint32_t inc);
uint32_t ht_sum_valid (void);
uint32_t sum_u32_list (uint32_t (*cb) (GModule, uint32_t), GModule module,
                       GSLList * list);
uint64_t ht_get_bw (GModule module, uint32_t key);
uint64_t ht_get_cumts (GModule module, uint32_t key);
uint64_t ht_get_maxts (GModule module, uint32_t key);
uint64_t ht_get_meta_data (GModule module, const char *key);
uint64_t ht_sum_bw (void);
uint64_t sum_u64_list (uint64_t (*cb) (GModule, uint32_t), GModule module,
                       GSLList * list);
void free_raw_hits (void);
void free_storage (void);
void ht_get_bw_min_max (GModule module, uint64_t * min, uint64_t * max);
void ht_get_cumts_min_max (GModule module, uint64_t * min, uint64_t * max);
void ht_get_hits_min_max (GModule module, uint32_t * min, uint32_t * max);
void ht_get_maxts_min_max (GModule module, uint64_t * min, uint64_t * max);
void ht_get_visitors_min_max (GModule module, uint32_t * min, uint32_t * max);
void init_storage (void);
void u64decode (uint64_t n, uint32_t * x, uint32_t * y);

GRawData *parse_raw_data (GModule module);
GSLList *ht_get_host_agent_list (GModule module, uint32_t key);

#endif // for #ifndef GKHASH_H
