/**
 *    _______       _______            __        __
 *   / ____/ |     / / ___/____  _____/ /_____  / /_
 *  / / __ | | /| / /\__ \/ __ \/ ___/ //_/ _ \/ __/
 * / /_/ / | |/ |/ /___/ / /_/ / /__/ ,< /  __/ /_
 * \____/  |__/|__//____/\____/\___/_/|_|\___/\__/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2020 Gerardo Orellana <hello @ goaccess.io>
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

#ifndef GSLIST_H_INCLUDED
#define GSLIST_H_INCLUDED

/* Generic Single linked-list */
typedef struct GSLList_ {
  void *data;
  struct GSLList_ *next;
} GSLList;

#define GSLIST_FOREACH(node, data, code) {  \
  while (node) {                            \
    (data) = node->data;                    \
    code;                                   \
    node = node->next;                      \
  }}

/* single linked-list */
GSLList *list_create (void *data);
GSLList *list_find (GSLList * node, int (*func) (void *, void *), void *data);
GSLList *list_insert_append (GSLList * node, void *data);
GSLList *list_insert_prepend (GSLList * list, void *data);
GSLList *list_copy (GSLList * node);
int list_count (GSLList * list);
int list_foreach (GSLList * node, int (*func) (void *, void *), void *user_data);
int list_remove_node (GSLList ** list, GSLList * node);
int list_remove_nodes (GSLList * list);

#endif // for #ifndef GSLIST_H
