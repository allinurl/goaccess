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

#include "commons.h"
#include "parser.h"

#define GO_LOGO "" \
 "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" \
 "8H8BM/XVggoDbGIAAAAASUVORK5CYII="

typedef struct GOutput_
{
  GModule module;
  void (*render) (FILE * fp, GHolder * h, int processed, int max_hit,
                  int max_vis, const struct GOutput_ *);
  void (*metrics_callback) (GMetrics * metrics);
  const char *clabel;           /* column label */
  int8_t visitors;
  int8_t hits;
  int8_t percent;
  int8_t bw;
  int8_t avgts;
  int8_t protocol;
  int8_t method;
  int8_t data;
  int8_t graph;
  int8_t sub_graph;
} GOutput;

void output_html (GLog * logger, GHolder * holder);

#endif
