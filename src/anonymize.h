/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
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

#ifndef ANONYMIZE_H_INCLUDED
#define ANONYMIZE_H_INCLUDED

#include <arpa/inet.h>
#include <stdint.h>

#include "siphash.h"

/* Version 1 keys the existing visitor fingerprint and groups masked hosts. */
#define ANONYMIZE_VERSION 1
#define ANONYMIZE_KEY_FILE "IP_PRIVACY.key"
#define ANONYMIZE_DB_PREFIX "ANON_"
#define ANONYMIZE_DEFAULT 1
#define ANONYMIZE_STRONG 2
#define ANONYMIZE_PEDANTIC 3

typedef struct GAnonymize_ {
  uint8_t key[SIPHASH_KEY_SIZE];
  uint32_t restored_level;
} GAnonymize;

void init_anonymization (uint32_t version, uint32_t level, uint64_t key_id);
void persist_anonymization_key (void);
void anonymize_restored_hosts (void);
uint64_t anonymize_visitor (uint64_t fingerprint);
uint64_t anonymization_key_id (void);
void anonymize_host (const char *host, char masked[INET6_ADDRSTRLEN]);
int is_anonymized_database (const char *filename);

#endif
