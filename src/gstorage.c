/**
 * gstorage.c -- common storage handling
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

#include <stdio.h>
#if !defined __SUNPRO_C
#include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "gstorage.h"

#include "error.h"
#include "xmalloc.h"

/* Allocate memory for a new GMetrics instance.
 *
 * On success, the newly allocated GMetrics is returned . */
GMetrics *
new_gmetrics (void)
{
  GMetrics *metrics = xcalloc (1, sizeof (GMetrics));

  return metrics;
}

/* Allocate space off the heap to store an int.
 *
 * On success, the newly allocated pointer is returned . */
int *
int2ptr (int val)
{
  int *ptr = xmalloc (sizeof (int));
  *ptr = val;

  return ptr;
}

/* Allocate space off the heap to store a uint64_t.
 *
 * On success, the newly allocated pointer is returned . */
uint64_t *
uint642ptr (uint64_t val)
{
  uint64_t *ptr = xmalloc (sizeof (uint64_t));
  *ptr = val;

  return ptr;
}

/* Set numeric metrics for each request given raw data.
 *
 * On success, numeric metrics are set into the given structure. */
void
set_data_metrics (GMetrics * ometrics, GMetrics ** nmetrics, GPercTotals totals)
{
  GMetrics *metrics;

  /* determine percentages for certain fields */
  float hits_perc = get_percentage (totals.hits, ometrics->hits);
  float visitors_perc = get_percentage (totals.visitors, ometrics->visitors);
  float bw_perc = get_percentage (totals.bw, ometrics->bw.nbw);

  metrics = new_gmetrics ();

  /* basic fields */
  metrics->id = ometrics->id;
  metrics->hits = ometrics->hits;
  metrics->visitors = ometrics->visitors;

  /* percentage fields */
  metrics->hits_perc = hits_perc < 0 ? 0 : hits_perc;
  metrics->bw_perc = bw_perc < 0 ? 0 : bw_perc;
  metrics->visitors_perc = visitors_perc < 0 ? 0 : visitors_perc;

  /* bandwidth field */
  metrics->bw.nbw = ometrics->bw.nbw;

  /* time served fields */
  if (conf.serve_usecs && ometrics->hits > 0) {
    metrics->avgts.nts = ometrics->avgts.nts;
    metrics->cumts.nts = ometrics->cumts.nts;
    metrics->maxts.nts = ometrics->maxts.nts;
  }

  /* protocol field */
  if (conf.append_method && ometrics->method)
    metrics->method = ometrics->method;

  /* method field */
  if (conf.append_method && ometrics->protocol)
    metrics->protocol = ometrics->protocol;

  /* data field */
  metrics->data = ometrics->data;

  *nmetrics = metrics;
}
