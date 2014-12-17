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

#ifndef GEOLOCATION_H_INCLUDED
#define GEOLOCATION_H_INCLUDED

#ifdef HAVE_LIBGEOIP
#include <GeoIP.h>
#endif

#define CITY_LEN         28
#define CONTINENT_LEN    48
#define COUNTRY_LEN      48 + 3 /* Country + two-letter Code */

typedef struct GLocation_
{
  char city[CITY_LEN];
  char continent[CONTINENT_LEN];
  int hits;
} GLocation;

extern GeoIP *geo_location_data;

char *geoip_get_country_code (const char *ip);
const char *get_continent_name_and_code (const char *continentid);
GeoIP *geoip_open_db (const char *db);
void geoip_get_city (const char *ip, char *location);
void geoip_get_continent (const char *ip, char *location);
void geoip_get_country (const char *ip, char *country);

#endif
