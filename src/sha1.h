#ifndef SHA1_H_INCLUDED
#define SHA1_H_INCLUDED

#include <sys/types.h>
#include <stdint.h>

// From http://www.mirrors.wiretapped.net/security/cryptography/hashes/sha1/sha1.c

typedef struct {
  uint32_t state[5];
  uint32_t count[2];
  uint8_t buffer[64];
} SHA1_CTX;

extern void SHA1Init (SHA1_CTX * context);
extern void SHA1Update (SHA1_CTX * context, uint8_t * data, uint32_t len);
extern void SHA1Final (uint8_t digest[20], SHA1_CTX * context);

#endif // for #ifndef SHA1_H
