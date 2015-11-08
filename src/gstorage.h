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

#ifndef GSTORAGE_H_INCLUDED
#define GSTORAGE_H_INCLUDED

#include "commons.h"

/* Total number of storage metrics (GSMetric) */
#define GSMTRC_TOTAL 13

/* Enumerated Storage Metrics */
typedef enum GSMetric_
{
  MTRC_KEYMAP,
  MTRC_ROOTMAP,
  MTRC_DATAMAP,
  MTRC_UNIQMAP,
  MTRC_ROOT,
  MTRC_HITS,
  MTRC_VISITORS,
  MTRC_BW,
  MTRC_CUMTS,
  MTRC_MAXTS,
  MTRC_METHODS,
  MTRC_PROTOCOLS,
  MTRC_AGENTS,
} GSMetric;

GMetrics *new_gmetrics (void);

int *int2ptr (int val);
uint64_t *uint642ptr (uint64_t val);

void *get_storage_metric_by_module (GModule module, GSMetric metric);
void *get_storage_metric (GModule module, GSMetric metric);
void set_data_metrics (GMetrics * ometrics, GMetrics ** nmetrics,
                       GPercTotals totals);

#endif // for #ifndef GSTORAGE_H
