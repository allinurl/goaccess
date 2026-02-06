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

#ifndef OUTPUT_H_INCLUDED
#define OUTPUT_H_INCLUDED

#define MAX_PLOTS 5 /* number of metrics we can plot */
#define FILENAME_JS  "goaccess.js"
#define FILENAME_CSS "goaccess.css"
#define FAVICON_BASE64 "AAABAAEAEBAQAAEABAAoAQAAFgAAACgAAAAQAAAAIAAAAAEABAAAAAAAgAAAAAAAAAAAAAAAEAAAAAAAAADGxsYAWFhYABwcHABfAP8A/9dfAADXrwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIiIiIiIiIiIjMlUkQgAiIiIiIiIiIiIiIzJVJEIAAAIiIiIiIiIiIiMyVSRCAAIiIiIiIiIiIiIRERERERERERERERERERERIiIiIiIiIiIgACVVUiIiIiIiIiIiIiIiIAAlVVIiIiIiIiIiIiIiIhEREREREREREREREREREREAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
#define APPLE_TOUCH_ICON_BASE64 "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"

#include "commons.h"
#include "parser.h"

/* Enumerated chart types */
typedef enum GChartType_ {
  CHART_NONE,
  CHART_VBAR,
  CHART_AREASPLINE,
  CHART_WMAP,
  CHART_GMAP,
} GChartType;

/* Chart axis structure */
typedef struct GChartDef_ {
  const char *key;
  const char *value;
} GChartDef;

/* Chart axis structure */
typedef struct GChart_ {
  const char *key;
  GChartDef *def;
} GChart;

/* Chart behavior */
typedef struct GHTMLPlot_ {
  GChartType chart_type;
  void (*plot) (FILE * fp, struct GHTMLPlot_ plot, int sp);
  int8_t chart_reverse;
  int8_t redraw_expand;
  char *chart_key;
  char *chart_lbl;
} GHTMLPlot;

/* Controls HTML panel output. */
typedef struct GHTML_ {
  GModule module;
  int8_t table;
  int8_t has_map;
  void (*metrics) (FILE * fp, const struct GHTML_ * def, int sp);
  GHTMLPlot chart[MAX_PLOTS];
} GHTML;

/* Metric definition . */
typedef struct GDefMetric_ {
  const char *cname;            /* metric class name */
  const char *cwidth;           /* metric column width */
  const char *datakey;          /* metric JSON data key */
  const char *datatype;         /* metric data value type */
  const char *lbl;              /* metric label (column name) */
  const char *metakey;          /* metric JSON meta key */
  const char *metatype;         /* metric meta value type */
  const char *metalbl;          /* metric meta value label */
  const char *hlregex;          /* highlight regex value */
} GDefMetric;

void output_html (GHolder * holder, const char *filename);

#endif
