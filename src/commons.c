/**
 * commons.c -- holds different data types
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "commons.h"
#include "error.h"
#include "util.h"
#include "xmalloc.h"

/* processing time */
time_t end_proc;
time_t timestamp;
time_t start_proc;

/* resizing/scheme */
size_t real_size_y = 0;
size_t term_h = 0;
size_t term_w = 0;

static GEnum MODULES[] = {
  {"VISITORS", VISITORS},
  {"REQUESTS", REQUESTS},
  {"REQUESTS_STATIC", REQUESTS_STATIC},
  {"NOT_FOUND", NOT_FOUND},
  {"HOSTS", HOSTS},
  {"OS", OS},
  {"BROWSERS", BROWSERS},
  {"VISIT_TIMES", VISIT_TIMES},
  {"REFERRERS", REFERRERS},
  {"REFERRING_SITES", REFERRING_SITES},
  {"KEYPHRASES", KEYPHRASES},
#ifdef HAVE_LIBGEOIP
  {"GEO_LOCATION", GEO_LOCATION},
#endif
  {"STATUS_CODES", STATUS_CODES},
};

/* calculate hits percentage */
float
get_percentage (unsigned long long total, unsigned long long hit)
{
  return ((float) (hit * 100) / (total));
}

void
display_storage (void)
{
#ifdef TCB_BTREE
  fprintf (stdout, "Built using Tokyo Cabinet On-Disk B+ Tree.\n");
#elif TCB_MEMHASH
  fprintf (stdout, "Built using Tokyo Cabinet On-Memory Hash database.\n");
#else
  fprintf (stdout, "Built using GLib On-Memory Hash database.\n");
#endif
}

void
display_version (void)
{
  fprintf (stdout, "GoAccess - %s.\n", GO_VERSION);
  fprintf (stdout, "For more details visit: http://goaccess.io\n");
  fprintf (stdout, "Copyright (C) 2009-2015 GNU GPL'd, by Gerardo Orellana\n");
}

int
get_module_enum (const char *str)
{
  return str2enum (MODULES, ARRAY_SIZE (MODULES), str);
}

int
str2enum (const GEnum map[], int len, const char *str)
{
  int i;

  for (i = 0; i < len; ++i) {
    if (!strcmp (str, map[i].str))
      return map[i].idx;
  }

  return -1;
}

GSLList *
list_create (void *data)
{
  GSLList *node = xmalloc (sizeof (GSLList));
  node->data = data;
  node->next = NULL;

  return node;
}

GSLList *
list_insert_append (GSLList * node, void *data)
{
  GSLList *newnode;
  newnode = list_create (data);
  newnode->next = node->next;
  node->next = newnode;

  return newnode;
}

GSLList *
list_insert_prepend (GSLList * list, void *data)
{
  GSLList *newnode;
  newnode = list_create (data);
  newnode->next = list;

  return newnode;
}

GSLList *
list_find (GSLList * node, int (*func) (void *, void *), void *data)
{
  while (node) {
    if (func (node->data, data) > 0)
      return node;
    node = node->next;
  }

  return NULL;
}

int
list_remove_nodes (GSLList * list)
{
  GSLList *tmp;
  while (list != NULL) {
    tmp = list->next;
    if (list->data)
      free (list->data);
    free (list);
    list = tmp;
  }

  return 0;
}

int
list_foreach (GSLList * node, int (*func) (void *, void *), void *user_data)
{
  while (node) {
    if (func (node->data, user_data) != 0)
      return -1;
    node = node->next;
  }

  return 0;
}

int
list_count (GSLList * node)
{
  int count = 0;
  while (node != 0) {
    count++;
    node = node->next;
  }
  return count;
}

GAgents *
new_gagents (void)
{
  GAgents *agents = xmalloc (sizeof (GAgents));
  memset (agents, 0, sizeof *agents);

  return agents;
}

GAgentItem *
new_gagent_item (uint32_t size)
{
  GAgentItem *item = xcalloc (size, sizeof (GAgentItem));

  return item;
}

void
format_date_visitors (GMetrics * metrics)
{
  char date[DATE_LEN] = "";     /* Ymd */
  char *datum = metrics->data;

  memset (date, 0, sizeof *date);
  /* verify we have a valid date conversion */
  if (convert_date (date, datum, "%Y%m%d", "%d/%b/%Y", DATE_LEN) == 0) {
    free (datum);
    metrics->data = xstrdup (date);
    return;
  }
  LOG_DEBUG (("invalid date: %s", datum));

  free (datum);
  metrics->data = xstrdup ("---");
}

int
has_timestamp (const char *fmt)
{
  if (strcmp ("%s", fmt) == 0)
    return 1;
  return 0;
}
