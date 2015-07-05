/**
 * gmenu.c -- goaccess main dashboard
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

#define _XOPEN_SOURCE 700

#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <regex.h>

#include "gdashboard.h"

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabdb.h"
#else
#include "glibht.h"
#endif

#ifdef HAVE_LIBGEOIP
#include "geolocation.h"
#endif

#include "error.h"
#include "gdns.h"
#include "settings.h"
#include "util.h"
#include "xmalloc.h"

static void add_data_to_holder (GRawDataItem item, GHolder * h,
                                const GPanel * panel);
static void add_host_to_holder (GRawDataItem item, GHolder * h,
                                const GPanel * panel);
static void add_root_to_holder (GRawDataItem item, GHolder * h,
                                const GPanel * panel);
static void add_host_child_to_holder (GHolder * h);
static void data_visitors (GHolder * h);

static GFind find_t;

/* *INDENT-OFF* */
/* module's styles */
static const GDashStyle module_style[TOTAL_MODULES] = {
  {COL_BLACK , COL_WHITE , COL_WHITE , COL_BLACK , COL_RED   , COL_WHITE , COL_BLACK , -1        , -1}        ,
  {COL_BLACK , COL_WHITE , COL_WHITE , COL_BLACK , COL_BLACK , -1        , COL_BLACK , COL_WHITE , COL_BLACK} ,
  {COL_BLACK , COL_WHITE , COL_WHITE , COL_BLACK , COL_BLACK , -1        , COL_BLACK , COL_WHITE , COL_BLACK} ,
  {COL_BLACK , COL_WHITE , COL_WHITE , COL_BLACK , COL_BLACK , -1        , COL_BLACK , COL_WHITE , COL_BLACK} ,
  {COL_BLACK , COL_WHITE , COL_WHITE , COL_BLACK , COL_BLACK , COL_WHITE , COL_BLACK , -1        , -1}        ,
  {COL_BLACK , COL_WHITE , COL_WHITE , COL_BLACK , COL_RED   , COL_WHITE , COL_BLACK , COL_BLACK , COL_WHITE} ,
  {COL_BLACK , COL_WHITE , COL_WHITE , COL_BLACK , COL_RED   , COL_WHITE , COL_BLACK , -1        , -1}        ,
  {COL_BLACK , COL_WHITE , COL_WHITE , COL_BLACK , COL_RED   , COL_WHITE , COL_BLACK , -1        , -1}        ,
  {COL_BLACK , COL_WHITE , COL_WHITE , COL_BLACK , COL_BLACK , -1        , COL_BLACK , -1        , -1}        ,
  {COL_BLACK , COL_WHITE , COL_WHITE , COL_BLACK , COL_BLACK , -1        , COL_BLACK , -1        , -1}        ,
  {COL_BLACK , COL_WHITE , COL_WHITE , COL_BLACK , COL_BLACK , -1        , COL_BLACK , -1        , -1}        ,
#ifdef HAVE_LIBGEOIP
  {COL_BLACK , COL_WHITE , COL_WHITE , COL_BLACK , COL_BLACK , -1        , COL_BLACK , -1        , -1}        ,
#endif
  {COL_BLACK , COL_WHITE , COL_WHITE , COL_BLACK , COL_BLACK , -1        , COL_BLACK , -1        , -1}        ,
};

static GPanel paneling[] = {
  {VISITORS        , add_data_to_holder, data_visitors} ,
  {REQUESTS        , add_data_to_holder, NULL} ,
  {REQUESTS_STATIC , add_data_to_holder, NULL} ,
  {NOT_FOUND       , add_data_to_holder, NULL} ,
  {HOSTS           , add_host_to_holder, add_host_child_to_holder} ,
  {OS              , add_root_to_holder, NULL} ,
  {BROWSERS        , add_root_to_holder, NULL} ,
  {VISIT_TIMES     , add_data_to_holder, NULL} ,
  {REFERRERS       , add_data_to_holder, NULL} ,
  {REFERRING_SITES , add_data_to_holder, NULL} ,
  {KEYPHRASES      , add_data_to_holder, NULL} ,
#ifdef HAVE_LIBGEOIP
  {GEO_LOCATION    , add_root_to_holder, NULL} ,
#endif
  {STATUS_CODES    , add_root_to_holder, NULL} ,
  {VISIT_TIMES     , add_data_to_holder, NULL} ,
};

/* *INDENT-ON* */

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

/* reset find indices */
void
reset_find (void)
{
  if (find_t.pattern != NULL && *find_t.pattern != '\0')
    free (find_t.pattern);

  find_t.look_in_sub = 0;
  find_t.module = 0;
  find_t.next_idx = 0;  /* next total index    */
  find_t.next_parent_idx = 0;   /* next parent index   */
  find_t.next_sub_idx = 0;      /* next sub item index */
  find_t.pattern = NULL;
}

/* allocate memory for dash */
GDash *
new_gdash (void)
{
  GDash *dash = xmalloc (sizeof (GDash));
  memset (dash, 0, sizeof *dash);
  dash->total_alloc = 0;

  return dash;
}

/* allocate memory for dash elements */
GDashData *
new_gdata (uint32_t size)
{
  GDashData *data = xcalloc (size, sizeof (GDashData));

  return data;
}

static void
free_dashboard_data (GDashData item)
{
  if (item.metrics == NULL)
    return;

  if (item.metrics->data)
    free (item.metrics->data);
  if (item.metrics->bw.sbw)
    free (item.metrics->bw.sbw);
  if (conf.serve_usecs && item.metrics->avgts.sts)
    free (item.metrics->avgts.sts);
  free (item.metrics);
}

/* free dash and its elements */
void
free_dashboard (GDash * dash)
{
  int i, j;
  for (i = 0; i < TOTAL_MODULES; i++) {
    for (j = 0; j < dash->module[i].alloc_data; j++) {
      free_dashboard_data (dash->module[i].data[j]);
    }
    free (dash->module[i].data);
  }
  free (dash);
}

/* allocate memory for holder */
GHolder *
new_gholder (uint32_t size)
{
  GHolder *holder = xmalloc (size * sizeof (GHolder));
  memset (holder, 0, size * sizeof *holder);

  return holder;
}

/* allocate memory for holder items */
static GHolderItem *
new_gholder_item (uint32_t size)
{
  GHolderItem *item = xcalloc (size, sizeof (GHolderItem));

  return item;
}

/* allocate memory for a sub list */
static GSubList *
new_gsublist (void)
{
  GSubList *sub_list = xmalloc (sizeof (GSubList));
  sub_list->head = NULL;
  sub_list->tail = NULL;
  sub_list->size = 0;

  return sub_list;
}

/* allocate memory for a sub list item */
static GSubItem *
new_gsubitem (GModule module, GMetrics * nmetrics)
{
  GSubItem *sub_item = xmalloc (sizeof (GSubItem));

  sub_item->metrics = nmetrics;
  sub_item->module = module;
  sub_item->prev = NULL;
  sub_item->next = NULL;

  return sub_item;
}

/* add an item to the end of a given sub list */
static void
add_sub_item_back (GSubList * sub_list, GModule module, GMetrics * nmetrics)
{
  GSubItem *sub_item = new_gsubitem (module, nmetrics);
  if (sub_list->tail) {
    sub_list->tail->next = sub_item;
    sub_item->prev = sub_list->tail;
    sub_list->tail = sub_item;
  } else {
    sub_list->head = sub_item;
    sub_list->tail = sub_item;
  }
  sub_list->size++;
}

/* delete entire given sub list */
static void
delete_sub_list (GSubList * sub_list)
{
  GSubItem *item = item;
  GSubItem *next = next;

  if (sub_list != NULL && sub_list->size == 0)
    goto clear;
  if (sub_list->size == 0)
    return;

  for (item = sub_list->head; item; item = next) {
    next = item->next;
    free (item->metrics->data);
    free (item->metrics);
    free (item);
  }
clear:
  sub_list->head = NULL;
  sub_list->size = 0;
  free (sub_list);
}

/* dynamically allocated holder fields */
static void
free_holder_data (GHolderItem item)
{
  if (item.sub_list != NULL)
    delete_sub_list (item.sub_list);
  if (item.metrics->data != NULL)
    free (item.metrics->data);
  if (item.metrics->method != NULL)
    free (item.metrics->method);
  if (item.metrics->protocol != NULL)
    free (item.metrics->protocol);
  if (item.metrics != NULL)
    free (item.metrics);
}

/* free memory allocated in holder for specific module */
void
free_holder_by_module (GHolder ** holder, GModule module)
{
  int j;

  if ((*holder) == NULL)
    return;

  for (j = 0; j < (*holder)[module].idx; j++) {
    free_holder_data ((*holder)[module].items[j]);
  }
  free ((*holder)[module].items);

  (*holder)[module].holder_size = 0;
  (*holder)[module].idx = 0;
  (*holder)[module].sub_items_size = 0;
}

/* free memory allocated in holder */
void
free_holder (GHolder ** holder)
{
  GModule module;
  int j;

  if ((*holder) == NULL)
    return;

  for (module = 0; module < TOTAL_MODULES; module++) {
    for (j = 0; j < (*holder)[module].idx; j++) {
      free_holder_data ((*holder)[module].items[j]);
    }
    free ((*holder)[module].items);
  }
  free (*holder);
  (*holder) = NULL;
}

static GModule
get_find_current_module (GDash * dash, int offset)
{
  GModule module;

  for (module = 0; module < TOTAL_MODULES; module++) {
    /* set current module */
    if (dash->module[module].pos_y == offset)
      return module;
    /* we went over by one module, set current - 1 */
    if (dash->module[module].pos_y > offset)
      return module - 1;
  }

  return 0;
}

/**
 * Determine which module should be expanded given the
 * current mouse position.
 */
int
set_module_from_mouse_event (GScroll * gscroll, GDash * dash, int y)
{
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

/* render child nodes */
static char *
render_child_node (const char *data)
{
  char *buf;
  int len = 0;

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

/* get a string of bars given current hits, maximum hit & xpos */
static char *
get_bars (int n, int max, int x)
{
  int w, h, len;

  getmaxyx (stdscr, h, w);
  (void) h;     /* avoid lint warning */

  if ((len = (n * (w - x) / max)) < 1)
    len = 1;
  return char_repeat (len, '|');
}

/*get largest method's length */
static int
get_max_method_len (GDashData * data, int size)
{
  int i, max = 0, len;
  for (i = 0; i < size; i++) {
    if (data[i].metrics->method == NULL)
      continue;
    len = strlen (data[i].metrics->method);
    if (len > max)
      max = len;
  }
  return max;
}

/*get largest data's length */
static int
get_max_data_len (GDashData * data, int size)
{
  int i, max = 0, len;
  for (i = 0; i < size; i++) {
    if (data[i].metrics->data == NULL)
      continue;
    len = strlen (data[i].metrics->data);
    if (len > max)
      max = len;
  }
  return max;
}

/*get largest hit's length */
static int
get_max_visitor_len (GDashData * data, int size)
{
  int i, max = 0;
  for (i = 0; i < size; i++) {
    int len = intlen (data[i].metrics->visitors);
    if (len > max)
      max = len;
  }
  return max;
}

/*get largest hit's length */
static int
get_max_hit_len (GDashData * data, int size)
{
  int i, max = 0;
  for (i = 0; i < size; i++) {
    int len = intlen (data[i].metrics->hits);
    if (len > max)
      max = len;
  }
  return max;
}

/* get largest hit */
static int
get_max_hit (GDashData * data, int size)
{
  int i, max = 0;
  for (i = 0; i < size; i++) {
    int cur = 0;
    if ((cur = data[i].metrics->hits) > max)
      max = cur;
  }
  return max;
}

/* set item's percent in GDashData */
static float
set_percent_data (GDashData * data, int n, int process)
{
  float max = 0.0;
  int i;
  for (i = 0; i < n; i++) {
    data[i].metrics->percent = get_percentage (process, data[i].metrics->hits);
    if (data[i].metrics->percent > max)
      max = data[i].metrics->percent;
  }
  return max;
}

/* render module's total */
static void
render_total_label (WINDOW * win, GDashModule * data, int y, int color)
{
  char *s;
  int win_h, win_w, total, ht_size;

  total = data->holder_size;
  ht_size = data->ht_size;

  s = xmalloc (snprintf (NULL, 0, "Total: %d/%d", total, ht_size) + 1);
  getmaxyx (win, win_h, win_w);
  (void) win_h;

  sprintf (s, "Total: %d/%d", total, ht_size);
  draw_header (win, s, "%s", y, win_w - strlen (s) - 2, win_w, color, 0);
  free (s);
}

/* render dashboard bars (graph) */
static void
render_bars (GDashModule * data, GDashRender render, int *x)
{
  WINDOW *win = render.win;
  GModule module = data->module;
  const GDashStyle *style = module_style;

  char *bar;
  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;

  if (style[module].color_bars == -1)
    return;

  bar = get_bars (data->data[idx].metrics->hits, data->max_hits, *x);
  if (sel)
    draw_header (win, bar, "%s", y, *x, w, HIGHLIGHT, 0);
  else
    mvwprintw (win, y, *x, "%s", bar);
  free (bar);
}

/* render dashboard data */
static void
render_data (GDashModule * data, GDashRender render, int *x)
{
  WINDOW *win = render.win;
  GModule module = data->module;
  const GDashStyle *style = module_style;

  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;

  char buf[DATE_LEN];
  char *value, *padded_data;

  value = substring (data->data[idx].metrics->data, 0, w - *x);
  if (module == VISITORS) {
    /* verify we have a valid date conversion */
    if (convert_date (buf, value, "%Y%m%d", "%d/%b/%Y", DATE_LEN) != 0) {
      LOG_DEBUG (("invalid date: %s", value));
      xstrncpy (buf, "---", 4);
    }
  }

  if (sel) {
    if (data->module == HOSTS && data->data[idx].is_subitem) {
      padded_data = left_pad_str (value, *x);
      draw_header (win, padded_data, "%s", y, 0, w, HIGHLIGHT, 0);
      free (padded_data);
    } else {
      draw_header (win, module == VISITORS ? buf : value, "%s", y, *x, w,
                   HIGHLIGHT, 0);
    }
  } else {
    wattron (win, COLOR_PAIR (style[module].color_data));
    mvwprintw (win, y, *x, "%s", module == VISITORS ? buf : value);
    wattroff (win, COLOR_PAIR (style[module].color_data));
  }

  *x += module == VISITORS ? DATE_LEN - 1 : data->data_len;
  *x += DASH_SPACE;
  free (value);
}

/* render dashboard request method */
static void
render_method (GDashModule * data, GDashRender render, int *x)
{
  WINDOW *win = render.win;
  GModule module = data->module;
  const GDashStyle *style = module_style;

  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;
  char *method = data->data[idx].metrics->method;

  if (style[module].color_method == -1)
    return;

  if (method == NULL || *method == '\0')
    return;

  /* selected state */
  if (sel) {
    draw_header (win, method, "%s", y, *x, w, HIGHLIGHT, 0);
  }
  /* regular state */
  else {
    wattron (win, A_BOLD | COLOR_PAIR (style[module].color_method));
    mvwprintw (win, y, *x, "%s", method);
    wattroff (win, A_BOLD | COLOR_PAIR (style[module].color_method));
  }

  *x += data->method_len + DASH_SPACE;
}

/* render dashboard request protocol */
static void
render_protocol (GDashModule * data, GDashRender render, int *x)
{
  WINDOW *win = render.win;
  GModule module = data->module;
  const GDashStyle *style = module_style;

  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;
  char *protocol = data->data[idx].metrics->protocol;

  if (style[module].color_protocol == -1)
    return;

  if (protocol == NULL || *protocol == '\0')
    return;

  /* selected state */
  if (sel) {
    draw_header (win, protocol, "%s", y, *x, w, HIGHLIGHT, 0);
  }
  /* regular state */
  else {
    wattron (win, A_BOLD | COLOR_PAIR (style[module].color_protocol));
    mvwprintw (win, y, *x, "%s", protocol);
    wattroff (win, A_BOLD | COLOR_PAIR (style[module].color_protocol));
  }

  *x += REQ_PROTO_LEN - 1 + DASH_SPACE;
}

/* render dashboard usecs */
static void
render_usecs (GDashModule * data, GDashRender render, int *x)
{
  WINDOW *win = render.win;
  GModule module = data->module;
  const GDashStyle *style = module_style;

  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;
  char *avgts = data->data[idx].metrics->avgts.sts;

  if (data->module == HOSTS && data->data[idx].is_subitem)
    goto out;
  if (style[module].color_usecs == -1)
    return;

  /* selected state */
  if (sel) {
    draw_header (win, avgts, "%9s", y, *x, w, HIGHLIGHT, 0);
  }
  /* regular state */
  else {
    wattron (win, A_BOLD | COLOR_PAIR (style[module].color_usecs));
    mvwprintw (win, y, *x, "%9s", avgts);
    wattroff (win, A_BOLD | COLOR_PAIR (style[module].color_usecs));
  }
out:

  *x += DASH_SRV_TM_LEN + DASH_SPACE;
}

/* render dashboard bandwidth */
static void
render_bandwidth (GDashModule * data, GDashRender render, int *x)
{
  WINDOW *win = render.win;
  GModule module = data->module;
  const GDashStyle *style = module_style;

  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;
  char *bw = data->data[idx].metrics->bw.sbw;

  if (data->module == HOSTS && data->data[idx].is_subitem)
    goto out;
  if (style[module].color_bw == -1)
    return;

  /* selected state */
  if (sel) {
    draw_header (win, bw, "%11s", y, *x, w, HIGHLIGHT, 0);
  }
  /* regular state */
  else {
    wattron (win, A_BOLD | COLOR_PAIR (style[module].color_bw));
    mvwprintw (win, y, *x, "%11s", bw);
    wattroff (win, A_BOLD | COLOR_PAIR (style[module].color_bw));
  }
out:

  *x += DASH_BW_LEN + DASH_SPACE;
}

/* render dashboard percent */
static void
render_percent (GDashModule * data, GDashRender render, int *x)
{
  WINDOW *win = render.win;
  GModule module = data->module;
  const GDashStyle *style = module_style;

  char *percent;
  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;
  int len = data->perc_len + 3;

  if (data->module == HOSTS && data->data[idx].is_subitem)
    goto out;
  if (style[module].color_percent == -1)
    return;

  /* selected state */
  if (sel) {
    percent = float_to_str (data->data[idx].metrics->percent);
    draw_header (win, percent, "%*s%%", y, *x, w, HIGHLIGHT, len);
    free (percent);
  }
  /* regular state */
  else {
    wattron (win, A_BOLD | COLOR_PAIR (style[module].color_percent));
    if (data->max_hits == data->data[idx].metrics->hits)
      wattron (win, A_BOLD | COLOR_PAIR (COL_YELLOW));
    if (style[module].color_percent == COL_BLACK)
      wattron (win, A_BOLD | COLOR_PAIR (style[module].color_percent));

    mvwprintw (win, y, *x, "%*.2f%%", len, data->data[idx].metrics->percent);

    if (style[module].color_percent == COL_BLACK)
      wattroff (win, A_BOLD | COLOR_PAIR (style[module].color_percent));
    if (data->max_hits == data->data[idx].metrics->hits)
      wattroff (win, A_BOLD | COLOR_PAIR (COL_YELLOW));
    wattroff (win, A_BOLD | COLOR_PAIR (style[module].color_percent));
  }
out:

  *x += len + 1 + DASH_SPACE;
}

/* render dashboard hits */
static void
render_hits (GDashModule * data, GDashRender render, int *x)
{
  WINDOW *win = render.win;
  GModule module = data->module;
  const GDashStyle *style = module_style;

  char *hits;
  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;
  int len = data->hits_len;

  if (data->module == HOSTS && data->data[idx].is_subitem)
    goto out;

  /* selected state */
  if (sel) {
    hits = int_to_str (data->data[idx].metrics->hits);
    draw_header (win, hits, "  %s", y, 0, w, HIGHLIGHT, 0);
    free (hits);
  }
  /* regular state */
  else {
    wattron (win, COLOR_PAIR (style[module].color_hits));
    mvwprintw (win, y, *x, "%d", data->data[idx].metrics->hits);
    wattroff (win, COLOR_PAIR (style[module].color_hits));
  }
out:

  *x += len + DASH_SPACE;
}

/* render dashboard hits */
static void
render_visitors (GDashModule * data, GDashRender render, int *x)
{
  WINDOW *win = render.win;
  GModule module = data->module;
  const GDashStyle *style = module_style;

  char *visitors;
  int y = render.y, w = render.w, idx = render.idx, sel = render.sel;
  int len = data->visitors_len;

  if (data->module == HOSTS && data->data[idx].is_subitem)
    goto out;

  /* selected state */
  if (sel) {
    visitors = int_to_str (data->data[idx].metrics->visitors);
    draw_header (win, visitors, "%*s", y, *x, w, HIGHLIGHT, len);
    free (visitors);
  }
  /* regular state */
  else {
    wattron (win, A_BOLD | COLOR_PAIR (style[module].color_visitors));
    mvwprintw (win, y, *x, "%*d", len, data->data[idx].metrics->visitors);
    wattroff (win, A_BOLD | COLOR_PAIR (style[module].color_visitors));
  }
out:

  *x += len + DASH_SPACE;
}

static void
render_header (WINDOW * win, GDashModule * data, GModule cur_module, int *y)
{
  char ind;
  char *hd;
  int k, w, h, color;

  getmaxyx (win, h, w);
  (void) h;

  k = data->module + 1;
  ind = cur_module == data->module ? '>' : ' ';
  color = cur_module == data->module &&
    conf.hl_header ? YELLOW_BLACK : HIGHLIGHT;
  hd = xmalloc (snprintf (NULL, 0, "%c %d - %s", ind, k, data->head) + 1);
  sprintf (hd, "%c %d - %s", ind, k, data->head);

  draw_header (win, hd, " %s", (*y), 0, w, color, 0);
  free (hd);

  render_total_label (win, data, (*y), color);
  data->pos_y = (*y);
  (*y)++;
}

static void
render_data_line (WINDOW * win, GDashModule * data, int *y, int j,
                  GScroll * gscroll)
{
  GDashRender render;
  GModule module = data->module;
  int expanded = 0, sel = 0, host_bars = 0;
  int x = 0, w, h;

  if (!conf.skip_term_resolver)
    host_bars = 1;

#ifdef HAVE_LIBGEOIP
  host_bars = 1;
#endif

  getmaxyx (win, h, w);
  (void) h;

  if (gscroll->expanded && module == gscroll->current)
    expanded = 1;

  x = DASH_INIT_X;

  if (j >= data->idx_data)
    goto out;

  sel = expanded && j == gscroll->module[module].scroll ? 1 : 0;

  render.win = win;
  render.y = *y;
  render.w = w;
  render.idx = j;
  render.sel = sel;

  render_hits (data, render, &x);
  render_visitors (data, render, &x);
  render_percent (data, render, &x);
  render_bandwidth (data, render, &x);

  /* render usecs if available */
  if (conf.serve_usecs)
    render_usecs (data, render, &x);
  /* render request method if available */
  if (conf.append_method)
    render_method (data, render, &x);
  /* render request protocol if available */
  if (conf.append_protocol)
    render_protocol (data, render, &x);
  render_data (data, render, &x);

  /* skip graph bars if module is expanded and we have sub nodes */
  if (module == HOSTS && expanded && host_bars);
  else
    render_bars (data, render, &x);

out:
  (*y)++;
}

/* render dashboard content */
static void
render_content (WINDOW * win, GDashModule * data, int *y, int *offset,
                int *total, GScroll * gscroll)
{
  GModule module = data->module;
  int i, j, size, h, w;

  getmaxyx (win, h, w);

  size = data->dash_size;
  for (i = *offset, j = 0; i < size; i++) {
    /* header */
    if ((i % size) == DASH_HEAD_POS) {
      render_header (win, data, gscroll->current, y);
    } else if ((i % size) == DASH_DESC_POS) {
      /* description */
      draw_header (win, data->desc, " %s", (*y)++, 0, w, 2, 0);
    } else if ((i % size) == DASH_EMPTY_POS || (i % size) == size - 1) {
      /* blank lines */
      (*y)++;
    } else if ((i % size) >= DASH_DATA_POS || (i % size) <= size - 2) {
      /* account for 2 lines at the header and 2 blank lines */
      j = ((i % size) - DASH_DATA_POS) + gscroll->module[module].offset;
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

/* entry point to render dashboard */
void
display_content (WINDOW * win, GLog * logger, GDash * dash, GScroll * gscroll)
{
  GDashData *idata;
  GModule module;
  float max_percent = 0.0;
  int j, n = 0, process = 0;

  int y = 0, offset = 0, total = 0;
  int dash_scroll = gscroll->dash;

  werase (win);

  for (module = 0; module < TOTAL_MODULES; module++) {
    n = dash->module[module].idx_data;
    offset = 0;
    for (j = 0; j < dash->module[module].dash_size; j++) {
      if (dash_scroll > total) {
        offset++;
        total++;
      }
    }

    idata = dash->module[module].data;
    process = logger->process;
    max_percent = set_percent_data (idata, n, process);

    dash->module[module].module = module;
    dash->module[module].method_len = get_max_method_len (idata, n);
    dash->module[module].data_len = get_max_data_len (idata, n);
    dash->module[module].hits_len = get_max_hit_len (idata, n);
    dash->module[module].max_hits = get_max_hit (idata, n);
    dash->module[module].perc_len = intlen ((int) max_percent);
    dash->module[module].visitors_len = get_max_visitor_len (idata, n);

    render_content (win, &dash->module[module], &y, &offset, &total, gscroll);
  }
  wrefresh (win);
}

/* reset scroll and offset for each module */
void
reset_scroll_offsets (GScroll * gscroll)
{
  int i;
  for (i = 0; i < TOTAL_MODULES; i++) {
    gscroll->module[i].scroll = 0;
    gscroll->module[i].offset = 0;
  }
}

/* compile the regular expression and see if it's valid */
static int
regexp_init (regex_t * regex, const char *pattern)
{
  int y, x, rc;
  char buf[REGEX_ERROR];

  getmaxyx (stdscr, y, x);
  rc = regcomp (regex, pattern, REG_EXTENDED | (find_t.icase ? REG_ICASE : 0));
  /* something went wrong */
  if (rc != 0) {
    regerror (rc, regex, buf, sizeof (buf));
    draw_header (stdscr, buf, "%s", y - 1, 0, x, WHITE_RED, 0);
    refresh ();
    return 1;
  }
  return 0;
}

/* set search gscroll */
static void
perform_find_dash_scroll (GScroll * gscroll, GModule module)
{
  int *scrll, *offset;
  int exp_size = DASH_EXPANDED - DASH_NON_DATA;

  /* reset gscroll offsets if we are changing module */
  if (gscroll->current != module)
    reset_scroll_offsets (gscroll);

  scrll = &gscroll->module[module].scroll;
  offset = &gscroll->module[module].offset;

  (*scrll) = find_t.next_idx;
  if (*scrll >= exp_size && *scrll >= *offset + exp_size)
    (*offset) = (*scrll) < exp_size - 1 ? 0 : (*scrll) - exp_size + 1;

  gscroll->current = module;
  gscroll->dash = module * DASH_COLLAPSED;
  gscroll->expanded = 1;
  find_t.module = module;
}

/* find item within the given sub_list */
static int
find_next_sub_item (GSubList * sub_list, regex_t * regex)
{
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

/* perform a forward search across all modules */
int
perform_next_find (GHolder * h, GScroll * gscroll)
{
  int y, x, j, n, rc;
  char buf[REGEX_ERROR];
  char *data;
  regex_t regex;
  GModule module;
  GSubList *sub_list;

  getmaxyx (stdscr, y, x);

  if (find_t.pattern == NULL || *find_t.pattern == '\0')
    return 1;

  /* compile and initialize regexp */
  if (regexp_init (&regex, find_t.pattern))
    return 1;

  /* use last find_t.module and start search */
  for (module = find_t.module; module < TOTAL_MODULES; module++) {
    n = h[module].idx;
    for (j = find_t.next_parent_idx; j < n; j++, find_t.next_idx++) {
      data = h[module].items[j].metrics->data;

      rc = regexec (&regex, data, 0, NULL, 0);
      if (rc != 0 && rc != REG_NOMATCH) {
        regerror (rc, &regex, buf, sizeof (buf));
        draw_header (stdscr, buf, "%s", y - 1, 0, x, WHITE_RED, 0);
        refresh ();
        regfree (&regex);
        return 1;
      } else if (rc == 0 && !find_t.look_in_sub) {
        find_t.look_in_sub = 1;
        goto found;
      } else {
        sub_list = h[module].items[j].sub_list;
        if (find_next_sub_item (sub_list, &regex) == 0)
          goto found;
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

found:
  perform_find_dash_scroll (gscroll, module);
out:
  regfree (&regex);
  return 0;
}

/* render find dialog */
int
render_find_dialog (WINDOW * main_win, GScroll * gscroll)
{
  int y, x, valid = 1;
  int w = FIND_DLG_WIDTH;
  int h = FIND_DLG_HEIGHT;
  char *query = NULL;
  WINDOW *win;

  getmaxyx (stdscr, y, x);

  win = newwin (h, w, (y - h) / 2, (x - w) / 2);
  keypad (win, TRUE);
  wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');
  draw_header (win, FIND_HEAD, " %s", 1, 1, w - 2, 1, 0);
  draw_header (win, FIND_DESC, " %s", 2, 1, w - 2, 2, 0);

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

/* iterate over holder and get the key index.
 * return -1 if not found */
static int
get_item_idx_in_holder (GHolder * holder, const char *k)
{
  int i;
  if (holder == NULL)
    return KEY_NOT_FOUND;
  if (holder->idx == 0)
    return KEY_NOT_FOUND;
  if (k == NULL || *k == '\0')
    return KEY_NOT_FOUND;

  for (i = 0; i < holder->idx; i++) {
    if (strcmp (k, holder->items[i].metrics->data) == 0)
      return i;
  }

  return KEY_NOT_FOUND;
}


/* Copy linked-list items to an array, sort, and move them back
 * to the list. Should be faster than sorting the list */
static void
sort_sub_list (GHolder * h, GSort sort)
{
  GHolderItem *arr;
  GSubItem *iter;
  GSubList *sub_list;
  int i, j, k;

  /* iterate over root-level nodes */
  for (i = 0; i < h->idx; i++) {
    sub_list = h->items[i].sub_list;
    if (sub_list == NULL)
      continue;

    arr = new_gholder_item (sub_list->size);

    /* copy items from the linked-list into an array */
    for (j = 0, iter = sub_list->head; iter; iter = iter->next, j++) {
      arr[j].metrics = new_gmetrics ();

      arr[j].metrics->bw.nbw = iter->metrics->bw.nbw;
      arr[j].metrics->data = xstrdup (iter->metrics->data);
      arr[j].metrics->hits = iter->metrics->hits;
      arr[j].metrics->id = iter->metrics->id;
      arr[j].metrics->visitors = iter->metrics->visitors;
      if (conf.serve_usecs)
        arr[j].metrics->avgts.nts = iter->metrics->avgts.nts;
    }
    sort_holder_items (arr, j, sort);
    delete_sub_list (sub_list);

    sub_list = new_gsublist ();
    for (k = 0; k < j; k++) {
      if (k > 0)
        sub_list = h->items[i].sub_list;

      add_sub_item_back (sub_list, h->module, arr[k].metrics);
      h->items[i].sub_list = sub_list;
    }
    free (arr);
  }
}

/* add an item from a sub_list to the dashboard */
static void
add_sub_item_to_dash (GDash ** dash, GHolderItem item, GModule module, int *i)
{
  GSubList *sub_list = item.sub_list;
  GSubItem *iter;
  GDashData *idata;

  char *entry;
  int *idx;
  idx = &(*dash)->module[module].idx_data;

  if (sub_list == NULL)
    return;

  for (iter = sub_list->head; iter; iter = iter->next, (*i)++) {
    entry = render_child_node (iter->metrics->data);
    if (!entry)
      continue;

    idata = &(*dash)->module[module].data[(*idx)];
    idata->metrics = new_gmetrics ();

    idata->metrics->visitors = iter->metrics->visitors;
    idata->metrics->bw.sbw = filesize_str (iter->metrics->bw.nbw);
    idata->metrics->data = xstrdup (entry);
    idata->metrics->hits = iter->metrics->hits;
    if (conf.serve_usecs)
      idata->metrics->avgts.sts = usecs_to_str (iter->metrics->avgts.nts);

    idata->is_subitem = 1;
    (*idx)++;
    free (entry);
  }
}

/* add a first level item to dashboard */
static void
add_item_to_dash (GDash ** dash, GHolderItem item, GModule module)
{
  GDashData *idata;
  int *idx = &(*dash)->module[module].idx_data;

  idata = &(*dash)->module[module].data[(*idx)];
  idata->metrics = new_gmetrics ();

  idata->metrics->bw.sbw = filesize_str (item.metrics->bw.nbw);
  idata->metrics->data = xstrdup (item.metrics->data);
  idata->metrics->hits = item.metrics->hits;
  idata->metrics->visitors = item.metrics->visitors;

  if (conf.append_method && item.metrics->method)
    idata->metrics->method = item.metrics->method;
  if (conf.append_protocol && item.metrics->protocol)
    idata->metrics->protocol = item.metrics->protocol;
  if (conf.serve_usecs)
    idata->metrics->avgts.sts = usecs_to_str (item.metrics->avgts.nts);

  (*idx)++;
}

/* load holder's data into dashboard */
void
load_data_to_dash (GHolder * h, GDash * dash, GModule module, GScroll * gscroll)
{
  int alloc_size = 0;
  int i, j;

  alloc_size = dash->module[module].alloc_data;
  if (gscroll->expanded && module == gscroll->current)
    alloc_size += h->sub_items_size;

  dash->module[module].alloc_data = alloc_size;
  dash->module[module].data = new_gdata (alloc_size);
  dash->module[module].holder_size = h->holder_size;

  for (i = 0, j = 0; i < alloc_size; i++) {
    if (h->items[j].metrics->data == NULL)
      continue;

    add_item_to_dash (&dash, h->items[j], module);
    if (gscroll->expanded && module == gscroll->current && h->sub_items_size)
      add_sub_item_to_dash (&dash, h->items[j], module, &i);
    j++;
  }
}

static void
data_visitors (GHolder * h)
{
  char date[DATE_LEN] = "";     /* Ymd */
  char *datum = h->items[h->idx].metrics->data;

  memset (date, 0, sizeof *date);
  /* verify we have a valid date conversion */
  if (convert_date (date, datum, conf.date_format, "%Y%m%d", DATE_LEN) == 0) {
    free (datum);
    h->items[h->idx].metrics->data = xstrdup (date);
    return;
  }
  LOG_DEBUG (("invalid date: %s", datum));

  free (datum);
  h->items[h->idx].metrics->data = xstrdup ("---");
}

static int
set_host_child_metrics (char *data, uint8_t id, GMetrics ** nmetrics)
{
  GMetrics *metrics;

  metrics = new_gmetrics ();
  metrics->data = xstrdup (data);
  metrics->id = id;
  *nmetrics = metrics;

  return 0;
}

static void
set_host_sub_list (GHolder * h, GSubList * sub_list)
{
  GMetrics *nmetrics;
#ifdef HAVE_LIBGEOIP
  char city[CITY_LEN] = "";
  char continent[CONTINENT_LEN] = "";
  char country[COUNTRY_LEN] = "";
#endif

  char *host = h->items[h->idx].metrics->data, *hostname = NULL;
#ifdef HAVE_LIBGEOIP
  /* add geolocation child nodes */
  set_geolocation (host, continent, country, city);

  /* country */
  if (country[0] != '\0') {
    set_host_child_metrics (country, MTRC_ID_COUNTRY, &nmetrics);
    add_sub_item_back (sub_list, h->module, nmetrics);
    h->items[h->idx].sub_list = sub_list;
    h->sub_items_size++;
  }

  /* city */
  if (city[0] != '\0') {
    set_host_child_metrics (city, MTRC_ID_CITY, &nmetrics);
    add_sub_item_back (sub_list, h->module, nmetrics);
    h->items[h->idx].sub_list = sub_list;
    h->sub_items_size++;
  }
#endif

  /* hostname */
  if (conf.enable_html_resolver && conf.output_html) {
    hostname = reverse_ip (host);
    set_host_child_metrics (hostname, MTRC_ID_HOSTNAME, &nmetrics);
    add_sub_item_back (sub_list, h->module, nmetrics);
    h->items[h->idx].sub_list = sub_list;
    h->sub_items_size++;
    free (hostname);
  }
}

static void
add_host_child_to_holder (GHolder * h)
{
  GMetrics *nmetrics;
  GSubList *sub_list = new_gsublist ();

  char *host = h->items[h->idx].metrics->data;
  char *hostname = NULL;
  int n = h->sub_items_size;

  /* add child nodes */
  set_host_sub_list (h, sub_list);

  pthread_mutex_lock (&gdns_thread.mutex);
  hostname = get_hostname (host);
  pthread_mutex_unlock (&gdns_thread.mutex);

  /* hostname */
  if (!hostname) {
    dns_resolver (host);
  } else if (hostname) {
    set_host_child_metrics (hostname, MTRC_ID_HOSTNAME, &nmetrics);
    add_sub_item_back (sub_list, h->module, nmetrics);
    h->items[h->idx].sub_list = sub_list;
    h->sub_items_size++;
    free (hostname);
  }

  /* did not add any items */
  if (n == h->sub_items_size)
    free (sub_list);
}

static void
add_host_to_holder (GRawDataItem item, GHolder * h, const GPanel * panel)
{
  add_data_to_holder (item, h, panel);
}

static void
add_data_to_holder (GRawDataItem item, GHolder * h, const GPanel * panel)
{
  GDataMap *map;
  char *data = NULL, *method = NULL, *protocol = NULL;
  int data_nkey = 0, visitors = 0;
  uint64_t bw = 0, ts = 0;

  data_nkey = (*(int *) item.key);
  map = (GDataMap *) item.value;
  if (map == NULL)
    return;

  if (!(data = get_node_from_key (data_nkey, h->module, MTRC_DATAMAP)))
    return;

  bw = get_cumulative_from_key (data_nkey, h->module, MTRC_BW);
  ts = get_cumulative_from_key (data_nkey, h->module, MTRC_TIME_SERVED);
  visitors = get_num_from_key (data_nkey, h->module, MTRC_VISITORS);

  h->items[h->idx].metrics = new_gmetrics ();
  h->items[h->idx].metrics->hits = map->data;
  h->items[h->idx].metrics->visitors = visitors;
  h->items[h->idx].metrics->data = data;
  h->items[h->idx].metrics->bw.nbw = bw;
  h->items[h->idx].metrics->avgts.nts = ts / map->data;

  if (conf.append_method) {
    method = get_node_from_key (data_nkey, h->module, MTRC_METHODS);
    h->items[h->idx].metrics->method = method;
  }

  if (conf.append_protocol) {
    protocol = get_node_from_key (data_nkey, h->module, MTRC_PROTOCOLS);
    h->items[h->idx].metrics->protocol = protocol;
  }

  if (panel->holder_callback)
    panel->holder_callback (h);

  h->idx++;
}

static int
set_root_metrics (int data_nkey, GDataMap * map, GModule module,
                  GMetrics ** nmetrics)
{
  GMetrics *metrics;
  char *data = NULL;
  uint64_t bw = 0, ts = 0;
  int visitors = 0;

  if (!(data = get_node_from_key (data_nkey, module, MTRC_DATAMAP)))
    return 1;

  bw = get_cumulative_from_key (data_nkey, module, MTRC_BW);
  ts = get_cumulative_from_key (data_nkey, module, MTRC_TIME_SERVED);
  visitors = get_num_from_key (data_nkey, module, MTRC_VISITORS);

  metrics = new_gmetrics ();
  metrics->avgts.nts = ts / map->data;
  metrics->bw.nbw = bw;
  metrics->data = data;
  metrics->hits = map->data;
  metrics->visitors = visitors;
  *nmetrics = metrics;

  return 0;
}

static void
add_root_to_holder (GRawDataItem item, GHolder * h,
                    GO_UNUSED const GPanel * panel)
{
  GDataMap *map;
  GSubList *sub_list;
  GMetrics *metrics, *nmetrics;
  char *root = NULL;
  int data_nkey = 0, root_idx = KEY_NOT_FOUND, idx = 0;

  data_nkey = (*(int *) item.key);
  map = (GDataMap *) item.value;
  if (map == NULL)
    return;

  if (set_root_metrics (data_nkey, map, h->module, &nmetrics) == 1)
    return;

  if (!(root = (get_root_from_key (map->root, h->module))))
    return;

  /* add data as a child node into holder */
  if (KEY_NOT_FOUND == (root_idx = get_item_idx_in_holder (h, root))) {
    idx = h->idx;
    sub_list = new_gsublist ();
    metrics = new_gmetrics ();

    h->items[idx].metrics = metrics;
    h->items[idx].metrics->data = root;
    h->idx++;
  } else {
    sub_list = h->items[root_idx].sub_list;
    metrics = h->items[root_idx].metrics;

    idx = root_idx;
    free (root);
  }

  add_sub_item_back (sub_list, h->module, nmetrics);
  h->items[idx].sub_list = sub_list;

  h->items[idx].metrics = metrics;
  h->items[idx].metrics->avgts.nts += nmetrics->avgts.nts;
  h->items[idx].metrics->bw.nbw += nmetrics->bw.nbw;
  h->items[idx].metrics->hits += nmetrics->hits;
  h->items[idx].metrics->visitors += nmetrics->visitors;

  h->sub_items_size++;
}

/* Load raw data into our holder structure */
void
load_holder_data (GRawData * raw_data, GHolder * h, GModule module, GSort sort)
{
  int i, size = 0;
  const GPanel *panel = panel_lookup (module);

  size = raw_data->size;
  h->holder_size = size > MAX_CHOICES ? MAX_CHOICES : size;
  h->ht_size = size;
  h->idx = 0;
  h->module = module;
  h->sub_items_size = 0;
  h->items = new_gholder_item (h->holder_size);

  for (i = 0; i < h->holder_size; i++) {
    panel->insert (raw_data->items[i], h, panel);
  }
  sort_holder_items (h->items, h->idx, sort);
  if (h->sub_items_size)
    sort_sub_list (h, sort);
  free_raw_data (raw_data);
}
