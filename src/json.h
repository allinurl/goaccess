/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2016 Gerardo Orellana <hello @ goaccess.io>
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

char *get_json (GLog * logger, GHolder * holder);

void output_json (GLog * logger, GHolder * holder, const char *filename);
void set_json_nlines (int nl);

void pskeyfval (FILE * fp, const char *key, float val, int sp, int last);
void pskeyival (FILE * fp, const char *key, int val, int sp, int last);
void pskeysval (FILE * fp, const char *key, const char *val, int sp, int last);
void pskeyu64val (FILE * fp, const char *key, uint64_t val, int sp, int last);

void pclose_arr (FILE * fp, int sp, int last);
void pclose_obj (FILE * fp, int iisp, int last);
void pjson (FILE * fp, const char *fmt, ...);
void popen_arr_attr (FILE * fp, const char *attr, int sp);
void popen_obj_attr (FILE * fp, const char *attr, int sp);
void popen_obj (FILE * fp, int iisp);

#endif
