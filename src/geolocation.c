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

/* Geolocation storage identities and restore compatibility. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "geolocation.h"

#include "error.h"
#include "gkhash.h"
#include "util.h"

/* Country and city labels use the same canonical continent names in both
 * GeoIP backends and in legacy persistence detection. */
static const struct {
  const char *code;
  const char *label;
} geo_continents[] = {
  {"AF", "AF Africa"},
  {"AN", "AN Antarctica"},
  {"AS", "AS Asia"},
  {"EU", "EU Europe"},
  {"NA", "NA North America"},
  {"OC", "OC Oceania"},
  {"SA", "SA South America"},
};

/* Restore filtering runs before the raw-data cache borrows these strings. */
typedef struct GGeoRestore_ {
  khash_t (is32) * data, *roots;
  khash_t (ii32) * keys;
  khash_t (imtv) * metrics;
} GGeoRestore;

/* Resolve a continent code to its canonical display label.
 *
 * On success, the continent label is returned.
 * On failure, the unknown-continent label is returned. */
const char *
geo_continent_name (const char *code) {
  size_t i = 0;

  for (i = 0; i < ARRAY_SIZE (geo_continents); ++i) {
    if (strcmp (code, geo_continents[i].code) == 0)
      return geo_continents[i].label;
  }

  return GEO_UNKNOWN_CONTINENT;
}

/* Write a country-qualified city identity into caller-owned scratch space. */
void
geo_city_key (char key[GEO_CITY_KEY_LEN], const char *country, const char *city) {
  int len =
    snprintf (key, GEO_CITY_KEY_LEN, GEO_CITY_KEY_PREFIX "%zu:%s%s", strlen (country), country,
              city);

  if (len < 0 || (size_t) len >= GEO_CITY_KEY_LEN)
    FATAL ("Geolocation identity exceeds its storage buffer.");
}

/* Find the display name in a city identity belonging to the given country.
 *
 * On success, a borrowed pointer to the city name is returned.
 * On failure, NULL is returned for a country row or an invalid identity. */
const char *
geo_city_name (const char *key, const char *country) {
  char prefix[GEO_CITY_KEY_LEN];
  size_t len = 0;

  if (strncmp (key, GEO_CITY_KEY_PREFIX, sizeof (GEO_CITY_KEY_PREFIX) - 1) != 0)
    return NULL;

  geo_city_key (prefix, country, "");
  len = strlen (prefix);
  if (strncmp (key, prefix, len) != 0 || key[len] == '\0')
    return NULL;

  return key + len;
}

/* Recognize a legacy continent-to-country row when no format marker exists.
 *
 * On success, non-zero is returned for a country row.
 * On failure, 0 is returned for a city row or an unknown hierarchy. */
static int
is_country_row (const char *root, const char *data) {
  size_t i = 0;

  if (strcmp (root, GEO_UNKNOWN) == 0 || strcmp (root, GEO_UNKNOWN_CONTINENT) == 0)
    return strcmp (data, GEO_UNKNOWN) == 0 || get_continent_for_country (data) != NULL;

  for (i = 0; i < ARRAY_SIZE (geo_continents); ++i) {
    if (strcmp (root, geo_continents[i].label) == 0)
      return 1;
  }

  return 0;
}

/* Remove identities belonging only to discarded legacy leaves. */
static void
prune_geo_keys (GGeoRestore *store) {
  khint_t k = 0;
  uint32_t id = 0;

  for (k = kh_begin (store->keys); k != kh_end (store->keys); ++k) {
    if (!kh_exist (store->keys, k))
      continue;
    id = kh_val (store->keys, k);
    /* A leaf ID can also name a parent; keep those dictionary entries. */
    if (kh_get (is32, store->data, id) == kh_end (store->data) &&
        kh_get (is32, store->roots, id) == kh_end (store->roots))
      kh_del (ii32, store->keys, k);
  }
}

/* Recompute geolocation metadata after discarding legacy city aggregates. */
static void
rebuild_geo_metadata (uint32_t date, GGeoRestore *store) {
  khash_t (su64) * metadata = get_hash (GEO_LOCATION, date, MTRC_METADATA);
  GKMetricVals *metrics = NULL;
  khint_t k = 0, km = 0;

  del_su64_free (metadata, 1);
  for (k = kh_begin (store->data); k != kh_end (store->data); ++k) {
    if (!kh_exist (store->data, k))
      continue;
    km = kh_get (imtv, store->metrics, kh_key (store->data, k));
    if (km == kh_end (store->metrics))
      continue;

    metrics = &kh_val (store->metrics, km);
    ht_insert_meta_data (GEO_LOCATION, date, "hits", metrics->hits);
    ht_insert_meta_data (GEO_LOCATION, date, "visitors", metrics->visitors);
    ht_insert_meta_data (GEO_LOCATION, date, "bytes", metrics->bw);
    ht_insert_meta_data (GEO_LOCATION, date, "cumts", metrics->cumts);
    /* Like ingestion, maxts metadata totals request times; the cache computes
     * the actual maximum from the surviving per-row maxima. */
    ht_insert_meta_data (GEO_LOCATION, date, "maxts", metrics->cumts);
  }
}

/* Discard legacy city rows while retaining country rows and other panels.
 *
 * On success, non-zero is returned if any rows were discarded.
 * On failure or when there are no unsafe rows, 0 is returned. */
static int
discard_geo_date (uint32_t date) {
  GGeoRestore store = {
    .data = get_hash (GEO_LOCATION, date, MTRC_DATAMAP),
    .roots = get_hash (GEO_LOCATION, date, MTRC_ROOTMAP),
    .keys = get_hash (GEO_LOCATION, date, MTRC_KEYMAP),
    .metrics = get_hash (GEO_LOCATION, date, MTRC_METRICS),
  };
  khint_t k = 0, kr = 0, km = 0;
  const char *label = NULL, *root = NULL;
  int discarded = 0;

  if (!store.data || !store.roots || !store.keys || !store.metrics)
    return 0;

  for (k = kh_begin (store.data); k != kh_end (store.data); ++k) {
    if (!kh_exist (store.data, k))
      continue;
    km = kh_get (imtv, store.metrics, kh_key (store.data, k));
    if (km == kh_end (store.metrics))
      continue;
    kr = kh_get (is32, store.roots, kh_val (store.metrics, km).root);
    label = kh_val (store.data, k);
    root = kr != kh_end (store.roots) ? kh_val (store.roots, kr) : NULL;
    if (root && (geo_city_name (label, root) || is_country_row (root, label)))
      continue;

    free (kh_val (store.data, k));
    kh_del (is32, store.data, k);
    kh_del (imtv, store.metrics, km);
    discarded = 1;
  }

  if (discarded) {
    prune_geo_keys (&store);
    rebuild_geo_metadata (date, &store);
  }

  return discarded;
}

/* Drop ambiguous legacy city aggregates and report the missing GEO history. */
void
discard_legacy_geo_cities (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khint_t k = 0;
  int discarded = 0;

  if (!dates)
    return;

  for (k = kh_begin (dates); k != kh_end (dates); ++k) {
    if (kh_exist (dates, k))
      discarded |= discard_geo_date (kh_key (dates, k));
  }

  if (discarded)
    fprintf (stderr,
             "Warning: Skipped legacy GEO city rows that may contain traffic merged across countries. "
             "Other report history is preserved. Reprocess the original logs in an empty --db-path "
             "for complete geolocation history.\n");
}

/* Check retained storage for rows before deciding whether to show a panel.
 *
 * On success, non-zero is returned when the module contains data rows.
 * On failure or when the module has no rows, 0 is returned. */
static int
has_geo_rows (GModule module) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) * dates = get_hdb (db, MTRC_DATES);
  khash_t (is32) * data = NULL;
  khint_t k = 0;

  if (!dates)
    return 0;

  for (k = kh_begin (dates); k != kh_end (dates); ++k) {
    if (!kh_exist (dates, k))
      continue;
    data = get_hash (module, kh_key (dates, k), MTRC_DATAMAP);
    if (data && kh_size (data))
      return 1;
  }

  return 0;
}

/* Hide empty restored GEO/ASN panels that cannot receive new lookup data. */
void
verify_restored_geo_panels (void) {
  if (!conf.has_geocountry && !conf.has_geocity && !has_geo_rows (GEO_LOCATION))
    remove_module (GEO_LOCATION);
  if (!conf.has_geoasn && !has_geo_rows (ASN))
    remove_module (ASN);
}
