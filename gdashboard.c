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

#ifdef HAVE_LIBGEOIP
#  include <GeoIP.h>
#endif

#include "gdashboard.h"

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabinet.h"
#else
#include "glibht.h"
#endif

#include "error.h"
#include "xmalloc.h"
#include "gdns.h"
#include "parser.h"
#include "settings.h"
#include "util.h"

/* *INDENT-OFF* */

static GFind find_t;
/* module's styles */
static const GDashStyle module_style[TOTAL_MODULES] = {
  {COL_WHITE, COL_WHITE, COL_BLACK, COL_RED, COL_WHITE, -1, -1, -1},                 /* VISITORS        */
  {COL_WHITE, COL_WHITE, COL_BLACK, COL_BLACK, -1, COL_BLACK, COL_BLACK, COL_WHITE}, /* REQUESTS        */
  {COL_WHITE, COL_WHITE, COL_BLACK, COL_BLACK, -1, COL_BLACK, COL_BLACK, COL_WHITE}, /* REQUESTS_STATIC */
  {COL_WHITE, COL_WHITE, COL_BLACK, COL_BLACK, -1, COL_BLACK, COL_BLACK, COL_WHITE}, /* NOT FOUND       */
  {COL_WHITE, COL_WHITE, COL_BLACK, COL_BLACK, COL_WHITE, COL_BLACK, -1, -1},        /* HOSTS           */
  {COL_WHITE, COL_WHITE, -1, COL_RED, COL_WHITE, -1, -1, -1},                        /* OS              */
  {COL_WHITE, COL_WHITE, -1, COL_RED, COL_WHITE, -1, -1, -1},                        /* BROWSERS        */
  {COL_WHITE, COL_WHITE, -1, COL_BLACK, -1, -1, -1, -1},                             /* REFERRERS       */
  {COL_WHITE, COL_WHITE, -1, COL_BLACK, -1, -1, -1, -1},                             /* REFERRING_SITES */
  {COL_WHITE, COL_WHITE, -1, COL_BLACK, -1, -1, -1, -1},                             /* KEYPHRASES      */
#ifdef HAVE_LIBGEOIP
  {COL_WHITE, COL_WHITE, -1, COL_BLACK, -1, -1, -1, -1},                             /* GEO_LOCATION    */
#endif
  {COL_WHITE, COL_WHITE, -1, COL_BLACK, -1, -1, -1, -1},                             /* STATUS CODES    */
};
/* *INDENT-ON* */

/* reset find indices */
void
reset_find (void)
{
  if (find_t.pattern != NULL && *find_t.pattern != '\0')
    free (find_t.pattern);

  find_t.look_in_sub = 0;
  find_t.module = 0;
  find_t.next_idx = 0;          /* next total index    */
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

/* free dash and its elements */
void
free_dashboard (GDash * dash)
{
  int i, j;
  for (i = 0; i < TOTAL_MODULES; i++) {
    for (j = 0; j < dash->module[i].alloc_data; j++) {
      free (dash->module[i].data[j].data);
      free (dash->module[i].data[j].bandwidth);
      if (conf.serve_usecs)
        free (dash->module[i].data[j].serve_time);
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
new_gsubitem (GModule module, char *data, int hits, uint64_t bw)
{
  GSubItem *sub_item = xmalloc (sizeof (GSubItem));
  sub_item->data = data;
  sub_item->hits = hits;
  sub_item->bw = bw;
  sub_item->module = module;
  sub_item->prev = NULL;
  sub_item->next = NULL;
  return sub_item;
}

/* add an item to the end of a given sub list */
static void
add_sub_item_back (GSubList * sub_list, GModule module, char *data, int hits,
                   uint64_t bw)
{
  GSubItem *sub_item = new_gsubitem (module, data, hits, bw);
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
    free (item->data);
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
  if (item.data != NULL)
    free (item.data);
  if (item.method != NULL)
    free (item.method);
  if (item.protocol != NULL)
    free (item.protocol);
}

/* free memory allocated in holder for specific module */
void
free_holder_by_module (GHolder ** holder, GModule module)
{
  GSubList *sub_list;
  int j;

  if ((*holder) == NULL)
    return;

  for (j = 0; j < (*holder)[module].holder_size; j++) {
    sub_list = (*holder)[module].items[j].sub_list;
    /* free the sub list */
    if (sub_list != NULL)
      delete_sub_list (sub_list);
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
  GSubList *sub_list;
  int i, j;

  if ((*holder) == NULL)
    return;

  for (i = 0; i < TOTAL_MODULES; i++) {
    for (j = 0; j < (*holder)[i].holder_size; j++) {
      sub_list = (*holder)[i].items[j].sub_list;
      /* free the sub list */
      if (sub_list != NULL)
        delete_sub_list (sub_list);
      free_holder_data ((*holder)[i].items[j]);
    }
    free ((*holder)[i].items);
  }
  free (*holder);
  (*holder) = NULL;
}

/**
 * Determine which module should be expanded given the
 * current mouse position.
 */
int
set_module_from_mouse_event (GScrolling * scrolling, GDash * dash, int y)
{
  int module = 0, i;
  int offset = y - MAX_HEIGHT_HEADER - MAX_HEIGHT_FOOTER + 1;
  if (scrolling->expanded) {
    for (i = 0; i < TOTAL_MODULES; i++) {
      /* set current module */
      if (dash->module[i].pos_y == offset) {
        module = i;
        break;
      }
      /* we went over by one module, set current - 1 */
      if (dash->module[i].pos_y > offset) {
        module = i - 1;
        break;
      }
    }
  } else {
    offset += scrolling->dash;
    module = offset / DASH_COLLAPSED;
  }

  if (module >= TOTAL_MODULES)
    module = TOTAL_MODULES - 1;
  else if (module < 0)
    module = 0;

  if ((int) scrolling->current == module)
    return 1;

  scrolling->current = module;
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
  (void) h;                     /* avoid lint warning */

  if ((len = (n * (w - x) / max)) < 1)
    len = 1;
  return char_repeat (len, '|');
}

/*get largest data's length */
static int
get_max_data_len (GDashData * data, int size)
{
  int i, max = 0, len;
  for (i = 0; i < size; i++) {
    if (data[i].data == NULL)
      continue;
    len = strlen (data[i].data);
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
    int len = intlen (data[i].hits);
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
    if ((cur = data[i].hits) > max)
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
    data[i].percent = get_percentage (process, data[i].hits);
    if (data[i].percent > max)
      max = data[i].percent;
  }
  return max;
}

/* render module's total */
static void
render_total_label (WINDOW * win, GDashModule * module_data, int y)
{
  char *s;
  int win_h, win_w, total, ht_size;

  total = module_data->holder_size;
  ht_size = module_data->ht_size;

  s = xmalloc (snprintf (NULL, 0, "Total: %d/%d", total, ht_size) + 1);
  getmaxyx (win, win_h, win_w);
  (void) win_h;

  sprintf (s, "Total: %d/%d", total, ht_size);
  draw_header (win, s, "%s", y, win_w - strlen (s) - 2, win_w, HIGHLIGHT);
  free (s);
}

/* render dashboard bars (graph) */
static void
render_bars (WINDOW * win, GDashModule * module_data, int y, int *x, int idx,
             int w, int selected)
{
  const GDashStyle *style = module_style;
  GModule module = module_data->module;
  char *bar;

  if (style[module].color_bars == -1)
    return;

  bar = get_bars (module_data->data[idx].hits, module_data->max_hits, *x);
  if (selected)
    draw_header (win, bar, "%s", y, *x, w, HIGHLIGHT);
  else
    mvwprintw (win, y, *x, "%s", bar);
  free (bar);
}

/* render dashboard data */
static void
render_data (WINDOW * win, GDashModule * module_data, int y, int *x, int idx,
             int w, int selected)
{
  char buf[DATE_LEN];
  char *data, *padded_data;

  const GDashStyle *style = module_style;
  GModule module = module_data->module;

  data = substring (module_data->data[idx].data, 0, w - *x);
  if (module == VISITORS)
    convert_date (buf, data, "%Y%m%d", "%d/%b/%Y", DATE_LEN);

  if (selected) {
    if (module_data->module == HOSTS && module_data->data[idx].is_subitem) {
      padded_data = left_pad_str (data, *x);
      draw_header (win, padded_data, "%s", y, 0, w, HIGHLIGHT);
      free (padded_data);
    } else {
      draw_header (win, module == VISITORS ? buf : data, "%s", y, *x, w,
                   HIGHLIGHT);
    }
  } else {
    wattron (win, COLOR_PAIR (style[module].color_hits));
    mvwprintw (win, y, *x, "%s", module == VISITORS ? buf : data);
    wattroff (win, COLOR_PAIR (style[module].color_hits));
  }
  *x += module == VISITORS ? DATE_LEN - 1 : module_data->data_len;
  *x += DASH_SPACE;
  free (data);
}

/* render dashboard request method */
static void
render_method (WINDOW * win, GDashModule * module_data, int y, int *x,
               int idx, int w, int selected)
{
  const char *method = module_data->data[idx].method;
  const GDashStyle *style = module_style;
  GModule module = module_data->module;

  if (style[module].color_method == -1)
    return;

  if (method == NULL || *method == '\0')
    return;

  if (selected) {
    draw_header (win, method, "%s", y, *x, w, HIGHLIGHT);
  } else {
    wattron (win, A_BOLD | COLOR_PAIR (style[module].color_method));
    mvwprintw (win, y, *x, "%s", method);
    wattroff (win, A_BOLD | COLOR_PAIR (style[module].color_method));
  }
  *x += strlen (module_data->data[idx].method) + DASH_SPACE;
}

/* render dashboard request protocol */
static void
render_protocol (WINDOW * win, GDashModule * module_data, int y, int *x,
                 int idx, int w, int selected)
{
  const char *protocol = module_data->data[idx].protocol;
  const GDashStyle *style = module_style;
  GModule module = module_data->module;

  if (style[module].color_protocol == -1)
    return;

  if (protocol == NULL || *protocol == '\0')
    return;

  if (selected) {
    draw_header (win, protocol, "%s", y, *x, w, HIGHLIGHT);
  } else {
    wattron (win, COLOR_PAIR (style[module].color_protocol));
    mvwprintw (win, y, *x, "%s", protocol);
    wattroff (win, COLOR_PAIR (style[module].color_protocol));
  }
  *x += REQ_PROTO_LEN - 1 + DASH_SPACE;
}

/* render dashboard usecs */
static void
render_usecs (WINDOW * win, GDashModule * module_data, int y, int *x, int idx,
              int w, int selected)
{
  const GDashStyle *style = module_style;
  GModule module = module_data->module;

  if (module_data->module == HOSTS && module_data->data[idx].is_subitem)
    goto inc;
  if (style[module].color_usecs == -1)
    return;

  if (selected) {
    draw_header (win, module_data->data[idx].serve_time, "%9s", y, *x, w,
                 HIGHLIGHT);
  } else {
    wattron (win, A_BOLD | COLOR_PAIR (style[module].color_usecs));
    mvwprintw (win, y, *x, "%9s", module_data->data[idx].serve_time);
    wattroff (win, A_BOLD | COLOR_PAIR (style[module].color_usecs));
  }
inc:
  *x += DASH_SRV_TM_LEN + DASH_SPACE;
}

/* render dashboard bandwidth */
static void
render_bandwidth (WINDOW * win, GDashModule * module_data, int y, int *x,
                  int idx, int w, int selected)
{
  const GDashStyle *style = module_style;
  GModule module = module_data->module;

  if (module_data->module == HOSTS && module_data->data[idx].is_subitem)
    goto inc;
  if (style[module].color_bw == -1)
    return;

  if (selected) {
    draw_header (win, module_data->data[idx].bandwidth, "%11s", y, *x, w,
                 HIGHLIGHT);
  } else {
    wattron (win, A_BOLD | COLOR_PAIR (style[module].color_bw));
    mvwprintw (win, y, *x, "%11s", module_data->data[idx].bandwidth);
    wattroff (win, A_BOLD | COLOR_PAIR (style[module].color_bw));
  }
inc:
  *x += DASH_BW_LEN + DASH_SPACE;
}

/* render dashboard percent */
static void
render_percent (WINDOW * win, GDashModule * module_data, int y, int *x,
                int idx, int w, int selected)
{
  int max_hit = 0;
  const GDashStyle *style = module_style;
  GModule module = module_data->module;

  if (module_data->module == HOSTS && module_data->data[idx].is_subitem)
    goto inc;
  if (style[module].color_percent == -1)
    return;

  if (module_data->max_hits == module_data->data[idx].hits)
    max_hit = 1;

  if (selected) {
    char *percent = float_to_str (module_data->data[idx].percent);
    draw_header (win, percent, "%s%%", y, *x, w, HIGHLIGHT);
    free (percent);
  } else {
    wattron (win, A_BOLD | COLOR_PAIR (style[module].color_percent));
    if (max_hit)
      wattron (win, A_BOLD | COLOR_PAIR (COL_YELLOW));
    if (style[module].color_percent == COL_BLACK)
      wattron (win, A_BOLD | COLOR_PAIR (style[module].color_percent));

    mvwprintw (win, y, *x, "%.2f%%", module_data->data[idx].percent);

    if (style[module].color_percent == COL_BLACK)
      wattroff (win, A_BOLD | COLOR_PAIR (style[module].color_percent));
    if (max_hit)
      wattroff (win, A_BOLD | COLOR_PAIR (COL_YELLOW));
    wattroff (win, A_BOLD | COLOR_PAIR (style[module].color_percent));
  }
inc:
  *x += module_data->perc_len + DASH_SPACE;
}

/* render dashboard hits */
static void
render_hits (WINDOW * win, GDashModule * module_data, int y, int *x, int idx,
             int w, int selected)
{
  const GDashStyle *style = module_style;
  char *hits;
  GModule module = module_data->module;

  if (module_data->module == HOSTS && module_data->data[idx].is_subitem)
    goto inc;

  if (selected) {
    hits = int_to_str (module_data->data[idx].hits);
    draw_header (win, hits, "  %s", y, 0, w, HIGHLIGHT);
    free (hits);
  } else {
    wattron (win, COLOR_PAIR (style[module].color_hits));
    mvwprintw (win, y, *x, "%d", module_data->data[idx].hits);
    wattroff (win, COLOR_PAIR (style[module].color_hits));
  }
inc:
  *x += module_data->hits_len + DASH_SPACE;
}

/* render dashboard content */
static void
render_content (WINDOW * win, GDashModule * module_data, int *y, int *offset,
                int *total, GScrolling * scrolling)
{
  char *hd;
  GModule module = module_data->module;
  int expanded = 0, sel = 0, host_bars = 0, size;
  int i, k, j, x = 0, w, h;

  if (!conf.skip_term_resolver)
    host_bars = 1;

#ifdef HAVE_LIBGEOIP
  host_bars = 1;
#endif

  getmaxyx (win, h, w);

  if (scrolling->expanded && module == scrolling->current)
    expanded = 1;

  size = module_data->dash_size;
  for (i = *offset, j = 0; i < size; i++) {
    /* header */
    if ((i % size) == DASH_HEAD_POS) {
      k = module + 1;
      hd = xmalloc (snprintf (NULL, 0, "%d - %s", k, module_data->head) + 1);
      sprintf (hd, "%d - %s", k, module_data->head);

      draw_header (win, hd, " %s", (*y), 0, w, 1);
      free (hd);

      render_total_label (win, module_data, (*y));
      module_data->pos_y = (*y);
      (*y)++;
    }
    /* description */
    else if ((i % size) == DASH_DESC_POS)
      draw_header (win, module_data->desc, " %s", (*y)++, 0, w, 2);
    /* blank lines */
    else if ((i % size) == DASH_EMPTY_POS || (i % size) == size - 1)
      (*y)++;
    /* actual data */
    else if ((i % size) >= DASH_DATA_POS || (i % size) <= size - 2) {
      x = DASH_INIT_X;
      /* account for 2 lines at the header and 2 blank lines */
      j = ((i % size) - DASH_DATA_POS) + scrolling->module[module].offset;

      if (j < module_data->idx_data) {
        sel = expanded && j == scrolling->module[module].scroll ? 1 : 0;
        render_hits (win, module_data, *y, &x, j, w, sel);
        render_percent (win, module_data, *y, &x, j, w, sel);
        render_bandwidth (win, module_data, *y, &x, j, w, sel);

        /* render usecs if available */
        if (conf.serve_usecs)
          render_usecs (win, module_data, *y, &x, j, w, sel);
        /* render request method if available */
        if (conf.append_protocol)
          render_protocol (win, module_data, *y, &x, j, w, sel);
        /* render request method if available */
        if (conf.append_method)
          render_method (win, module_data, *y, &x, j, w, sel);
        render_data (win, module_data, *y, &x, j, w, sel);

        /* skip graph bars if module is expanded and we have sub nodes */
        if (module == HOSTS && expanded && host_bars);
        else
          render_bars (win, module_data, *y, &x, j, w, sel);
      }
      (*y)++;
    }
    /* everything else should be empty */
    else
      (*y)++;
    (*total)++;
    if (*y >= h)
      break;
  }
}

/* entry point to render dashboard */
void
display_content (WINDOW * win, GLog * logger, GDash * dash,
                 GScrolling * scrolling)
{
  float max_percent = 0.0;
  int i, j, n = 0, process = 0;

  int y = 0, offset = 0, total = 0;
  int dash_scroll = scrolling->dash;

  werase (win);

  for (i = 0; i < TOTAL_MODULES; i++) {
    n = dash->module[i].idx_data;
    offset = 0;
    for (j = 0; j < dash->module[i].dash_size; j++) {
      if (dash_scroll > total) {
        offset++;
        total++;
      }
    }

    /* Every module other than VISITORS, GEO_LOCATION, BROWSERS and OS
     * will use total req as base
     */
    switch (i) {
#ifdef HAVE_LIBGEOIP
     case GEO_LOCATION:
#endif
     case VISITORS:
     case BROWSERS:
     case OS:
       process = get_ht_size (ht_unique_visitors);
       break;
     default:
       process = logger->process;
    }
    max_percent = set_percent_data (dash->module[i].data, n, process);
    dash->module[i].module = i;
    dash->module[i].max_hits = get_max_hit (dash->module[i].data, n);
    dash->module[i].hits_len = get_max_hit_len (dash->module[i].data, n);
    dash->module[i].data_len = get_max_data_len (dash->module[i].data, n);
    dash->module[i].perc_len = intlen ((int) max_percent) + 4;

    render_content (win, &dash->module[i], &y, &offset, &total, scrolling);
  }
  wrefresh (win);
}

/* reset scroll and offset for each module */
void
reset_scroll_offsets (GScrolling * scrolling)
{
  int i;
  for (i = 0; i < TOTAL_MODULES; i++) {
    scrolling->module[i].scroll = 0;
    scrolling->module[i].offset = 0;
  }
}

/* compile the regular expression and see if it's valid */
static int
regexp_init (regex_t * regex, const char *pattern)
{
  int y, x, rc;
  char buf[REGEX_ERROR];

  getmaxyx (stdscr, y, x);
  rc =
    regcomp (regex, pattern, REG_EXTENDED | (find_t.icase ? REG_ICASE : 0));
  /* something went wrong */
  if (rc != 0) {
    regerror (rc, regex, buf, sizeof (buf));
    draw_header (stdscr, buf, "%s", y - 1, 0, x, WHITE_RED);
    refresh ();
    return 1;
  }
  return 0;
}

/* set search scrolling */
static void
perform_find_dash_scroll (GScrolling * scrolling, GModule module)
{
  int *scrll, *offset;
  int exp_size = DASH_EXPANDED - DASH_NON_DATA;

  /* reset scrolling offsets if we are changing module */
  if (scrolling->current != module)
    reset_scroll_offsets (scrolling);

  scrll = &scrolling->module[module].scroll;
  offset = &scrolling->module[module].offset;

  (*scrll) = find_t.next_idx;
  if (*scrll >= exp_size && *scrll >= *offset + exp_size)
    (*offset) = (*scrll) < exp_size - 1 ? 0 : (*scrll) - exp_size + 1;

  scrolling->current = module;
  scrolling->dash = module * DASH_COLLAPSED;
  scrolling->expanded = 1;
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
      rc = regexec (regex, iter->data, 0, NULL, 0);
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
perform_next_find (GHolder * h, GScrolling * scrolling)
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
      data = h[module].items[j].data;

      rc = regexec (&regex, data, 0, NULL, 0);
      if (rc != 0 && rc != REG_NOMATCH) {
        regerror (rc, &regex, buf, sizeof (buf));
        draw_header (stdscr, buf, "%s", y - 1, 0, x, WHITE_RED);
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
      reset_scroll_offsets (scrolling);
      scrolling->expanded = 0;
    }
    if (module == TOTAL_MODULES - 1) {
      find_t.module = 0;
      goto out;
    }
  }

found:
  perform_find_dash_scroll (scrolling, module);
out:
  regfree (&regex);
  return 0;
}

/* render find dialog */
int
render_find_dialog (WINDOW * main_win, GScrolling * scrolling)
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
  draw_header (win, FIND_HEAD, " %s", 1, 1, w - 2, 1);
  draw_header (win, FIND_DESC, " %s", 2, 1, w - 2, 2);

  find_t.icase = 0;
  query = input_string (win, 4, 2, w - 3, "", 1, &find_t.icase);
  if (query != NULL && *query != '\0') {
    reset_scroll_offsets (scrolling);
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
    return -1;
  if (holder->idx == 0)
    return -1;

  for (i = 0; i < holder->idx; i++) {
    if (strcmp (k, holder->items[i].data) == 0)
      return i;
  }

  return -1;
}

/* add an item from a sub_list to the dashboard */
static void
add_sub_item_to_dash (GDash ** dash, GHolderItem item, GModule module, int *i)
{
  GSubList *sub_list = item.sub_list;
  GSubItem *iter;

  char *entry;
  int *idx;
  idx = &(*dash)->module[module].idx_data;

  for (iter = sub_list->head; iter; iter = iter->next) {
    entry = render_child_node (iter->data);
    if (entry) {
      (*dash)->module[module].data[(*idx)].bandwidth =
        filesize_str (iter->bw);
      (*dash)->module[module].data[(*idx)].bw = iter->bw;
      (*dash)->module[module].data[(*idx)].data = xstrdup (entry);
      (*dash)->module[module].data[(*idx)].hits = iter->hits;
      (*dash)->module[module].data[(*idx)++].is_subitem = 1;
      free (entry);
    }
    (*i)++;
  }
}

/* add a host item to holder */
static void
add_host_node (GHolder * h, int hits, char *data, uint64_t bw, uint64_t usecs)
{
  GSubList *sub_list = new_gsublist ();
  char *ip = xstrdup (data), *hostname = NULL;
  int found = 0;
  void *value_ptr;

#ifdef HAVE_LIBGEOIP
  const char *addr = data;
  const char *location = NULL;
#endif

  h->items[h->idx].bw += bw;
  h->items[h->idx].hits += hits;
  h->items[h->idx].data = data;
  if (conf.serve_usecs)
    h->items[h->idx].usecs = usecs;
  h->items[h->idx].sub_list = sub_list;

#ifdef HAVE_LIBGEOIP
  location = get_geoip_data (addr);
  add_sub_item_back (sub_list, h->module, xstrdup (location), hits, bw);
  h->items[h->idx].sub_list = sub_list;
  h->sub_items_size++;
#endif

  pthread_mutex_lock (&gdns_thread.mutex);
#ifdef HAVE_LIBTOKYOCABINET
  value_ptr = tcbdbget2 (ht_hostnames, ip);
  if (value_ptr) {
    found = 1;
    hostname = value_ptr;
  }
#else
  found = g_hash_table_lookup_extended (ht_hostnames, ip, NULL, &value_ptr);
  if (found && value_ptr)
    hostname = xstrdup (value_ptr);
#endif
  pthread_mutex_unlock (&gdns_thread.mutex);

  if (!found) {
    dns_resolver (ip);
  } else if (hostname) {
    add_sub_item_back (sub_list, h->module, hostname, hits, bw);
    h->items[h->idx].sub_list = sub_list;
    h->sub_items_size++;
  }
  free (ip);

  h->idx++;
}

static void
add_os_node (GHolder * h, GOpeSys * opesys, char *data, uint64_t bw)
{
  GSubList *sub_list;
  int type_idx = -1;

  type_idx = get_item_idx_in_holder (h, opesys->os_type);
  if (type_idx == -1) {
    h->items[h->idx].bw += bw;
    h->items[h->idx].hits += opesys->hits;
    h->items[h->idx].data = xstrdup (opesys->os_type);

    /* data (child) */
    sub_list = new_gsublist ();
    add_sub_item_back (sub_list, h->module, data, opesys->hits, bw);
    h->items[h->idx++].sub_list = sub_list;
    h->sub_items_size++;
  } else {
    sub_list = h->items[type_idx].sub_list;
    add_sub_item_back (sub_list, h->module, data, opesys->hits, bw);

    h->items[type_idx].sub_list = sub_list;
    h->items[type_idx].bw += bw;
    h->items[type_idx].hits += opesys->hits;
    h->sub_items_size++;
  }
}

static void
add_browser_node (GHolder * h, GBrowser * browser, char *data, uint64_t bw)
{
  GSubList *sub_list;
  int type_idx = -1;

  type_idx = get_item_idx_in_holder (h, browser->browser_type);
  if (type_idx == -1) {
    h->items[h->idx].bw += bw;
    h->items[h->idx].hits += browser->hits;
    h->items[h->idx].data = xstrdup (browser->browser_type);

    /* data (child) */
    sub_list = new_gsublist ();
    add_sub_item_back (sub_list, h->module, data, browser->hits, bw);
    h->items[h->idx++].sub_list = sub_list;
    h->sub_items_size++;
  } else {
    sub_list = h->items[type_idx].sub_list;
    add_sub_item_back (sub_list, h->module, data, browser->hits, bw);

    h->items[type_idx].sub_list = sub_list;
    h->items[type_idx].bw += bw;
    h->items[type_idx].hits += browser->hits;
    h->sub_items_size++;
  }
}

/* add request items (e.g., method, protocol, request) to holder */
static void
add_request_node (GHolder * h, GRequest * request, char *key, uint64_t bw)
{
  uint64_t usecs = 0;
  /* serve time in usecs */
  if (conf.serve_usecs) {
    usecs = get_serve_time (key, h->module);
    usecs = usecs / request->hits;
  }
  h->items[h->idx].bw = bw;
  h->items[h->idx].data = xstrdup (request->request);
  h->items[h->idx].hits = request->hits;
  if (conf.append_method && request->method)
    h->items[h->idx].method = xstrdup (request->method);
  if (conf.append_protocol && request->protocol)
    h->items[h->idx].protocol = xstrdup (request->protocol);
  if (conf.serve_usecs)
    h->items[h->idx].usecs = usecs;
  h->idx++;

  free (key);
}

/* add a geolocation item to holder */
#ifdef HAVE_LIBGEOIP
static void
add_geolocation_node (GHolder * h, GLocation * loc, char *data, uint64_t bw)
{
  GSubList *sub_list;
  int type_idx = -1;

  type_idx = get_item_idx_in_holder (h, loc->continent);
  if (type_idx == -1) {
    h->items[h->idx].bw += bw;
    h->items[h->idx].hits += loc->hits;
    h->items[h->idx].data = xstrdup (loc->continent);

    /* data (child) */
    sub_list = new_gsublist ();
    add_sub_item_back (sub_list, h->module, data, loc->hits, bw);
    h->items[h->idx++].sub_list = sub_list;
    h->sub_items_size++;
  } else {
    sub_list = h->items[type_idx].sub_list;
    add_sub_item_back (sub_list, h->module, data, loc->hits, bw);

    h->items[type_idx].sub_list = sub_list;
    h->items[type_idx].bw += bw;
    h->items[type_idx].hits += loc->hits;
    h->sub_items_size++;
  }
}
#endif

/* add a status code item to holder */
static void
add_status_code_node (GHolder * h, int hits, char *data, uint64_t bw)
{
  GSubList *sub_list;
  const char *type = NULL, *status = NULL;
  int type_idx = -1;

  type = verify_status_code_type (data);
  status = verify_status_code (data);

  type_idx = get_item_idx_in_holder (h, type);
  if (type_idx == -1) {
    h->items[h->idx].bw += bw;
    h->items[h->idx].hits += hits;
    h->items[h->idx].data = xstrdup (type);

    /* data (child) */
    sub_list = new_gsublist ();
    add_sub_item_back (sub_list, h->module, xstrdup (status), hits, bw);
    h->items[h->idx++].sub_list = sub_list;
    h->sub_items_size++;
  } else {
    sub_list = h->items[type_idx].sub_list;
    add_sub_item_back (sub_list, h->module, xstrdup (status), hits, bw);

    h->items[type_idx].sub_list = sub_list;
    h->items[type_idx].bw += bw;
    h->items[type_idx].hits += hits;
    h->sub_items_size++;
  }
  free (data);
}

/* add a first level item to dashboard */
static void
add_item_to_dash (GDash ** dash, GHolderItem item, GModule module)
{
  int *idx = &(*dash)->module[module].idx_data;

  (*dash)->module[module].data[(*idx)].bandwidth = filesize_str (item.bw);
  (*dash)->module[module].data[(*idx)].bw = item.bw;
  (*dash)->module[module].data[(*idx)].data = xstrdup (item.data);
  (*dash)->module[module].data[(*idx)].hits = item.hits;
  if (conf.append_method && item.method)
    (*dash)->module[module].data[(*idx)].method = item.method;
  if (conf.append_protocol && item.protocol)
    (*dash)->module[module].data[(*idx)].protocol = item.protocol;
  if (conf.serve_usecs) {
    (*dash)->module[module].data[(*idx)].usecs = item.usecs;
    (*dash)->module[module].data[(*idx)].serve_time =
      usecs_to_str (item.usecs);
  }
  (*idx)++;
}

/* load holder's data into dashboard */
void
load_data_to_dash (GHolder * h, GDash * dash, GModule module,
                   GScrolling * scrolling)
{
  int alloc_size = 0;
  int i, j;

  alloc_size = dash->module[module].alloc_data;
  if (scrolling->expanded && module == scrolling->current) {
    if (module == OS || module == BROWSERS || module == HOSTS ||
        module == STATUS_CODES
#ifdef HAVE_LIBGEOIP
        || module == GEO_LOCATION
#endif
      )
      alloc_size += h->sub_items_size;
  }
  dash->module[module].alloc_data = alloc_size;
  dash->module[module].data = new_gdata (alloc_size);
  dash->module[module].holder_size = h->holder_size;

  for (i = 0, j = 0; i < alloc_size; i++) {
    if (j < dash->module[module].ht_size && h->items[j].data != NULL) {
      add_item_to_dash (&dash, h->items[j], module);
      if (scrolling->expanded && module == scrolling->current) {
        if (module == OS || module == BROWSERS || module == HOSTS ||
            module == STATUS_CODES
#ifdef HAVE_LIBGEOIP
            || module == GEO_LOCATION
#endif
          )
          add_sub_item_to_dash (&dash, h->items[j], module, &i);
      }
      j++;
    }
  }
}

/* apply user defined sort */
static void
sort_holder_items (GHolderItem * items, int size, GSort sort)
{
  switch (sort.field) {
   case SORT_BY_HITS:
     if (sort.sort == SORT_DESC)
       qsort (items, size, sizeof (GHolderItem), cmp_num_desc);
     else
       qsort (items, size, sizeof (GHolderItem), cmp_num_asc);
     break;
   case SORT_BY_DATA:
     if (sort.sort == SORT_DESC)
       qsort (items, size, sizeof (GHolderItem), cmp_data_desc);
     else
       qsort (items, size, sizeof (GHolderItem), cmp_data_asc);
     break;
   case SORT_BY_BW:
     if (sort.sort == SORT_DESC)
       qsort (items, size, sizeof (GHolderItem), cmp_bw_desc);
     else
       qsort (items, size, sizeof (GHolderItem), cmp_bw_asc);
     break;
   case SORT_BY_USEC:
     if (sort.sort == SORT_DESC)
       qsort (items, size, sizeof (GHolderItem), cmp_usec_desc);
     else
       qsort (items, size, sizeof (GHolderItem), cmp_usec_asc);
     break;
   case SORT_BY_PROT:
     if (sort.sort == SORT_DESC)
       qsort (items, size, sizeof (GHolderItem), cmp_proto_desc);
     else
       qsort (items, size, sizeof (GHolderItem), cmp_proto_asc);
     break;
   case SORT_BY_MTHD:
     if (sort.sort == SORT_DESC)
       qsort (items, size, sizeof (GHolderItem), cmp_mthd_desc);
     else
       qsort (items, size, sizeof (GHolderItem), cmp_mthd_asc);
     break;
  }
}

/* Copy linked-list items to an array, sort, and move them back
 * to the list should be faster than sorting the list
 */
static void
sort_sub_list (GHolder * h, GSort sort)
{
  int i, j, k;
  GHolderItem *arr_items;

  for (i = 0; i < h->idx; i++) {
    GSubList *sub_list = h->items[i].sub_list;
    GSubItem *iter;
    arr_items = new_gholder_item (sub_list->size);

    /* copy items from the linked-list into an rray */
    for (j = 0, iter = sub_list->head; iter; iter = iter->next) {
      arr_items[j].data = xstrdup ((char *) iter->data);
      arr_items[j++].hits = iter->hits;
    }
    sort_holder_items (arr_items, j, sort);
    delete_sub_list (sub_list);

    sub_list = new_gsublist ();
    for (k = 0; k < j; k++) {
      if (k > 0)
        sub_list = h->items[i].sub_list;
      add_sub_item_back (sub_list, h->module, arr_items[k].data,
                         arr_items[k].hits, 0);
      h->items[i].sub_list = sub_list;
    }
    free (arr_items);
  }
}

uint32_t
get_ht_size_by_module (GModule module)
{
#ifdef HAVE_LIBTOKYOCABINET
  TCBDB *ht;
#else
  GHashTable *ht;
#endif

  switch (module) {
   case VISITORS:
     ht = ht_unique_vis;
     break;
   case REQUESTS:
     ht = ht_requests;
     break;
   case REQUESTS_STATIC:
     ht = ht_requests_static;
     break;
   case NOT_FOUND:
     ht = ht_not_found_requests;
     break;
   case HOSTS:
     ht = ht_hosts;
     break;
   case OS:
     ht = ht_os;
     break;
   case BROWSERS:
     ht = ht_browsers;
     break;
   case REFERRERS:
     ht = ht_referrers;
     break;
   case REFERRING_SITES:
     ht = ht_referring_sites;
     break;
   case KEYPHRASES:
     ht = ht_keyphrases;
     break;
#ifdef HAVE_LIBGEOIP
   case GEO_LOCATION:
     ht = ht_countries;
     break;
#endif
   case STATUS_CODES:
     ht = ht_status_code;
     break;
   default:
     return 0;
  }

  return get_ht_size (ht);
}

/* Load raw data into our holder structure.
 *
 * Note: the key from raw data needs to be allocated as well as
 * the pointer value since they are free'd after the data gets into
 * the holder structure.
 */
void
load_data_to_holder (GRawData * raw_data, GHolder * h, GModule module,
                     GSort sort)
{
  char *data;
  int hits;
  int i, size = 0;
  uint64_t bw = 0;
  uint64_t usecs = 0;

  size = raw_data->size;
  h->holder_size = size > MAX_CHOICES ? MAX_CHOICES : size;
  h->idx = 0;
  h->module = module;
  h->sub_items_size = 0;
  h->items = new_gholder_item (h->holder_size);

  for (i = 0; i < h->holder_size; i++) {
    data = xstrdup (raw_data->items[i].key);    /* key */
    bw = get_bandwidth (data, module);

    switch (module) {
     case REQUESTS:
     case REQUESTS_STATIC:
     case NOT_FOUND:
       add_request_node (h, raw_data->items[i].value, data, bw);
       break;
     case OS:
       add_os_node (h, raw_data->items[i].value, data, 0);
       break;
     case BROWSERS:
       add_browser_node (h, raw_data->items[i].value, data, 0);
       break;
     case HOSTS:
       hits = (*(int *) raw_data->items[i].value);
       /* serve time in usecs */
       if (conf.serve_usecs) {
         usecs = get_serve_time (data, module);
         usecs = usecs / hits;
       }
       add_host_node (h, hits, data, bw, usecs);
       break;
     case STATUS_CODES:
       hits = (*(int *) raw_data->items[i].value);
       add_status_code_node (h, hits, data, bw);
       break;
#ifdef HAVE_LIBGEOIP
     case GEO_LOCATION:
       add_geolocation_node (h, raw_data->items[i].value, data, 0);
       break;
#endif
     default:
       hits = (*(int *) raw_data->items[i].value);
       /* serve time in usecs */
       if (conf.serve_usecs) {
         usecs = get_serve_time (data, module);
         usecs = usecs / hits;
       }
       h->items[h->idx].bw = bw;
       h->items[h->idx].data = data;
       h->items[h->idx].hits = hits;
       if (conf.serve_usecs)
         h->items[h->idx].usecs = usecs;
       h->idx++;
    }
  }
  sort_holder_items (h->items, h->idx, sort);
  /* HOSTS module does not have "real" sub items, thus we don't include it */
  if (module == OS || module == BROWSERS || module == STATUS_CODES
#ifdef HAVE_LIBGEOIP
      || module == GEO_LOCATION
#endif
    )
    sort_sub_list (h, sort);

  free_raw_data (raw_data);
}
