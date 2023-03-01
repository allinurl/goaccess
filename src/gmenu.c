/**
 * gmenu.c -- goaccess menus
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gmenu.h"

#include "xmalloc.h"
#include "ui.h"

/* Allocate memory for a new GMenu instance.
 *
 * On success, the newly allocated GMenu is returned . */
GMenu *
new_gmenu (WINDOW * parent, int h, int w, int y, int x) {
  GMenu *menu = xmalloc (sizeof (GMenu));

  memset (menu, 0, sizeof *menu);
  menu->count = 0;
  menu->idx = 0;
  menu->multiple = 0;
  menu->selectable = 0;
  menu->start = 0;
  menu->status = 0;

  menu->h = h;
  menu->w = w;
  menu->x = x;
  menu->y = y;
  menu->win = derwin (parent, menu->h, menu->w, menu->y, menu->x);

  return menu;
}

/* Render actual menu item */
static void
draw_menu_item (GMenu * menu, char *s, int x, int y, int w, int checked,
                GColors * (*func) (void)) {
  char check, *lbl = NULL;

  if (menu->selectable) {
    check = checked ? 'x' : ' ';
    lbl = xmalloc (snprintf (NULL, 0, "[%c] %s", check, s) + 1);
    sprintf (lbl, "[%c] %s", check, s);
    draw_header (menu->win, lbl, "%s", y, x, w, (*func));
    free (lbl);
  } else {
    draw_header (menu->win, s, "%s", y, x, w, (*func));
  }
}

/* Displays a menu to its associated window.
 *
 * On error, 1 is returned.
 * On success, the newly created menu is added to the window and 0 is
 * returned. */
int
post_gmenu (GMenu * menu) {
  GColors *(*func) (void);
  int i = 0, j = 0, start, end, height, total, checked = 0;

  if (menu == NULL)
    return 1;

  werase (menu->win);

  height = menu->h;
  start = menu->start;
  total = menu->size;
  end = height < total ? start + height : total;

  for (i = start; i < end; i++, j++) {
    func = i == menu->idx ? color_selected : color_default;
    checked = menu->items[i].checked ? 1 : 0;
    draw_menu_item (menu, menu->items[i].name, 0, j, menu->w, checked, func);
  }
  wrefresh (menu->win);

  return 0;
}

/* Main work horse of the menu system processing input events */
void
gmenu_driver (GMenu * menu, int c) {
  int i;

  switch (c) {
  case REQ_DOWN:
    if (menu->idx >= menu->size - 1)
      break;
    ++menu->idx;
    if (menu->idx >= menu->h && menu->idx >= menu->start + menu->h)
      menu->start++;
    post_gmenu (menu);
    break;
  case REQ_UP:
    if (menu->idx <= 0)
      break;
    --menu->idx;
    if (menu->idx < menu->start)
      --menu->start;
    post_gmenu (menu);
    break;
  case REQ_SEL:
    if (!menu->multiple) {
      for (i = 0; i < menu->size; i++)
        menu->items[i].checked = 0;
    }
    if (menu->items[menu->idx].checked)
      menu->items[menu->idx].checked = 0;
    else
      menu->items[menu->idx].checked = 1;
    post_gmenu (menu);
    break;
  }
}
