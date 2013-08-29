/**
 * parser.c -- web log parsing
 * Copyright (C) 2009-2013 by Gerardo Orellana <goaccess@prosoftcorp.com>
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

/*
 * "_XOPEN_SOURCE" is required for the GNU libc to export "strptime(3)"
 * correctly.
 */
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#define _XOPEN_SOURCE 700

#include <ctype.h>
#include <errno.h>

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LIBNCURSESW
#include <ncursesw/curses.h>
#else
#include <ncurses.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <arpa/inet.h>

#include "parser.h"

#include "commons.h"
#include "error.h"
#include "xmalloc.h"
#include "gdashboard.h"
#include "settings.h"
#include "util.h"

/* *INDENT-OFF* */
/* Definitions checked against declarations */
GHashTable *ht_browsers               = NULL;
GHashTable *ht_date_bw                = NULL;
GHashTable *ht_file_bw                = NULL;
GHashTable *ht_file_serve_usecs       = NULL;
GHashTable *ht_host_serve_usecs       = NULL;
GHashTable *ht_host_bw                = NULL;
GHashTable *ht_hostnames              = NULL;
GHashTable *ht_hosts_agents           = NULL;
GHashTable *ht_hosts                  = NULL;
GHashTable *ht_keyphrases             = NULL;
GHashTable *ht_monthly                = NULL;
GHashTable *ht_not_found_requests     = NULL;
GHashTable *ht_os                     = NULL;
GHashTable *ht_referrers              = NULL;
GHashTable *ht_referring_sites        = NULL;
GHashTable *ht_requests               = NULL;
GHashTable *ht_requests_static        = NULL;
GHashTable *ht_status_code            = NULL;
GHashTable *ht_countries              = NULL;
GHashTable *ht_continents             = NULL;
GHashTable *ht_unique_visitors        = NULL;
GHashTable *ht_unique_vis             = NULL;
/* *INDENT-ON* */

/* sort data ascending */
int
cmp_data_asc (const void *a, const void *b)
{
   const GHolderItem *ia = a;
   const GHolderItem *ib = b;
   return strcmp (ia->data, ib->data);
}

/* sort data descending */
int
cmp_data_desc (const void *a, const void *b)
{
   const GHolderItem *ia = a;
   const GHolderItem *ib = b;
   return strcmp (ib->data, ia->data);
}

/* sort numeric descending */
int
cmp_num_desc (const void *a, const void *b)
{
   const GHolderItem *ia = a;
   const GHolderItem *ib = b;
   return (int) (ib->hits - ia->hits);
}

/* sort numeric ascending */
int
cmp_num_asc (const void *a, const void *b)
{
   const GHolderItem *ia = a;
   const GHolderItem *ib = b;
   return (int) (ia->hits - ib->hits);
}

/* sort bandwidth descending */
int
cmp_bw_desc (const void *a, const void *b)
{
   const GHolderItem *ia = a;
   const GHolderItem *ib = b;
   return (unsigned long long) (ib->bw - ia->bw);
}

/* sort bandwidth ascending */
int
cmp_bw_asc (const void *a, const void *b)
{
   const GHolderItem *ia = a;
   const GHolderItem *ib = b;
   return (unsigned long long) (ia->bw - ib->bw);
}

/* sort usec descending */
int
cmp_usec_desc (const void *a, const void *b)
{
   const GHolderItem *ia = a;
   const GHolderItem *ib = b;
   return (unsigned long long) (ib->usecs - ia->usecs);
}

/* sort usec ascending */
int
cmp_usec_asc (const void *a, const void *b)
{
   const GHolderItem *ia = a;
   const GHolderItem *ib = b;
   return (unsigned long long) (ia->usecs - ib->usecs);
}

void
reset_struct (GLog * logger)
{
   logger->invalid = 0;
   logger->process = 0;
   logger->resp_size = 0LL;
}

GLog *
init_log (void)
{
   GLog *log = xmalloc (sizeof (GLog));
   memset (log, 0, sizeof *log);

   return log;
}

GLogItem *
init_log_item (GLog * logger)
{
   logger->items = xmalloc (sizeof (GLogItem));
   GLogItem *log = logger->items;
   memset (log, 0, sizeof *log);

   log->agent = NULL;
   log->date = NULL;
   log->host = NULL;
   log->ref = NULL;
   log->req = NULL;
   log->status = NULL;
   log->resp_size = 0LL;
   log->serve_time = 0;

   return log;
}

static void
free_logger (GLogItem * log)
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
   free (log);
}

/* process bandwidth & time taken to serve the request */
static int
process_request_meta (GHashTable * ht, char *key, unsigned long long size)
{
   if ((ht == NULL) || (key == NULL))
      return (EINVAL);

   gpointer value_ptr;
   unsigned long long add_value;
   unsigned long long *ptr_value;

   value_ptr = g_hash_table_lookup (ht, key);
   if (value_ptr != NULL) {
      ptr_value = (unsigned long long *) value_ptr;
      add_value = *ptr_value + size;
   } else
      add_value = 0 + size;

   ptr_value = xmalloc (sizeof (unsigned long long));
   *ptr_value = add_value;

   g_hash_table_replace (ht, g_strdup (key), ptr_value);

   return 0;
}

/* process data based on the amount of requests */
static int
process_generic_data (GHashTable * ht, const char *key)
{
   int first = 0;

   gint value_int;
   gpointer value_ptr;

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

   /* replace the entry. old key will be freed by "free_key_value" */
   g_hash_table_replace (ht, g_strdup (key), GINT_TO_POINTER (value_int));

   return first ? KEY_NOT_FOUND : KEY_FOUND;
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

   if (!(out = ptr = xstrdup (url)))
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
   if (strlen (url) == (ptr - out)) {
   }
   return trim_str (out);
}

/* process keyphrases used on Google search, Google cache, and
 * Google translate. */
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
   } else if ((r = strstr (ref, "&q=")) != NULL ||
              (r = strstr (ref, "?q=")) != NULL)
      r += 3;
   else if ((r = strstr (ref, "%26q%3D")) != NULL ||
            (r = strstr (ref, "%3Fq%3D")) != NULL)
      r += 7;
   else
      return -1;
   dec = spc_decode_url (r);
   if ((ptr = strstr (dec, "%26")) != NULL || (ptr = strchr (dec, '&')) != NULL)
      *ptr = '\0';

   p = dec;
   if (p[0] == '\0') {
      if (dec != NULL)
         free (dec);
      return -1;
   }
   while (*p != '\0') {
      if (*p == '+')
         *p = ' ';
      p++;
   }
   process_generic_data (ht_keyphrases, trim_str (dec));
   if (dec != NULL)
      free (dec);

   return 0;
}

/* process host agent strings */
static int
process_host_agents (char *host, char *agent)
{
   GHashTable *ht = ht_hosts_agents;
   gpointer value_ptr;

   if ((ht == NULL) || (host == NULL) || (agent == NULL))
      return (EINVAL);

   char *ptr_value, *tmp, *a = spc_decode_url (agent);

   value_ptr = g_hash_table_lookup (ht, host);
   if (value_ptr != NULL) {
      ptr_value = (char *) value_ptr;
      if (strstr (ptr_value, a)) {
         if (a != NULL)
            free (a);
         return 0;
      }

      size_t len1 = strlen (ptr_value);
      size_t len2 = strlen (a);

      tmp = xmalloc (len1 + len2 + 2);
      memcpy (tmp, ptr_value, len1);
      tmp[len1] = '|';
      /* NUL-terminated */
      memcpy (tmp + len1 + 1, a, len2 + 1);
   } else
      tmp = alloc_string (a);

   g_hash_table_replace (ht, g_strdup (host), tmp);
   if (a != NULL)
      free (a);
   return 0;
}

/* process country */
static void
process_country (const char *countryname)
{
   process_generic_data (ht_countries, countryname);
}

/* process continent */
static void
process_continent (const char *continentid)
{
   if (memcmp (continentid, "NA", 2) == 0)
      process_generic_data (ht_continents, "North America");
   else if (memcmp (continentid, "OC", 2) == 0)
      process_generic_data (ht_continents, "Oceania");
   else if (memcmp (continentid, "EU", 2) == 0)
      process_generic_data (ht_continents, "Europe");
   else if (memcmp (continentid, "SA", 2) == 0)
      process_generic_data (ht_continents, "South America");
   else if (memcmp (continentid, "AF", 2) == 0)
      process_generic_data (ht_continents, "Africa");
   else if (memcmp (continentid, "AN", 2) == 0)
      process_generic_data (ht_continents, "Antarctica");
   else if (memcmp (continentid, "AS", 2) == 0)
      process_generic_data (ht_continents, "Asia");
   else
      process_generic_data (ht_continents, "Unknown");
}

/* process referer */
static void
process_referrers (char *referrer)
{
   if (referrer == NULL)
      return;

   char *ref = spc_decode_url (referrer);
   char url[512] = "";
   /* extract the host part, i.e., www.foo.com */
   if (sscanf (ref, "%*[^/]%*[/]%[^/]", url) == 1)
      process_generic_data (ht_referring_sites, url);
   process_generic_data (ht_referrers, ref);
   process_keyphrases (referrer);
   free (ref);
}

/* process data based on a unique key, this includes the following
 * modules, VISITORS, BROWSERS, OS */
static void
process_unique_data (char *host, char *date, char *agent)
{
   char visitor_key[2048];
   char *browser = NULL;
   char *opsys = NULL;
   char *dup_key = NULL, *clean_key = NULL;
   char *browser_key = NULL;
   char *os_key = NULL;

   if (agent == NULL)
      agent = "-";

   snprintf (visitor_key, sizeof (visitor_key), "%s|%s|%s", host, date, agent);
   (visitor_key)[sizeof (visitor_key) - 1] = '\0';

   /* insert agents that are part of a host */
   if (conf.list_agents)
      process_host_agents (host, agent);

   /* process unique data  */
   if (process_generic_data (ht_unique_visitors, visitor_key) == -1) {
      /* replace '+' in user-agent + with ' ' */
      clean_key = spc_decode_url (visitor_key);
      dup_key = char_replace (strdup (clean_key), '+', ' ');

      browser_key = strdup (dup_key);
      os_key = strdup (dup_key);

      /* extract browser & OS from agent  */
      browser = verify_browser (browser_key, BROWSER);
      opsys = verify_os (os_key, OPESYS);

      if (browser != NULL)
         process_generic_data (ht_browsers, browser);

      if (opsys != NULL)
         process_generic_data (ht_os, opsys);

      if ((date = strchr (visitor_key, '|')) != NULL) {
         char *tmp_date = NULL;
         tmp_date = clean_date (++date);
         if (tmp_date != NULL) {
            process_generic_data (ht_unique_vis, tmp_date);
            free (tmp_date);
         }
      }
   }

   if (browser != NULL)
      free (browser);
   if (browser_key != NULL)
      free (browser_key);
   if (os_key != NULL)
      free (os_key);
   if (opsys != NULL)
      free (opsys);
   if (dup_key != NULL)
      free (dup_key);
   if (clean_key != NULL)
      free (clean_key);
}

/* returns 1 if the request seems to be a static file */
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

      /* didn't find it - weird  */
      if ((req_r = strstr (line, " HTTP/1.0")) == NULL &&
          (req_r = strstr (line, " HTTP/1.1")) == NULL)
         return alloc_string ("-");

      req_l += strlen (lookfor);
      ptrdiff_t req_len = req_r - req_l;

      /* make sure we don't have some weird requests */
      if (req_len <= 0)
         return alloc_string ("-");

      reqs = xmalloc (req_len + 1);
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
         p = xmalloc (len);
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
parse_format (GLogItem * log, const char *fmt, const char *date_format,
              char *str)
{
   const char *p;

   if (str == NULL || *str == '\0')
      return 1;

   struct tm tm;
   memset (&tm, 0, sizeof (tm));

   int special = 0;
   /* iterate over the log format */
   for (p = fmt; *p; p++) {
      if (*p == '%') {
         special++;
         continue;
      }
      if (special && *p != '\0') {
         char *pch, *sEnd, *bEnd, *tkn = NULL, *end = NULL;
         errno = 0;
         unsigned long long bandw = 0;
         unsigned long long serve_time = 0;
         double serve_secs = 0;

         switch (*p) {
             /* date */
          case 'd':
             if (log->date)
                return 1;
             tkn = parse_string (&str, p[1]);
             if (tkn == NULL)
                return 1;
             end = strptime (tkn, date_format, &tm);
             if (end == NULL || *end != '\0') {
                free (tkn);
                return 1;
             }
             log->date = tkn;
             break;
             /* remote hostname (IP only) */
          case 'h':
             if (log->host)
                return 1;
             tkn = parse_string (&str, p[1]);
             if (tkn == NULL)
                return 1;
             if (invalid_ipaddr (tkn)) {
                free (tkn);
                return 1;
             }
             log->host = tkn;
             break;
             /* request */
          case 'r':
             if (log->req)
                return 1;
             tkn = parse_string (&str, p[1]);
             if (tkn == NULL)
                return 1;
             log->req = parse_req (tkn);
             free (tkn);
             break;
             /* Status Code */
          case 's':
             if (log->status)
                return 1;
             tkn = parse_string (&str, p[1]);
             if (tkn == NULL)
                return 1;
             strtol (tkn, &sEnd, 10);
             if (tkn == sEnd || *sEnd != '\0' || errno == ERANGE) {
                free (tkn);
                return 1;
             }
             log->status = tkn;
             break;
             /* size of response in bytes - excluding HTTP headers */
          case 'b':
             if (log->resp_size)
                return 1;
             tkn = parse_string (&str, p[1]);
             if (tkn == NULL)
                return 1;
             bandw = strtol (tkn, &bEnd, 10);
             if (tkn == bEnd || *bEnd != '\0' || errno == ERANGE)
                bandw = 0;
             log->resp_size = bandw;
             conf.bandwidth = 1;
             free (tkn);
             break;
             /* referrer */
          case 'R':
             if (log->ref)
                return 1;
             tkn = parse_string (&str, p[1]);
             if (tkn == NULL)
                tkn = alloc_string ("-");
             if (tkn != NULL && *tkn == '\0') {
                free (tkn);
                tkn = alloc_string ("-");
             }
             log->ref = tkn;
             break;
             /* user agent */
          case 'u':
             if (log->agent)
                return 1;
             tkn = parse_string (&str, p[1]);
             if (tkn == NULL)
                tkn = alloc_string ("-");
             if (tkn != NULL && *tkn == '\0') {
                free (tkn);
                tkn = alloc_string ("-");
             }
             log->agent = tkn;
             break;
             /* time taken to serve the request, in seconds */
          case 'T':
             if (log->serve_time)
                return 1;
             /* ignore seconds if we have microseconds */
             if (strstr (fmt, "%D") != NULL)
                break;

             tkn = parse_string (&str, p[1]);
             if (tkn == NULL)
                return 1;
             serve_secs = strtoull (tkn, &bEnd, 10);
             if (tkn == bEnd || *bEnd != '\0' || errno == ERANGE)
                serve_secs = 0;
             /* convert it to microseconds */
             if (serve_secs > 0)
                log->serve_time = serve_secs * SECS;
             else
                log->serve_time = 0;
             conf.serve_usecs = 1;
             free (tkn);
             break;
             /* time taken to serve the request, in microseconds */
          case 'D':
             if (log->serve_time)
                return 1;
             tkn = parse_string (&str, p[1]);
             if (tkn == NULL)
                return 1;
             serve_time = strtoull (tkn, &bEnd, 10);
             if (tkn == bEnd || *bEnd != '\0' || errno == ERANGE)
                serve_time = 0;
             log->serve_time = serve_time;
             conf.serve_usecs = 1;
             free (tkn);
             break;
             /* everything else skip it */
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

/* process a line from the log and store it accordingly */
static int
process_log (GLog * logger, char *line, int test)
{
   int geo_id;
   const char *location = NULL;
   char buf[DATE_LEN];

   /* make compiler happy */
   memset (buf, 0, sizeof (buf));

   if (conf.date_format == NULL || *conf.date_format == '\0')
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "No date format was found on your conf file.");

   if (conf.log_format == NULL || *conf.log_format == '\0')
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "No log format was found on your conf file.");

   if ((line == NULL) || (*line == '\0')) {
      logger->invalid++;
      return 0;
   }

   if (*line == '#' || *line == '\n')
      return 0;

   logger->process++;

   GLogItem *log = init_log_item (logger);
   if (parse_format (log, conf.log_format, conf.date_format, line) == 1) {
      free_logger (log);
      logger->invalid++;
      return 0;
   }

   if (log->host == NULL || log->date == NULL || log->status == NULL ||
       log->req == NULL) {
      free_logger (log);
      logger->invalid++;
      return 0;
   }

   if (test) {
      free_logger (log);
      return 0;
   }

   convert_date (buf, log->date, conf.date_format, "%Y%m%d", DATE_LEN);
   if (buf == NULL)
      return 0;

   /* ignore host */
   if (conf.ignore_host != NULL && strcmp (log->host, conf.ignore_host) == 0)
      return 0;

   /* process visitors, browsers, and OS */
   process_unique_data (log->host, buf, log->agent);

   /* process 404s */
   if (!memcmp (log->status, "404", 3))
      process_generic_data (ht_not_found_requests, log->req);

   /* process static files */
   if (verify_static_content (log->req))
      process_generic_data (ht_requests_static, log->req);
   /* process regular files */
   else
      process_generic_data (ht_requests, log->req);

   /* process referrers */
   process_referrers (log->ref);
   /* process status codes */
   process_generic_data (ht_status_code, log->status);

#ifdef HAVE_LIBGEOIP
   geo_id = GeoIP_id_by_name (geo_location_data, log->host);

   /* process country */
   location = get_geoip_data (log->host);
   process_country (location);

   /* process continent */
   process_continent (GeoIP_continent_by_id (geo_id));
#endif

   /* process hosts */
   process_generic_data (ht_hosts, log->host);

   /* process bandwidth  */
   process_request_meta (ht_date_bw, buf, log->resp_size);
   process_request_meta (ht_file_bw, log->req, log->resp_size);
   process_request_meta (ht_host_bw, log->host, log->resp_size);

   /* process time taken to serve the request, in microseconds */
   process_request_meta (ht_file_serve_usecs, log->req, log->serve_time);
   process_request_meta (ht_host_serve_usecs, log->host, log->serve_time);

   logger->resp_size += log->resp_size;

   free_logger (log);
   return 0;
}

/* entry point to parse the log line by line */
int
parse_log (GLog ** logger, char *tail, int n)
{
   FILE *fp = NULL;

   char line[LINE_BUFFER];
   int i = 0, test = -1 == n ? 0 : 1;

   if (tail != NULL) {
      if (process_log ((*logger), tail, test))
         return 1;
      return 0;
   }

   if (conf.ifile == NULL) {
      fp = stdin;
      (*logger)->piping = 1;
   }

   /* make sure we can open the log (if not reading from stdin) */
   if (!(*logger)->piping && (fp = fopen (conf.ifile, "r")) == NULL)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__, strerror (errno));

   while (fgets (line, LINE_BUFFER, fp) != NULL) {
      if (n >= 0 && i++ == n)
         break;
      if (process_log ((*logger), line, test)) {
         if (!(*logger)->piping)
            fclose (fp);
         return 1;
      }
   }

   /* definitely not portable! */
   if ((*logger)->piping)
      freopen ("/dev/tty", "r", stdin);

   if (!(*logger)->piping)
      fclose (fp);
   return 0;
}

/* make sure we have valid hits */
int
test_format (GLog * logger)
{
   if (parse_log (&logger, NULL, 20))
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Error while processing file");

   if ((logger->process == 0) || (logger->process == logger->invalid))
      return 1;
   return 0;
}
