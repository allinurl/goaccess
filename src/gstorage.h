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

typedef struct GStorageMetrics_
{
  /* Maps keys (string) to numeric values (integer).
   * This mitigates the issue of having multiple stores
   * with the same string key, and therefore, avoids unnecessary
   * memory usage (for most cases).
   * HEAD|/index.php   -> 1
   * POST|/index.php   -> 2
   * POST|Windows XP   -> 3
   * POST|Ubuntu 10.10 -> 4
   * GET|Ubuntu 10.10  -> 5
   * Linux             -> 6
   * 26/Dec/2014       -> 7
   * Windows           -> 8
   */
  void *keymap;

  /* Maps numeric keys to actual key values (strings), e.g.,
   * 1 -> /index.php
   * 2 -> /index.php
   * 3 -> Windows XP
   * 4 -> Ubuntu 10.10
   * 5 -> Ubuntu 10.10
   * 7 -> 26/Dec/2014
   */
  void *datamap;

  /* Maps numeric keys of root elements to actual
   * key values (strings), e.g.,
   * 6 -> Linux
   * 8 -> Windows
   */
  void *rootmap;

  /* Maps numeric keys to actual key values (strings), e.g.,
   * 1 -> 2
   * 1 -> 10
   */
  void *uniqmap;

  /* Key to hits map, e.g.,
   * 1 -> hits:10934 , root: -1 ,
   * 2 -> hits:3231  , root: -1 ,
   * 3 -> hits:500   , root: 8  ,
   * 4 -> hits:200   , root: 6  ,
   * 5 -> hits:200   , root: 6  ,
   * 7 -> 60233
   */
  void *hits;

  void *visitors;
  void *bw;
  void *time_served;
  void *methods;
  void *protocols;
  void *agents;
} GStorageMetrics;

typedef struct GStorage_
{
  GModule module;
  GStorageMetrics *metrics;
} GStorage;

extern GStorage *ht_storage;

GMetrics *new_gmetrics (void);
GStorageMetrics *get_storage_metrics_by_module (GModule module);
GStorageMetrics *new_ht_metrics (void);
GStorage *new_gstorage (uint32_t size);
int *int2ptr (int val);
uint64_t *uint642ptr (uint64_t val);
void *get_storage_metric_by_module (GModule module, GMetric metric);
void *get_storage_metric (GModule module, GMetric metric);
void set_data_metrics (GMetrics * ometrics, GMetrics ** nmetrics,
                       int processed);

#endif // for #ifndef GSTORAGE_H
