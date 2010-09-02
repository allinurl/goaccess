/** 
 * util.c -- a set of handy functions to help parsing 
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
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

#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#include "util.h"
#include "commons.h"
#include "error.h"

/* helper functions */
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
   if ((buffer = malloc (len + 1)) == NULL)
      return NULL;
   memcpy (buffer, &(str[begin]), len);
   buffer[len] = '\0';

   return buffer;
}

char *
alloc_string (const char *str)
{
   char *new = malloc (strlen (str) + 1);
   if (new == NULL)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Unable to allocate memory");
   strcpy (new, str);
   return new;
}

char *
reverse_ip (char *str)
{
   char *p;
   struct hostent *hent;
   int is_valid = 1, dots = 0;
   unsigned int addr = 0;

   if (str == NULL || *str == '\0')
      return (NULL);

   /* double check and make sure we got an ip in here */
   for (p = str; *p; p++) {
      if (*p == '.') {
         dots++;
         continue;
      } else if (!isdigit (*p)) {
         is_valid = 0;
         break;
      }
   }
   if (dots != 3)
      is_valid = 0;

   if (is_valid) {
      addr = inet_addr (str);
      hent = gethostbyaddr ((char *) &addr, sizeof (unsigned int), AF_INET);
   } else
      hent = gethostbyname (str);

   return (hent != NULL ? strdup (hent->h_name) : NULL);
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
verify_browser (char *str)
{
   char *a, *b, *p, *ptr;
   size_t i;

   if (str == NULL || *str == '\0')
      return (NULL);

   for (i = 0; i < browsers_size (); i++) {
      if ((a = strstr (str, browsers[i][0])) == NULL)
         continue;
      if (!(b = a))
         return "-";
      ptr = a;
      /* MSIE needs additional code. sigh... */
      if ((strstr (a, "MSIE")) != NULL || (strstr (a, "Ask")) != NULL) {
         while (*ptr != ';' && *ptr != ')' && *ptr != '-' && *ptr != '\0') {
            if (*ptr == ' ')
               *ptr = '/';
            ptr++;
         }
      }
      for (p = a; *p; p++) {
         if (isalnum (p[0]) || *p == '.' || *p == '/' || *p == '_'
             || *p == '-') {
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
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Unable to allocate memory");
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
clean_date_time (char *s)
{
   return substring (s + 1, 0, 20);
}

char *
clean_date (char *s)
{
   return substring (s + 1, 0, 8);
}

char *
clean_time (char *s)
{
   return substring (s, 13, 8);
}

char *
clean_status (char *s)
{
   return substring (s, 0, 3);
}
