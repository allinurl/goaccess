/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2026 Gerardo Orellana <hello @ goaccess.io>
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

#ifndef GEOLOCATION_H_INCLUDED
#define GEOLOCATION_H_INCLUDED

#include <limits.h>
#include <stddef.h>

#include "geoip1.h"

/* Display label shared by unresolved countries and continents. */
#define GEO_UNKNOWN "Unknown"
/* Label for an unrecognized, nonempty continent code. */
#define GEO_UNKNOWN_CONTINENT "-- Unknown"

/* Version of country-qualified city identities written to persistence. */
#define GEO_KEY_VERSION 1
/* Length-prefixed country labels prevent ambiguous city identities. */
#define GEO_CITY_KEY_PREFIX "geo-city:"
/* A size_t needs no more decimal digits than bits; the label bounds also
 * reserve enough space for the separator and terminating null byte. */
#define GEO_CITY_KEY_LEN (sizeof (GEO_CITY_KEY_PREFIX) + (COUNTRY_LEN) + (CITY_LEN) + sizeof (size_t) * CHAR_BIT)

void geo_city_key (char key[GEO_CITY_KEY_LEN], const char *country, const char *city);
const char *geo_city_name (const char *key, const char *country);
const char *geo_continent_name (const char *code);
void discard_legacy_geo_cities (void);
void verify_restored_geo_panels (void);

#endif
