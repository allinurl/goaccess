/**
 * commons.c -- holds different data types
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

/* list of available modules/panels */
int module_list[TOTAL_MODULES] = {[0 ... TOTAL_MODULES - 1] = -1 };

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
  fprintf (stdout, "Copyright (C) 2009-2016 by Gerardo Orellana\n");
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

/* Instantiate a new Singly linked-list node.
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

/* Remove the given node from the list.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
int
list_remove_node (GSLList * list, GSLList * node)
{
  while (list->next && list->next != node)
    list = list->next;

  if (list->next) {
    list->next = node->next;
    if (node->data)
      free (node->data);
    free (node);
    return 0;
  }
  return 1;
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
    if (mod == (unsigned int) module) {
      return 1;
    }
  }

  return 0;
}

/* Get the number of available modules/panels.
 *
 * The number of modules available is returned. */
uint32_t
get_num_modules (void)
{
  size_t idx = 0;
  uint32_t num = 0;

  FOREACH_MODULE (idx, module_list) {
    num++;
  }

  return num;
}

/* Get the index from the module_list given a module.
 *
 * If the module is not within the array, -1 is returned.
 * If the module is within the array, the index is returned. */
int
get_module_index (int module)
{
  size_t idx = 0;

  FOREACH_MODULE (idx, module_list) {
    if (module_list[idx] == module)
      return idx;
  }

  return -1;
}

/* Remove the given module from the module_list array.
 *
 * If the module is not within the array, 1 is returned.
 * If the module is within the array, it is removed from the array and
 * 0 is returned. */
int
remove_module (GModule module)
{
  int idx = get_module_index (module);
  if (idx == -1)
    return 1;

  if (idx < TOTAL_MODULES - 1)
    memmove (&module_list[idx], &module_list[idx + 1],
             ((TOTAL_MODULES - 1) - idx) * sizeof (module_list[0]));
  module_list[TOTAL_MODULES - 1] = -1;

  return 0;
}

/* Find the next module given the current module.
 *
 * The next available module in the array is returned. */
int
get_next_module (GModule module)
{
  int next = get_module_index (module) + 1;

  if (next == TOTAL_MODULES || module_list[next] == -1)
    return module_list[0];

  return module_list[next];
}

/* Find the previous module given the current module.
 *
 * The previous available module in the array is returned. */
int
get_prev_module (GModule module)
{
  int i;
  int next = get_module_index (module) - 1;

  if (next >= 0 && module_list[next] != -1)
    return module_list[next];

  for (i = TOTAL_MODULES - 1; i >= 0; i--) {
    if (module_list[i] != -1) {
      return module_list[i];
    }
  }

  return 0;
}

/* Build an array of available modules (ignores listed panels).
 *
 * If there are no modules enabled, 0 is returned.
 * On success, the first enabled module is returned. */
int
init_modules (void)
{
  GModule module;
  int i;

  /* init - terminating with -1 */
  for (module = 0; module < TOTAL_MODULES; ++module)
    module_list[module] = -1;

  for (i = 0, module = 0; module < TOTAL_MODULES; ++module) {
    if (!ignore_panel (module)) {
      module_list[i++] = module;
    }
  }

  return module_list[0] > -1 ? module_list[0] : 0;
}
