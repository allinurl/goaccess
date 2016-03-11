/**
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef OUTPUT_H_INCLUDED
#define OUTPUT_H_INCLUDED

#define OUTPUT_N  10
#define MAX_PLOTS 5

#define REP_TITLE "Server Statistics"

#include "commons.h"
#include "parser.h"

/* Enumerated chart types */
typedef enum GChartType_
{
  CHART_NONE,
  CHART_VBAR,
  CHART_AREASPLINE,
} GChartType;

/* Metric definition . */
typedef struct GDefMetric_
{
  const char *key;
  const char *lbl;
  const char *vtype;
  const char *meta;
  const char *cname;
  const char *cwidth;
} GDefMetric;

typedef struct GHTMLPlot_
{
  void (*plot) (FILE * fp, int isp);
} GHTMLPlot;
/* Controls HTML output. */
typedef struct GHTML_
{
  GModule module;
  GChartType chart_type;
  int8_t chart_reverse;
  int8_t table;
  GHTMLPlot plot[MAX_PLOTS];
} GHTML;

void output_html (GLog * logger, GHolder * holder);

#endif
