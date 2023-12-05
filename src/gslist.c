/**
 * gslist.c -- A Singly link list implementation
 *    _______       _______            __        __
 *   / ____/ |     / / ___/____  _____/ /_____  / /_
 *  / / __ | | /| / /\__ \/ __ \/ ___/ //_/ _ \/ __/
 * / /_/ / | |/ |/ /___/ / /_/ / /__/ ,< /  __/ /_
 * \____/  |__/|__//____/\____/\___/_/|_|\___/\__/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2023 Gerardo Orellana <hello @ goaccess.io>
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "gslist.h"
#include "gstorage.h"
#include "xmalloc.h"

/* Instantiate a new Singly linked-list node.
 *
 * On error, aborts if node can't be malloc'd.
 * On success, the GSLList node. */
GSLList *
list_create (void *data) {
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
list_insert_append (GSLList *node, void *data) {
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
list_insert_prepend (GSLList *list, void *data) {
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
list_find (GSLList *node, int (*func) (void *, void *), void *data) {
  while (node) {
    if (func (node->data, data) > 0)
      return node;
    node = node->next;
  }

  return NULL;
}

GSLList *
list_copy (GSLList *node) {
  GSLList *list = NULL;

  while (node) {
    if (!list)
      list = list_create (i322ptr ((*(uint32_t *) node->data)));
    else
      list = list_insert_prepend (list, i322ptr ((*(uint32_t *) node->data)));
    node = node->next;
  }

  return list;
}

/* Remove all nodes from the list.
 *
 * On success, 0 is returned. */
int
list_remove_nodes (GSLList *list) {
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
list_remove_node (GSLList **list, GSLList *node) {
  GSLList **current = list, *next = NULL;
  for (; *current; current = &(*current)->next) {
    if ((*current) != node)
      continue;

    next = (*current)->next;
    if ((*current)->data)
      free ((*current)->data);
    free (*current);
    *current = next;
    return 0;
  }
  return 1;
}

/* Iterate over the single linked-list and call function pointer.
 *
 * If function pointer does not return 0, -1 is returned.
 * On success, 0 is returned. */
int
list_foreach (GSLList *node, int (*func) (void *, void *), void *user_data) {
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
list_count (GSLList *node) {
  int count = 0;
  while (node != 0) {
    count++;
    node = node->next;
  }
  return count;
}
