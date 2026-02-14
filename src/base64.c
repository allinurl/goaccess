/**
 * base64.c -- A basic base64 encode implementation
 *    _______       _______            __        __
 *   / ____/ |     / / ___/____  _____/ /_____  / /_
 *  / / __ | | /| / /\__ \/ __ \/ ___/ //_/ _ \/ __/
 * / /_/ / | |/ |/ /___/ / /_/ / /__/ ,< /  __/ /_
 * \____/  |__/|__//____/\____/\___/_/|_|\___/\__/
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

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "base64.h"
#include "xmalloc.h"

/* Encodes the given data with base64.
 *
 * On success, the encoded nul-terminated data, as a string is returned. */
char *
base64_encode (const void *buf, size_t size) {
  static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  char *str = (char *) xmalloc ((size + 3) * 4 / 3 + 1);

  char *p = str;
  const unsigned char *q = (const unsigned char *) buf;
  size_t i = 0;

  while (i < size) {
    int c = q[i++];
    c *= 256;
    if (i < size)
      c += q[i];
    i++;

    c *= 256;
    if (i < size)
      c += q[i];
    i++;

    *p++ = base64[(c & 0x00fc0000) >> 18];
    *p++ = base64[(c & 0x0003f000) >> 12];

    if (i > size + 1)
      *p++ = '=';
    else
      *p++ = base64[(c & 0x00000fc0) >> 6];

    if (i > size)
      *p++ = '=';
    else
      *p++ = base64[c & 0x0000003f];
  }

  *p = 0;

  return str;
}

/*
 * base64_decode
 *
 * Given a Base64 encoded string in 'data', this function decodes it into
 * a newly allocated binary buffer. The length of the decoded data is stored
 * in *out_len. The caller is responsible for freeing the returned buffer.
 *
 * Returns NULL on error (for example, if the data's length is not a multiple
 * of 4).
 */
char *
base64_decode (const char *data, size_t *out_len) {
  size_t decoded_len = 0, i = 0, j = 0, pad = 0, len = 0;
  char *out = NULL;
  uint32_t triple = 0;

  /* Create a lookup table for decoding.
   * For valid Base64 characters 'A'-'Z', 'a'-'z', '0'-'9', '+', '/',
   * we place their value; for '=' we simply return 0.
   * All other characters are marked as 0x80 (an invalid marker).
   */
  static const unsigned char dtable[256] = {
    ['A'] = 0,['B'] = 1,['C'] = 2,['D'] = 3,
    ['E'] = 4,['F'] = 5,['G'] = 6,['H'] = 7,
    ['I'] = 8,['J'] = 9,['K'] = 10,['L'] = 11,
    ['M'] = 12,['N'] = 13,['O'] = 14,['P'] = 15,
    ['Q'] = 16,['R'] = 17,['S'] = 18,['T'] = 19,
    ['U'] = 20,['V'] = 21,['W'] = 22,['X'] = 23,
    ['Y'] = 24,['Z'] = 25,
    ['a'] = 26,['b'] = 27,['c'] = 28,['d'] = 29,
    ['e'] = 30,['f'] = 31,['g'] = 32,['h'] = 33,
    ['i'] = 34,['j'] = 35,['k'] = 36,['l'] = 37,
    ['m'] = 38,['n'] = 39,['o'] = 40,['p'] = 41,
    ['q'] = 42,['r'] = 43,['s'] = 44,['t'] = 45,
    ['u'] = 46,['v'] = 47,['w'] = 48,['x'] = 49,
    ['y'] = 50,['z'] = 51,
    ['0'] = 52,['1'] = 53,['2'] = 54,['3'] = 55,
    ['4'] = 56,['5'] = 57,['6'] = 58,['7'] = 59,
    ['8'] = 60,['9'] = 61,
    ['+'] = 62,['/'] = 63,
    ['='] = 0,
    /* All other values are implicitly 0 (or you can mark them as invalid
       by setting them to 0x80 if you prefer stricter checking). */
  };

  len = strlen (data);
  /* Validate length: Base64 encoded data must be a multiple of 4 */
  if (len % 4 != 0)
    return NULL;

  /* Count padding characters at the end */
  if (len) {
    if (data[len - 1] == '=')
      pad++;
    if (len > 1 && data[len - 2] == '=')
      pad++;
  }

  /* Calculate the length of the decoded data */
  decoded_len = (len / 4) * 3 - pad;
  out = (char *) xmalloc (decoded_len + 1); /* +1 for a null terminator if needed */

  for (i = 0, j = 0; i < len;) {
    unsigned int sextet_a = data[i] == '=' ? 0 : dtable[(unsigned char) data[i]];
    unsigned int sextet_b = data[i + 1] == '=' ? 0 : dtable[(unsigned char) data[i + 1]];
    unsigned int sextet_c = data[i + 2] == '=' ? 0 : dtable[(unsigned char) data[i + 2]];
    unsigned int sextet_d = data[i + 3] == '=' ? 0 : dtable[(unsigned char) data[i + 3]];
    i += 4;

    triple = (sextet_a << 18) | (sextet_b << 12) | (sextet_c << 6) | sextet_d;
    if (j < decoded_len)
      out[j++] = (triple >> 16) & 0xFF;
    if (j < decoded_len)
      out[j++] = (triple >> 8) & 0xFF;
    if (j < decoded_len)
      out[j++] = triple & 0xFF;
  }

  out[decoded_len] = '\0'; /* Null-terminate the output buffer */
  if (out_len)
    *out_len = decoded_len;
  return out;
}

/*
 * base64UrlEncode - Converts a standard Base64 encoded string
 * into Base64Url format.
 *
 * This replaces '+' with '-', '/' with '_', and removes '=' padding.
 */
char *
base64UrlEncode (const char *base64) {
  char *url = NULL;
  size_t len = 0;
  if (!base64)
    return NULL;

  // Duplicate the input string to modify it in-place.
  url = strdup (base64);
  if (!url)
    return NULL;

  // Replace characters: '+' => '-', '/' => '_'
  for (char *p = url; *p; p++) {
    if (*p == '+')
      *p = '-';
    else if (*p == '/')
      *p = '_';
  }

  // Remove trailing '=' characters.
  len = strlen (url);
  while (len && url[len - 1] == '=') {
    url[len - 1] = '\0';
    len--;
  }
  return url;
}

/*
 * base64UrlDecode - Converts a Base64Url encoded string into standard Base64 format.
 *
 * This replaces '-' with '+', '_' with '/', and adds the necessary '=' padding.
 * Caller is responsible for freeing the returned string.
 */
char *
base64UrlDecode (const char *base64Url) {
  size_t len = 0, padding = 0, i = 0;
  char *padded = NULL, *base64 = NULL;

  if (!base64Url)
    return NULL;

  // Duplicate the input so we can modify it.
  base64 = strdup (base64Url);
  if (!base64)
    return NULL;

  // Replace '-' with '+' and '_' with '/'
  for (char *p = base64; *p; p++) {
    if (*p == '-')
      *p = '+';
    else if (*p == '_')
      *p = '/';
  }

  // Calculate the padding required (base64 length must be a multiple of 4)
  len = strlen (base64);
  padding = (4 - (len % 4)) % 4;
  padded = xmalloc (len + padding + 1);
  if (!padded) {
    free (base64);
    return NULL;
  }
  strcpy (padded, base64);
  for (i = 0; i < padding; i++) {
    padded[len + i] = '=';
  }
  padded[len + padding] = '\0';
  free (base64);
  return padded;
}
