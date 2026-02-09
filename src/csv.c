/**
 * output.c -- output csv to the standard output stream
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2025 Gerardo Orellana <hello @ goaccess.io>
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

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <ctype.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include "csv.h"

#include "error.h"
#include "gkhash.h"
#include "ui.h"
#include "util.h"

struct tm now_tm;

/* Panel output */
typedef struct GPanel_ {
  GModule module;
  void (*render) (FILE * fp, GHolder * h, GPercTotals totals);
} GPanel;

static void print_csv_data (FILE * fp, GHolder * h, GPercTotals totals);

/* *INDENT-OFF* */
/* A function pointer for each panel */
static const GPanel paneling[] = {
  {VISITORS        , print_csv_data} ,
  {REQUESTS        , print_csv_data} ,
  {REQUESTS_STATIC , print_csv_data} ,
  {NOT_FOUND       , print_csv_data} ,
  {HOSTS           , print_csv_data} ,
  {OS              , print_csv_data} ,
  {BROWSERS        , print_csv_data} ,
  {VISIT_TIMES     , print_csv_data} ,
  {VIRTUAL_HOSTS   , print_csv_data} ,
  {REFERRERS       , print_csv_data} ,
  {REFERRING_SITES , print_csv_data} ,
  {KEYPHRASES      , print_csv_data} ,
  {STATUS_CODES    , print_csv_data} ,
  {REMOTE_USER     , print_csv_data} ,
  {CACHE_STATUS    , print_csv_data} ,
#ifdef HAVE_GEOLOCATION
  {GEO_LOCATION    , print_csv_data} ,
  {ASN             , print_csv_data} ,
#endif
  {MIME_TYPE       , print_csv_data} ,
  {TLS_TYPE        , print_csv_data} ,
};
/* *INDENT-ON* */

/* Get a panel from the GPanel structure given a module.
 *
 * On error, or if not found, NULL is returned.
 * On success, the panel value is returned. */
static const GPanel *
panel_lookup (GModule module) {
  int i, num_panels = ARRAY_SIZE (paneling);

  for (i = 0; i < num_panels; i++) {
    if (paneling[i].module == module)
      return &paneling[i];
  }
  return NULL;
}

/* Iterate over the string and escape CSV output. */
static void
escape_cvs_output (FILE *fp, char *s) {
  while (*s) {
    switch (*s) {
    case '"':
      fprintf (fp, "\"\"");
      break;
    default:
      fputc (*s, fp);
      break;
    }
    s++;
  }
}

/* Output metrics.
 *
 * On success, outputs item value. */
static void
print_csv_metric_block (FILE *fp, GMetrics *nmetrics) {
  /* basic metrics */
  fprintf (fp, "\"%" PRIu64 "\",", nmetrics->hits);
  fprintf (fp, "\"%4.2f%%\",", nmetrics->hits_perc);
  fprintf (fp, "\"%" PRIu64 "\",", nmetrics->visitors);
  fprintf (fp, "\"%4.2f%%\",", nmetrics->visitors_perc);

  /* bandwidth */
  if (conf.bandwidth) {
    fprintf (fp, "\"%" PRIu64 "\",", nmetrics->nbw);
    fprintf (fp, "\"%4.2f%%\",", nmetrics->bw_perc);
  }

  /* time served metrics */
  if (conf.serve_usecs) {
    fprintf (fp, "\"%" PRIu64 "\",", nmetrics->avgts.nts);
    fprintf (fp, "\"%" PRIu64 "\",", nmetrics->cumts.nts);
    fprintf (fp, "\"%" PRIu64 "\",", nmetrics->maxts.nts);
  }

  /* request method */
  if (conf.append_method && nmetrics->method)
    fprintf (fp, "\"%s\"", nmetrics->method);
  fprintf (fp, ",");

  /* request protocol */
  if (conf.append_protocol && nmetrics->protocol)
    fprintf (fp, "\"%s\"", nmetrics->protocol);
  fprintf (fp, ",");

  /* data field */
  fprintf (fp, "\"");
  escape_cvs_output (fp, nmetrics->data);
  fprintf (fp, "\"\r\n");
}

/* Output a sublist (double linked-list) items for a particular parent node.
 *
 * On error, it exits early.
 * On success, outputs item value. */
static void
print_csv_sub_items (FILE *fp, GSubList *sub_list, GModule module, int parent_idx,
                     GPercTotals totals, int depth) {
  GMetrics *nmetrics;
  GSubItem *iter;

  int i = 0;

  if (sub_list == NULL)
    return;

  for (iter = sub_list->head; iter; iter = iter->next, i++) {
    set_data_metrics (iter->metrics, &nmetrics, totals);

    fprintf (fp, "\"%d\",", i); /* idx */
    fprintf (fp, "\"%d\",", parent_idx); /* parent idx */
    fprintf (fp, "\"%s\",", module_to_id (module));
    fprintf (fp, "\"%d\",", depth); /* depth */

    /* output metrics */
    print_csv_metric_block (fp, nmetrics);

    /* recurse into nested sub-items */
    if (iter->sub_list != NULL)
      print_csv_sub_items (fp, iter->sub_list, module, i, totals, depth + 1);

    free (nmetrics);
  }
}

/* Output first-level items.
 *
 * On success, outputs item value. */
static void
print_csv_data (FILE *fp, GHolder *h, GPercTotals totals) {
  GMetrics *nmetrics;
  int i;

  for (i = 0; i < h->idx; i++) {
    set_data_metrics (h->items[i].metrics, &nmetrics, totals);

    fprintf (fp, "\"%d\",", i); /* idx */
    fprintf (fp, ","); /* no parent */
    fprintf (fp, "\"%s\",", module_to_id (h->module));

    /* output metrics */
    print_csv_metric_block (fp, nmetrics);

    if (h->sub_items_size)
      print_csv_sub_items (fp, h->items[i].sub_list, h->module, i, totals, 1);

    free (nmetrics);
  }
}

#pragma GCC diagnostic ignored "-Wformat-nonliteral"
/* Output general statistics information. */
static void
print_csv_summary (FILE *fp) {
  char now[DATE_TIME];
  char *source = NULL;
  const char *fmt;
  int i = 0;
  uint64_t total = 0;
  uint32_t t = 0;

  generate_time ();
  strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S %z", &now_tm);

  /* generated date time */
  fmt = "\"%d\",,\"%s\",,,,,,,,\"%s\",\"%s\"\r\n";
  fprintf (fp, fmt, i++, GENER_ID, now, OVERALL_DATETIME);

  /* total requests */
  fmt = "\"%d\",,\"%s\",,,,,,,,\"%" PRIu64 "\",\"%s\"\r\n";
  total = ht_get_processed ();
  fprintf (fp, fmt, i++, GENER_ID, total, OVERALL_REQ);

  /* valid requests */
  fmt = "\"%d\",,\"%s\",,,,,,,,\"%" PRIu64 "\",\"%s\"\r\n";
  total = ht_sum_valid ();
  fprintf (fp, fmt, i++, GENER_ID, total, OVERALL_VALID);

  /* invalid requests */
  total = ht_get_invalid ();
  fprintf (fp, fmt, i++, GENER_ID, total, OVERALL_FAILED);

  /* generated time */
  fmt = "\"%d\",,\"%s\",,,,,,,,\"%u\",\"%s\"\r\n";
  t = ht_get_processing_time ();
  fprintf (fp, fmt, i++, GENER_ID, t, OVERALL_GENTIME);

  /* visitors */
  fmt = "\"%d\",,\"%s\",,,,,,,,\"%" PRIu64 "\",\"%s\"\r\n";
  total = ht_get_size_uniqmap (VISITORS);
  fprintf (fp, fmt, i++, GENER_ID, total, OVERALL_VISITORS);

  /* files */
  total = ht_get_size_datamap (REQUESTS);
  fprintf (fp, fmt, i++, GENER_ID, total, OVERALL_FILES);

  /* excluded hits */
  total = ht_get_excluded_ips ();
  fprintf (fp, fmt, i++, GENER_ID, total, OVERALL_EXCL_HITS);

  /* referrers */
  total = ht_get_size_datamap (REFERRERS);
  fprintf (fp, fmt, i++, GENER_ID, total, OVERALL_REF);

  /* not found */
  total = ht_get_size_datamap (NOT_FOUND);
  fprintf (fp, fmt, i++, GENER_ID, total, OVERALL_NOTFOUND);

  /* static files */
  total = ht_get_size_datamap (REQUESTS_STATIC);
  fprintf (fp, fmt, i++, GENER_ID, total, OVERALL_STATIC);

  /* log size */
  fmt = "\"%d\",,\"%s\",,,,,,,,\"%jd\",\"%s\"\r\n";
  fprintf (fp, fmt, i++, GENER_ID, (intmax_t) get_log_sizes (), OVERALL_LOGSIZE);

  /* bandwidth */
  fmt = "\"%d\",,\"%s\",,,,,,,,\"%" PRIu64 "\",\"%s\"\r\n";
  fprintf (fp, fmt, i++, GENER_ID, ht_sum_bw (), OVERALL_BANDWIDTH);

  /* log path */
  source = get_log_source_str (0);
  fmt = "\"%d\",,\"%s\",,,,,,,,\"%s\",\"%s\"\r\n";
  fprintf (fp, fmt, i++, GENER_ID, source, OVERALL_LOG);
  free (source);
}

#pragma GCC diagnostic warning "-Wformat-nonliteral"

/* Entry point to generate a csv report writing it to the fp */
void
output_csv (GHolder *holder, const char *filename) {
  GModule module;
  GPercTotals totals;
  const GPanel *panel = NULL;
  size_t idx = 0;
  FILE *fp;

  fp = (filename != NULL) ? fopen (filename, "w") : stdout;
  if (!fp)
    FATAL ("Unable to open CSV file: %s.", strerror (errno));

  if (!conf.no_csv_summary)
    print_csv_summary (fp);

  set_module_totals (&totals);

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    if (!(panel = panel_lookup (module))) {
      continue;
    }

    panel->render (fp, holder + module, totals);
  }

  fclose (fp);
}
