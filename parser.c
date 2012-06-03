/**
 * parser.c -- web log parsing
 * Copyright (C) 2010-2012 by Gerardo Orellana <goaccess@prosoftcorp.com>
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

/* "_XOPEN_SOURCE" is required for the GNU libc to export "strptime(3)"
 * correctly. */
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#define _XOPEN_SOURCE 700

#include <ctype.h>
#include <errno.h>

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_LIBNCURSESW
#  include <ncursesw/curses.h>
#else
#  include <ncurses.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <arpa/inet.h>

#include "error.h"
#include "commons.h"
#include "parser.h"
#include "util.h"

/* qsort struct comparision function (hits field) */
int
struct_cmp_by_hits (const void *a, const void *b)
{
   struct struct_holder *ia = *(struct struct_holder **) a;
   struct struct_holder *ib = *(struct struct_holder **) b;
   return (int) (ib->hits - ia->hits);
   /* integer comparison: returns negative if b > a and positive if a > b */
}

int
struct_cmp_desc (const void *a, const void *b)
{
   struct struct_holder *ia = *(struct struct_holder **) a;
   struct struct_holder *ib = *(struct struct_holder **) b;
   return strcmp (ib->data, ia->data);
}

int
struct_cmp_asc (const void *a, const void *b)
{
   struct struct_holder *ia = *(struct struct_holder **) a;
   struct struct_holder *ib = *(struct struct_holder **) b;
   return strcmp (ia->data, ib->data);
}

static void
free_logger (struct logger *log)
{
   if (log->agent != NULL)
      free (log->agent);
   if (log->date != NULL)
      free (log->date);
   if (log->host != NULL)
      free (log->host);
   if (log->ref != NULL)
      free (log->ref);
   if (log->req != NULL)
      free (log->req);
   if (log->status != NULL)
      free (log->status);
}

static char *
verify_browser_type (char *str)
{
   char *lookfor = NULL;

   if ((lookfor = "Chrome", strstr (str, lookfor)) != NULL ||
       (lookfor = "Crawlers", strstr (str, lookfor)) != NULL ||
       (lookfor = "Firefox", strstr (str, lookfor)) != NULL ||
       (lookfor = "MSIE", strstr (str, lookfor)) != NULL ||
       (lookfor = "Opera", strstr (str, lookfor)) != NULL ||
       (lookfor = "Safari", strstr (str, lookfor)) != NULL ||
       (lookfor = "Others", strstr (str, lookfor)) != NULL) {
      return lookfor;
   } else
      return NULL;
}

static char *
verify_os_type (char *str)
{
   char *lookfor = NULL;

   if ((lookfor = "Windows", strstr (str, lookfor)) != NULL ||
       (lookfor = "Macintosh", strstr (str, lookfor)) != NULL ||
       (lookfor = "Linux", strstr (str, lookfor)) != NULL ||
       (lookfor = "BSD", strstr (str, lookfor)) != NULL ||
       (lookfor = "Others", strstr (str, lookfor)) != NULL) {
      return lookfor;
   } else
      return NULL;
}

static int
process_request_bw (char *host, char *date, char *req, long long size)
{
   GHashTable *ht = NULL;
   gpointer value_ptr;
   int i;                       /* hash_tables to process */

   if ((host == NULL) || (date == NULL) || (req == NULL))
      return (EINVAL);

   char *key = NULL;
   long long add_value;
   long long *ptr_value;

   for (i = 0; i < BW_HASHTABLES; i++) {
      switch (i) {
       case 0:                 /* HOSTS bandwith */
          ht = ht_host_bw;
          key = host;
          break;
       case 1:                 /* FILES bandwith */
          ht = ht_file_bw;
          key = req;
          break;
       case 2:                 /* DATE bandwith */
          ht = ht_date_bw;
          key = date;
          break;
      }
      value_ptr = g_hash_table_lookup (ht, key);
      if (value_ptr != NULL) {
         ptr_value = (long long *) value_ptr;
         add_value = *ptr_value + size;
      } else
         add_value = 0 + size;

      ptr_value = g_malloc (sizeof (long long));
      *ptr_value = add_value;

      g_hash_table_replace (ht, g_strdup (key), ptr_value);
   }

   return 0;
}

static int
process_generic_data (GHashTable * ht, const char *key)
{
   char *date = NULL;
   gint value_int;
   gpointer value_ptr;
   int first = 0;

   if ((ht == NULL) || (key == NULL))
      return (EINVAL);

   value_ptr = g_hash_table_lookup (ht, key);
   if (value_ptr != NULL)
      value_int = GPOINTER_TO_INT (value_ptr);
   else {
      value_int = 0;
      first = 1;
   }
   value_int++;

   /* replace the entry. old key will be freed by "free_key_value". */
   g_hash_table_replace (ht, g_strdup (key), GINT_TO_POINTER (value_int));
   if (first && ht == ht_unique_visitors) {
      char *new_key = strdup (key);
      new_key = char_replace (new_key, '+', ' ');

      char *dup_k_b = strdup (new_key), *dup_k_o = strdup (new_key);
      char *b_key = NULL, *o_key = NULL;

      /* get browser & OS from agent */
      b_key = verify_browser (dup_k_b);
      o_key = verify_os (dup_k_o);

      /* insert new keys & values */
      process_generic_data (ht_browsers, b_key);
      process_generic_data (ht_os, o_key);
      /* check type & add up totals */
      process_generic_data (ht_browsers, verify_browser_type (b_key));
      process_generic_data (ht_os, verify_os_type (o_key));

      /* clean stuff up */
      free (b_key);
      free (dup_k_b);
      free (dup_k_o);
      free (new_key);
      free (o_key);

      if ((date = strchr (key, '|')) != NULL) {
         char *tmp_date = NULL;
         tmp_date = clean_date (++date);
         if (tmp_date != NULL) {
            process_generic_data (ht_unique_vis, tmp_date);
            free (tmp_date);
         }
      }
   }
   return (0);
}

/** from oreillynet.com
 ** with minor modifications **/
#define SPC_BASE16_TO_10(x) (((x) >= '0' && (x) <= '9') ? \
                            ((x) - '0') : (toupper((x)) - 'A' + 10))
static char *
spc_decode_url (char *url)
{
   char *out, *ptr;
   const char *c;

   if (!(out = ptr = strdup (url)))
      return 0;
   for (c = url; *c; c++) {
      if (*c != '%' || !isxdigit (c[1]) || !isxdigit (c[2]))
         *ptr++ = *c;
      else {
         *ptr++ = (SPC_BASE16_TO_10 (c[1]) * 16) + (SPC_BASE16_TO_10 (c[2]));
         c += 2;
      }
   }
   *ptr = 0;
   if (strlen (url) == (ptr - out));
   return trim_str (out);
}

static int
process_keyphrases (char *ref)
{
   if (!(strstr (ref, "http://www.google.")) &&
       !(strstr (ref, "http://webcache.googleusercontent.com/")) &&
       !(strstr (ref, "http://translate.googleusercontent.com/")))
      return -1;

   char *r, *ptr, *p, *dec, *pch;
   if ((r = strstr (ref, "/+&")) != NULL)
      r = "-";
   else if ((r = strstr (ref, "/+")) != NULL)
      r += 2;
   else if ((r = strstr (ref, "q=cache:")) != NULL) {
      pch = strchr (r, '+');
      if (pch)
         r += pch - r + 1;
   } else if ((r = strstr (ref, "&q=")) != NULL
              || (r = strstr (ref, "?q=")) != NULL)
      r += 3;
   else if ((r = strstr (ref, "%26q%3D")) != NULL
            || (r = strstr (ref, "%3Fq%3D")) != NULL)
      r += 7;
   else
      return -1;
   dec = spc_decode_url (r);
   if ((ptr = strstr (dec, "%26")) != NULL ||
       (ptr = strchr (dec, '&')) != NULL)
      *ptr = '\0';
   p = dec;
   if (p[0] == '\0')
      return -1;
   while (*p != '\0') {
      if (*p == '+')
         *p = ' ';
      p++;
   }
   process_generic_data (ht_keyphrases, trim_str (dec));
   free (dec);

   return 0;
}

static int
process_host_agents (char *host, char *agent)
{
   GHashTable *ht = ht_hosts_agents;
   gpointer value_ptr;

   if ((ht == NULL) || (host == NULL) || (agent == NULL))
      return (EINVAL);

   char *ptr_value, *tmp;

   value_ptr = g_hash_table_lookup (ht, host);
   if (value_ptr != NULL) {
      ptr_value = (char *) value_ptr;
      if (strstr (ptr_value, agent))
         return 0;

      size_t len1 = strlen (ptr_value);
      size_t len2 = strlen (agent);

      tmp = malloc (len1 + len2 + 2);
      if (tmp == NULL)
         error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                        "Unable to allocate memory.");
      memcpy (tmp, ptr_value, len1);
      tmp[len1] = '|';
      /* NUL-terminated */
      memcpy (tmp + len1 + 1, agent, len2 + 1);
   } else
      tmp = alloc_string (agent);

   g_hash_table_replace (ht, g_strdup (host), tmp);
   return 0;
}

static void
process_unique_data (char *host, char *date, char *agent, char *status,
                     char *referrer)
{
   char unique_visitors_key[2048];
   /*
    * C struct initialization and strptime
    * C may not initialize stack structs and arrays to zeros
    * so strptime uses struct for output and input as well.
    */
   if (agent == NULL)
      agent = "-";
   snprintf (unique_visitors_key, sizeof (unique_visitors_key), "%s|%s|%s",
             host, date, agent);
   unique_visitors_key[sizeof (unique_visitors_key) - 1] = 0;

   char url[512] = "";
   if (referrer != NULL && sscanf (referrer, "http://%511[^/\n]", url) == 1) {
      process_generic_data (ht_referring_sites, url);
   }
   if (referrer != NULL)
      process_keyphrases (referrer);

   process_generic_data (ht_status_code, status);

   if (ignore_host != NULL && strcmp (host, ignore_host) == 0) {
      /* ignore */
   } else {
      process_generic_data (ht_hosts, host);
   }
   if (host_agents_list_flag)
      process_host_agents (host, agent);
   process_generic_data (ht_unique_visitors, unique_visitors_key);
}

static int
verify_static_content (char *req)
{
   char *nul = req + strlen (req);

   if (strlen (req) < 5)
      return 0;
   if (!memcmp (nul - 4, ".jpg", 4) || !memcmp (nul - 4, ".JPG", 4) ||
       !memcmp (nul - 4, ".png", 4) || !memcmp (nul - 4, ".PNG", 4) ||
       !memcmp (nul - 3, ".js", 3) || !memcmp (nul - 3, ".JS", 3) ||
       !memcmp (nul - 4, ".gif", 4) || !memcmp (nul - 4, ".GIF", 4) ||
       !memcmp (nul - 4, ".css", 4) || !memcmp (nul - 4, ".CSS", 4) ||
       !memcmp (nul - 4, ".ico", 4) || !memcmp (nul - 4, ".ICO", 4) ||
       !memcmp (nul - 4, ".swf", 4) || !memcmp (nul - 4, ".SWF", 4) ||
       !memcmp (nul - 5, ".jpeg", 5) || !memcmp (nul - 4, ".JPEG", 5))
      return 1;
   return 0;
}

static char *
parse_req (char *line)
{
   char *reqs, *req_l = NULL, *req_r = NULL, *lookfor = NULL;

   if ((lookfor = "GET ", req_l = strstr (line, lookfor)) != NULL ||
       (lookfor = "POST ", req_l = strstr (line, lookfor)) != NULL ||
       (lookfor = "HEAD ", req_l = strstr (line, lookfor)) != NULL ||
       (lookfor = "get ", req_l = strstr (line, lookfor)) != NULL ||
       (lookfor = "post ", req_l = strstr (line, lookfor)) != NULL ||
       (lookfor = "head ", req_l = strstr (line, lookfor)) != NULL) {

      /* didn't find it - weird */
      if ((req_r = strstr (line, " HTTP/1.0")) == NULL &&
          (req_r = strstr (line, " HTTP/1.1")) == NULL)
         return alloc_string ("-");

      req_l += strlen (lookfor);
      ptrdiff_t req_len = req_r - req_l;

      /* make sure we don't have some weird requests */
      if (req_len <= 0)
         return alloc_string ("-");

      reqs = malloc (req_len + 1);
      strncpy (reqs, req_l, req_len);
      (reqs)[req_len] = 0;
   } else
      reqs = alloc_string (line);

   return reqs;
}

static char *
parse_string (char **str, char end)
{
   char *pch = *str, *p;
   do {
      if (*pch == end || *pch == '\0') {
         size_t len = (pch - *str + 1);
         p = malloc (len);
         if (p == NULL)
            error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                           "Unable to allocate memory");
         memcpy (p, *str, (len - 1));
         p[len - 1] = '\0';
         *str += len - 1;
         return trim_str (p);
      }
      /* advance to the first unescaped delim */
      if (*pch == '\\')
         pch++;
   } while (*pch++);
   return NULL;
}

static int
parse_format (struct logger *logger, const char *fmt, char *str)
{
   const char *p;

   if (str == NULL || *str == '\0')
      return 1;

   struct tm tm;
   memset (&tm, 0, sizeof (tm));

   int special = 0;
   for (p = fmt; *p; p++) {
      if (*p == '%') {
         special++;
         continue;
      }
      if (special && *p != '\0') {
         char *pch, *sEnd, *bEnd, *tkn = NULL, *end = NULL;
         errno = 0;
         long long bandw = 0;
         long status = 0;

         switch (*p) {
          case 'd':
             if (logger->date)
                return 1;
             tkn = parse_string (&str, p[1]);
             if (tkn == NULL)
                return 1;
             end = strptime (tkn, date_format, &tm);
             if (end == NULL || *end != '\0') {
                free (tkn);
                return 1;
             }
             logger->date = tkn;
             break;
          case 'h':
             if (logger->host)
                return 1;
             tkn = parse_string (&str, p[1]);
             if (tkn == NULL)
                return 1;
             if (invalid_ipaddr (tkn)) {
                free (tkn);
                return 1;
             }
             logger->host = tkn;
             break;
          case 'r':
             if (logger->req)
                return 1;
             tkn = parse_string (&str, p[1]);
             if (tkn == NULL)
                return 1;
             logger->req = parse_req (tkn);
             free (tkn);
             break;
          case 's':
             if (logger->status)
                return 1;
             tkn = parse_string (&str, p[1]);
             if (tkn == NULL)
                return 1;
             status = strtol (tkn, &sEnd, 10);
             if (tkn == sEnd || *sEnd != '\0' || errno == ERANGE) {
                free (tkn);
                return 1;
             }
             logger->status = tkn;
             break;
          case 'b':
             if (logger->resp_size)
                return 1;
             tkn = parse_string (&str, p[1]);
             if (tkn == NULL)
                return 1;
             bandw = strtol (tkn, &bEnd, 10);
             if (tkn == bEnd || *bEnd != '\0' || errno == ERANGE)
                bandw = 0;
             logger->resp_size = bandw;
             req_size += bandw;
             free (tkn);
             break;
          case 'R':
             if (logger->ref)
                return 1;
             tkn = parse_string (&str, p[1]);
             if (tkn == NULL)
                tkn = alloc_string ("-");
             if (tkn != NULL && *tkn == '\0') {
                free (tkn);
                tkn = alloc_string ("-");
             }
             logger->ref = tkn;
             break;
          case 'u':
             if (logger->agent)
                return 1;
             tkn = parse_string (&str, p[1]);
             if (tkn == NULL)
                tkn = alloc_string ("-");
             if (tkn != NULL && *tkn == '\0') {
                free (tkn);
                tkn = alloc_string ("-");
             }
             logger->agent = tkn;
             break;
          default:
             if ((pch = strchr (str, p[1])) != NULL)
                str += pch - str;
         }
         if ((str == NULL) || (*str == '\0'))
            return 0;
         special = 0;
      } else if (special && isspace (p[0])) {
         return 1;
      } else
         str++;
   }
   return 0;
}

static int
process_log (struct logger *logger, char *line, int test)
{
   struct logger log;

   struct tm tm;
   char buf[32];

   /* make compiler happy */
   memset (buf, 0, sizeof (buf));
   memset (&log, 0, sizeof (log));
   memset (&tm, 0, sizeof (tm));

   if (date_format == NULL || *date_format == '\0')
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "No date format was found on your conf file.");

   if (log_format == NULL || *log_format == '\0')
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "No log format was found on your conf file.");

   if ((line == NULL) || (*line == '\0')) {
      logger->total_invalid++;
      return 0;
   }
   if (*line == '#' || *line == '\n')
      return 0;

   logger->total_process++;
   if (parse_format (&log, log_format, line) == 1) {
      free_logger (&log);
      logger->total_invalid++;
      return 0;
   }
   if (log.host == NULL || log.date == NULL || log.status == NULL ||
       log.req == NULL) {
      free_logger (&log);
      logger->total_invalid++;
      return 0;
   }
   if (test) {
      free_logger (&log);
      return 0;
   }

   char *end = strptime (log.date, date_format, &tm);
   if (end == NULL || *end != '\0')
      return 0;
   if (strftime (buf, sizeof (buf) - 1, "%Y%m%d", &tm) == 0)
      return 0;

   process_unique_data (log.host, buf, log.agent, log.status, log.ref);

   if (!memcmp (log.status, "404", 3))
      process_generic_data (ht_not_found_requests, log.req);
   if (verify_static_content (log.req))
      process_generic_data (ht_requests_static, log.req);
   else
      process_generic_data (ht_requests, log.req);

   process_generic_data (ht_referrers, log.ref);
   process_request_bw (log.host, buf, log.req, log.resp_size);

   free_logger (&log);
   return 0;
}

void
reset_struct (struct logger *logger)
{
   logger->alloc_counter = 0;
   logger->counter = 0;
   logger->current_module = 1;
   logger->resp_size = 0LL;
   logger->total_invalid = 0;
   logger->total_process = 0;
   req_size = 0;
}

int
parse_log (struct logger *logger, char *filename, char *tail, int n)
{
   FILE *fp = NULL;
   char line[BUFFER];
   int i = 0, test = -1 == n ? 0 : 1;

   if (tail != NULL) {
      if (process_log (logger, tail, test))
         return 1;
      return 0;
   }
   if (filename == NULL) {
      fp = stdin;
      piping = 1;
   }
   if (!piping && (fp = fopen (filename, "r")) == NULL)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "An error has occurred while opening the log file. Make sure it exists.");

   while (fgets (line, BUFFER, fp) != NULL) {
      if (n >= 0 && i++ == n)
         break;
      if (process_log (logger, line, test)) {
         if (!piping)
            fclose (fp);
         return 1;
      }
   }
   /* definitely not portable! */
   if (piping)
      stdin = freopen ("/dev/tty", "r", stdin);

   if (!piping)
      fclose (fp);
   return 0;
}

static void
unique_visitors_iter (gpointer k, gpointer v, struct struct_holder **t_holder)
{
   t_holder[iter_ctr] = malloc (sizeof (struct struct_holder));
   t_holder[iter_ctr]->data = (gchar *) k;
   t_holder[iter_ctr++]->hits = GPOINTER_TO_INT (v);
}

void
generate_unique_visitors (struct struct_display **disp, struct logger *logger)
{
   int i, r;
   int s_size = g_hash_table_size (ht_unique_vis);
   struct struct_holder **t_holder;

   t_holder = malloc (sizeof (struct struct_holder *) * s_size);
   g_hash_table_foreach (ht_unique_vis, (GHFunc) unique_visitors_iter,
                         t_holder);

   qsort (t_holder, s_size, sizeof (struct struct_holder *), struct_cmp_desc);

   /* fixed value in here, perhaps it could be dynamic depending on the module */
   for (i = 0, r = 0; i < 10; i++) {
      disp[logger->alloc_counter]->hits = 0;
      disp[logger->alloc_counter]->module = 1;
      if (i == 0)
         disp[logger->alloc_counter++]->data = alloc_string (vis_head);
      else if (i == 1)
         disp[logger->alloc_counter++]->data = alloc_string (vis_desc);
      else if (i == 2 || i == 9)
         disp[logger->alloc_counter++]->data = alloc_string ("");
      else if (r < s_size) {
         disp[logger->alloc_counter]->hits = t_holder[r]->hits;
         disp[logger->alloc_counter++]->data =
            alloc_string (t_holder[r]->data);
         r++;
      } else
         disp[logger->alloc_counter++]->data = alloc_string ("");
   }

   for (i = 0; i < s_size; i++)
      free (t_holder[i]);
   free (t_holder);

   iter_ctr = 0;
}

static void
struct_data_iter (gpointer k, gpointer v, struct struct_holder **s_holder)
{
   if (iter_module == BROWSERS || iter_module == OS)
      if (strchr ((gchar *) k, '|') != NULL)
         return;
   s_holder[iter_ctr]->data = (gchar *) k;
   s_holder[iter_ctr++]->hits = GPOINTER_TO_INT (v);
}

void
generate_struct_data (GHashTable * hash_table,
                      struct struct_holder **s_holder,
                      struct struct_display **disp, struct logger *logger,
                      int module)
{
   int row, col;
   int i = 0, r;
   getmaxyx (stdscr, row, col);

   iter_module = module;
   g_hash_table_foreach (hash_table, (GHFunc) struct_data_iter, s_holder);
   qsort (s_holder, iter_ctr, sizeof (struct struct_holder *),
          struct_cmp_by_hits);

   /* headers & sub-headers */
   const char *head = NULL, *desc = NULL;
   switch (module) {
    case 2:
       head = req_head;
       desc = req_desc;
       break;
    case 3:
       head = static_head;
       desc = status_desc;
       break;
    case 4:
       head = ref_head;
       desc = ref_desc;
       break;
    case 5:
       head = not_found_head;
       desc = not_found_desc;
       break;
    case 6:
       head = os_head;
       desc = os_desc;
       break;
    case 7:
       head = browser_head;
       desc = browser_desc;
       break;
    case 8:
       head = host_head;
       desc = host_desc;
       break;
    case 9:
       head = status_head;
       desc = status_desc;
       break;
    case 10:
       head = sites_head;
       desc = sites_desc;
       break;
    case 11:
       head = key_head;
       desc = key_desc;
       break;
   }

   for (i = 0, r = 0; i < 10; i++) {
      disp[logger->alloc_counter]->hits = 0;
      disp[logger->alloc_counter]->module = module;
      if (i == 0)
         disp[logger->alloc_counter++]->data = alloc_string (head);
      else if (i == 1)
         disp[logger->alloc_counter++]->data = alloc_string (desc);
      else if (i == 2 || i == 9)
         disp[logger->alloc_counter++]->data = alloc_string ("");
      else if (r < iter_ctr) {
         disp[logger->alloc_counter]->data = alloc_string (s_holder[r]->data);
         disp[logger->alloc_counter++]->hits = s_holder[r]->hits;
         r++;
      } else
         disp[logger->alloc_counter++]->data = alloc_string ("");
   }

   for (i = 0; i < g_hash_table_size (hash_table); i++)
      free (s_holder[i]);
   free (s_holder);
   iter_ctr = 0;
}
