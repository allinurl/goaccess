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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_NCURSESW_NCURSES_H
#include <ncursesw/ncurses.h>
#elif HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#elif HAVE_CURSES_H
#include <curses.h>
#endif

#ifndef GMENU_H_INCLUDED
#define GMENU_H_INCLUDED

enum ACTION
{
  REQ_DOWN,
  REQ_UP,
  REQ_SEL
};

typedef struct GMenu_ GMenu;
typedef struct GItem_ GItem;

struct GItem_
{
  char *name;
  int checked;
};

struct GMenu_
{
  WINDOW *win;

  int count;
  int size;
  int idx;
  int start;
  int h;
  int w;
  int x;
  int y;
  unsigned short multiple;
  unsigned short selectable;
  unsigned short status;
  GItem *items;
};

GMenu *new_gmenu (WINDOW * parent, int h, int w, int y, int x);
int post_gmenu (GMenu * menu);
void gmenu_driver (GMenu * menu, int c);

#endif
