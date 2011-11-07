/** 
 * parser.c -- web log parsing
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - Ncurses apache weblog analyzer & interactive viewer
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

#include "error.h"
#include "commons.h"
#include "parser.h"
#include "util.h"

static int
is_agent_present (const char *str)
{
   return (str && *str && str[strlen (str) - 2] == '"') ? 0 : 1;
}

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
process_request_bw (char *host, char *date, char *req, long long resp_size)
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
         add_value = *ptr_value + resp_size;
      } else
         add_value = 0 + resp_size;

      if (resp_size == -1)
         add_value = 0;

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
      char *dup_k_b = strdup (key), *dup_k_o = strdup (key);
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
      free (dup_k_b);
      free (dup_k_o);
      free (o_key);
      free (b_key);
      if ((date = strchr (key, '|')) != NULL) {
         char *tmp_date;
         tmp_date = clean_date (date);
         process_generic_data (ht_unique_vis, tmp_date);
         free (tmp_date);
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
   if ((ptr = strstr (dec, "%26")) != NULL
       || (ptr = strchr (dec, '&')) != NULL)
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
   struct tm tm;
   char buf[32];

   memset (&tm, 0, sizeof (tm));
   memset (buf, 0, sizeof (buf));

   if (strptime (date, "%d/%b/%Y", &tm) == NULL)
      return;
   strftime (buf, sizeof (buf) - 1, "%Y%m%d", &tm);

   snprintf (unique_visitors_key, sizeof (unique_visitors_key), "%s|%s|%s",
             host, buf, agent);
   unique_visitors_key[sizeof (unique_visitors_key) - 1] = 0;

   char url[512] = "";
   if (sscanf (referrer, "http://%511[^/\n]", url) == 1) {
      process_generic_data (ht_referring_sites, url);
   }
   process_keyphrases (referrer);

   if (http_status_code_flag) {
      process_generic_data (ht_status_code, status);
   }

   if (ignore_host != NULL && strcmp (host, ignore_host) == 0) {
      /* ignore */
   } else {
      process_generic_data (ht_hosts, host);
   }
   if (agent[strlen (agent) - 1] == '\n')
      agent[strlen (agent) - 1] = 0;
   if (host_agents_list_flag)
      process_host_agents (host, agent);
   process_generic_data (ht_unique_visitors, unique_visitors_key);
}

static int
verify_static_content (char *url)
{
   char *nul = url + strlen (url);

   if (strlen (url) < 5)
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

   if ((lookfor = "\"GET ", req_l = strstr (line, lookfor)) != NULL ||
       (lookfor = "\"POST ", req_l = strstr (line, lookfor)) != NULL ||
       (lookfor = "\"HEAD ", req_l = strstr (line, lookfor)) != NULL ||
       (lookfor = "\"get ", req_l = strstr (line, lookfor)) != NULL ||
       (lookfor = "\"post ", req_l = strstr (line, lookfor)) != NULL ||
       (lookfor = "\"head ", req_l = strstr (line, lookfor)) != NULL) {

      /* The last part of the request is the protocol being used, 
         at the time of this writing typically HTTP/1.0 or HTTP/1.1. */
      if ((req_r = strstr (line, " HTTP")) == NULL)
         /* didn't find it :( weird */
         return alloc_string ("-");

      req_l += strlen (lookfor);
      ptrdiff_t req_len = req_r - req_l;

      /* make sure we don't have some weird requests */
      if (req_len < 0)
         return alloc_string ("-");

      reqs = malloc (req_len + 1);
      strncpy (reqs, req_l, req_len);
      (reqs)[req_len] = 0;
   } else
      reqs = alloc_string ("-");

   return reqs;
}

static int
parse_req_size (char *line, int format)
{
   long size = 0;

   /* Common Log Format */
   if ((strstr (line, " -\n") != NULL))
      return -1;

   /* Common Log Format */
   char *c;
   if (format) {
      if ((c = strrchr (trim_str (line), ' ')) != 0)
         size = strtol (c + 1, NULL, 10);
      return size;
   }
   /* Combined Log Format */
   if ((c = strstr (line, "1.1\" ")) != NULL
       || (c = strstr (line, "1.0\" ")) != NULL)
      c++;
   else
      return -1;                /* no protocol used? huh... */

   char *p = NULL;
   if ((p = strchr (c + 6, ' ')) != 0)
      size = strtol (p + 1, NULL, 10);
   else
      size = -1;

   return size;
}

static int
parse_request (struct logger *logger, char *line, char *cpy_line,
               char **status_code, char **req)
{
   char *ptr, *prb = NULL, *fqm = NULL, *sqm =
      NULL, *host, *date, *ref, *h, *p;
   int format = 0;
   long long band_size = 0;

   host = line;
   if ((h = strchr (line, ' ')) == NULL)
      return 1;

   /* vhost_combined? */
   for (p = h; *p; p++) {
      if (isdigit (p[1]) && isspace (p[0])) {
         host = h + 1;
         line = h + 1;
      } else
         break;
   }

   if ((date = strchr (line, '[')) == NULL)
      return 1;
   date++;

   /* agent */
   if (is_agent_present (line)) {
      format = 1;
      fqm = "-";
      goto noagent;
   }
   for (prb = line; *prb; prb++) {
      if (*prb != '"')
         continue;
      else if (fqm == 0)
         fqm = prb;
      else if (sqm == 0)
         sqm = prb;
      else {
         fqm = sqm;
         sqm = prb;
      }
   }
 noagent:;
   if ((ref = strstr (line, "\"http")) != NULL
       || (ref = strstr (line, "\"HTTP")) != NULL)
      ref++;
   else
      ref = "-";

   if (!bandwidth_flag)
      goto nobanwidth;
   /* bandwidth */
   band_size = parse_req_size (cpy_line, format);
   if (band_size != -1)
      req_size = req_size + band_size;
   else
      band_size = 0;
 nobanwidth:;

   if ((ptr = strchr (host, ' ')) == NULL)
      return 1;
   *ptr = '\0';
   if ((ptr = strchr (date, ']')) == NULL)
      return 1;
   *ptr = '\0';
   if ((ptr = strchr (date, ':')) == NULL)
      return 1;
   *ptr = '\0';
   if ((ptr = strchr (ref, '"')) == NULL)
      ref = "-";
   else
      *ptr = '\0';

   /* req */
   *req = parse_req (cpy_line);

   if (!http_status_code_flag)
      goto nohttpstatuscode;

   char *lookfor = NULL, *s_l;
   if ((lookfor = "/1.0\" ", s_l = strstr (cpy_line, lookfor)) != NULL ||
       (lookfor = "/1.1\" ", s_l = strstr (cpy_line, lookfor)) != NULL)
      *status_code = clean_status (s_l + 6);
   else
      /* perhaps something wrong with the log,
         more likely malformed request syntax */
      *status_code = alloc_string ("---");
 nohttpstatuscode:;

   logger->host = host;
   logger->agent = fqm;
   logger->date = date;
   logger->referrer = ref;
   logger->request = *req;
   logger->resp_size = band_size;

   if (http_status_code_flag)
      logger->status = *status_code;

   return 0;
}

static int
process_log (struct logger *logger, char *line)
{
   char *cpy_line = strdup (line);
   char *status_code = NULL, *req;
   struct logger log;
   logger->total_process++;

   /* Make compiler happy */
   memset (&log, 0, sizeof (log));
   if (parse_request (&log, line, cpy_line, &status_code, &req) != 0) {
      free (cpy_line);
      logger->total_invalid++;
      return 0;
   }

   process_unique_data (log.host, log.date, log.agent, log.status,
                        log.referrer);
   if (http_status_code_flag)
      free (status_code);
   if (verify_static_content (log.request)) {
      if (strstr (cpy_line, "\" 404 "))
         process_generic_data (ht_not_found_requests, log.request);
      process_generic_data (ht_requests_static, log.request);
   } else {
      if (strstr (cpy_line, "\" 404 "))
         process_generic_data (ht_not_found_requests, log.request);
      process_generic_data (ht_requests, log.request);
   }
   process_generic_data (ht_referrers, log.referrer);
   if (bandwidth_flag)
      process_request_bw (log.host, log.date, log.request, log.resp_size);
   free (cpy_line);
   free (req);
   return 0;
}

int
parse_log (struct logger *logger, char *filename, char *tail)
{
   FILE *fp = NULL;
   char line[BUFFER];

   if (tail != NULL) {
      if (process_log (logger, tail))
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
      if (process_log (logger, line)) {
         if (!piping)
            fclose (fp);
         return 1;
      }
   }
   /* definitely not portable! */
   if (piping)
      stdin = freopen ("/dev/tty", "r", stdin);

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
generate_unique_visitors (struct struct_display **s_display,
                          struct logger *logger)
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
      s_display[logger->alloc_counter]->hits = 0;
      s_display[logger->alloc_counter]->module = 1;
      if (i == 0)
         s_display[logger->alloc_counter++]->data =
            alloc_string (" 1 - Unique visitors per day - Including spiders");
      else if (i == 1)
         s_display[logger->alloc_counter++]->data =
            alloc_string
            (" HTTP requests having the same IP, same date and same agent are considered a unique visit");
      else if (i == 2 || i == 9)
         s_display[logger->alloc_counter++]->data = alloc_string ("");
      else if (r < s_size) {
         s_display[logger->alloc_counter]->hits = t_holder[r]->hits;
         s_display[logger->alloc_counter++]->data =
            alloc_string (t_holder[r]->data);
         r++;
      } else
         s_display[logger->alloc_counter++]->data = alloc_string ("");
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
                      struct struct_display **s_display,
                      struct logger *logger, int module)
{
   int row, col;
   int i = 0, r;
   getmaxyx (stdscr, row, col);

   iter_module = module;
   g_hash_table_foreach (hash_table, (GHFunc) struct_data_iter, s_holder);
   qsort (s_holder, iter_ctr, sizeof (struct struct_holder *),
          struct_cmp_by_hits);

   /* headers & sub-headers */
   char *head = NULL, *desc = NULL;
   switch (module) {
    case 2:
       head = " 2 - Requested files (Pages-URL)";
       desc =
          " Top 6 different files requested sorted by requests - percent - [bandwidth]";
       break;
    case 3:
       head = " 3 - Requested static files - (Static content: png,js,etc)";
       desc =
          " Top 6 different static files requested, sorted by requests - percent - [bandwidth]";
       break;
    case 4:
       head = " 4 - Referrers URLs";
       desc = " Top 6 different referrers sorted by requests";
       break;
    case 5:
       head = " 5 - HTTP 404 Not Found response code";
       desc = " Top 6 different 404 sorted by requests";
       break;
    case 6:
       head = " 6 - Operating Systems";
       desc = " Top 6 different Operating Systems sorted by unique requests";
       break;
    case 7:
       head = " 7 - Browsers";
       desc = " Top 6 different browsers sorted by unique requests";
       break;
    case 8:
       head = " 8 - Hosts";
       desc = " Top 6 different hosts sorted by requests";
       break;
    case 9:
       head = " 9 - HTTP Status Codes";
       desc = " Top 6 different status codes sorted by requests";
       break;
    case 10:
       head = " 10 - Top Referring Sites";
       desc = " Top 6 different referring sites sorted by requests";
       break;
    case 11:
       head = " 11 - Top Keyphrases used on Google's search engine";
       desc = " Top 6 different keyphrases sorted by requests";
       break;
   }

   for (i = 0, r = 0; i < 10; i++) {
      s_display[logger->alloc_counter]->hits = 0;
      s_display[logger->alloc_counter]->module = module;
      if (i == 0)
         s_display[logger->alloc_counter++]->data = alloc_string (head);
      else if (i == 1)
         s_display[logger->alloc_counter++]->data = alloc_string (desc);
      else if (i == 2 || i == 9)
         s_display[logger->alloc_counter++]->data = alloc_string ("");
      else if (r < iter_ctr) {
         s_display[logger->alloc_counter]->data =
            alloc_string (s_holder[r]->data);
         s_display[logger->alloc_counter++]->hits = s_holder[r]->hits;
         r++;
      } else
         s_display[logger->alloc_counter++]->data = alloc_string ("");
   }

   for (i = 0; i < g_hash_table_size (hash_table); i++)
      free (s_holder[i]);
   free (s_holder);
   iter_ctr = 0;
}
