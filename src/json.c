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
  void (*render) (FILE * fp, GHolder * h, GPercTotals totals,
                  const struct GPanel_ *);
  void (*subitems) (FILE * fp, GSubList * sl, GPercTotals totals, int iisp);
} GPanel;

/* number of new lines (applicable fields) */
static int nlines = 0;

static void print_json_data (FILE * fp, GHolder * h, GPercTotals totals,
                             const struct GPanel_ *);
static void print_json_sub_items (FILE * fp, GSubList * sl, GPercTotals totals,
                                  int iisp);
static void print_json_host_geo (FILE * fp, GSubList * sl, GPercTotals totals,
                                 int iisp);

/* *INDENT-OFF* */
static GPanel paneling[] = {
  {VISITORS            , print_json_data , NULL } ,
  {REQUESTS            , print_json_data , NULL } ,
  {REQUESTS_STATIC     , print_json_data , NULL } ,
  {NOT_FOUND           , print_json_data , NULL } ,
  {HOSTS               , print_json_data , print_json_host_geo  } ,
  {OS                  , print_json_data , print_json_sub_items } ,
  {BROWSERS            , print_json_data , print_json_sub_items } ,
  {VISIT_TIMES         , print_json_data , NULL } ,
  {VIRTUAL_HOSTS       , print_json_data , NULL } ,
  {REFERRERS           , print_json_data , NULL } ,
  {REFERRING_SITES     , print_json_data , NULL } ,
  {KEYPHRASES          , print_json_data , NULL } ,
#ifdef HAVE_LIBGEOIP
  {GEO_LOCATION        , print_json_data , print_json_sub_items } ,
#endif
  {STATUS_CODES        , print_json_data , print_json_sub_items } ,
};
/* *INDENT-ON* */

/* Get panel output data for the given module.
 *
 * If not found, NULL is returned.
 * On success, panel data is returned . */
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

/* Escape and output valid JSON.
 *
 * On success, escaped JSON data is outputted. */
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

/* A wrapper function to output a formatted string.
 *
 * On success, data is outputted. */
static void
pjson (FILE * fp, const char *fmt, ...)
{
  va_list args;

  va_start (args, fmt);
  vfprintf (fp, fmt, args);
  va_end (args);
}

/* Output date and time for the overall object.
 *
 * On success, data is outputted. */
static void
poverall_datetime (FILE * fp, int isp)
{
  char now[DATE_TIME];

  generate_time ();
  strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S", now_tm);

  pjson (fp, "%.*s\"%s\": \"%s\",%.*s", isp, TAB, OVERALL_DATETIME, now, nlines,
         NL);
}

/* Output date and time for the overall object.
 *
 * On success, data is outputted. */
static void
poverall_requests (FILE * fp, GLog * logger, int isp)
{
  int total = logger->processed;
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_REQ, total, nlines, NL);
}

/* Output the number of valid requests under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_valid_reqs (FILE * fp, GLog * logger, int isp)
{
  int total = logger->valid;
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_VALID, total, nlines, NL);
}

/* Output the number of invalid requests under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_invalid_reqs (FILE * fp, GLog * logger, int isp)
{
  int total = logger->invalid;
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_FAILED, total, nlines,
         NL);
}

/* Output the total processed time under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_processed_time (FILE * fp, int isp)
{
  long long t = (long long) end_proc - start_proc;
  pjson (fp, "%.*s\"%s\": %lld,%.*s", isp, TAB, OVERALL_GENTIME, t, nlines, NL);
}

/* Output the total number of unique visitors under the overall
 * object.
 *
 * On success, data is outputted. */
static void
poverall_visitors (FILE * fp, int isp)
{
  int total = ht_get_size_uniqmap (VISITORS);
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_VISITORS, total, nlines,
         NL);
}

/* Output the total number of unique files under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_files (FILE * fp, int isp)
{
  int total = ht_get_size_datamap (REQUESTS);
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_FILES, total, nlines, NL);
}

/* Output the total number of excluded requests under the overall
 * object.
 *
 * On success, data is outputted. */
static void
poverall_excluded (FILE * fp, GLog * logger, int isp)
{
  int total = logger->excluded_ip;
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_EXCL_HITS, total, nlines,
         NL);
}

/* Output the number of referrers under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_refs (FILE * fp, int isp)
{
  int total = ht_get_size_datamap (REFERRERS);
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_REF, total, nlines, NL);
}

/* Output the number of not found (404s) under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_notfound (FILE * fp, int isp)
{
  int total = ht_get_size_datamap (NOT_FOUND);
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_NOTFOUND, total, nlines,
         NL);
}

/* Output the number of static files (jpg, pdf, etc) under the overall
 * object.
 *
 * On success, data is outputted. */
static void
poverall_static_files (FILE * fp, int isp)
{
  int total = ht_get_size_datamap (REQUESTS_STATIC);
  pjson (fp, "%.*s\"%s\": %d,%.*s", isp, TAB, OVERALL_STATIC, total, nlines,
         NL);
}

/* Output the size of the log being parsed under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_log_size (FILE * fp, GLog * logger, int isp)
{
  off_t log_size = 0;

  if (!logger->piping && conf.ifile)
    log_size = file_size (conf.ifile);
  pjson (fp, "%.*s\"%s\": %jd,%.*s", isp, TAB, OVERALL_LOGSIZE,
         (intmax_t) log_size, nlines, NL);
}

/* Output the total bandwidth consumed under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_bandwidth (FILE * fp, GLog * logger, int isp)
{
  pjson (fp, "%.*s\"%s\": %llu,%.*s", isp, TAB, OVERALL_BANDWIDTH,
         logger->resp_size, nlines, NL);
}

/* Output the path of the log being parsed under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_log (FILE * fp, int isp)
{
  if (conf.ifile == NULL)
    conf.ifile = (char *) "STDIN";
  pjson (fp, "%.*s\"%s\": \"", isp, TAB, OVERALL_LOG);
  escape_json_output (fp, conf.ifile);
  pjson (fp, "\"%.*s", nlines, NL);
}

/* Output hits data.
 *
 * On success, data is outputted. */
static void
phits (FILE * fp, GMetrics * nmetrics, int sp)
{
  int isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  pjson (fp, "%.*s\"hits\": {%.*s", sp, TAB, nlines, NL);
  /* print hits */
  pjson (fp, "%.*s\"count\": %d,%.*s", isp, TAB, nmetrics->hits, nlines, NL);
  /* print hits percent */
  pjson (fp, "%.*s\"percent\": %4.2f%.*s", isp, TAB, nmetrics->hits_perc,
         nlines, NL);
  pjson (fp, "%.*s},%.*s", sp, TAB, nlines, NL);
}

/* Output visitors data.
 *
 * On success, data is outputted. */
static void
pvisitors (FILE * fp, GMetrics * nmetrics, int sp)
{
  int isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  pjson (fp, "%.*s\"visitors\": {%.*s", sp, TAB, nlines, NL);
  /* print visitors */
  pjson (fp, "%.*s\"count\": %d,%.*s", isp, TAB, nmetrics->visitors, nlines,
         NL);
  /* print visitors percent */
  pjson (fp, "%.*s\"percent\": %4.2f%.*s", isp, TAB, nmetrics->visitors_perc,
         nlines, NL);
  pjson (fp, "%.*s},%.*s", sp, TAB, nlines, NL);
}

/* Output bandwidth data.
 *
 * On success, data is outputted. */
static void
pbw (FILE * fp, GMetrics * nmetrics, int sp)
{
  int isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

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

/* Output average time served data.
 *
 * On success, data is outputted. */
static void
pavgts (FILE * fp, GMetrics * nmetrics, int sp)
{
  if (!conf.serve_usecs)
    return;
  pjson (fp, "%.*s\"avgts\": %lld,%.*s", sp, TAB,
         (long long) nmetrics->avgts.nts, nlines, NL);
}

/* Output cumulative time served data.
 *
 * On success, data is outputted. */
static void
pcumts (FILE * fp, GMetrics * nmetrics, int sp)
{
  if (!conf.serve_usecs)
    return;
  pjson (fp, "%.*s\"cumts\": %lld,%.*s", sp, TAB,
         (long long) nmetrics->cumts.nts, nlines, NL);
}

/* Output maximum time served data.
 *
 * On success, data is outputted. */
static void
pmaxts (FILE * fp, GMetrics * nmetrics, int sp)
{
  if (!conf.serve_usecs)
    return;
  pjson (fp, "%.*s\"maxts\": %lld,%.*s", sp, TAB,
         (long long) nmetrics->maxts.nts, nlines, NL);
}

/* Output request method data.
 *
 * On success, data is outputted. */
static void
pmethod (FILE * fp, GMetrics * nmetrics, int sp)
{
  /* request method */
  if (conf.append_method && nmetrics->method) {
    pjson (fp, "%.*s\"method\": \"%s\",%.*s", sp, TAB, nmetrics->method, nlines,
           NL);
  }
}

/* Output protocol method data.
 *
 * On success, data is outputted. */
static void
pprotocol (FILE * fp, GMetrics * nmetrics, int sp)
{
  /* request protocol */
  if (conf.append_protocol && nmetrics->protocol) {
    pjson (fp, "%.*s\"protocol\": \"%s\",%.*s", sp, TAB, nmetrics->protocol,
           nlines, NL);
  }
}

/* Output the panel name as an array of objects.
 *
 * On success, data is outputted. */
static void
print_open_panel_attr (FILE * fp, const char *attr, int sp)
{
  pjson (fp, "%.*s\"%s\": {%.*s", sp, TAB, attr, nlines, NL);
}

/* Close the array of objects.
 *
 * On success, data is outputted. */
static void
print_close_panel_attr (FILE * fp, int sp)
{
  pjson (fp, "%.*s}", sp, TAB);
}

/* Output the metadata key as an object.
 *
 * On success, data is outputted. */
static void
print_open_meta_attr (FILE * fp, int sp)
{
  /* open data metric block */
  pjson (fp, "%.*s\"metadata\": {%.*s", sp, TAB, nlines, NL);
}

/* Close the metadata object.
 *
 * On success, data is outputted. */
static void
print_close_meta_attr (FILE * fp, int sp)
{
  /* close meta data block */
  pjson (fp, "%.*s},%.*s", sp, TAB, nlines, NL);
}

/* Output the data key as an array.
 *
 * On success, data is outputted. */
static void
print_open_data_attr (FILE * fp, int isp)
{
  /* open data metric data */
  pjson (fp, "%.*s\"data\": [%.*s", isp, TAB, nlines, NL);
}

/* Close the data array.
 *
 * On success, data is outputted. */
static void
print_close_data_attr (FILE * fp, int isp)
{
  /* close data data */
  pjson (fp, "%.*s]%.*s", isp, TAB, nlines, NL);
}

/* Output the open block item object.
 *
 * On success, data is outputted. */
static void
print_open_block_attr (FILE * fp, int iisp)
{
  /* open data metric block */
  pjson (fp, "%.*s{%.*s", iisp, TAB, nlines, NL);
}

/* Close the block item object.
 *
 * On success, data is outputted. */
static void
print_close_block_attr (FILE * fp, int iisp, char comma)
{
  /* close data block */
  pjson (fp, "%.*s%.*s}%c%.*s", nlines, NL, iisp, TAB, comma, nlines, NL);
}

/* Output the hits meta data object.
 *
 * If no metadata found, it simply returns.
 * On success, meta data is outputted. */
static void
pmeta_data_hits (FILE * fp, GModule module, int sp)
{
  int isp = 0;
  uint64_t count = ht_get_meta_data (module, "hits");
  int max = 0, min = 0;

  ht_get_hits_min_max (module, &min, &max);

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  pjson (fp, "%.*s\"hits\": {%.*s", sp, TAB, nlines, NL);
  pjson (fp, "%.*s\"count\": %lld,%.*s", isp, TAB, (long long) count, nlines,
         NL);
  pjson (fp, "%.*s\"max\": %d,%.*s", isp, TAB, max, nlines, NL);
  pjson (fp, "%.*s\"min\": %d%.*s", isp, TAB, min, nlines, NL);
  pjson (fp, "%.*s}%.*s", sp, TAB, nlines, NL);
}

/* Output the visitors meta data object.
 *
 * If no metadata found, it simply returns.
 * On success, meta data is outputted. */
static void
pmeta_data_visitors (FILE * fp, GModule module, int sp)
{
  int isp = 0;
  uint64_t count = ht_get_meta_data (module, "visitors");
  int max = 0, min = 0;

  ht_get_visitors_min_max (module, &min, &max);

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  pjson (fp, "%.*s\"visitors\": {%.*s", sp, TAB, nlines, NL);
  pjson (fp, "%.*s\"count\": %lld,%.*s", isp, TAB, (long long) count, nlines,
         NL);
  pjson (fp, "%.*s\"max\": %d,%.*s", isp, TAB, max, nlines, NL);
  pjson (fp, "%.*s\"min\": %d%.*s", isp, TAB, min, nlines, NL);
  pjson (fp, "%.*s},%.*s", sp, TAB, nlines, NL);
}

/* Output the bytes meta data object.
 *
 * If no metadata found, it simply returns.
 * On success, meta data is outputted. */
static void
pmeta_data_bw (FILE * fp, GModule module, int sp)
{
  int isp = 0;
  uint64_t count = 0;
  uint64_t max = 0, min = 0;

  if (!conf.bandwidth)
    return;

  ht_get_bw_min_max (module, &min, &max);

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  count = ht_get_meta_data (module, "bytes");
  pjson (fp, "%.*s\"bytes\": {%.*s", sp, TAB, nlines, NL);
  pjson (fp, "%.*s\"count\": %lld,%.*s", isp, TAB, (long long) count, nlines,
         NL);
  pjson (fp, "%.*s\"max\": %lld,%.*s", isp, TAB, (long long) max, nlines, NL);
  pjson (fp, "%.*s\"min\": %lld%.*s", isp, TAB, (long long) min, nlines, NL);
  pjson (fp, "%.*s},%.*s", sp, TAB, nlines, NL);
}

/* Output the average of the average time served meta data object.
 *
 * If no metadata found, it simply returns.
 * On success, meta data is outputted. */
static void
pmeta_data_avgts (FILE * fp, GModule module, int sp)
{
  int isp = 0;
  uint64_t avg = 0, hits = 0, cumts = 0;

  if (!conf.serve_usecs)
    return;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  cumts = ht_get_meta_data (module, "cumts");
  hits = ht_get_meta_data (module, "hits");
  if (hits > 0)
    avg = cumts / hits;

  pjson (fp, "%.*s\"avgts\": {%.*s", sp, TAB, nlines, NL);
  pjson (fp, "%.*s\"avg\": %lld%.*s", isp, TAB, (long long) avg, nlines, NL);
  pjson (fp, "%.*s},%.*s", sp, TAB, nlines, NL);
}

/* Output the cumulative time served meta data object.
 *
 * If no metadata found, it simply returns.
 * On success, meta data is outputted. */
static void
pmeta_data_cumts (FILE * fp, GModule module, int sp)
{
  int isp = 0;
  uint64_t count = 0, max = 0, min = 0;

  if (!conf.serve_usecs)
    return;

  ht_get_cumts_min_max (module, &min, &max);

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  count = ht_get_meta_data (module, "cumts");
  pjson (fp, "%.*s\"cumts\": {%.*s", sp, TAB, nlines, NL);
  pjson (fp, "%.*s\"count\": %lld,%.*s", isp, TAB, (long long) count, nlines,
         NL);
  pjson (fp, "%.*s\"max\": %lld,%.*s", isp, TAB, (long long) max, nlines, NL);
  pjson (fp, "%.*s\"min\": %lld%.*s", isp, TAB, (long long) min, nlines, NL);
  pjson (fp, "%.*s},%.*s", sp, TAB, nlines, NL);
}

/* Output the maximum time served meta data object.
 *
 * If no metadata found, it simply returns.
 * On success, meta data is outputted. */
static void
pmeta_data_maxts (FILE * fp, GModule module, int sp)
{
  int isp = 0;
  uint64_t count = 0, max = 0, min = 0;

  if (!conf.serve_usecs)
    return;

  ht_get_maxts_min_max (module, &min, &max);

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  count = ht_get_meta_data (module, "maxts");
  pjson (fp, "%.*s\"maxts\": {%.*s", sp, TAB, nlines, NL);
  pjson (fp, "%.*s\"count\": %lld,%.*s", isp, TAB, (long long) count, nlines,
         NL);
  pjson (fp, "%.*s\"max\": %lld,%.*s", isp, TAB, (long long) max, nlines, NL);
  pjson (fp, "%.*s\"min\": %lld%.*s", isp, TAB, (long long) min, nlines, NL);
  pjson (fp, "%.*s},%.*s", sp, TAB, nlines, NL);
}

/* Entry point to output panel's metadata. */
static void
print_meta_data (FILE * fp, GHolder * h, int sp)
{
  int isp = 0, iisp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1, iisp = sp + 2;

  print_open_meta_attr (fp, isp);

  pmeta_data_avgts (fp, h->module, iisp);
  pmeta_data_cumts (fp, h->module, iisp);
  pmeta_data_maxts (fp, h->module, iisp);
  pmeta_data_bw (fp, h->module, iisp);
  pmeta_data_visitors (fp, h->module, iisp);
  pmeta_data_hits (fp, h->module, iisp);

  print_close_meta_attr (fp, isp);
}

/* A wrapper function to ouput data metrics per panel. */
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

/* A wrapper function to ouput children nodes. */
static void
print_json_sub_items (FILE * fp, GSubList * sl, GPercTotals totals, int iisp)
{
  GMetrics *nmetrics;
  GSubItem *iter;
  char comma = ' ';
  int i = 0, iiisp = 0, iiiisp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    iiisp = iisp + 1, iiiisp = iiisp + 1;

  if (sl == NULL)
    return;

  pjson (fp, ",%.*s%.*s\"items\": [%.*s", nlines, NL, iisp, TAB, nlines, NL);
  for (iter = sl->head; iter; iter = iter->next, i++) {
    set_data_metrics (iter->metrics, &nmetrics, totals);

    pjson (fp, "%.*s{%.*s", iiisp, TAB, nlines, NL);
    print_json_block (fp, nmetrics, iiiisp);
    comma = (i != sl->size - 1) ? ',' : ' ';
    pjson (fp, "%.*s%.*s}%c%.*s", nlines, NL, iiisp, TAB, comma, nlines, NL);
    free (nmetrics);
  }
  pjson (fp, "%.*s]", iisp, TAB);
}

/* Ouput Geolocation data and the IP's hostname. */
static void
print_json_host_geo (FILE * fp, GSubList * sl, GO_UNUSED GPercTotals totals,
                     int iisp)
{
  int i;
  GSubItem *iter;
  static const char *key[] = {
    "country",
    "city",
    "hostname",
  };

  if (sl == NULL)
    return;

  pjson (fp, ",%.*s", nlines, NL);

  /* Iterate over child properties (country, city, etc) and print them out */
  for (i = 0, iter = sl->head; iter; iter = iter->next, i++) {
    pjson (fp, "%.*s\"%s\": \"", iisp, TAB, key[iter->metrics->id]);
    escape_json_output (fp, iter->metrics->data);
    pjson (fp, (i != sl->size - 1) ? "\",%.*s" : "\"", nlines, NL);
  }
}

/* Ouput data and determine if there are children nodes. */
static void
print_data_metrics (FILE * fp, GHolder * h, GPercTotals totals, int sp,
                    const struct GPanel_ *panel)
{
  GMetrics *nmetrics;
  char comma = ' ';
  int i, isp = 0, iisp = 0, iiisp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1, iisp = sp + 2, iiisp = sp + 3;

  print_open_data_attr (fp, isp);
  /* output data metrics */
  for (i = 0; i < h->idx; i++) {
    set_data_metrics (h->items[i].metrics, &nmetrics, totals);

    /* open data metric block */
    print_open_block_attr (fp, iisp);
    /* output data metric block */
    print_json_block (fp, nmetrics, iiisp);
    /* if there are children notes, spit them out */
    if (panel->subitems && h->sub_items_size)
      panel->subitems (fp, h->items[i].sub_list, totals, iiisp);
    /* close data metric block */
    comma = (i != h->idx - 1) ? ',' : ' ';
    print_close_block_attr (fp, iisp, comma);

    free (nmetrics);
  }
  print_close_data_attr (fp, isp);
}

/* Entry point to ouput data metrics per panel. */
static void
print_json_data (FILE * fp, GHolder * h, GPercTotals totals,
                 const struct GPanel_ *panel)
{
  int sp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    sp = 1;

  /* output open panel attribute */
  print_open_panel_attr (fp, module_to_id (h->module), sp);
  /* output panel metadata */
  print_meta_data (fp, h, sp);
  /* output panel data */
  print_data_metrics (fp, h, totals, sp, panel);
  /* output close panel attribute */
  print_close_panel_attr (fp, sp);
}

/* Output overall data. */
static void
print_json_summary (FILE * fp, GLog * logger)
{
  int sp = 0, isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    sp = 1, isp = 2;

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

/* Entry point to generate a a json report writing it to the fp */
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

  /* use new lines to prettify output */
  if (conf.json_pretty_print)
    nlines = 1;

  pjson (fp, "{%.*s", nlines, NL);
  print_json_summary (fp, logger);

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    if (!(panel = panel_lookup (module)))
      continue;

    panel->render (fp, holder + module, totals, panel);
    pjson (fp, (module != TOTAL_MODULES - 1) ? ",%.*s" : "%.*s", nlines, NL);
  }
  pjson (fp, "}");

  fclose (fp);
}
