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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef JSON_H_INCLUDED
#define JSON_H_INCLUDED

#define TAB "\t\t\t\t\t\t\t\t\t\t\t"
#define NL "\n\n\n"

#include "parser.h"

typedef struct GJSON_ {
  char *buf;                    /* pointer to buffer */
  size_t size;                  /* size of malloc'd buffer */
  size_t offset;                /* current write offset */
} GJSON;

char *get_json (GHolder * holder, int escape_html);

void output_json (GHolder * holder, const char *filename);
void set_json_nlines (int nl);

void fpskeyival (FILE * fp, const char *key, int val, int sp, int last);
void fpskeysval (FILE * fp, const char *key, const char *val, int sp, int last);
void fpskeyaval (FILE * fp, const char *key, const char *val, int sp, int last);

void fpclose_arr (FILE * fp, int sp, int last);
void fpclose_obj (FILE * fp, int iisp, int last);
void fpjson (FILE * fp, const char *fmt, ...) __attribute__((format (printf, 2, 3)));
void fpopen_arr_attr (FILE * fp, const char *attr, int sp);
void fpopen_obj_attr (FILE * fp, const char *attr, int sp);
void fpopen_obj (FILE * fp, int iisp);

#endif
