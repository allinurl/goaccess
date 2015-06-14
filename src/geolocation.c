/**
 * geolocation.c -- GeoLocation related functions
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

#ifdef HAVE_LIBGEOIP
#include <GeoIP.h>
#include <GeoIPCity.h>
#endif

#include "geolocation.h"

#include "error.h"
#include "util.h"

GeoIP *geo_location_data;

/* Get continent name concatenated with code */
static const char *
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

/* Geolocation data */
GeoIP *
geoip_open_db (const char *db)
{
  GeoIP *geoip;
  geoip = GeoIP_open (db, GEOIP_MEMORY_CACHE);

  if (geoip == NULL)
    FATAL ("Unable to open GeoIP database: %s\n", db);

  GeoIP_set_charset (geoip, GEOIP_CHARSET_UTF8);
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

static void
geoip_set_city (const char *city, const char *region, char *loc)
{
  sprintf (loc, "%s, %s", city ? city : "N/A City",
           region ? region : "N/A Region");
}

static void
geoip_set_continent (const char *continent, char *loc)
{
  if (continent)
    sprintf (loc, "%s", get_continent_name_and_code (continent));
  else
    sprintf (loc, "%s", "Continent Unknown");
}

static GeoIPRecord *
get_geoip_record (const char *addr, GTypeIP type_ip)
{
  GeoIPRecord *rec = NULL;

  if (TYPE_IPV4 == type_ip)
    rec = GeoIP_record_by_name (geo_location_data, addr);
  else if (TYPE_IPV6 == type_ip)
    rec = GeoIP_record_by_name_v6 (geo_location_data, addr);

  return rec;
}

static void
geoip_set_country_by_record (const char *ip, char *location, GTypeIP type_ip)
{
  GeoIPRecord *rec = NULL;
  const char *country = NULL, *code = NULL, *addr = ip;

  if (conf.geoip_database == NULL || geo_location_data == NULL)
    return;

  /* Custom GeoIP database */
  if ((rec = get_geoip_record (addr, type_ip))) {
    country = rec->country_name;
    code = rec->country_code;
  }

  geoip_set_country (country, code, location);
  if (rec != NULL) {
    GeoIPRecord_delete (rec);
  }
}

static int
geoip_get_geoid (const char *addr, GTypeIP type_ip)
{
  int geoid = 0;

  if (TYPE_IPV4 == type_ip)
    geoid = GeoIP_id_by_name (geo_location_data, addr);
  else if (TYPE_IPV6 == type_ip)
    geoid = GeoIP_id_by_name_v6 (geo_location_data, addr);

  return geoid;
}

static const char *
geoip_get_country_by_geoid (const char *addr, GTypeIP type_ip)
{
  const char *country = NULL;

  if (TYPE_IPV4 == type_ip)
    country = GeoIP_country_name_by_name (geo_location_data, addr);
  else if (TYPE_IPV6 == type_ip)
    country = GeoIP_country_name_by_name_v6 (geo_location_data, addr);

  return country;
}

static void
geoip_set_country_by_geoid (const char *ip, char *location, GTypeIP type_ip)
{
  const char *country = NULL, *code = NULL, *addr = ip;
  int geoid = 0;

  if (geo_location_data == NULL)
    return;

  geoid = geoip_get_geoid (addr, type_ip);
  country = geoip_get_country_by_geoid (addr, type_ip);
  code = GeoIP_code_by_id (geoid);

  geoip_set_country (country, code, location);
}

void
geoip_get_country (const char *ip, char *location, GTypeIP type_ip)
{
  unsigned char rec = GeoIP_database_edition (geo_location_data);

  switch (rec) {
  case GEOIP_COUNTRY_EDITION:
    if (TYPE_IPV4 == type_ip)
      geoip_set_country_by_geoid (ip, location, TYPE_IPV4);
    break;
  case GEOIP_COUNTRY_EDITION_V6:
    if (TYPE_IPV6 == type_ip)
      geoip_set_country_by_geoid (ip, location, TYPE_IPV6);
    break;
  case GEOIP_CITY_EDITION_REV0:
  case GEOIP_CITY_EDITION_REV1:
    if (TYPE_IPV4 == type_ip)
      geoip_set_country_by_record (ip, location, TYPE_IPV4);
    break;
  case GEOIP_CITY_EDITION_REV0_V6:
  case GEOIP_CITY_EDITION_REV1_V6:
    if (TYPE_IPV6 == type_ip)
      geoip_set_country_by_record (ip, location, TYPE_IPV6);
    break;
  }
}

static void
geoip_set_continent_by_record (const char *ip, char *location, GTypeIP type_ip)
{
  GeoIPRecord *rec = NULL;
  const char *continent = NULL, *addr = ip;

  if (conf.geoip_database == NULL || geo_location_data == NULL)
    return;

  /* Custom GeoIP database */
  if ((rec = get_geoip_record (addr, type_ip)))
    continent = rec->continent_code;

  geoip_set_continent (continent, location);
  if (rec != NULL) {
    GeoIPRecord_delete (rec);
  }
}

static void
geoip_set_continent_by_geoid (const char *ip, char *location, GTypeIP type_ip)
{
  const char *continent = NULL, *addr = ip;
  int geoid = 0;

  if (geo_location_data == NULL)
    return;

  geoid = geoip_get_geoid (addr, type_ip);
  continent = GeoIP_continent_by_id (geoid);
  geoip_set_continent (continent, location);
}


void
geoip_get_continent (const char *ip, char *location, GTypeIP type_ip)
{
  unsigned char rec = GeoIP_database_edition (geo_location_data);

  switch (rec) {
  case GEOIP_COUNTRY_EDITION:
    if (TYPE_IPV4 == type_ip)
      geoip_set_continent_by_geoid (ip, location, TYPE_IPV4);
    break;
  case GEOIP_COUNTRY_EDITION_V6:
    if (TYPE_IPV6 == type_ip)
      geoip_set_continent_by_geoid (ip, location, TYPE_IPV6);
    break;
  case GEOIP_CITY_EDITION_REV0:
  case GEOIP_CITY_EDITION_REV1:
    if (TYPE_IPV4 == type_ip)
      geoip_set_continent_by_record (ip, location, TYPE_IPV4);
    break;
  case GEOIP_CITY_EDITION_REV0_V6:
  case GEOIP_CITY_EDITION_REV1_V6:
    if (TYPE_IPV6 == type_ip)
      geoip_set_continent_by_record (ip, location, TYPE_IPV6);
    break;
  }
}

static void
geoip_set_city_by_record (const char *ip, char *location, GTypeIP type_ip)
{
  GeoIPRecord *rec = NULL;
  const char *city = NULL, *region = NULL, *addr = ip;

  /* Custom GeoIP database */
  if ((rec = get_geoip_record (addr, type_ip))) {
    city = rec->city;
    region = rec->region;
  }

  geoip_set_city (city, region, location);
  if (rec != NULL) {
    GeoIPRecord_delete (rec);
  }
}

/* Custom GeoIP database - i.e., GeoLiteCity.dat */
void
geoip_get_city (const char *ip, char *location, GTypeIP type_ip)
{
  unsigned char rec = GeoIP_database_edition (geo_location_data);

  if (conf.geoip_database == NULL || geo_location_data == NULL)
    return;

  switch (rec) {
  case GEOIP_CITY_EDITION_REV0:
  case GEOIP_CITY_EDITION_REV1:
    if (TYPE_IPV4 == type_ip)
      geoip_set_city_by_record (ip, location, TYPE_IPV4);
    break;
  case GEOIP_CITY_EDITION_REV0_V6:
  case GEOIP_CITY_EDITION_REV1_V6:
    if (TYPE_IPV6 == type_ip)
      geoip_set_city_by_record (ip, location, TYPE_IPV6);
    break;
  }
}

int
set_geolocation (char *host, char *continent, char *country, char *city)
{
  int type_ip = 0;

  if (geo_location_data == NULL)
    return 1;

  if (invalid_ipaddr (host, &type_ip))
    return 1;

  geoip_get_country (host, country, type_ip);
  geoip_get_continent (host, continent, type_ip);
  if (conf.geoip_database)
    geoip_get_city (host, city, type_ip);

  return 0;
}
