/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2024 Gerardo Orellana <hello @ goaccess.io>
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

#include "gkmhash.h"

#define DB_VERSION  2
#define DB_INSTANCE 1

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
/* uint64_t key            , GLastParse payload */
KHASH_MAP_INIT_INT64 (iglp   , GLastParse);
/* uint32_t keys           , GSLList payload */
KHASH_MAP_INIT_INT (igsl   , GSLList *);
/* string keys             , uint64_t payload */
KHASH_MAP_INIT_STR (su64   , uint64_t);
/* uint64_t key            , uint8_t payload */
KHASH_MAP_INIT_INT64 (u648 , uint8_t);
/* *INDENT-ON* */

/* Whole App Data store */
typedef struct GKHashDB_ {
  GKHashMetric metrics[GAMTRC_TOTAL];
} GKHashDB;

/* DB */
struct GKDB_ {
  GKHashDB *hdb;                /* app-level hash tables */
  Logs *logs;                   /* logs parsing per db instance */
  GKHashModule *cache;          /* cache modules */
  GKHashStorage *store;         /* per date OR module */
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

extern const GKHashMetric app_metrics[];
extern const size_t app_metrics_len;

/* *INDENT-OFF* */
void *new_igsl_ht (void);
void *new_ii08_ht (void);
void *new_ii32_ht (void);
void *new_is32_ht (void);
void *new_iu64_ht (void);
void *new_si32_ht (void);
void *new_su64_ht (void);
void *new_u648_ht (void);

void del_igsl_free (void *h, uint8_t free_data);
void del_ii08 (void *h, GO_UNUSED uint8_t free_data);
void del_ii32 (void *h, GO_UNUSED uint8_t free_data);
void del_is32_free (void *h, uint8_t free_data);
void del_iu64 (void *h, GO_UNUSED uint8_t free_data);
void del_si32_free (void *h, uint8_t free_data);
void del_su64_free (void *h, uint8_t free_data);
void del_u648 (void *h, GO_UNUSED uint8_t free_data);
void des_igsl_free (void *h, uint8_t free_data);
void des_ii08 (void *h, GO_UNUSED uint8_t free_data);
void des_ii32 (void *h, GO_UNUSED uint8_t free_data);
void des_is32_free (void *h, uint8_t free_data);
void des_iu64 (void *h, GO_UNUSED uint8_t free_data);
void des_si32_free (void *h, uint8_t free_data);
void des_su64_free (void *h, uint8_t free_data);
void des_u648 (void *h, GO_UNUSED uint8_t free_data);

int inc_iu64 (khash_t (iu64) * hash, uint32_t key, uint64_t inc);
int inc_su64 (khash_t (su64) * hash, const char *key, uint64_t inc);
int ins_iglp (khash_t (iglp) * hash, uint64_t key, GLastParse lp);
int ins_igsl (khash_t (igsl) * hash, uint32_t key, uint32_t value);
int ins_ii08 (khash_t (ii08) * hash, uint32_t key, uint8_t value);
int ins_ii32 (khash_t (ii32) * hash, uint32_t key, uint32_t value);
int ins_is32 (khash_t (is32) * hash, uint32_t key, char *value);
int ins_iu64 (khash_t (iu64) * hash, uint32_t key, uint64_t value);
int ins_si08 (khash_t (si08) * hash, const char *key, uint8_t value);
int ins_si32 (khash_t (si32) * hash, const char *key, uint32_t value);
int ins_su64 (khash_t (su64) * hash, const char *key, uint64_t value);
int ins_u648 (khash_t (u648) * hash, uint64_t key, uint8_t value);
uint32_t inc_ii32 (khash_t (ii32) * hash, uint32_t key, uint32_t inc);
uint32_t ins_ii32_ai (khash_t (ii32) * hash, uint32_t key);
uint32_t ins_ii32_inc (khash_t (ii32) * hash, uint32_t key, uint32_t (*cb) (khash_t (si32) *, const char *), khash_t (si32) * seqs, const char *seqk);
uint32_t ins_si32_inc (khash_t (si32) * hash, const char *key, uint32_t (*cb) (khash_t (si32) *, const char *), khash_t (si32) * seqs, const char *seqk);

char *get_is32 (khash_t (is32) * hash, uint32_t key);
uint32_t get_ii32 (khash_t (ii32) * hash, uint32_t key);
uint32_t get_si32 (khash_t (si32) * hash, const char *key);
uint64_t get_iu64 (khash_t (iu64) * hash, uint32_t key);
uint64_t get_su64 (khash_t (su64) * hash, const char *key);
uint8_t get_ii08 (khash_t (ii08) * hash, uint32_t key);
uint8_t get_si08 (khash_t (si08) * hash, const char *key);
void get_ii32_min_max (khash_t (ii32) * hash, uint32_t * min, uint32_t * max);
void get_iu64_min_max (khash_t (iu64) * hash, uint64_t * min, uint64_t * max);

int ht_insert_hostname (const char *ip, const char *host);
int ht_insert_json_logfmt (GO_UNUSED void *userdata, char *key, char *spec);
int ht_insert_last_parse (uint64_t key, GLastParse lp);
uint32_t ht_inc_cnt_overall (const char *key, uint32_t val);
uint32_t ht_ins_seq (khash_t (si32) * hash, const char *key);
uint8_t ht_insert_meth_proto (const char *key);

char *ht_get_hostname (const char *host);
char *ht_get_json_logfmt (const char *key);
uint32_t ht_get_excluded_ips (void);
uint32_t ht_get_invalid (void);
uint32_t ht_get_processed (void);
uint32_t ht_get_processing_time (void);
uint8_t get_method_proto (const char *value);
uint32_t *get_sorted_dates (uint32_t * len);

void free_storage (void);
void init_pre_storage (Logs *logs);

char *get_mtr_type_str (GSMetricType type);
void *get_db_instance (uint32_t key);
void *get_hdb (GKDB * db, GAMetric mtrc);

GLastParse ht_get_last_parse (uint64_t key);
Logs *get_db_logs(uint32_t instance);
/* *INDENT-ON* */

#endif // for #ifndef GKHASH_H
