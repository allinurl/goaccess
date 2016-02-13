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
#include "settings.h"
#include "util.h"
#include "xmalloc.h"

/* processing time */
time_t end_proc;
time_t timestamp;
time_t start_proc;

/* Calculate a percentage.
 *
 * The percentage is returned. */
float
get_percentage (unsigned long long total, unsigned long long hit)
{
  return ((float) (hit * 100) / (total));
}

/* Display the storage being used. */
void
display_storage (void)
{
#ifdef TCB_BTREE
  fprintf (stdout, "Built using Tokyo Cabinet On-Disk B+ Tree.\n");
#elif TCB_MEMHASH
  fprintf (stdout, "Built using Tokyo Cabinet On-Memory Hash database.\n");
#else
  fprintf (stdout, "Built using the default On-Memory Hash database.\n");
#endif
}

/* Display the path of the default configuration file when `-p` is not used */
void
display_default_config_file (void)
{
  char *path = get_config_file_path ();

  if (!path) {
    fprintf (stdout, "No default config file found.\n");
    fprintf (stdout, "You may specify one with `-p /path/goaccess.conf`\n");
  } else {
    fprintf (stdout, "%s\n", path);
    free (path);
  }
}

/* Display the current version. */
void
display_version (void)
{
  fprintf (stdout, "GoAccess - %s.\n", GO_VERSION);
  fprintf (stdout, "For more details visit: http://goaccess.io\n");
  fprintf (stdout, "Copyright (C) 2009-2015 GNU GPL'd, by Gerardo Orellana\n");
}

/* Get the enumerated value given a string.
 *
 * On error, -1 is returned.
 * On success, the enumerated module value is returned. */
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

/* Get the enumerated module value given a module string.
 *
 * On error, -1 is returned.
 * On success, the enumerated module value is returned. */
int
get_module_enum (const char *str)
{
  /* String modules to enumerated modules */
  GEnum enum_modules[] = {
    {"VISITORS", VISITORS},
    {"REQUESTS", REQUESTS},
    {"REQUESTS_STATIC", REQUESTS_STATIC},
    {"NOT_FOUND", NOT_FOUND},
    {"HOSTS", HOSTS},
    {"OS", OS},
    {"BROWSERS", BROWSERS},
    {"VISIT_TIMES", VISIT_TIMES},
    {"VIRTUAL_HOSTS", VIRTUAL_HOSTS},
    {"REFERRERS", REFERRERS},
    {"REFERRING_SITES", REFERRING_SITES},
    {"KEYPHRASES", KEYPHRASES},
#ifdef HAVE_LIBGEOIP
    {"GEO_LOCATION", GEO_LOCATION},
#endif
    {"STATUS_CODES", STATUS_CODES},
  };

  return str2enum (enum_modules, ARRAY_SIZE (enum_modules), str);
}

/* Instantiate a new Single linked-list node.
 *
 * On error, aborts if node can't be malloc'd.
 * On success, the GSLList node. */
GSLList *
list_create (void *data)
{
  GSLList *node = xmalloc (sizeof (GSLList));
  node->data = data;
  node->next = NULL;

  return node;
}

/* Create and insert a node after a given node.
 *
 * On error, aborts if node can't be malloc'd.
 * On success, the newly created node. */
GSLList *
list_insert_append (GSLList * node, void *data)
{
  GSLList *newnode;
  newnode = list_create (data);
  newnode->next = node->next;
  node->next = newnode;

  return newnode;
}

/* Create and insert a node in front of the list.
 *
 * On error, aborts if node can't be malloc'd.
 * On success, the newly created node. */
GSLList *
list_insert_prepend (GSLList * list, void *data)
{
  GSLList *newnode;
  newnode = list_create (data);
  newnode->next = list;

  return newnode;
}

/* Find a node given a pointer to a function that compares them.
 *
 * If comparison fails, NULL is returned.
 * On success, the existing node is returned. */
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

/* Remove all nodes from the list.
 *
 * On success, 0 is returned. */
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

/* Iterate over the single linked-list and call function pointer.
 *
 * If function pointer does not return 0, -1 is returned.
 * On success, 0 is returned. */
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

/* Count the number of elements on the linked-list.
 *
 * On success, the number of elements is returned. */
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

/* Instantiate a new GAgents structure.
 *
 * On success, the newly malloc'd structure is returned. */
GAgents *
new_gagents (void)
{
  GAgents *agents = xmalloc (sizeof (GAgents));
  memset (agents, 0, sizeof *agents);

  return agents;
}

/* Instantiate a new GAgentItem structure.
 *
 * On success, the newly malloc'd structure is returned. */
GAgentItem *
new_gagent_item (uint32_t size)
{
  GAgentItem *item = xcalloc (size, sizeof (GAgentItem));

  return item;
}

/* Determine if the given date format is a timestamp.
 *
 * On error, 0 is returned.
 * On success, 1 is returned. */
int
has_timestamp (const char *fmt)
{
  if (strcmp ("%s", fmt) == 0 || strcmp ("%f", fmt) == 0)
    return 1;
  return 0;
}

/* Determine if the given module is set to be ignored.
 *
 * If ignored, 1 is returned, else 0 is returned. */
int
ignore_panel (GModule mod)
{
  int i, module;

  for (i = 0; i < conf.ignore_panel_idx; ++i) {
    if ((module = get_module_enum (conf.ignore_panels[i])) == -1)
      continue;
    if (mod == (unsigned int) module)
      return 1;
  }

  return 0;
}
