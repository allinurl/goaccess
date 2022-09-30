/**
 * geoip2.c -- implementation of GeoIP2
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_LIBMAXMINDDB
#include <maxminddb.h>
#endif

#include "geoip1.h"

#include "error.h"
#include "labels.h"
#include "util.h"
#include "xmalloc.h"

/* should be reused across lookups */
static int geoip_city_type = 0;
static MMDB_s *mmdb = NULL;

/* Determine if we have a valid geoip resource.
 *
 * If the geoip resource is NULL, 0 is returned.
 * If the geoip resource is valid and malloc'd, 1 is returned. */
int
is_geoip_resource (void) {
  return mmdb != NULL ? 1 : 0;
}

/* Free up GeoIP resources */
void
geoip_free (void) {
  if (!is_geoip_resource ())
    return;

  MMDB_close (mmdb);
  free (mmdb);
  mmdb = NULL;
}

/* Open the given GeoIP2 database.
 *
 * On error, it aborts.
 * On success, a new geolocation structure is set. */
void
init_geoip (void) {
  const char *fn = conf.geoip_database;
  int status = 0;

  if (fn == NULL)
    return;

  /* open custom city GeoIP database */
  mmdb = xcalloc (1, sizeof (MMDB_s));
  if ((status = MMDB_open (fn, MMDB_MODE_MMAP, mmdb)) != MMDB_SUCCESS) {
    free (mmdb);
    FATAL ("Unable to open GeoIP2 database %s: %s\n", fn, MMDB_strerror (status));
  }

  if (strstr (mmdb->metadata.database_type, "-City") != NULL)
    geoip_city_type = 1;
}

/* Look up an IP address that is passed in as a null-terminated string.
 *
 * On error, it aborts.
 * If no entry is found, 1 is returned.
 * On success, MMDB_lookup_result_s struct is set and 0 is returned. */
static int
geoip_lookup (MMDB_lookup_result_s * res, const char *ip) {
  int gai_err, mmdb_err;

  *res = MMDB_lookup_string (mmdb, ip, &gai_err, &mmdb_err);
  if (0 != gai_err)
    return 1;

  if (MMDB_SUCCESS != mmdb_err)
    FATAL ("Error from libmaxminddb: %s\n", MMDB_strerror (mmdb_err));

  if (!(*res).found_entry)
    return 1;

  return 0;
}

/* Get continent name concatenated with code.
 *
 * If continent not found, "Unknown" is returned.
 * On success, the continent code & name is returned . */
static const char *
get_continent_name_and_code (const char *continentid) {
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
    return "-- Unknown";
}

/* Compose a string with the country name and code and store it in the
 * given buffer. */
static void
geoip_set_country (const char *country, const char *code, char *loc) {
  if (country && code)
    snprintf (loc, COUNTRY_LEN, "%s %s", code, country);
  else
    snprintf (loc, COUNTRY_LEN, "%s", "Unknown");
}

/* Compose a string with the city name and state/region and store it
 * in the given buffer. */
static void
geoip_set_city (const char *city, const char *region, char *loc) {
  snprintf (loc, CITY_LEN, "%s, %s", city ? city : "N/A City", region ? region : "N/A Region");
}

/* Compose a string with the continent name and store it in the given
 * buffer. */
static void
geoip_set_continent (const char *continent, char *loc) {
  if (continent)
    snprintf (loc, CONTINENT_LEN, "%s", get_continent_name_and_code (continent));
  else
    snprintf (loc, CONTINENT_LEN, "%s", "Unknown");
}

/* A wrapper to fetch the looked up result set.
 *
 * On error, it aborts.
 * If no data is found, NULL is returned.
 * On success, the fetched value is returned. */
static char *
get_value (MMDB_lookup_result_s res, ...) {
  MMDB_entry_data_s entry_data;
  char *value = NULL;
  int status = 0;
  va_list keys;
  va_start (keys, res);

  status = MMDB_vget_value (&res.entry, &entry_data, keys);
  va_end (keys);

  if (status != MMDB_SUCCESS)
    return NULL;

  if (!entry_data.has_data)
    return NULL;

  if (entry_data.type != MMDB_DATA_TYPE_UTF8_STRING)
    FATAL ("Invalid data UTF8 GeoIP2 data %d:\n", entry_data.type);

  if ((value = strndup (entry_data.utf8_string, entry_data.data_size)) == NULL)
    FATAL ("Unable to allocate buffer %s: ", strerror (errno));

  return value;
}

/* A wrapper to fetch the looked up result and set the city and region.
 *
 * If no data is found, NULL is set.
 * On success, the fetched value is set. */
static void
geoip_query_city (MMDB_lookup_result_s res, char *location) {
  char *city = NULL, *region = NULL;

  if (res.found_entry) {
    city = get_value (res, "city", "names", DOC_LANG, NULL);
    region = get_value (res, "subdivisions", "0", "names", DOC_LANG, NULL);
    if (!city) {
      city = get_value (res, "city", "names", "en", NULL);
    }
    if (!region) {
      region = get_value (res, "subdivisions", "0", "names", "en", NULL);
    }
  }
  geoip_set_city (city, region, location);
  free (city);
  free (region);
}

/* A wrapper to fetch the looked up result and set the country and code.
 *
 * If no data is found, NULL is set.
 * On success, the fetched value is set. */
static void
geoip_query_country (MMDB_lookup_result_s res, char *location) {
  char *country = NULL, *code = NULL;

  if (res.found_entry) {
    country = get_value (res, "country", "names", DOC_LANG, NULL);
    code = get_value (res, "country", "iso_code", NULL);
    if (!country) {
      country = get_value (res, "country", "names", "en", NULL);
    }
  }
  geoip_set_country (country, code, location);
  free (code);
  free (country);
}

/* A wrapper to fetch the looked up result and set the continent code.
 *
 * If no data is found, NULL is set.
 * On success, the fetched value is set. */
static void
geoip_query_continent (MMDB_lookup_result_s res, char *location) {
  char *code = NULL;

  if (res.found_entry)
    code = get_value (res, "continent", "code", NULL);
  geoip_set_continent (code, location);
  free (code);
}

/* Set country data by record into the given `location` buffer */
void
geoip_get_country (const char *ip, char *location, GO_UNUSED GTypeIP type_ip) {
  MMDB_lookup_result_s res;

  geoip_lookup (&res, ip);
  geoip_query_country (res, location);
}

/* A wrapper to fetch the looked up result and set the continent. */
void
geoip_get_continent (const char *ip, char *location, GO_UNUSED GTypeIP type_ip) {
  MMDB_lookup_result_s res;

  geoip_lookup (&res, ip);
  geoip_query_continent (res, location);
}

/* Entry point to set GeoIP location into the corresponding buffers,
 * (continent, country, city).
 *
 * On error, 1 is returned
 * On success, buffers are set and 0 is returned */
int
set_geolocation (char *host, char *continent, char *country, char *city) {
  MMDB_lookup_result_s res;

  if (!is_geoip_resource ())
    return 1;

  geoip_lookup (&res, host);
  geoip_query_country (res, country);
  geoip_query_continent (res, continent);
  if (geoip_city_type)
    geoip_query_city (res, city);

  return 0;
}
