/**
 * output.c -- output json to the standard output stream
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2016 Gerardo Orellana <hello @ goaccess.io>
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
  void (*subitems) (FILE * fp, GHolderItem * item, GPercTotals totals, int size,
                    int iisp);
} GPanel;

/* number of new lines (applicable fields) */
static int nlines = 0;

static void print_json_data (FILE * fp, GHolder * h, GPercTotals totals,
                             const struct GPanel_ *);
static void print_json_host_items (FILE * fp, GHolderItem * item,
                                   GPercTotals totals, int size, int iisp);
static void print_json_sub_items (FILE * fp, GHolderItem * item,
                                  GPercTotals totals, int size, int iisp);

/* *INDENT-OFF* */
static GPanel paneling[] = {
  {VISITORS            , print_json_data , NULL } ,
  {REQUESTS            , print_json_data , NULL } ,
  {REQUESTS_STATIC     , print_json_data , NULL } ,
  {NOT_FOUND           , print_json_data , NULL } ,
  {HOSTS               , print_json_data , print_json_host_items } ,
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

/* Set number of new lines when --json-pretty-print is used. */
void
set_json_nlines (int newline)
{
  nlines = newline;
}

/* A wrapper function to output a formatted string.
 *
 * On success, data is outputted. */
void
pjson (FILE * fp, const char *fmt, ...)
{
  va_list args;

  va_start (args, fmt);
  vfprintf (fp, fmt, args);
  va_end (args);
}

/* Output a JSON a key/value pair.
 *
 * On success, data is outputted. */
void
pskeysval (FILE * fp, const char *key, const char *val, int sp, int last)
{
  if (!last)
    pjson (fp, "%.*s\"%s\": \"%s\",%.*s", sp, TAB, key, val, nlines, NL);
  else
    pjson (fp, "%.*s\"%s\": \"%s\"", sp, TAB, key, val);
}

/* Output a JSON string key, int value pair.
 *
 * On success, data is outputted. */
void
pskeyival (FILE * fp, const char *key, int val, int sp, int last)
{
  if (!last)
    pjson (fp, "%.*s\"%s\": %d,%.*s", sp, TAB, key, val, nlines, NL);
  else
    pjson (fp, "%.*s\"%s\": %d", sp, TAB, key, val);
}

/* Output a JSON string key, uint64_t value pair.
 *
 * On success, data is outputted. */
void
pskeyu64val (FILE * fp, const char *key, uint64_t val, int sp, int last)
{
  if (!last)
    pjson (fp, "%.*s\"%s\": %" PRIu64 ",%.*s", sp, TAB, key, val, nlines, NL);
  else
    pjson (fp, "%.*s\"%s\": %" PRIu64 "", sp, TAB, key, val);
}

/* Output a JSON string key, int value pair.
 *
 * On success, data is outputted. */
void
pskeyfval (FILE * fp, const char *key, float val, int sp, int last)
{
  if (!last)
    pjson (fp, "%.*s\"%s\": %4.2f,%.*s", sp, TAB, key, val, nlines, NL);
  else
    pjson (fp, "%.*s\"%s\": %4.2f", sp, TAB, key, val);
}

/* Output the open block item object.
 *
 * On success, data is outputted. */
void
popen_obj (FILE * fp, int iisp)
{
  /* open data metric block */
  pjson (fp, "%.*s{%.*s", iisp, TAB, nlines, NL);
}

/* Output a JSON open object attribute.
 *
 * On success, data is outputted. */
void
popen_obj_attr (FILE * fp, const char *attr, int sp)
{
  /* open object attribute */
  pjson (fp, "%.*s\"%s\": {%.*s", sp, TAB, attr, nlines, NL);
}

/* Close JSON object.
 *
 * On success, data is outputted. */
void
pclose_obj (FILE * fp, int iisp, int last)
{
  if (!last)
    pjson (fp, "%.*s%.*s},%.*s", nlines, NL, iisp, TAB, nlines, NL);
  else
    pjson (fp, "%.*s%.*s}", nlines, NL, iisp, TAB);
}

/* Output a JSON open array attribute.
 *
 * On success, data is outputted. */
void
popen_arr_attr (FILE * fp, const char *attr, int sp)
{
  /* open object attribute */
  pjson (fp, "%.*s\"%s\": [%.*s", sp, TAB, attr, nlines, NL);
}

/* Close the data array.
 *
 * On success, data is outputted. */
void
pclose_arr (FILE * fp, int sp, int last)
{
  if (!last)
    pjson (fp, "%.*s%.*s],%.*s", nlines, NL, sp, TAB, nlines, NL);
  else
    pjson (fp, "%.*s%.*s]", nlines, NL, sp, TAB);
}

/* Output date and time for the overall object.
 *
 * On success, data is outputted. */
static void
poverall_datetime (FILE * fp, int sp)
{
  char now[DATE_TIME];

  generate_time ();
  strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S", now_tm);

  pskeysval (fp, OVERALL_DATETIME, now, sp, 0);
}

/* Output date and time for the overall object.
 *
 * On success, data is outputted. */
static void
poverall_requests (FILE * fp, GLog * logger, int sp)
{
  pskeyival (fp, OVERALL_REQ, logger->processed, sp, 0);
}

/* Output the number of valid requests under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_valid_reqs (FILE * fp, GLog * logger, int sp)
{
  pskeyival (fp, OVERALL_VALID, logger->valid, sp, 0);
}

/* Output the number of invalid requests under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_invalid_reqs (FILE * fp, GLog * logger, int sp)
{
  pskeyival (fp, OVERALL_FAILED, logger->invalid, sp, 0);
}

/* Output the total processed time under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_processed_time (FILE * fp, int sp)
{
  pskeyu64val (fp, OVERALL_GENTIME, end_proc - start_proc, sp, 0);
}

/* Output the total number of unique visitors under the overall
 * object.
 *
 * On success, data is outputted. */
static void
poverall_visitors (FILE * fp, int sp)
{
  pskeyival (fp, OVERALL_VISITORS, ht_get_size_uniqmap (VISITORS), sp, 0);
}

/* Output the total number of unique files under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_files (FILE * fp, int sp)
{
  pskeyival (fp, OVERALL_FILES, ht_get_size_datamap (REQUESTS), sp, 0);
}

/* Output the total number of excluded requests under the overall
 * object.
 *
 * On success, data is outputted. */
static void
poverall_excluded (FILE * fp, GLog * logger, int sp)
{
  pskeyival (fp, OVERALL_EXCL_HITS, logger->excluded_ip, sp, 0);
}

/* Output the number of referrers under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_refs (FILE * fp, int sp)
{
  pskeyival (fp, OVERALL_REF, ht_get_size_datamap (REFERRERS), sp, 0);
}

/* Output the number of not found (404s) under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_notfound (FILE * fp, int sp)
{
  pskeyival (fp, OVERALL_NOTFOUND, ht_get_size_datamap (NOT_FOUND), sp, 0);
}

/* Output the number of static files (jpg, pdf, etc) under the overall
 * object.
 *
 * On success, data is outputted. */
static void
poverall_static_files (FILE * fp, int sp)
{
  pskeyival (fp, OVERALL_STATIC, ht_get_size_datamap (REQUESTS_STATIC), sp, 0);
}

/* Output the size of the log being parsed under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_log_size (FILE * fp, GLog * logger, int sp)
{
  off_t log_size = 0;

  if (!logger->piping && conf.ifile)
    log_size = file_size (conf.ifile);

  pjson (fp, "%.*s\"%s\": %jd,%.*s", sp, TAB, OVERALL_LOGSIZE,
         (intmax_t) log_size, nlines, NL);
}

/* Output the total bandwidth consumed under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_bandwidth (FILE * fp, GLog * logger, int sp)
{
  pskeyu64val (fp, OVERALL_BANDWIDTH, logger->resp_size, sp, 0);
}

/* Output the path of the log being parsed under the overall object.
 *
 * On success, data is outputted. */
static void
poverall_log (FILE * fp, int sp)
{
  if (conf.ifile == NULL)
    conf.ifile = (char *) "STDIN";

  pjson (fp, "%.*s\"%s\": \"", sp, TAB, OVERALL_LOG);
  escape_json_output (fp, conf.ifile);
  pjson (fp, "\"");
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

  popen_obj_attr (fp, "hits", sp);
  /* print hits */
  pskeyival (fp, "count", nmetrics->hits, isp, 0);
  /* print hits percent */
  pskeyfval (fp, "percent", nmetrics->hits_perc, isp, 1);
  pclose_obj (fp, sp, 0);
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

  popen_obj_attr (fp, "visitors", sp);
  /* print visitors */
  pskeyival (fp, "count", nmetrics->visitors, isp, 0);
  /* print visitors percent */
  pskeyfval (fp, "percent", nmetrics->visitors_perc, isp, 1);
  pclose_obj (fp, sp, 0);
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

  popen_obj_attr (fp, "bytes", sp);
  /* print bandwidth */
  pskeyu64val (fp, "count", nmetrics->bw.nbw, isp, 0);
  /* print bandwidth percent */
  pskeyfval (fp, "percent", nmetrics->bw_perc, isp, 1);
  pclose_obj (fp, sp, 0);
}

/* Output average time served data.
 *
 * On success, data is outputted. */
static void
pavgts (FILE * fp, GMetrics * nmetrics, int sp)
{
  if (!conf.serve_usecs)
    return;
  pskeyu64val (fp, "avgts", nmetrics->avgts.nts, sp, 0);
}

/* Output cumulative time served data.
 *
 * On success, data is outputted. */
static void
pcumts (FILE * fp, GMetrics * nmetrics, int sp)
{
  if (!conf.serve_usecs)
    return;
  pskeyu64val (fp, "cumts", nmetrics->cumts.nts, sp, 0);
}

/* Output maximum time served data.
 *
 * On success, data is outputted. */
static void
pmaxts (FILE * fp, GMetrics * nmetrics, int sp)
{
  if (!conf.serve_usecs)
    return;
  pskeyu64val (fp, "maxts", nmetrics->maxts.nts, sp, 0);
}

/* Output request method data.
i *
 * On success, data is outputted. */
static void
pmethod (FILE * fp, GMetrics * nmetrics, int sp)
{
  /* request method */
  if (conf.append_method && nmetrics->method) {
    pskeysval (fp, "method", nmetrics->method, sp, 0);
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
    pskeysval (fp, "protocol", nmetrics->protocol, sp, 0);
  }
}

/* Output the hits meta data object.
 *
 * If no metadata found, it simply returns.
 * On success, meta data is outputted. */
static void
pmeta_data_hits (FILE * fp, GModule module, int sp)
{
  int isp = 0;
  int max = 0, min = 0;

  ht_get_hits_min_max (module, &min, &max);

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  popen_obj_attr (fp, "hits", sp);
  pskeyu64val (fp, "count", ht_get_meta_data (module, "hits"), isp, 0);
  pskeyival (fp, "max", max, isp, 0);
  pskeyival (fp, "min", min, isp, 1);
  pclose_obj (fp, sp, 1);
}

/* Output the visitors meta data object.
 *
 * If no metadata found, it simply returns.
 * On success, meta data is outputted. */
static void
pmeta_data_visitors (FILE * fp, GModule module, int sp)
{
  int isp = 0;
  int max = 0, min = 0;

  ht_get_visitors_min_max (module, &min, &max);

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  popen_obj_attr (fp, "visitors", sp);
  pskeyu64val (fp, "count", ht_get_meta_data (module, "visitors"), isp, 0);
  pskeyival (fp, "max", max, isp, 0);
  pskeyival (fp, "min", min, isp, 1);
  pclose_obj (fp, sp, 0);
}

/* Output the bytes meta data object.
 *
 * If no metadata found, it simply returns.
 * On success, meta data is outputted. */
static void
pmeta_data_bw (FILE * fp, GModule module, int sp)
{
  int isp = 0;
  uint64_t max = 0, min = 0;

  if (!conf.bandwidth)
    return;

  ht_get_bw_min_max (module, &min, &max);

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  popen_obj_attr (fp, "bytes", sp);
  pskeyu64val (fp, "count", ht_get_meta_data (module, "bytes"), isp, 0);
  pskeyu64val (fp, "max", max, isp, 0);
  pskeyu64val (fp, "min", min, isp, 1);
  pclose_obj (fp, sp, 0);
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

  popen_obj_attr (fp, "avgts", sp);
  pskeyu64val (fp, "avg", avg, isp, 1);
  pclose_obj (fp, sp, 0);
}

/* Output the cumulative time served meta data object.
 *
 * If no metadata found, it simply returns.
 * On success, meta data is outputted. */
static void
pmeta_data_cumts (FILE * fp, GModule module, int sp)
{
  int isp = 0;
  uint64_t max = 0, min = 0;

  if (!conf.serve_usecs)
    return;

  ht_get_cumts_min_max (module, &min, &max);

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;


  popen_obj_attr (fp, "cumts", sp);
  pskeyu64val (fp, "count", ht_get_meta_data (module, "cumts"), isp, 0);
  pskeyu64val (fp, "max", max, isp, 0);
  pskeyu64val (fp, "min", min, isp, 1);
  pclose_obj (fp, sp, 0);
}

/* Output the maximum time served meta data object.
 *
 * If no metadata found, it simply returns.
 * On success, meta data is outputted. */
static void
pmeta_data_maxts (FILE * fp, GModule module, int sp)
{
  int isp = 0;
  uint64_t max = 0, min = 0;

  if (!conf.serve_usecs)
    return;

  ht_get_maxts_min_max (module, &min, &max);

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  popen_obj_attr (fp, "maxts", sp);
  pskeyu64val (fp, "count", ht_get_meta_data (module, "maxts"), isp, 0);
  pskeyu64val (fp, "max", max, isp, 0);
  pskeyu64val (fp, "min", min, isp, 1);
  pclose_obj (fp, sp, 0);
}

/* Entry point to output panel's metadata. */
static void
print_meta_data (FILE * fp, GHolder * h, int sp)
{
  int isp = 0, iisp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1, iisp = sp + 2;

  popen_obj_attr (fp, "metadata", isp);

  pmeta_data_avgts (fp, h->module, iisp);
  pmeta_data_cumts (fp, h->module, iisp);
  pmeta_data_maxts (fp, h->module, iisp);
  pmeta_data_bw (fp, h->module, iisp);
  pmeta_data_visitors (fp, h->module, iisp);
  pmeta_data_hits (fp, h->module, iisp);

  pclose_obj (fp, isp, 0);
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

/* Add the given user agent value into our array of GAgents.
 *
 * On error, 1 is returned.
 * On success, the user agent is added to the array and 0 is returned. */
static int
fill_host_agents (void *val, void *user_data)
{
  GAgents *agents = user_data;
  char *agent = ht_get_host_agent_val ((*(int *) val));

  if (agent == NULL)
    return 1;

  agents->items[agents->size].agent = agent;
  agents->size++;

  return 0;
}

/* Iterate over the list of agents */
static void
load_host_agents (void *list, void *user_data, GO_UNUSED int count)
{
  GSLList *lst = list;
  GAgents *agents = user_data;

  agents->items = new_gagent_item (count);
  list_foreach (lst, fill_host_agents, agents);
}

/* A wrapper function to ouput an array of user agents for each host. */
static void
process_host_agents (FILE * fp, GHolderItem * item, int iisp)
{
  GAgents *agents = new_gagents ();
  int i, n = 0, iiisp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    iiisp = iisp + 1;

  if (set_host_agents (item->metrics->data, load_host_agents, agents) == 1)
    return;

  pjson (fp, ",%.*s%.*s\"items\": [%.*s", nlines, NL, iisp, TAB, nlines, NL);

  n = agents->size > 10 ? 10 : agents->size;
  for (i = 0; i < n; ++i) {
    pjson (fp, "%.*s\"", iiisp, TAB);
    escape_json_output (fp, agents->items[i].agent);
    if (i == n - 1)
      pjson (fp, "\"");
    else
      pjson (fp, "\",%.*s", nlines, NL);
  }

  pclose_arr (fp, iisp, 1);

  /* clean stuff up */
  free_agents_array (agents);
}

/* A wrapper function to ouput children nodes. */
static void
print_json_sub_items (FILE * fp, GHolderItem * item, GPercTotals totals,
                      int size, int iisp)
{
  GMetrics *nmetrics;
  GSubItem *iter;
  GSubList *sl = item->sub_list;
  int i = 0, iiisp = 0, iiiisp = 0;

  /* no sub items, nothing to output */
  if (size == 0)
    return;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    iiisp = iisp + 1, iiiisp = iiisp + 1;

  if (sl == NULL)
    return;

  pjson (fp, ",%.*s%.*s\"items\": [%.*s", nlines, NL, iisp, TAB, nlines, NL);
  for (iter = sl->head; iter; iter = iter->next, i++) {
    set_data_metrics (iter->metrics, &nmetrics, totals);

    popen_obj (fp, iiisp);
    print_json_block (fp, nmetrics, iiiisp);
    pclose_obj (fp, iiisp, (i == sl->size - 1));
    free (nmetrics);
  }
  pclose_arr (fp, iisp, 1);
}

/* A wrapper function to ouput geolocation fields for the given host. */
static void
print_json_host_geo (FILE * fp, GSubList * sl, int iisp)
{
  GSubItem *iter;
  int i;
  static const char *key[] = {
    "country",
    "city",
    "hostname",
  };

  pjson (fp, ",%.*s", nlines, NL);

  /* Iterate over child properties (country, city, etc) and print them out */
  for (i = 0, iter = sl->head; iter; iter = iter->next, i++) {
    pjson (fp, "%.*s\"%s\": \"", iisp, TAB, key[iter->metrics->id]);
    escape_json_output (fp, iter->metrics->data);
    pjson (fp, (i != sl->size - 1) ? "\",%.*s" : "\"", nlines, NL);
  }
}

/* Ouput Geolocation data and the IP's hostname. */
static void
print_json_host_items (FILE * fp, GHolderItem * item, GPercTotals totals,
                       int size, int iisp)
{
  (void) totals;
  /* print geolocation fields */
  if (size > 0 && item->sub_list != NULL)
    print_json_host_geo (fp, item->sub_list, iisp);

  /* print list of user agents */
  if (conf.list_agents)
    process_host_agents (fp, item, iisp);
}

/* Ouput data and determine if there are children nodes. */
static void
print_data_metrics (FILE * fp, GHolder * h, GPercTotals totals, int sp,
                    const struct GPanel_ *panel)
{
  GMetrics *nmetrics;
  int i, isp = 0, iisp = 0, iiisp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1, iisp = sp + 2, iiisp = sp + 3;

  popen_arr_attr (fp, "data", isp);
  /* output data metrics */
  for (i = 0; i < h->idx; i++) {
    set_data_metrics (h->items[i].metrics, &nmetrics, totals);

    /* open data metric block */
    popen_obj (fp, iisp);
    /* output data metric block */
    print_json_block (fp, nmetrics, iiisp);
    /* if there are children nodes, spit them out */
    if (panel->subitems)
      panel->subitems (fp, h->items + i, totals, h->sub_items_size, iiisp);
    /* close data metric block */
    pclose_obj (fp, iisp, (i == h->idx - 1));

    free (nmetrics);
  }
  pclose_arr (fp, isp, 1);
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
  popen_obj_attr (fp, module_to_id (h->module), sp);
  /* output panel metadata */
  print_meta_data (fp, h, sp);
  /* output panel data */
  print_data_metrics (fp, h, totals, sp, panel);
  /* output close panel attribute */
  pclose_obj (fp, sp, 1);
}

/* Get the number of available panels.
 *
 * On success, the total number of available panels is returned . */
static int
num_panels (void)
{
  size_t idx = 0, npanels = 0;

  FOREACH_MODULE (idx, module_list)
    npanels++;

  return npanels;
}

/* Output overall data. */
static void
print_json_summary (FILE * fp, GLog * logger)
{
  int sp = 0, isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    sp = 1, isp = 2;

  popen_obj_attr (fp, GENER_ID, sp);
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
  pclose_obj (fp, sp, num_panels () > 0 ? 0 : 1);
}

/* Iterate over all panels and generate json output. */
static void
init_json_output (FILE * fp, GLog * logger, GHolder * holder)
{
  GModule module;
  const GPanel *panel = NULL;
  size_t idx = 0, npanels = num_panels (), cnt = 0;

  GPercTotals totals = {
    .hits = logger->valid,
    .visitors = ht_get_size_uniqmap (VISITORS),
    .bw = logger->resp_size,
  };

  popen_obj (fp, 0);
  print_json_summary (fp, logger);

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    if (!(panel = panel_lookup (module)))
      continue;
    panel->render (fp, holder + module, totals, panel);
    pjson (fp, (cnt++ != npanels - 1) ? ",%.*s" : "%.*s", nlines, NL);
  }

  pclose_obj (fp, 0, 1);
}

/* Open and write to a dynamically sized output buffer.
 *
 * On success, the newly allocated buffer is returned . */
char *
get_json (GLog * logger, GHolder * holder)
{
  FILE *out;
  char *buf;
  size_t size;

  out = open_memstream (&buf, &size);
  init_json_output (out, logger, holder);
  fflush (out);
  fclose (out);

  return buf;
}

/* Entry point to generate a json report writing it to the fp */
void
output_json (GLog * logger, GHolder * holder, const char *filename)
{
  FILE *fp;

  if (filename != NULL)
    fp = fopen (filename, "w");
  else
    fp = stdout;

  /* use new lines to prettify output */
  if (conf.json_pretty_print)
    nlines = 1;
  init_json_output (fp, logger, holder);

  fclose (fp);
}
