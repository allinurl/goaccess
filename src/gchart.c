/**
 * gchart.c -- TUI chart implementation 
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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <ctype.h>

#include "gchart.h"
#include "ui.h"
#include "color.h"
#include "commons.h"
#include "gholder.h"
#include "settings.h"
#include "xmalloc.h"

#define round(x) ((x) < 0 ? (int)((x) - 0.5) : (int)((x) + 0.5))

/* Formatting helpers */

char *
format_number_abbrev (uint64_t val, char *buf, size_t len) {
  if (val >= 1000000000ULL) {
    snprintf (buf, len, "%" PRIu64 "G", (uint64_t) ((val + 500000000ULL) / 1000000000ULL));
  } else if (val >= 1000000ULL) {
    snprintf (buf, len, "%" PRIu64 "M", (uint64_t) ((val + 500000ULL) / 1000000ULL));
  } else if (val >= 1000ULL) {
    snprintf (buf, len, "%" PRIu64 "K", (uint64_t) ((val + 500ULL) / 1000ULL));
  } else {
    snprintf (buf, len, "%" PRIu64, val);
  }
  buf[4] = '\0';
  return buf;
}

static char *
format_bw_abbrev (uint64_t bytes, char *buf, size_t len) {
  uint64_t v = bytes;
  const char *s = "B";

  if (bytes >= (1ULL << 40)) {
    v = (bytes + (1ULL << 39)) >> 40;
    s = "T";
  } else if (bytes >= (1ULL << 30)) {
    v = (bytes + (1ULL << 29)) >> 30;
    s = "G";
  } else if (bytes >= (1ULL << 20)) {
    v = (bytes + (1ULL << 19)) >> 20;
    s = "M";
  } else if (bytes >= (1ULL << 10)) {
    v = (bytes + (1ULL << 9)) >> 10;
    s = "K";
  } else {
    v = bytes;
    s = "B";
  }

  if (v >= 1000) {
    snprintf (buf, len, "%" PRIu64, bytes);
  } else {
    snprintf (buf, len, "%" PRIu64 "%s", v, s);
  }
  buf[4] = '\0';
  return buf;
}

static char *
format_time_abbrev (uint64_t us, char *buf, size_t len) {
  uint64_t v = us;
  const char *s = "\u00b5s";

  if (us < 1000ULL) {
    snprintf (buf, len, "%" PRIu64 "%s", us, s);
  } else if (us < 1000000ULL) {
    v = (us + 500ULL) / 1000ULL;
    s = "ms";
    snprintf (buf, len, "%" PRIu64 "%s", v, s);
  } else if (us < 60000000ULL) {
    v = (us + 500000ULL) / 1000000ULL;
    s = "s";
    snprintf (buf, len, "%" PRIu64 "%s", v, s);
  } else if (us < 3600000000ULL) {
    v = (us + 30000000ULL) / 60000000ULL;
    s = "m";
    snprintf (buf, len, "%" PRIu64 "%s", v, s);
  } else if (us < 86400000000ULL) {
    v = (us + 1800000000ULL) / 3600000000ULL;
    s = "h";
    snprintf (buf, len, "%" PRIu64 "%s", v, s);
  } else {
    v = (us + 43200000000ULL) / 86400000000ULL;
    s = "d";
    snprintf (buf, len, "%" PRIu64 "%s", v, s);
  }
  buf[5] = '\0';
  return buf;
}

static char *
format_metric_value (uint64_t val, char *buf, size_t len, int metric_type) {
  switch (metric_type) {
  case CHART_METRIC_AVGTS:
  case CHART_METRIC_CUMTS:
  case CHART_METRIC_MAXTS:
    return format_time_abbrev (val, buf, len);
  case CHART_METRIC_BW:
    return format_bw_abbrev (val, buf, len);
  default:
    return format_number_abbrev (val, buf, len);
  }
}

static const char *
get_metric_name (int metric_type) {
  switch (metric_type) {
  case CHART_METRIC_HITS:
    return MTRC_HITS_LBL;
  case CHART_METRIC_VISITORS:
    return MTRC_VISITORS_LBL;
  case CHART_METRIC_BW:
    return MTRC_BW_LBL;
  case CHART_METRIC_AVGTS:
    return MTRC_AVGTS_LBL;
  case CHART_METRIC_CUMTS:
    return MTRC_CUMTS_LBL;
  case CHART_METRIC_MAXTS:
    return MTRC_MAXTS_LBL;
  default:
    return MTRC_HITS_LBL;
  }
}

/* Data helpers */

int
metric_has_data (GHolder *h, int metric_type) {
  int i, has = 0;
  GMetrics *m;
  GSubItem *sub;

  if (!h || h->idx == 0)
    return 0;

  for (i = 0; i < h->idx; i++) {
    m = h->items[i].metrics;
    if (m) {
      switch (metric_type) {
      case CHART_METRIC_HITS:
        if (m->hits > 0)
          has = 1;
        break;
      case CHART_METRIC_VISITORS:
        if (m->visitors > 0)
          has = 1;
        break;
      case CHART_METRIC_BW:
        if (m->bw.nbw > 0)
          has = 1;
        break;
      case CHART_METRIC_AVGTS:
        if (conf.serve_usecs && m->avgts.nts > 0)
          has = 1;
        break;
      case CHART_METRIC_CUMTS:
        if (conf.serve_usecs && m->cumts.nts > 0)
          has = 1;
        break;
      case CHART_METRIC_MAXTS:
        if (conf.serve_usecs && m->maxts.nts > 0)
          has = 1;
        break;
      default:
        if (m->hits > 0)
          has = 1;
        break;
      }
      if (has)
        return 1;
    }

    if (h->module != HOSTS && h->items[i].sub_list) {
      for (sub = h->items[i].sub_list->head; sub; sub = sub->next) {
        m = sub->metrics;
        if (m) {
          switch (metric_type) {
          case CHART_METRIC_HITS:
            if (m->hits > 0)
              return 1;
            break;
          case CHART_METRIC_VISITORS:
            if (m->visitors > 0)
              return 1;
            break;
          case CHART_METRIC_BW:
            if (m->bw.nbw > 0)
              return 1;
            break;
          case CHART_METRIC_AVGTS:
            if (conf.serve_usecs && m->avgts.nts > 0)
              return 1;
            break;
          case CHART_METRIC_CUMTS:
            if (conf.serve_usecs && m->cumts.nts > 0)
              return 1;
            break;
          case CHART_METRIC_MAXTS:
            if (conf.serve_usecs && m->maxts.nts > 0)
              return 1;
            break;
          default:
            if (m->hits > 0)
              return 1;
            break;
          }
        }
      }
    }
  }
  return 0;
}

int
get_available_metrics (GModule module, int metrics[CHART_METRIC_COUNT]) {
  int count = 0;
  const GOutput *out = output_lookup (module);
  if (!out)
    return 0;

  if (out->hits)
    metrics[count++] = CHART_METRIC_HITS;
  if (out->visitors)
    metrics[count++] = CHART_METRIC_VISITORS;
  if (out->bw && conf.bandwidth)
    metrics[count++] = CHART_METRIC_BW;
  if (out->avgts && conf.serve_usecs)
    metrics[count++] = CHART_METRIC_AVGTS;
  if (out->cumts && conf.serve_usecs)
    metrics[count++] = CHART_METRIC_CUMTS;
  if (out->maxts && conf.serve_usecs)
    metrics[count++] = CHART_METRIC_MAXTS;

  return count;
}

static uint64_t
get_metric_value (GMetrics *metrics, int metric_type) {
  if (!metrics)
    return 0;
  switch (metric_type) {
  case CHART_METRIC_HITS:
    return metrics->hits;
  case CHART_METRIC_VISITORS:
    return metrics->visitors;
  case CHART_METRIC_BW:
    return metrics->bw.nbw;
  case CHART_METRIC_AVGTS:
    return conf.serve_usecs ? metrics->avgts.nts : 0;
  case CHART_METRIC_CUMTS:
    return conf.serve_usecs ? metrics->cumts.nts : 0;
  case CHART_METRIC_MAXTS:
    return conf.serve_usecs ? metrics->maxts.nts : 0;
  default:
    return metrics->hits;
  }
}

/* Chart item flattening */

typedef struct {
  GMetrics *metrics;
  int is_subitem;
  int flat_idx;
} ChartItem;

static int
build_chart_items (GHolder *h, ChartItem **out) {
  int i, count = h->idx, idx = 0;
  ChartItem *items;
  GSubItem *sub;

  if (h->module != HOSTS)
    count += h->sub_items_size;

  items = xcalloc (count, sizeof (ChartItem));

  for (i = 0; i < h->idx; i++) {
    items[idx] = (ChartItem) {
    .metrics = h->items[i].metrics,.is_subitem = 0,.flat_idx = idx};
    idx++;

    if (h->module != HOSTS && h->items[i].sub_list) {
      for (sub = h->items[i].sub_list->head; sub; sub = sub->next) {
        items[idx] = (ChartItem) {
        .metrics = sub->metrics,.is_subitem = 1,.flat_idx = idx};
        idx++;
      }
    }
  }

  *out = items;
  return count;
}

static uint64_t
apply_log_scale (uint64_t value, uint64_t max_value) {
  double log_val, log_max;
  if (value == 0 || max_value == 0)
    return 0;
  log_val = log10 ((double) value + 1.0);
  log_max = log10 ((double) max_value + 1.0);
  if (log_max == 0)
    return value;
  return (uint64_t) ((log_val / log_max) * max_value);
}

/* Drawing primitives */
static void
draw_vbar (ChartDrawCtx *ctx, int x, int filled, GColors *color, int is_selected,
           uint64_t actual_val) {
  WINDOW *w = ctx->win;
  int y_top = ctx->y_chart_start;
  int width = ctx->bar_width;
  int i, y, ly, lx;
  char buf[16];
  GColors *val_col;

  for (i = 0; i < ctx->bar_height; i++)
    mvwhline (w, y_top + i, x, ' ', width);

  if (is_selected) {
    /* Use COLOR_SELECTED colors + underline + bold for selected bar */
    GColors *sel = get_color (COLOR_SELECTED);
    wattron (w, COLOR_PAIR (sel->pair->idx) | A_BOLD | A_UNDERLINE);
  } else {
    /* Normal bar: original color + reverse */
    wattron (w, COLOR_PAIR (color->pair->idx) | color->attr | A_REVERSE);
  }

  /* Draw filled part */
  for (i = 0; i < filled; i++) {
    y = y_top + ctx->bar_height - 1 - i;
    mvwhline (w, y, x, ' ', width);
  }

  /* Turn off what we turned on */
  wattroff (w, COLOR_PAIR (color->pair->idx) | color->attr | A_REVERSE | A_BOLD | A_UNDERLINE);

  /* Value label (unchanged) */
  if (actual_val > 0 && ctx->show_bar_values) {
    format_metric_value (actual_val, buf, sizeof (buf), ctx->metric_type);
    y = y_top + ctx->bar_height - filled;
    ly = y - 1;
    if (ly < y_top - 1)
      ly = y_top - 1;

    val_col = get_color (COLOR_CHART_VALUES);
    wattron (w, COLOR_PAIR (val_col->pair->idx) | val_col->attr);
    if (is_selected)
      wattron (w, A_BOLD);
    lx = x + (width - (int) strlen (buf)) / 2;
    if (lx < x)
      lx = x;
    mvwprintw (w, ly, lx, "%s", buf);
    wattroff (w, COLOR_PAIR (val_col->pair->idx) | val_col->attr);
    if (is_selected)
      wattroff (w, A_BOLD);
  }
}

static void
draw_axes (ChartDrawCtx *ctx) {
  WINDOW *w;
  int ys, xs, h, i, yt, lx;
  GColors *axis;
  uint64_t maxv;
  char buf[16];
  const int ticks = 4;

  if (!ctx->show_axes)
    return;

  w = ctx->win;
  ys = ctx->y_chart_start;
  xs = ctx->x_start;
  h = ctx->bar_height;
  axis = get_color (COLOR_CHART_AXIS);
  maxv = ctx->display_max;

  wattron (w, axis->attr | COLOR_PAIR (axis->pair->idx));

  for (i = 0; i <= h; i++)
    mvwaddch (w, ys + i, xs - 2, '|');

  for (i = 0; i < getmaxx (w) - xs - 2; i++)
    mvwaddch (w, ys + h, xs + i, '-');

  for (i = 0; i <= ticks; i++) {
    uint64_t v = (maxv * (uint64_t) i + ticks / 2) / ticks;
    yt = ys + h - ((v * (int64_t) h + maxv / 2) / maxv);
    format_metric_value (v, buf, sizeof (buf), ctx->metric_type);
    lx = xs - 3 - (int) strlen (buf);
    if (lx < 0)
      lx = 0;
    mvwprintw (w, yt, lx, "%s", buf);
    mvwaddch (w, yt, xs - 2, '+');
  }

  wattroff (w, axis->attr | COLOR_PAIR (axis->pair->idx));
}

static void
draw_chart_indicator (ChartDrawCtx *ctx) {
  char ind[96], mname[64];
  GColors *col = get_color (COLOR_CHART_AXIS);

  strncpy (mname, get_metric_name (ctx->metric_type), sizeof (mname) - 1);
  mname[sizeof (mname) - 1] = '\0';
  for (char *p = mname; *p; p++)
    *p = toupper ((unsigned char) *p);

  snprintf (ind, sizeof (ind), "[%s:%s%s]",
            mname, ctx->use_log_scale ? "LOG" : "LINEAR", ctx->reverse_bars ? ":REV" : "");

  wattron (ctx->win, col->attr | COLOR_PAIR (col->pair->idx));
  mvwprintw (ctx->win, ctx->y_start, 1, "%s", ind);
  wattroff (ctx->win, col->attr | COLOR_PAIR (col->pair->idx));
}

/* Window & selection helpers */

static int
get_chart_selected_root (const ChartDrawCtx *ctx) {
  int cum = 0, j = 0, subsz = 0;
  if (ctx->selected_idx < 0)
    return -1;
  if (ctx->holder->module != HOSTS)
    return ctx->selected_idx;

  for (j = 0; j < ctx->holder->idx; j++) {
    subsz = ctx->holder->items[j].sub_list ? ctx->holder->items[j].sub_list->size : 0;
    if (ctx->selected_idx == cum)
      return j;
    cum += 1 + subsz;
  }
  return -1;
}

static void
compute_bar_window (ChartDrawCtx *ctx, int num_items) {
  int start = ctx->scroll_offset;
  int vis = ctx->visible_bars;
  int sel = get_chart_selected_root (ctx);

  if (sel >= 0 && num_items > vis) {
    if (sel < start)
      start = sel;
    else if (sel >= start + vis)
      start = sel - vis + 1;
  }

  if (start < 0)
    start = 0;
  if (num_items > vis && start > num_items - vis)
    start = num_items - vis;
  if (num_items <= vis)
    start = 0;

  ctx->start_bar = start;
  ctx->end_bar = start + vis;
  if (ctx->end_bar > num_items)
    ctx->end_bar = num_items;
}

static uint64_t
compute_local_max (ChartItem *items, const ChartDrawCtx *ctx) {
  uint64_t mx = 0;
  for (int i = ctx->start_bar; i < ctx->end_bar; i++) {
    uint64_t v = get_metric_value (items[i].metrics, ctx->metric_type);
    if (v > mx)
      mx = v;
  }
  return mx;
}

static void
compute_chart_window (ChartDrawCtx *ctx, ChartItem *items) {
  compute_bar_window (ctx, ctx->num_items);
  ctx->display_max = compute_local_max (items, ctx);
}

static void
draw_chart_bars (ChartDrawCtx *ctx, ChartItem *items) {
  GColors *bar_c = get_color (COLOR_BARS), *c = NULL;
  GColors *sub_c = get_color (COLOR_SUBBARS);
  int sel_root = get_chart_selected_root (ctx);
  int i, x, data_idx, vis_pos, is_sel;
  uint64_t val, dval;
  int filled;

  for (i = ctx->start_bar; i < ctx->end_bar; i++) {
    data_idx = ctx->reverse_bars ? (ctx->end_bar - 1 - (i - ctx->start_bar)) : i;
    if (data_idx >= ctx->num_items)
      continue;

    val = get_metric_value (items[data_idx].metrics, ctx->metric_type);
    dval = ctx->use_log_scale ? apply_log_scale (val, ctx->display_max) : val;

    filled = (int) ((dval * (int64_t) ctx->bar_height + ctx->display_max / 2) / ctx->display_max);
    if (filled > ctx->bar_height)
      filled = ctx->bar_height;

    vis_pos = i - ctx->start_bar;
    x = ctx->x_start + vis_pos * (ctx->bar_width + ctx->bar_gap);

    is_sel = (items[data_idx].flat_idx == sel_root);
    c = items[data_idx].is_subitem ? sub_c : bar_c;

    draw_vbar (ctx, x, filled, c, is_sel, ctx->show_bar_values ? val : 0);
  }
}

static void
draw_chart_caret (ChartDrawCtx *ctx, ChartItem *items) {
  int sel_root = get_chart_selected_root (ctx);
  if (sel_root < 0)
    return;

  for (int i = ctx->start_bar; i < ctx->end_bar; i++) {
    int didx = ctx->reverse_bars ? (ctx->end_bar - 1 - (i - ctx->start_bar)) : i;
    if (didx >= ctx->num_items)
      continue;

    if (items[didx].flat_idx == sel_root) {
      int pos = i - ctx->start_bar;
      int x = ctx->x_start + pos * (ctx->bar_width + ctx->bar_gap) + ctx->bar_width / 2;
      mvwaddch (ctx->win, ctx->y_start + ctx->chart_height, x, '^');
      break;
    }
  }
}

static void
check_top_padding (ChartDrawCtx *ctx, ChartItem *items) {
  ctx->needs_top_padding = 0;
  for (int i = ctx->start_bar; i < ctx->end_bar; i++) {
    uint64_t v = get_metric_value (items[i].metrics, ctx->metric_type);
    uint64_t dv = ctx->use_log_scale ? apply_log_scale (v, ctx->display_max) : v;
    int f = (int) ((dv * (int64_t) ctx->bar_height + ctx->display_max / 2) / ctx->display_max);
    if (f >= ctx->bar_height) {
      ctx->needs_top_padding = 1;
      break;
    }
  }
}

/* Main function */

void
draw_panel_chart (WINDOW *win, GHolder *h, const ChartDrawCtx *user_opts) {
  uint64_t global_max = 0, v = 0;
  int i = 0, y = 0, height = 0, screen_w = 0;
  ChartDrawCtx ctx = *user_opts;
  ChartItem *items = NULL;
  ctx.win = win;
  ctx.holder = h;
  ctx.x_start = ctx.show_axes ? 8 : 2;

  ctx.num_items = build_chart_items (h, &items);
  if (ctx.num_items == 0) {
    free (items);
    return;
  }

  /* Early exit if no data for this metric */
  for (i = 0; i < ctx.num_items; i++) {
    v = get_metric_value (items[i].metrics, ctx.metric_type);
    if (v > global_max)
      global_max = v;
  }
  if (global_max == 0) {
    free (items);
    return;
  }

  draw_chart_indicator (&ctx);

  y = ctx.y_start + 1;
  height = ctx.chart_height - 1;
  ctx.bar_height = height - 1;

  screen_w = getmaxx (win) - ctx.x_start - 2;
  if (ctx.auto_width) {
    ctx.bar_gap = 1;
    ctx.bar_width = (screen_w - (ctx.num_items - 1) * ctx.bar_gap) / ctx.num_items;
    if (ctx.bar_width < 1)
      ctx.bar_width = 1;
  } else {
    ctx.bar_width = BAR_W;
    ctx.bar_gap = BAR_GAP;
  }

  ctx.visible_bars = screen_w / (ctx.bar_width + ctx.bar_gap);
  if (ctx.visible_bars < 1)
    ctx.visible_bars = 1;

  compute_chart_window (&ctx, items);
  check_top_padding (&ctx, items);
  ctx.y_chart_start = y + (ctx.needs_top_padding ? 1 : 0);

  draw_chart_bars (&ctx, items);
  draw_axes (&ctx);
  draw_chart_caret (&ctx, items);

  free (items);
}
