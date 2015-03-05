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

#ifdef HAVE_LIBGEOIP
#include "geolocation.h"
#endif

#include "commons.h"
#include "error.h"
#include "gdns.h"
#include "settings.h"
#include "ui.h"
#include "util.h"
#include "xmalloc.h"

static void print_json_data (FILE * fp, GHolder * h, int processed);
static void print_json_host_data (FILE * fp, GHolder * h, int processed);

static GJSON paneling[] = {
  {VISITORS, print_json_data},
  {REQUESTS, print_json_data},
  {REQUESTS_STATIC, print_json_data},
  {NOT_FOUND, print_json_data},
  {HOSTS, print_json_host_data},
  {BROWSERS, print_json_data},
  {OS, print_json_data},
  {REFERRERS, print_json_data},
  {REFERRING_SITES, print_json_data},
  {KEYPHRASES, print_json_data},
#ifdef HAVE_LIBGEOIP
  {GEO_LOCATION, print_json_data},
#endif
  {STATUS_CODES, print_json_data},
};

static GJSON *
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
print_json_block (FILE * fp, GMetrics * nmetrics, char *sep)
{
  fprintf (fp, "%s\t\"hits\": %d,\n", sep, nmetrics->hits);
  fprintf (fp, "%s\t\"visitors\": %d,\n", sep, nmetrics->visitors);
  fprintf (fp, "%s\t\"percent\": %4.2f,\n", sep, nmetrics->percent);
  fprintf (fp, "%s\t\"bytes\": %ld,\n", sep, nmetrics->bw.nbw);

  if (conf.serve_usecs)
    fprintf (fp, "%s\t\"time_served\": %ld,\n", sep, nmetrics->avgts.nts);

  if (conf.append_method && nmetrics->method)
    fprintf (fp, "%s\t\"method\": \"%s\",\n", sep, nmetrics->method);

  if (conf.append_protocol && nmetrics->protocol)
    fprintf (fp, "%s\t\"protocol\": \"%s\",\n", sep, nmetrics->protocol);

  fprintf (fp, "%s\t\"data\": \"", sep);
  escape_json_output (fp, nmetrics->data);
  fprintf (fp, "\"");
}

static void
print_json_host_geo (FILE * fp, GSubList * sub_list, char *sep)
{
  GSubItem *iter;
  static const char *key[] = {
    "country",
    "city",
    "hostname",
  };

  int i;
  if (sub_list == NULL)
    return;

  fprintf (fp, ",\n");
  for (i = 0, iter = sub_list->head; iter; iter = iter->next, i++) {
    fprintf (fp, "%s\t\"%s\": \"", sep, key[iter->metrics->id]);
    escape_json_output (fp, iter->metrics->data);
    fprintf (fp, (i != sub_list->size - 1) ? "\",\n" : "\"");
  }
}

static void
print_json_host_data (FILE * fp, GHolder * h, int processed)
{
  GMetrics *nmetrics;
  char *sep = char_repeat (2, '\t');
  int i;

  fprintf (fp, "\t\"%s\": [\n", module_to_id (h->module));
  for (i = 0; i < h->idx; i++) {
    set_data_metrics (h->items[i].metrics, &nmetrics, processed);

    fprintf (fp, "%s{\n", sep);
    print_json_block (fp, nmetrics, sep);
    print_json_host_geo (fp, h->items[i].sub_list, sep);
    fprintf (fp, (i != h->idx - 1) ? "\n%s},\n" : "\n%s}\n", sep);

    free (nmetrics);
  }
  fprintf (fp, "\t]");

  free (sep);
}

static void
print_json_sub_items (FILE * fp, GHolder * h, int idx, int processed)
{
  GMetrics *nmetrics;
  GSubItem *iter;
  GSubList *sub_list = h->items[idx].sub_list;
  char *sep = char_repeat (3, '\t');
  int i = 0;

  if (sub_list == NULL)
    return;

  fprintf (fp, ",\n%s\"items\": [\n", sep);
  for (iter = sub_list->head; iter; iter = iter->next, i++) {
    set_data_metrics (iter->metrics, &nmetrics, processed);

    fprintf (fp, "%s{\n", sep);
    print_json_block (fp, nmetrics, sep);
    fprintf (fp, (i != sub_list->size - 1) ? "\n%s},\n" : "\n%s}\n", sep);
    free (nmetrics);
  }
  fprintf (fp, "\t\t\t]");

  free (sep);
}

static void
print_json_data (FILE * fp, GHolder * h, int processed)
{
  GMetrics *nmetrics;
  char *sep = char_repeat (2, '\t');
  int i;

  fprintf (fp, "\t\"%s\": [\n", module_to_id (h->module));
  for (i = 0; i < h->idx; i++) {
    set_data_metrics (h->items[i].metrics, &nmetrics, processed);

    fprintf (fp, "%s{\n", sep);
    print_json_block (fp, nmetrics, sep);
    if (h->sub_items_size)
      print_json_sub_items (fp, h, i, processed);
    fprintf (fp, (i != h->idx - 1) ? "\n%s},\n" : "\n%s}\n", sep);

    free (nmetrics);
  }
  fprintf (fp, "\t]");

  free (sep);
}

/* entry point to generate a a json report writing it to the fp */
/* follow the JSON style similar to http://developer.github.com/v3/ */
void
output_json (GLog * logger, GHolder * holder)
{
  GModule module;
  FILE *fp = stdout;

  fprintf (fp, "{\n");
  for (module = 0; module < TOTAL_MODULES; module++) {
    const GJSON *panel = panel_lookup (module);
    if (!panel)
      continue;
    panel->render (fp, holder + module, logger->process);
    module != TOTAL_MODULES - 1 ? fprintf (fp, ",\n") : fprintf (fp, "\n");
  }
  fprintf (fp, "}");

  fclose (fp);
}
