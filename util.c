/**
 * util.c -- a set of handy functions to help parsing
 * Copyright (C) 2009-2012 by Gerardo Orellana <goaccess@prosoftcorp.com>
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

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#define H_SIZE 256

#define _XOPEN_SOURCE 700

#ifdef __FreeBSD__
#include <sys/socket.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "util.h"
#include "commons.h"
#include "error.h"

/* helper functions */
char *
alloc_string (const char *str)
{
   char *new = malloc (strlen (str) + 1);
   if (new == NULL)
      error_handler (__PRETTY_FUNCTION__,
                     __FILE__, __LINE__, "Unable to allocate memory");
   strcpy (new, str);
   return new;
}

int
count_occurrences (const char *s1, char c)
{
   const char *ptr = s1;
   int n = 0;
   do {
      if (*ptr == c)
         n++;
   } while (*(ptr++));
   return n;
}

char *
convert_date (char *result, char *data, int size)
{
   struct tm tm;

   memset (&tm, 0, sizeof (tm));

   char *end = strptime (data, "%Y%m%d", &tm);
   if (end == NULL || *end != '\0')
      return 0;
   if (strftime (result, size, "%d/%b/%Y", &tm) <= 0)
      return 0;

   return result;
}

int
invalid_ipaddr (char *str)
{
   if (str == NULL || *str == '\0')
      return 1;

   union
   {
      struct sockaddr addr;
      struct sockaddr_in6 addr6;
      struct sockaddr_in addr4;
   } a;
   memset (&a, 0, sizeof (a));
   if (1 == inet_pton (AF_INET, str, &a.addr4.sin_addr))
      return 0;
   else if (1 == inet_pton (AF_INET6, str, &a.addr6.sin6_addr))
      return 0;
   return 1;
}

char *
reverse_host (const struct sockaddr *a, socklen_t length)
{
   char h[H_SIZE];
   int flags, st;

   flags = NI_NAMEREQD;
   st = getnameinfo (a, length, h, H_SIZE, NULL, 0, flags);
   if (!st)
      return alloc_string (h);
   return alloc_string (gai_strerror (st));
}

char *
reverse_ip (char *str)
{
   if (str == NULL || *str == '\0')
      return NULL;

   union
   {
      struct sockaddr addr;
      struct sockaddr_in6 addr6;
      struct sockaddr_in addr4;
   } a;

   memset (&a, 0, sizeof (a));
   if (1 == inet_pton (AF_INET, str, &a.addr4.sin_addr)) {
      a.addr4.sin_family = AF_INET;
      return reverse_host (&a.addr, sizeof (a.addr4));
   } else if (1 == inet_pton (AF_INET6, str, &a.addr6.sin6_addr)) {
      a.addr6.sin6_family = AF_INET6;
      return reverse_host (&a.addr, sizeof (a.addr6));
   }
   return NULL;
}

/* off_t becomes 64 bit aware */
off_t
file_size (const char *filename)
{
   struct stat st;

   if (stat (filename, &st) == 0)
      return st.st_size;

   fprintf (stderr, "Cannot determine size of %s: %s\n", filename,
            strerror (errno));

   return -1;
}

char *
verify_os (char *str)
{
   char *a, *b, *s, *p, *lookfor;
   size_t i;

   if (str == NULL || *str == '\0')
      return (NULL);

   for (i = 0; i < os_size (); i++) {
      if ((a = strstr (str, os[i][0])) == NULL)
         continue;
      /* agents w/ space in between */
      if ((lookfor = "Windows", strstr (str, lookfor)) != NULL ||
          (lookfor = "Mac OS", strstr (str, lookfor)) != NULL ||
          (lookfor = "Red Hat", strstr (str, lookfor)) != NULL ||
          (lookfor = "Win", strstr (str, lookfor)) != NULL) {
         s = malloc (snprintf (NULL, 0, "%s|%s", os[i][1], os[i][0]) + 1);
         sprintf (s, "%s|%s", os[i][1], os[i][0]);
         return s;
      }
      if (!(b = a))
         return "-";
      for (p = a; *p; p++) {
         if (*p != ' ' && isalnum (p[0]) && *p != '\0') {
            a++;
            continue;
         } else
            break;
      }
      *p = 0;
      if (strlen (a) == (p - b));

      s = malloc (snprintf (NULL, 0, "%s|%s", os[i][1], b) + 1);
      sprintf (s, "%s|%s", os[i][1], b);

      return s;
   }

   return alloc_string ("Unknown");
}

char *
char_replace (char *str, char o, char n)
{
   if (str == NULL || *str == '\0')
      return str;

   char *p = str;
   while ((p = strchr (p, o)) != NULL)
      *p++ = n;
   return str;
}

char *
verify_browser (char *str)
{
   char *a, *b, *p, *ptr, *slash;
   size_t i;

   if (str == NULL || *str == '\0')
      return (NULL);

   for (i = 0; i < browsers_size (); i++) {
      if ((a = strstr (str, browsers[i][0])) == NULL)
         continue;
      if (!(b = a))
         return "-";
      ptr = a;

      if ((strstr (a, "Opera")) != NULL) {
         slash = strrchr (str, '/');
         if ((slash != NULL) && (a < slash))
            memmove (a + 5, slash, strlen (slash) + 1);
      }
      /* MSIE needs additional code. sigh... */
      if ((strstr (a, "MSIE")) != NULL || (strstr (a, "Ask")) != NULL) {
         while (*ptr != ';' && *ptr != ')' && *ptr != '-' && *ptr != '\0') {
            if (*ptr == ' ')
               *ptr = '/';
            ptr++;
         }
      }
      for (p = a; *p; p++) {
         if (isalnum (p[0]) || *p == '.' || *p == '/' || *p == '_' ||
             *p == '-') {
            a++;
            continue;
         } else
            break;
      }
      *p = 0;
      if (strlen (a) == (p - b));

      char *s = malloc (snprintf (NULL, 0, "%s|%s", browsers[i][1], b) + 1);
      sprintf (s, "%s|%s", browsers[i][1], b);

      return s;
   }
   return alloc_string ("Unknown");
}

char *
verify_status_code (char *str)
{
   size_t i;
   for (i = 0; i < codes_size (); i++)
      if (strstr (str, codes[i][0]) != NULL)
         return codes[i][1];
   return "Unknown";
}

char *
trim_str (char *str)
{
   char *p;
   if (!str)
      return NULL;
   if (!*str)
      return str;
   for (p = str + strlen (str) - 1; (p >= str) && isspace (*p); --p);
   p[1] = '\0';
   return str;
}

char *
filesize_str (off_t log_size)
{
   char *size = (char *) malloc (sizeof (char) * 10);
   if (size == NULL)
      error_handler (__PRETTY_FUNCTION__,
                     __FILE__, __LINE__, "Unable to allocate memory");
   if (log_size >= GB)
      snprintf (size, 10, "%.2f GB", (double) (log_size) / GB);
   else if (log_size >= MB)
      snprintf (size, 10, "%.2f MB", (double) (log_size) / MB);
   else if (log_size >= KB)
      snprintf (size, 10, "%.2f KB", (double) (log_size) / KB);
   else
      snprintf (size, 10, "%.1f  B", (double) (log_size));

   return size;
}

char *
clean_date (char *s)
{
   if ((s == NULL) || (*s == '\0'))
      return NULL;

   char *buffer = malloc (strlen (s) + 1);
   if (buffer == NULL)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Unable to allocate memory");
   if (sscanf (s, "%8[^|]", buffer) != 1) {
      free (buffer);
      return NULL;
   }
   return buffer;
}

char *
clean_month (char *s)
{
   if ((s == NULL) || (*s == '\0'))
      return NULL;

   char *buffer = malloc (strlen (s) + 1);
   if (buffer == NULL)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Unable to allocate memory");
   if (sscanf (s, "%6[^|]", buffer) != 1) {
      free (buffer);
      return NULL;
   }
   return buffer;
}
