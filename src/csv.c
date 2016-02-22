/**
 * output.c -- output csv to the standard output stream
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

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>

#include "csv.h"

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabdb.h"
#else
#include "gkhash.h"
#endif

#include "ui.h"
#include "util.h"

/* Panel output */
typedef struct GPanel_
{
  GModule module;
  void (*render) (FILE * fp, GHolder * h, GPercTotals totals);
} GPanel;

static void print_csv_data (FILE * fp, GHolder * h, GPercTotals totals);

/* A function pointer for each panel */
static GPanel paneling[] = {
  {VISITORS, print_csv_data},
  {REQUESTS, print_csv_data},
  {REQUESTS_STATIC, print_csv_data},
  {NOT_FOUND, print_csv_data},
  {HOSTS, print_csv_data},
  {OS, print_csv_data},
  {BROWSERS, print_csv_data},
  {VISIT_TIMES, print_csv_data},
  {VIRTUAL_HOSTS, print_csv_data},
  {REFERRERS, print_csv_data},
  {REFERRING_SITES, print_csv_data},
  {KEYPHRASES, print_csv_data},
#ifdef HAVE_LIBGEOIP
  {GEO_LOCATION, print_csv_data},
#endif
  {STATUS_CODES, print_csv_data},
};

/* Get a panel from the GPanel structure given a module.
 *
 * On error, or if not found, NULL is returned.
 * On success, the panel value is returned. */
static GPanel *
panel_lookup (GModule module)
{
  int i, num_panels = ARRAY_SIZE (paneling);

  for (i = 0; i < num_panels; i++) {
    if (paneling[i].module == module)
      return &paneling[i];
  }
  return NULL;
}

/* Iterate over the string and escape CSV output. */
static void
escape_cvs_output (FILE * fp, char *s)
{
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
print_csv_metric_block (FILE * fp, GMetrics * nmetrics)
{
  /* basic metrics */
  fprintf (fp, "\"%d\",", nmetrics->hits);
  fprintf (fp, "\"%4.2f%%\",", nmetrics->hits_perc);
  fprintf (fp, "\"%d\",", nmetrics->visitors);
  fprintf (fp, "\"%4.2f%%\",", nmetrics->visitors_perc);

  /* bandwidth */
  if (conf.bandwidth) {
    fprintf (fp, "\"%lld\",", (long long) nmetrics->bw.nbw);
    fprintf (fp, "\"%4.2f%%\",", nmetrics->bw_perc);
  }

  /* time served metrics */
  if (conf.serve_usecs) {
    fprintf (fp, "\"%lld\",", (long long) nmetrics->avgts.nts);
    fprintf (fp, "\"%lld\",", (long long) nmetrics->cumts.nts);
    fprintf (fp, "\"%lld\",", (long long) nmetrics->maxts.nts);
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
print_csv_sub_items (FILE * fp, GHolder * h, int idx, GPercTotals totals)
{
  GMetrics *nmetrics;
  GSubList *sub_list = h->items[idx].sub_list;
  GSubItem *iter;

  int i = 0;

  if (sub_list == NULL)
    return;

  for (iter = sub_list->head; iter; iter = iter->next, i++) {
    set_data_metrics (iter->metrics, &nmetrics, totals);

    fprintf (fp, "\"%d\",", i); /* idx */
    fprintf (fp, "\"%d\",", idx);       /* parent idx */
    fprintf (fp, "\"%s\",", module_to_id (h->module));

    /* output metrics */
    print_csv_metric_block (fp, nmetrics);
    free (nmetrics);
  }
}

/* Output first-level items.
 *
 * On success, outputs item value. */
static void
print_csv_data (FILE * fp, GHolder * h, GPercTotals totals)
{
  GMetrics *nmetrics;
  int i;

  for (i = 0; i < h->idx; i++) {
    set_data_metrics (h->items[i].metrics, &nmetrics, totals);

    fprintf (fp, "\"%d\",", i); /* idx */
    fprintf (fp, ",");  /* no parent */
    fprintf (fp, "\"%s\",", module_to_id (h->module));

    /* output metrics */
    print_csv_metric_block (fp, nmetrics);

    if (h->sub_items_size)
      print_csv_sub_items (fp, h, i, totals);

    free (nmetrics);
  }
}

#pragma GCC diagnostic ignored "-Wformat-nonliteral"
/* Output general statistics information. */
static void
print_csv_summary (FILE * fp, GLog * logger)
{
  long long t = 0LL;
  int i = 0, total = 0;
  off_t log_size = 0;
  char now[DATE_TIME];
  const char *fmt;

  generate_time ();
  strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S", now_tm);

  /* generated date time */
  fmt = "\"%d\",,\"%s\",,,,,,,,\"%s\",\"%s\"\r\n";
  fprintf (fp, fmt, i++, GENER_ID, now, OVERALL_DATETIME);

  /* total requests */
  fmt = "\"%d\",,\"%s\",,,,,,,,\"%d\",\"%s\"\r\n";
  total = logger->processed;
  fprintf (fp, fmt, i++, GENER_ID, total, OVERALL_REQ);

  /* valid requests */
  fmt = "\"%d\",,\"%s\",,,,,,,,\"%d\",\"%s\"\r\n";
  total = logger->valid;
  fprintf (fp, fmt, i++, GENER_ID, total, OVERALL_VALID);

  /* invalid requests */
  total = logger->invalid;
  fprintf (fp, fmt, i++, GENER_ID, total, OVERALL_FAILED);

  /* generated time */
  fmt = "\"%d\",,\"%s\",,,,,,,,\"%llu\",\"%s\"\r\n";
  t = (long long) end_proc - start_proc;
  fprintf (fp, fmt, i++, GENER_ID, t, OVERALL_GENTIME);

  /* visitors */
  fmt = "\"%d\",,\"%s\",,,,,,,,\"%d\",\"%s\"\r\n";
  total = ht_get_size_uniqmap (VISITORS);
  fprintf (fp, fmt, i++, GENER_ID, total, OVERALL_VISITORS);

  /* files */
  total = ht_get_size_datamap (REQUESTS);
  fprintf (fp, fmt, i++, GENER_ID, total, OVERALL_FILES);

  /* excluded hits */
  total = logger->excluded_ip;
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
  if (!logger->piping && conf.ifile)
    log_size = file_size (conf.ifile);
  fmt = "\"%d\",,\"%s\",,,,,,,,\"%jd\",\"%s\"\r\n";
  fprintf (fp, fmt, i++, GENER_ID, (intmax_t) log_size, OVERALL_LOGSIZE);

  /* bandwidth */
  fmt = "\"%d\",,\"%s\",,,,,,,,\"%lld\",\"%s\"\r\n";
  fprintf (fp, fmt, i++, GENER_ID, logger->resp_size, OVERALL_BANDWIDTH);

  /* log path */
  if (conf.ifile == NULL)
    conf.ifile = (char *) "STDIN";

  fmt = "\"%d\",,\"%s\",,,,,,,,\"%s\",\"%s\"\r\n";
  fprintf (fp, fmt, i++, GENER_ID, conf.ifile, OVERALL_LOG);
}

#pragma GCC diagnostic warning "-Wformat-nonliteral"

/* Entry point to generate a a csv report writing it to the fp */
void
output_csv (GLog * logger, GHolder * holder)
{
  GModule module;
  FILE *fp = stdout;
  const GPanel *panel = NULL;
  size_t idx = 0;

  GPercTotals totals = {
    .hits = logger->valid,
    .visitors = ht_get_size_uniqmap (VISITORS),
    .bw = logger->resp_size,
  };

  if (!conf.no_csv_summary)
    print_csv_summary (fp, logger);

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    if (!(panel = panel_lookup (module)))
      continue;
    panel->render (fp, holder + module, totals);
  }

  fclose (fp);
}
