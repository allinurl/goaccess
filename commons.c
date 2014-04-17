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

#ifdef HAVE_LIBGEOIP
#include <GeoIP.h>
#include <GeoIPCity.h>
#endif

#include <time.h>

#include "commons.h"
#include "error.h"
#include "settings.h"
#include "util.h"

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
GeoIP *
geoip_open_db (const char *db)
{
  GeoIP *geoip;
  geoip = GeoIP_open (db, GEOIP_INDEX_CACHE);
  if (geoip == NULL) {
    LOG_DEBUG (("Unable to open GeoIP City database: %s\n", db));
    error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                   "Unable to open GeoIP City database.");
  }
  LOG_DEBUG (("Opened GeoIP City database: %s\n", db));

  return geoip;
}

static void
geoip_set_country (const char *country, const char *code, char *loc)
{
  if (country && code)
    sprintf (loc, "%s %s", code, country);
  else
    sprintf (loc, "%s", "Country Unknown");
}

void
geoip_get_country (const char *ip, char *location)
{
  GeoIPRecord *rec = NULL;
  const char *country = NULL, *code = NULL, *addr = ip;
  int geoid = 0;

  /* Custom GeoIP database */
  if (conf.geoip_city_data != NULL && geo_location_data != NULL) {
    rec = GeoIP_record_by_name (geo_location_data, addr);
    if (rec) {
      code = rec->country_code;
      country = rec->country_name;
    }
  }
  /* Legacy GeoIP database */
  else if (geo_location_data != NULL) {
    geoid = GeoIP_id_by_name (geo_location_data, addr);
    country = GeoIP_country_name_by_name (geo_location_data, addr);
    code = GeoIP_code_by_id (geoid);
  }

  geoip_set_country (country, code, location);
  if (rec != NULL)
    GeoIPRecord_delete (rec);
}

static void
geoip_set_continent (const char *continent, char *loc)
{
  if (continent)
    sprintf (loc, "%s", get_continent_name_and_code (continent));
  else
    sprintf (loc, "%s", "Continent Unknown");
}

void
geoip_get_continent (const char *ip, char *location)
{
  GeoIPRecord *rec = NULL;
  const char *continent = NULL, *addr = ip;
  int geoid = 0;

  /* Custom GeoIP database */
  if (conf.geoip_city_data != NULL && geo_location_data != NULL) {
    rec = GeoIP_record_by_name (geo_location_data, addr);
    if (rec)
      continent = rec->continent_code;
  }
  /* Legacy GeoIP database */
  else if (geo_location_data != NULL) {
    geoid = GeoIP_id_by_name (geo_location_data, addr);
    continent = GeoIP_continent_by_id (geoid);
  }

  if (rec != NULL)
    GeoIPRecord_delete (rec);
  geoip_set_continent (continent, location);
}

static void
geoip_set_city (const char *city, const char *region, char *loc)
{
  sprintf (loc, "%s, %s", city ? city : "N/A City",
           region ? region : "N/A Region");
}

/* Custom GeoIP database - i.e., GeoLiteCity.dat */
void
geoip_get_city (const char *ip, char *location)
{
  GeoIPRecord *rec = NULL;
  const char *city = NULL, *region = NULL, *addr = ip;

  if (geo_location_data != NULL)
    rec = GeoIP_record_by_name (geo_location_data, addr);

  if (rec != NULL) {
    city = rec->city;
    region = rec->region;
  }

  geoip_set_city (city, region, location);
  if (rec != NULL)
    GeoIPRecord_delete (rec);
}

/* Get continent name concatenated with code */
const char *
get_continent_name_and_code (const char *continentid)
{
  if (memcmp (continentid, "NA", 2) == 0)
    return "NA North America";
  else if (memcmp (continentid, "OC", 2) == 0)
    return "OC Oceania";
  else if (memcmp (continentid, "EU", 2) == 0)
    return "EU Europe";
  else if (memcmp (continentid, "SA", 2) == 0)
    return "SA South America";
  else if (memcmp (continentid, "AF", 2) == 0)
    return "AF Africa";
  else if (memcmp (continentid, "AN", 2) == 0)
    return "AN Antarctica";
  else if (memcmp (continentid, "AS", 2) == 0)
    return "AS Asia";
  else
    return "-- Location Unknown";
}
#endif
