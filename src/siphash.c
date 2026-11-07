/*
   SipHash reference C implementation

   Copyright (c) 2012-2022 Jean-Philippe Aumasson
   <jeanphilippe.aumasson@gmail.com>
   Copyright (c) 2012-2014 Daniel J. Bernstein <djb@cr.yp.to>

   To the extent possible under law, the author(s) have dedicated all copyright
   and related and neighboring rights to this software to the public domain
   worldwide. This software is distributed without any warranty.

   You should have received a copy of the CC0 Public Domain Dedication along
   with
   this software. If not, see
   <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

/* Adapted from https://github.com/veorq/SipHash for 64-bit output. */

#include "siphash.h"

/* Compression/finalization rounds and constants from the SipHash-2-4 specification. */
#define SIPHASH_COMPRESSION_ROUNDS 2
#define SIPHASH_FINAL_ROUNDS 4
#define ROTL(x,b) (((x) << (b)) | ((x) >> (64 - (b))))
#define SIPROUND do { \
  v0 += v1; v1 = ROTL (v1, 13); v1 ^= v0; v0 = ROTL (v0, 32); \
  v2 += v3; v3 = ROTL (v3, 16); v3 ^= v2; \
  v0 += v3; v3 = ROTL (v3, 21); v3 ^= v0; \
  v2 += v1; v1 = ROTL (v1, 17); v1 ^= v2; v2 = ROTL (v2, 32); \
} while (0)

/* Decode an unaligned little-endian word.
 *
 * On success, the decoded word is returned; there is no failure case. */
static uint64_t
read_le64 (const uint8_t *p) {
  uint64_t value = 0;
  unsigned int i;

  for (i = 0; i < sizeof (value); ++i)
    value |= (uint64_t) p[i] << (i * 8);
  return value;
}

/* Calculate SipHash-2-4 with a 128-bit key.
 *
 * On success, the 64-bit hash is returned; there is no failure case. */
uint64_t
siphash24 (const void *data, size_t len, const uint8_t key[SIPHASH_KEY_SIZE]) {
  const uint8_t *p = data;
  uint64_t k0 = read_le64 (key), k1 = read_le64 (key + sizeof (uint64_t));
  uint64_t v0 = UINT64_C (0x736f6d6570736575) ^ k0, v1 = UINT64_C (0x646f72616e646f6d) ^ k1;
  uint64_t v2 = UINT64_C (0x6c7967656e657261) ^ k0, v3 = UINT64_C (0x7465646279746573) ^ k1;
  uint64_t last = (uint64_t) len << 56, word = 0;
  size_t i;

  while (len >= sizeof (word)) {
    word = read_le64 (p);
    v3 ^= word;
    for (i = 0; i < SIPHASH_COMPRESSION_ROUNDS; ++i)
      SIPROUND;
    v0 ^= word;
    p += sizeof (word);
    len -= sizeof (word);
  }
  for (i = 0; i < len; ++i)
    last |= (uint64_t) p[i] << (i * 8);

  v3 ^= last;
  for (i = 0; i < SIPHASH_COMPRESSION_ROUNDS; ++i)
    SIPROUND;
  v0 ^= last;
  v2 ^= 0xff;
  for (i = 0; i < SIPHASH_FINAL_ROUNDS; ++i)
    SIPROUND;

  return v0 ^ v1 ^ v2 ^ v3;
}
