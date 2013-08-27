/**
 * gmenu.c -- goaccess main dashboard
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

#include "error.h"
#include "xmalloc.h"
#include "gdns.h"
#include "parser.h"
#include "settings.h"
#include "util.h"

/* *INDENT-OFF* */

GHolder *holder = NULL;
static GFind find_t;
/* module's styles */
static GDashStyle module_style[TOTAL_MODULES] = {
   {COL_WHITE, COL_WHITE, COL_BLACK, COL_RED, COL_WHITE, -1},          /* VISITORS        */
   {COL_WHITE, COL_WHITE, COL_BLACK, COL_BLACK, -1, COL_BLACK},        /* REQUESTS        */
   {COL_WHITE, COL_WHITE, COL_BLACK, COL_BLACK, -1, COL_BLACK},        /* REQUESTS_STATIC */
   {COL_WHITE, COL_WHITE, COL_BLACK, COL_BLACK, -1, COL_BLACK,},       /* NOT FOUND       */
   {COL_WHITE, COL_WHITE, COL_BLACK, COL_BLACK, COL_WHITE, COL_BLACK}, /* HOSTS           */
   {COL_WHITE, COL_WHITE, -1, COL_RED, COL_WHITE, -1},                 /* OS              */
   {COL_WHITE, COL_WHITE, -1, COL_RED, COL_WHITE, -1},                 /* BROWSERS        */
   {COL_WHITE, COL_WHITE, -1, COL_BLACK, -1, -1},                      /* REFERRERS       */
   {COL_WHITE, COL_WHITE, -1, COL_BLACK, -1, -1},                      /* REFERRING_SITES */
   {COL_WHITE, COL_WHITE, -1, COL_BLACK, -1, -1},                      /* KEYPHRASES      */
   {COL_WHITE, COL_WHITE, -1, COL_BLACK, -1, -1},                      /* STATUS CODES    */
};
/* *INDENT-ON* */

/* reset find indices */
void
reset_find ()
{
   if (find_t.pattern != NULL && *find_t.pattern != '\0')
      free (find_t.pattern);

   find_t.look_in_sub = 0;
   find_t.module = 0;
   find_t.next_idx = 0;         /* next total index    */
   find_t.next_parent_idx = 0;  /* next parent index   */
   find_t.next_sub_idx = 0;     /* next sub item index */
   find_t.pattern = NULL;
}

/* allocate memory for dash */
GDash *
new_gdash ()
{
   GDash *dash = xmalloc (sizeof (GDash));
   memset (dash, 0, sizeof *dash);
   dash->total_alloc = 0;

   return dash;
}

/* allocate memory for dash elements */
GDashData *
new_gdata (unsigned int size)
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
new_gholder (unsigned int size)
{
   GHolder *holder = xmalloc (size * sizeof (GHolder));
   memset (holder, 0, size * sizeof *holder);

   return holder;
}

/* allocate memory for holder items */
GHolderItem *
new_gholder_item (unsigned int size)
{
   GHolderItem *item = xcalloc (size, sizeof (GHolderItem));
   return item;
}

/* allocate memory for a sub list */
GSubList *
new_gsublist (void)
{
   GSubList *sub_list = xmalloc (sizeof (GSubList));
   sub_list->head = NULL;
   sub_list->tail = NULL;
   sub_list->size = 0;
   return sub_list;
}

/* allocate memory for a sub list item */
GSubItem *
new_gsubitem (GModule module, const char *data, int hits, unsigned long long bw)
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
void
add_sub_item_back (GSubList * sub_list, GModule module, const char *data,
                   int hits, unsigned long long bw)
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
   if (sub_list != NULL && sub_list->size == 0)
      goto clear;
   if (sub_list->size == 0)
      return;

   GSubItem *item = item;
   GSubItem *next = next;

   for (item = sub_list->head; item; item = next) {
      next = item->next;
      free (item);
   }
 clear:
   sub_list->head = NULL;
   sub_list->size = 0;
   free (sub_list);
}

/* free allocated memory for holder */
void
free_holder (GHolder ** holder)
{
   if ((*holder) == NULL)
      return;

   GSubList *sub_list;
   int i, j;
   for (i = 0; i < TOTAL_MODULES; i++) {
      for (j = 0; j < (*holder)[i].holder_size; j++) {
         sub_list = (*holder)[i].items[j].sub_list;
         /* free the sub list */
         if (sub_list != NULL)
            delete_sub_list (sub_list);
         if ((*holder)[i].items[j].data != NULL)
            free ((*holder)[i].items[j].data);
      }
      free ((*holder)[i].items);
   }
   free (*holder);
   (*holder) = NULL;
}

/* allocate memory for ht raw data */
GRawData *
new_grawdata ()
{
   GRawData *raw_data = xmalloc (sizeof (GRawData));
   memset (raw_data, 0, sizeof *raw_data);

   return raw_data;
}

/* allocate memory for raw data items */
GRawDataItem *
new_grawdata_item (unsigned int size)
{
   GRawDataItem *item = xcalloc (size, sizeof (GRawDataItem));
   return item;
}

/* free allocated memory for raw data */
void
free_raw_data (GRawData * raw_data)
{
   free (raw_data->items);
   free (raw_data);
}

/* calculate hits percentage */
float
get_percentage (unsigned long long total, unsigned long long hit)
{
   return ((float) (hit * 100) / (total));
}

/* render child nodes */
static char *
render_child_node (const char *data)
{
   if (data == NULL || *data == '\0')
      return NULL;

#ifdef HAVE_LIBNCURSESW
   char *bend = "\xe2\x94\x9c";
   char *horz = "\xe2\x94\x80";
#else
   char *bend = "|";
   char *horz = "`-";
#endif

   char *buf;
   int len = 0;

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
   h = h;                       /* avoid lint warning */

   if ((len = (n * (w - x) / max)) < 1)
      len = 1;
   return char_repeat (len, '|');
}

/*get largest data's length */
int
get_max_data_len (GDashData * data, int size)
{
   int i, max = 0;
   for (i = 0; i < size; i++) {
      if (data[i].data == NULL)
         continue;
      int len = strlen (data[i].data);
      if (len > max)
         max = len;
   }
   return max;
}

/*get largest hit's length */
int
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
int
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
float
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
void
render_total_label (WINDOW * win, GDashModule * module_data, int y)
{
   int win_h, win_w;
   getmaxyx (win, win_h, win_w);
   win_h = win_h;

   int total = module_data->holder_size;
   int ht_size = module_data->ht_size;

   char *s = xmalloc (snprintf (NULL, 0, "Total: %d/%d", total, ht_size) + 1);
   sprintf (s, "Total: %d/%d", total, ht_size);

   wattron (win, COLOR_PAIR (HIGHLIGHT));
   mvwprintw (win, y, win_w - strlen (s) - 2, "%s", s);
   wattroff (win, COLOR_PAIR (HIGHLIGHT));
   free (s);
}

/* render dashboard bars (graph) */
void
render_bars (WINDOW * win, GDashModule * module_data, int y, int *x, int idx,
             int w, int selected)
{
   GDashStyle *style = module_style;
   GModule module = module_data->module;
   char *bar;

   if (style[module].color_bars == -1)
      return;

   bar = get_bars (module_data->data[idx].hits, module_data->max_hits, *x);
   if (selected) {
      if (conf.color_scheme == MONOCHROME)
         init_pair (1, COLOR_BLACK, COLOR_WHITE);
      else
         init_pair (1, COLOR_BLACK, COLOR_GREEN);

      wattron (win, COLOR_PAIR (HIGHLIGHT));
      mvwhline (win, y, *x, ' ', w);
      mvwprintw (win, y, *x, "%s", bar);
      wattroff (win, COLOR_PAIR (HIGHLIGHT));
   } else {
      mvwprintw (win, y, *x, "%s", bar);
   }
   free (bar);
}

/* render dashboard data */
void
render_data (WINDOW * win, GDashModule * module_data, int y, int *x, int idx,
             int w, int selected)
{
   char buf[DATE_LEN];
   char *data;

   GDashStyle *style = module_style;
   GModule module = module_data->module;

   data = substring (module_data->data[idx].data, 0, w - *x);
   if (module == VISITORS)
      convert_date (buf, data, "%Y%m%d", "%d/%b/%Y", DATE_LEN);

   if (selected) {
      if (conf.color_scheme == MONOCHROME)
         init_pair (1, COLOR_BLACK, COLOR_WHITE);
      else
         init_pair (1, COLOR_BLACK, COLOR_GREEN);

      wattron (win, COLOR_PAIR (HIGHLIGHT));
      if (module_data->module == HOSTS && module_data->data[idx].is_subitem)
         mvwhline (win, y, 0, ' ', w);
      else
         mvwhline (win, y, *x, ' ', w);
      mvwprintw (win, y, *x, "%s", module == VISITORS ? buf : data);
      wattroff (win, COLOR_PAIR (HIGHLIGHT));
   } else {
      wattron (win, COLOR_PAIR (style[module].color_hits));
      mvwprintw (win, y, *x, "%s", module == VISITORS ? buf : data);
      wattroff (win, COLOR_PAIR (style[module].color_hits));
   }
   *x += module == VISITORS ? DATE_LEN - 1 : module_data->data_len;
   *x += DASH_SPACE;
   free (data);
}

/* render dashboard bandwidth */
void
render_bandwidth (WINDOW * win, GDashModule * module_data, int y, int *x,
                  int idx, int w, int selected)
{
   GDashStyle *style = module_style;
   GModule module = module_data->module;

   if (module_data->module == HOSTS && module_data->data[idx].is_subitem)
      goto inc;
   if (style[module].color_bw == -1)
      return;

   if (selected) {
      if (conf.color_scheme == MONOCHROME)
         init_pair (1, COLOR_BLACK, COLOR_WHITE);
      else
         init_pair (1, COLOR_BLACK, COLOR_GREEN);

      wattron (win, COLOR_PAIR (HIGHLIGHT));
      mvwhline (win, y, *x, ' ', w);
      mvwprintw (win, y, *x, "%11s", module_data->data[idx].bandwidth);
      wattroff (win, COLOR_PAIR (HIGHLIGHT));
   } else {
      wattron (win, A_BOLD | COLOR_PAIR (style[module].color_bw));
      mvwprintw (win, y, *x, "%11s", module_data->data[idx].bandwidth);
      wattroff (win, A_BOLD | COLOR_PAIR (style[module].color_bw));
   }
 inc:
   *x += DASH_BW_LEN + DASH_SPACE;
}

/* render dashboard usecs */
void
render_usecs (WINDOW * win, GDashModule * module_data, int y, int *x,
              int idx, int w, int selected)
{
   GDashStyle *style = module_style;
   GModule module = module_data->module;

   if (module_data->module == HOSTS && module_data->data[idx].is_subitem)
      goto inc;
   if (style[module].color_usecs == -1)
      return;

   if (selected) {
      if (conf.color_scheme == MONOCHROME)
         init_pair (1, COLOR_BLACK, COLOR_WHITE);
      else
         init_pair (1, COLOR_BLACK, COLOR_GREEN);

      wattron (win, COLOR_PAIR (HIGHLIGHT));
      mvwhline (win, y, *x, ' ', w);
      mvwprintw (win, y, *x, "%9s", module_data->data[idx].serve_time);
      wattroff (win, COLOR_PAIR (HIGHLIGHT));
   } else {
      wattron (win, A_BOLD | COLOR_PAIR (style[module].color_usecs));
      mvwprintw (win, y, *x, "%9s", module_data->data[idx].serve_time);
      wattroff (win, A_BOLD | COLOR_PAIR (style[module].color_usecs));
   }
 inc:
   *x += DASH_SRV_TM_LEN + DASH_SPACE;
}

/* render dashboard percent */
void
render_percent (WINDOW * win, GDashModule * module_data, int y, int *x, int idx,
                int w, int selected)
{
   int max_hit = 0;
   GDashStyle *style = module_style;
   GModule module = module_data->module;

   if (module_data->module == HOSTS && module_data->data[idx].is_subitem)
      goto inc;
   if (style[module].color_percent == -1)
      return;

   if (module_data->max_hits == module_data->data[idx].hits)
      max_hit = 1;

   if (selected) {
      if (conf.color_scheme == MONOCHROME)
         init_pair (1, COLOR_BLACK, COLOR_WHITE);
      else
         init_pair (1, COLOR_BLACK, COLOR_GREEN);

      wattron (win, COLOR_PAIR (HIGHLIGHT));
      mvwhline (win, y, *x, ' ', w);
      mvwprintw (win, y, *x, "%.2f%%", module_data->data[idx].percent);
      wattroff (win, COLOR_PAIR (HIGHLIGHT));
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
void
render_hits (WINDOW * win, GDashModule * module_data, int y, int *x, int idx,
             int w, int selected)
{
   GDashStyle *style = module_style;
   GModule module = module_data->module;

   if (module_data->module == HOSTS && module_data->data[idx].is_subitem)
      goto inc;

   if (selected) {
      if (conf.color_scheme == MONOCHROME)
         init_pair (1, COLOR_BLACK, COLOR_WHITE);
      else
         init_pair (1, COLOR_BLACK, COLOR_GREEN);

      wattron (win, COLOR_PAIR (HIGHLIGHT));
      mvwhline (win, y, 0, ' ', w);
      mvwprintw (win, y, *x, "%d", module_data->data[idx].hits);
      wattroff (win, COLOR_PAIR (HIGHLIGHT));
   } else {
      wattron (win, COLOR_PAIR (style[module].color_hits));
      mvwprintw (win, y, *x, "%d", module_data->data[idx].hits);
      wattroff (win, COLOR_PAIR (style[module].color_hits));
   }
 inc:
   *x += module_data->hits_len + DASH_SPACE;
}

/* render dashboard content */
void
render_content (WINDOW * win, GDashModule * module_data, int *y, int *offset,
                int *total, GScrolling * scrolling)
{
   int expanded = 0, sel = 0, host_bars = 0;
   int i, j, x = 0, w, h;
   GModule module = module_data->module;

   if (!conf.skip_resolver)
      host_bars = 1;

#ifdef HAVE_LIBGEOIP
   host_bars = 1;
#endif

   getmaxyx (win, h, w);

   if (scrolling->expanded && module == scrolling->current)
      expanded = 1;

   int size = module_data->dash_size;
   for (i = *offset, j = 0; i < size; i++) {
      /* header */
      if ((i % size) == DASH_HEAD_POS) {
         draw_header (win, module_data->head, 0, (*y), w, 1);
         render_total_label (win, module_data, (*y));
         (*y)++;
      }
      /* description */
      else if ((i % size) == DASH_DESC_POS)
         draw_header (win, module_data->desc, 0, (*y)++, w, 2);
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
   int win_h, win_w;
   getmaxyx (win, win_h, win_w);
   win_w = win_w;

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

      /* Every module other than VISITORS, BROWSERS and OS
       * will use total req as base
       */
      switch (i) {
       case VISITORS:
       case BROWSERS:
       case OS:
          process = g_hash_table_size (ht_unique_visitors);
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
      if (y >= win_h)
         break;
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
int
regexp_init (regex_t * regex, const char *pattern)
{
   int y, x;
   getmaxyx (stdscr, y, x);

   char buf[REGEX_ERROR];
   int rc;
   rc = regcomp (regex, pattern, REG_EXTENDED | (find_t.icase ? REG_ICASE : 0));
   /* something went wrong */
   if (rc != 0) {
      regerror (rc, regex, buf, sizeof (buf));
      draw_header (stdscr, buf, 0, y - 1, x, WHITE_RED);
      refresh ();
      return 1;
   }
   return 0;
}

/* set search scrolling */
void
perform_find_dash_scroll (GScrolling * scrolling, GModule module)
{
   int *scroll, *offset;
   int exp_size = DASH_EXPANDED - DASH_NON_DATA;

   /* reset scrolling offsets if we are changing module */
   if (scrolling->current != module)
      reset_scroll_offsets (scrolling);

   scroll = &scrolling->module[module].scroll;
   offset = &scrolling->module[module].offset;

   (*scroll) = find_t.next_idx;
   if (*scroll >= exp_size && *scroll >= *offset + exp_size)
      (*offset) = (*scroll) < exp_size - 1 ? 0 : (*scroll) - exp_size + 1;

   scrolling->current = module;
   scrolling->dash = module * DASH_COLLAPSED;
   scrolling->expanded = 1;
   find_t.module = module;
}

/* find item within the given sub_list */
static int
find_next_sub_item (GSubList * sub_list, regex_t * regex)
{
   if (sub_list == NULL)
      goto out;

   GSubItem *iter;
   int i = 0, rc;

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
            draw_header (stdscr, buf, 0, y - 1, x, WHITE_RED);
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

   getmaxyx (stdscr, y, x);

   WINDOW *win = newwin (h, w, (y - h) / 2, (x - w) / 2);
   keypad (win, TRUE);
   wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');
   draw_header (win, FIND_HEAD, 1, 1, w - 2, 1);
   draw_header (win, FIND_DESC, 1, 2, w - 2, 2);

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

/* get bandwidth consumption for given key */
static unsigned long long
get_bandwidth (const char *key, GModule module)
{
   gpointer value_ptr;

   /* bandwidth modules */
   GHashTable *ht = NULL;
   switch (module) {
    case VISITORS:
       ht = ht_date_bw;
       break;
    case REQUESTS:
    case REQUESTS_STATIC:
    case NOT_FOUND:
       ht = ht_file_bw;
       break;
    case HOSTS:
       ht = ht_host_bw;
       break;
    default:
       ht = NULL;
   }

   if (ht == NULL)
      return 0;

   value_ptr = g_hash_table_lookup (ht, key);
   if (value_ptr != NULL)
      return (*(unsigned long long *) value_ptr);

   return 0;
}

/* get time taken to serve the request, in microseconds for given key */
static unsigned long long
get_serve_time (const char *key, GModule module)
{
   gpointer value_ptr;

   /* bandwidth modules */
   GHashTable *ht = NULL;
   switch (module) {
    case HOSTS:
       ht = ht_host_serve_usecs;
       break;
    case REQUESTS:
    case REQUESTS_STATIC:
    case NOT_FOUND:
       ht = ht_file_serve_usecs;
       break;
    default:
       ht = NULL;
   }

   if (ht == NULL)
      return 0;

   value_ptr = g_hash_table_lookup (ht, key);
   if (value_ptr != NULL)
      return *(unsigned long long *) value_ptr;
   return 0;
}

/* iterate over holder and get the key index.
 * return -1 if not found */
int
get_item_idx_in_holder (GHolder * holder, char *k)
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

/* Geolocation data */
char *
get_geoip_data (const char *data)
{
   const char *location = NULL;

#ifdef HAVE_LIBGEOIP
   const char *addr = data;
   /* Geolocation data */
   GeoIP *gi;
   gi = GeoIP_new (GEOIP_STANDARD);
   if (gi != NULL)
      location = GeoIP_country_name_by_name (gi, addr);
   if (location == NULL)
      location = "Location Unknown";
   if (gi != NULL)
      GeoIP_delete (gi);
#endif
   return (char *) location;
}

/* add a host item to holder */
static void
add_host_node (GHolder * h, int hits, char *data, unsigned long long bw,
               unsigned long long usecs)
{
   GSubList *sub_list = new_gsublist ();
   char *ip = xstrdup (data);
   gpointer value_ptr;
   gboolean found;

#ifdef HAVE_LIBGEOIP
   const char *addr = data;
   const char *location = NULL;
#endif

   h->items[h->idx].bw += bw;
   h->items[h->idx].hits += hits;
   h->items[h->idx].data = xstrdup (data);
   if (conf.serve_usecs)
      h->items[h->idx].usecs = usecs;
   h->items[h->idx].sub_list = sub_list;

#ifdef HAVE_LIBGEOIP
   location = get_geoip_data (addr);
   add_sub_item_back (sub_list, h->module, location, hits, bw);
   h->items[h->idx].sub_list = sub_list;
   h->sub_items_size++;
#endif

   pthread_mutex_lock (&gdns_thread.mutex);
   found = g_hash_table_lookup_extended (ht_hostnames, ip, NULL, &value_ptr);
   pthread_mutex_unlock (&gdns_thread.mutex);

   if (!found) {
      dns_resolver (ip);
   } else if (value_ptr) {
      add_sub_item_back (sub_list, h->module, (char *) value_ptr, hits, bw);
      h->items[h->idx].sub_list = sub_list;
      h->sub_items_size++;
   }
   free (ip);

   h->idx++;
}

/* add a browser item to holder */
static void
add_os_browser_node (GHolder * h, int hits, char *data, unsigned long long bw)
{
   GSubList *sub_list;
   char *type = NULL;
   int type_idx = -1;

   if (h->module == OS)
      type = verify_os (data, OPESYS_TYPE);
   else
      type = verify_browser (data, BROWSER_TYPE);

   type_idx = get_item_idx_in_holder (h, type);
   if (type_idx == -1) {
      h->items[h->idx].bw += bw;
      h->items[h->idx].hits += hits;
      h->items[h->idx].data = xstrdup (type);

      /* data (child) */
      sub_list = new_gsublist ();
      add_sub_item_back (sub_list, h->module, data, hits, bw);
      h->items[h->idx++].sub_list = sub_list;
      h->sub_items_size++;
   } else {
      sub_list = h->items[type_idx].sub_list;
      add_sub_item_back (sub_list, h->module, data, hits, bw);

      h->items[type_idx].sub_list = sub_list;
      h->items[type_idx].bw += bw;
      h->items[type_idx].hits += hits;
      h->sub_items_size++;
   }
   free (type);
}

/* add a status code item to holder */
static void
add_status_code_node (GHolder * h, int hits, char *data, unsigned long long bw)
{
   GSubList *sub_list;
   char *type = NULL, *status = NULL;
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
      add_sub_item_back (sub_list, h->module, status, hits, bw);
      h->items[h->idx++].sub_list = sub_list;
      h->sub_items_size++;
   } else {
      sub_list = h->items[type_idx].sub_list;
      add_sub_item_back (sub_list, h->module, status, hits, bw);

      h->items[type_idx].sub_list = sub_list;
      h->items[type_idx].bw += bw;
      h->items[type_idx].hits += hits;
      h->sub_items_size++;
   }
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
      if (module == OS || module == BROWSERS || module == HOSTS)
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
                module == STATUS_CODES)
               add_sub_item_to_dash (&dash, h->items[j], module, &i);
         }
         j++;
      }
   }
}

/* apply user defined sort */
void
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
   }
}

/* copy linked-list items to an array, sort, and move them back to the list */
/* should be faster than sorting the list */
void
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
         arr_items[j].data = (char *) iter->data;
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

/* load raw hash table's data into holder */
void
load_data_to_holder (GRawData * raw_data, GHolder * h, GModule module,
                     GSort sort)
{
   char *data;
   int hits, i;
   int size = 0;;
   unsigned long long bw = 0;

   size = raw_data->size;
   h->holder_size = size > MAX_CHOICES ? MAX_CHOICES : size;
   h->idx = 0;
   h->module = module;
   h->sub_items_size = 0;
   h->items = new_gholder_item (h->holder_size);

   for (i = 0; i < h->holder_size; i++) {
      bw = raw_data->items[i].bw;
      data = raw_data->items[i].data;
      hits = raw_data->items[i].hits;

      switch (module) {
       case OS:
       case BROWSERS:
          add_os_browser_node (h, hits, data, bw);
          break;
       case HOSTS:
          add_host_node (h, hits, data, bw, raw_data->items[i].usecs);
          break;
       case STATUS_CODES:
          add_status_code_node (h, hits, data, bw);
          break;
       default:
          h->items[h->idx].bw = bw;
          h->items[h->idx].data = xstrdup (data);
          h->items[h->idx].hits = hits;
          if (conf.serve_usecs)
             h->items[h->idx].usecs = raw_data->items[i].usecs;
          h->idx++;
      }
   }
   sort_holder_items (h->items, h->idx, sort);
   /* HOSTS module does not have "real" sub items, thus we don't include it */
   if (module == OS || module == BROWSERS || module == STATUS_CODES)
      sort_sub_list (h, sort);

   free_raw_data (raw_data);
}

/* iterate over the key/value pairs in the hash table */
static void
raw_data_iter (gpointer k, gpointer v, gpointer data_ptr)
{
   char *data;
   GRawData *raw_data = data_ptr;
   int hits;
   unsigned long long bw = 0;
   unsigned long long usecs = 0;

   hits = GPOINTER_TO_INT (v);
   data = (gchar *) k;
   bw = get_bandwidth (data, raw_data->module);

   raw_data->items[raw_data->idx].bw = bw;
   raw_data->items[raw_data->idx].data = data;
   raw_data->items[raw_data->idx].hits = hits;

   /* serve time in usecs */
   if (conf.serve_usecs) {
      usecs = get_serve_time (data, raw_data->module);
      raw_data->items[raw_data->idx].usecs = usecs / hits;
   }

   raw_data->idx++;
}

/* store the key/value pairs from a hash table into raw_data */
GRawData *
parse_raw_data (GHashTable * ht, int ht_size, GModule module)
{
   GRawData *raw_data;
   raw_data = new_grawdata ();

   raw_data->size = ht_size;
   raw_data->module = module;
   raw_data->idx = 0;
   raw_data->items = new_grawdata_item (ht_size);

   g_hash_table_foreach (ht, (GHFunc) raw_data_iter, raw_data);
   if (ht == ht_unique_vis)
      qsort (raw_data->items, ht_size, sizeof (GRawDataItem), cmp_data_desc);
   else
      qsort (raw_data->items, ht_size, sizeof (GRawDataItem), cmp_num_desc);

   return raw_data;
}
