/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2025 Gerardo Orellana <hello @ goaccess.io>
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef GDASHBOARD_H_INCLUDED
#define GDASHBOARD_H_INCLUDED

#include <stdint.h>

#include "ui.h"

/* IMPORTANT: These defines are BASE VALUES used for calculation.
 * Actual positions are calculated dynamically in render_content() */

#define DASH_HEAD_POS    0 /* position of header line */
#define DASH_EMPTY_POS   1 /* empty line position */

/* Chart positioning - these are fixed */
#define DASH_CHART_START 2 /* first chart row */
#define DASH_CHART_END   (DASH_CHART_START + DASH_CHART_HEIGHT - 1) /* last chart row */

/* These are reference values - actual positions calculated dynamically */
#define DASH_BLANK1_POS  (DASH_CHART_END + 1) /* blank after chart (for caret) */
#define DASH_BLANK2_POS  (DASH_CHART_END + 2) /* blank before cols (if cols enabled) */
#define DASH_COLS_POS    (DASH_CHART_END + 3) /* position of column names (if enabled) */
#define DASH_DASHES_POS  (DASH_CHART_END + 4) /* position of dashes (if enabled) */
#define DASH_DATA_POS    (DASH_CHART_END + 5) /* data line position (with cols) */

/* Helper macros for calculations */
#define DASH_NON_DATA    (DASH_DATA_POS) /* number of rows without data stats */
#define DASH_COL_ROWS    2 /* number of rows for column values + dashed lines */

/* Panel sizes - these determine how many rows each panel occupies
 * Structure:
 * With columns:    header + blank + chart(7) + blank + blank + cols + dashes + data(N) + blank = 13 + N
 * Without columns: header + blank + chart(7) + blank + data(N) + blank = 11 + N
 */
#define DASH_COLLAPSED   (DASH_DATA_POS + 8 + 1) /* +1 for trailing blank */
#define DASH_EXPANDED    (DASH_DATA_POS + 28 + 1) /* +1 for trailing blank */

/* Other constants */
#define DASH_INIT_X      1 /* start position (x-axis) */
#define DASH_BW_LEN      11 /* max bandwidth string length */
#define DASH_SRV_TM_LEN  9 /* max time served length */
#define DASH_SPACE       1 /* space between columns (metrics) */

typedef struct {
  int header_pos;
  int blank_after_header;
  int chart_start;
  int chart_end;
  int blank_for_caret;
  int blank_before_cols;
  int cols_header_pos;
  int cols_dashes_pos;
  int data_start;
  int data_end;
} PanelLayout;

/* Common render data line fields */
typedef struct GDashRender_ {
  WINDOW *win;
  int y;
  int w;
  int idx;
  int sel;
} GDashRender;

/* Dashboard panel item */
typedef struct GDashData_ {
  GMetrics *metrics;
  short is_subitem;
  short has_children;           /* 1 if this node has sub-items */
  int node_full_idx;            /* index into node_expanded[] (full DFS position) */
} GDashData;

/* Dashboard panel meta data */
typedef struct GDashMeta_ {
  uint64_t max_hits;            /* maximum value on the hits column */
  uint64_t max_visitors;        /* maximum value on the visitors column */
  uint64_t max_bw;

  /* determine the maximum metric's length of these metrics */
  /* for instance, 1022 is the max value for the hits column and its length = 4 */
  int hits_len;
  int hits_perc_len;
  int visitors_len;
  int visitors_perc_len;
  int bw_len;
  int bw_perc_len;
  int avgts_len;
  int cumts_len;
  int maxts_len;
  int method_len;
  int protocol_len;
  int data_len;
} GDashMeta;

/* Dashboard panel */
typedef struct GDashModule_ {
  GDashData *data;              /* data metrics */
  GModule module;               /* module */
  GDashMeta meta;               /* meta data */

  const char *head;             /* panel header */
  const char *desc;             /* panel description */

  int alloc_data;               /* number of data items allocated. */
  /* e.g., MAX_CHOICES or holder size */
  int dash_size;                /* dashboard size */
  int holder_size;              /* hash table size  */
  int ht_size;                  /* hash table size  */
  int idx_data;                 /* idx data */

  unsigned short pos_y;         /* dashboard current Y position */
} GDashModule;

/* Dashboard */
typedef struct GDash_ {
  int total_alloc;              /* number of allocated dashboard lines */
  GDashModule module[TOTAL_MODULES];
} GDash;

/* Function Prototypes */
GDashData *new_gdata (uint32_t size);
GDash *new_gdash (void);
int get_num_collapsed_data_rows (void);
int get_num_expanded_data_rows (void);
int perform_next_find (GHolder * h, GScroll * scroll);
int render_find_dialog (WINDOW * main_win, GScroll * scroll);
int set_module_from_mouse_event (GScroll * scroll, GDash * dash, int y);
uint32_t get_ht_size_by_module (GModule module);
void display_content (WINDOW * win, GDash * dash, GScroll * gscroll, GHolder * holder);
void free_dashboard (GDash * dash);
void load_data_to_dash (GHolder * h, GDash * dash, GModule module, GScroll * scroll);
void reset_find (void);
void reset_scroll_offsets (GScroll * scroll);

#endif
