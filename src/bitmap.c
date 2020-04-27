/**
 * bitmap.c -- A quick bitmap implementation
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

#include <stdlib.h>
#include <string.h>
#include <limits.h>     /* for CHAR_BIT */
#include <stdint.h>

#include "error.h"
#include "xmalloc.h"

#include "bitmap.h"

/*
 * Returns the hamming weight (i.e. the number of bits set) in a word.
 * NOTE: This routine borrowed from Linux 2.4.9 <linux/bitops.h>.
 */
static uint32_t
hweight (uint32_t w) {
  uint32_t res;

  res = (w & 0x55555555) + ((w >> 1) & 0x55555555);
  res = (res & 0x33333333) + ((res >> 2) & 0x33333333);
  res = (res & 0x0F0F0F0F) + ((res >> 4) & 0x0F0F0F0F);
  res = (res & 0x00FF00FF) + ((res >> 8) & 0x00FF00FF);
  res = (res & 0x0000FFFF) + ((res >> 16) & 0x0000FFFF);

  return res;
}


/* Returns the number of bytes required for bm->len bits. */
inline uint32_t
bitmap_sizeof (uint32_t i) {
  return bitmap_word (i) * sizeof (word_t);
}

void
free_bitmap (bitmap * bm) {
  free ((uint32_t *) bm->bmp);
  free (bm);
}

int
bitmap_set_bit (word_t * words, uint32_t n) {
  words[WORD_OFFSET (n)] |= ((word_t) 1 << BIT_OFFSET (n));
  return 0;
}

int
bitmap_get_bit (word_t * words, uint32_t n) {
  word_t bit = words[WORD_OFFSET (n)] & ((word_t) 1 << BIT_OFFSET (n));
  return bit != 0;
}

uint32_t
bitmap_count_set (const bitmap * bm) {
  uint32_t i, n = 0, len = 0;

  if (!bm)
    return 0;

  len = bitmap_word (bm->len);
  for (i = 0; i < len; ++i)
    n += hweight (bm->bmp[i]);

  return n;
}

uint32_t
bitmap_ffs (bitmap * bm) {
  uint32_t i, pos = 1, len = 0;
  uint32_t __bitset;

  if (!bm)
    return 0;

  len = bitmap_word (bm->len);
  for (i = 0; i < len; ++i) {
    __bitset = bm->bmp[i];
    if ((pos = __builtin_ffs (__bitset)))
      break;
  }

  return (BITS_PER_WORD * i) + pos;
}

bitmap *
bitmap_create (uint32_t bit) {
  bitmap *bm = xcalloc (1, sizeof (bitmap));
  uint32_t bytes = bitmap_sizeof (bit);

  bm->bmp = xmalloc (bytes);
  memset (bm->bmp, 0, bytes);
  bm->len = bit;

  return bm;
}

int
bitmap_realloc (bitmap * bm, uint32_t bit) {
  uint32_t *tmp = NULL;
  uint32_t oldlen = 0, newlen = 0;

  newlen = bitmap_sizeof (bit);
  oldlen = bitmap_sizeof (bm->len);
  if (newlen <= oldlen)
    return 1;

  tmp = realloc (bm->bmp, newlen);
  if ((tmp == NULL && newlen > 0) || bit < bm->len)
    FATAL ("Unable to realloc bitmap hash value %u %u", newlen, bm->len);

  LOG_DEBUG (("bit: %d, bm->len: %d, oldlen: %d, newlen: %d\n", bit, bm->len, oldlen, newlen));
  memset (tmp + bitmap_word (bm->len), 0, (newlen - oldlen));
  bm->len = bit;
  bm->bmp = tmp;

  return 0;
}

bitmap *
bitmap_copy (const bitmap * bm) {
  bitmap *ret;

  if (!bm)
    return NULL;
  if (!(ret = bitmap_create (bm->len)))
    return NULL;

  memcpy (ret->bmp, bm->bmp, bitmap_sizeof (bm->len));

  return ret;
}

int
bitmap_key_exists (bitmap * bm, uint32_t bit) {
  if (bm->len < bit)
    bitmap_realloc (bm, bit);

  /* if bit set, then it's the same visitor */
  if (bitmap_get_bit (bm->bmp, bit - 1))
    return 1;
  bitmap_set_bit (bm->bmp, bit - 1);

  return 0;
}
