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
