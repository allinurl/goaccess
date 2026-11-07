/* SHA-256, adapted from Brad Conte's public-domain implementation:
 * https://github.com/B-Con/crypto-algorithms/blob/master/sha256.c
 * This implementation computes file fingerprints without an OpenSSL dependency. */

#include <string.h>

#include "sha256.h"

/* Block, state, schedule and length sizes specified by FIPS 180-4. */
#define SHA256_BLOCK_SIZE 64
#define SHA256_STATE_WORDS 8
#define SHA256_ROUNDS 64
#define SHA256_LENGTH_SIZE 8

#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32 - (b))))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT (x, 2) ^ ROTRIGHT (x, 13) ^ ROTRIGHT (x, 22))
#define EP1(x) (ROTRIGHT (x, 6) ^ ROTRIGHT (x, 11) ^ ROTRIGHT (x, 25))
#define SIG0(x) (ROTRIGHT (x, 7) ^ ROTRIGHT (x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT (x, 17) ^ ROTRIGHT (x, 19) ^ ((x) >> 10))

/* SHA-256 round constants from FIPS 180-4, section 4.2.2. */
static const uint32_t rounds[SHA256_ROUNDS] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/* Incorporate one complete block into the SHA-256 state. */
static void
sha256_transform (uint32_t state[SHA256_STATE_WORDS], const uint8_t *data) {
  uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
  uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
  uint32_t t1, t2, m[SHA256_ROUNDS];
  unsigned int i, j;

  for (i = 0, j = 0; i < SHA256_BLOCK_SIZE / sizeof (uint32_t); i++, j += sizeof (uint32_t))
    m[i] = ((uint32_t) data[j] << 24) | ((uint32_t) data[j + 1] << 16) |
      ((uint32_t) data[j + 2] << 8) | data[j + 3];
  for (; i < SHA256_ROUNDS; i++)
    m[i] = SIG1 (m[i - 2]) + m[i - 7] + SIG0 (m[i - 15]) + m[i - 16];

  for (i = 0; i < SHA256_ROUNDS; i++) {
    t1 = h + EP1 (e) + CH (e, f, g) + rounds[i] + m[i];
    t2 = EP0 (a) + MAJ (a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

/* Compute the SHA-256 digest of a byte buffer. */
void
sha256_digest (const void *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]) {
  const uint8_t *p = data;
  uint8_t block[SHA256_BLOCK_SIZE] = { 0 };
  /* Initial state specified by FIPS 180-4, section 5.3.3. */
  uint32_t state[SHA256_STATE_WORDS] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
  };
  uint64_t bits = (uint64_t) len * 8;
  size_t i;

  while (len >= SHA256_BLOCK_SIZE) {
    sha256_transform (state, p);
    p += SHA256_BLOCK_SIZE;
    len -= SHA256_BLOCK_SIZE;
  }
  if (len)
    memcpy (block, p, len);
  block[len] = 0x80;

  /* The padding bit and the length may require an extra block. */
  if (len >= SHA256_BLOCK_SIZE - SHA256_LENGTH_SIZE) {
    sha256_transform (state, block);
    memset (block, 0, sizeof (block));
  }
  for (i = 0; i < SHA256_LENGTH_SIZE; i++)
    block[SHA256_BLOCK_SIZE - 1 - i] = bits >> (i * 8);
  sha256_transform (state, block);

  for (i = 0; i < SHA256_DIGEST_SIZE; i++)
    digest[i] = state[i / sizeof (uint32_t)] >> (24 - (i % sizeof (uint32_t)) * 8);
}
