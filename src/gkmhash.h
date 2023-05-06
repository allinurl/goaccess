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

#ifndef GKMHASH_H_INCLUDED
#define GKMHASH_H_INCLUDED

#include "gstorage.h"

typedef struct GKHashMetric_ GKHashMetric;

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
extern GKHashMetric module_metrics[];
extern const GKHashMetric global_metrics[];
extern size_t global_metrics_len;
extern size_t module_metrics_len;

char *ht_get_datamap (GModule module, uint32_t key);
char *ht_get_host_agent_val (uint32_t key);
char *ht_get_method (GModule module, uint32_t key);
char *ht_get_protocol (GModule module, uint32_t key);
char *ht_get_root (GModule module, uint32_t key);
uint32_t ht_get_hits (GModule module, int key);
uint32_t ht_get_keymap (GModule module, const char *key);
uint32_t ht_get_size_datamap (GModule module);
uint32_t ht_get_size_dates (void);
uint32_t ht_get_size_uniqmap (GModule module);
uint32_t ht_get_visitors (GModule module, uint32_t key);
uint32_t ht_sum_valid (void);
uint64_t ht_get_bw (GModule module, uint32_t key);
uint64_t ht_get_cumts (GModule module, uint32_t key);
uint64_t ht_get_maxts (GModule module, uint32_t key);
uint64_t ht_get_meta_data (GModule module, const char *key);
uint64_t ht_sum_bw (void);
void *get_hash (int module, uint64_t key, GSMetric metric);
void ht_get_bw_min_max (GModule module, uint64_t * min, uint64_t * max);
void ht_get_cumts_min_max (GModule module, uint64_t * min, uint64_t * max);
void ht_get_hits_min_max (GModule module, uint32_t * min, uint32_t * max);
void ht_get_maxts_min_max (GModule module, uint64_t * min, uint64_t * max);
void ht_get_visitors_min_max (GModule module, uint32_t * min, uint32_t * max);

int ht_inc_cnt_bw (uint32_t date, uint64_t inc);
int ht_insert_agent (GModule module, uint32_t date, uint32_t key, uint32_t value);
int ht_insert_agent_value (uint32_t date, uint32_t key, char *value);
int ht_insert_bw (GModule module, uint32_t date, uint32_t key, uint64_t inc, uint32_t ckey);
int ht_insert_cumts (GModule module, uint32_t date, uint32_t key, uint64_t inc, uint32_t ckey);
int ht_insert_datamap (GModule module, uint32_t date, uint32_t key, const char *value, uint32_t ckey);
int ht_insert_date (uint32_t key);
int ht_insert_maxts (GModule module, uint32_t date, uint32_t key, uint64_t value, uint32_t ckey);
int ht_insert_method (GModule module, uint32_t date, uint32_t key, const char *value, uint32_t ckey);
int ht_insert_protocol (GModule module, uint32_t date, uint32_t key, const char *value, uint32_t ckey);
int ht_insert_root (GModule module, uint32_t date, uint32_t key, uint32_t value, uint32_t dkey, uint32_t rkey);
int ht_insert_rootmap (GModule module, uint32_t date, uint32_t key, const char *value, uint32_t ckey);
int ht_insert_uniqmap (GModule module, uint32_t date, uint32_t key, uint32_t value);
uint32_t ht_inc_cnt_valid (uint32_t date, uint32_t inc);
uint32_t ht_insert_agent_key (uint32_t date, uint32_t key);
uint32_t ht_insert_hits (GModule module, uint32_t date, uint32_t key, uint32_t inc, uint32_t ckey);
uint32_t ht_insert_keymap (GModule module, uint32_t date, uint32_t key, uint32_t * ckey);
uint32_t ht_insert_unique_key (uint32_t date, const char *key);
uint32_t ht_insert_visitor (GModule module, uint32_t date, uint32_t key, uint32_t inc, uint32_t ckey);
int ht_insert_meta_data (GModule module, uint32_t date, const char *key, uint64_t value);

int invalidate_date (int date);
int rebuild_rawdata_cache (void);
void des_igkh (void *h);
void free_cache (GKHashModule * cache);
void init_storage (void);

GRawData *parse_raw_data (GModule module);
GSLList *ht_get_host_agent_list (GModule module, uint32_t key);
GSLList *ht_get_keymap_list_from_key (GModule module, uint32_t key);
/* *INDENT-ON* */

#endif // for #ifndef GKMHASH_H
