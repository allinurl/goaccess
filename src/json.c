/**
 * output.c -- output json to the standard output stream
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2022 Gerardo Orellana <hello @ goaccess.io>
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

#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>

#include "json.h"

#include "error.h"
#include "gkhash.h"
#include "settings.h"
#include "ui.h"
#include "util.h"
#include "websocket.h"
#include "xmalloc.h"

typedef struct GPanel_ {
  GModule module;
  void (*render) (GJSON * json, GHolder * h, GPercTotals totals, const struct GPanel_ *);
  void (*subitems) (GJSON * json, GHolderItem * item, GPercTotals totals, int size, int iisp);
} GPanel;

/* number of new lines (applicable fields) */
static int nlines = 0;
/* escape HTML in JSON data values */
static int escape_html_output = 0;

static void print_json_data (GJSON * json, GHolder * h, GPercTotals totals,
                             const struct GPanel_ *);
static void print_json_host_items (GJSON * json, GHolderItem * item,
                                   GPercTotals totals, int size, int iisp);
static void print_json_sub_items (GJSON * json, GHolderItem * item,
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
  {STATUS_CODES        , print_json_data , print_json_sub_items } ,
  {REMOTE_USER         , print_json_data , NULL } ,
  {CACHE_STATUS        , print_json_data , NULL } ,
#ifdef HAVE_GEOLOCATION
  {GEO_LOCATION        , print_json_data , print_json_sub_items } ,
  {ASN                 , print_json_data , NULL} ,
#endif
  {MIME_TYPE           , print_json_data , print_json_sub_items } ,
  {TLS_TYPE            , print_json_data , print_json_sub_items } ,

};
/* *INDENT-ON* */

/* Get panel output data for the given module.
 *
 * If not found, NULL is returned.
 * On success, panel data is returned . */
static GPanel *
panel_lookup (GModule module) {
  int i, num_panels = ARRAY_SIZE (paneling);

  for (i = 0; i < num_panels; i++) {
    if (paneling[i].module == module)
      return &paneling[i];
  }
  return NULL;
}

/* Allocate memory for a new GJSON instance.
 *
 * On success, the newly allocated GJSON is returned . */
static GJSON *
new_gjson (void) {
  GJSON *json = xcalloc (1, sizeof (GJSON));

  return json;
}

/* Free malloc'd GJSON resources. */
static void
free_json (GJSON * json) {
  if (!json)
    return;

  free (json->buf);
  free (json);
}

/* Set number of new lines when --json-pretty-print is used. */
void
set_json_nlines (int newline) {
  nlines = newline;
}

/* Make sure that we have enough storage to write "len" bytes at the
 * current offset. */
static void
set_json_buffer (GJSON * json, int len) {
  char *tmp = NULL;
  /* Maintain a null byte at the end of the buffer */
  size_t need = json->offset + len + 1, newlen = 0;

  if (need <= json->size)
    return;

  if (json->size == 0) {
    newlen = INIT_BUF_SIZE;
  } else {
    newlen = json->size;
    newlen += newlen / 2;       /* resize by 3/2 */
  }

  if (newlen < need)
    newlen = need;

  tmp = realloc (json->buf, newlen);
  if (tmp == NULL) {
    free_json (json);
    FATAL (("Unable to realloc JSON buffer.\n"));
  }
  json->buf = tmp;
  json->size = newlen;
}

#pragma GCC diagnostic ignored "-Wformat-nonliteral"
/* A wrapper function to write a formatted string and expand the
 * buffer if necessary.
 *
 * On success, data is outputted. */
__attribute__((format (printf, 2, 3)))
  static void pjson (GJSON * json, const char *fmt, ...) {
  int len = 0;
  va_list args;

  va_start (args, fmt);
  if ((len = vsnprintf (NULL, 0, fmt, args)) < 0)
    FATAL (("Unable to write JSON formatted data.\n"));
  va_end (args);

  /* malloc/realloc buffer as needed */
  set_json_buffer (json, len);

  va_start (args, fmt); /* restart args */
  vsprintf (json->buf + json->offset, fmt, args);
  va_end (args);
  json->offset += len;
  }

/* A wrapper function to output a formatted string to a file pointer.
 *
 * On success, data is outputted. */
void
fpjson (FILE * fp, const char *fmt, ...) {
  va_list args;

  va_start (args, fmt);
  vfprintf (fp, fmt, args);
  va_end (args);
}

#pragma GCC diagnostic warning "-Wformat-nonliteral"

/* Escape all other characters accordingly. */
static void
escape_json_other (GJSON * json, const char **s) {
  /* Since JSON data is bootstrapped into the HTML document of a report,
   * then we perform the following four translations in case weird stuff
   * is put into the document.
   *
   * Note: The following scenario assumes that the user manually makes
   * the HTML report a PHP file (GoAccess doesn't allow the creation of a
   * PHP file):
   *
   * /index.html<?php eval(base_decode('iZWNobyAiPGgxPkhFTExPPC9oMT4iOw=='));?>
   */
  if (escape_html_output) {
    switch (**s) {
    case '\'':
      pjson (json, "&#39;");
      return;
    case '&':
      pjson (json, "&amp;");
      return;
    case '<':
      pjson (json, "&lt;");
      return;
    case '>':
      pjson (json, "&gt;");
      return;
    }
  }

  if ((uint8_t) ** s <= 0x1f) {
    /* Control characters (U+0000 through U+001F) */
    char buf[8];
    snprintf (buf, sizeof buf, "\\u%04x", **s);
    pjson (json, "%s", buf);
  } else if ((uint8_t) ** s == 0xe2 && (uint8_t) * (*s + 1) == 0x80 &&
             (uint8_t) * (*s + 2) == 0xa8) {
    /* Line separator (U+2028) - 0xE2 0x80 0xA8 */
    pjson (json, "\\u2028");
    *s += 2;
  } else if ((uint8_t) ** s == 0xe2 && (uint8_t) * (*s + 1) == 0x80 &&
             (uint8_t) * (*s + 2) == 0xa9) {
    /* Paragraph separator (U+2019) - 0xE2 0x80 0xA9 */
    pjson (json, "\\u2029");
    *s += 2;
  } else {
    char buf[2];
    snprintf (buf, sizeof buf, "%c", **s);
    pjson (json, "%s", buf);
  }
}

/* Escape and write to a valid JSON buffer.
 *
 * On success, escaped JSON data is outputted. */
static void
escape_json_output (GJSON * json, const char *s) {
  while (*s) {
    switch (*s) {
      /* These are required JSON special characters that need to be escaped. */
    case '"':
      pjson (json, "\\\"");
      break;
    case '\\':
      pjson (json, "\\\\");
      break;
    case '\b':
      pjson (json, "\\b");
      break;
    case '\f':
      pjson (json, "\\f");
      break;
    case '\n':
      pjson (json, "\\n");
      break;
    case '\r':
      pjson (json, "\\r");
      break;
    case '\t':
      pjson (json, "\\t");
      break;
    case '/':
      pjson (json, "\\/");
      break;
    default:
      escape_json_other (json, &s);
      break;
    }
    s++;
  }
}

/* Write to a buffer a JSON a key/value pair. */
static void
pskeysval (GJSON * json, const char *key, const char *val, int sp, int last) {
  if (!last)
    pjson (json, "%.*s\"%s\": \"%s\",%.*s", sp, TAB, key, val, nlines, NL);
  else
    pjson (json, "%.*s\"%s\": \"%s\"", sp, TAB, key, val);
}

/* Output a JSON string key, array value pair. */
void
fpskeyaval (FILE * fp, const char *key, const char *val, int sp, int last) {
  if (!last)
    fpjson (fp, "%.*s\"%s\": %s,%.*s", sp, TAB, key, val, nlines, NL);
  else
    fpjson (fp, "%.*s\"%s\": %s", sp, TAB, key, val);
}

/* Output a JSON a key/value pair. */
void
fpskeysval (FILE * fp, const char *key, const char *val, int sp, int last) {
  if (!last)
    fpjson (fp, "%.*s\"%s\": \"%s\",%.*s", sp, TAB, key, val, nlines, NL);
  else
    fpjson (fp, "%.*s\"%s\": \"%s\"", sp, TAB, key, val);
}

/* Output a JSON string key, int value pair. */
void
fpskeyival (FILE * fp, const char *key, int val, int sp, int last) {
  if (!last)
    fpjson (fp, "%.*s\"%s\": %d,%.*s", sp, TAB, key, val, nlines, NL);
  else
    fpjson (fp, "%.*s\"%s\": %d", sp, TAB, key, val);
}

/* Write to a buffer a JSON string key, uint64_t value pair. */
static void
pskeyu64val (GJSON * json, const char *key, uint64_t val, int sp, int last) {
  if (!last)
    pjson (json, "%.*s\"%s\": %" PRIu64 ",%.*s", sp, TAB, key, val, nlines, NL);
  else
    pjson (json, "%.*s\"%s\": %" PRIu64 "", sp, TAB, key, val);
}

/* Write to a buffer a JSON string key, int value pair. */
static void
pskeyfval (GJSON * json, const char *key, float val, int sp, int last) {
  if (!last)
    pjson (json, "%.*s\"%s\": \"%05.2f\",%.*s", sp, TAB, key, val, nlines, NL);
  else
    pjson (json, "%.*s\"%s\": \"%05.2f\"", sp, TAB, key, val);
}

/* Write to a buffer the open block item object. */
static void
popen_obj (GJSON * json, int iisp) {
  /* open data metric block */
  pjson (json, "%.*s{%.*s", iisp, TAB, nlines, NL);
}

/* Output the open block item object. */
void
fpopen_obj (FILE * fp, int iisp) {
  /* open data metric block */
  fpjson (fp, "%.*s{%.*s", iisp, TAB, nlines, NL);
}

/* Write to a buffer a JSON open object attribute. */
static void
popen_obj_attr (GJSON * json, const char *attr, int sp) {
  /* open object attribute */
  pjson (json, "%.*s\"%s\": {%.*s", sp, TAB, attr, nlines, NL);
}

/* Output a JSON open object attribute. */
void
fpopen_obj_attr (FILE * fp, const char *attr, int sp) {
  /* open object attribute */
  fpjson (fp, "%.*s\"%s\": {%.*s", sp, TAB, attr, nlines, NL);
}

/* Close JSON object. */
static void
pclose_obj (GJSON * json, int iisp, int last) {
  if (!last)
    pjson (json, "%.*s%.*s},%.*s", nlines, NL, iisp, TAB, nlines, NL);
  else
    pjson (json, "%.*s%.*s}", nlines, NL, iisp, TAB);
}

/* Close JSON object. */
void
fpclose_obj (FILE * fp, int iisp, int last) {
  if (!last)
    fpjson (fp, "%.*s%.*s},%.*s", nlines, NL, iisp, TAB, nlines, NL);
  else
    fpjson (fp, "%.*s%.*s}", nlines, NL, iisp, TAB);
}

/* Write to a buffer a JSON open array attribute. */
static void
popen_arr_attr (GJSON * json, const char *attr, int sp) {
  /* open object attribute */
  pjson (json, "%.*s\"%s\": [%.*s", sp, TAB, attr, nlines, NL);
}

/* Output a JSON open array attribute. */
void
fpopen_arr_attr (FILE * fp, const char *attr, int sp) {
  /* open object attribute */
  fpjson (fp, "%.*s\"%s\": [%.*s", sp, TAB, attr, nlines, NL);
}

/* Close the data array. */
static void
pclose_arr (GJSON * json, int sp, int last) {
  if (!last)
    pjson (json, "%.*s%.*s],%.*s", nlines, NL, sp, TAB, nlines, NL);
  else
    pjson (json, "%.*s%.*s]", nlines, NL, sp, TAB);
}

/* Close the data array. */
void
fpclose_arr (FILE * fp, int sp, int last) {
  if (!last)
    fpjson (fp, "%.*s%.*s],%.*s", nlines, NL, sp, TAB, nlines, NL);
  else
    fpjson (fp, "%.*s%.*s]", nlines, NL, sp, TAB);
}

/* Write to a buffer the date and time for the overall object. */
static void
poverall_datetime (GJSON * json, int sp) {
  char now[DATE_TIME];

  generate_time ();
  strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S %z", &now_tm);

  pskeysval (json, OVERALL_DATETIME, now, sp, 0);
}

/* Write to a buffer the date and time for the overall object. */
static void
poverall_start_end_date (GJSON * json, GHolder * h, int sp) {
  char *start = NULL, *end = NULL;

  if (h->idx == 0 || get_start_end_parsing_dates (&start, &end, "%d/%b/%Y"))
    return;

  pskeysval (json, OVERALL_STARTDATE, start, sp, 0);
  pskeysval (json, OVERALL_ENDDATE, end, sp, 0);

  free (end);
  free (start);
}

/* Write to a buffer date and time for the overall object. */
static void
poverall_requests (GJSON * json, int sp) {
  pskeyu64val (json, OVERALL_REQ, ht_get_processed (), sp, 0);
}

/* Write to a buffer the number of valid requests under the overall
 * object. */
static void
poverall_valid_reqs (GJSON * json, int sp) {
  pskeyu64val (json, OVERALL_VALID, ht_sum_valid (), sp, 0);
}

/* Write to a buffer the number of invalid requests under the overall
 * object. */
static void
poverall_invalid_reqs (GJSON * json, int sp) {
  pskeyu64val (json, OVERALL_FAILED, ht_get_invalid (), sp, 0);
}

/* Write to a buffer the total processed time under the overall
 * object. */
static void
poverall_processed_time (GJSON * json, int sp) {
  pskeyu64val (json, OVERALL_GENTIME, ht_get_processing_time (), sp, 0);
}

/* Write to a buffer the total number of unique visitors under the
 * overall object. */
static void
poverall_visitors (GJSON * json, int sp) {
  pskeyu64val (json, OVERALL_VISITORS, ht_get_size_uniqmap (VISITORS), sp, 0);
}

/* Write to a buffer the total number of unique files under the
 * overall object. */
static void
poverall_files (GJSON * json, int sp) {
  pskeyu64val (json, OVERALL_FILES, ht_get_size_datamap (REQUESTS), sp, 0);
}

/* Write to a buffer the total number of excluded requests under the
 * overall object. */
static void
poverall_excluded (GJSON * json, int sp) {
  pskeyu64val (json, OVERALL_EXCL_HITS, ht_get_excluded_ips (), sp, 0);
}

/* Write to a buffer the number of referrers under the overall object. */
static void
poverall_refs (GJSON * json, int sp) {
  pskeyu64val (json, OVERALL_REF, ht_get_size_datamap (REFERRERS), sp, 0);
}

/* Write to a buffer the number of not found (404s) under the overall
 * object. */
static void
poverall_notfound (GJSON * json, int sp) {
  pskeyu64val (json, OVERALL_NOTFOUND, ht_get_size_datamap (NOT_FOUND), sp, 0);
}

/* Write to a buffer the number of static files (jpg, pdf, etc) under
 * the overall object. */
static void
poverall_static_files (GJSON * json, int sp) {
  pskeyu64val (json, OVERALL_STATIC, ht_get_size_datamap (REQUESTS_STATIC), sp, 0);
}

/* Write to a buffer the size of the log being parsed under the
 * overall object. */
static void
poverall_log_size (GJSON * json, int sp) {
  pjson (json, "%.*s\"%s\": %jd,%.*s", sp, TAB, OVERALL_LOGSIZE,
         (intmax_t) get_log_sizes (), nlines, NL);
}

/* Write to a buffer the total bandwidth consumed under the overall
 * object. */
static void
poverall_bandwidth (GJSON * json, int sp) {
  pskeyu64val (json, OVERALL_BANDWIDTH, ht_sum_bw (), sp, 0);
}

static void
poverall_log_path (GJSON * json, int idx, int isp) {
  pjson (json, "%.*s\"", isp, TAB);
  if (conf.filenames[idx][0] == '-' && conf.filenames[idx][1] == '\0')
    pjson (json, "STDIN");
  else
    escape_json_output (json, conf.filenames[idx]);
  pjson (json, conf.filenames_idx - 1 != idx ? "\",\n" : "\"");
}

/* Write to a buffer the path of the log being parsed under the
 * overall object. */
static void
poverall_log (GJSON * json, int sp) {
  int idx, isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  popen_arr_attr (json, OVERALL_LOG, sp);
  for (idx = 0; idx < conf.filenames_idx; ++idx)
    poverall_log_path (json, idx, isp);
  pclose_arr (json, sp, 1);
}

/* Write to a buffer hits data. */
static void
phits (GJSON * json, GMetrics * nmetrics, int sp) {
  int isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  popen_obj_attr (json, "hits", sp);
  /* print hits */
  pskeyu64val (json, "count", nmetrics->hits, isp, 0);
  /* print hits percent */
  pskeyfval (json, "percent", nmetrics->hits_perc, isp, 1);
  pclose_obj (json, sp, 0);
}

/* Write to a buffer visitors data. */
static void
pvisitors (GJSON * json, GMetrics * nmetrics, int sp) {
  int isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  popen_obj_attr (json, "visitors", sp);
  /* print visitors */
  pskeyu64val (json, "count", nmetrics->visitors, isp, 0);
  /* print visitors percent */
  pskeyfval (json, "percent", nmetrics->visitors_perc, isp, 1);
  pclose_obj (json, sp, 0);
}

/* Write to a buffer bandwidth data. */
static void
pbw (GJSON * json, GMetrics * nmetrics, int sp) {
  int isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  if (!conf.bandwidth)
    return;

  popen_obj_attr (json, "bytes", sp);
  /* print bandwidth */
  pskeyu64val (json, "count", nmetrics->bw.nbw, isp, 0);
  /* print bandwidth percent */
  pskeyfval (json, "percent", nmetrics->bw_perc, isp, 1);
  pclose_obj (json, sp, 0);
}

/* Write to a buffer average time served data. */
static void
pavgts (GJSON * json, GMetrics * nmetrics, int sp) {
  if (!conf.serve_usecs)
    return;
  pskeyu64val (json, "avgts", nmetrics->avgts.nts, sp, 0);
}

/* Write to a buffer cumulative time served data. */
static void
pcumts (GJSON * json, GMetrics * nmetrics, int sp) {
  if (!conf.serve_usecs)
    return;
  pskeyu64val (json, "cumts", nmetrics->cumts.nts, sp, 0);
}

/* Write to a buffer maximum time served data. */
static void
pmaxts (GJSON * json, GMetrics * nmetrics, int sp) {
  if (!conf.serve_usecs)
    return;
  pskeyu64val (json, "maxts", nmetrics->maxts.nts, sp, 0);
}

/* Write to a buffer request method data. */
static void
pmethod (GJSON * json, GMetrics * nmetrics, int sp) {
  /* request method */
  if (conf.append_method && nmetrics->method) {
    pskeysval (json, "method", nmetrics->method, sp, 0);
  }
}

/* Write to a buffer protocol method data. */
static void
pprotocol (GJSON * json, GMetrics * nmetrics, int sp) {
  /* request protocol */
  if (conf.append_protocol && nmetrics->protocol) {
    pskeysval (json, "protocol", nmetrics->protocol, sp, 0);
  }
}

static void
pmeta_i64_data (GJSON * json, GHolder * h, void (*cb) (GModule, uint64_t *, uint64_t *),
                const char *key, int show_perc, int sp) {
  int isp = 0;
  uint64_t max = 0, min = 0, total = ht_get_meta_data (h->module, key);
  float avg = (total == 0 ? 0 : (((float) total) / h->ht_size));

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  cb (h->module, &min, &max);

  popen_obj_attr (json, "total", sp);
  pskeyu64val (json, "value", total, isp, 1);
  pclose_obj (json, sp, 0);

  popen_obj_attr (json, "avg", sp);
  pskeyu64val (json, "value", avg, isp, !show_perc);
  if (show_perc) {
    pskeyfval (json, "percent", get_percentage (total, avg), isp, 1);
  }
  pclose_obj (json, sp, 0);

  popen_obj_attr (json, "max", sp);
  pskeyu64val (json, "value", max, isp, !show_perc);
  if (show_perc) {
    pskeyfval (json, "percent", get_percentage (total, max), isp, 1);
  }
  pclose_obj (json, sp, 0);

  popen_obj_attr (json, "min", sp);
  pskeyu64val (json, "value", min, isp, !show_perc);
  if (show_perc) {
    pskeyfval (json, "percent", get_percentage (total, min), isp, 1);
  }
  pclose_obj (json, sp, 1);
}

static void
pmeta_i32_data (GJSON * json, GHolder * h, void (*cb) (GModule, uint32_t *, uint32_t *),
                const char *key, int show_perc, int sp) {
  int isp = 0;
  uint32_t max = 0, min = 0, total = ht_get_meta_data (h->module, key);
  float avg = (total == 0 ? 0 : (((float) total) / h->ht_size));

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  cb (h->module, &min, &max);

  popen_obj_attr (json, "total", sp);
  pskeyu64val (json, "value", total, isp, 1);
  pclose_obj (json, sp, 0);

  popen_obj_attr (json, "avg", sp);
  pskeyu64val (json, "value", avg, isp, !show_perc);
  if (show_perc) {
    pskeyfval (json, "percent", get_percentage (total, avg), isp, 1);
  }
  pclose_obj (json, sp, 0);

  popen_obj_attr (json, "max", sp);
  pskeyu64val (json, "value", max, isp, !show_perc);
  if (show_perc) {
    pskeyfval (json, "percent", get_percentage (total, max), isp, 1);
  }
  pclose_obj (json, sp, 0);

  popen_obj_attr (json, "min", sp);
  pskeyu64val (json, "value", min, isp, !show_perc);
  if (show_perc) {
    pskeyfval (json, "percent", get_percentage (total, min), isp, 1);
  }
  pclose_obj (json, sp, 1);
}

/* Write to a buffer the hits meta data object. */
static void
pmeta_data_unique (GJSON * json, int ht_size, int sp) {
  int isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  popen_obj_attr (json, "data", sp);

  popen_obj_attr (json, "total", isp);
  pskeyu64val (json, "value", ht_size, isp + 1, 1);
  pclose_obj (json, isp, 1);

  pclose_obj (json, sp, 1);
}

/* Write to a buffer the hits meta data object. */
static void
pmeta_data_hits (GJSON * json, GHolder * h, int sp) {
  int isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  popen_obj_attr (json, "hits", sp);
  pmeta_i32_data (json, h, ht_get_hits_min_max, "hits", 1, isp);
  pclose_obj (json, sp, 0);
}

/* Write to a buffer the visitors meta data object. */
static void
pmeta_data_visitors (GJSON * json, GHolder * h, int sp) {
  int isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  popen_obj_attr (json, "visitors", sp);
  pmeta_i32_data (json, h, ht_get_visitors_min_max, "visitors", 1, isp);
  pclose_obj (json, sp, 0);
}

/* Write to a buffer the bytes meta data object. */
static void
pmeta_data_bw (GJSON * json, GHolder * h, int sp) {
  int isp = 0;
  if (!conf.bandwidth)
    return;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  popen_obj_attr (json, "bytes", sp);
  pmeta_i64_data (json, h, ht_get_bw_min_max, "bytes", 1, isp);
  pclose_obj (json, sp, 0);
}

/* Write to a buffer the average of the average time served meta data
 * object. */
static void
pmeta_data_avgts (GJSON * json, GHolder * h, int sp) {
  int isp = 0;
  uint64_t avg = 0, hits = 0, cumts = 0;

  if (!conf.serve_usecs)
    return;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  cumts = ht_get_meta_data (h->module, "cumts");
  hits = ht_get_meta_data (h->module, "hits");
  if (hits > 0)
    avg = cumts / hits;

  popen_obj_attr (json, "avgts", sp);

  popen_obj_attr (json, "avg", isp);
  pskeyu64val (json, "value", avg, isp + 1, 1);
  pclose_obj (json, isp, 1);

  pclose_obj (json, sp, 0);
}

/* Write to a buffer the cumulative time served meta data object. */
static void
pmeta_data_cumts (GJSON * json, GHolder * h, int sp) {
  int isp = 0;

  if (!conf.serve_usecs)
    return;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  popen_obj_attr (json, "cumts", sp);
  pmeta_i64_data (json, h, ht_get_cumts_min_max, "cumts", 0, isp);
  pclose_obj (json, sp, 0);
}

/* Write to a buffer the maximum time served meta data object. */
static void
pmeta_data_maxts (GJSON * json, GHolder * h, int sp) {
  int isp = 0;
  if (!conf.serve_usecs)
    return;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  popen_obj_attr (json, "maxts", sp);
  pmeta_i64_data (json, h, ht_get_maxts_min_max, "maxts", 0, isp);
  pclose_obj (json, sp, 0);
}

/* Entry point to output panel's metadata. */
static void
print_meta_data (GJSON * json, GHolder * h, int sp) {
  int isp = 0, iisp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1, iisp = sp + 2;

  popen_obj_attr (json, "metadata", isp);

  pmeta_data_avgts (json, h, iisp);
  pmeta_data_cumts (json, h, iisp);
  pmeta_data_maxts (json, h, iisp);
  pmeta_data_bw (json, h, iisp);
  pmeta_data_visitors (json, h, iisp);
  pmeta_data_hits (json, h, iisp);
  pmeta_data_unique (json, h->ht_size, iisp);

  pclose_obj (json, isp, 0);
}

/* A wrapper function to output data metrics per panel. */
static void
print_json_block (GJSON * json, GMetrics * nmetrics, int sp) {
  /* print hits */
  phits (json, nmetrics, sp);
  /* print visitors */
  pvisitors (json, nmetrics, sp);
  /* print bandwidth */
  pbw (json, nmetrics, sp);

  /* print time served metrics */
  pavgts (json, nmetrics, sp);
  pcumts (json, nmetrics, sp);
  pmaxts (json, nmetrics, sp);

  /* print protocol/method */
  pmethod (json, nmetrics, sp);
  pprotocol (json, nmetrics, sp);

  /* data metric */
  pjson (json, "%.*s\"data\": \"", sp, TAB);
  escape_json_output (json, nmetrics->data);
  pjson (json, "\"");
}

/* A wrapper function to output an array of user agents for each host. */
static void
process_host_agents (GJSON * json, GHolderItem * item, int iisp) {
  GAgents *agents = NULL;
  int i, n = 0, iiisp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    iiisp = iisp + 1;

  /* create a new instance of GMenu and make it selectable */
  if (!(agents = load_host_agents (item->metrics->data)))
    return;

  pjson (json, ",%.*s%.*s\"items\": [%.*s", nlines, NL, iisp, TAB, nlines, NL);

  n = agents->idx > 10 ? 10 : agents->idx;
  for (i = 0; i < n; ++i) {
    pjson (json, "%.*s\"", iiisp, TAB);
    escape_json_output (json, agents->items[i].agent);
    if (i == n - 1)
      pjson (json, "\"");
    else
      pjson (json, "\",%.*s", nlines, NL);
  }

  pclose_arr (json, iisp, 1);

  /* clean stuff up */
  free_agents_array (agents);
}

/* A wrapper function to output children nodes. */
static void
print_json_sub_items (GJSON * json, GHolderItem * item, GPercTotals totals, int size, int iisp) {
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

  pjson (json, ",%.*s%.*s\"items\": [%.*s", nlines, NL, iisp, TAB, nlines, NL);
  for (iter = sl->head; iter; iter = iter->next, i++) {
    set_data_metrics (iter->metrics, &nmetrics, totals);

    popen_obj (json, iiisp);
    print_json_block (json, nmetrics, iiiisp);
    pclose_obj (json, iiisp, (i == sl->size - 1));
    free (nmetrics);
  }
  pclose_arr (json, iisp, 1);
}

/* A wrapper function to output geolocation fields for the given host. */
static void
print_json_host_geo (GJSON * json, GSubList * sl, int iisp) {
  GSubItem *iter;
  int i;
  static const char *key[] = {
    "country",
    "city",
    "asn",
    "hostname",
  };

  pjson (json, ",%.*s", nlines, NL);

  /* Iterate over child properties (country, city, asn, etc) and print them out */
  for (i = 0, iter = sl->head; iter; iter = iter->next, i++) {
    pjson (json, "%.*s\"%s\": \"", iisp, TAB, key[iter->metrics->id]);
    escape_json_output (json, iter->metrics->data);
    pjson (json, (i != sl->size - 1) ? "\",%.*s" : "\"", nlines, NL);
  }
}

/* Output Geolocation data and the IP's hostname. */
static void
print_json_host_items (GJSON * json, GHolderItem * item, GPercTotals totals,
                       int size, int iisp) {
  (void) totals;
  /* print geolocation fields */
  if (size > 0 && item->sub_list != NULL)
    print_json_host_geo (json, item->sub_list, iisp);

  /* print list of user agents */
  if (conf.list_agents)
    process_host_agents (json, item, iisp);
}

/* Output data and determine if there are children nodes. */
static void
print_data_metrics (GJSON * json, GHolder * h, GPercTotals totals, int sp,
                    const struct GPanel_ *panel) {
  GMetrics *nmetrics;
  int i, isp = 0, iisp = 0, iiisp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1, iisp = sp + 2, iiisp = sp + 3;

  popen_arr_attr (json, "data", isp);
  /* output data metrics */
  for (i = 0; i < h->idx; i++) {
    set_data_metrics (h->items[i].metrics, &nmetrics, totals);

    /* open data metric block */
    popen_obj (json, iisp);
    /* output data metric block */
    print_json_block (json, nmetrics, iiisp);
    /* if there are children nodes, spit them out */
    if (panel->subitems)
      panel->subitems (json, h->items + i, totals, h->sub_items_size, iiisp);
    /* close data metric block */
    pclose_obj (json, iisp, (i == h->idx - 1));

    free (nmetrics);
  }
  pclose_arr (json, isp, 1);
}

/* Entry point to output data metrics per panel. */
static void
print_json_data (GJSON * json, GHolder * h, GPercTotals totals, const struct GPanel_ *panel) {
  int sp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    sp = 1;

  /* output open panel attribute */
  popen_obj_attr (json, module_to_id (h->module), sp);
  /* output panel metadata */
  print_meta_data (json, h, sp);
  /* output panel data */
  print_data_metrics (json, h, totals, sp, panel);
  /* output close panel attribute */
  pclose_obj (json, sp, 1);
}

/* Get the number of available panels.
 *
 * On success, the total number of available panels is returned . */
static int
num_panels (void) {
  size_t idx = 0, npanels = 0;

  FOREACH_MODULE (idx, module_list)
    npanels++;

  return npanels;
}

/* Write to a buffer overall data. */
static void
print_json_summary (GJSON * json, GHolder * holder) {
  int sp = 0, isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    sp = 1, isp = 2;

  popen_obj_attr (json, GENER_ID, sp);
  /* generated start/end date */
  poverall_start_end_date (json, holder, isp);
  /* generated date time */
  poverall_datetime (json, isp);
  /* total requests */
  poverall_requests (json, isp);
  /* valid requests */
  poverall_valid_reqs (json, isp);
  /* invalid requests */
  poverall_invalid_reqs (json, isp);
  /* generated time */
  poverall_processed_time (json, isp);
  /* visitors */
  poverall_visitors (json, isp);
  /* files */
  poverall_files (json, isp);
  /* excluded hits */
  poverall_excluded (json, isp);
  /* referrers */
  poverall_refs (json, isp);
  /* not found */
  poverall_notfound (json, isp);
  /* static files */
  poverall_static_files (json, isp);
  /* log size */
  poverall_log_size (json, isp);
  /* bandwidth */
  poverall_bandwidth (json, isp);
  /* log path */
  poverall_log (json, isp);
  pclose_obj (json, sp, num_panels () > 0 ? 0 : 1);
}

/* Iterate over all panels and generate json output. */
static GJSON *
init_json_output (GHolder * holder) {
  GJSON *json = NULL;
  GModule module;
  GPercTotals totals;
  const GPanel *panel = NULL;
  size_t idx = 0, npanels = num_panels (), cnt = 0;

  json = new_gjson ();

  popen_obj (json, 0);
  print_json_summary (json, holder);

  set_module_totals (&totals);

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    if (!(panel = panel_lookup (module)))
      continue;

    panel->render (json, holder + module, totals, panel);
    pjson (json, (cnt++ != npanels - 1) ? ",%.*s" : "%.*s", nlines, NL);
  }

  pclose_obj (json, 0, 1);

  return json;
}

/* Open and write to a dynamically sized output buffer.
 *
 * On success, the newly allocated buffer is returned . */
char *
get_json (GHolder * holder, int escape_html) {
  GJSON *json = NULL;
  char *buf = NULL;

  if (holder == NULL)
    return NULL;

  escape_html_output = escape_html;
  if ((json = init_json_output (holder)) && json->size > 0) {
    buf = xstrdup (json->buf);
    free_json (json);
  }

  return buf;
}

/* Entry point to generate a json report writing it to the fp */
void
output_json (GHolder * holder, const char *filename) {
  GJSON *json = NULL;
  FILE *fp;

  if (filename != NULL)
    fp = fopen (filename, "w");
  else
    fp = stdout;

  if (!fp)
    FATAL ("Unable to open JSON file: %s.", strerror (errno));

  /* use new lines to prettify output */
  if (conf.json_pretty_print)
    nlines = 1;

  /* spit it out */
  if ((json = init_json_output (holder)) && json->size > 0) {
    fprintf (fp, "%s", json->buf);
    free_json (json);
  }

  fclose (fp);
}
