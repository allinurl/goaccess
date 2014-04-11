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
#include "tcabinet.h"
#else
#include "glibht.h"
#endif

#include "commons.h"
#include "error.h"
#include "gdns.h"
#include "settings.h"
#include "ui.h"
#include "util.h"
#include "xmalloc.h"

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
       /* unusual to be on a log */
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
       /* TODO: escape four-hex-digits, \u */
     default:
       fputc (*s, fp);
       break;
    }
    s++;
  }
}

static void
print_json_sub_items (FILE * fp, GSubList * sub_list, int process)
{
  char *data;
  float percent;
  GSubItem *iter;
  int hits, i = 0;

  fprintf (fp, ",\n\t\t\t\"items\": [\n");
  for (iter = sub_list->head; iter; iter = iter->next) {
    hits = iter->hits;
    data = (char *) iter->data;
    percent = get_percentage (process, hits);
    percent = percent < 0 ? 0 : percent;

    fprintf (fp, "\t\t\t\t{\n");
    fprintf (fp, "\t\t\t\t\t\"hits\": \"%d\",\n", hits);
    fprintf (fp, "\t\t\t\t\t\"percent\": \"%4.2f%%\",\n", percent);
    fprintf (fp, "\t\t\t\t\t\"data\": \"");
    escape_json_output (fp, data);
    fprintf (fp, "\"\n");
    fprintf (fp, "\t\t\t\t}");

    if (i != sub_list->size - 1)
      fprintf (fp, ",\n");
    else
      fprintf (fp, "\n");
    i++;
  }
  fprintf (fp, "\t\t\t]");
}

/**
 * Generate JSON on partial fields for the following modules:
 * OS, BROWSERS, REFERRERS, REFERRING_SITES, KEYPHRASES, STATUS_CODES
 */
static void
print_json_generic (FILE * fp, const GHolder * h, int process)
{
  char *data;
  const char *id = NULL;
  float percent;
  int i, hits;

  if (h->module == BROWSERS)
    id = BROWS_ID;
  else if (h->module == OS)
    id = OPERA_ID;
  else if (h->module == REFERRERS)
    id = REFER_ID;
  else if (h->module == REFERRING_SITES)
    id = SITES_ID;
  else if (h->module == KEYPHRASES)
    id = KEYPH_ID;
  else if (h->module == STATUS_CODES)
    id = CODES_ID;
#ifdef HAVE_LIBGEOIP
  else if (h->module == GEO_LOCATION)
    id = GEOLO_ID;
#endif

  fprintf (fp, "\t\"%s\": [\n", id);

  for (i = 0; i < h->idx; i++) {
    hits = h->items[i].hits;
    data = h->items[i].data;
    percent = get_percentage (process, hits);
    percent = percent < 0 ? 0 : percent;

    fprintf (fp, "\t\t{\n");
    fprintf (fp, "\t\t\t\"hits\": \"%d\",\n", hits);
    fprintf (fp, "\t\t\t\"percent\": \"%4.2f%%\",\n", percent);
    fprintf (fp, "\t\t\t\"data\": \"");
    escape_json_output (fp, data);
    fprintf (fp, "\"");

    if (h->module == OS || h->module == BROWSERS || h->module == STATUS_CODES
#ifdef HAVE_LIBGEOIP
        || h->module == GEO_LOCATION
#endif
      )
      print_json_sub_items (fp, h->items[i].sub_list, process);

    fprintf (fp, "\n\t\t}");

    if (i != h->idx - 1)
      fprintf (fp, ",\n");
    else
      fprintf (fp, "\n");
  }
  fprintf (fp, "\n\t]");
}

/**
 * Generate JSON on complete fields for the following modules:
 * REQUESTS, REQUESTS_STATIC, NOT_FOUND, HOSTS
 */
static void
print_json_complete (FILE * fp, GHolder * holder, int process)
{
#ifdef HAVE_LIBGEOIP
  const char *location = NULL;
#endif

  char *data, *host, *method = NULL, *protocol = NULL;
  float percent;
  GHolder *h;
  int i, j, hits;
  unsigned long long bw, usecs;

  for (i = 0; i < 4; i++) {
    switch (i) {
     case 0:
       h = holder + REQUESTS;
       fprintf (fp, "\t\"%s\": [\n", REQUE_ID);
       break;
     case 1:
       h = holder + REQUESTS_STATIC;
       fprintf (fp, "\t\"%s\": [\n", STATI_ID);
       break;
     case 2:
       h = holder + NOT_FOUND;
       fprintf (fp, "\t\"%s\": [\n", FOUND_ID);
       break;
     case 3:
       h = holder + HOSTS;
       fprintf (fp, "\t\"%s\": [\n", HOSTS_ID);
       break;
    }

    for (j = 0; j < h->idx; j++) {
      hits = h->items[j].hits;
      data = h->items[j].data;
      percent = get_percentage (process, hits);
      percent = percent < 0 ? 0 : percent;
      bw = h->items[j].bw;
      usecs = h->items[j].usecs;
      method = h->items[j].method;
      protocol = h->items[j].protocol;

      fprintf (fp, "\t\t{\n");
      fprintf (fp, "\t\t\t\"hits\": \"%d\",\n", hits);
      fprintf (fp, "\t\t\t\"percent\": \"%4.2f%%\",\n", percent);
      fprintf (fp, "\t\t\t\"data\": \"");
      escape_json_output (fp, data);
      fprintf (fp, "\",\n");
      fprintf (fp, "\t\t\t\"bytes\": \"%lld\"", bw);

      if (h->module == HOSTS) {
        if (conf.enable_html_resolver) {
          host = reverse_ip (data);
          fprintf (fp, ",\n\t\t\t\"host\": \"");
          escape_json_output (fp, host);
          fprintf (fp, "\"");
          free (host);
        }
#ifdef HAVE_LIBGEOIP
        location = get_geoip_data (data);
        fprintf (fp, ",\n\t\t\t\"country\": \"");
        escape_json_output (fp, (char *) location);
        fprintf (fp, "\"");
#endif
      }
      if (conf.serve_usecs)
        fprintf (fp, ",\n\t\t\t\"time_served\": \"%lld\"", usecs);
      if (conf.append_protocol && protocol)
        fprintf (fp, ",\n\t\t\t\"protocol\": \"%s\"", protocol);
      if (conf.append_method && method)
        fprintf (fp, ",\n\t\t\t\"method\": \"%s\"", method);

      fprintf (fp, "\n\t\t}");

      if (j != h->idx - 1)
        fprintf (fp, ",\n");
      else
        fprintf (fp, "\n");
    }
    if (i != 3)
      fprintf (fp, "\t],\n");
    else
      fprintf (fp, "\t]\n");
  }
}

/* generate JSON unique visitors stats */
static void
print_json_visitors (FILE * fp, GHolder * h)
{
  char *data, buf[DATE_LEN];
  float percent;
  int hits, bw, i, process = get_ht_size (ht_unique_visitors);

  /* make compiler happy */
  memset (buf, 0, sizeof (buf));
  fprintf (fp, "\t\"%s\": [\n", VISIT_ID);
  for (i = 0; i < h->idx; i++) {
    hits = h->items[i].hits;
    data = h->items[i].data;
    percent = get_percentage (process, hits);
    percent = percent < 0 ? 0 : percent;
    bw = h->items[i].bw;
    convert_date (buf, data, "%Y%m%d", "%d/%b/%Y", DATE_LEN);
    fprintf (fp, "\t\t{\n\t\t\t\"hits\": \"%d\",\n", hits);
    fprintf (fp, "\t\t\t\"percent\": \"%4.2f%%\",\n", percent);
    fprintf (fp, "\t\t\t\"date\": \"%s\",\n", buf);
    fprintf (fp, "\t\t\t\"bytes\": \"%d\"\n", bw);
    fprintf (fp, "\t\t}");

    if (i != h->idx - 1)
      fprintf (fp, ",\n");
    else
      fprintf (fp, "\n");
  }
  fprintf (fp, "\t]");
}

/* generate overview stats */
static void
print_json_summary (FILE * fp, GLog * logger)
{
  off_t log_size = 0;
  char now[DATE_TIME];

  generate_time ();
  strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S", now_tm);

  fprintf (fp, "\t\"%s\": {\n", GENER_ID);
  /* general statistics info */
  fprintf (fp, "\t\t\"date_time\": \"%s\",\n", now);
  fprintf (fp, "\t\t\"total_requests\": %d,\n", logger->process);
  fprintf (fp, "\t\t\"unique_visitors\": %d,\n",
           get_ht_size (ht_unique_visitors));
  fprintf (fp, "\t\t\"referrers\": %d,\n", get_ht_size (ht_referrers));

  if (!logger->piping)
    log_size = file_size (conf.ifile);

  fprintf (fp, "\t\t\"log_size\": %jd,\n", (intmax_t) log_size);
  fprintf (fp, "\t\t\"failed_requests\": %d,\n", logger->invalid);
  fprintf (fp, "\t\t\"unique_files\": %d,\n", get_ht_size (ht_requests));
  fprintf (fp, "\t\t\"unique_404\": %d,\n",
           get_ht_size (ht_not_found_requests));

  fprintf (fp, "\t\t\"bandwidth\": %lld,\n", logger->resp_size);
  fprintf (fp, "\t\t\"generation_time\": %llu,\n",
           ((long long) end_proc - start_proc));
  fprintf (fp, "\t\t\"static_files\": %d,\n",
           get_ht_size (ht_requests_static));

  if (conf.ifile == NULL)
    conf.ifile = (char *) "STDIN";

  fprintf (fp, "\t\t\"log_file\": \"%s\"\n", conf.ifile);
  fprintf (fp, "\t}");
}

/* entry point to generate a a json report writing it to the fp */
/* follow the JSON style similar to http://developer.github.com/v3/ */
void
output_json (GLog * logger, GHolder * holder)
{
  FILE *fp = stdout;
  fprintf (fp, "{\n");          /* open */

  print_json_summary (fp, logger);
  fprintf (fp, ",\n");

  print_json_visitors (fp, holder + VISITORS);
  fprintf (fp, ",\n");

  print_json_complete (fp, holder, logger->process);
  fprintf (fp, ",\n");

  print_json_generic (fp, holder + OS, get_ht_size (ht_unique_visitors));
  fprintf (fp, ",\n");

  print_json_generic (fp, holder + BROWSERS,
                      get_ht_size (ht_unique_visitors));
  fprintf (fp, ",\n");

  print_json_generic (fp, holder + REFERRERS, logger->process);
  fprintf (fp, ",\n");

  print_json_generic (fp, holder + REFERRING_SITES, logger->process);
  fprintf (fp, ",\n");

  print_json_generic (fp, holder + KEYPHRASES, logger->process);
  fprintf (fp, ",\n");

#ifdef HAVE_LIBGEOIP
  print_json_generic (fp, holder + GEO_LOCATION, logger->process);
  fprintf (fp, ",\n");
#endif

  print_json_generic (fp, holder + STATUS_CODES, logger->process);
  fprintf (fp, "\n");

  fprintf (fp, "\n}\n");        /* close */

  fclose (fp);
}
