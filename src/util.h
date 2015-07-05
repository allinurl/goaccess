/**
 * Copyright (C) 2009-2014 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An Ncurses apache weblog analyzer & interactive viewer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * A copy of the GNU General Public License is attached to this
 * source distribution for its full text.
 *
 * Visit http://goaccess.prosoftcorp.com for new releases.
 */

#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define REGEX_ERROR 100
#define KB          1024
#define MB          (KB * 1024)
#define GB          (MB * 1024)

#define MILS        1000ULL
#define SECS        1000000ULL
#define MINS        60000000ULL
#define HOUR        3600000000ULL

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
char *float_to_str (float num);
char *get_global_config (void);
char *get_home (void);
char *ints_to_str (int a, int b);
char *int_to_str (int d);
char *left_pad_str (const char *s, int indent);
char *ltrim (char *s);
char *replace_str (const char *str, const char *old, const char *new);
char *rtrim (char *s);
char *secs_to_str (int secs);
char* strtoupper(char* str);
char *substring (const char *str, int begin, int len);
char *trim_str (char *str);
char *unescape_str (const char *src);
char *usecs_to_str (unsigned long long usec);
const char *verify_status_code (char *str);
const char *verify_status_code_type (const char *str);
int convert_date (char *res, char *data, const char *from, const char *to, int size);
int count_matches (const char *s1, char c);
int ignore_referer (const char *ref);
int intlen (int num);
int invalid_ipaddr (char *str, int *ipvx);
int ip_in_range (const char *ip);
int str_to_time (const char *str, const char *fmt, struct tm *tm);
int wc_match(char *wc, char * str);
off_t file_size (const char *filename);
uint32_t ip_to_binary (const char *ip);
void strip_newlines (char *str);
void xstrncpy (char *dest, const char *source, const size_t dest_size);

/* *INDENT-ON* */

#endif
