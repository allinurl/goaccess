/**
 * utm.h -- UTM campaign parameter handling
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_|\___/\___/\___/____/____/
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

#ifndef UTM_H_INCLUDED
#define UTM_H_INCLUDED

#include <stddef.h>

/* Classic medium, source, campaign, content, and term hierarchy depth. */
#define UTM_PRIMARY_LEVEL_COUNT 5

typedef enum GUTMLevel_ {
  UTM_MEDIUM,
  UTM_SOURCE,
  UTM_CAMPAIGN,
  UTM_CONTENT,
  UTM_TERM,
  UTM_SOURCE_PLATFORM,
  UTM_CREATIVE_FORMAT,
  UTM_MARKETING_TACTIC,
  UTM_CAMPAIGN_ID,
  UTM_LEVEL_COUNT,
} GUTMLevel;

typedef enum GUTMInput_ {
  UTM_INPUT_URI,
  UTM_INPUT_QUERY,
} GUTMInput;

typedef struct GUTM_ {
  char *value[UTM_LEVEL_COUNT];
  unsigned char missing[UTM_PRIMARY_LEVEL_COUNT];
} GUTM;

int decode_utm_path (const char *path, GUTM * utm);
int extract_utm (const char *input, GUTMInput type, GUTM * utm);
char *encode_utm_path (const GUTM * utm);
const char *get_utm_parameter_name (GUTMLevel level);
void free_utm (GUTM * utm);

#endif // for #ifndef UTM_H_INCLUDED
