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

#ifdef HAVE_NCURSESW_NCURSES_H
#include <ncursesw/ncurses.h>
#elif HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#elif HAVE_CURSES_H
#include <curses.h>
#endif

#include <time.h>

#include "commons.h"
#include "error.h"
#include "settings.h"
#include "sort.h"
#include "util.h"

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
  fprintf (stdout, "Copyright (C) 2009-2014 GNU GPL'd, by Gerardo Orellana\n");
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
