/**
 * base64.c -- A basic base64 encode implementation
 *    _______       _______            __        __
 *   / ____/ |     / / ___/____  _____/ /_____  / /_
 *  / / __ | | /| / /\__ \/ __ \/ ___/ //_/ _ \/ __/
 * / /_/ / | |/ |/ /___/ / /_/ / /__/ ,< /  __/ /_
 * \____/  |__/|__//____/\____/\___/_/|_|\___/\__/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2023 Gerardo Orellana <hello @ goaccess.io>
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

#include "base64.h"
#include "xmalloc.h"

/* Encodes the given data with base64..
 *
 * On success, the encoded nul-terminated data, as a string is returned. */
char *
base64_encode (const void *buf, size_t size) {
  static const char base64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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
