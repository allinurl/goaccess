/**
 * output.c -- output json to the standard output stream
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

#include "json.h"

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabdb.h"
#else
#include "gkhash.h"
#endif

#include "settings.h"
#include "ui.h"
#include "util.h"

typedef struct GPanel_
{
  GModule module;
  void (*render) (FILE * fp, GHolder * h, GPercTotals totals);
} GPanel;

static int nlines = 1;          /* number of new lines (applicable fields) */
static void print_json_data (FILE * fp, GHolder * h, GPercTotals totals);
static void print_json_host_data (FILE * fp, GHolder * h, GPercTotals totals);

static GPanel paneling[] = {
  {VISITORS, print_json_data},
  {REQUESTS, print_json_data},
  {REQUESTS_STATIC, print_json_data},
  {NOT_FOUND, print_json_data},
  {HOSTS, print_json_host_data},
  {OS, print_json_data},
  {BROWSERS, print_json_data},
  {VISIT_TIMES, print_json_data},
  {VIRTUAL_HOSTS, print_json_data},
  {REFERRERS, print_json_data},
  {REFERRING_SITES, print_json_data},
  {KEYPHRASES, print_json_data},
#ifdef HAVE_LIBGEOIP
  {GEO_LOCATION, print_json_data},
#endif
  {STATUS_CODES, print_json_data},
};

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

static void
escape_json_output (FILE * fp, char *s)
{
  while (*s) {
    switch (*s) {
    case '"':
      fprintf (fp, "\\\"");
      break;
    case '\\':
      fprintf (fp, "\\\\");
      break;
    case '\b':
      fprintf (fp, "\\\b");
      break;
    case '\f':
      fprintf (fp, "\\\f");
      break;
    case '\n':
      fprintf (fp, "\\\n");
      break;
    case '\r':
      fprintf (fp, "\\\r");
      break;
    case '\t':
      fprintf (fp, "\\\t");
      break;
    case '/':
      fprintf (fp, "\\/");
      break;
    default:
      if ((uint8_t) * s <= 0x1f) {
        /* Control characters (U+0000 through U+001F) */
        char buf[8];
        snprintf (buf, sizeof buf, "\\u%04x", *s);
        fprintf (fp, "%s", buf);
      } else if ((uint8_t) * s == 0xe2 && (uint8_t) * (s + 1) == 0x80 &&
                 (uint8_t) * (s + 2) == 0xa8) {
        /* Line separator (U+2028) */
        fprintf (fp, "\\u2028");
        s += 2;
      } else if ((uint8_t) * s == 0xe2 && (uint8_t) * (s + 1) == 0x80 &&
                 (uint8_t) * (s + 2) == 0xa9) {
        /* Paragraph separator (U+2019) */
        fprintf (fp, "\\u2029");
        s += 2;
      } else {
        fputc (*s, fp);
      }
      break;
    }
    s++;
  }
}

static void
pjson (FILE * fp, const char *fmt, ...)
{
  va_list args;

  va_start (args, fmt);
  vfprintf (fp, fmt, args);
  va_end (args);
}

static void
poverall_datetime (FILE * fp, int isp)
{
  char now[DATE_TIME];

  generate_time ();
  strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S", now_tm);

  pjson (fp, "%.*s\"%s\": \"%s\",%.*s", isp, TAB, OVERALL_DATETIME, now, nlines,
         NL);
}

static void
poverall_requests (FILE * fp, GLog * logger, int isp)
{
  int total = logger->processed;
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_REQ, total, nlines, NL);
}

static void
poverall_valid_reqs (FILE * fp, GLog * logger, int isp)
{
  int total = logger->valid;
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_VALID, total, nlines, NL);
}

static void
poverall_invalid_reqs (FILE * fp, GLog * logger, int isp)
{
  int total = logger->invalid;
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_FAILED, total, nlines,
         NL);
}

static void
poverall_processed_time (FILE * fp, int isp)
{
  long long t = (long long) end_proc - start_proc;
  pjson (fp, "%.*s\"%s\": %lld,%.*s", isp, TAB, OVERALL_GENTIME, t, nlines, NL);
}

static void
poverall_visitors (FILE * fp, int isp)
{
  int total = ht_get_size_uniqmap (VISITORS);
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_VISITORS, total, nlines,
         NL);
}

static void
poverall_files (FILE * fp, int isp)
{
  int total = ht_get_size_datamap (REQUESTS);
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_FILES, total, nlines, NL);
}

static void
poverall_excluded (FILE * fp, GLog * logger, int isp)
{
  int total = logger->excluded_ip;
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_EXCL_HITS, total, nlines,
         NL);
}

static void
poverall_refs (FILE * fp, int isp)
{
  int total = ht_get_size_datamap (REFERRERS);
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_REF, total, nlines, NL);
}

static void
poverall_notfound (FILE * fp, int isp)
{
  int total = ht_get_size_datamap (NOT_FOUND);
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_NOTFOUND, total, nlines,
         NL);
}

static void
poverall_static_files (FILE * fp, int isp)
{
  int total = ht_get_size_datamap (REQUESTS_STATIC);
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_STATIC, total, nlines,
         NL);
}

static void
poverall_log_size (FILE * fp, GLog * logger, int isp)
{
  off_t log_size = 0;

  if (!logger->piping && conf.ifile)
    log_size = file_size (conf.ifile);
  pjson (fp, "%.*s\"%s\": %jd,%.*s", isp, TAB, OVERALL_LOGSIZE,
         (intmax_t) log_size, nlines, NL);
}

static void
poverall_bandwidth (FILE * fp, GLog * logger, int isp)
{
  pjson (fp, "%.*s\"%s\": %llu,%.*s", isp, TAB, OVERALL_BANDWIDTH,
         logger->resp_size, nlines, NL);
}

static void
poverall_log (FILE * fp, int isp)
{
  if (conf.ifile == NULL)
    conf.ifile = (char *) "STDIN";
  pjson (fp, "%.*s\"%s\": \"", isp, TAB, OVERALL_LOG);
  escape_json_output (fp, conf.ifile);
  pjson (fp, "\"%.*s", nlines, NL);
}

static void
phits (FILE * fp, GMetrics * nmetrics, int sp)
{
  int isp = sp + 1;

  pjson (fp, "%.*s\"hits\": {%.*s", sp, TAB, nlines, NL);
  /* print hits */
  pjson (fp, "%.*s\"count\": %d,%.*s", isp, TAB, nmetrics->hits, nlines, NL);
  /* print hits percent */
  pjson (fp, "%.*s\"percent\": %4.2f%.*s", isp, TAB, nmetrics->hits_perc,
         nlines, NL);
  pjson (fp, "%.*s},%.*s", sp, TAB, nlines, NL);
}

static void
pvisitors (FILE * fp, GMetrics * nmetrics, int sp)
{
  int isp = sp + 1;

  pjson (fp, "%.*s\"visitors\": {%.*s", sp, TAB, nlines, NL);
  /* print visitors */
  pjson (fp, "%.*s\"count\": %d,%.*s", isp, TAB, nmetrics->visitors, nlines,
         NL);
  /* print visitors percent */
  pjson (fp, "%.*s\"percent\": %4.2f%.*s", isp, TAB, nmetrics->visitors_perc,
         nlines, NL);
  pjson (fp, "%.*s},%.*s", sp, TAB, nlines, NL);
}

static void
pbw (FILE * fp, GMetrics * nmetrics, int sp)
{
  int isp = sp + 1;

  if (!conf.bandwidth)
    return;

  pjson (fp, "%.*s\"bytes\": {%.*s", sp, TAB, nlines, NL);
  /* print bandwidth */
  pjson (fp, "%.*s\"count\": %d,%.*s", isp, TAB, nmetrics->bw.nbw, nlines, NL);
  /* print bandwidth percent */
  pjson (fp, "%.*s\"percent\": %4.2f%.*s", isp, TAB, nmetrics->bw_perc, nlines,
         NL);
  pjson (fp, "%.*s},%.*s", sp, TAB, nlines, NL);
}

static void
pavgts (FILE * fp, GMetrics * nmetrics, int sp)
{
  if (!conf.serve_usecs)
    return;
  pjson (fp, "%.*s\"avgts\": %lld,%.*s", sp, TAB,
         (long long) nmetrics->avgts.nts, nlines, NL);
}

static void
pcumts (FILE * fp, GMetrics * nmetrics, int sp)
{
  if (!conf.serve_usecs)
    return;
  pjson (fp, "%.*s\"cumts\": %lld,%.*s", sp, TAB,
         (long long) nmetrics->cumts.nts, nlines, NL);
}

static void
pmaxts (FILE * fp, GMetrics * nmetrics, int sp)
{
  if (!conf.serve_usecs)
    return;
  pjson (fp, "%.*s\"maxts\": %lld,%.*s", sp, TAB,
         (long long) nmetrics->maxts.nts, nlines, NL);
}

static void
pmethod (FILE * fp, GMetrics * nmetrics, int sp)
{
  /* request method */
  if (conf.append_method && nmetrics->method) {
    pjson (fp, "%.*s\"method\": \"%s\",%.*s", sp, TAB, nmetrics->method, nlines,
           NL);
  }
}

static void
pprotocol (FILE * fp, GMetrics * nmetrics, int sp)
{
  /* request protocol */
  if (conf.append_protocol && nmetrics->protocol) {
    pjson (fp, "%.*s\"protocol\": \"%s\",%.*s", sp, TAB, nmetrics->protocol,
           nlines, NL);
  }
}

static void
print_json_block (FILE * fp, GMetrics * nmetrics, int sp)
{
  /* print hits */
  phits (fp, nmetrics, sp);
  /* print visitors */
  pvisitors (fp, nmetrics, sp);
  /* print bandwidth */
  pbw (fp, nmetrics, sp);
  /* print time served metrics */
  pavgts (fp, nmetrics, sp);
  pcumts (fp, nmetrics, sp);
  pmaxts (fp, nmetrics, sp);
  /* print protocol/method */
  pmethod (fp, nmetrics, sp);
  pprotocol (fp, nmetrics, sp);
  /* data metric */
  pjson (fp, "%.*s\"data\": \"", sp, TAB);
  escape_json_output (fp, nmetrics->data);
  pjson (fp, "\"");
}

static void
print_json_host_geo (FILE * fp, GSubList * sub_list, int sp)
{
  int i;
  GSubItem *iter;
  static const char *key[] = {
    "country",
    "city",
    "hostname",
  };

  if (sub_list == NULL)
    return;

  pjson (fp, ",%.*s", nlines, NL);

  /* Iterate over child properties (country, city, etc) and print them out */
  for (i = 0, iter = sub_list->head; iter; iter = iter->next, i++) {
    pjson (fp, "%.*s\"%s\": \"", sp, TAB, key[iter->metrics->id]);
    escape_json_output (fp, iter->metrics->data);
    pjson (fp, (i != sub_list->size - 1) ? "\",%.*s" : "\"", nlines, NL);
  }
}

static void
print_json_host_data (FILE * fp, GHolder * h, GPercTotals totals)
{
  GMetrics *nmetrics;
  int i;
  int sp = 1, isp = 2, iisp = 3;

  pjson (fp, "%.*s\"%s\": [%.*s", sp, TAB, module_to_id (h->module), nlines,
         NL);
  for (i = 0; i < h->idx; i++) {
    set_data_metrics (h->items[i].metrics, &nmetrics, totals);

    pjson (fp, "%.*s{%.*s", isp, TAB, nlines, NL);
    print_json_block (fp, nmetrics, iisp);
    print_json_host_geo (fp, h->items[i].sub_list, iisp);
    pjson (fp, (i != h->idx - 1) ? "%.*s%.*s},%.*s" : "%.*s%.*s}%.*s", nlines,
           NL, isp, TAB, nlines, NL);

    free (nmetrics);
  }
  pjson (fp, "%.*s]", sp, TAB);
}

static void
print_json_sub_items (FILE * fp, GHolder * h, int idx, int iisp,
                      GPercTotals totals)
{
  GMetrics *nmetrics;
  GSubItem *iter;
  GSubList *sub_list = h->items[idx].sub_list;
  int i = 0;
  int iiisp = iisp + 1, iiiisp = iiisp + 1;

  if (sub_list == NULL)
    return;

  pjson (fp, ",%.*s%.*s\"items\": [%.*s", nlines, NL, iisp, TAB, nlines, NL);
  for (iter = sub_list->head; iter; iter = iter->next, i++) {
    set_data_metrics (iter->metrics, &nmetrics, totals);

    pjson (fp, "%.*s{%.*s", iiisp, TAB, nlines, NL);
    print_json_block (fp, nmetrics, iiiisp);
    pjson (fp, (i != sub_list->size - 1) ? "%.*s%.*s},%.*s" : "%.*s%.*s}%.*s",
           nlines, NL, iiisp, TAB, nlines, NL);
    free (nmetrics);
  }
  pjson (fp, "%.*s]", iisp, TAB);
}

static void
print_json_data (FILE * fp, GHolder * h, GPercTotals totals)
{
  GMetrics *nmetrics;
  int i;
  int sp = 1, isp = 2, iisp = 3;

  pjson (fp, "%.*s\"%s\": [%.*s", sp, TAB, module_to_id (h->module), nlines,
         NL);
  for (i = 0; i < h->idx; i++) {
    set_data_metrics (h->items[i].metrics, &nmetrics, totals);

    pjson (fp, "%.*s{%.*s", isp, TAB, nlines, NL);
    print_json_block (fp, nmetrics, iisp);

    if (h->sub_items_size)
      print_json_sub_items (fp, h, i, iisp, totals);

    pjson (fp, (i != h->idx - 1) ? "%.*s%.*s},%.*s" : "%.*s%.*s}%.*s", nlines,
           NL, isp, TAB, nlines, NL);

    free (nmetrics);
  }
  pjson (fp, "%.*s]", sp, TAB);
}

static void
print_json_summary (FILE * fp, GLog * logger)
{
  int sp = 1, isp = 2;

  pjson (fp, "%.*s\"%s\": {%.*s", sp, TAB, GENER_ID, nlines, NL);
  /* generated date time */
  poverall_datetime (fp, isp);
  /* total requests */
  poverall_requests (fp, logger, isp);
  /* valid requests */
  poverall_valid_reqs (fp, logger, isp);
  /* invalid requests */
  poverall_invalid_reqs (fp, logger, isp);
  /* generated time */
  poverall_processed_time (fp, isp);
  /* visitors */
  poverall_visitors (fp, isp);
  /* files */
  poverall_files (fp, isp);
  /* excluded hits */
  poverall_excluded (fp, logger, isp);
  /* referrers */
  poverall_refs (fp, isp);
  /* not found */
  poverall_notfound (fp, isp);
  /* static files */
  poverall_static_files (fp, isp);
  /* log size */
  poverall_log_size (fp, logger, isp);
  /* bandwidth */
  poverall_bandwidth (fp, logger, isp);
  /* log path */
  poverall_log (fp, isp);
  pjson (fp, "%.*s},%.*s", sp, TAB, nlines, NL);
}

/* entry point to generate a a json report writing it to the fp */
/* follow the JSON style similar to http://developer.github.com/v3/ */
void
output_json (GLog * logger, GHolder * holder)
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

  pjson (fp, "{%.*s", nlines, NL);
  print_json_summary (fp, logger);

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    if (!(panel = panel_lookup (module)))
      continue;

    panel->render (fp, holder + module, totals);
    pjson (fp, (module != TOTAL_MODULES - 1) ? ",%.*s" : "%.*s", nlines, NL);
  }

  fprintf (fp, "}");

  fclose (fp);
}
