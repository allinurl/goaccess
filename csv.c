/**
 * output.c -- output csv to the standard output stream
 * Copyright (C) 2009-2013 by Gerardo Orellana <goaccess@prosoftcorp.com>
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

#include "commons.h"
#include "error.h"
#include "gdashboard.h"
#include "output.h"
#include "ui.h"
#include "util.h"
#include "xmalloc.h"

static void
escape_cvs_output (FILE * fp, char *s)
{
   while (*s) {
      switch (*s) {
       case '"':
          fprintf (fp, "\\\"");
          break;
       default:
          fputc (*s, fp);
          break;
      }
      s++;
   }
}

/**
 * Generate CSV on partial fields for the following modules:
 * OS, BROWSERS, REFERRERS, REFERRING_SITES, KEYPHRASES, STATUS_CODES
 */
static void
print_csv_generic (FILE * fp, GHolder * holder, int process)
{
   char id[REF_LEN], *data;
   float percent;
   GHolder *h;
   int i, j, hits;

   /* os, browsers, referrers, referring sites, keyphrases & status code report */
   for (i = 0; i < 6; i++) {
      switch (i) {
       case 0:
          process = g_hash_table_size (ht_unique_visitors);
          h = holder + OS;
          strcpy (id, "os");
          break;
       case 1:
          process = g_hash_table_size (ht_unique_visitors);
          h = holder + BROWSERS;
          strcpy (id, "browser");
          break;
       case 2:
          h = holder + REFERRERS;
          strcpy (id, "ref");
          break;
       case 3:
          h = holder + REFERRING_SITES;
          strcpy (id, "refsite");
          break;
       case 4:
          h = holder + KEYPHRASES;
          strcpy (id, "keyphrase");
          break;
       case 5:
          h = holder + STATUS_CODES;
          strcpy (id, "status");
          break;
      }

      for (j = 0; j < h->idx; j++) {
         hits = h->items[j].hits;
         data = h->items[j].data;
         percent = get_percentage (process, hits);
         percent = percent < 0 ? 0 : percent;

         fprintf (fp, "\"%s\",\"%d\",\"%4.2f%%\",\"", id, hits, percent);
         escape_cvs_output (fp, data);
         fprintf (fp, "\"\r\n");
      }
   }
}

/**
 * Generate CSV on complete fields for the following modules:
 * REQUESTS, REQUESTS_STATIC, NOT_FOUND, HOSTS
 */
static void
print_csv_complete (FILE * fp, GHolder * holder, int process)
{
   char id[REF_LEN], *data;
   float percent;
   GHolder *h;
   int i, j, hits;
   unsigned long long bw, usecs;

   for (i = 0; i < 4; i++) {
      switch (i) {
       case 0:
          h = holder + REQUESTS;
          strcpy (id, "req");
          break;
       case 1:
          h = holder + REQUESTS_STATIC;
          strcpy (id, "static");
          break;
       case 2:
          h = holder + NOT_FOUND;
          strcpy (id, "notfound");
          break;
       case 3:
          h = holder + HOSTS;
          strcpy (id, "host");
          break;
      }

      for (j = 0; j < h->idx; j++) {
         hits = h->items[j].hits;
         data = h->items[j].data;
         percent = get_percentage (process, hits);
         percent = percent < 0 ? 0 : percent;
         bw = h->items[j].bw;
         usecs = h->items[j].usecs;

         fprintf (fp, "\"%s\",\"%d\",\"%4.2f%%\",\"", id, hits, percent);
         escape_cvs_output (fp, data);
         fprintf (fp, "\",\"%lld\",\"%lld\"\r\n", bw, usecs);
      }
   }
}

/* generate CSV unique visitors stats */
static void
print_csv_visitors (FILE * fp, GHolder * h)
{
   char *data, buf[DATE_LEN];
   float percent;
   int hits, bw, i, process = g_hash_table_size (ht_unique_visitors);

   /* make compiler happy */
   memset (buf, 0, sizeof (buf));

   for (i = 0; i < h->idx; i++) {
      hits = h->items[i].hits;
      data = h->items[i].data;
      percent = get_percentage (process, hits);
      percent = percent < 0 ? 0 : percent;
      bw = h->items[i].bw;
      convert_date (buf, data, "%Y%m%d", "%d/%b/%Y", DATE_LEN);

      fprintf (fp, "\"visitors\",\"%d\",\"%4.2f%%\",\"%s\",\"%d\"\r\n", hits,
               percent, buf, bw);
   }
}

/* generate overview stats */
static void
print_csv_summary (FILE * fp, GLog * logger)
{
   off_t log_size = 0;
   char now[DATE_TIME];

   generate_time ();
   strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S", now_tm);

   /* general statistics info */
   fprintf (fp, "\"general\",\"date_time\",\"%s\"\r\n", now);
   fprintf (fp, "\"general\",\"total_requests\",\"%d\"\r\n", logger->process);
   fprintf (fp, "\"general\",\"unique_visitors\",\"%d\"\r\n",
            g_hash_table_size (ht_unique_visitors));
   fprintf (fp, "\"general\",\"referrers\",\"%d\"\r\n",
            g_hash_table_size (ht_referrers));

   if (!logger->piping)
      log_size = file_size (conf.ifile);

   fprintf (fp, "\"general\",\"log_size\",\"%jd\"\r\n", (intmax_t) log_size);
   fprintf (fp, "\"general\",\"failed_requests\",\"%d\"\r\n", logger->invalid);
   fprintf (fp, "\"general\",\"unique_files\",\"%d\"\r\n",
            g_hash_table_size (ht_requests));
   fprintf (fp, "\"general\",\"unique_404\",\"%d\"\r\n",
            g_hash_table_size (ht_not_found_requests));

   fprintf (fp, "\"general\",\"bandwidth\",\"%lld\"\r\n", logger->resp_size);
   fprintf (fp, "\"general\",\"generation_time\",\"%llu\"\r\n",
            ((long long) end_proc - start_proc));
   fprintf (fp, "\"general\",\"static_files\",\"%d\"\r\n",
            g_hash_table_size (ht_requests_static));

   if (conf.ifile == NULL)
      conf.ifile = (char *) "STDIN";

   fprintf (fp, "\"general\",\"filename\",\"%s\"\r\n", conf.ifile);
}

/* entry point to generate a a csv report writing it to the fp */
void
output_csv (GLog * logger, GHolder * holder)
{
   FILE *fp = stdout;

   print_csv_summary (fp, logger);
   print_csv_visitors (fp, holder + VISITORS);
   print_csv_complete (fp, holder, logger->process);
   print_csv_generic (fp, holder, logger->process);

   fclose (fp);
}
