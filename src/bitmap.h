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


#ifndef BITMAP_H_INCLUDED
#define BITMAP_H_INCLUDED

#include <limits.h> /* for CHAR_BIT */

typedef uint32_t word_t;        // I want to change this, from uint32_t to uint64_t
enum { BITS_PER_WORD = sizeof (word_t) * CHAR_BIT };
#define WORD_OFFSET(b) ((b) / BITS_PER_WORD)
#define BIT_OFFSET(b)  ((b) % BITS_PER_WORD)

typedef struct bitmap_ {
  uint32_t *bmp;
  uint32_t len; /** Length of the bitmap, in bits */
} bitmap;

static inline uint32_t
bitmap_word (uint32_t i) {
  return (((i) + (BITS_PER_WORD) - 1) / (BITS_PER_WORD));
}

#define BITMAP_FOREACH(bm, pos, code) { size_t __k; \
  int __r; uint32_t __bitset, __t;                  \
  uint32_t __len = bitmap_word(bm->len);            \
  for (__k = 0; __k < __len; ++__k) {               \
    __bitset = bm->bmp[__k];                        \
    while (__bitset != 0) {                         \
      __t = __bitset & -__bitset;                   \
      __r = __builtin_ctzl (__bitset);              \
      pos = (__k * 32) + __r + 1;                   \
      code;                                         \
      __bitset ^= __t;                              \
    }                                               \
  } }

bitmap *bitmap_copy (const bitmap * bm);
bitmap *bitmap_create (uint32_t bit);
int bitmap_get_bit (word_t * words, uint32_t n);
int bitmap_key_exists (bitmap * bm, uint32_t bit);
int bitmap_realloc (bitmap * bm, uint32_t bit);
int bitmap_set_bit (word_t * words, uint32_t n);
uint32_t bitmap_count_set (const bitmap * bm);
uint32_t bitmap_ffs (bitmap * bm);
uint32_t bitmap_sizeof (uint32_t nbits);
void free_bitmap (bitmap * bm);

#endif // for #ifndef BITMAP_H
