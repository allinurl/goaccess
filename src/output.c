/**
 * output.c -- output to the standard output stream
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

#include "output.h"

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabdb.h"
#else
#include "gkhash.h"
#endif

#include "error.h"
#include "json.h"
#include "settings.h"
#include "ui.h"
#include "util.h"
#include "xmalloc.h"

#include "tpls.h"
#include "bootstrapcss.h"
#include "appcss.h"
#include "d3js.h"
#include "hoganjs.h"
#include "appjs.h"

static void hits_visitors_plot (FILE * fp, int sp);
static void hits_bw_plot (FILE * fp, int sp);

static void print_metrics (FILE * fp, const GHTML * def, int sp);
static void print_host_metrics (FILE * fp, const GHTML * def, int sp);

/* *INDENT-OFF* */
static GHTML htmldef[] = {
  {VISITORS        , CHART_AREASPLINE , 1 , 1, print_metrics, {
      {hits_visitors_plot}, {hits_bw_plot}
  }},
  {REQUESTS        , CHART_NONE       , 0 , 1, print_metrics } ,
  {REQUESTS_STATIC , CHART_NONE       , 0 , 1, print_metrics } ,
  {NOT_FOUND       , CHART_NONE       , 0 , 1, print_metrics } ,
  {HOSTS           , CHART_AREASPLINE , 0 , 1, print_host_metrics, {
      {hits_visitors_plot}, {hits_bw_plot}
  }},
  {OS              , CHART_VBAR 	    , 0 , 1, print_metrics, {
      {hits_visitors_plot}, {hits_bw_plot}
  }},
  {BROWSERS        , CHART_VBAR 	    , 0 , 1, print_metrics, {
      {hits_visitors_plot}, {hits_bw_plot}
  }},
  {VISIT_TIMES     , CHART_AREASPLINE , 0 , 1, print_metrics, {
      {hits_visitors_plot}, {hits_bw_plot}
  }},
  {VIRTUAL_HOSTS   , CHART_NONE       , 0 , 1, print_metrics } ,
  {REFERRERS       , CHART_NONE       , 0 , 1, print_metrics } ,
  {REFERRING_SITES , CHART_NONE       , 0 , 1, print_metrics } ,
  {KEYPHRASES      , CHART_NONE       , 0 , 1, print_metrics } ,
#ifdef HAVE_LIBGEOIP
  {GEO_LOCATION    , CHART_NONE       , 0 , 1, print_metrics } ,
#endif
  {STATUS_CODES    , CHART_VBAR 	    , 0 , 1, print_metrics, {
      {hits_visitors_plot}, {hits_bw_plot}
  }},
};
/* *INDENT-ON* */

/* number of new lines (applicable fields) */
static int nlines = 0;

static const char *
chart2str (GChartType type)
{
  static const char *strings[] = { "null", "bar", "area-spline" };
  return strings[type];
}

static GHTML *
panel_lookup (GModule module)
{
  int i, num_panels = ARRAY_SIZE (htmldef);

  for (i = 0; i < num_panels; i++) {
    if (htmldef[i].module == module)
      return &htmldef[i];
  }
  return NULL;
}

/* sanitize output with html entities for special chars */
static void
clean_output (FILE * fp, char *s)
{
  if (!s) {
    LOG_DEBUG (("NULL data on clean_output.\n"));
    return;
  }

  while (*s) {
    switch (*s) {
    case '\'':
      fprintf (fp, "&#39;");
      break;
    case '"':
      fprintf (fp, "&#34;");
      break;
    case '&':
      fprintf (fp, "&amp;");
      break;
    case '<':
      fprintf (fp, "&lt;");
      break;
    case '>':
      fprintf (fp, "&gt;");
      break;
    case ' ':
      fprintf (fp, "&nbsp;");
      break;
    default:
      fputc (*s, fp);
      break;
    }
    s++;
  }
}

static void
print_html_title (FILE * fp, char *now)
{
  const char *title =
    conf.html_report_title ? conf.html_report_title : REP_TITLE;

  fprintf (fp, "<title>");
  clean_output (fp, (char *) title);
  fprintf (fp, " - %s</title>", now);
}

/* *INDENT-OFF* */
static void
print_html_header (FILE * fp, char *now)
{
  fprintf (fp,
  "<!DOCTYPE html>"
  "<html>"
  "<head>"
  "<meta charset='UTF-8' />"
  "<meta http-equiv='X-UA-Compatible' content='IE=edge'>"
  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
  "<meta name='robots' content='noindex, nofollow' />");

  print_html_title (fp, now);

  fprintf (fp, "<link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/font-awesome/4.4.0/css/font-awesome.min.css'>");
  fprintf (fp, "<style>%s</style>", bootstrap_css);
  fprintf (fp, "<style>%s</style>", app_css);

  fprintf (fp,
  "</head>"
  "<body>");

  fprintf (fp, "%s", tpls);
}

static void
print_html_body (FILE * fp, const char *now)
{
  fprintf (fp,
  "<nav class='hidden-xs hidden-sm'>"
  "<div class='icon fa fa-bars'></div>"
  "<div class='panel-nav'></div>"
  "</nav>"

  "<div class='container'>"
  "<div class='row row-offcanvas row-offcanvas-right'>"
  "<div class='col-md-12'>"
  "<div class='page-header clearfix'>"
  "<div class='pull-right'>"
  "<h4>"
  "<span class='label label-info'>"
  "Last Updated: %s"
  "</span>"
  "</h4>"
  "</div>"
  "<h1><i class='fa fa-tachometer'></i> Dashboard</h1>"
  "</div>"
  "<div class='wrap-general'></div>"
  "<div class='wrap-panels'></div>"
  "</div>"
  "</div>"
  "</div>", now);
}

static void
print_html_footer (FILE * fp)
{
  fprintf (fp, "<script>%s</script>", d3_js);
  fprintf (fp, "<script>%s</script>", hogan_js);
  fprintf (fp, "<script>%s</script>", app_js);

  fprintf (fp, "</body>");
  fprintf (fp, "</html>");
}
/* *INDENT-ON* */

static const GChartDef ChartDefStopper = { NULL, NULL };

static int
get_chartdef_cnt (GChart * chart)
{
  GChartDef *def = chart->def;

  while (memcmp (def, &ChartDefStopper, sizeof ChartDefStopper)) {
    ++def;
  }

  return def - chart->def;
}

static void
print_d3_chart_def (FILE * fp, GChart * chart, size_t n, int sp)
{
  GChartDef def = { 0 };
  size_t i = 0, j = 0, cnt = 0;
  int isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  for (i = 0; i < n; ++i) {
    cnt = get_chartdef_cnt (chart + i);

    popen_obj_attr (fp, chart[i].key, sp);
    for (j = 0; j < cnt; ++j) {
      def = chart[i].def[j];
      pskeysval (fp, def.key, def.value, isp, j == cnt - 1);
    }
    pclose_obj (fp, sp, (i == n - 1));
  }
}

/* Output D3.js hits/visitors plot definitions. */
static void
hits_visitors_plot (FILE * fp, int sp)
{
  /* *INDENT-OFF* */
  GChart chart[] = {
    {"y0", (GChartDef[]) {
      {"key", "hits"}, {"label", "Hits"}, ChartDefStopper
    }},
    {"y1", (GChartDef[]) {
      {"key", "visitors"}, {"label", "Visitors"}, ChartDefStopper
    }},
  };
  /* *INDENT-ON* */

  int isp = 0, iisp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1, iisp = sp + 2;

  pskeysval (fp, "label", "Hits/Visitors", isp, 0);
  pskeysval (fp, "className", "hits-visitors", isp, 0);

  /* D3.js data */
  popen_obj_attr (fp, "d3", isp);
  /* print chart definitions */
  print_d3_chart_def (fp, chart, ARRAY_SIZE (chart), iisp);
  /* close D3 */
  pclose_obj (fp, isp, 1);
}

/* Output C3.js bandwidth plot definitions. */
static void
hits_bw_plot (FILE * fp, int sp)
{
  /* *INDENT-OFF* */
  GChart chart[] = {
    {"y0", (GChartDef[]) {
      {"key", "bytes"}, {"label", "Bandwidth"}, {"format", "bytes"}, ChartDefStopper
    }},
  };
  /* *INDENT-ON* */

  int isp = 0, iisp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1, iisp = sp + 2;

  pskeysval (fp, "label", "Bandwidth", isp, 0);
  pskeysval (fp, "className", "bandwidth", isp, 0);

  /* D3.js data */
  popen_obj_attr (fp, "d3", isp);
  /* print chart definitions */
  print_d3_chart_def (fp, chart, ARRAY_SIZE (chart), iisp);
  /* close D3 */
  pclose_obj (fp, isp, 1);
}

static void
print_json_data (FILE * fp, GLog * logger, GHolder * holder)
{
  char *json = NULL;

  if ((json = get_json (logger, holder)) == NULL)
    return;

  fprintf (fp, "<script type='text/javascript'>");
  fprintf (fp, "var json_data=%s", json);
  fprintf (fp, "</script>");

  free (json);
}

static void
print_def_metric (FILE * fp, const GDefMetric def, int sp)
{
  int isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  if (def.cname)
    pskeysval (fp, "className", def.cname, isp, 0);
  if (def.cwidth)
    pskeysval (fp, "colWidth", def.cwidth, isp, 0);
  if (def.meta)
    pskeysval (fp, "meta", def.meta, isp, 0);
  if (def.vtype)
    pskeysval (fp, "valueType", def.vtype, isp, 0);
  if (def.key)
    pskeysval (fp, "key", def.key, isp, 0);
  if (def.lbl)
    pskeysval (fp, "label", def.lbl, isp, 1);
}

static void
print_def_block (FILE * fp, const GDefMetric def, int sp, int last)
{
  popen_obj (fp, sp);
  print_def_metric (fp, def, sp);
  pclose_obj (fp, sp, last);
}

static void
print_def_overall_requests (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_REQUESTS,
    .vtype = "numeric",
    .cname = "black"
  };
  popen_obj_attr (fp, OVERALL_REQ, sp);
  print_def_metric (fp, def, sp);
  pclose_obj (fp, sp, 0);
}

static void
print_def_overall_valid_reqs (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_VALID,
    .vtype = "numeric",
    .cname = "green"
  };
  popen_obj_attr (fp, OVERALL_VALID, sp);
  print_def_metric (fp, def, sp);
  pclose_obj (fp, sp, 0);
}

static void
print_def_overall_invalid_reqs (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_FAILED,
    .vtype = "numeric",
    .cname = "red"
  };
  popen_obj_attr (fp, OVERALL_FAILED, sp);
  print_def_metric (fp, def, sp);
  pclose_obj (fp, sp, 0);
}

static void
print_def_overall_processed_time (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_GEN_TIME,
    .vtype = "numeric",
    .cname = "gray"
  };
  popen_obj_attr (fp, OVERALL_GENTIME, sp);
  print_def_metric (fp, def, sp);
  pclose_obj (fp, sp, 0);
}

static void
print_def_overall_visitors (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_UNIQUE_VIS,
    .vtype = "numeric",
    .cname = "blue"
  };
  popen_obj_attr (fp, OVERALL_VISITORS, sp);
  print_def_metric (fp, def, sp);
  pclose_obj (fp, sp, 0);
}

static void
print_def_overall_files (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_UNIQUE_FIL,
    .vtype = "numeric",
  };
  popen_obj_attr (fp, OVERALL_FILES, sp);
  print_def_metric (fp, def, sp);
  pclose_obj (fp, sp, 0);
}

static void
print_def_overall_excluded (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_EXCLUDE_IP,
    .vtype = "numeric",
  };
  popen_obj_attr (fp, OVERALL_EXCL_HITS, sp);
  print_def_metric (fp, def, sp);
  pclose_obj (fp, sp, 0);
}

static void
print_def_overall_refs (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_REFERRER,
    .vtype = "numeric",
  };
  popen_obj_attr (fp, OVERALL_REF, sp);
  print_def_metric (fp, def, sp);
  pclose_obj (fp, sp, 0);
}

static void
print_def_overall_notfound (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_UNIQUE404,
    .vtype = "numeric",
  };
  popen_obj_attr (fp, OVERALL_NOTFOUND, sp);
  print_def_metric (fp, def, sp);
  pclose_obj (fp, sp, 0);
}

static void
print_def_overall_static_files (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_STATIC_FIL,
    .vtype = "numeric",
  };
  popen_obj_attr (fp, OVERALL_STATIC, sp);
  print_def_metric (fp, def, sp);
  pclose_obj (fp, sp, 0);
}

static void
print_def_overall_log_size (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_LOG,
    .vtype = "bytes",
  };
  popen_obj_attr (fp, OVERALL_LOGSIZE, sp);
  print_def_metric (fp, def, sp);
  pclose_obj (fp, sp, 0);
}

static void
print_def_overall_bandwidth (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_BW,
    .vtype = "bytes",
  };
  popen_obj_attr (fp, OVERALL_BANDWIDTH, sp);
  print_def_metric (fp, def, sp);
  pclose_obj (fp, sp, 1);
}

static void
print_def_hits (FILE * fp, int sp)
{
  GDefMetric def = {
    .key = "hits",
    .lbl = MTRC_HITS_LBL,
    .vtype = "numeric",
    .meta = "count",
    .cwidth = "12%",
  };
  print_def_block (fp, def, sp, 0);
}

static void
print_def_visitors (FILE * fp, int sp)
{
  GDefMetric def = {
    .key = "visitors",
    .lbl = MTRC_VISITORS_LBL,
    .vtype = "numeric",
    .meta = "count",
    .cwidth = "12%",
  };
  print_def_block (fp, def, sp, 0);
}

static void
print_def_bw (FILE * fp, int sp)
{
  GDefMetric def = {
    .key = "bytes",
    .lbl = MTRC_BW_LBL,
    .vtype = "bytes",
    .meta = "count",
    .cwidth = "12%",
  };

  if (!conf.bandwidth)
    return;

  print_def_block (fp, def, sp, 0);
}

static void
print_def_avgts (FILE * fp, int sp)
{
  GDefMetric def = {
    .key = "avgts",
    .lbl = MTRC_AVGTS_LBL,
    .vtype = "utime",
    .meta = "avg",
    .cwidth = "8%",
  };

  if (!conf.serve_usecs)
    return;

  print_def_block (fp, def, sp, 0);
}

static void
print_def_cumts (FILE * fp, int sp)
{
  GDefMetric def = {
    .key = "cumts",
    .lbl = MTRC_CUMTS_LBL,
    .vtype = "utime",
    .meta = "count",
    .cwidth = "8%",
  };

  if (!conf.serve_usecs)
    return;

  print_def_block (fp, def, sp, 0);
}

static void
print_def_maxts (FILE * fp, int sp)
{
  GDefMetric def = {
    .key = "maxts",
    .lbl = MTRC_MAXTS_LBL,
    .vtype = "utime",
    .meta = "count",
    .cwidth = "8%",
  };

  if (!conf.serve_usecs)
    return;
  print_def_block (fp, def, sp, 0);
}

static void
print_def_method (FILE * fp, int sp)
{
  GDefMetric def = {
    .key = "method",
    .lbl = MTRC_METHODS_LBL,
    .vtype = "string",
    .cwidth = "6%",
  };

  if (!conf.append_method)
    return;

  print_def_block (fp, def, sp, 0);
}

static void
print_def_protocol (FILE * fp, int sp)
{
  GDefMetric def = {
    .key = "protocol",
    .lbl = MTRC_PROTOCOLS_LBL,
    .vtype = "string",
    .cwidth = "7%",
  };

  if (!conf.append_protocol)
    return;

  print_def_block (fp, def, sp, 0);
}

static void
print_def_city (FILE * fp, int sp)
{
  GDefMetric def = {
    .key = "city",
    .lbl = MTRC_CITY_LBL,
    .vtype = "string",
  };

  if (!conf.has_geocity)
    return;

  print_def_block (fp, def, sp, 0);
}

static void
print_def_country (FILE * fp, int sp)
{
  GDefMetric def = {
    .key = "country",
    .lbl = MTRC_COUNTRY_LBL,
    .vtype = "string",
  };

  if (!conf.has_geocountry)
    return;

  print_def_block (fp, def, sp, 0);
}

static void
print_def_hostname (FILE * fp, int sp)
{
  GDefMetric def = {
    .key = "hostname",
    .lbl = MTRC_HOSTNAME_LBL,
    .vtype = "string",
    .cname = "light",
  };

  if (!conf.enable_html_resolver)
    return;

  print_def_block (fp, def, sp, 0);
}

static void
print_def_data (FILE * fp, GModule module, int sp)
{
  GDefMetric def = {
    .key = "data",
    .lbl = MTRC_DATA_LBL,
    .vtype = module == VISITORS ? "date" : "string",
    .cname = "trunc",
    .cwidth = "100%",
  };

  print_def_block (fp, def, sp, 1);
}

static int
count_plot_fp (const GHTML * def)
{
  int i = 0;
  for (i = 0; def->chart[i].plot != 0; ++i);
  return i;
}

static void
print_def_plot (FILE * fp, const GHTML * def, int sp)
{
  int i, isp = 0, n = count_plot_fp (def);
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  popen_arr_attr (fp, "plot", sp);

  for (i = 0; i < n; ++i) {
    popen_obj (fp, isp);
    def->chart[i].plot (fp, isp);
    pclose_obj (fp, isp, (i == n - 1));
  }

  pclose_arr (fp, sp, 0);
}

static void
print_host_metrics (FILE * fp, const GHTML * def, int sp)
{
  const GOutput *output = output_lookup (def->module);

  print_def_hits (fp, sp);
  print_def_visitors (fp, sp);
  print_def_bw (fp, sp);
  print_def_avgts (fp, sp);
  print_def_cumts (fp, sp);
  print_def_maxts (fp, sp);

  if (output->method)
    print_def_method (fp, sp);
  if (output->protocol)
    print_def_protocol (fp, sp);

  print_def_city (fp, sp);
  print_def_country (fp, sp);
  print_def_hostname (fp, sp);

  print_def_data (fp, def->module, sp);
}

static void
print_metrics (FILE * fp, const GHTML * def, int sp)
{
  const GOutput *output = output_lookup (def->module);

  print_def_hits (fp, sp);
  print_def_visitors (fp, sp);
  print_def_bw (fp, sp);
  print_def_avgts (fp, sp);
  print_def_cumts (fp, sp);
  print_def_maxts (fp, sp);

  if (output->method)
    print_def_method (fp, sp);
  if (output->protocol)
    print_def_protocol (fp, sp);

  print_def_data (fp, def->module, sp);
}

static void
print_def_metrics (FILE * fp, const GHTML * def, int sp)
{
  int isp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  /* open data metric data */
  popen_arr_attr (fp, "items", sp);
  /* definition metrics */
  def->metrics (fp, def, isp);
  /* close metrics block */
  pclose_arr (fp, sp, 1);
}

static void
print_def_meta (FILE * fp, const char *head, const char *desc, int sp)
{
  int isp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  pskeysval (fp, "head", head, isp, 0);
  pskeysval (fp, "desc", desc, isp, 0);
}

static void
print_panel_def_meta (FILE * fp, const GHTML * def, int sp)
{
  const char *chart = chart2str (def->chart_type);
  const char *desc = module_to_desc (def->module);
  const char *head = module_to_head (def->module);
  const char *id = module_to_id (def->module);

  int isp = 0, rev = def->chart_reverse;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  print_def_meta (fp, head, desc, sp);

  pskeysval (fp, "id", id, isp, 0);
  pskeyival (fp, "table", def->table, isp, 0);

  if (def->chart_type) {
    pskeysval (fp, "chartType", chart, isp, 0);
    pskeyival (fp, "chartReverse", rev, isp, 0);
  }

  print_def_plot (fp, def, isp);
  print_def_metrics (fp, def, isp);
}

static void
print_json_def (FILE * fp, const GHTML * def)
{
  int sp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    sp = 1;

  /* output open panel attribute */
  popen_obj_attr (fp, module_to_id (def->module), sp);
  /* output panel data definitions */
  print_panel_def_meta (fp, def, sp);
  /* output close panel attribute */
  pclose_obj (fp, sp, 1);

  pjson (fp, (def->module != TOTAL_MODULES - 1) ? ",%.*s" : "%.*s", nlines, NL);
}

static void
print_def_summary (FILE * fp, int sp)
{
  int isp = 0, iisp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1, iisp = sp + 2;

  /* open metrics block */
  popen_obj_attr (fp, "items", isp);

  print_def_overall_requests (fp, iisp);
  print_def_overall_valid_reqs (fp, iisp);
  print_def_overall_invalid_reqs (fp, iisp);
  print_def_overall_processed_time (fp, iisp);
  print_def_overall_visitors (fp, iisp);
  print_def_overall_files (fp, iisp);
  print_def_overall_excluded (fp, iisp);
  print_def_overall_refs (fp, iisp);
  print_def_overall_notfound (fp, iisp);
  print_def_overall_static_files (fp, iisp);
  print_def_overall_log_size (fp, iisp);
  print_def_overall_bandwidth (fp, iisp);

  /* close metrics block */
  pclose_obj (fp, isp, 1);
}

static void
print_json_def_summary (FILE * fp, GHolder * holder)
{
  char *head = NULL;
  int sp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    sp = 1;

  head = get_overall_header (holder);
  /* output open panel attribute */
  popen_obj_attr (fp, GENER_ID, sp);
  print_def_meta (fp, head, "", sp);
  print_def_summary (fp, sp);
  /* output close panel attribute */
  pclose_obj (fp, sp, 0);
  free (head);
}

static void
print_json_defs (FILE * fp, GHolder * holder)
{
  const GHTML *def;
  size_t idx = 0;

  fprintf (fp, "<script type='text/javascript'>");
  fprintf (fp, "var user_interface=");
  popen_obj (fp, 0);

  print_json_def_summary (fp, holder);
  FOREACH_MODULE (idx, module_list) {
    if ((def = panel_lookup (module_list[idx]))) {
      print_json_def (fp, def);
    }
  }

  pclose_obj (fp, 0, 1);

  fprintf (fp, "</script>");
}

/* entry point to generate a report writing it to the fp */
void
output_html (GLog * logger, GHolder * holder)
{
  FILE *fp = stdout;
  char now[DATE_TIME];

  /* use new lines to prettify output */
  if (conf.json_pretty_print)
    nlines = 1;
  set_json_nlines (nlines);

  generate_time ();
  strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S", now_tm);

  print_html_header (fp, now);

  print_json_defs (fp, holder);
  print_json_data (fp, logger, holder);

  print_html_body (fp, now);
  print_html_footer (fp);

  fclose (fp);
}
