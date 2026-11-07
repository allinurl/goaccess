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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "anonymize.h"
#include "error.h"
#include "gkhash.h"
#include "util.h"
#include "xmalloc.h"

/* Non-IP tokens accepted by --no-ip-validation have no address bits to mask. */
#define ANONYMIZED_UNKNOWN_HOST "-"
/* Keep hash tables at most half full after combining legacy host rows. */
#define COMPACT_TABLE_SLACK 2
/* Use the operating system's cryptographic random source without an SSL dependency. */
#define ANONYMIZE_RANDOM_SOURCE "/dev/urandom"
/* mkstemp creates a private file before its name is linked into the database. */
#define ANONYMIZE_KEY_TEMP_SUFFIX ".XXXXXX"
/* A separate hash domain binds persisted metadata to its original key. */
#define ANONYMIZE_KEY_ID_DOMAIN "goaccess-ip-key"

/* Build the path to the database's private visitor key.
 *
 * On success, the allocated path is returned; allocation failure is fatal. */
static char *
anonymization_key_path (void) {
  const char *dir = conf.db_path ? conf.db_path : DB_PATH;
  char *path = xmalloc (strlen (dir) + sizeof (ANONYMIZE_KEY_FILE) + 1);

  sprintf (path, "%s/%s", dir, ANONYMIZE_KEY_FILE);
  return path;
}

/* Read the database's existing key without accepting truncated or public keys.
 *
 * On success, 1 is returned. If absent, 0 is returned; other failures are fatal. */
static int
read_anonymization_key (const char *path, uint8_t key[SIPHASH_KEY_SIZE]) {
  FILE *fp = fopen (path, "rb");
  struct stat st = { 0 };
  int valid = 0;

  if (!fp) {
    if (errno == ENOENT)
      return 0;
    FATAL ("Unable to read visitor key '%s': %s", path, strerror (errno));
  }
  if (fstat (fileno (fp), &st) != 0 || !S_ISREG (st.st_mode) || (st.st_mode & (S_IRWXG | S_IRWXO)) != 0)
    FATAL ("Visitor key '%s' must be a regular file accessible only to its owner (chmod 600).", path);

  valid = fread (key, 1, SIPHASH_KEY_SIZE, fp) == SIPHASH_KEY_SIZE && fgetc (fp) == EOF && !ferror (fp);
  fclose (fp);
  if (!valid)
    FATAL ("Invalid visitor key '%s'; restore the original key with its database.", path);
  return 1;
}

/* Initialize a database key from the operating system's random source. */
static void
generate_anonymization_key (uint8_t key[SIPHASH_KEY_SIZE]) {
  FILE *fp = fopen (ANONYMIZE_RANDOM_SOURCE, "rb");

  if (!fp || fread (key, 1, SIPHASH_KEY_SIZE, fp) != SIPHASH_KEY_SIZE)
    FATAL ("Unable to obtain random bytes for the visitor key.");
  fclose (fp);
}

/* Identify a key without including the key itself in the public metadata.
 *
 * On success, the 64-bit identifier is returned; there is no failure case. */
uint64_t
anonymization_key_id (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);

  return siphash24 (ANONYMIZE_KEY_ID_DOMAIN, sizeof (ANONYMIZE_KEY_ID_DOMAIN) - 1, db->anonymize.key);
}

/* Restore masking settings and initialize the database's single visitor key. */
void
init_anonymization (uint32_t version, uint32_t level, uint64_t key_id) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  char *path = NULL;
  int found = 0;

  if (version) {
    if (version != ANONYMIZE_VERSION || level < ANONYMIZE_DEFAULT || level > ANONYMIZE_PEDANTIC)
      FATAL ("Unsupported persisted IP anonymization settings.");
    if (conf.anonymize_level && conf.anonymize_level != (int) level)
      FATAL ("This database uses --anonymize-level=%u; restore it with the same level.", level);
    conf.anonymize_ip = 1;
    conf.anonymize_level = level;
    db->anonymize.restored_level = level;
  }
  if (!conf.anonymize_ip)
    return;

  if (!conf.anonymize_level)
    conf.anonymize_level = ANONYMIZE_DEFAULT;
  if (conf.anonymize_level < ANONYMIZE_DEFAULT || conf.anonymize_level > ANONYMIZE_PEDANTIC)
    FATAL ("IP anonymization level must be 1, 2 or 3.");

  /* Addresses are grouped before reporting, so per-address agents and DNS
   * resolution would no longer describe an individual host. */
  conf.list_agents = 0;
  conf.skip_term_resolver = 1;
  conf.enable_html_resolver = 0;

  if (conf.restore || conf.persist) {
    path = anonymization_key_path ();
    found = read_anonymization_key (path, db->anonymize.key);
    if (!found && version)
      FATAL ("Missing visitor key '%s'; restore the original key with its database.", path);
    free (path);
  }
  if (!found)
    generate_anonymization_key (db->anonymize.key);
  if (version && anonymization_key_id () != key_id)
    FATAL ("Visitor key does not match this database; restore its original IP_PRIVACY.key.");
}

/* Write a key completely, retrying interrupted or short writes.
 *
 * On success, 1 is returned. On a write failure, 0 is returned. */
static int
write_anonymization_key (int fd, const uint8_t key[SIPHASH_KEY_SIZE]) {
  size_t offset = 0;
  ssize_t written = 0;

  while (offset < SIPHASH_KEY_SIZE) {
    written = write (fd, key + offset, SIPHASH_KEY_SIZE - offset);
    if (written < 0 && errno == EINTR)
      continue;
    if (written <= 0)
      return 0;
    offset += written;
  }
  return 1;
}

/* Save the key before any dependent data, preserving an existing key. */
void
persist_anonymization_key (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  uint8_t key[SIPHASH_KEY_SIZE];
  char *path = NULL, *tmp = NULL;
  size_t len = 0;
  int fd = -1, ret = 0;

  if (!conf.anonymize_ip)
    return;

  path = anonymization_key_path ();
  if (read_anonymization_key (path, key)) {
    if (memcmp (key, db->anonymize.key, sizeof (key)) != 0)
      FATAL ("Visitor key changed while processing '%s'.", path);
    free (path);
    return;
  }

  len = strlen (path);
  tmp = xmalloc (len + sizeof (ANONYMIZE_KEY_TEMP_SUFFIX));
  memcpy (tmp, path, len);
  memcpy (tmp + len, ANONYMIZE_KEY_TEMP_SUFFIX, sizeof (ANONYMIZE_KEY_TEMP_SUFFIX));
  if ((fd = mkstemp (tmp)) == -1)
    FATAL ("Unable to create visitor key '%s': %s", path, strerror (errno));
  ret = write_anonymization_key (fd, db->anonymize.key) && fsync (fd) == 0;
  if (close (fd) != 0)
    ret = 0;
  if (ret)
    ret = link (tmp, path) == 0;
  unlink (tmp);
  if (!ret)
    FATAL ("Unable to save visitor key '%s'.", path);
  free (tmp);
  free (path);
}

/* Key an existing fingerprint so legacy visitor sequences remain usable.
 *
 * On success, the non-zero fingerprint is returned; there is no failure case. */
uint64_t
anonymize_visitor (uint64_t fingerprint) {
  GKDB *db = NULL;
  uint8_t bytes[sizeof (fingerprint)];
  size_t i;
  uint64_t value = 0;

  if (!conf.anonymize_ip)
    return fingerprint;

  db = get_db_instance (DB_INSTANCE);
  /* Explicit byte order keeps restored IDs stable across architectures. */
  for (i = 0; i < sizeof (bytes); ++i)
    bytes[i] = fingerprint >> (i * 8);
  value = siphash24 (bytes, sizeof (bytes), db->anonymize.key);
  return value ? value : UINT64_MAX;
}

/* Produce a masked host label without retaining unrecognized host tokens. */
void
anonymize_host (const char *host, char masked[INET6_ADDRSTRLEN]) {
  if (!mask_ip_address (host, masked, INET6_ADDRSTRLEN, conf.anonymize_level))
    strcpy (masked, ANONYMIZED_UNKNOWN_HOST);
}

/* Identify database filenames containing host identities or visitor keys.
 *
 * On a matching filename, 1 is returned. Otherwise, 0 is returned. */
int
is_anonymized_database (const char *filename) {
  static const char *types[] = { "II32", "IS32", "IU64", "SI32", "U648", "IGSL", "SU64" };
  const char *part = strchr (filename, '_');
  size_t i, len = strlen (filename);

  if (strcmp (filename, "U6432_UNIQUE_KEYS.db") == 0 || strcmp (filename, "SI32_UNIQUE_KEYS.db") == 0)
    return 1;
  if (!part || strncmp (part, "_HOSTS_MTRC_", sizeof ("_HOSTS_MTRC_") - 1) != 0 ||
      len < sizeof (".db") || strcmp (filename + len - (sizeof (".db") - 1), ".db") != 0)
    return 0;

  for (i = 0; i < ARRAY_SIZE (types); ++i) {
    if ((size_t) (part - filename) == strlen (types[i]) && strncmp (filename, types[i], part - filename) == 0)
      return 1;
  }
  return 0;
}

/* Combine metrics for host rows that now share the same masked address. */
static void
merge_host_metrics (khash_t (imtv) *metrics, uint32_t from, uint32_t to) {
  khint_t source = kh_get (imtv, metrics, from);
  GKMetricVals saved = { 0 }, *target = NULL;

  if (source == kh_end (metrics))
    return;

  saved = kh_val (metrics, source);
  kh_del (imtv, metrics, source);
  if (!(target = ins_imtv (metrics, to)))
    FATAL ("Unable to combine anonymized host metrics.");
  target->hits += saved.hits;
  target->visitors += saved.visitors;
  target->bw += saved.bw;
  target->cumts += saved.cumts;
  target->maxts = MAX (target->maxts, saved.maxts);
  target->touched |= saved.touched;
}

/* Replace one day's raw host labels and keys with grouped, masked hosts. */
static void
anonymize_host_date (uint32_t date) {
  khash_t (ii32) *keys = get_hash (HOSTS, date, MTRC_KEYMAP);
  khash_t (is32) *data = get_hash (HOSTS, date, MTRC_DATAMAP);
  khash_t (imtv) *metrics = get_hash (HOSTS, date, MTRC_METRICS);
  char masked[INET6_ADDRSTRLEN];
  uint32_t hash = 0, survivor = 0, id = 0;
  khint_t k;

  if (!keys || !data || !metrics)
    return;

  kh_clear (ii32, keys);
  for (k = kh_begin (data); k != kh_end (data); ++k) {
    if (!kh_exist (data, k))
      continue;

    id = kh_key (data, k);
    anonymize_host (kh_val (data, k), masked);
    hash = djb2 ((const unsigned char *) masked);
    survivor = get_ii32 (keys, hash);
    free (kh_val (data, k));
    if (survivor) {
      kh_del (is32, data, k);
      merge_host_metrics (metrics, id, survivor);
      continue;
    }

    kh_val (data, k) = xstrdup (masked);
    if (ins_ii32 (keys, hash, id) == -1)
      FATAL ("Unable to store anonymized host key.");
  }

  /* HOSTS counts from visitor sequences, so it needs neither the old pair
   * set nor the per-IP agent lists after migration. */
  del_u648 (get_hash (HOSTS, date, MTRC_UNIQMAP), 0);
  del_igsl_free (get_hash (HOSTS, date, MTRC_AGENTS), 1);
  if (kh_resize (ii32, keys, kh_size (keys) * COMPACT_TABLE_SLACK) == -1 ||
      kh_resize (is32, data, kh_size (data) * COMPACT_TABLE_SLACK) == -1 ||
      kh_resize (imtv, metrics, kh_size (metrics) * COMPACT_TABLE_SLACK) == -1)
    FATAL ("Unable to compact anonymized host storage.");
}

/* Mask and combine legacy hosts one date at a time, before building caches. */
void
anonymize_restored_hosts (void) {
  GKDB *db = get_db_instance (DB_INSTANCE);
  khash_t (igkh) *dates = get_hdb (db, MTRC_DATES);
  khint_t k;

  if (!conf.anonymize_ip || db->anonymize.restored_level)
    return;

  for (k = kh_begin (dates); k != kh_end (dates); ++k) {
    if (kh_exist (dates, k))
      anonymize_host_date (kh_key (dates, k));
  }
}
