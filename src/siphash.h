/* SipHash-2-4 interface. See siphash.c for public-domain attribution. */

#ifndef SIPHASH_H_INCLUDED
#define SIPHASH_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

/* SipHash-2-4 uses a 128-bit key and returns a 64-bit value. */
#define SIPHASH_KEY_SIZE 16

uint64_t siphash24 (const void *data, size_t len, const uint8_t key[SIPHASH_KEY_SIZE]);

#endif
