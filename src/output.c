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

#include <errno.h>
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
#include "gwsocket.h"
#include "json.h"
#include "settings.h"
#include "ui.h"
#include "util.h"
#include "xmalloc.h"

#include "tpls.h"
#include "bootstrapcss.h"
#include "facss.h"
#include "appcss.h"
#include "d3js.h"
#include "hoganjs.h"
#include "chartsjs.h"
#include "appjs.h"

static void hits_visitors_plot (FILE * fp, const GHTMLPlot plot, int sp);
static void hits_bw_plot (FILE * fp, const GHTMLPlot plot, int sp);

static void print_metrics (FILE * fp, const GHTML * def, int sp);
static void print_host_metrics (FILE * fp, const GHTML * def, int sp);

/* *INDENT-OFF* */
static GHTML htmldef[] = {
  {VISITORS       , 1, print_metrics, {
    {CHART_AREASPLINE, hits_visitors_plot, 1, 1} ,
    {CHART_AREASPLINE, hits_bw_plot, 1, 1} ,
  }},
  {REQUESTS        , 1, print_metrics } ,
  {REQUESTS_STATIC , 1, print_metrics } ,
  {NOT_FOUND       , 1, print_metrics } ,
  {HOSTS           , 1, print_host_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 0},
    {CHART_VBAR, hits_bw_plot, 0, 0},
  }},
  {OS              , 1, print_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 1},
    {CHART_VBAR, hits_bw_plot, 0, 1},
  }},
  {BROWSERS        , 1, print_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 1},
    {CHART_VBAR, hits_bw_plot, 0, 1},
  }},
  {VISIT_TIMES     , 1, print_metrics, {
    {CHART_AREASPLINE, hits_visitors_plot, 0, 1},
    {CHART_AREASPLINE, hits_bw_plot, 0, 1},
  }},
  {VIRTUAL_HOSTS   , 1, print_metrics } ,
  {REFERRERS       , 1, print_metrics } ,
  {REFERRING_SITES , 1, print_metrics } ,
  {KEYPHRASES      , 1, print_metrics } ,
  {STATUS_CODES    , 1, print_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 1},
    {CHART_VBAR, hits_bw_plot, 0, 1},
  }},
  {REMOTE_USER     , 1, print_metrics } ,
#ifdef HAVE_LIBGEOIP
  {GEO_LOCATION    , 1, print_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 1},
    {CHART_VBAR, hits_bw_plot, 0, 1},
  }},
#endif
};
/* *INDENT-ON* */

/* number of new lines (applicable fields) */
static int nlines = 0;

/* Get the chart type for the JSON definition.
 *
 * On success, the chart type string is returned. */
static const char *
chart2str (GChartType type)
{
  static const char *strings[] = { "null", "bar", "area-spline" };
  return strings[type];
}

/* Get panel output data for the given module.
 *
 * If not found, NULL is returned.
 * On success, panel data is returned . */
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

/* Sanitize output with html entities for special chars */
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

/* Set the HTML document title and the generated date/time */
static void
print_html_title (FILE * fp)
{
  const char *title =
    conf.html_report_title ? conf.html_report_title : REP_TITLE;

  fprintf (fp, "<title>");
  clean_output (fp, (char *) title);
  fprintf (fp, "</title>");
}

/* *INDENT-OFF* */
/* Output all the document head elements. */
static void
print_html_header (FILE * fp)
{
  fprintf (fp,
  "<!DOCTYPE html>"
  "<html>"
  "<head>"
  "<meta charset='UTF-8' />"
  "<meta http-equiv='X-UA-Compatible' content='IE=edge'>"
  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
  "<meta name='robots' content='noindex, nofollow' />");

  print_html_title (fp);

  fprintf (fp, "<style>%s</style>", fa_css);
  fprintf (fp, "<style>%s</style>", bootstrap_css);
  fprintf (fp, "<style>%s</style>", app_css);
  /* load custom CSS file, if any */
  if (conf.html_custom_css)
    fprintf (fp, "<link rel='stylesheet' href='%s'>", conf.html_custom_css);

  fprintf (fp,
  "</head>"
  "<body>");
}

/* Output all structural elements of the HTML document. */
static void
print_html_body (FILE * fp, const char *now)
{
  fprintf (fp,
  "<nav class='hidden-xs hidden-sm hide'>"
  "</nav>"

  "<i class='spinner fa fa-circle-o-notch fa-spin fa-3x fa-fw'></i>"
  "<div class='container hide'>"
  "<div class='row row-offcanvas row-offcanvas-right'>"
  "<div class='col-md-12'>"
  "<div class='page-header clearfix'>"
  "<div class='pull-right'>"
  "<h4>"
  "<span class='label label-info' style='display:%s'>"
  "Last Updated: <span class='last-updated'>%s</span>"
  "</span>"
  "</h4>"
  "</div>"
  "<h1>"
  "<span class='hidden-xs hidden-sm'>"
  "<i class='fa fa-tachometer'></i> Dashboard"
  "</span>"
  "<span class='visible-xs visible-sm'>"
  "<i class='fa fa-bars nav-minibars'></i>"
  "<i class='fa fa-circle nav-ws-status mini'></i>"
  "</span>"
  "</h1>", conf.no_html_last_updated ? "none" : "block", now);

  fprintf (fp,
  "<div class='report-title'>%s</div>"
  "</div>"
  "<div class='wrap-general'></div>"
  "<div class='wrap-panels'></div>"
  "</div>"
  "</div>"
  "</div>", conf.html_report_title ? conf.html_report_title : "");
  fprintf (fp, "%s", tpls);
}

/* Output all the document footer elements such as script and closing
 * tags. */
static void
print_html_footer (FILE * fp)
{
  fprintf (fp, "<script>%s</script>", d3_js);
  fprintf (fp, "<script>%s</script>", hogan_js);
  fprintf (fp, "<script>%s</script>", app_js);
  fprintf (fp, "<script>%s</script>", charts_js);

  /* load custom JS file, if any */
  if (conf.html_custom_js)
    fprintf (fp, "<script src='%s'></script>", conf.html_custom_js);

  fprintf (fp, "</body>");
  fprintf (fp, "</html>");
}
/* *INDENT-ON* */

static const GChartDef ChartDefStopper = { NULL, NULL };

/* Get the number of chart definitions per panel.
 *
 * The number of chart definitions is returned . */
static int
get_chartdef_cnt (GChart * chart)
{
  GChartDef *def = chart->def;

  while (memcmp (def, &ChartDefStopper, sizeof ChartDefStopper)) {
    ++def;
  }

  return def - chart->def;
}

/* Output the given JSON chart definition for the given panel. */
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

    fpopen_obj_attr (fp, chart[i].key, sp);
    for (j = 0; j < cnt; ++j) {
      def = chart[i].def[j];
      fpskeysval (fp, def.key, def.value, isp, j == cnt - 1);
    }
    fpclose_obj (fp, sp, (i == n - 1));
  }
}

/* Output D3.js hits/visitors plot definitions. */
static void
hits_visitors_plot (FILE * fp, const GHTMLPlot plot, int sp)
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

  fpskeysval (fp, "label", "Hits/Visitors", isp, 0);
  fpskeysval (fp, "className", "hits-visitors", isp, 0);
  fpskeysval (fp, "chartType", chart2str (plot.chart_type), isp, 0);
  fpskeyival (fp, "chartReverse", plot.chart_reverse, isp, 0);
  fpskeyival (fp, "redrawOnExpand", plot.redraw_expand, isp, 0);

  /* D3.js data */
  fpopen_obj_attr (fp, "d3", isp);
  /* print chart definitions */
  print_d3_chart_def (fp, chart, ARRAY_SIZE (chart), iisp);
  /* close D3 */
  fpclose_obj (fp, isp, 1);
}

/* Output C3.js bandwidth plot definitions. */
static void
hits_bw_plot (FILE * fp, const GHTMLPlot plot, int sp)
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

  if (!conf.bandwidth)
    return;

  fpskeysval (fp, "label", "Bandwidth", isp, 0);
  fpskeysval (fp, "className", "bandwidth", isp, 0);
  fpskeysval (fp, "chartType", chart2str (plot.chart_type), isp, 0);
  fpskeyival (fp, "chartReverse", plot.chart_reverse, isp, 0);
  fpskeyival (fp, "redrawOnExpand", plot.redraw_expand, isp, 0);

  /* D3.js data */
  fpopen_obj_attr (fp, "d3", isp);
  /* print chart definitions */
  print_d3_chart_def (fp, chart, ARRAY_SIZE (chart), iisp);
  /* close D3 */
  fpclose_obj (fp, isp, 1);
}

/* Output JSON data definitions. */
static void
print_json_data (FILE * fp, GLog * glog, GHolder * holder)
{
  char *json = NULL;

  if ((json = get_json (glog, holder, 1)) == NULL)
    return;

  fprintf (fp, "<script type='text/javascript'>");
  fprintf (fp, "var json_data=%s", json);
  fprintf (fp, "</script>");

  free (json);
}

/* Output WebSocket connection definition. */
static void
print_conn_def (FILE * fp)
{
  int sp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    sp += 1;

  if (!conf.real_time_html)
    return;

  fprintf (fp, "<script type='text/javascript'>");
  fprintf (fp, "var connection = ");

  fpopen_obj (fp, sp);
  fpskeysval (fp, "url", (conf.ws_url ? conf.ws_url : "localhost"), sp, 0);
  fpskeyival (fp, "port", (conf.port ? atoi (conf.port) : 7890), sp, 1);
  fpclose_obj (fp, sp, 1);

  fprintf (fp, "</script>");
}

/* Output JSON per panel metric definitions. */
static void
print_def_metric (FILE * fp, const GDefMetric def, int sp)
{
  int isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  if (def.cname)
    fpskeysval (fp, "className", def.cname, isp, 0);
  if (def.cwidth)
    fpskeysval (fp, "colWidth", def.cwidth, isp, 0);
  if (def.meta)
    fpskeysval (fp, "meta", def.meta, isp, 0);
  if (def.vtype)
    fpskeysval (fp, "valueType", def.vtype, isp, 0);
  if (def.key)
    fpskeysval (fp, "key", def.key, isp, 0);
  if (def.lbl)
    fpskeysval (fp, "label", def.lbl, isp, 1);
}

/* Output JSON metric definition block. */
static void
print_def_block (FILE * fp, const GDefMetric def, int sp, int last)
{
  fpopen_obj (fp, sp);
  print_def_metric (fp, def, sp);
  fpclose_obj (fp, sp, last);
}

/* Output JSON overall requests definition block. */
static void
print_def_overall_requests (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_REQUESTS,
    .vtype = "numeric",
    .cname = "black"
  };
  fpopen_obj_attr (fp, OVERALL_REQ, sp);
  print_def_metric (fp, def, sp);
  fpclose_obj (fp, sp, 0);
}

/* Output JSON overall valid requests definition block. */
static void
print_def_overall_valid_reqs (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_VALID,
    .vtype = "numeric",
    .cname = "green"
  };
  fpopen_obj_attr (fp, OVERALL_VALID, sp);
  print_def_metric (fp, def, sp);
  fpclose_obj (fp, sp, 0);
}

/* Output JSON overall invalid requests definition block. */
static void
print_def_overall_invalid_reqs (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_FAILED,
    .vtype = "numeric",
    .cname = "red"
  };
  fpopen_obj_attr (fp, OVERALL_FAILED, sp);
  print_def_metric (fp, def, sp);
  fpclose_obj (fp, sp, 0);
}

/* Output JSON process time definition block. */
static void
print_def_overall_processed_time (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_GEN_TIME,
    .vtype = "numeric",
    .cname = "gray"
  };
  fpopen_obj_attr (fp, OVERALL_GENTIME, sp);
  print_def_metric (fp, def, sp);
  fpclose_obj (fp, sp, 0);
}

/* Output JSON overall visitors definition block. */
static void
print_def_overall_visitors (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_UNIQUE_VIS,
    .vtype = "numeric",
    .cname = "blue"
  };
  fpopen_obj_attr (fp, OVERALL_VISITORS, sp);
  print_def_metric (fp, def, sp);
  fpclose_obj (fp, sp, 0);
}

/* Output JSON overall files definition block. */
static void
print_def_overall_files (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_UNIQUE_FIL,
    .vtype = "numeric",
  };
  fpopen_obj_attr (fp, OVERALL_FILES, sp);
  print_def_metric (fp, def, sp);
  fpclose_obj (fp, sp, 0);
}

/* Output JSON overall excluded requests definition block. */
static void
print_def_overall_excluded (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_EXCLUDE_IP,
    .vtype = "numeric",
  };
  fpopen_obj_attr (fp, OVERALL_EXCL_HITS, sp);
  print_def_metric (fp, def, sp);
  fpclose_obj (fp, sp, 0);
}

/* Output JSON overall referrers definition block. */
static void
print_def_overall_refs (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_REFERRER,
    .vtype = "numeric",
  };
  fpopen_obj_attr (fp, OVERALL_REF, sp);
  print_def_metric (fp, def, sp);
  fpclose_obj (fp, sp, 0);
}

/* Output JSON overall not found definition block. */
static void
print_def_overall_notfound (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_UNIQUE404,
    .vtype = "numeric",
  };
  fpopen_obj_attr (fp, OVERALL_NOTFOUND, sp);
  print_def_metric (fp, def, sp);
  fpclose_obj (fp, sp, 0);
}

/* Output JSON overall static files definition block. */
static void
print_def_overall_static_files (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_STATIC_FIL,
    .vtype = "numeric",
  };
  fpopen_obj_attr (fp, OVERALL_STATIC, sp);
  print_def_metric (fp, def, sp);
  fpclose_obj (fp, sp, 0);
}

/* Output JSON log size definition block. */
static void
print_def_overall_log_size (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_LOG,
    .vtype = "bytes",
  };
  fpopen_obj_attr (fp, OVERALL_LOGSIZE, sp);
  print_def_metric (fp, def, sp);
  fpclose_obj (fp, sp, 0);
}

/* Output JSON overall bandwidth definition block. */
static void
print_def_overall_bandwidth (FILE * fp, int sp)
{
  GDefMetric def = {
    .lbl = T_BW,
    .vtype = "bytes",
  };
  fpopen_obj_attr (fp, OVERALL_BANDWIDTH, sp);
  print_def_metric (fp, def, sp);
  fpclose_obj (fp, sp, 1);
}

/* Output JSON hits definition block. */
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

/* Output JSON visitors definition block. */
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

/* Output JSON bandwidth definition block. */
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

/* Output JSON Avg. T.S. definition block. */
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

/* Output JSON Cum. T.S. definition block. */
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

/* Output JSON Max. T.S. definition block. */
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

/* Output JSON method definition block. */
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

/* Output JSON protocol definition block. */
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

/* Output JSON city definition block. */
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

/* Output JSON country definition block. */
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

/* Output JSON hostname definition block. */
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

/* Output JSON data definition block. */
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

/* Get the number of plots for the given panel definition.
 *
 * The number of plots for the given panel is returned. */
static int
count_plot_fp (const GHTML * def)
{
  int i = 0;
  for (i = 0; def->chart[i].plot != 0; ++i);
  return i;
}

/* Entry function to output JSON plot definition block. */
static void
print_def_plot (FILE * fp, const GHTML * def, int sp)
{
  int i, isp = 0, n = count_plot_fp (def);
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  fpopen_arr_attr (fp, "plot", sp);

  for (i = 0; i < n; ++i) {
    fpopen_obj (fp, isp);
    def->chart[i].plot (fp, def->chart[i], isp);
    fpclose_obj (fp, isp, (i == n - 1));
  }

  fpclose_arr (fp, sp, 0);
}

/* Output JSON host panel definitions. */
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

/* Output JSON panel definitions. */
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

/* Entry point to output JSON metric definitions. */
static void
print_def_metrics (FILE * fp, const GHTML * def, int sp)
{
  int isp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  /* open data metric data */
  fpopen_arr_attr (fp, "items", sp);
  /* definition metrics */
  def->metrics (fp, def, isp);
  /* close metrics block */
  fpclose_arr (fp, sp, 1);
}

/* Output panel header and description metadata definitions. */
static void
print_def_meta (FILE * fp, const char *head, const char *desc, int sp)
{
  int isp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  fpskeysval (fp, "head", head, isp, 0);
  fpskeysval (fp, "desc", desc, isp, 0);
}

/* Output panel sort metadata definitions. */
static void
print_def_sort (FILE * fp, const GHTML * def, int sp)
{
  GSort sort = module_sort[def->module];
  int isp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  /* output open sort attribute */
  fpopen_obj_attr (fp, "sort", sp);
  fpskeysval (fp, "field", get_sort_field_key (sort.field), isp, 0);
  fpskeysval (fp, "order", get_sort_order_str (sort.sort), isp, 1);
  /* output close sort attribute */
  fpclose_obj (fp, sp, 0);
}

/* Output panel metadata definitions. */
static void
print_panel_def_meta (FILE * fp, const GHTML * def, int sp)
{
  const char *desc = module_to_desc (def->module);
  const char *head = module_to_head (def->module);
  const char *id = module_to_id (def->module);

  int isp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  print_def_meta (fp, head, desc, sp);

  fpskeysval (fp, "id", id, isp, 0);
  fpskeyival (fp, "table", def->table, isp, 0);

  print_def_sort (fp, def, isp);
  print_def_plot (fp, def, isp);
  print_def_metrics (fp, def, isp);
}

/* Output definitions for the given panel. */
static void
print_json_def (FILE * fp, const GHTML * def)
{
  int sp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    sp = 1;

  /* output open panel attribute */
  fpopen_obj_attr (fp, module_to_id (def->module), sp);
  /* output panel data definitions */
  print_panel_def_meta (fp, def, sp);
  /* output close panel attribute */
  fpclose_obj (fp, sp, 1);

  fpjson (fp, (def->module != TOTAL_MODULES - 1) ? ",%.*s" : "%.*s", nlines,
          NL);
}

/* Output overall definitions. */
static void
print_def_summary (FILE * fp, int sp)
{
  int isp = 0, iisp = 0;
  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1, iisp = sp + 2;

  /* open metrics block */
  fpopen_obj_attr (fp, "items", isp);

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
  fpclose_obj (fp, isp, 1);
}

/* Output definitions for the given panel. */
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
  fpopen_obj_attr (fp, GENER_ID, sp);
  print_def_meta (fp, head, "", sp);
  print_def_summary (fp, sp);
  /* output close panel attribute */
  fpclose_obj (fp, sp, 0);
  free (head);
}

/* Entry point to output definitions for all panels. */
static void
print_json_defs (FILE * fp, GHolder * holder)
{
  const GHTML *def;
  size_t idx = 0;

  fprintf (fp, "<script type='text/javascript'>");
  fprintf (fp, "var user_interface=");
  fpopen_obj (fp, 0);

  print_json_def_summary (fp, holder);
  FOREACH_MODULE (idx, module_list) {
    if ((def = panel_lookup (module_list[idx]))) {
      print_json_def (fp, def);
    }
  }

  fpclose_obj (fp, 0, 1);

  fprintf (fp, "</script>");
}

/* entry point to generate a report writing it to the fp */
void
output_html (GLog * glog, GHolder * holder, const char *filename)
{
  FILE *fp;
  char now[DATE_TIME];

  if (filename != NULL)
    fp = fopen (filename, "w");
  else
    fp = stdout;

  if (!fp)
    FATAL ("Unable to open HTML file: %s.", strerror (errno));

  /* use new lines to prettify output */
  if (conf.json_pretty_print)
    nlines = 1;
  set_json_nlines (nlines);

  generate_time ();
  strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S", now_tm);

  print_html_header (fp);

  print_html_body (fp, now);
  print_json_defs (fp, holder);
  print_json_data (fp, glog, holder);
  print_conn_def (fp);

  print_html_footer (fp);

  fclose (fp);
}
