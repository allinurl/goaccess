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
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>

#include "csv.h"

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabinet.h"
#elif HAVE_LIBGLIB_2_0
#include "glibht.h"
#endif

#include "commons.h"
#include "error.h"
#include "ui.h"
#include "util.h"
#include "xmalloc.h"

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

static void
print_csv_sub_items (FILE * fp, GSubList * sub_list, int process,
                     const char *id, int *idx)
{
  char *data;
  float percent;
  GSubItem *iter;
  int hits, i = 0;

  for (iter = sub_list->head; iter; iter = iter->next) {
    hits = iter->hits;
    data = (char *) iter->data;
    percent = get_percentage (process, hits);
    percent = percent < 0 ? 0 : percent;

    fprintf (fp, "\"%d\",", i); /* idx */
    fprintf (fp, "\"%d\",", (*idx));    /* parent idx */
    fprintf (fp, "\"%s\",", id);
    fprintf (fp, "\"%d\",", hits);
    fprintf (fp, "\"%4.2f%%\",", percent);
    fprintf (fp, "\"");
    escape_cvs_output (fp, data);
    fprintf (fp, "\",");
    fprintf (fp, "\r\n");       /* parent idx */
    i++;
  }
}

/**
 * Generate CSV on partial fields for the following modules:
 * OS, BROWSERS, REFERRERS, REFERRING_SITES, KEYPHRASES, STATUS_CODES
 */
static void
print_csv_generic (FILE * fp, const GHolder * h, int process)
{
  char *data;
  const char *id = NULL;
  float percent;
  int i, idx, hits;

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

  for (i = 0, idx = 0; i < h->idx; i++, idx++) {
    hits = h->items[i].hits;
    data = h->items[i].data;
    percent = get_percentage (process, hits);
    percent = percent < 0 ? 0 : percent;

    fprintf (fp, "\"%d\",", idx);       /* idx */
    fprintf (fp, ",");          /* parent idx */
    fprintf (fp, "\"%s\",", id);
    fprintf (fp, "\"%d\",", hits);
    fprintf (fp, "\"%4.2f%%\",", percent);
    fprintf (fp, "\"");
    escape_cvs_output (fp, data);
    fprintf (fp, "\",");
    fprintf (fp, "\r\n");       /* parent idx */

    if (h->module == OS || h->module == BROWSERS || h->module == STATUS_CODES
#ifdef HAVE_LIBGEOIP
        || h->module == GEO_LOCATION
#endif
      )
      print_csv_sub_items (fp, h->items[i].sub_list, process, id, &idx);
  }
}

/**
 * Generate CSV on complete fields for the following modules:
 * REQUESTS, REQUESTS_STATIC, NOT_FOUND, HOSTS
 */
static void
print_csv_complete (FILE * fp, GHolder * holder, int process)
{
  char *data, *method = NULL, *protocol = NULL;
  const char *id = NULL;
  float percent;
  GHolder *h;
  int i, j, hits;
  unsigned long long bw, usecs;

  for (i = 0; i < 4; i++) {
    switch (i) {
     case 0:
       h = holder + REQUESTS;
       id = REQUE_ID;
       break;
     case 1:
       h = holder + REQUESTS_STATIC;
       id = STATI_ID;
       break;
     case 2:
       h = holder + NOT_FOUND;
       id = FOUND_ID;
       break;
     case 3:
       h = holder + HOSTS;
       id = HOSTS_ID;
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

      fprintf (fp, "\"%d\",", j);       /* idx */
      fprintf (fp, ",");        /* parent idx */
      fprintf (fp, "\"%s\",", id);
      fprintf (fp, "\"%d\",", hits);
      fprintf (fp, "\"%4.2f%%\",", percent);
      fprintf (fp, "\"");
      escape_cvs_output (fp, data);
      fprintf (fp, "\",");
      fprintf (fp, "\"%lld\"", bw);

      if (conf.serve_usecs)
        fprintf (fp, ",\"%lld\"", usecs);
      if (conf.append_protocol && protocol)
        fprintf (fp, ",\"%s\"", protocol);
      if (conf.append_method && method)
        fprintf (fp, ",\"%s\"", method);
      fprintf (fp, "\r\n");
    }
  }
}

/* generate CSV unique visitors stats */
static void
print_csv_visitors (FILE * fp, GHolder * h)
{
  char *data, buf[DATE_LEN];
  float percent;
  int hits, bw, i, process = get_ht_size (ht_unique_visitors);

  /* make compiler happy */
  memset (buf, 0, sizeof (buf));

  for (i = 0; i < h->idx; i++) {
    hits = h->items[i].hits;
    data = h->items[i].data;
    percent = get_percentage (process, hits);
    percent = percent < 0 ? 0 : percent;
    bw = h->items[i].bw;
    convert_date (buf, data, "%Y%m%d", "%d/%b/%Y", DATE_LEN);

    fprintf (fp, "\"%d\",", i); /* idx */
    fprintf (fp, ",");          /* parent idx */
    fprintf (fp, "\"%s\",", VISIT_ID);
    fprintf (fp, "\"%d\",", hits);
    fprintf (fp, "\"%4.2f%%\",", percent);
    fprintf (fp, "\"%s\",", buf);
    fprintf (fp, "\"%d\"\r\n", bw);
  }
}

/* generate overview stats */
static void
print_csv_summary (FILE * fp, GLog * logger)
{
  int i = 0;
  off_t log_size = 0;
  char now[DATE_TIME];

  generate_time ();
  strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S", now_tm);

  /* general statistics info */
  fprintf (fp, "\"%d\",,\"%s\",\"date_time\",\"%s\"\r\n", i++, GENER_ID, now);
  fprintf (fp, "\"%d\",,\"%s\",\"total_requests\",\"%d\"\r\n", i++, GENER_ID,
           logger->process);
  fprintf (fp, "\"%d\",,\"%s\",\"unique_visitors\",\"%d\"\r\n", i++, GENER_ID,
           get_ht_size (ht_unique_visitors));
  fprintf (fp, "\"%d\",,\"%s\",\"referrers\",\"%d\"\r\n", i++, GENER_ID,
           get_ht_size (ht_referrers));

  if (!logger->piping)
    log_size = file_size (conf.ifile);

  fprintf (fp, "\"%d\",,\"%s\",\"log_size\",\"%jd\"\r\n", i++, GENER_ID,
           (intmax_t) log_size);
  fprintf (fp, "\"%d\",,\"%s\",\"failed_requests\",\"%d\"\r\n", i++, GENER_ID,
           logger->invalid);
  fprintf (fp, "\"%d\",,\"%s\",\"unique_files\",\"%d\"\r\n", i++, GENER_ID,
           get_ht_size (ht_requests));
  fprintf (fp, "\"%d\",,\"%s\",\"unique_404\",\"%d\"\r\n", i++, GENER_ID,
           get_ht_size (ht_not_found_requests));

  fprintf (fp, "\"%d\",,\"%s\",\"bandwidth\",\"%lld\"\r\n", i++, GENER_ID,
           logger->resp_size);
  fprintf (fp, "\"%d\",,\"%s\",\"generation_time\",\"%llu\"\r\n", i++,
           GENER_ID, (long long) end_proc - start_proc);
  fprintf (fp, "\"%d\",,\"%s\",\"static_files\",\"%d\"\r\n", i++, GENER_ID,
           get_ht_size (ht_requests_static));

  if (conf.ifile == NULL)
    conf.ifile = (char *) "STDIN";

  fprintf (fp, "\"%d\",,\"%s\",\"log_file\",\"%s\"\r\n", i, GENER_ID,
           conf.ifile);
}

/* entry point to generate a a csv report writing it to the fp */
void
output_csv (GLog * logger, GHolder * holder)
{
  FILE *fp = stdout;

  print_csv_summary (fp, logger);
  print_csv_visitors (fp, holder + VISITORS);
  print_csv_complete (fp, holder, logger->process);
  print_csv_generic (fp, holder + OS, get_ht_size (ht_unique_visitors));
  print_csv_generic (fp, holder + BROWSERS, get_ht_size (ht_unique_visitors));
  print_csv_generic (fp, holder + REFERRERS, logger->process);
  print_csv_generic (fp, holder + REFERRING_SITES, logger->process);
  print_csv_generic (fp, holder + KEYPHRASES, logger->process);
#ifdef HAVE_LIBGEOIP
  print_csv_generic (fp, holder + GEO_LOCATION, logger->process);
#endif
  print_csv_generic (fp, holder + STATUS_CODES, logger->process);

  fclose (fp);
}
