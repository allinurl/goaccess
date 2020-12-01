/**
 * gstorage.c -- common storage handling
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

#include <stdio.h>
#if !defined __SUNPRO_C
#include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "gkhash.h"
#include "gstorage.h"
#include "commons.h"
#include "error.h"
#include "util.h"
#include "xmalloc.h"

/* Allocate memory for a new GMetrics instance.
 *
 * On success, the newly allocated GMetrics is returned . */
GMetrics *
new_gmetrics (void) {
  GMetrics *metrics = xcalloc (1, sizeof (GMetrics));

  return metrics;
}

/* Free memory of a GMetrics object */
void
free_gmetrics (GMetrics * metric) {
  if (metric == NULL)
    return;

  free (metric->data);
  free (metric->method);
  free (metric->protocol);
  free (metric);
}

/* Get the module string value given a metric enum value.
 *
 * On error, NULL is returned.
 * On success, the string module value is returned. */
char *
get_mtr_str (GSMetric metric) {
  /* String modules to enumerated modules */
  GEnum enum_metrics[] = {
    {"MTRC_KEYMAP", MTRC_KEYMAP},
    {"MTRC_ROOTMAP", MTRC_ROOTMAP},
    {"MTRC_DATAMAP", MTRC_DATAMAP},
    {"MTRC_UNIQMAP", MTRC_UNIQMAP},
    {"MTRC_ROOT", MTRC_ROOT},
    {"MTRC_HITS", MTRC_HITS},
    {"MTRC_VISITORS", MTRC_VISITORS},
    {"MTRC_BW", MTRC_BW},
    {"MTRC_CUMTS", MTRC_CUMTS},
    {"MTRC_MAXTS", MTRC_MAXTS},
    {"MTRC_METHODS", MTRC_METHODS},
    {"MTRC_PROTOCOLS", MTRC_PROTOCOLS},
    {"MTRC_AGENTS", MTRC_AGENTS},
    {"MTRC_METADATA", MTRC_METADATA},
    {"MTRC_UNIQUE_KEYS", MTRC_UNIQUE_KEYS},
    {"MTRC_AGENT_KEYS", MTRC_AGENT_KEYS},
    {"MTRC_AGENT_VALS", MTRC_AGENT_VALS},
    {"MTRC_CNT_VALID", MTRC_CNT_VALID},
    {"MTRC_CNT_BW", MTRC_CNT_BW},
  };
  return enum2str (enum_metrics, ARRAY_SIZE (enum_metrics), metric);
}

/* Allocate space off the heap to store a uint32_t.
 *
 * On success, the newly allocated pointer is returned . */
uint32_t *
i322ptr (uint32_t val) {
  uint32_t *ptr = xmalloc (sizeof (uint32_t));
  *ptr = val;

  return ptr;
}

/* Allocate space off the heap to store a uint64_t.
 *
 * On success, the newly allocated pointer is returned . */
uint64_t *
uint642ptr (uint64_t val) {
  uint64_t *ptr = xmalloc (sizeof (uint64_t));
  *ptr = val;

  return ptr;
}

/* Set the module totals to calculate percentages. */
void
set_module_totals (GPercTotals * totals) {
  totals->bw = ht_sum_bw ();
  totals->hits = ht_sum_valid ();
  totals->visitors = ht_get_size_uniqmap (VISITORS);
}

/* Set numeric metrics for each request given raw data.
 *
 * On success, numeric metrics are set into the given structure. */
void
set_data_metrics (GMetrics * ometrics, GMetrics ** nmetrics, GPercTotals totals) {
  GMetrics *metrics;

  /* determine percentages for certain fields */
  float hits_perc = get_percentage (totals.hits, ometrics->hits);
  float visitors_perc = get_percentage (totals.visitors, ometrics->visitors);
  float bw_perc = get_percentage (totals.bw, ometrics->bw.nbw);

  metrics = new_gmetrics ();

  /* basic fields */
  metrics->id = ometrics->id;
  metrics->hits = ometrics->hits;
  metrics->visitors = ometrics->visitors;

  /* percentage fields */
  metrics->hits_perc = hits_perc < 0 ? 0 : hits_perc;
  metrics->bw_perc = bw_perc < 0 ? 0 : bw_perc;
  metrics->visitors_perc = visitors_perc < 0 ? 0 : visitors_perc;

  /* bandwidth field */
  metrics->bw.nbw = ometrics->bw.nbw;

  /* time served fields */
  if (conf.serve_usecs && ometrics->hits > 0) {
    metrics->avgts.nts = ometrics->avgts.nts;
    metrics->cumts.nts = ometrics->cumts.nts;
    metrics->maxts.nts = ometrics->maxts.nts;
  }

  /* method field */
  if (conf.append_method && ometrics->method)
    metrics->method = ometrics->method;

  /* protocol field */
  if (conf.append_protocol && ometrics->protocol)
    metrics->protocol = ometrics->protocol;

  /* data field */
  metrics->data = ometrics->data;

  *nmetrics = metrics;
}
