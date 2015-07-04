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
   * memory usage (in most cases).
   * HEAD|/index.php  -> 1
   * POST|/index.php  -> 2
   * Windows XP       -> 3
   * Ubuntu 10.10     -> 4
   * GET|Ubuntu 10.10 -> 5
   * Linux            -> 6
   * 26/Dec/2014      -> 7
   * Windows          -> 8
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

  /* Maps a string key made from the numeric key of the IP/date/UA and the
   * numeric key from the data field of each module to numeric autoincremented
   * values. e.g., 1 -> unique visitor key (concatenated) with 4 -> data key = 14
   * "14" -> 1
   * "15" -> 2
   */
  void *uniqmap;

  /* Numeric key to a structure containing hits/root elements, e.g.,
   * If root does not exist, it will have the value of 0
   * 1 -> hits:10934 , root: 0 ,
   * 2 -> hits:3231  , root: 0 ,
   * 3 -> hits:500   , root: 8 ,
   * 4 -> hits:200   , root: 6 ,
   * 5 -> hits:200   , root: 6 ,
   * 5 -> hits:2030  , root: 0 ,
   */
  void *hits;

  /* Maps numeric keys made from the uniqmap store to autoincremented values
   * (counter).
   * 10 -> 100
   * 40 -> 56
   */
  void *visitors;

  /* Maps numeric data keys to bandwidth (in bytes).
   * 1 -> 1024
   * 2 -> 2048
   */
  void *bw;

  /* Maps numeric data keys to average time served (in usecs/msecs).
   * 1 -> 187
   * 2 -> 208
   */
  void *time_served;

  /* Maps numeric data keys to string values.
   * 1 -> GET
   * 2 -> POST
   */
  void *methods;

  /* Maps numeric data keys to string values.
   * 1 -> HTTP/1.1
   * 2 -> HTTP/1.0
   */
  void *protocols;

  /* Maps numeric unique user-agent keys to the
   * corresponding numeric value.
   * 1 -> 3
   * 2 -> 4
   */
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
