/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2024 Gerardo Orellana <hello @ goaccess.io>
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

#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define REGEX_ERROR 100

#define KIB(n) (n << 10)
#define MIB(n) (n << 20)
#define GIB(n) (n << 30)
#define TIB(n) (n << 40)
#define PIB(n) (n << 50)

#define MILS 1000ULL
#define SECS 1000000ULL
#define MINS 60000000ULL
#define HOUR 3600000000ULL
#define DAY  86400000000ULL
#define TZ_NAME_LEN 48

#define RAND_FN 7 + 1

/* Convenient macros */
#define MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define MAX(a,b) (((a)>(b))?(a):(b))

/* *INDENT-OFF* */
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

char *alloc_string (const char *str);
char *char_repeat (int n, char c);
char *char_replace (char *str, char o, char n);
char *deblank (char *str);
char *escape_str (const char *src);
char *filesize_str (unsigned long long log_size);
char *float2str (float d, int width);
char *get_global_config (void);
char *get_user_config (void);
char *get_visitors_date (const char *odate, const char *from, const char *to);
char *int2str (int d, int width);
char *left_pad_str (const char *s, int indent);
char *ltrim (char *s);
char *regex_extract_string (const char *str, const char *regex, int max_groups, char const **err);
char *rtrim (char *s);
char *strtoupper (char *str);
char *substring (const char *str, int begin, int len);
char *trim_str (char *str);
char *u322str (uint32_t d, int width);
char *u642str (uint64_t d, int width);
char *unescape_str (const char *src);
char *usecs_to_str (unsigned long long usec);
const char *verify_status_code (int code);
const char *verify_status_code_type (int code);
int convert_date (char *res, const char *data, const char *from, const char *to, int size);
int count_matches (const char *s1, char c);
int find_output_type (char **filename, const char *ext, int alloc);
int hide_referer (const char *host);
int ignore_referer (const char *host);
int intlen (uint64_t num);
int invalid_ipaddr (const char *str, int *ipvx);
int ip_in_range (const char *ip);
int ptr2int (char *ptr);
int str2int (const char *date);
int str_inarray (const char *s, const char *arr[], int size);
int str_to_time (const char *str, const char *fmt, struct tm *tm, int tz);
int valid_output_type (const char *filename);
off_t file_size (const char *filename);
size_t append_str (char **dest, const char *src);
uint32_t djb2 (const unsigned char *str);
uint64_t u64encode (uint32_t x, uint32_t y);
void genstr (char *dest, size_t len);
void set_tz (void);
void strip_newlines (char *str);
void u64decode (uint64_t n, uint32_t * x, uint32_t * y);
void xstrncpy (char *dest, const char *source, const size_t dest_size);

/* *INDENT-ON* */

#endif
