/* SHA-256 interface for the adaptation of Brad Conte's public-domain code.
 * See sha256.c for attribution. */

#ifndef SHA256_H_INCLUDED
#define SHA256_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

/* SHA-256 produces a 256-bit digest, independent of the input length. */
#define SHA256_DIGEST_SIZE 32

void sha256_digest (const void *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]);

#endif
