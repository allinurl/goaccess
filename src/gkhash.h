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

#ifndef GKHASH_H_INCLUDED
#define GKHASH_H_INCLUDED

#include <stdint.h>

#include "gslist.h"
#include "gstorage.h"
#include "khash.h"
#include "parser.h"

#define DB_VERSION  2
#define DB_INSTANCE 1

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
  /* string key   - uint8_t val */
  MTRC_TYPE_SI08,
  /* uint32_t key - uint8_t val */
  MTRC_TYPE_II08,
  /* string key   - string val */
  MTRC_TYPE_SS32,
  /* uint32_t key - GSLList val */
  MTRC_TYPE_IGSL,
  /* string key   - uint64_t val */
  MTRC_TYPE_SU64,
  /* uint32_t key - GKHashStorage_ val */
  MTRC_TYPE_IGKH,
  /* uint64_t key - uint32_t val */
  MTRC_TYPE_U648,
  /* uint32_t key - GLastParse val */
  MTRC_TYPE_IGLP,
} GSMetricType;

typedef struct GKDB_ GKDB;
typedef struct GKHashStorage_ GKHashStorage;

/* *INDENT-OFF* */
/* uint32_t keys           , GKDB payload */
KHASH_MAP_INIT_INT (igdb   , GKDB *);
/* uint32_t keys           , GKHashStorage payload */
KHASH_MAP_INIT_INT (igkh   , GKHashStorage *);
/* uint32_t keys           , uint32_t payload */
KHASH_MAP_INIT_INT (ii32   , uint32_t);
/* uint32_t keys           , string payload */
KHASH_MAP_INIT_INT (is32   , char *);
/* uint32_t keys           , uint64_t payload */
KHASH_MAP_INIT_INT (iu64   , uint64_t);
/* string keys             , uint32_t payload */
KHASH_MAP_INIT_STR (si32   , uint32_t);
/* string keys             , uint8_t payload */
KHASH_MAP_INIT_STR (si08   , uint8_t);
/* uint8_t keys            , uint8_t payload */
KHASH_MAP_INIT_INT (ii08   , uint8_t);
/* string keys             , string payload */
KHASH_MAP_INIT_STR (ss32   , char *);
/* uint32_t key            , GLastParse payload */
KHASH_MAP_INIT_INT (iglp   , GLastParse);
/* uint32_t keys           , GSLList payload */
KHASH_MAP_INIT_INT (igsl   , GSLList *);
/* string keys             , uint64_t payload */
KHASH_MAP_INIT_STR (su64   , uint64_t);
/* uint64_t key            , uint8_t payload */
KHASH_MAP_INIT_INT64 (u648 , uint8_t);
/* *INDENT-ON* */

typedef struct GKHashMetric_ {
  union {
    GSMetric storem;
    GAMetric dbm;
  } metric;
  GSMetricType type;
  void *(*alloc) (void);
  void (*des) (void *, uint8_t free_data);
  void (*del) (void *, uint8_t free_data);
  uint8_t free_data:1;
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

/* Whole App Data store */
typedef struct GKHashDB_ {
  GKHashMetric metrics[GAMTRC_TOTAL];
} GKHashDB;

/* DB */
struct GKDB_ {
  GKHashDB *hdb;                /* app-level hash tables */
  GKHashModule *cache;          /* cache modules */
  GKHashStorage *store;         /* per date OR module */
  Logs *logs;                   /* logs parsing per db instance */
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

extern GKHashMetric module_metrics[];
extern const GKHashMetric global_metrics[];
extern const GKHashMetric app_metrics[];
extern size_t global_metrics_len;
extern size_t module_metrics_len;
extern size_t app_metrics_len;

/* Metrics Storage */

/* Most metrics are encapsulated within a GKHashStorage structure, which is
 * conformed of a dated key and a GKHashStorage struct value. This helps to
 * easily destroy the entire dated storage at any time. */

/* GLOBAL METRICS */
/* ============== */
/* Maps a string key containing an IP|DATE|UA(hash uint32_t => hex) to an
 * autoincremented value.
 *
 * 192.168.0.1|27/Apr/2020|7E8E0E -> 1
 * 192.168.0.1|28/Apr/2020|7E8E0E -> 2
 */
/*khash_t(si32) MTRC_UNIQUE_KEYS */

/* Maps string keys made out of the user agent to an autoincremented value.
 *
 * Debian APT-HTTP/1.3 (1.0.9.8.5)                      -> 1838302 -> 1
 * Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.1;) -> 8723842 -> 2
 */
/*khash_t(ii32) MTRC_AGENT_KEYS */

/* Maps integer keys from the autoincremented MTRC_AGENT_KEYS value to the user
 * agent.
 *
 * 1 -> Debian APT-HTTP/1.3 (1.0.9.8.5)
 * 2 -> Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.1; WOW64; Trident/6.0)
 */
/*khash_t(is32) MTRC_AGENT_VALS */

/* Maps a single numeric key (usually 1) to an autoincremented hits value.
 *
 * 1 -> 5
 */
/*khash_t(is32) MTRC_CNT_VALID */

/* Maps a single numeric key (usually 1) to an autoincremented bw value.
 *
 * 1 -> 592933
 */
/*khash_t(iu64) MTRC_CNT_BW */


/* MODULE METRICS */
/* ============== */
/* Maps keys (string) to a hash to a numeric values (uint32_t).
 * this mitigates the issue of having multiple stores
 * with the same string key, and therefore, avoids unnecessary
 * memory usage (in most cases).
 *
 * HEAD|/index.php -> 9872347 -> 1
 * POST|/index.php -> 3452345 -> 2
 * Windows XP      -> 2842343 -> 3
 * Ubuntu 10.10    -> 1852342 -> 4
 * GET|Ubuntu 10.10-> 4872343 -> 5
 * GNU+Linux       -> 5862347 -> 6
 * 26/Dec/2014     -> 9874347 -> 7
 * Windows         -> 3875347 -> 8
 */
/*khash_t(si32) MTRC_KEYMAP */

/* Maps integer keys of root elements from the keymap hash
 * to actual string values.
 *
 * 6 -> GNU+Linux
 * 8 -> Windows
 */
/*khash_t(is32) MTRC_ROOTMAP */

/* Maps integer keys of data elements from the keymap hash
 * to actual string values.
 *
 * 1 -> /index.php
 * 2 -> /index.php
 * 3 -> Windows xp
 * 4 -> Ubuntu 10.10
 * 5 -> Ubuntu 10.10
 * 7 -> 26/dec/2014
 */
/*khash_t(is32) MTRC_DATAMAP */

/* Maps the unique uint32_t key of the IP/date/UA and the uint32_t key from the
 * data field encoded into a uint64_t key to numeric autoincremented values.
 * e.g., "1&4" => 12938238293 to ai.
 *
 * 201023232 -> 1
 * 202939232 -> 2
 */
/*khash_t(si32) MTRC_UNIQMAP */

/* Maps integer keys made from a data key to an integer root key in
 * MTRC_KEYMAP.
 *
 * 4 -> 6
 * 3 -> 8
 */
/*khash_t(ii32) MTRC_ROOT */

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

/* Maps numeric data keys to uint8_t values.
 * 1 -> 3
 * 2 -> 4
 */
/*khash_t(is32) MTRC_METHODS */

/* Maps numeric data keys to uint8_t values.
 * 1 -> 1
 * 2 -> 1
 */
/*khash_t(is32) MTRC_PROTOCOLS */

/* Maps numeric unique data keys (e.g., 192.168.0.1 => 1) to the unique user
 * agent key. Therefore, 1 IP can contain multiple user agents
 * 1 -> 3,5
 * 2 -> 4,5,6,8
 */
/*khash_t(igsl) MTRC_AGENTS */

/* Maps a string key counter such as sum of hits to an autoincremented value
 * "sum_hits" -> 9383
 * "sum_bw"   -> 3232932
 */
/*khash_t(igsl) MTRC_METADATA */

/* *INDENT-OFF* */
char *get_mtr_type_str (GSMetricType type);
char *ht_get_datamap (GModule module, uint32_t key);
char *ht_get_host_agent_val (uint32_t key);
char *ht_get_hostname (const char *host);
char *ht_get_json_logfmt (const char *key);
char *ht_get_method (GModule module, uint32_t key);
char *ht_get_protocol (GModule module, uint32_t key);
char *ht_get_root (GModule module, uint32_t key);
int ht_inc_cnt_bw (uint32_t date, uint64_t inc);
int ht_insert_agent (GModule module, uint32_t date, uint32_t key, uint32_t value);
int ht_insert_agent_value (uint32_t date, uint32_t key, char *value);
int ht_insert_bw (GModule module, uint32_t date, uint32_t key, uint64_t inc, uint32_t ckey);
int ht_insert_cumts (GModule module, uint32_t date, uint32_t key, uint64_t inc, uint32_t ckey);
int ht_insert_datamap (GModule module, uint32_t date, uint32_t key, const char *value, uint32_t ckey);
int ht_insert_date (uint32_t key);
int ht_insert_hostname (const char *ip, const char *host);
int ht_insert_json_logfmt (GO_UNUSED void *userdata, char *key, char *spec);
int ht_insert_last_parse (uint32_t key, GLastParse lp);
int ht_insert_maxts (GModule module, uint32_t date, uint32_t key, uint64_t value, uint32_t ckey);
int ht_insert_meta_data (GModule module, uint32_t date, const char *key, uint64_t value);
int ht_insert_method (GModule module, uint32_t date, uint32_t key, const char *value, uint32_t ckey);
int ht_insert_protocol (GModule module, uint32_t date, uint32_t key, const char *value, uint32_t ckey);
int ht_insert_root (GModule module, uint32_t date, uint32_t key, uint32_t value, uint32_t dkey, uint32_t rkey);
int ht_insert_rootmap (GModule module, uint32_t date, uint32_t key, const char *value, uint32_t ckey);
int ht_insert_uniqmap (GModule module, uint32_t date, uint32_t key, uint32_t value);
int invalidate_date (int date);
int rebuild_rawdata_cache (void);
uint32_t *get_sorted_dates (uint32_t *len);
uint32_t ht_get_excluded_ips (void);
uint32_t ht_get_hits (GModule module, int key);
uint32_t ht_get_invalid (void);
uint32_t ht_get_keymap (GModule module, const char *key);
uint32_t ht_get_processed (void);
uint32_t ht_get_processing_time (void);
uint32_t ht_get_size_datamap (GModule module);
uint32_t ht_get_size_dates (void);
uint32_t ht_get_size_uniqmap (GModule module);
uint32_t ht_get_visitors (GModule module, uint32_t key);
uint32_t ht_inc_cnt_overall (const char *key, uint32_t val);
uint32_t ht_inc_cnt_valid (uint32_t date, uint32_t inc);
uint32_t ht_insert_agent_key (uint32_t date, uint32_t key);
uint32_t ht_insert_hits (GModule module, uint32_t date, uint32_t key, uint32_t inc, uint32_t ckey);
uint32_t ht_insert_keymap (GModule module, uint32_t date, uint32_t key, uint32_t * ckey);
uint32_t ht_insert_unique_key (uint32_t date, const char *key);
uint32_t ht_insert_unique_seq (const char *key);
uint32_t ht_insert_visitor (GModule module, uint32_t date, uint32_t key, uint32_t inc, uint32_t ckey);
uint32_t ht_sum_valid (void);
uint64_t ht_get_bw (GModule module, uint32_t key);
uint64_t ht_get_cumts (GModule module, uint32_t key);
uint64_t ht_get_maxts (GModule module, uint32_t key);
uint64_t ht_get_meta_data (GModule module, const char *key);
uint64_t ht_sum_bw (void);
uint8_t ht_insert_meth_proto (const char *key);
void destroy_date_stores (int date);
void free_storage (void);
void ht_get_bw_min_max (GModule module, uint64_t * min, uint64_t * max);
void ht_get_cumts_min_max (GModule module, uint64_t * min, uint64_t * max);
void ht_get_hits_min_max (GModule module, uint32_t * min, uint32_t * max);
void ht_get_maxts_min_max (GModule module, uint64_t * min, uint64_t * max);
void ht_get_visitors_min_max (GModule module, uint32_t * min, uint32_t * max);
void init_pre_storage (Logs *logs);
void init_storage (void);
void u64decode (uint64_t n, uint32_t * x, uint32_t * y);

int ins_iglp (khash_t (iglp) * hash, uint32_t key, GLastParse lp);
int ins_igsl (khash_t (igsl) * hash, uint32_t key, uint32_t value);
int ins_ii08 (khash_t (ii08) * hash, uint32_t key, uint8_t value);
int ins_ii32 (khash_t (ii32) * hash, uint32_t key, uint32_t value);
int ins_is32 (khash_t (is32) * hash, uint32_t key, char *value);
int ins_iu64 (khash_t (iu64) * hash, uint32_t key, uint64_t value);
int ins_si08 (khash_t (si08) * hash, const char *key, uint8_t value);
int ins_si32 (khash_t (si32) * hash, const char *key, uint32_t value);
int ins_su64 (khash_t (su64) * hash, const char *key, uint64_t value);
int ins_u648 (khash_t (u648) * hash, uint64_t key, uint8_t value);
void *get_db_instance (uint32_t key);
void * get_hash (int module, uint64_t key, GSMetric metric);
void *get_hdb (GKDB * db, GAMetric mtrc);

GLastParse ht_get_last_parse (uint32_t key);
GRawData *parse_raw_data (GModule module);
GSLList *ht_get_host_agent_list (GModule module, uint32_t key);
GSLList *ht_get_keymap_list_from_key (GModule module, uint32_t key);
Logs *get_db_logs(uint32_t instance);
/* *INDENT-ON* */

#endif // for #ifndef GKHASH_H
