/**
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef GDASHBOARD_H_INCLUDED
#define GDASHBOARD_H_INCLUDED

#include <stdint.h>

#include "ui.h"

/* *INDENT-OFF* */
#define DASH_HEAD_POS    0  /* position of header line */
#define DASH_EMPTY_POS   1  /* empty line position */
#define DASH_COLS_POS    2  /* position of column names */
#define DASH_DASHES_POS  3  /* position of dashes under column names */
#define DASH_DATA_POS    4  /* data line position */

#define DASH_NON_DATA    5  /* number of rows without data stats */
#define DASH_COL_ROWS    2  /* number of rows for column values + dashed lines */

#define DASH_COLLAPSED   12 /* number of rows per panel (collapsed) */
#define DASH_EXPANDED    32 /* number of rows per panel (expanded) */

#define DASH_INIT_X      1  /* start position (x-axis) */

#define DASH_BW_LEN      11 /* max bandwidth string length, e.g., 151.69 MiB */
#define DASH_SRV_TM_LEN  9  /* max time served length, e.g., 483.00 us */
#define DASH_SPACE       1  /* space between columns (metrics) */

#define COLUMN_HITS_LEN  4  /* column header name length */
#define COLUMN_VIS_LEN   4  /* column header name length */

/* Render holder */
typedef struct GDashRender_
{
  WINDOW *win;
  int y;
  int w;
  int idx;
  int sel;
} GDashRender;

/* Dashboard panel item */
typedef struct GDashData_
{
  GMetrics *metrics;
  short is_subitem;
} GDashData;

/* Dashboard panel */
typedef struct GDashModule_
{
  GDashData *data;
  GModule module;
  const char *head;
  const char *desc;

  int alloc_data;  /* alloc data items */
  int dash_size;   /* dashboard size   */
  int data_len;
  int hits_len;
  int holder_size; /* hash table size  */
  int ht_size;     /* hash table size  */
  int idx_data;    /* idx data         */
  int max_hits;
  int method_len;
  int perc_len;
  int visitors_len;
  unsigned short pos_y;
} GDashModule;

/* Dashboard */
typedef struct GDash_
{
  int total_alloc;
  GDashModule module[TOTAL_MODULES];
} GDash;

/* Function Prototypes */
GDashData *new_gdata (uint32_t size);
GDash *new_gdash (void);
int get_num_collapsed_data_rows(void);
int get_num_expanded_data_rows(void);
int perform_next_find (GHolder * h, GScroll * scroll);
int render_find_dialog (WINDOW * main_win, GScroll * scroll);
int set_module_from_mouse_event (GScroll *scroll, GDash *dash, int y);
uint32_t get_ht_size_by_module (GModule module);
void display_content (WINDOW * win, GLog * logger, GDash * dash, GScroll * scroll);
void free_dashboard (GDash * dash);
void load_data_to_dash (GHolder * h, GDash * dash, GModule module, GScroll * scroll);
void reset_find (void);
void reset_scroll_offsets (GScroll * scroll);

/* *INDENT-ON* */

#endif
