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

static void hits_bw_plot (FILE * fp, GHTMLPlot plot, int sp);
static void hits_bw_req_plot (FILE * fp, GHTMLPlot plot, int sp);
static void hits_visitors_plot (FILE * fp, GHTMLPlot plot, int sp);
static void hits_visitors_req_plot (FILE * fp, GHTMLPlot plot, int sp);

static void print_metrics (FILE * fp, const GHTML * def, int sp);
static void print_host_metrics (FILE * fp, const GHTML * def, int sp);

/* *INDENT-OFF* */
static GHTML htmldef[] = {
  {VISITORS, 1, print_metrics, {
    {CHART_AREASPLINE, hits_visitors_plot, 1, 1, NULL, NULL} ,
    {CHART_AREASPLINE, hits_bw_plot, 1, 1, NULL, NULL} ,
  }},
  {REQUESTS, 1, print_metrics, {
    {CHART_VBAR, hits_visitors_req_plot, 0, 0, NULL, NULL},
    {CHART_VBAR, hits_bw_req_plot, 0, 0, NULL, NULL},
  }},
  {REQUESTS_STATIC, 1, print_metrics, {
    {CHART_VBAR, hits_visitors_req_plot, 0, 0, NULL, NULL},
    {CHART_VBAR, hits_bw_req_plot, 0, 0, NULL, NULL},
  }},
  {NOT_FOUND, 1, print_metrics, {
    {CHART_VBAR, hits_visitors_req_plot, 0, 0, NULL, NULL},
    {CHART_VBAR, hits_bw_req_plot, 0, 0, NULL, NULL},
  }},
  {HOSTS, 1, print_host_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 0, NULL, NULL},
    {CHART_VBAR, hits_bw_plot, 0, 0, NULL, NULL},
  }},
  {OS, 1, print_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 1, NULL, NULL},
    {CHART_VBAR, hits_bw_plot, 0, 1, NULL, NULL},
  }},
  {BROWSERS, 1, print_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 1, NULL, NULL},
    {CHART_VBAR, hits_bw_plot, 0, 1, NULL, NULL},
  }},
  {VISIT_TIMES, 1, print_metrics, {
    {CHART_AREASPLINE, hits_visitors_plot, 0, 1, NULL, NULL},
    {CHART_AREASPLINE, hits_bw_plot, 0, 1, NULL, NULL},
  }},
  {VIRTUAL_HOSTS, 1, print_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 0, NULL, NULL},
    {CHART_VBAR, hits_bw_plot, 0, 0, NULL, NULL},
  }},
  {REFERRERS, 1, print_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 0, NULL, NULL},
    {CHART_VBAR, hits_bw_plot, 0, 0, NULL, NULL},
  }},
  {REFERRING_SITES, 1, print_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 0, NULL, NULL},
    {CHART_VBAR, hits_bw_plot, 0, 0, NULL, NULL},
  }},
  {KEYPHRASES, 1, print_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 0, NULL, NULL},
    {CHART_VBAR, hits_bw_plot, 0, 0, NULL, NULL},
  }},
  {STATUS_CODES, 1, print_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 1, NULL, NULL},
    {CHART_VBAR, hits_bw_plot, 0, 1, NULL, NULL},
  }},
  {REMOTE_USER, 1, print_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 0, NULL, NULL},
    {CHART_VBAR, hits_bw_plot, 0, 0, NULL, NULL},
  }},
  {CACHE_STATUS, 1, print_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 0, NULL, NULL},
    {CHART_VBAR, hits_bw_plot, 0, 0, NULL, NULL},
  }},
#ifdef HAVE_GEOLOCATION
  {GEO_LOCATION, 1, print_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 1, NULL, NULL},
    {CHART_VBAR, hits_bw_plot, 0, 1, NULL, NULL},
  }},
#endif
  {MIME_TYPE, 1, print_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 0, NULL, NULL},
    {CHART_VBAR, hits_bw_plot, 0, 0, NULL, NULL},
  }},
  {TLS_TYPE, 1, print_metrics, {
    {CHART_VBAR, hits_visitors_plot, 0, 0, NULL, NULL},
    {CHART_VBAR, hits_bw_plot, 0, 0, NULL, NULL},
  }},

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
clean_output (FILE * fp, const char *s)
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
    conf.html_report_title ? conf.html_report_title : HTML_REPORT_TITLE;

  fprintf (fp, "<title>");
  clean_output (fp, title);
  fprintf (fp, "</title>");
}

/* *INDENT-OFF* */
/* Output all the document head elements. */
static void
print_html_header (FILE * fp)
{
  fprintf (fp,
  "<!DOCTYPE html>"
  "<html lang='%s'>"
  "<head>"
  "<meta charset='UTF-8'>"
  "<meta name='referrer' content='no-referrer'>"
  "<meta http-equiv='X-UA-Compatible' content='IE=edge'>"
  "<meta name='google' content='notranslate'>"
  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
  "<meta name='robots' content='noindex, nofollow'>", _(DOC_LANG));

  /* Output base64 encoded goaccess favicon.ico*/
  fprintf (fp, "<link rel='icon' href='data:image/x-icon;base64,AAABAAEA"
  "EBAQAAEABAAoAQAAFgAAACgAAAAQAAAAIAAAAAEABAAAAAAAgAAAAAAAAAAAAAAAEAAAA"
  "AAAAADGxsYAWFhYABwcHABfAP8A/9dfAADXrwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
  "AAAAAAAAAAAAAAAAAAAAAAIiIiIiIiIiIjMlUkQgAiIiIiIiIiIiIiIzJVJEIAAAIiIiI"
  "iIiIiIiMyVSRCAAIiIiIiIiIiIiIRERERERERERERERERERERIiIiIiIiIiIgACVVUiIi"
  "IiIiIiIiIiIiIAAlVVIiIiIiIiIiIiIiIhEREREREREREREREREREREAAAAAAAAAAAAAA"
  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
  "AA' type='image/x-icon' />");

  print_html_title (fp);

  fprintf (fp, "<style>%.*s</style>", fa_css_length, fa_css);
  fprintf (fp, "<style>%.*s</style>", bootstrap_css_length, bootstrap_css);
  fprintf (fp, "<style>%.*s</style>", app_css_length, app_css);
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
  "<div class='wrap-header'>"
  "<div class='row row-offcanvas row-offcanvas-right'>"
  "<div class='col-md-12'>"
  "<div class='page-header clearfix'>"
  "<div class='pull-right'>"
  "<h4>"
  "<span class='label label-info' style='display:%s'>"
  "<span class='hidden-xs'>%s: </span>"
  "<span class='last-updated'>%s</span>"
  "</span>"
  "</h4>"
  "</div>"
  "<h1>"
  "<span class='hidden-xs hidden-sm'>"
  "<i class='fa fa-tachometer'></i> %s"
  "</span>"
  "<span class='visible-xs visible-sm'>"
  "<i class='fa fa-bars nav-minibars'></i>"
  "<i class='fa fa-circle nav-ws-status mini'></i>"
  "</span>"
  "</h1>", conf.no_html_last_updated ? "none" : "block", INFO_LAST_UPDATED, now, T_DASH);

  fprintf (fp,
  "<div class='report-title'>%s</div>"
  "</div>"
  "<div class='wrap-general'></div>"
  "</div>"
  "</div>"
  "</div>"
  "<div class='wrap-panels'></div>"
  "</div>", conf.html_report_title ? conf.html_report_title : "");

  fprintf (fp, "%.*s", tpls_length, tpls);
}

/* Output all the document footer elements such as script and closing
 * tags. */
static void
print_html_footer (FILE * fp)
{
  fprintf (fp, "<script>%.*s</script>", d3_js_length, d3_js);
  fprintf (fp, "<script>%.*s</script>", hogan_js_length, hogan_js);
  fprintf (fp, "<script>%.*s</script>", app_js_length, app_js);
  fprintf (fp, "<script>%.*s</script>", charts_js_length, charts_js);

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

  while (memcmp (def, &ChartDefStopper, sizeof ChartDefStopper))
    ++def;

  return def - chart->def;
}

/* Output the given JSON chart axis definition for the given panel. */
static void
print_d3_chart_def_axis (FILE * fp, GChart * chart, size_t cnt, int isp)
{
  GChartDef *def = chart->def;
  size_t j = 0;

  for (j = 0; j < cnt; ++j) {
    if (strchr (def[j].value, '[') != NULL)
      fpskeyaval (fp, def[j].key, def[j].value, isp, j == cnt - 1);
    else
      fpskeysval (fp, def[j].key, def[j].value, isp, j == cnt - 1);
  }
}

/* Output the given JSON chart definition for the given panel. */
static void
print_d3_chart_def (FILE * fp, GChart * chart, size_t n, int sp)
{
  size_t i = 0, cnt = 0;
  int isp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1;

  for (i = 0; i < n; ++i) {
    cnt = get_chartdef_cnt (chart + i);

    fpopen_obj_attr (fp, chart[i].key, sp);
    print_d3_chart_def_axis (fp, chart + i, cnt, isp);
    fpclose_obj (fp, sp, (i == n - 1));
  }
}

static void
print_plot_def (FILE * fp, const GHTMLPlot plot, GChart * chart, int n, int sp)
{
  int isp = 0, iisp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    isp = sp + 1, iisp = sp + 2;

  fpskeysval (fp, "className", plot.chart_key, isp, 0);
  fpskeysval (fp, "label", plot.chart_lbl, isp, 0);
  fpskeysval (fp, "chartType", chart2str (plot.chart_type), isp, 0);
  fpskeyival (fp, "chartReverse", plot.chart_reverse, isp, 0);
  fpskeyival (fp, "redrawOnExpand", plot.redraw_expand, isp, 0);

  /* D3.js data */
  fpopen_obj_attr (fp, "d3", isp);
  /* print chart definitions */
  print_d3_chart_def (fp, chart, n, iisp);
  /* close D3 */
  fpclose_obj (fp, isp, 1);
}

/* Output D3.js hits/visitors plot definitions. */
static void
hits_visitors_plot (FILE * fp, GHTMLPlot plot, int sp)
{
  /* *INDENT-OFF* */
  GChart def[] = {
    {"y0", (GChartDef[]) {
      {"key", "hits"}, {"label", MTRC_HITS_LBL}, ChartDefStopper
    }},
    {"y1", (GChartDef[]) {
      {"key", "visitors"}, {"label", MTRC_VISITORS_LBL}, ChartDefStopper
    }},
  };

  plot.chart_key = (char[]) {"hits-visitors"};
  plot.chart_lbl = (char *) {HTML_PLOT_HITS_VIS};
  /* *INDENT-ON* */
  print_plot_def (fp, plot, def, ARRAY_SIZE (def), sp);
}

/* Output D3.js hits/visitors plot definitions. */
static void
hits_visitors_req_plot (FILE * fp, GHTMLPlot plot, int sp)
{
  /* *INDENT-OFF* */
  GChart def[] = {
    {"x", (GChartDef[]) {
      {"key", "[\"method\", \"data\", \"protocol\"]"}, ChartDefStopper
    }},
    {"y0", (GChartDef[]) {
      {"key", "hits"}, {"label", MTRC_HITS_LBL}, ChartDefStopper
    }},
    {"y1", (GChartDef[]) {
      {"key", "visitors"}, {"label", MTRC_VISITORS_LBL}, ChartDefStopper
    }},
  };

  plot.chart_key = (char[]) {"hits-visitors"};
  plot.chart_lbl = (char *) {HTML_PLOT_HITS_VIS};
  /* *INDENT-ON* */
  print_plot_def (fp, plot, def, ARRAY_SIZE (def), sp);
}

/* Output C3.js bandwidth plot definitions. */
static void
hits_bw_plot (FILE * fp, GHTMLPlot plot, int sp)
{
  /* *INDENT-OFF* */
  GChart def[] = {
    {"y0", (GChartDef[]) {
      {"key", "bytes"}, {"label", MTRC_BW_LBL}, {"format", "bytes"}, ChartDefStopper
    }},
  };

  if (!conf.bandwidth)
    return;

  plot.chart_key = (char[]) {"bandwidth"};
  plot.chart_lbl = (char *) {MTRC_BW_LBL};
  /* *INDENT-ON* */
  print_plot_def (fp, plot, def, ARRAY_SIZE (def), sp);
}

/* Output C3.js bandwidth plot definitions. */
static void
hits_bw_req_plot (FILE * fp, GHTMLPlot plot, int sp)
{
  /* *INDENT-OFF* */
  GChart def[] = {
    {"x", (GChartDef[]) {
      {"key", "[\"method\", \"protocol\", \"data\"]"}, ChartDefStopper
    }},
    {"y0", (GChartDef[]) {
      {"key", "bytes"}, {"label", MTRC_BW_LBL}, {"format", "bytes"}, ChartDefStopper
    }},
  };

  if (!conf.bandwidth)
    return;

  plot.chart_key = (char[]) {"bandwidth"};
  plot.chart_lbl = (char *) {MTRC_BW_LBL};
  /* *INDENT-ON* */
  print_plot_def (fp, plot, def, ARRAY_SIZE (def), sp);
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
  fpskeysval (fp, "url", (conf.ws_url ? conf.ws_url : ""), sp, 0);
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
  if (def.metakey)
    fpskeysval (fp, "meta", def.metakey, isp, 0);
  if (def.metatype)
    fpskeysval (fp, "metaType", def.metatype, isp, 0);
  if (def.metalbl)
    fpskeysval (fp, "metaLabel", def.metalbl, isp, 0);
  if (def.datatype)
    fpskeysval (fp, "dataType", def.datatype, isp, 0);
  if (def.datakey)
    fpskeysval (fp, "key", def.datakey, isp, 0);
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
    .datatype = "numeric",
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
    .datatype = "numeric",
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
    .datatype = "numeric",
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
    .datatype = "secs",
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
    .lbl = T_UNIQUE_VISITORS,
    .datatype = "numeric",
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
    .lbl = T_UNIQUE_FILES,
    .datatype = "numeric",
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
    .datatype = "numeric",
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
    .datatype = "numeric",
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
    .datatype = "numeric",
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
    .lbl = T_STATIC_FILES,
    .datatype = "numeric",
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
    .datatype = "bytes",
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
    .datatype = "bytes",
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
    .datakey = "hits",
    .lbl = MTRC_HITS_LBL,
    .datatype = "numeric",
    .metakey = "count",
    .cwidth = "12%",
  };
  print_def_block (fp, def, sp, 0);
}

/* Output JSON visitors definition block. */
static void
print_def_visitors (FILE * fp, int sp)
{
  GDefMetric def = {
    .datakey = "visitors",
    .lbl = MTRC_VISITORS_LBL,
    .datatype = "numeric",
    .metakey = "count",
    .cwidth = "12%",
  };
  print_def_block (fp, def, sp, 0);
}

/* Output JSON bandwidth definition block. */
static void
print_def_bw (FILE * fp, int sp)
{
  GDefMetric def = {
    .datakey = "bytes",
    .lbl = MTRC_BW_LBL,
    .datatype = "bytes",
    .metakey = "count",
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
    .datakey = "avgts",
    .lbl = MTRC_AVGTS_LBL,
    .datatype = "utime",
    .metakey = "avg",
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
    .datakey = "cumts",
    .lbl = MTRC_CUMTS_LBL,
    .datatype = "utime",
    .metakey = "count",
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
    .datakey = "maxts",
    .lbl = MTRC_MAXTS_LBL,
    .datatype = "utime",
    .metakey = "count",
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
    .datakey = "method",
    .lbl = MTRC_METHODS_LBL,
    .datatype = "string",
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
    .datakey = "protocol",
    .lbl = MTRC_PROTOCOLS_LBL,
    .datatype = "string",
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
    .datakey = "city",
    .lbl = MTRC_CITY_LBL,
    .datatype = "string",
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
    .datakey = "country",
    .lbl = MTRC_COUNTRY_LBL,
    .datatype = "string",
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
    .datakey = "hostname",
    .lbl = MTRC_HOSTNAME_LBL,
    .datatype = "string",
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
    .cname = "trunc",
    .cwidth = "100%",
    .datakey = "data",
    .datatype = module == VISITORS ? "date" : "string",
    .lbl = MTRC_DATA_LBL,
    .metakey = "unique",
    .metalbl = "Total",
    .metatype = "numeric",
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

/* Cheap JSON internationalisation (i18n) - report labels */
static void
print_json_i18n_def (FILE * fp)
{
  int sp = 0;
  size_t i = 0;

  /* *INDENT-OFF* */
  static const char *json_i18n[][2] = {
    {"theme"          , HTML_REPORT_NAV_THEME}          ,
    {"dark_gray"      , HTML_REPORT_NAV_DARK_GRAY}      ,
    {"bright"         , HTML_REPORT_NAV_BRIGHT}         ,
    {"dark_blue"      , HTML_REPORT_NAV_DARK_BLUE}      ,
    {"dark_purple"    , HTML_REPORT_NAV_DARK_PURPLE}    ,
    {"panels"         , HTML_REPORT_NAV_PANELS}         ,
    {"items_per_page" , HTML_REPORT_NAV_ITEMS_PER_PAGE} ,
    {"tables"         , HTML_REPORT_NAV_TABLES}         ,
    {"display_tables" , HTML_REPORT_NAV_DISPLAY_TABLES} ,
    {"ah_small"       , HTML_REPORT_NAV_AH_SMALL}       ,
    {"ah_small_title" , HTML_REPORT_NAV_AH_SMALL_TITLE} ,
    {"layout"         , HTML_REPORT_NAV_LAYOUT}         ,
    {"horizontal"     , HTML_REPORT_NAV_HOR}            ,
    {"vertical"       , HTML_REPORT_NAV_VER}            ,
    {"file_opts"      , HTML_REPORT_NAV_FILE_OPTS}      ,
    {"export_json"    , HTML_REPORT_NAV_EXPORT_JSON}    ,
    {"panel_opts"     , HTML_REPORT_PANEL_PANEL_OPTS}   ,
    {"previous"       , HTML_REPORT_PANEL_PREVIOUS}     ,
    {"next"           , HTML_REPORT_PANEL_NEXT}         ,
    {"first"          , HTML_REPORT_PANEL_FIRST}        ,
    {"last"           , HTML_REPORT_PANEL_LAST}         ,
    {"chart_opts"     , HTML_REPORT_PANEL_CHART_OPTS}   ,
    {"chart"          , HTML_REPORT_PANEL_CHART}        ,
    {"type"           , HTML_REPORT_PANEL_TYPE}         ,
    {"area_spline"    , HTML_REPORT_PANEL_AREA_SPLINE}  ,
    {"bar"            , HTML_REPORT_PANEL_BAR}          ,
    {"plot_metric"    , HTML_REPORT_PANEL_PLOT_METRIC}  ,
    {"table_columns"  , HTML_REPORT_PANEL_TABLE_COLS}   ,
    {"thead"          , T_HEAD}                         ,
    {"version"        , GO_VERSION}                     ,
  };
  /* *INDENT-ON* */

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    sp = 1;

  fpopen_obj (fp, 0);
  for (i = 0; i < ARRAY_SIZE (json_i18n); ++i) {
    fpskeysval (fp, json_i18n[i][0], _(json_i18n[i][1]), sp, 0);
  }
  fpclose_obj (fp, 0, 1);
}

/* Output definitions for the given panel. */
static void
print_json_def_summary (FILE * fp)
{
  int sp = 0;

  /* use tabs to prettify output */
  if (conf.json_pretty_print)
    sp = 1;

  /* output open panel attribute */
  fpopen_obj_attr (fp, GENER_ID, sp);
  print_def_meta (fp, _(T_HEAD), "", sp);
  print_def_summary (fp, sp);
  /* output close panel attribute */
  fpclose_obj (fp, sp, 0);
}

/* Entry point to output definitions for all panels. */
static void
print_json_defs (FILE * fp)
{
  const GHTML *def;
  size_t idx = 0;

  fprintf (fp, "<script type='text/javascript'>");

  fprintf (fp, "var json_i18n=");
  print_json_i18n_def (fp);
  fprintf (fp, ";");

  fprintf (fp, "var html_prefs=%s;", conf.html_prefs ? conf.html_prefs : "{}");
  fprintf (fp, "var user_interface=");
  fpopen_obj (fp, 0);

  print_json_def_summary (fp);
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
  strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S %z", now_tm);

  print_html_header (fp);

  print_html_body (fp, now);
  print_json_defs (fp);
  print_json_data (fp, glog, holder);
  print_conn_def (fp);

  print_html_footer (fp);

  fclose (fp);
}
