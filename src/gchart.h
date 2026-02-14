/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2026 Gerardo Orellana <hello @ goaccess.io>
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

#ifndef GCHART_H_INCLUDED
#define GCHART_H_INCLUDED

#include <ncurses.h>
#include "commons.h"

/* Forward declarations */
typedef struct GHolder_ GHolder;

#define DASH_CHART_HEIGHT 7
#define BAR_W 5
#define BAR_GAP 1

typedef enum {
  CHART_METRIC_HITS = 0,
  CHART_METRIC_VISITORS,
  CHART_METRIC_BW,
  CHART_METRIC_AVGTS,
  CHART_METRIC_CUMTS,
  CHART_METRIC_MAXTS,
  CHART_METRIC_COUNT
} GChartMetric;

/* All drawing options + computed state in one place */
typedef struct {
  WINDOW *win;
  GHolder *holder;
  int y_start;
  int chart_height;
  int auto_width;
  int selected_idx;
  int scroll_offset;
  int show_axes;
  int show_bar_values;
  int metric_type;
  int use_log_scale;
  int reverse_bars;

  /* Computed values */
  int x_start;
  int bar_width;
  int bar_gap;
  int visible_bars;
  int start_bar;
  int end_bar;
  uint64_t display_max;
  int bar_height;
  int y_chart_start;
  int needs_top_padding;
  int num_items;                /* total number of chart items */

  /* Per-item expand state for filtering visible chart bars */
  const uint8_t *item_expanded; /* NULL = all expanded, else per-root-item state */
  int item_expanded_size;       /* number of entries in item_expanded */
} ChartDrawCtx;

/* Main drawing function */
void draw_panel_chart (WINDOW * win, GHolder * h, const ChartDrawCtx * opts);

int metric_has_data (GHolder * h, int metric_type);
int get_available_metrics (GModule module, int metrics[CHART_METRIC_COUNT]);
char *format_number_abbrev (uint64_t val, char *buf, size_t len);

#endif
