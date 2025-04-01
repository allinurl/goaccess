/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2025 Gerardo Orellana <hello @ goaccess.io>
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

#ifndef GEOIP_H_INCLUDED
#define GEOIP_H_INCLUDED

#include "commons.h"

#define CITY_LEN       47 + 1 /* max string length for a city */
#define CONTINENT_LEN  47 + 1 /* max string length for a country */
#define COUNTRY_LEN    48 + 3 /* Country + two-letter Code */
#define ASN_LEN        64 + 6 /* ASN + 5 digit/16-bit number/code */

/* Type of IP */
typedef enum {
  TYPE_COUNTRY,
  TYPE_CITY,
  TYPE_ASN
} GO_GEOIP_DB;

typedef struct GLocation_ {
  char city[CITY_LEN];
  char continent[CONTINENT_LEN];
  int hits;
} GLocation;

int is_geoip_resource (void);
int set_geolocation (char *host, char *continent, char *country, char *city, char *asn);
void geoip_asn (char *host, char *asn);
void geoip_free (void);
void geoip_get_continent (const char *ip, char *location, GTypeIP type_ip);
void geoip_get_country (const char *ip, char *location, GTypeIP type_ip);
void init_geoip (void);

#endif // for #ifndef GEOIP_H
