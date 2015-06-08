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

GStorage *
new_gstorage (uint32_t size)
{
  GStorage *store = xcalloc (size, sizeof (GStorage));
  return store;
}

GMetrics *
new_gmetrics (void)
{
  GMetrics *metrics = xcalloc (1, sizeof (GMetrics));

  return metrics;
}

GStorageMetrics *
new_ht_metrics (void)
{
  GStorageMetrics *metrics = xmalloc (sizeof (GStorageMetrics));

  memset (metrics, 0, sizeof *metrics);

  /* maps */
  metrics->keymap = NULL;
  metrics->datamap = NULL;
  metrics->rootmap = NULL;
  metrics->uniqmap = NULL;

  /* metrics */
  metrics->hits = NULL;
  metrics->visitors = NULL;
  metrics->bw = NULL;
  metrics->time_served = NULL;
  metrics->protocols = NULL;
  metrics->methods = NULL;
  metrics->agents = NULL;

  return metrics;
}

GStorageMetrics *
get_storage_metrics_by_module (GModule module)
{
  return ht_storage[module].metrics;
}

int *
int2ptr (int val)
{
  int *ptr = xmalloc (sizeof (int));
  *ptr = val;
  return ptr;
}

uint64_t *
uint642ptr (uint64_t val)
{
  uint64_t *ptr = xmalloc (sizeof (uint64_t));
  *ptr = val;
  return ptr;
}

void *
get_storage_metric_by_module (GModule module, GMetric metric)
{
  void *ht;
  GStorageMetrics *metrics;

  metrics = get_storage_metrics_by_module (module);
  switch (metric) {
  case MTRC_KEYMAP:
    ht = metrics->keymap;
    break;
  case MTRC_ROOTMAP:
    ht = metrics->rootmap;
    break;
  case MTRC_DATAMAP:
    ht = metrics->datamap;
    break;
  case MTRC_UNIQMAP:
    ht = metrics->uniqmap;
    break;
  case MTRC_HITS:
    ht = metrics->hits;
    break;
  case MTRC_VISITORS:
    ht = metrics->visitors;
    break;
  case MTRC_BW:
    ht = metrics->bw;
    break;
  case MTRC_TIME_SERVED:
    ht = metrics->time_served;
    break;
  case MTRC_METHODS:
    ht = metrics->methods;
    break;
  case MTRC_PROTOCOLS:
    ht = metrics->protocols;
    break;
  case MTRC_AGENTS:
    ht = metrics->agents;
    break;
  default:
    ht = NULL;
  }

  return ht;
}

void
set_data_metrics (GMetrics * ometrics, GMetrics ** nmetrics, int processed)
{
  GMetrics *metrics;
  float percent = get_percentage (processed, ometrics->hits);

  metrics = new_gmetrics ();
  metrics->bw.nbw = ometrics->bw.nbw;
  metrics->id = ometrics->id;
  metrics->data = ometrics->data;
  metrics->hits = ometrics->hits;
  metrics->percent = percent < 0 ? 0 : percent;
  metrics->visitors = ometrics->visitors;

  if (conf.serve_usecs && ometrics->hits > 0)
    metrics->avgts.nts = ometrics->avgts.nts;

  if (conf.append_method && ometrics->method)
    metrics->method = ometrics->method;

  if (conf.append_method && ometrics->protocol)
    metrics->protocol = ometrics->protocol;

  *nmetrics = metrics;
}

void *
get_storage_metric (GModule module, GMetric metric)
{
  return get_storage_metric_by_module (module, metric);
}
