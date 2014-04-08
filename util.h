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

#define REGEX_ERROR      100
#define DATE_LEN         12     /* date length */
#define KB               1024
#define MB               (KB * 1024)
#define GB               (MB * 1024)

#define MILS             1000ULL
#define SECS             1000000ULL
#define MINS             60000000ULL
#define HOUR             3600000000ULL

char *alloc_string (const char *str);
char *char_repeat (int n, char c);
char *char_replace (char *str, char o, char n);
char *clean_date (char *s);
char *clean_month (char *s);
char *convert_date (char *result, char *data, const char *from,
                    const char *to, int size);
char *deblank (char *str);
char *escape_str (const char *src);
char *filesize_str (unsigned long long log_size);
char *float_to_str (float num);
char *int_to_str (int d);
char *int_to_str (int num);
char *left_pad_str (const char *s, int indent);
char *replace_str (const char *str, const char *old, const char *new);
char *reverse_ip (char *str);
char *secs_to_str (int secs);
char *substring (const char *str, int begin, int len);
char *trim_str (char *str);
char *unescape_str (const char *src);
char *usecs_to_str (unsigned long long usec);
const char *get_continent_name_and_code (const char *continentid);
const char *verify_status_code (char *str);
const char *verify_status_code_type (const char *str);
int count_occurrences (const char *s1, char c);
int intlen (int num);
int invalid_ipaddr (char *str);
off_t file_size (const char *filename);
void str_to_upper (char *s);
void xstrncpy (char *dest, const char *source, const size_t dest_size);

#endif
