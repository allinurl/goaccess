/**
 * util.c -- a set of handy functions to help parsing
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

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#define _GNU_SOURCE
#define _DEFAULT_SOURCE

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <inttypes.h>
#include <regex.h>
#include <pthread.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include "util.h"

#include "error.h"
#include "labels.h"
#include "xmalloc.h"

pthread_mutex_t tz_mutex = PTHREAD_MUTEX_INITIALIZER;

/* HTTP status codes categories */
static const char *code_type[] = {
  STATUS_CODE_0XX,
  STATUS_CODE_1XX,
  STATUS_CODE_2XX,
  STATUS_CODE_3XX,
  STATUS_CODE_4XX,
  STATUS_CODE_5XX,
};

/* HTTP status codes */
static const char *codes[1000] = {
  [0] = STATUS_CODE_0,
  [100] = STATUS_CODE_100, STATUS_CODE_101,
  [200] = STATUS_CODE_200, STATUS_CODE_201, STATUS_CODE_202, STATUS_CODE_203, STATUS_CODE_204,
  [205] = STATUS_CODE_205, STATUS_CODE_206, STATUS_CODE_207, STATUS_CODE_208,
  [218] = STATUS_CODE_218,
  [300] = STATUS_CODE_300, STATUS_CODE_301, STATUS_CODE_302, STATUS_CODE_303, STATUS_CODE_304,
  [305] = STATUS_CODE_305, NULL, STATUS_CODE_307, STATUS_CODE_308,
  [400] = STATUS_CODE_400, STATUS_CODE_401, STATUS_CODE_402, STATUS_CODE_403, STATUS_CODE_404,
  [405] = STATUS_CODE_405, STATUS_CODE_406, STATUS_CODE_407, STATUS_CODE_408, STATUS_CODE_409,
  [410] = STATUS_CODE_410, STATUS_CODE_411, STATUS_CODE_412, STATUS_CODE_413, STATUS_CODE_414,
  [415] = STATUS_CODE_415, STATUS_CODE_416, STATUS_CODE_417, STATUS_CODE_418, STATUS_CODE_419,
  [420] = STATUS_CODE_420, STATUS_CODE_421, STATUS_CODE_422, STATUS_CODE_423, STATUS_CODE_424,
  [425] = NULL, STATUS_CODE_426, NULL, STATUS_CODE_428, STATUS_CODE_429, STATUS_CODE_430,
  [431] = STATUS_CODE_431,
  [440] = STATUS_CODE_440,
  [444] = STATUS_CODE_444,
  [449] = STATUS_CODE_449,
  [450] = STATUS_CODE_450,
  [451] = STATUS_CODE_451,
  [460] = STATUS_CODE_460, STATUS_CODE_463, STATUS_CODE_464,
  [494] = STATUS_CODE_494,
  [495] = STATUS_CODE_495, STATUS_CODE_496, STATUS_CODE_497, STATUS_CODE_498, STATUS_CODE_499,
  [500] = STATUS_CODE_500, STATUS_CODE_501, STATUS_CODE_502, STATUS_CODE_503, STATUS_CODE_504,
  [505] = STATUS_CODE_505,
  [509] = STATUS_CODE_509,
  [520] = STATUS_CODE_520, STATUS_CODE_521, STATUS_CODE_522, STATUS_CODE_523, STATUS_CODE_524,
  STATUS_CODE_525, STATUS_CODE_526, STATUS_CODE_527, STATUS_CODE_529,
  [530] = STATUS_CODE_530,
  [540] = STATUS_CODE_540,
  [561] = STATUS_CODE_561,
  [590] = STATUS_CODE_598, STATUS_CODE_599,
  [999] = NULL
};

/* Return part of a string
 *
 * On error NULL is returned.
 * On success the extracted part of string is returned */
char *
substring (const char *str, int begin, int len) {
  char *buffer;
  if (str == NULL)
    return NULL;
  if (begin < 0)
    begin = strlen (str) + begin;
  if (begin < 0)
    begin = 0;
  if (len < 0)
    len = 0;
  if (((size_t) begin) > strlen (str))
    begin = strlen (str);
  if (((size_t) len) > strlen (&str[begin]))
    len = strlen (&str[begin]);
  if ((buffer = xmalloc (len + 1)) == NULL)
    return NULL;
  memcpy (buffer, &(str[begin]), len);
  buffer[len] = '\0';

  return buffer;
}

/* A pointer to the allocated memory of the new string
 *
 * On success, a pointer to a new string is returned */
char *
alloc_string (const char *str) {
  char *new = xmalloc (strlen (str) + 1);
  strcpy (new, str);
  return new;
}

/* A wrapper function to copy the first num characters of source to
 * destination. */
void
xstrncpy (char *dest, const char *source, const size_t dest_size) {
  strncpy (dest, source, dest_size);
  if (dest_size > 0) {
    dest[dest_size - 1] = '\0';
  } else {
    dest[0] = '\0';
  }
}

/* A random string generator. */
void
genstr (char *dest, size_t len) {
  char set[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

  for (; len > 0; --len)
    *dest++ = set[rand () % (sizeof (set) - 1)];
  *dest = '\0';
}

/* Count the number of matches on the string `s1` given a character `c`
 *
 * If the character is not found, 0 is returned
 * On success, the number of characters found */
int
count_matches (const char *s1, char c) {
  const char *ptr = s1;
  int n = 0;
  do {
    if (*ptr == c)
      n++;
  } while (*(ptr++));
  return n;
}

/* Simple but efficient uint32_t hashing. */
#if defined(__clang__) && defined(__clang_major__) && (__clang_major__ >= 4)
__attribute__((no_sanitize ("unsigned-integer-overflow")))
#if (__clang_major__ >= 12)
  __attribute__((no_sanitize ("unsigned-shift-base")))
#endif
#endif
  uint32_t
djb2 (const unsigned char *str) {
  uint32_t hash = 5381;
  int c;

  while ((c = *str++))
    hash = ((hash << 5) + hash) + c;    /* hash * 33 + c */

  return hash;
}

/* String matching where one string contains wildcard characters.
 *
 * If no match found, 1 is returned.
 * If match found, 0 is returned. */
static int
wc_match (const char *wc, char *str) {
  while (*wc && *str) {
    if (*wc == '*') {
      while (*wc && *wc == '*')
        wc++;
      if (!*wc)
        return 1;

      while (*str && *str != *wc)
        str++;
    } else if (*wc == '?' || *wc == *str) {
      wc++;
      str++;
    } else {
      break;
    }
  }
  if (!*wc && !*str)
    return 1;
  return 0;
}

/**
 * Extracts the hostname part from a given URL.
 *
 * On error, NULL is returned.
 * On success, a dynamically allocated string containing the hostname is returned.
 */
static char *
extract_hostname (const char *url) {
  char *start, *end;
  char *hostname = NULL;

  start = strstr (url, "://");
  if (start != NULL) {
    start += 3;
  } else {
    start = (char *) url;
  }

  end = strchr (start, '/');
  if (end == NULL) {
    /* no path, use the entire string */
    end = start + strlen (start);
  }

  hostname = xmalloc (end - start + 1);
  strncpy (hostname, start, end - start);
  hostname[end - start] = '\0';

  return hostname;
}

/* Generic routine to extract all groups from a string given a POSIX regex.
 *
 * If no match found or error, NULL is returned.
 * If match found, a string is returned. */
char *
regex_extract_string (const char *str, const char *regex, int max_groups, char const **err) {
  char *copy = NULL, *dest = NULL;
  int i, ret = 0;
  regex_t re;
  regmatch_t groups[max_groups];

  if (str == NULL || *str == '\0') {
    *err = "Invalid string.";
    return NULL;
  }

  if (regcomp (&re, regex, REG_EXTENDED)) {
    *err = "Unable to compile regular expression upon extraction";
    return NULL;
  }

  ret = regexec (&re, str, max_groups, groups, 0);
  if (ret == REG_NOMATCH) {
    *err = "Unable to match regular expression extraction.";
    goto out;
  }

  if (ret != 0) {
    *err = "Error while matching regular expression extraction.";
    goto out;
  }

  dest = xstrdup ("");
  for (i = 0; i < max_groups; ++i) {
    if (groups[i].rm_so == -1)
      break;

    copy = xstrdup (str);
    copy[groups[i].rm_eo] = 0;

    append_str (&dest, copy + groups[i].rm_so);
    free (copy);
  }

out:
  regfree (&re);

  return ret == 0 ? dest : NULL;
}

static int
handle_referer (const char *host, const char **referers, int referer_idx) {
  char *needle = NULL, *hostname = NULL;
  int i, ignore = 0;

  if (referer_idx == 0)
    return 0;

  if (host == NULL || *host == '\0')
    return 0;

  needle = xstrdup (host);
  for (i = 0; i < referer_idx; ++i) {
    if (referers[i] == NULL || *referers[i] == '\0')
      continue;

    if (strchr (referers[i], '*') != NULL || strchr (referers[i], '?') != NULL) {
      if (wc_match (referers[i], needle)) {
        ignore = 1;
        goto out;
      }
    } else {
      hostname = extract_hostname (host);
      if (strcmp (referers[i], hostname) == 0) {
        ignore = 1;
        free (hostname);
        goto out;
      }
      free (hostname);
    }
  }

out:
  free (needle);
  return ignore;
}

/* Determine if the given host needs to be ignored given the list of
 * referrers to ignore.
 *
 * On error, or the referrer is not found, 0 is returned
 * On success, or if the host needs to be ignored, 1 is returned */
int
ignore_referer (const char *host) {
  return handle_referer (host, conf.ignore_referers, conf.ignore_referer_idx);
}

/* Determine if the given host needs to be hidden given the list of
 * referrers to hide.
 *
 * On error, or the referrer is not found, 0 is returned
 * On success, or if the host needs to be ignored, 1 is returned */
int
hide_referer (const char *host) {
  return handle_referer (host, conf.hide_referers, conf.hide_referer_idx);
}

/* Determine if the given ip is within a range of IPs.
 *
 * On error, or not within the range, 0 is returned
 * On success, or if within the range, 1 is returned */
static int
within_range (const char *ip, const char *start, const char *end) {
  struct in6_addr addr6, start6, end6;
  struct in_addr addr4, start4, end4;

  if (start == NULL || *start == '\0')
    return 0;
  if (end == NULL || *end == '\0')
    return 0;
  if (ip == NULL || *ip == '\0')
    return 0;

  /* IPv4 */
  if (1 == inet_pton (AF_INET, ip, &addr4)) {
    if (1 != inet_pton (AF_INET, start, &start4))
      return 0;
    if (1 != inet_pton (AF_INET, end, &end4))
      return 0;
    if (memcmp (&addr4, &start4, sizeof (addr4)) >= 0 &&
        memcmp (&addr4, &end4, sizeof (addr4)) <= 0)
      return 1;
  }
  /* IPv6 */
  else if (1 == inet_pton (AF_INET6, ip, &addr6)) {
    if (1 != inet_pton (AF_INET6, start, &start6))
      return 0;
    if (1 != inet_pton (AF_INET6, end, &end6))
      return 0;
    if (memcmp (&addr6, &start6, sizeof (addr6)) >= 0 &&
        memcmp (&addr6, &end6, sizeof (addr6)) <= 0)
      return 1;
  }

  return 0;
}

/* Determine if the given IP needs to be ignored given the list of IPs
 * to ignore.
 *
 * On error, or not within the range, 0 is returned
 * On success, or if within the range, 1 is returned */
int
ip_in_range (const char *ip) {
  char *start, *end, *dash;
  int i;

  for (i = 0; i < conf.ignore_ip_idx; ++i) {
    end = NULL;
    if (conf.ignore_ips[i] == NULL || *conf.ignore_ips[i] == '\0')
      continue;

    start = xstrdup (conf.ignore_ips[i]);
    /* split range */
    if ((dash = strchr (start, '-')) != NULL) {
      *dash = '\0';
      end = dash + 1;
    }

    /* matches single IP */
    if (end == NULL) {
      if (strcmp (ip, start) == 0) {
        free (start);
        return 1;
      }
    }
    /* within range */
    else {
      if (within_range (ip, start, end)) {
        free (start);
        return 1;
      }
    }
    free (start);
  }

  return 0;
}

/* Searches the array of output formats for the given extension value.
 *
 * If not found, 1 is returned.
 * On success, the given filename is malloc'd and assigned and 0 is
 * returned. */
int
find_output_type (char **filename, const char *ext, int alloc) {
  int i;
  const char *dot = NULL;

  for (i = 0; i < conf.output_format_idx; ++i) {
    /* for backwards compatibility. i.e., -o json  */
    if (strcmp (conf.output_formats[i], ext) == 0)
      return 0;

    if ((dot = strrchr (conf.output_formats[i], '.')) != NULL && strcmp (dot + 1, ext) == 0) {
      if (alloc)
        *filename = xstrdup (conf.output_formats[i]);
      return 0;
    }
  }

  return 1;
}

/* Validates the '-o' filename extension for any of:
 * 1) .csv
 * 2) .json
 * 3) .html
 *
 * Return Value
 * 1: valid
 * 0: invalid
 * -1: non-existent extension
 */
int
valid_output_type (const char *filename) {
  const char *ext = NULL;
  size_t sl;

  if ((ext = strrchr (filename, '.')) == NULL)
    return -1;

  ext++;
  /* Is extension 3<=len<=4? */
  sl = strlen (ext);
  if (sl < 3 || sl > 4)
    return 0;

  if (strcmp ("html", ext) == 0)
    return 1;

  if (strcmp ("json", ext) == 0)
    return 1;

  if (strcmp ("csv", ext) == 0)
    return 1;

  return 0;
}

/* Get the path to the user config file (ie. HOME/.goaccessrc).
 *
 * On error, it returns NULL.
 * On success, the path of the user config file is returned. */
char *
get_user_config (void) {
  char *user_home = NULL, *path = NULL;
  size_t len;

  user_home = getenv ("HOME");
  if (user_home == NULL)
    return NULL;

  len = snprintf (NULL, 0, "%s/.goaccessrc", user_home) + 1;
  path = xmalloc (len);
  snprintf (path, len, "%s/.goaccessrc", user_home);

  return path;
}

/* Get the path to the global config file.
 *
 * On success, the path of the global config file is returned. */
char *
get_global_config (void) {
  char *path = NULL;
  size_t len;

  len = snprintf (NULL, 0, "%s/goaccess/goaccess.conf", SYSCONFDIR) + 1;
  path = xmalloc (len);
  snprintf (path, len, "%s/goaccess/goaccess.conf", SYSCONFDIR);

  return path;
}

/* A self-checking wrapper to convert_date().
 *
 * On error, a newly malloc'd '---' string is returned.
 * On success, a malloc'd 'Ymd' date is returned. */
char *
get_visitors_date (const char *odate, const char *from, const char *to) {
  char date[DATE_TIME] = "";    /* Ymd */

  memset (date, 0, sizeof *date);
  /* verify we have a valid date conversion */
  if (convert_date (date, odate, from, to, DATE_TIME) == 0)
    return xstrdup (date);

  LOG_DEBUG (("invalid date: %s", odate));
  return xstrdup ("---");
}

static time_t
tm2time (const struct tm *src) {
  struct tm tmp;

  tmp = *src;
  return timegm (&tmp) - src->tm_gmtoff;
}

void
set_tz (void) {
  /* this will persist for the duration of the program but also assumes that all
   * threads have the same conf.tz_name values */
  static char tz[TZ_NAME_LEN] = { 0 };

  if (!conf.tz_name)
    return;

  if (pthread_mutex_lock (&tz_mutex) != 0) {
    LOG_DEBUG (("Failed to acquire tz_mutex"));
    return;
  }

  snprintf (tz, TZ_NAME_LEN, "TZ=%s", conf.tz_name);
  if ((putenv (tz)) != 0) {
    int old_errno = errno;
    LOG_DEBUG (("Can't set TZ env variable %s: %s: %d\n", tz, strerror (old_errno),
                old_errno));
    goto release;
  }

  tzset ();

release:

  if (pthread_mutex_unlock (&tz_mutex) != 0) {
    LOG_DEBUG (("Failed to release tz_mutex"));
  }

  return;
}

#if defined(__linux__) && !defined(__GLIBC__)
static int
parse_tz_specifier (const char *str, const char *fmt, struct tm *tm) {
  char *fmt_notz = NULL, *p = NULL, *end = NULL, *ptr = NULL;
  int tz_offset_hours = 0, tz_offset_minutes = 0, neg = 0;

  /* new format string that excludes %z */
  fmt_notz = xstrdup (fmt);

  p = strstr (fmt_notz, "%z");
  if (p != NULL)
    *p = '\0';

  /* parse date/time without timezone offset */
  end = strptime (str, fmt_notz, tm);
  free (fmt_notz);
  if (end == NULL)
    return 1;

  /* bail early if no timezone offset is expected */
  if (*end == '\0') {
    tm->tm_gmtoff = 0;
    return 0;
  }

  /* try to parse timezone offset else bail early, +/-0500 */
  if ((*end != '+' && *end != '-') || strlen (end) < 4)
    return 1;

  /* divide by 100 to extract the hours part (e.g., 400 / 100 = 4) */
  tz_offset_hours = labs (strtol (end, &ptr, 10)) / 100;
  if (*ptr != '\0')
    return 1;

  if (strlen (end) >= 5) {
    /* minutes part of the offset is present */
    tz_offset_minutes = strtol (end + 3, &ptr, 10);
    if (*ptr != '\0')
      return 1;
  }

  neg = (*end == '-');
  tm->tm_gmtoff = (tz_offset_hours * 3600 + tz_offset_minutes * 60) * (neg ? -1 : 1);

  return 0;
}
#endif

/* Format the given date/time according the given format.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
int
str_to_time (const char *str, const char *fmt, struct tm *tm, int tz) {
  time_t t;
  char *end = NULL, *sEnd = NULL;
  unsigned long long ts = 0;
  int us, ms;
#if !defined(__GLIBC__)
  int se;
#endif

  time_t seconds = 0;

  if (str == NULL || *str == '\0' || fmt == NULL || *fmt == '\0')
    return 1;

  us = strcmp ("%f", fmt) == 0;
  ms = strcmp ("%*", fmt) == 0;
#if !defined(__GLIBC__)
  se = strcmp ("%s", fmt) == 0;
#endif

  /* check if char string needs to be converted from milli/micro seconds */
  /* note that MUSL doesn't have %s under strptime(3) */
#if !defined(__GLIBC__)
  if (se || us || ms) {
#else
  if (us || ms) {
#endif
    errno = 0;

    ts = strtoull (str, &sEnd, 10);
    if (str == sEnd || *sEnd != '\0' || errno == ERANGE)
      return 1;

    seconds = (us) ? ts / SECS : ((ms) ? ts / MILS : ts);

    if (conf.tz_name && tz)
      set_tz ();

    /* if GMT needed, gmtime_r instead of localtime_r. */
    localtime_r (&seconds, tm);

    return 0;
  }
#if defined(__linux__) && !defined(__GLIBC__)
  if (parse_tz_specifier (str, fmt, tm))
    return -1;
#else
  end = strptime (str, fmt, tm);
  if (end == NULL || *end != '\0')
    return 1;
#endif

  if (!tz || !conf.tz_name)
    return 0;

  if ((t = tm2time (tm)) == -1) {
    LOG_DEBUG (("Can't set time via tm2time() %s: %s\n", str, strerror (errno)));
    return 0;
  }

  set_tz ();
  localtime_r (&t, tm);

  return 0;
}

/* Convert a date from one format to another and store in the given buffer.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
int
convert_date (char *res, const char *data, const char *from, const char *to, int size) {
  struct tm tm;

  memset (&tm, 0, sizeof (tm));
  timestamp = time (NULL);
  localtime_r (&timestamp, &now_tm);

  /* This assumes that the given date is already in the correct timezone. */
  if (str_to_time (data, from, &tm, 0) != 0)
    return 1;

  /* if not a timestamp, use current year if not passed */
  if (!has_timestamp (from) && strpbrk (from, "Yy") == NULL)
    tm.tm_year = now_tm.tm_year;

  if (strftime (res, size, to, &tm) <= 0)
    return 1;

  return 0;
}

#pragma GCC diagnostic warning "-Wformat-nonliteral"

/* Determine if the given IP is a valid IPv4/IPv6 address.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
int
invalid_ipaddr (const char *str, int *ipvx) {
  union {
    struct sockaddr addr;
    struct sockaddr_in6 addr6;
    struct sockaddr_in addr4;
  } a;

  (*ipvx) = TYPE_IPINV;
  if (str == NULL || *str == '\0')
    return 1;

  memset (&a, 0, sizeof (a));
  if (1 == inet_pton (AF_INET, str, &a.addr4.sin_addr)) {
    (*ipvx) = TYPE_IPV4;
    return 0;
  } else if (1 == inet_pton (AF_INET6, str, &a.addr6.sin6_addr)) {
    (*ipvx) = TYPE_IPV6;
    return 0;
  }

  return 1;
}

/* Encode a data key and a unique visitor's key to a new uint64_t key
  *
  * ###NOTE: THIS LIMITS THE MAX VALUE OF A DATA TABLE TO uint32_t
  * WILL NEED TO CHANGE THIS IF WE GO OVER uint32_t
  */
uint64_t
u64encode (uint32_t x, uint32_t y) {
  return x > y ? (uint32_t) y | ((uint64_t) x << 32) : (uint32_t) x | ((uint64_t) y << 32);
}

/* Decode a uint64_t number into the original two uint32_t  */
void
u64decode (uint64_t n, uint32_t *x, uint32_t *y) {
  *x = (uint64_t) n >> 32;
  *y = (uint64_t) n & 0xFFFFFFFF;
}

/* Get information about the filename.
 *
 * On error, -1 is returned.
 * On success, the file size of the given filename. */
off_t
file_size (const char *filename) {
  struct stat st;

  if (stat (filename, &st) == 0)
    return st.st_size;

  LOG_DEBUG (("Can't determine size of %s: %s\n", filename, strerror (errno)));

  return -1;
}

/* Determine if the given status code is within the list of status
 * codes and find out the status type/category.
 *
 * If not found, "Unknown" is returned.
 * On success, the status code type/category is returned. */
const char *
verify_status_code_type (int code) {
  if (code >= 0 && code <= 599 && code_type[code / 100] == NULL)
    return "Unknown";

  return code_type[code / 100];
}

/* Determine if the given status code is within the list of status
 * codes.
 *
 * If not found, "Unknown" is returned.
 * On success, the status code is returned. */
const char *
verify_status_code (int code) {
  if (code >= 0 && code <= 599 && code_type[code / 100] == NULL && codes[code] == NULL)
    return "Unknown";

  return codes[code];
}

int
is_valid_http_status (int code) {
  return code >= 0 && code <= 599 && code_type[code / 100] != NULL && codes[code] != NULL;
}

/* Checks if the given string is within the given array.
 *
 * If not found, -1 is returned.
 * If found, the key for needle in the array is returned. */
int
str_inarray (const char *s, const char *arr[], int size) {
  int i;
  for (i = 0; i < size; i++) {
    if (strcmp (arr[i], s) == 0)
      return i;
  }
  return -1;
}

/* Strip whitespace from the beginning of a string.
 *
 * On success, a string with whitespace stripped from the beginning of
 * the string is returned. */
char *
ltrim (char *s) {
  char *begin = s;

  while (isspace (*begin))
    ++begin;
  memmove (s, begin, strlen (begin) + 1);

  return s;
}

/* Strip whitespace from the end of a string.
 *
 * On success, a string with whitespace stripped from the end of the
 * string is returned. */
char *
rtrim (char *s) {
  char *end = s + strlen (s);

  while ((end != s) && isspace (*(end - 1)))
    --end;
  *end = '\0';

  return s;
}

/* Strip whitespace from the beginning and end of the string.
 *
 * On success, the trimmed string is returned. */
char *
trim_str (char *str) {
  return rtrim (ltrim (str));
}

/* Convert the file size in bytes to a human-readable format.
 *
 * On error, the original size of the string in bytes is returned.
 * On success, the file size in a human-readable format is returned. */
char *
filesize_str (unsigned long long log_size) {
  char *size = xmalloc (sizeof (char) * 12);
  if (log_size >= (1ULL << 50))
    snprintf (size, 12, "%.2f PiB", (double) (log_size) / PIB (1ULL));
  else if (log_size >= (1ULL << 40))
    snprintf (size, 12, "%.2f TiB", (double) (log_size) / TIB (1ULL));
  else if (log_size >= (1ULL << 30))
    snprintf (size, 12, "%.2f GiB", (double) (log_size) / GIB (1ULL));
  else if (log_size >= (1ULL << 20))
    snprintf (size, 12, "%.2f MiB", (double) (log_size) / MIB (1ULL));
  else if (log_size >= (1ULL << 10))
    snprintf (size, 12, "%.2f KiB", (double) (log_size) / KIB (1ULL));
  else
    snprintf (size, 12, "%.1f   B", (double) (log_size));

  return size;
}

/* Convert microseconds to a human-readable format.
 *
 * On error, a malloc'd string in microseconds is returned.
 * On success, the time in a human-readable format is returned. */
char *
usecs_to_str (unsigned long long usec) {
  char *size = xmalloc (sizeof (char) * 11);
  if (usec >= DAY)
    snprintf (size, 11, "%.2f  d", (double) (usec) / DAY);
  else if (usec >= HOUR)
    snprintf (size, 11, "%.2f hr", (double) (usec) / HOUR);
  else if (usec >= MINS)
    snprintf (size, 11, "%.2f mn", (double) (usec) / MINS);
  else if (usec >= SECS)
    snprintf (size, 11, "%.2f  s", (double) (usec) / SECS);
  else if (usec >= MILS)
    snprintf (size, 11, "%.2f ms", (double) (usec) / MILS);
  else
    snprintf (size, 11, "%.2f us", (double) (usec));

  return size;
}

/* Convert the given int to a string with the ability to add some
 * padding.
 *
 * On success, the given number as a string is returned. */
char *
int2str (int d, int width) {
  char *s = xmalloc (snprintf (NULL, 0, "%*d", width, d) + 1);
  sprintf (s, "%*d", width, d);

  return s;
}

/* Convert the given uint32_t to a string with the ability to add some
 * padding.
 *
 * On success, the given number as a string is returned. */
char *
u322str (uint32_t d, int width) {
  char *s = xmalloc (snprintf (NULL, 0, "%*u", width, d) + 1);
  sprintf (s, "%*u", width, d);

  return s;
}

/* Convert the given uint64_t to a string with the ability to add some
 * padding.
 *
 * On success, the given number as a string is returned. */
char *
u642str (uint64_t d, int width) {
  char *s = xmalloc (snprintf (NULL, 0, "%*" PRIu64, width, d) + 1);
  sprintf (s, "%*" PRIu64, width, d);

  return s;
}

/* Convert the given float to a string with the ability to add some
 * padding.
 *
 * On success, the given number as a string is returned. */
char *
float2str (float d, int width) {
  char *s = xmalloc (snprintf (NULL, 0, "%*.2f", width, d) + 1);
  sprintf (s, "%*.2f", width, d);

  return s;
}

int
ptr2int (char *ptr) {
  char *sEnd = NULL;
  int value = -1;

  value = strtol (ptr, &sEnd, 10);
  if (ptr == sEnd || *sEnd != '\0' || errno == ERANGE) {
    LOG_DEBUG (("Invalid parse of integer value from pointer. \n"));
    return -1;
  }

  return value;
}

int
str2int (const char *date) {
  char *sEnd = NULL;
  int d = strtol (date, &sEnd, 10);
  if (date == sEnd || *sEnd != '\0' || errno == ERANGE)
    return -1;
  return d;
}

/* Determine the length of an integer (number of digits).
 *
 * On success, the length of the number is returned. */
int
intlen (uint64_t num) {
  int l = 1;
  while (num > 9) {
    l++;
    num /= 10;
  }

  return l;
}

/* Allocate a new string and fill it with the given character.
 *
 * On success, the newly allocated string is returned. */
char *
char_repeat (int n, char c) {
  char *dest = xmalloc (n + 1);
  memset (dest, c, n);
  dest[n] = '\0';

  return dest;
}

/* Replace all occurrences of the given char with the replacement
 * char.
 *
 * On error the original string is returned.
 * On success, a string with the replaced values is returned. */
char *
char_replace (char *str, char o, char n) {
  char *p = str;

  if (str == NULL || *str == '\0')
    return str;

  while ((p = strchr (p, o)) != NULL)
    *p++ = n;

  return str;
}

/* Remove all occurrences of a new line.
 *
 * On success, a string with the replaced new lines is returned. */
void
strip_newlines (char *str) {
  char *src, *dst;
  for (src = dst = str; *src != '\0'; src++) {
    *dst = *src;
    if (*dst != '\r' && *dst != '\n')
      dst++;
  }
  *dst = '\0';
}

/* Strip blanks from a string.
 *
 * On success, a string without whitespace is returned. */
char *
deblank (char *str) {
  char *out = str, *put = str;

  for (; *str != '\0'; ++str) {
    if (*str != ' ')
      *put++ = *str;
  }
  *put = '\0';

  return out;
}

/* Make a string uppercase.
 *
 * On error the original string is returned.
 * On success, the uppercased string is returned. */
char *
strtoupper (char *str) {
  char *p = str;
  if (str == NULL || *str == '\0')
    return str;

  while (*p != '\0') {
    *p = toupper (*p);
    p++;
  }

  return str;
}

/* Left-pad a string with n amount of spaces.
 *
 * On success, a left-padded string is returned. */
char *
left_pad_str (const char *s, int indent) {
  char *buf = NULL;

  indent = strlen (s) + indent;
  buf = xmalloc (snprintf (NULL, 0, "%*s", indent, s) + 1);
  sprintf (buf, "%*s", indent, s);

  return buf;
}

/* Append the source string to destination and reallocates and
 * updating the destination buffer appropriately. */
size_t
append_str (char **dest, const char *src) {
  size_t curlen = strlen (*dest);
  size_t srclen = strlen (src);
  size_t newlen = curlen + srclen;

  char *str = xrealloc (*dest, newlen + 1);
  memcpy (str + curlen, src, srclen + 1);
  *dest = str;

  return newlen;
}

/* Escapes the special characters, e.g., '\n', '\r', '\t', '\'
 * in the string source by inserting a '\' before them.
 *
 * On error NULL is returned.
 * On success the escaped string is returned */
char *
escape_str (const char *src) {
  char *dest, *q;
  const unsigned char *p;

  if (src == NULL || *src == '\0')
    return NULL;

  p = (const unsigned char *) src;
  q = dest = xmalloc (strlen (src) * 4 + 1);

  while (*p) {
    switch (*p) {
    case '\\':
      *q++ = '\\';
      *q++ = '\\';
      break;
    case '\n':
      *q++ = '\\';
      *q++ = 'n';
      break;
    case '\r':
      *q++ = '\\';
      *q++ = 'r';
      break;
    case '\t':
      *q++ = '\\';
      *q++ = 't';
      break;
    default:
      /* not ASCII */
      if ((*p < ' ') || (*p >= 0177)) {
        *q++ = '\\';
        *q++ = '0' + (((*p) >> 6) & 07);
        *q++ = '0' + (((*p) >> 3) & 07);
        *q++ = '0' + ((*p) & 07);
      } else
        *q++ = *p;
      break;
    }
    p++;
  }
  *q = 0;
  return dest;
}

/* Get an unescaped malloc'd string
 *
 * On error NULL is returned.
 * On success the unescaped string is returned */
char *
unescape_str (const char *src) {
  char *dest, *q;
  const char *p = src;

  if (src == NULL || *src == '\0')
    return NULL;

  dest = xmalloc (strlen (src) + 1);
  q = dest;

  while (*p) {
    if (*p == '\\') {
      p++;
      switch (*p) {
      case '\0':
        /* warning... */
        goto out;
      case 'n':
        *q++ = '\n';
        break;
      case 'r':
        *q++ = '\r';
        break;
      case 't':
        *q++ = '\t';
        break;
      default:
        *q++ = *p;
        break;
      }
    } else
      *q++ = *p;
    p++;
  }
out:
  *q = 0;

  return dest;
}
