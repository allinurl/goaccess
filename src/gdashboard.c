/**
 * gdashboard.c -- goaccess main dashboard
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2023 Gerardo Orellana <hello @ goaccess.io>
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

#define _XOPEN_SOURCE 700

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <regex.h>
#include <inttypes.h>

#include "gdashboard.h"

#include "gkhash.h"
#include "gholder.h"
#include "color.h"
#include "error.h"
#include "gstorage.h"
#include "util.h"
#include "xmalloc.h"

static GFind find_t;

/* Reset find indices */
void
reset_find (void) {
  if (find_t.pattern != NULL && *find_t.pattern != '\0')
    free (find_t.pattern);

  find_t.look_in_sub = 0;
  find_t.module = 0;
  find_t.next_idx = 0;  /* next total index    */
  find_t.next_parent_idx = 0;   /* next parent index   */
  find_t.next_sub_idx = 0;      /* next sub item index */
  find_t.pattern = NULL;
}

/* Allocate memory for a new GDash instance.
 *
 * On success, the newly allocated GDash is returned . */
GDash *
new_gdash (void) {
  GDash *dash = xmalloc (sizeof (GDash));
  memset (dash, 0, sizeof *dash);
  dash->total_alloc = 0;

  return dash;
}

/* Allocate memory for a new GDashData instance.
 *
 * On success, the newly allocated GDashData is returned . */
GDashData *
new_gdata (uint32_t size) {
  GDashData *data = xcalloc (size, sizeof (GDashData));

  return data;
}

/* Free memory allocated for a GDashData instance. Includes malloc'd
 * strings. */
static void
free_dashboard_data (GDashData item) {
  if (item.metrics == NULL)
    return;

  if (item.metrics->data)
    free (item.metrics->data);
  if (item.metrics->bw.sbw)
    free (item.metrics->bw.sbw);
  if (conf.serve_usecs && item.metrics->avgts.sts)
    free (item.metrics->avgts.sts);
  if (conf.serve_usecs && item.metrics->cumts.sts)
    free (item.metrics->cumts.sts);
  if (conf.serve_usecs && item.metrics->maxts.sts)
    free (item.metrics->maxts.sts);
  free (item.metrics);
}

/* Free memory allocated for a GDash instance, and nested structure
 * data. */
void
free_dashboard (GDash *dash) {
  GModule module;
  int j;
  size_t idx = 0;

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    for (j = 0; j < dash->module[module].alloc_data; j++) {
      free_dashboard_data (dash->module[module].data[j]);
    }
    free (dash->module[module].data);
  }
  free (dash);
}

/* Get the current panel/module given the `Y` offset (position) in the
 * terminal dashboard.
 *
 * If not found, 0 is returned.
 * If found, the module number is returned . */
static GModule
get_find_current_module (GDash *dash, int offset) {
  GModule module;
  size_t idx = 0;

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    /* set current module */
    if (dash->module[module].pos_y == offset)
      return module;
    /* we went over by one module, set current - 1 */
    if (dash->module[module].pos_y > offset)
      return module - 1;
  }

  return 0;
}

/* Get the number of rows that a collapsed dashboard panel contains.
 *
 * On success, the number of rows is returned. */
int
get_num_collapsed_data_rows (void) {
  /* The default number of rows is fixed */
  int size = DASH_COLLAPSED - DASH_NON_DATA;
  /* If no column names, then add the number of rows occupied by the
   * column values to the default number */
  return conf.no_column_names ? size + DASH_COL_ROWS : size;
}

/* Get the number of rows that an expanded dashboard panel contains.
 *
 * On success, the number of rows is returned. */
int
get_num_expanded_data_rows (void) {
  /* The default number of rows is fixed */
  int size = DASH_EXPANDED - DASH_NON_DATA;
  /* If no column names, then add the number of rows occupied by the
   * column values to the default number */
  return conf.no_column_names ? size + DASH_COL_ROWS : size;
}

/* Get the Y position of the terminal dashboard where data rows
 * (metrics) start.
 *
 * On success, the Y position is returned. */
static int
get_data_pos_rows (void) {
  return conf.no_column_names ? DASH_DATA_POS - DASH_COL_ROWS : DASH_DATA_POS;
}

/* Get the initial X position of the terminal dashboard where metrics
 * and data columns start.
 *
 * On success, the X position is returned. */
static int
get_xpos (void) {
  return DASH_INIT_X;
}

/* Determine which module should be expanded given the current mouse
 * position.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
int
set_module_from_mouse_event (GScroll *gscroll, GDash *dash, int y) {
  int module = 0;
  int offset = y - MAX_HEIGHT_HEADER - MAX_HEIGHT_FOOTER + 1;
  if (gscroll->expanded) {
    module = get_find_current_module (dash, offset);
  } else {
    offset += gscroll->dash;
    module = offset / DASH_COLLAPSED;
  }

  if (module >= TOTAL_MODULES)
    module = TOTAL_MODULES - 1;
  else if (module < 0)
    module = 0;

  if ((int) gscroll->current == module)
    return 1;

  gscroll->current = module;
  return 0;
}

/* Allocate a new string for a sub item on the terminal dashboard.
 *
 * On error, NULL is returned.
 * On success, the newly allocated string is returned. */
static char *
render_child_node (const char *data) {
  char *buf;
  int len = 0;

  /* chars to use based on encoding used */
#ifdef HAVE_LIBNCURSESW
  const char *bend = "\xe2\x94\x9c";
  const char *horz = "\xe2\x94\x80";
#else
  const char *bend = "|";
  const char *horz = "`-";
#endif

  if (data == NULL || *data == '\0')
    return NULL;

  len = snprintf (NULL, 0, " %s%s %s", bend, horz, data);
  buf = xmalloc (len + 3);
  sprintf (buf, " %s%s %s", bend, horz, data);

  return buf;
}

/* Get a string of bars given current hits, maximum hit & xpos.
 *
 * On success, the newly allocated string representing the chart is
 * returned. */
static char *
get_bars (int n, int max, int x) {
  int w, h;
  float len = 0.0;

  getmaxyx (stdscr, h, w);
  (void) h;     /* avoid lint warning */

  len = ((((float) n) / max));
  len *= (w - x);
  if (len < 1)
    len = 1;
  return char_repeat (len, '|');
}

/* Get largest hits metric.
 *
 * On error, 0 is returned.
 * On success, largest hits metric is returned. */
static void
set_max_metrics (GDashMeta *meta, GDashData *idata) {
  if (meta->max_hits < idata->metrics->hits)
    meta->max_hits = idata->metrics->hits;
  if (meta->max_visitors < idata->metrics->visitors)
    meta->max_visitors = idata->metrics->visitors;
}

/* Set largest hits metric (length of the integer). */
static void
set_max_hit_len (GDashMeta *meta, GDashData *idata) {
  int vlen = intlen (idata->metrics->hits);
  int llen = strlen (MTRC_HITS_LBL);

  if (vlen > meta->hits_len)
    meta->hits_len = vlen;

  /* if outputting with column names, then determine if the value is
   * longer than the length of the column name */
  if (llen > meta->hits_len)
    meta->hits_len = llen;
}

/* Get the percent integer length. */
static void
set_max_hit_perc_len (GDashMeta *meta, GDashData *idata) {
  int vlen = intlen (idata->metrics->hits_perc);
  int llen = strlen (MTRC_HITS_PERC_LBL);

  if (vlen > meta->hits_perc_len)
    meta->hits_perc_len = vlen;

  /* if outputting with column names, then determine if the value is
   * longer than the length of the column name */
  if (llen > meta->hits_perc_len)
    meta->hits_perc_len = llen;
}

/* Set largest hits metric (length of the integer). */
static void
set_max_visitors_len (GDashMeta *meta, GDashData *idata) {
  int vlen = intlen (idata->metrics->visitors);
  int llen = strlen (MTRC_VISITORS_SHORT_LBL);

  if (vlen > meta->visitors_len)
    meta->visitors_len = vlen;

  /* if outputting with column names, then determine if the value is
   * longer than the length of the column name */
  if (llen > meta->visitors_len)
    meta->visitors_len = llen;
}

/* Get the percent integer length. */
static void
set_max_visitors_perc_len (GDashMeta *meta, GDashData *idata) {
  int vlen = intlen (idata->metrics->visitors_perc);
  int llen = strlen (MTRC_VISITORS_PERC_LBL);

  if (vlen > meta->visitors_perc_len)
    meta->visitors_perc_len = vlen;

  /* if outputting with column names, then determine if the value is
   * longer than the length of the column name */
  if (llen > meta->visitors_perc_len)
    meta->visitors_perc_len = llen;
}

/* Get the percent integer length. */
static void
set_max_bw_len (GDashMeta *meta, GDashData *idata) {
  int vlen = strlen (idata->metrics->bw.sbw);
  int llen = strlen (MTRC_BW_LBL);

  if (vlen > meta->bw_len)
    meta->bw_len = vlen;

  /* if outputting with column names, then determine if the value is
   * longer than the length of the column name */
  if (llen > meta->bw_len)
    meta->bw_len = llen;
}

/* Get the percent integer length. */
static void
set_max_avgts_len (GDashMeta *meta, GDashData *idata) {
  int vlen = 0, llen = 0;

  if (!conf.serve_usecs || !idata->metrics->avgts.sts)
    return;

  vlen = strlen (idata->metrics->avgts.sts);
  llen = strlen (MTRC_AVGTS_LBL);

  if (vlen > meta->avgts_len)
    meta->avgts_len = vlen;

  /* if outputting with column names, then determine if the value is
   * longer than the length of the column name */
  if (llen > meta->avgts_len)
    meta->avgts_len = llen;
}

/* Get the percent integer length. */
static void
set_max_cumts_len (GDashMeta *meta, GDashData *idata) {
  int vlen = 0, llen = 0;

  if (!conf.serve_usecs || !idata->metrics->cumts.sts)
    return;

  vlen = strlen (idata->metrics->cumts.sts);
  llen = strlen (MTRC_AVGTS_LBL);

  if (vlen > meta->cumts_len)
    meta->cumts_len = vlen;

  /* if outputting with column names, then determine if the value is
   * longer than the length of the column name */
  if (llen > meta->cumts_len)
    meta->cumts_len = llen;
}

/* Get the percent integer length. */
static void
set_max_maxts_len (GDashMeta *meta, GDashData *idata) {
  int vlen = 0, llen = 0;

  if (!conf.serve_usecs || !idata->metrics->maxts.sts)
    return;

  vlen = strlen (idata->metrics->maxts.sts);
  llen = strlen (MTRC_AVGTS_LBL);

  if (vlen > meta->maxts_len)
    meta->maxts_len = vlen;

  /* if outputting with column names, then determine if the value is
   * longer than the length of the column name */
  if (llen > meta->maxts_len)
    meta->maxts_len = llen;
}

/* Get the percent integer length. */
static void
set_max_method_len (GDashMeta *meta, GDashData *idata) {
  int vlen = 0, llen = 0;

  if (!conf.append_method || !idata->metrics->method)
    return;

  vlen = strlen (idata->metrics->method);
  llen = strlen (MTRC_METHODS_SHORT_LBL);

  if (vlen > meta->method_len)
    meta->method_len = vlen;

  /* if outputting with column names, then determine if the value is
   * longer than the length of the column name */
  if (llen > meta->method_len)
    meta->method_len = llen;
}

/* Get the percent integer length. */
static void
set_max_protocol_len (GDashMeta *meta, GDashData *idata) {
  int vlen = 0, llen = 0;

  if (!conf.append_protocol || !idata->metrics->protocol)
    return;

  vlen = strlen (idata->metrics->protocol);
  llen = strlen (MTRC_PROTOCOLS_SHORT_LBL);

  if (vlen > meta->protocol_len)
    meta->protocol_len = vlen;

  /* if outputting with column names, then determine if the value is
   * longer than the length of the column name */
  if (llen > meta->protocol_len)
    meta->protocol_len = llen;
}

/* Get the percent integer length. */
static void
set_max_data_len (GDashMeta *meta, GDashData *idata) {
  int vlen = 0, llen = 0;

  vlen = strlen (idata->metrics->data);
  llen = strlen (MTRC_DATA_LBL);

  if (vlen > meta->data_len)
    meta->data_len = vlen;

  /* if outputting with column names, then determine if the value is
   * longer than the length of the column name */
  if (llen > meta->data_len)
    meta->data_len = llen;
}

static void
set_metrics_len (GDashMeta *meta, GDashData *idata) {
  /* integer-based length */
  set_max_hit_len (meta, idata);
  set_max_hit_perc_len (meta, idata);
  set_max_visitors_len (meta, idata);
  set_max_visitors_perc_len (meta, idata);

  /* string-based length */
  set_max_bw_len (meta, idata);
  set_max_avgts_len (meta, idata);
  set_max_cumts_len (meta, idata);
  set_max_maxts_len (meta, idata);

  set_max_method_len (meta, idata);
  set_max_protocol_len (meta, idata);
  set_max_data_len (meta, idata);
}

/* Render host's panel selected row */
static void
render_data_hosts (WINDOW *win, GDashRender render, char *value, int x) {
  char *padded_data;

  padded_data = left_pad_str (value, x);
  draw_header (win, padded_data, "%s", render.y, 0, render.w, color_selected);
  free (padded_data);
}

/* Set panel's date on the given buffer
 *
 * On error, '---' placeholder is returned.
 * On success, the formatted date is returned. */
static char *
set_visitors_date (const char *value) {
  return get_visitors_date (value, conf.spec_date_time_num_format, conf.spec_date_time_format);
}

static char *
get_fixed_fmt_width (int w, char type) {
  char *fmt = xmalloc (snprintf (NULL, 0, "%%%d%c", w, type) + 1);
  sprintf (fmt, "%%%d%c", w, type);

  return fmt;
}

/* Render the 'total' label on each panel */
static void
render_total_label (WINDOW *win, GDashModule *data, int y, GColors *(*func) (void)) {
  char *s;
  int win_h, win_w, total, ht_size;

  total = data->holder_size;
  ht_size = data->ht_size;

  s = xmalloc (snprintf (NULL, 0, "%s: %d/%d", GEN_TOTAL, total, ht_size) + 1);
  getmaxyx (win, win_h, win_w);
  (void) win_h;

  sprintf (s, "%s: %d/%d", GEN_TOTAL, total, ht_size);
  draw_header (win, s, "%s", y, win_w - strlen (s) - 2, win_w, func);
  free (s);
}

/* Render panel bar graph */
static void
render_bars (GDashModule *data, GDashRender render, int *x) {
  GColors *color = get_color (COLOR_BARS);
  WINDOW *win = render.win;
  char *bar;
  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;

  bar = get_bars (data->data[idx].metrics->hits, data->meta.max_hits, *x);
  if (sel)
    draw_header (win, bar, "%s", y, *x, w, color_selected);
  else {
    wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
    mvwprintw (win, y, *x, "%s", bar);
    wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));
  }
  free (bar);
}

/* Render the data metric for each panel */
static void
render_data (GDashModule *data, GDashRender render, int *x) {
  GColors *color = get_color_by_item_module (COLOR_MTRC_DATA, data->module);
  WINDOW *win = render.win;

  char *date = NULL, *value = NULL, *buf = NULL;
  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;
  int date_len = 0;

  value = substring (data->data[idx].metrics->data, 0, w - *x);
  if (data->module == VISITORS) {
    date = set_visitors_date (value);
    date_len = strlen (date);
  }

  if (sel && data->module == HOSTS && data->data[idx].is_subitem) {
    render_data_hosts (win, render, value, *x);
  } else if (sel) {
    buf = data->module == VISITORS ? date : value;
    draw_header (win, buf, "%s", y, *x, w, color_selected);
  } else {
    wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
    mvwprintw (win, y, *x, "%s", data->module == VISITORS ? date : value);
    wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));
  }

  *x += data->module == VISITORS ? date_len : data->meta.data_len;
  *x += DASH_SPACE;
  free (value);
  free (date);
}

/* Render the method metric for each panel
 *
 * On error, no method is rendered and it returns.
 * On success, method is rendered. */
static void
render_method (GDashModule *data, GDashRender render, int *x) {
  GColors *color = get_color_by_item_module (COLOR_MTRC_MTHD, data->module);
  WINDOW *win = render.win;

  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;
  char *method = data->data[idx].metrics->method;

  if (method == NULL || *method == '\0')
    return;

  if (sel) {
    /* selected state */
    draw_header (win, method, "%s", y, *x, w, color_selected);
  } else {
    /* regular state */
    wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
    mvwprintw (win, y, *x, "%s", method);
    wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));
  }

  *x += data->meta.method_len + DASH_SPACE;
}

/* Render the protocol metric for each panel
 *
 * On error, no protocol is rendered and it returns.
 * On success, protocol is rendered. */
static void
render_proto (GDashModule *data, GDashRender render, int *x) {
  GColors *color = get_color_by_item_module (COLOR_MTRC_PROT, data->module);
  WINDOW *win = render.win;

  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;
  char *protocol = data->data[idx].metrics->protocol;

  if (protocol == NULL || *protocol == '\0')
    return;

  if (sel) {
    /* selected state */
    draw_header (win, protocol, "%s", y, *x, w, color_selected);
  } else {
    /* regular state */
    wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
    mvwprintw (win, y, *x, "%s", protocol);
    wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));
  }

  *x += REQ_PROTO_LEN - 1 + DASH_SPACE;
}

/* Render the average time served metric for each panel */
static void
render_avgts (GDashModule *data, GDashRender render, int *x) {
  GColors *color = get_color_by_item_module (COLOR_MTRC_AVGTS, data->module);
  WINDOW *win = render.win;

  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;
  char *avgts = data->data[idx].metrics->avgts.sts;

  if (data->module == HOSTS && data->data[idx].is_subitem)
    goto out;

  if (sel) {
    /* selected state */
    draw_header (win, avgts, "%9s", y, *x, w, color_selected);
  } else {
    /* regular state */
    wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
    mvwprintw (win, y, *x, "%9s", avgts);
    wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));
  }

out:
  *x += DASH_SRV_TM_LEN + DASH_SPACE;
}

/* Render the cumulative time served metric for each panel */
static void
render_cumts (GDashModule *data, GDashRender render, int *x) {
  GColors *color = get_color_by_item_module (COLOR_MTRC_CUMTS, data->module);
  WINDOW *win = render.win;

  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;
  char *cumts = data->data[idx].metrics->cumts.sts;

  if (data->module == HOSTS && data->data[idx].is_subitem)
    goto out;

  if (sel) {
    /* selected state */
    draw_header (win, cumts, "%9s", y, *x, w, color_selected);
  } else {
    /* regular state */
    wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
    mvwprintw (win, y, *x, "%9s", cumts);
    wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));
  }

out:
  *x += DASH_SRV_TM_LEN + DASH_SPACE;
}

/* Render the maximum time served metric for each panel */
static void
render_maxts (GDashModule *data, GDashRender render, int *x) {
  GColors *color = get_color_by_item_module (COLOR_MTRC_MAXTS, data->module);
  WINDOW *win = render.win;

  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;
  char *maxts = data->data[idx].metrics->maxts.sts;

  if (data->module == HOSTS && data->data[idx].is_subitem)
    goto out;

  if (sel) {
    /* selected state */
    draw_header (win, maxts, "%9s", y, *x, w, color_selected);
  } else {
    /* regular state */
    wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
    mvwprintw (win, y, *x, "%9s", maxts);
    wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));
  }

out:
  *x += DASH_SRV_TM_LEN + DASH_SPACE;
}

/* Render the bandwidth metric for each panel */
static void
render_bw (GDashModule *data, GDashRender render, int *x) {
  GColors *color = get_color_by_item_module (COLOR_MTRC_BW, data->module);
  WINDOW *win = render.win;

  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;
  char *bw = data->data[idx].metrics->bw.sbw;

  if (data->module == HOSTS && data->data[idx].is_subitem)
    goto out;

  if (sel) {
    char *fw = get_fixed_fmt_width (data->meta.bw_len, 's');
    /* selected state */
    draw_header (win, bw, fw, y, *x, w, color_selected);
    free (fw);
  } else {
    /* regular state */
    wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
    mvwprintw (win, y, *x, "%*s", data->meta.bw_len, bw);
    wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));
  }

out:
  *x += data->meta.bw_len + DASH_SPACE;
}

/* Render a percent metric */
static void
render_percent (GDashRender render, GColors *color, float perc, int len, int x) {
  WINDOW *win = render.win;
  char *percent;
  int y = render.y, w = render.w, sel = render.sel;

  if (sel) {
    /* selected state */
    percent = float2str (perc, len);
    draw_header (win, percent, "%s%%", y, x, w, color_selected);
    free (percent);
  } else {
    /* regular state */
    wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
    mvwprintw (win, y, x, "%*.2f%%", len, perc);
    wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));
  }
}

/* Render the hits percent metric for each panel */
static void
render_hits_percent (GDashModule *data, GDashRender render, int *x) {
  GColorItem item = COLOR_MTRC_HITS_PERC;
  GColors *color;
  int l = data->meta.hits_perc_len + 3, idx = render.idx;

  if (data->module == HOSTS && data->data[idx].is_subitem)
    goto out;

  if (data->meta.max_hits == data->data[idx].metrics->hits)
    item = COLOR_MTRC_HITS_PERC_MAX;

  color = get_color_by_item_module (item, data->module);
  render_percent (render, color, data->data[idx].metrics->hits_perc, l, *x);

out:
  *x += l + 1 + DASH_SPACE;
}

/* Render the visitors percent metric for each panel */
static void
render_visitors_percent (GDashModule *data, GDashRender render, int *x) {
  GColorItem item = COLOR_MTRC_VISITORS_PERC;
  GColors *color;
  int l = data->meta.visitors_perc_len + 3, idx = render.idx;

  if (data->module == HOSTS && data->data[idx].is_subitem)
    goto out;

  if (data->meta.max_visitors == data->data[idx].metrics->visitors)
    item = COLOR_MTRC_VISITORS_PERC_MAX;

  color = get_color_by_item_module (item, data->module);
  render_percent (render, color, data->data[idx].metrics->visitors_perc, l, *x);

out:
  *x += l + 1 + DASH_SPACE;
}

/* Render the hits metric for each panel */
static void
render_hits (GDashModule *data, GDashRender render, int *x) {
  GColors *color = get_color_by_item_module (COLOR_MTRC_HITS, data->module);
  WINDOW *win = render.win;

  char *hits;
  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;
  int len = data->meta.hits_len;

  if (data->module == HOSTS && data->data[idx].is_subitem)
    goto out;

  if (sel) {
    /* selected state */
    hits = u642str (data->data[idx].metrics->hits, len);
    draw_header (win, hits, " %s", y, 0, w, color_selected);
    free (hits);
  } else {
    /* regular state */
    wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
    mvwprintw (win, y, *x, "%*" PRIu64 "", len, data->data[idx].metrics->hits);
    wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));
  }

out:
  *x += len + DASH_SPACE;
}

/* Render the visitors metric for each panel */
static void
render_visitors (GDashModule *data, GDashRender render, int *x) {
  GColors *color = get_color_by_item_module (COLOR_MTRC_VISITORS, data->module);
  WINDOW *win = render.win;

  char *visitors;
  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;
  int len = data->meta.visitors_len;

  if (data->module == HOSTS && data->data[idx].is_subitem)
    goto out;

  if (sel) {
    /* selected state */
    visitors = u642str (data->data[idx].metrics->visitors, len);
    draw_header (win, visitors, "%s", y, *x, w, color_selected);
    free (visitors);
  } else {
    /* regular state */
    wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
    mvwprintw (win, y, *x, "%*" PRIu64 "", len, data->data[idx].metrics->visitors);
    wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));
  }

out:
  *x += len + DASH_SPACE;
}

/* Render the header row for each panel */
static void
render_header (WINDOW *win, GDashModule *data, GModule cur_module, int *y) {
  GColors *(*func) (void);
  char ind;
  char *hd;
  int k, w, h;

  getmaxyx (win, h, w);
  (void) h;

  k = data->module + 1;
  ind = cur_module == data->module ? '>' : ' ';
  func = cur_module == data->module &&
    conf.hl_header ? color_panel_active : color_panel_header;
  hd = xmalloc (snprintf (NULL, 0, "%c %d - %s", ind, k, data->head) + 1);
  sprintf (hd, "%c %d - %s", ind, k, data->head);

  draw_header (win, hd, " %s", (*y), 0, w, func);
  free (hd);

  render_total_label (win, data, (*y), func);
  data->pos_y = (*y);
  (*y)++;
}

/* Render the description row for each panel */
static void
render_description (WINDOW *win, GDashModule *data, int *y) {
  int w, h;

  getmaxyx (win, h, w);
  (void) h;

  draw_header (win, data->desc, " %s", (*y), 0, w, color_panel_desc);

  data->pos_y = (*y);
  (*y)++;
  (*y)++;       /* add empty line underneath description */
}

/* Render available metrics per panel.
 * ###TODO: Have the abilitity to display metrics in specific order */
static void
render_metrics (GDashModule *data, GDashRender render, int expanded) {
  int x = get_xpos ();
  GModule module = data->module;
  const GOutput *output = output_lookup (module);

  /* basic metrics */
  if (output->hits)
    render_hits (data, render, &x);
  if (output->percent)
    render_hits_percent (data, render, &x);
  if (output->visitors)
    render_visitors (data, render, &x);
  if (output->percent)
    render_visitors_percent (data, render, &x);

  /* render bandwidth if available */
  if (conf.bandwidth && output->bw)
    render_bw (data, render, &x);

  /* render avgts, cumts and maxts if available */
  if (output->avgts && conf.serve_usecs)
    render_avgts (data, render, &x);
  if (output->cumts && conf.serve_usecs)
    render_cumts (data, render, &x);
  if (output->maxts && conf.serve_usecs)
    render_maxts (data, render, &x);

  /* render request method if available */
  if (output->method && conf.append_method)
    render_method (data, render, &x);
  /* render request protocol if available */
  if (output->protocol && conf.append_protocol)
    render_proto (data, render, &x);
  if (output->data)
    render_data (data, render, &x);

  /* skip graph bars if module is expanded and we have sub nodes */
  if ((output->graph && !expanded) || (output->sub_graph && expanded))
    render_bars (data, render, &x);
}

/* Render a dashboard row. */
static void
render_data_line (WINDOW *win, GDashModule *data, int *y, int j, GScroll *gscroll) {
  GDashRender render;
  GModule module = data->module;
  int expanded = 0, sel = 0;
  int w, h;

  getmaxyx (win, h, w);
  (void) h;

  if (gscroll->expanded && module == gscroll->current)
    expanded = 1;

  if (j >= data->idx_data)
    goto out;

  sel = expanded && j == gscroll->module[module].scroll ? 1 : 0;

  render.win = win;
  render.y = *y;
  render.w = w;
  render.idx = j;
  render.sel = sel;

  render_metrics (data, render, expanded);

out:
  (*y)++;
}

/* Render a dashed line underneath the metric label. */
static void
print_horizontal_dash (WINDOW *win, int y, int x, int len) {
  mvwprintw (win, y, x, "%.*s", len, "----------------");
}

/* Render left-aligned column label. */
static void
lprint_col (WINDOW *win, int y, int *x, int len, const char *str) {
  GColors *color = get_color (COLOR_PANEL_COLS);

  wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
  mvwprintw (win, y, *x, "%s", str);
  print_horizontal_dash (win, y + 1, *x, len);
  wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));

  *x += len + DASH_SPACE;
}

/* Render right-aligned column label. */
static void
rprint_col (WINDOW *win, int y, int *x, int len, const char *str) {
  GColors *color = get_color (COLOR_PANEL_COLS);

  wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
  mvwprintw (win, y, *x, "%*s", len, str);
  print_horizontal_dash (win, y + 1, *x, len);
  wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));

  *x += len + DASH_SPACE;
}

/* Render column names for available metrics.
 * ###TODO: Have the abilitity to display metrics in specific order */
static void
render_cols (WINDOW *win, GDashModule *data, int *y) {
  GModule module = data->module;
  const GOutput *output = output_lookup (module);
  int x = get_xpos ();

  if (data->idx_data == 0 || conf.no_column_names)
    return;

  if (output->hits)
    lprint_col (win, *y, &x, data->meta.hits_len, MTRC_HITS_LBL);

  if (output->percent)
    rprint_col (win, *y, &x, data->meta.hits_perc_len + 4, MTRC_HITS_PERC_LBL);

  if (output->visitors)
    rprint_col (win, *y, &x, data->meta.visitors_len, MTRC_VISITORS_SHORT_LBL);

  if (output->percent)
    rprint_col (win, *y, &x, data->meta.visitors_perc_len + 4, MTRC_VISITORS_PERC_LBL);

  if (output->bw && conf.bandwidth)
    rprint_col (win, *y, &x, data->meta.bw_len, MTRC_BW_LBL);

  if (output->avgts && conf.serve_usecs)
    rprint_col (win, *y, &x, DASH_SRV_TM_LEN, MTRC_AVGTS_LBL);

  if (output->cumts && conf.serve_usecs)
    rprint_col (win, *y, &x, DASH_SRV_TM_LEN, MTRC_CUMTS_LBL);

  if (output->maxts && conf.serve_usecs)
    rprint_col (win, *y, &x, DASH_SRV_TM_LEN, MTRC_MAXTS_LBL);

  if (output->method && conf.append_method)
    lprint_col (win, *y, &x, data->meta.method_len, MTRC_METHODS_SHORT_LBL);

  if (output->protocol && conf.append_protocol)
    lprint_col (win, *y, &x, 8, MTRC_PROTOCOLS_SHORT_LBL);

  if (output->data)
    lprint_col (win, *y, &x, 4, MTRC_DATA_LBL);
}

/* Iterate over all dashboard data and render its content. */
static void
render_content (WINDOW *win, GDashModule *data, int *y, int *offset,
                int *total, GScroll *gscroll) {
  GModule module = data->module;
  int i, j, size, h, w, data_pos = get_data_pos_rows ();

  getmaxyx (win, h, w);
  (void) w;

  size = data->dash_size;
  for (i = *offset; i < size; i++) {
    /* header */
    if ((i % size) == DASH_HEAD_POS) {
      render_header (win, data, gscroll->current, y);
    } else if ((i % size) == DASH_EMPTY_POS && conf.no_column_names) {
      /* if no column names, print panel description */
      render_description (win, data, y);
    } else if ((i % size) == DASH_EMPTY_POS || (i % size) == size - 1) {
      /* blank lines */
      (*y)++;
    } else if ((i % size) == DASH_DASHES_POS && !conf.no_column_names) {
      /* account for already printed dash lines under columns */
      (*y)++;
    } else if ((i % size) == DASH_COLS_POS && !conf.no_column_names) {
      /* column headers lines */
      render_cols (win, data, y);
      (*y)++;
    } else if ((i % size) >= data_pos || (i % size) <= size - 2) {
      /* account for 2 lines at the header and 2 blank lines */
      j = ((i % size) - data_pos) + gscroll->module[module].offset;
      /* actual data */
      render_data_line (win, data, y, j, gscroll);
    } else {
      /* everything else should be empty */
      (*y)++;
    }
    (*total)++;
    if (*y >= h)
      break;
  }
}

/* Entry point to render the terminal dashboard. */
void
display_content (WINDOW *win, GDash *dash, GScroll *gscroll) {
  GModule module;
  int j = 0;
  size_t idx = 0;

  int y = 0, offset = 0, total = 0;
  int dash_scroll = gscroll->dash;

  werase (win);

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    offset = 0;
    for (j = 0; j < dash->module[module].dash_size; j++) {
      if (dash_scroll > total) {
        offset++;
        total++;
      }
    }
    /* used module */
    dash->module[module].module = module;

    render_content (win, &dash->module[module], &y, &offset, &total, gscroll);
  }
  wrefresh (win);
}

/* Reset the scroll and offset fields for each panel/module. */
void
reset_scroll_offsets (GScroll *gscroll) {
  GModule module;
  size_t idx = 0;

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    gscroll->module[module].scroll = 0;
    gscroll->module[module].offset = 0;
  }
}

/* Compile the regular expression and see if it's valid.
 *
 * If unable to compile, an error as described in <regex.h>.
 * Upon successful completion, function returns 0. */
static int
regexp_init (regex_t *regex, const char *pattern) {
  int y, x, rc;
  char buf[REGEX_ERROR];

  getmaxyx (stdscr, y, x);
  rc = regcomp (regex, pattern, REG_EXTENDED | (find_t.icase ? REG_ICASE : 0));
  /* something went wrong */
  if (rc != 0) {
    regerror (rc, regex, buf, sizeof (buf));
    draw_header (stdscr, buf, "%s", y - 1, 0, x, color_error);
    refresh ();
    return 1;
  }
  return 0;
}

/* Set the dashboard scroll and offset based on the search index. */
static void
perform_find_dash_scroll (GScroll *gscroll, GModule module) {
  int *scrll, *offset;
  int exp_size = get_num_expanded_data_rows ();

  /* reset gscroll offsets if we are changing module */
  if (gscroll->current != module)
    reset_scroll_offsets (gscroll);

  scrll = &gscroll->module[module].scroll;
  offset = &gscroll->module[module].offset;

  (*scrll) = find_t.next_idx;
  if (*scrll >= exp_size && *scrll >= *offset + exp_size)
    (*offset) = (*scrll) < exp_size - 1 ? 0 : (*scrll) - exp_size + 1;

  gscroll->current = module;
  gscroll->dash = get_module_index (module) * DASH_COLLAPSED;
  gscroll->expanded = 1;
  find_t.module = module;
}

/* Find the searched item within the given sub list.
 *
 * If not found, the GFind structure is reset and 1 is returned.
 * If found, a GFind structure is set and 0 is returned. */
static int
find_next_sub_item (GSubList *sub_list, regex_t *regex) {
  GSubItem *iter;
  int i = 0, rc;

  if (sub_list == NULL)
    goto out;

  for (iter = sub_list->head; iter; iter = iter->next) {
    if (i >= find_t.next_sub_idx) {
      rc = regexec (regex, iter->metrics->data, 0, NULL, 0);
      if (rc == 0) {
        find_t.next_idx++;
        find_t.next_sub_idx = (1 + i);
        return 0;
      }
      find_t.next_idx++;
    }
    i++;
  }
out:
  find_t.next_parent_idx++;
  find_t.next_sub_idx = 0;
  find_t.look_in_sub = 0;

  return 1;
}

/* Perform a forward search across all modules.
 *
 * On error or if not found, 1 is returned.
 * On success or if found, a GFind structure is set and 0 is returned. */
int
perform_next_find (GHolder *h, GScroll *gscroll) {
  GModule module;
  GSubList *sub_list;
  regex_t regex;
  char buf[REGEX_ERROR], *data;
  int y, x, j, n, rc;
  size_t idx = 0;

  getmaxyx (stdscr, y, x);

  if (find_t.pattern == NULL || *find_t.pattern == '\0')
    return 1;

  /* compile and initialize regexp */
  if (regexp_init (&regex, find_t.pattern))
    return 1;

  /* use last find_t.module and start search */
  idx = find_t.module;
  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];

    n = h[module].idx;
    for (j = find_t.next_parent_idx; j < n; j++, find_t.next_idx++) {
      data = h[module].items[j].metrics->data;

      rc = regexec (&regex, data, 0, NULL, 0);
      /* error matching against the precompiled pattern buffer */
      if (rc != 0 && rc != REG_NOMATCH) {
        regerror (rc, &regex, buf, sizeof (buf));
        draw_header (stdscr, buf, "%s", y - 1, 0, x, color_error);
        refresh ();
        regfree (&regex);
        return 1;
      }
      /* a match was found (data level) */
      else if (rc == 0 && !find_t.look_in_sub) {
        find_t.look_in_sub = 1;
        perform_find_dash_scroll (gscroll, module);
        goto out;
      }
      /* look at sub list nodes */
      else {
        sub_list = h[module].items[j].sub_list;
        if (find_next_sub_item (sub_list, &regex) == 0) {
          perform_find_dash_scroll (gscroll, module);
          goto out;
        }
      }
    }

    /* reset find */
    find_t.next_idx = 0;
    find_t.next_parent_idx = 0;
    find_t.next_sub_idx = 0;

    if (find_t.module != module) {
      reset_scroll_offsets (gscroll);
      gscroll->expanded = 0;
    }
    if (module == TOTAL_MODULES - 1) {
      find_t.module = 0;
      goto out;
    }
  }

out:
  regfree (&regex);
  return 0;
}

/* Render a find dialog.
 *
 * On error or if no query is set, 1 is returned.
 * On success, the dialog is rendered and 0 is returned. */
int
render_find_dialog (WINDOW *main_win, GScroll *gscroll) {
  int y, x, valid = 1;
  int w = FIND_DLG_WIDTH;
  int h = FIND_DLG_HEIGHT;
  char *query = NULL;
  WINDOW *win;

  getmaxyx (stdscr, y, x);

  win = newwin (h, w, (y - h) / 2, (x - w) / 2);
  keypad (win, TRUE);
  wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');
  draw_header (win, FIND_HEAD, " %s", 1, 1, w - 2, color_panel_header);
  mvwprintw (win, 2, 1, " %s", FIND_DESC);

  find_t.icase = 0;
  query = input_string (win, 4, 2, w - 3, "", 1, &find_t.icase);
  if (query != NULL && *query != '\0') {
    reset_scroll_offsets (gscroll);
    reset_find ();
    find_t.pattern = xstrdup (query);
    valid = 0;
  }
  if (query != NULL)
    free (query);
  touchwin (main_win);
  close_win (win);
  wrefresh (main_win);

  return valid;
}

static void
set_dash_metrics (GDash **dash, GMetrics *metrics, GModule module,
                  GPercTotals totals, int is_subitem) {
  GDashData *idata = NULL;
  GDashMeta *meta = NULL;
  char *data = NULL;
  int *idx;

  data = is_subitem ? render_child_node (metrics->data) : metrics->data;
  if (!data)
    return;

  idx = &(*dash)->module[module].idx_data;
  idata = &(*dash)->module[module].data[(*idx)];
  meta = &(*dash)->module[module].meta;

  idata->metrics = new_gmetrics ();
  idata->is_subitem = is_subitem;

  idata->metrics->hits = metrics->hits;
  idata->metrics->hits_perc = get_percentage (totals.hits, metrics->hits);
  idata->metrics->visitors = metrics->visitors;
  idata->metrics->visitors_perc = get_percentage (totals.visitors, metrics->visitors);
  idata->metrics->bw.sbw = filesize_str (metrics->bw.nbw);
  idata->metrics->data = xstrdup (data);

  if (conf.append_method && metrics->method)
    idata->metrics->method = metrics->method;
  if (conf.append_protocol && metrics->protocol)
    idata->metrics->protocol = metrics->protocol;

  if (!conf.serve_usecs)
    goto out;

  idata->metrics->avgts.sts = usecs_to_str (metrics->avgts.nts);
  idata->metrics->cumts.sts = usecs_to_str (metrics->cumts.nts);
  idata->metrics->maxts.sts = usecs_to_str (metrics->maxts.nts);

out:
  if (is_subitem)
    free (data);

  set_metrics_len (meta, idata);
  set_max_metrics (meta, idata);

  (*idx)++;
}

/* Add an item from a sub list to the dashboard.
 *
 * If no items on the sub list, the function returns.
 * On success, sub list data is set into the dashboard structure. */
static void
add_sub_item_to_dash (GDash **dash, GHolderItem item, GModule module, GPercTotals totals,
                      int *i) {
  GSubList *sub_list = item.sub_list;
  GSubItem *iter;

  if (sub_list == NULL)
    return;

  for (iter = sub_list->head; iter; iter = iter->next, (*i)++) {
    set_dash_metrics (dash, iter->metrics, module, totals, 1);
  }
}

/* Add a first level item to dashboard.
 *
 * On success, data is set into the dashboard structure. */
static void
add_item_to_dash (GDash **dash, GHolderItem item, GModule module, GPercTotals totals) {
  set_dash_metrics (dash, item.metrics, module, totals, 0);
}

/* Load holder's data into the dashboard structure. */
void
load_data_to_dash (GHolder *h, GDash *dash, GModule module, GScroll *gscroll) {
  int alloc_size = 0;
  int i, j;
  GPercTotals totals;

  alloc_size = dash->module[module].alloc_data;
  if (gscroll->expanded && module == gscroll->current)
    alloc_size += h->sub_items_size;

  dash->module[module].alloc_data = alloc_size;
  dash->module[module].data = new_gdata (alloc_size);
  dash->module[module].holder_size = h->holder_size;
  memset (&dash->module[module].meta, 0, sizeof (GDashData));

  set_module_totals (&totals);

  for (i = 0, j = 0; i < alloc_size; i++) {
    if (h->items[j].metrics->data == NULL)
      continue;

    add_item_to_dash (&dash, h->items[j], module, totals);
    if (gscroll->expanded && module == gscroll->current && h->sub_items_size)
      add_sub_item_to_dash (&dash, h->items[j], module, totals, &i);
    j++;
  }
}
