/**
 * utm.c -- UTM campaign parameter handling
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_|\___/\___/\___/____/____/
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

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utm.h"

#include "settings.h"
#include "util.h"
#include "xmalloc.h"

#define UTM_LENGTH_SEPARATOR ':'
#define UTM_LEVEL_SEPARATOR  ','
/* Bytes below this value can alter terminal state instead of displaying text. */
#define UTM_ASCII_PRINTABLE_MIN 0x20
/* ASCII DEL is also a terminal control byte. */
#define UTM_ASCII_DELETE        0x7f

static const char *const utm_parameters[UTM_LEVEL_COUNT] = {
  "utm_medium",
  "utm_source",
  "utm_campaign",
  "utm_content",
  "utm_term",
  "utm_source_platform",
  "utm_creative_format",
  "utm_marketing_tactic",
  "utm_id",
};

/* Remove terminal-active ASCII control bytes from a decoded UTM value. */
static void
strip_utm_control_chars (char *value) {
  unsigned char *src = NULL;
  char *dst = NULL;

  src = (unsigned char *) value;
  dst = value;
  while (*src != '\0') {
    if (*src >= UTM_ASCII_PRINTABLE_MIN && *src != UTM_ASCII_DELETE)
      *dst++ = (char) *src;
    src++;
  }
  *dst = '\0';
}

/* Check whether at least one supported UTM value has been extracted.
 *
 * On success, non-zero is returned.
 * On failure, 0 is returned. */
static int
has_utm_value (const GUTM *utm) {
  size_t level = 0;

  for (level = 0; level < UTM_LEVEL_COUNT; level++) {
    if (utm->value[level] != NULL)
      return 1;
  }

  return 0;
}

/* Match a query parameter name to its UTM hierarchy level.
 *
 * On success, the matching level is assigned and 0 is returned.
 * On failure, 1 is returned. */
static int
get_utm_level (const char *name, size_t len, GUTMLevel *level) {
  size_t idx = 0, parameter_len = 0;

  for (idx = 0; idx < UTM_LEVEL_COUNT; idx++) {
    parameter_len = strlen (utm_parameters[idx]);
    if (len == parameter_len && strncmp (name, utm_parameters[idx], len) == 0) {
      *level = (GUTMLevel) idx;
      return 0;
    }
  }

  return 1;
}

/* Get the canonical query parameter name for a UTM hierarchy level.
 *
 * On success, the parameter name is returned.
 * On failure, NULL is returned. */
const char *
get_utm_parameter_name (GUTMLevel level) {
  if (level < UTM_MEDIUM || level >= UTM_LEVEL_COUNT)
    return NULL;

  return utm_parameters[level];
}

/* Decode and retain the first nonempty value for a UTM hierarchy level.
 *
 * On success, the decoded value is assigned and 0 is returned.
 * On failure, 1 is returned. */
static int
set_utm_value (GUTM *utm, GUTMLevel level, const char *value, size_t len) {
  char *decoded = NULL, *trimmed = NULL;

  if (utm->value[level] != NULL || len == 0)
    return 1;

  decoded = xmalloc (len + 1);
  memcpy (decoded, value, len);
  decoded[len] = '\0';
  decode_hex (decoded, decoded, 1);
  if (conf.double_decode)
    decode_hex (decoded, decoded, 1);

  strip_utm_control_chars (decoded);
  trimmed = trim_str (decoded);
  if (trimmed != decoded)
    memmove (decoded, trimmed, strlen (trimmed) + 1);
  if (*decoded == '\0') {
    free (decoded);
    return 1;
  }

  utm->value[level] = decoded;
  return 0;
}

/* Extract supported UTM parameters from a request URI or query string.
 *
 * On success, at least one UTM value is assigned and 0 is returned.
 * On failure, 1 is returned. */
int
extract_utm (const char *input, GUTMInput type, GUTM *utm) {
  const char *cursor = NULL, *end = NULL, *equals = NULL;
  GUTMLevel level = UTM_MEDIUM;
  size_t name_len = 0, value_len = 0;

  if (input == NULL || utm == NULL)
    return 1;

  if (type == UTM_INPUT_URI) {
    if (!(cursor = strchr (input, '?')))
      return 1;
    cursor++;
  } else {
    cursor = *input == '?' ? input + 1 : input;
  }

  while (*cursor != '\0') {
    end = cursor;
    while (*end != '\0' && *end != '&' && *end != '#' && !isspace ((unsigned char) *end))
      end++;

    equals = memchr (cursor, '=', (size_t) (end - cursor));
    if (equals != NULL) {
      name_len = (size_t) (equals - cursor);
      value_len = (size_t) (end - equals - 1);
      if (get_utm_level (cursor, name_len, &level) == 0)
        set_utm_value (utm, level, equals + 1, value_len);
    }

    if (*end != '&')
      break;
    cursor = end + 1;
  }

  return has_utm_value (utm) ? 0 : 1;
}

/* Encode the populated UTM hierarchy as an unambiguous storage key.
 *
 * On success, an allocated hierarchy path is returned.
 * On failure, NULL is returned. */
char *
encode_utm_path (const GUTM *utm) {
  const char *values[UTM_LEVEL_COUNT] = { NULL };
  char *path = NULL;
  unsigned char included[UTM_LEVEL_COUNT] = { 0 };
  int header_len = 0;
  size_t component_count = 0, deepest = UTM_PRIMARY_LEVEL_COUNT, idx = 0;
  size_t len = 0, offset = 0, total = 0;

  if (utm == NULL)
    return NULL;

  for (idx = UTM_PRIMARY_LEVEL_COUNT; idx > 0; idx--) {
    if (utm->value[idx - 1] != NULL) {
      deepest = idx - 1;
      break;
    }
  }

  if (deepest < UTM_PRIMARY_LEVEL_COUNT) {
    for (idx = 0; idx <= deepest; idx++) {
      values[idx] = utm->value[idx];
      included[idx] = 1;
      component_count++;
    }
  }

  for (idx = UTM_PRIMARY_LEVEL_COUNT; idx < UTM_LEVEL_COUNT; idx++) {
    if (utm->value[idx] == NULL)
      continue;

    values[idx] = utm->value[idx];
    included[idx] = 1;
    component_count++;
  }

  if (component_count == 0)
    return NULL;

  for (idx = 0; idx < UTM_LEVEL_COUNT; idx++) {
    if (!included[idx])
      continue;

    len = values[idx] != NULL ? strlen (values[idx]) : 0;
    header_len = snprintf (NULL, 0, "%zu%c%zu%c", idx, UTM_LEVEL_SEPARATOR, len,
                           UTM_LENGTH_SEPARATOR);
    if (header_len < 0 || (size_t) header_len > SIZE_MAX - total ||
        len >= SIZE_MAX - total - (size_t) header_len)
      return NULL;
    total += (size_t) header_len + len;
  }

  path = xmalloc (total + 1);
  for (idx = 0; idx < UTM_LEVEL_COUNT; idx++) {
    if (!included[idx])
      continue;

    len = values[idx] != NULL ? strlen (values[idx]) : 0;
    header_len = snprintf (path + offset, total + 1 - offset, "%zu%c%zu%c", idx,
                           UTM_LEVEL_SEPARATOR, len, UTM_LENGTH_SEPARATOR);
    offset += (size_t) header_len;
    if (len > 0)
      memcpy (path + offset, values[idx], len);
    offset += len;
  }
  path[offset] = '\0';

  return path;
}

/* Parse an unsigned number from the current UTM storage path position.
 *
 * On success, the number and next path position are assigned and 0 is returned.
 * On failure, 1 is returned. */
static int
parse_utm_number (const char **position, size_t *number) {
  const char *cursor = NULL;
  size_t digit = 0, value = 0;

  if (position == NULL || *position == NULL || number == NULL)
    return 1;

  cursor = *position;
  if (!isdigit ((unsigned char) *cursor))
    return 1;

  while (isdigit ((unsigned char) *cursor)) {
    digit = (size_t) (*cursor - '0');
    if (value > (SIZE_MAX - digit) / 10)
      return 1;
    value = (value * 10) + digit;
    cursor++;
  }

  *number = value;
  *position = cursor;
  return 0;
}

/* Release all values in a UTM hierarchy. */
void
free_utm (GUTM *utm) {
  size_t level = 0;

  if (utm == NULL)
    return;

  for (level = 0; level < UTM_LEVEL_COUNT; level++) {
    free (utm->value[level]);
    utm->value[level] = NULL;
  }
  memset (utm->missing, 0, sizeof (utm->missing));
}

/* Decode a stored UTM hierarchy path into its component values.
 *
 * On success, the component values are assigned and 0 is returned.
 * On failure, allocated values are released and 1 is returned. */
int
decode_utm_path (const char *path, GUTM *utm) {
  const char *cursor = NULL;
  size_t count = 0, len = 0, level = 0, remaining = 0;

  if (path == NULL || *path == '\0' || utm == NULL)
    return 1;

  memset (utm, 0, sizeof (*utm));
  cursor = path;
  while (*cursor != '\0' && count < UTM_LEVEL_COUNT) {
    if (parse_utm_number (&cursor, &level) == 1 || level >= UTM_LEVEL_COUNT ||
        utm->value[level] != NULL ||
        (level < UTM_PRIMARY_LEVEL_COUNT && utm->missing[level]) || *cursor != UTM_LEVEL_SEPARATOR)
      goto fail;
    cursor++;

    if (parse_utm_number (&cursor, &len) == 1 || *cursor != UTM_LENGTH_SEPARATOR)
      goto fail;
    cursor++;

    if (len == 0) {
      if (level >= UTM_PRIMARY_LEVEL_COUNT)
        goto fail;
      utm->missing[level] = 1;
      count++;
      continue;
    }

    remaining = strlen (cursor);
    if (len > remaining)
      goto fail;

    utm->value[level] = xmalloc (len + 1);
    memcpy (utm->value[level], cursor, len);
    utm->value[level][len] = '\0';
    cursor += len;
    count++;
  }

  if (*cursor != '\0' || count == 0)
    goto fail;

  return 0;

fail:
  free_utm (utm);
  return 1;
}
