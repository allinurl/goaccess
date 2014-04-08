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

#ifdef HAVE_LIBNCURSESW
#ifdef __FreeBSD__
#include <ncurses.h>
#else
#include <ncursesw/curses.h>
#endif
#else
#include <ncurses.h>
#endif

#include <time.h>

#include "commons.h"
#include "error.h"

/* processing time */
time_t end_proc;
time_t timestamp;
time_t start_proc;

/* resizing/scheme */
size_t real_size_y = 0;
size_t term_h = 0;
size_t term_w = 0;

#ifdef HAVE_LIBGEOIP
GeoIP *geo_location_data;
#endif

/* calculate hits percentage */
float
get_percentage (unsigned long long total, unsigned long long hit)
{
  return ((float) (hit * 100) / (total));
}

/* Geolocation data */
#ifdef HAVE_LIBGEOIP
char *
get_geoip_data (const char *data)
{
  const char *location = NULL;
  const char *addr = data;

  /* Geolocation data */
  if (geo_location_data != NULL)
    location = GeoIP_country_name_by_name (geo_location_data, addr);
  if (location == NULL)
    location = "Location Unknown";
  return (char *) location;
}
#endif
