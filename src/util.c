/**
 * util.c -- a set of handy functions to help parsing
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

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#define _XOPEN_SOURCE     700

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "util.h"

#include "error.h"
#include "xmalloc.h"

/* HTTP status codes categories */
static const char *code_type[][2] = {
  {"1", "1xx Informational"},
  {"2", "2xx Success"},
  {"3", "3xx Redirection"},
  {"4", "4xx Client Error"},
  {"5", "5xx Server Error"},
};

/* HTTP status codes */
static const char *codes[][2] = {
  {"100", "100 - Continue: Server received the initial part of the request"},
  {"101", "101 - Switching Protocols: Client asked to switch protocols"},
  {"200", "200 - OK: The request sent by the client was successful"},
  {"201", "201 - Created: The request has been fulfilled and created"},
  {"202", "202 - Accepted: The request has been accepted for processing"},
  {"203", "203 - Non-authoritative Information: Response from a third party"},
  {"204", "204 - No Content: Request did not return any content"},
  {"205", "205 - Reset Content: Server asked the client to reset the document"},
  {"206", "206 - Partial Content: The partial GET has been successful"},
  {"207", "207 - Multi-Status: WebDAV; RFC 4918"},
  {"208", "208 - Already Reported: WebDAV; RFC 5842"},
  {"300", "300 - Multiple Choices: Multiple options for the resource"},
  {"301", "301 - Moved Permanently: Resource has permanently moved"},
  {"302", "302 - Moved Temporarily (redirect)"},
  {"303", "303 - See Other Document: The response is at a different URI"},
  {"304", "304 - Not Modified: Resource has not been modified"},
  {"305", "305 - Use Proxy: Can only be accessed through the proxy"},
  {"307", "307 - Temporary Redirect: Resource temporarily moved"},
  {"400", "400 - Bad Request: The syntax of the request is invalid"},
  {"401", "401 - Unauthorized: Request needs user authentication"},
  {"402", "402 - Payment Required"},
  {"403", "403 - Forbidden: Server is refusing to respond to it"},
  {"404", "404 - Not Found: Requested resource could not be found"},
  {"405", "405 - Method Not Allowed: Request method not supported"},
  {"406", "406 - Not Acceptable"},
  {"407", "407 - Proxy Authentication Required"},
  {"408", "408 - Request Timeout: Server timed out waiting for the request"},
  {"409", "409 - Conflict: Conflict in the request"},
  {"410", "410 - Gone: Resource requested is no longer available"},
  {"411", "411 - Length Required: Invalid Content-Length"},
  {"412", "412 - Precondition Failed: Server does not meet preconditions"},
  {"413", "413 - Payload Too Large"},
  {"414", "414 - Request-URI Too Long"},
  {"415", "415 - Unsupported Media Type: Media type is not supported"},
  {"416", "416 - Requested Range Not Satisfiable: Cannot supply that portion"},
  {"417", "417 - Expectation Failed"},
  {"444", "444 - (Nginx) Connection closed without sending any headers"},
  {"494", "494 - (Nginx) Request Header Too Large"},
  {"495", "495 - (Nginx) SSL client certificate error"},
  {"496", "496 - (Nginx) Client didn't provide certificate"},
  {"497", "497 - (Nginx) HTTP request sent to HTTPS port"},
  {"499", "499 - (Nginx) Connection closed by client while processing request"},
  {"500", "500 - Internal Server Error"},
  {"501", "501 - Not Implemented"},
  {"502", "502 - Bad Gateway: Received an invalid response from the upstream"},
  {"503", "503 - Service Unavailable: The server is currently unavailable"},
  {"504", "504 - Gateway Timeout: The upstream server failed to send request"},
  {"505", "505 - HTTP Version Not Supported"},
  {"520", "520 - CloudFlare - Web server is returning an unknown error"},
  {"521", "521 - CloudFlare - Web server is down"},
  {"522", "522 - CloudFlare - Connection timed out"},
  {"523", "523 - CloudFlare - Origin is unreachable"},
  {"524", "524 - CloudFlare - A timeout occured"}
};

/* Return part of a string
 *
 * On error NULL is returned.
 * On success the extracted part of string is returned */
char *
substring (const char *str, int begin, int len)
{
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
alloc_string (const char *str)
{
  char *new = xmalloc (strlen (str) + 1);
  strcpy (new, str);
  return new;
}

/* A wrapper function to copy the first num characters of source to
 * destination. */
void
xstrncpy (char *dest, const char *source, const size_t dest_size)
{
  strncpy (dest, source, dest_size);
  if (dest_size > 0) {
    dest[dest_size - 1] = '\0';
  } else {
    dest[0] = '\0';
  }
}

/* Count the number of matches on the string `s1` given a character `c`
 *
 * If the character is not found, 0 is returned
 * On success, the number of characters found */
int
count_matches (const char *s1, char c)
{
  const char *ptr = s1;
  int n = 0;
  do {
    if (*ptr == c)
      n++;
  } while (*(ptr++));
  return n;
}

/* String matching where one string contains wildcard characters.
 *
 * If no match found, 1 is returned.
 * If match found, 0 is returned. */
static int
wc_match (const char *wc, char *str)
{
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

/* Determine if the given host needs to be ignored given the list of
 * referrers to ignore.
 *
 * On error, or the referrer is not found, 0 is returned
 * On success, or if the host needs to be ignored, 1 is returned */
int
ignore_referer (const char *host)
{
  char *needle = NULL;
  int i, ignore = 0;

  if (conf.ignore_referer_idx == 0)
    return 0;
  if (host == NULL || *host == '\0')
    return 0;

  needle = xstrdup (host);
  for (i = 0; i < conf.ignore_referer_idx; ++i) {
    if (conf.ignore_referers[i] == NULL || *conf.ignore_referers[i] == '\0')
      continue;

    if (wc_match (conf.ignore_referers[i], needle)) {
      ignore = 1;
      goto out;
    }
  }
out:
  free (needle);

  return ignore;
}

/* Determine if the given ip is within a range of IPs.
 *
 * On error, or not within the range, 0 is returned
 * On success, or if within the range, 1 is returned */
static int
within_range (const char *ip, const char *start, const char *end)
{
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
ip_in_range (const char *ip)
{
  char *start = NULL, *end, *dash;
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
    if (end == NULL && start) {
      if (strcmp (ip, start) == 0) {
        free (start);
        return 1;
      }
    }
    /* within range */
    else if (start && end) {
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
find_output_type (char **filename, const char *ext, int alloc)
{
  int i;
  const char *dot = NULL;

  for (i = 0; i < conf.output_format_idx; ++i) {
    /* for backwards compatibility. i.e., -o json  */
    if (strcmp (conf.output_formats[i], ext) == 0)
      return 0;

    if ((dot = strrchr (conf.output_formats[i], '.')) != NULL &&
        strcmp (dot + 1, ext) == 0) {
      if (alloc)
        *filename = xstrdup (conf.output_formats[i]);
      return 0;
    }
  }

  return 1;
}

/* Search the environment HOME variable and append GoAccess' config
 * file.
 *
 * On error, it outputs an error message and the program terminates.
 * On success, the path of HOME and the config file is returned. */
char *
get_home (void)
{
  char *user_home = NULL, *path = NULL;

  user_home = getenv ("HOME");
  if (user_home == NULL)
    FATAL ("Unable to determine the HOME environment variable.");

  path = xmalloc (snprintf (NULL, 0, "%s/.goaccessrc", user_home) + 1);
  sprintf (path, "%s/.goaccessrc", user_home);

  return path;
}

/* Get the path to the global config file.
 *
 * On success, the path of the global config file is returned. */
char *
get_global_config (void)
{
  char *path = NULL;

  path = xmalloc (snprintf (NULL, 0, "%s/goaccess.conf", SYSCONFDIR) + 1);
  sprintf (path, "%s/goaccess.conf", SYSCONFDIR);

  return path;
}

/* A self-checking wrapper to convert_date().
 *
 * On error, a newly malloc'd '---' string is returned.
 * On success, a malloc'd 'Ymd' date is returned. */
char *
get_visitors_date (const char *odate, const char *from, const char *to)
{
  char date[DATE_TIME] = "";    /* Ymd */

  memset (date, 0, sizeof *date);
  /* verify we have a valid date conversion */
  if (convert_date (date, odate, from, to, DATE_TIME) == 0)
    return xstrdup (date);

  LOG_DEBUG (("invalid date: %s", odate));
  return xstrdup ("---");
}

/* Format the given date/time according the given format.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
int
str_to_time (const char *str, const char *fmt, struct tm *tm)
{
  char *end = NULL, *sEnd = NULL;
  unsigned long long usecs = 0;

  if (str == NULL || *str == '\0' || fmt == NULL || *fmt == '\0')
    return 1;

  /* check if char string needs to be converted from microseconds */
  if (strcmp ("%f", fmt) == 0) {
    errno = 0;
    tm->tm_year = 1970 - 1900;
    tm->tm_mday = 1;

    usecs = strtoull (str, &sEnd, 10);
    if (str == sEnd || *sEnd != '\0' || errno == ERANGE)
      return 1;

    tm->tm_sec = usecs / SECS;
    tm->tm_isdst = -1;
    if (mktime (tm) == -1)
      return 1;

    return 0;
  }

  end = strptime (str, fmt, tm);
  if (end == NULL || *end != '\0')
    return 1;

  return 0;
}

/* Convert a date from one format to another and store inot the given buffer.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
int
convert_date (char *res, const char *data, const char *from, const char *to,
              int size)
{
  struct tm tm;

  memset (&tm, 0, sizeof (tm));
  timestamp = time (NULL);
  now_tm = localtime (&timestamp);

  if (str_to_time (data, from, &tm) != 0)
    return 1;

  /* if not a timestamp, use current year if not passed */
  if (!has_timestamp (from) && strpbrk (from, "Yy") == NULL)
    tm.tm_year = now_tm->tm_year;

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
invalid_ipaddr (char *str, int *ipvx)
{
  union
  {
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

/* Get information about the filename.
 *
 * On error, -1 is returned.
 * On success, the file size of the given filename. */
off_t
file_size (const char *filename)
{
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
verify_status_code_type (const char *str)
{
  size_t i;
  for (i = 0; i < ARRAY_SIZE (code_type); i++)
    if (strchr (code_type[i][0], str[0]) != NULL)
      return code_type[i][1];

  return "Unknown";
}

/* Determine if the given status code is within the list of status
 * codes.
 *
 * If not found, "Unknown" is returned.
 * On success, the status code is returned. */
const char *
verify_status_code (char *str)
{
  size_t i;
  for (i = 0; i < ARRAY_SIZE (codes); i++)
    if (strstr (str, codes[i][0]) != NULL)
      return codes[i][1];

  return "Unknown";
}

/* Checks if the given string is within the given array.
 *
 * If not found, -1 is returned.
 * If found, the key for needle in the array is returned. */
int
str_inarray (const char *s, const char *arr[], int size)
{
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
ltrim (char *s)
{
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
rtrim (char *s)
{
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
trim_str (char *str)
{
  return rtrim (ltrim (str));
}

/* Convert the file size in bytes to a human readable format.
 *
 * On error, the original size of the string in bytes is returned.
 * On success, the file size in a human readable format is returned. */
char *
filesize_str (unsigned long long log_size)
{
  char *size = xmalloc (sizeof (char) * 12);
  if (log_size >= TIB)
    snprintf (size, 12, "%.2f TiB", (double) (log_size) / TIB);
  else if (log_size >= GIB)
    snprintf (size, 12, "%.2f GiB", (double) (log_size) / GIB);
  else if (log_size >= MIB)
    snprintf (size, 12, "%.2f MiB", (double) (log_size) / MIB);
  else if (log_size >= KIB)
    snprintf (size, 12, "%.2f KiB", (double) (log_size) / KIB);
  else
    snprintf (size, 12, "%.1f   B", (double) (log_size));

  return size;
}

/* Convert microseconds to a human readable format.
 *
 * On error, a malloc'd string in microseconds is returned.
 * On success, the time in a human readable format is returned. */
char *
usecs_to_str (unsigned long long usec)
{
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
int2str (int d, int width)
{
  char *s = xmalloc (snprintf (NULL, 0, "%*d", width, d) + 1);
  sprintf (s, "%*d", width, d);

  return s;
}

/* Convert the given float to a string with the ability to add some
 * padding.
 *
 * On success, the given number as a string is returned. */
char *
float2str (float d, int width)
{
  char *s = xmalloc (snprintf (NULL, 0, "%*.2f", width, d) + 1);
  sprintf (s, "%*.2f", width, d);

  return s;
}

/* Determine the length of an integer (number of digits).
 *
 * On success, the length of the number is returned. */
int
intlen (int num)
{
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
char_repeat (int n, char c)
{
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
char_replace (char *str, char o, char n)
{
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
strip_newlines (char *str)
{
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
deblank (char *str)
{
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
strtoupper (char *str)
{
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
left_pad_str (const char *s, int indent)
{
  char *buf = NULL;

  indent = strlen (s) + indent;
  buf = xmalloc (snprintf (NULL, 0, "%*s", indent, s) + 1);
  sprintf (buf, "%*s", indent, s);

  return buf;
}

/* Append the source string to destination and reallocates and
 * updating the destination buffer appropriately. */
void
append_str (char **dest, const char *src)
{
  size_t curlen = strlen (*dest);
  size_t srclen = strlen (src);
  size_t newlen = curlen + srclen;

  char *str = xrealloc (*dest, newlen + 1);
  memcpy (str + curlen, src, srclen + 1);
  *dest = str;
}

/* Escapes the special characters, e.g., '\n', '\r', '\t', '\'
 * in the string source by inserting a '\' before them.
 *
 * On error NULL is returned.
 * On success the escaped string is returned */
char *
escape_str (const char *src)
{
  char *dest, *q;
  const unsigned char *p;

  if (src == NULL || *src == '\0')
    return NULL;

  p = (unsigned char *) src;
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
unescape_str (const char *src)
{
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
