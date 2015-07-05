/**
 * parser.c -- web log parsing
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

#include <arpa/inet.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabdb.h"
#include "tcbtdb.h"
#else
#include "glibht.h"
#endif

#ifdef HAVE_LIBGEOIP
#include "geolocation.h"
#endif

#include "parser.h"

#include "browsers.h"
#include "goaccess.h"
#include "error.h"
#include "opesys.h"
#include "util.h"
#include "xmalloc.h"

/* private prototypes */

/* key/data generators for each module */
static int gen_visitor_key (GKeyData * kdata, GLogItem * glog);
static int gen_404_key (GKeyData * kdata, GLogItem * glog);
static int gen_browser_key (GKeyData * kdata, GLogItem * glog);
static int gen_host_key (GKeyData * kdata, GLogItem * glog);
static int gen_keyphrase_key (GKeyData * kdata, GLogItem * glog);
static int gen_os_key (GKeyData * kdata, GLogItem * glog);
static int gen_referer_key (GKeyData * kdata, GLogItem * glog);
static int gen_ref_site_key (GKeyData * kdata, GLogItem * glog);
static int gen_request_key (GKeyData * kdata, GLogItem * glog);
static int gen_static_request_key (GKeyData * kdata, GLogItem * glog);
static int gen_status_code_key (GKeyData * kdata, GLogItem * glog);
static int gen_visit_time_key (GKeyData * kdata, GLogItem * glog);
#ifdef HAVE_LIBGEOIP
static int gen_geolocation_key (GKeyData * kdata, GLogItem * glog);
#endif

/* insertion routines */
static void insert_data (int data_nkey, const char *data, GModule module);
static void insert_root (int root_nkey, const char *root, GModule module);

/* insertion metric routines */
static void insert_hit (int data_nkey, int uniq_nkey, int root_nkey,
                        GModule module);
static void insert_visitor (int uniq_nkey, GModule module);
static void insert_bw (int data_nkey, uint64_t size, GModule module);
static void insert_time_served (int data_nkey, uint64_t ts, GModule module);
static void insert_method (int data_nkey, const char *method, GModule module);
static void insert_protocol (int data_nkey, const char *proto, GModule module);
static void insert_agent (int data_nkey, int agent_nkey, GModule module);

/* *INDENT-OFF* */
static GParse paneling[] = {
  {
    VISITORS,
    gen_visitor_key,
    insert_data,
    NULL,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_time_served,
    NULL,
    NULL,
    NULL,
  }, {
    REQUESTS,
    gen_request_key,
    insert_data,
    NULL,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_time_served,
    insert_method,
    insert_protocol,
    NULL,
  }, {
    REQUESTS_STATIC,
    gen_static_request_key,
    insert_data,
    NULL,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_time_served,
    insert_method,
    insert_protocol,
    NULL,
  }, {
    NOT_FOUND,
    gen_404_key,
    insert_data,
    NULL,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_time_served,
    insert_method,
    insert_protocol,
    NULL,
  }, {
    HOSTS,
    gen_host_key,
    insert_data,
    NULL,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_time_served,
    NULL,
    NULL,
    insert_agent,
  }, {
    OS,
    gen_os_key,
    insert_data,
    insert_root,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_time_served,
    insert_method,
    insert_protocol,
    NULL,
  }, {
    BROWSERS,
    gen_browser_key,
    insert_data,
    insert_root,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_time_served,
    NULL,
    NULL,
    NULL,
  }, {
    REFERRERS,
    gen_referer_key,
    insert_data,
    NULL,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_time_served,
    NULL,
    NULL,
    NULL,
  }, {
    REFERRING_SITES,
    gen_ref_site_key,
    insert_data,
    NULL,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_time_served,
    NULL,
    NULL,
    NULL,
  }, {
    KEYPHRASES,
    gen_keyphrase_key,
    insert_data,
    NULL,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_time_served,
    NULL,
    NULL,
    NULL,
  },
#ifdef HAVE_LIBGEOIP
  {
    GEO_LOCATION,
    gen_geolocation_key,
    insert_data,
    insert_root,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_time_served,
    NULL,
    NULL,
    NULL,
  },
#endif
  {
    STATUS_CODES,
    gen_status_code_key,
    insert_data,
    insert_root,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_time_served,
    NULL,
    NULL,
    NULL,
  }, {
    VISIT_TIMES,
    gen_visit_time_key,
    insert_data,
    NULL,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_time_served,
    NULL,
    NULL,
    NULL,
  },
};
/* *INDENT-ON* */

static void
new_modulekey (GKeyData * kdata)
{
  GKeyData key = {
    .data = NULL,
    .data_key = NULL,
    .data_nkey = 0,
    .root = NULL,
    .root_key = NULL,
    .root_nkey = 0,
    .uniq_key = NULL,
    .uniq_nkey = 0,
  };
  *kdata = key;
}

static GParse *
panel_lookup (GModule module)
{
  int i, num_panels = ARRAY_SIZE (paneling);

  for (i = 0; i < num_panels; i++) {
    if (paneling[i].module == module)
      return &paneling[i];
  }
  return NULL;
}

/* allocate memory for ht raw data */
GRawData *
new_grawdata (void)
{
  GRawData *raw_data = xmalloc (sizeof (GRawData));
  memset (raw_data, 0, sizeof *raw_data);

  return raw_data;
}

/* allocate memory for raw data items */
GRawDataItem *
new_grawdata_item (unsigned int size)
{
  GRawDataItem *item = xcalloc (size, sizeof (GRawDataItem));
  return item;
}

/* free memory allocated in raw data */
void
free_raw_data (GRawData * raw_data)
{
#ifdef HAVE_LIBTOKYOCABINET
  int i;
  for (i = 0; i < raw_data->size; i++) {
    if (raw_data->items[i].key != NULL)
      free (raw_data->items[i].key);
    if (raw_data->items[i].value != NULL)
      free (raw_data->items[i].value);
  }
#endif
  free (raw_data->items);
  free (raw_data);
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
  GLog *glog = xmalloc (sizeof (GLog));
  memset (glog, 0, sizeof *glog);

  return glog;
}

GLogItem *
init_log_item (GLog * logger)
{
  GLogItem *glog;
  logger->items = xmalloc (sizeof (GLogItem));
  glog = logger->items;
  memset (glog, 0, sizeof *glog);

  glog->agent = NULL;
  glog->browser = NULL;
  glog->browser_type = NULL;
  glog->continent = NULL;
  glog->country = NULL;
  glog->date = NULL;
  glog->host = NULL;
  glog->keyphrase = NULL;
  glog->method = NULL;
  glog->os = NULL;
  glog->os_type = NULL;
  glog->protocol = NULL;
  glog->ref = NULL;
  glog->req_key = NULL;
  glog->req = NULL;
  glog->status = NULL;
  glog->time = NULL;
  glog->uniq_key = NULL;

  glog->resp_size = 0LL;
  glog->serve_time = 0;

  strncpy (glog->site, "", REF_SITE_LEN);
  glog->site[REF_SITE_LEN - 1] = '\0';

  return glog;
}

static void
free_logger (GLogItem * glog)
{
  if (glog->agent != NULL)
    free (glog->agent);
  if (glog->browser != NULL)
    free (glog->browser);
  if (glog->browser_type != NULL)
    free (glog->browser_type);
  if (glog->continent != NULL)
    free (glog->continent);
  if (glog->country != NULL)
    free (glog->country);
  if (glog->date != NULL)
    free (glog->date);
  if (glog->host != NULL)
    free (glog->host);
  if (glog->keyphrase != NULL)
    free (glog->keyphrase);
  if (glog->method != NULL)
    free (glog->method);
  if (glog->os != NULL)
    free (glog->os);
  if (glog->os_type != NULL)
    free (glog->os_type);
  if (glog->protocol != NULL)
    free (glog->protocol);
  if (glog->ref != NULL)
    free (glog->ref);
  if (glog->req_key != NULL)
    free (glog->req_key);
  if (glog->req != NULL)
    free (glog->req);
  if (glog->status != NULL)
    free (glog->status);
  if (glog->time != NULL)
    free (glog->time);
  if (glog->uniq_key != NULL)
    free (glog->uniq_key);

  free (glog);
}

#define B16210(x) (((x) >= '0' && (x) <= '9') ? ((x) - '0') : (toupper((x)) - 'A' + 10))
static void
decode_hex (char *url, char *out)
{
  char *ptr;
  const char *c;

  for (c = url, ptr = out; *c; c++) {
    if (*c != '%' || !isxdigit (c[1]) || !isxdigit (c[2])) {
      *ptr++ = *c;
    } else {
      *ptr++ = (B16210 (c[1]) * 16) + (B16210 (c[2]));
      c += 2;
    }
  }
  *ptr = 0;
}

static char *
decode_url (char *url)
{
  char *out, *decoded;

  if ((url == NULL) || (*url == '\0'))
    return NULL;

  out = decoded = xstrdup (url);
  decode_hex (url, out);
  if (conf.double_decode)
    decode_hex (decoded, out);
  strip_newlines (out);

  return trim_str (out);
}

/* Process keyphrases from Google search, cache, and translate.
 * Note that the referer hasn't been decoded at the entry point
 * since there could be '&' within the search query. */
static int
extract_keyphrase (char *ref, char **keyphrase)
{
  char *r, *ptr, *pch, *referer;
  int encoded = 0;

  if (!(strstr (ref, "http://www.google.")) &&
      !(strstr (ref, "http://webcache.googleusercontent.com/")) &&
      !(strstr (ref, "http://translate.googleusercontent.com/")))
    return 1;

  /* webcache.googleusercontent */
  if ((r = strstr (ref, "/+&")) != NULL)
    return 1;
  /* webcache.googleusercontent */
  else if ((r = strstr (ref, "/+")) != NULL)
    r += 2;
  /* webcache.googleusercontent */
  else if ((r = strstr (ref, "q=cache:")) != NULL) {
    pch = strchr (r, '+');
    if (pch)
      r += pch - r + 1;
  }
  /* www.google.* or translate.googleusercontent */
  else if ((r = strstr (ref, "&q=")) != NULL ||
           (r = strstr (ref, "?q=")) != NULL)
    r += 3;
  else if ((r = strstr (ref, "%26q%3D")) != NULL ||
           (r = strstr (ref, "%3Fq%3D")) != NULL)
    encoded = 1, r += 7;
  else
    return 1;

  if (!encoded && (ptr = strchr (r, '&')) != NULL)
    *ptr = '\0';
  else if (encoded && (ptr = strstr (r, "%26")) != NULL)
    *ptr = '\0';

  referer = decode_url (r);
  if (referer == NULL || *referer == '\0')
    return 1;

  referer = char_replace (referer, '+', ' ');
  *keyphrase = trim_str (referer);

  return 0;
}

#ifdef HAVE_LIBGEOIP
static int
extract_geolocation (GLogItem * glog, char *continent, char *country)
{
  if (geo_location_data == NULL)
    return 1;

  geoip_get_country (glog->host, country, glog->type_ip);
  geoip_get_continent (glog->host, continent, glog->type_ip);

  return 0;
}
#endif


/* parses a URI and extracts the *host* part from it
 * i.e., //www.example.com/path?googleguy > www.example.com */
static int
extract_referer_site (const char *referer, char *host)
{
  char *url, *begin, *end;
  int len = 0;

  if ((referer == NULL) || (*referer == '\0'))
    return 1;

  url = strdup (referer);
  if ((begin = strstr (url, "//")) == NULL)
    goto clean;

  begin += 2;
  if ((len = strlen (begin)) == 0)
    goto clean;

  if ((end = strchr (begin, '/')) != NULL)
    len = end - begin;

  if (len == 0)
    goto clean;

  if (len >= REF_SITE_LEN)
    len = REF_SITE_LEN - 1;

  memcpy (host, begin, len);
  host[len] = '\0';
  free (url);
  return 0;
clean:
  free (url);

  return 1;
}

/* returns 1 if the request seems to be a static file */
static int
verify_static_content (char *req)
{
  char *nul = req + strlen (req), *ext = NULL;
  int elen = 0, i;

  if (strlen (req) < conf.static_file_max_len)
    return 0;

  for (i = 0; i < conf.static_file_idx; ++i) {
    ext = conf.static_files[i];
    if (ext == NULL || *ext == '\0')
      continue;

    elen = strlen (ext);
    if (!memcmp (nul - elen, ext, elen))
      return 1;
  }
  return 0;
}

static const char *
extract_method (const char *token)
{
  const char *lookfor = NULL;

  if ((lookfor = "OPTIONS", !memcmp (token, lookfor, 7)) ||
      (lookfor = "GET", !memcmp (token, lookfor, 3)) ||
      (lookfor = "HEAD", !memcmp (token, lookfor, 4)) ||
      (lookfor = "POST", !memcmp (token, lookfor, 4)) ||
      (lookfor = "PUT", !memcmp (token, lookfor, 3)) ||
      (lookfor = "DELETE", !memcmp (token, lookfor, 6)) ||
      (lookfor = "TRACE", !memcmp (token, lookfor, 5)) ||
      (lookfor = "CONNECT", !memcmp (token, lookfor, 7)) ||
      (lookfor = "PATCH", !memcmp (token, lookfor, 5)) ||
      (lookfor = "options", !memcmp (token, lookfor, 7)) ||
      (lookfor = "get", !memcmp (token, lookfor, 3)) ||
      (lookfor = "head", !memcmp (token, lookfor, 4)) ||
      (lookfor = "post", !memcmp (token, lookfor, 4)) ||
      (lookfor = "put", !memcmp (token, lookfor, 3)) ||
      (lookfor = "delete", !memcmp (token, lookfor, 6)) ||
      (lookfor = "trace", !memcmp (token, lookfor, 5)) ||
      (lookfor = "connect", !memcmp (token, lookfor, 7)) ||
      (lookfor = "patch", !memcmp (token, lookfor, 5)))
    return lookfor;
  return NULL;
}

static int
invalid_protocol (const char *token)
{
  const char *lookfor = NULL;

  return !((lookfor = "HTTP/1.0", !memcmp (token, lookfor, 8)) ||
           (lookfor = "HTTP/1.1", !memcmp (token, lookfor, 8)));
}

static char *
parse_req (char *line, char **method, char **protocol)
{
  char *req = NULL, *request = NULL, *proto = NULL, *dreq = NULL;
  const char *meth;
  ptrdiff_t rlen;

  meth = extract_method (line);

  /* couldn't find a method, so use the whole request line */
  if (meth == NULL) {
    request = xstrdup (line);
  }
  /* method found, attempt to parse request */
  else {
    req = line + strlen (meth);
    if ((proto = strstr (line, " HTTP/1.0")) == NULL &&
        (proto = strstr (line, " HTTP/1.1")) == NULL) {
      return alloc_string ("-");
    }

    req++;
    if ((rlen = proto - req) <= 0)
      return alloc_string ("-");

    request = xmalloc (rlen + 1);
    strncpy (request, req, rlen);
    request[rlen] = 0;

    if (conf.append_method)
      (*method) = strtoupper (xstrdup (meth));

    if (conf.append_protocol)
      (*protocol) = strtoupper (xstrdup (++proto));
  }

  if ((dreq = decode_url (request)) && dreq != '\0') {
    free (request);
    return dreq;
  }

  return request;
}

static char *
parse_string (char **str, char end, int cnt)
{
  int idx = 0;
  char *pch = *str, *p;
  do {
    if (*pch == end)
      idx++;
    if ((*pch == end && cnt == idx) || *pch == '\0') {
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
parse_specifier (GLogItem * glog, char **str, const char *p)
{
  struct tm tm;
  const char *dfmt = conf.date_format;
  const char *tfmt = conf.time_format;

  char *pch, *sEnd, *bEnd, *tkn = NULL;
  double serve_secs = 0.0;
  uint64_t bandw = 0, serve_time = 0;

  errno = 0;
  memset (&tm, 0, sizeof (tm));

  switch (*p) {
    /* date */
  case 'd':
    if (glog->date)
      return 1;
    /* parse date format including dates containing spaces,
     * i.e., syslog date format (Jul 15 20:10:56) */
    tkn = parse_string (&(*str), p[1], count_matches (dfmt, ' ') + 1);
    if (tkn == NULL)
      return 1;
    if (str_to_time (tkn, dfmt, &tm) != 0) {
      free (tkn);
      return 1;
    }
    glog->date = tkn;
    break;
    /* time */
  case 't':
    if (glog->time)
      return 1;
    tkn = parse_string (&(*str), p[1], 1);
    if (tkn == NULL)
      return 1;
    if (str_to_time (tkn, tfmt, &tm) != 0) {
      free (tkn);
      return 1;
    }
    glog->time = tkn;
    break;
    /* date/time as decimal, i.e., timestamps, ms/us  */
  case 'x':
    if (glog->time && glog->date)
      return 1;
    tkn = parse_string (&(*str), p[1], 1);
    if (tkn == NULL)
      return 1;
    if (str_to_time (tkn, tfmt, &tm) != 0) {
      free (tkn);
      return 1;
    }
    glog->date = xstrdup (tkn);
    glog->time = tkn;
    break;
    /* remote hostname (IP only) */
  case 'h':
    if (glog->host)
      return 1;
    tkn = parse_string (&(*str), p[1], 1);
    if (tkn == NULL)
      return 1;
    if (invalid_ipaddr (tkn, &glog->type_ip)) {
      free (tkn);
      return 1;
    }
    glog->host = tkn;
    break;
    /* request method */
  case 'm':
    if (glog->method)
      return 1;
    tkn = parse_string (&(*str), p[1], 1);
    if (tkn == NULL)
      return 1;
    if (!extract_method (tkn)) {
      free (tkn);
      return 1;
    }
    glog->method = tkn;
    break;
    /* request not including method or protocol */
  case 'U':
    if (glog->req)
      return 1;
    tkn = parse_string (&(*str), p[1], 1);
    if (tkn == NULL || *tkn == '\0')
      return 1;
    if ((glog->req = decode_url (tkn)) == NULL)
      return 1;
    free (tkn);
    break;
    /* request protocol */
  case 'H':
    if (glog->protocol)
      return 1;
    tkn = parse_string (&(*str), p[1], 1);
    if (tkn == NULL)
      return 1;
    if (invalid_protocol (tkn)) {
      free (tkn);
      return 1;
    }
    glog->protocol = tkn;
    break;
    /* request, including method + protocol */
  case 'r':
    if (glog->req)
      return 1;
    tkn = parse_string (&(*str), p[1], 1);
    if (tkn == NULL)
      return 1;
    glog->req = parse_req (tkn, &glog->method, &glog->protocol);
    free (tkn);
    break;
    /* Status Code */
  case 's':
    if (glog->status)
      return 1;
    tkn = parse_string (&(*str), p[1], 1);
    if (tkn == NULL)
      return 1;
    strtol (tkn, &sEnd, 10);
    if (tkn == sEnd || *sEnd != '\0' || errno == ERANGE) {
      free (tkn);
      return 1;
    }
    glog->status = tkn;
    break;
    /* size of response in bytes - excluding HTTP headers */
  case 'b':
    if (glog->resp_size)
      return 1;
    tkn = parse_string (&(*str), p[1], 1);
    if (tkn == NULL)
      return 1;
    bandw = strtol (tkn, &bEnd, 10);
    if (tkn == bEnd || *bEnd != '\0' || errno == ERANGE)
      bandw = 0;
    glog->resp_size = bandw;
    conf.bandwidth = 1;
    free (tkn);
    break;
    /* referrer */
  case 'R':
    if (glog->ref)
      return 1;
    tkn = parse_string (&(*str), p[1], 1);
    if (tkn == NULL)
      tkn = alloc_string ("-");
    if (tkn != NULL && *tkn == '\0') {
      free (tkn);
      tkn = alloc_string ("-");
    }
    if (strcmp (tkn, "-") != 0) {
      extract_keyphrase (tkn, &glog->keyphrase);
      extract_referer_site (tkn, glog->site);
    }
    glog->ref = tkn;
    break;
    /* user agent */
  case 'u':
    if (glog->agent)
      return 1;
    tkn = parse_string (&(*str), p[1], 1);
    if (tkn != NULL && *tkn != '\0') {
      /* Make sure the user agent is decoded (i.e.: CloudFront)
       * and replace all '+' with ' ' (i.e.: w3c) */
      glog->agent = char_replace (decode_url (tkn), '+', ' ');
      free (tkn);
      break;
    } else if (tkn != NULL && *tkn == '\0') {
      free (tkn);
      tkn = alloc_string ("-");
    }
    /* must be null */
    else {
      tkn = alloc_string ("-");
    }
    glog->agent = tkn;
    break;
    /* time taken to serve the request, in milliseconds as a decimal number */
  case 'L':
    /* ignore it if we already have served time */
    if (glog->serve_time)
      break;

    tkn = parse_string (&(*str), p[1], 1);
    if (tkn == NULL)
      return 1;
    serve_secs = strtoull (tkn, &bEnd, 10);

    if (tkn == bEnd || *bEnd != '\0' || errno == ERANGE)
      serve_secs = 0;
    /* convert it to microseconds */
    glog->serve_time = (serve_secs > 0) ? serve_secs * MILS : 0;

    conf.serve_usecs = 1;       /* flag */
    free (tkn);
    break;
    /* time taken to serve the request, in seconds with a milliseconds
     * resolution */
  case 'T':
    /* ignore it if we already have served time */
    if (glog->serve_time)
      break;

    tkn = parse_string (&(*str), p[1], 1);
    if (tkn == NULL)
      return 1;
    if (strchr (tkn, '.') != NULL)
      serve_secs = strtod (tkn, &bEnd);
    else
      serve_secs = strtoull (tkn, &bEnd, 10);

    if (tkn == bEnd || *bEnd != '\0' || errno == ERANGE)
      serve_secs = 0;
    /* convert it to microseconds */
    glog->serve_time = (serve_secs > 0) ? serve_secs * SECS : 0;

    conf.serve_usecs = 1;       /* flag */
    free (tkn);
    break;
    /* time taken to serve the request, in microseconds */
  case 'D':
    /* ignore it if we already have served time */
    if (glog->serve_time)
      break;

    tkn = parse_string (&(*str), p[1], 1);
    if (tkn == NULL)
      return 1;
    serve_time = strtoull (tkn, &bEnd, 10);
    if (tkn == bEnd || *bEnd != '\0' || errno == ERANGE)
      serve_time = 0;
    glog->serve_time = serve_time;

    conf.serve_usecs = 1;       /* flag */
    free (tkn);
    break;
    /* everything else skip it */
  default:
    if ((pch = strchr (*str, p[1])) != NULL)
      *str += pch - *str;
  }

  return 0;
}

static int
parse_format (GLogItem * glog, char *str)
{
  const char *p;
  const char *lfmt = conf.log_format;
  int special = 0;

  if (str == NULL || *str == '\0')
    return 1;

  /* iterate over the log format */
  for (p = lfmt; *p; p++) {
    if (*p == '%') {
      special++;
      continue;
    }
    if (special && *p != '\0') {
      if ((str == NULL) || (*str == '\0'))
        return 0;

      /* attempt to parse format specifiers */
      if (parse_specifier (glog, &str, p) == 1)
        return 1;
      special = 0;
    } else if (special && isspace (p[0])) {
      return 1;
    } else {
      str++;
    }
  }

  return 0;
}

static int
valid_line (char *line)
{
  /* invalid line */
  if ((line == NULL) || (*line == '\0'))
    return 1;
  /* ignore comments */
  if (*line == '#' || *line == '\n')
    return 1;

  return 0;
}

static void
lock_spinner (void)
{
  if (parsing_spinner != NULL && parsing_spinner->state == SPN_RUN)
    pthread_mutex_lock (&parsing_spinner->mutex);
}

static void
unlock_spinner (void)
{
  if (parsing_spinner != NULL && parsing_spinner->state == SPN_RUN)
    pthread_mutex_unlock (&parsing_spinner->mutex);
}

static void
strip_qstring (char *req)
{
  char *qmark;
  if ((qmark = strchr (req, '?')) != NULL) {
    if ((qmark - req) > 0)
      *qmark = '\0';
  }
}

static void
inc_resp_size (GLog * logger, uint64_t resp_size)
{
  logger->resp_size += resp_size;
#ifdef TCB_BTREE
  ht_inc_u64_from_str_key (ht_general_stats, "bandwidth", resp_size);
#endif
}

static void
count_invalid (GLog * logger, int test)
{
  logger->invalid++;
#ifdef TCB_BTREE
  if (!test)
    ht_inc_int_from_str_key (ht_general_stats, "failed_requests", 1);
#else
  (void) test;
#endif
}

static void
count_process (GLog * logger, int test)
{
  lock_spinner ();
  logger->process++;
#ifdef TCB_BTREE
  if (!test)
    ht_inc_int_from_str_key (ht_general_stats, "total_requests", 1);
#else
  (void) test;
#endif
  unlock_spinner ();
}

static int
exclude_ip (GLog * logger, GLogItem * glog, int test)
{
  if (conf.ignore_ip_idx && ip_in_range (glog->host)) {
    logger->exclude_ip++;
#ifdef TCB_BTREE
    if (!test)
      ht_inc_int_from_str_key (ht_general_stats, "exclude_ip", 1);
#else
    (void) test;
#endif
    return 0;
  }
  return 1;
}

static int
exclude_crawler (GLogItem * glog)
{
  return conf.ignore_crawlers && is_crawler (glog->agent) ? 0 : 1;
}

static int
is_static (GLogItem * glog)
{
  return verify_static_content (glog->req);
}

static int
is_404 (GLogItem * glog)
{
  /* is this a 404? */
  if (glog->status && !memcmp (glog->status, "404", 3))
    return 1;
  /* treat 444 as 404? */
  else if (glog->status && !memcmp (glog->status, "444", 3) &&
           conf.code444_as_404)
    return 1;
  return 0;
}

static int
insert_keymap (const char *key, GModule module)
{
  GStorageMetrics *metrics;

  metrics = get_storage_metrics_by_module (module);
  return ht_insert_keymap (metrics->keymap, key);
}

static int
insert_uniqmap (char *uniq_key, GModule module)
{
  GStorageMetrics *metrics;

  metrics = get_storage_metrics_by_module (module);
  return ht_insert_uniqmap (metrics->uniqmap, uniq_key);
}

static void
insert_root (int root_nkey, const char *root, GModule module)
{
  GStorageMetrics *metrics;
  metrics = get_storage_metrics_by_module (module);

  ht_insert_str_from_int_key (metrics->rootmap, root_nkey, root);
}

static void
insert_data (int nkey, const char *data, GModule module)
{
  GStorageMetrics *metrics;
  metrics = get_storage_metrics_by_module (module);

  ht_insert_str_from_int_key (metrics->datamap, nkey, data);
}

static void
insert_hit (int data_nkey, int uniq_nkey, int root_nkey, GModule module)
{
  GStorageMetrics *metrics;
  metrics = get_storage_metrics_by_module (module);

  ht_insert_hit (metrics->hits, data_nkey, uniq_nkey, root_nkey);
}

static void
insert_visitor (int uniq_nkey, GModule module)
{
  GStorageMetrics *metrics;
  metrics = get_storage_metrics_by_module (module);

  ht_inc_int_from_int_key (metrics->visitors, uniq_nkey, 1);
}

static void
insert_bw (int data_nkey, uint64_t size, GModule module)
{
  GStorageMetrics *metrics;
  metrics = get_storage_metrics_by_module (module);

  ht_inc_u64_from_int_key (metrics->bw, data_nkey, size);
}

static void
insert_time_served (int data_nkey, uint64_t ts, GModule module)
{
  GStorageMetrics *metrics;
  metrics = get_storage_metrics_by_module (module);

  ht_inc_u64_from_int_key (metrics->time_served, data_nkey, ts);
}

static void
insert_method (int nkey, const char *data, GModule module)
{
  GStorageMetrics *metrics;
  metrics = get_storage_metrics_by_module (module);

  ht_insert_str_from_int_key (metrics->methods, nkey, data ? data : "---");
}

static void
insert_protocol (int nkey, const char *data, GModule module)
{
  GStorageMetrics *metrics;
  metrics = get_storage_metrics_by_module (module);

  ht_insert_str_from_int_key (metrics->protocols, nkey, data ? data : "---");
}

static void
insert_agent (int data_nkey, int agent_nkey, GModule module)
{
  GStorageMetrics *metrics;
  metrics = get_storage_metrics_by_module (module);

  ht_insert_host_agent (metrics->agents, data_nkey, agent_nkey);
}

/* The following generates a unique key to identity unique visitors.
 * The key is made out of the IP, date, and user agent.
 * Note that for readability, doing a simple snprintf/sprintf should
 * suffice, however, memcpy is the fastest solution */
static char *
get_uniq_visitor_key (GLogItem * glog)
{
  char *ua, *key;
  size_t s1, s2, s3;

  ua = deblank (xstrdup (glog->agent));

  s1 = strlen (glog->host);
  s2 = strlen (glog->date);
  s3 = strlen (ua);

  /* includes terminating null */
  key = xmalloc (s1 + s2 + s3 + 3);

  memcpy (key, glog->host, s1);

  key[s1] = '|';
  memcpy (key + s1 + 1, glog->date, s2 + 1);

  key[s1 + s2 + 1] = '|';
  memcpy (key + s1 + s2 + 2, ua, s3 + 1);

  free (ua);
  return key;
}

static char *
gen_unique_req_key (GLogItem * glog)
{
  char *key;
  size_t s1 = 0, s2 = 0, s3 = 0;

  /* nothing to do */
  if (!conf.append_method && !conf.append_protocol)
    return xstrdup (glog->req);

  /* still nothing to do */
  if (!glog->method && !glog->protocol)
    return xstrdup (glog->req);

  s1 = strlen (glog->req);
  if (glog->method)
    s2 = strlen (glog->method);
  if (glog->protocol)
    s3 = strlen (glog->protocol);

  /* includes terminating null */
  key = xmalloc (s1 + s2 + s3 + 3);
  /* append request */
  memcpy (key, glog->req, s1);

  if (glog->method) {
    key[s1] = '|';
    memcpy (key + s1 + 1, glog->method, s2 + 1);
  }

  if (glog->protocol) {
    key[s1 + s2 + 1] = '|';
    memcpy (key + s1 + s2 + 2, glog->protocol, s3 + 1);
  }

  return key;
}

static void
get_kdata (GKeyData * kdata, char *data_key, char *data)
{
  /* inserted in keymap */
  kdata->data_key = data_key;
  /* inserted in datamap */
  kdata->data = data;
}

static int
gen_visitor_key (GKeyData * kdata, GLogItem * glog)
{
  if (!glog->date)
    return 1;

  get_kdata (kdata, glog->date, glog->date);

  return 0;
}

static int
gen_req_key (GKeyData * kdata, GLogItem * glog)
{
  glog->req_key = gen_unique_req_key (glog);
  get_kdata (kdata, glog->req_key, glog->req);

  return 0;
}

static int
gen_request_key (GKeyData * kdata, GLogItem * glog)
{
  if (!glog->req || glog->is_404 || glog->is_static)
    return 1;

  return gen_req_key (kdata, glog);
}

static int
gen_404_key (GKeyData * kdata, GLogItem * glog)
{
  if (glog->req && glog->is_404)
    return gen_req_key (kdata, glog);
  return 1;
}

static int
gen_static_request_key (GKeyData * kdata, GLogItem * glog)
{
  if (glog->req && glog->is_static)
    return gen_req_key (kdata, glog);
  return 1;
}

static int
gen_host_key (GKeyData * kdata, GLogItem * glog)
{
  if (!glog->host)
    return 1;

  get_kdata (kdata, glog->host, glog->host);

  return 0;
}

static int
gen_browser_key (GKeyData * kdata, GLogItem * glog)
{
  char *agent = NULL;
  char browser_type[BROWSER_TYPE_LEN] = "";

  if (glog->agent == NULL || *glog->agent == '\0')
    return 1;

  agent = xstrdup (glog->agent);
  glog->browser = verify_browser (agent, browser_type);
  glog->browser_type = xstrdup (browser_type);

  /* e.g., Firefox 11.12 */
  kdata->data = glog->browser;
  kdata->data_key = glog->browser;

  /* Firefox */
  kdata->root = glog->browser_type;
  kdata->root_key = glog->browser_type;

  free (agent);

  return 0;
}

static int
gen_os_key (GKeyData * kdata, GLogItem * glog)
{
  char *agent = NULL;
  char os_type[OPESYS_TYPE_LEN] = "";

  if (glog->agent == NULL || *glog->agent == '\0')
    return 1;

  agent = xstrdup (glog->agent);
  glog->os = verify_os (agent, os_type);
  glog->os_type = xstrdup (os_type);

  /* e.g., Linux,Ubuntu 10.12 */
  kdata->data = glog->os;
  kdata->data_key = glog->os;

  /* Linux */
  kdata->root = glog->os_type;
  kdata->root_key = glog->os_type;

  free (agent);

  return 0;
}

static int
gen_referer_key (GKeyData * kdata, GLogItem * glog)
{
  if (!glog->ref)
    return 1;

  get_kdata (kdata, glog->ref, glog->ref);

  return 0;
}

static int
gen_ref_site_key (GKeyData * kdata, GLogItem * glog)
{
  if (glog->site[0] == '\0')
    return 1;

  get_kdata (kdata, glog->site, glog->site);

  return 0;
}

static int
gen_keyphrase_key (GKeyData * kdata, GLogItem * glog)
{
  if (!glog->keyphrase)
    return 1;

  get_kdata (kdata, glog->keyphrase, glog->keyphrase);

  return 0;
}

#ifdef HAVE_LIBGEOIP
static int
gen_geolocation_key (GKeyData * kdata, GLogItem * glog)
{
  char continent[CONTINENT_LEN] = "";
  char country[COUNTRY_LEN] = "";

  if (extract_geolocation (glog, continent, country) == 1)
    return 1;

  if (country[0] != '\0')
    glog->country = xstrdup (country);

  if (continent[0] != '\0')
    glog->continent = xstrdup (continent);

  kdata->data_key = glog->country;
  kdata->data = glog->country;

  kdata->root = glog->continent;
  kdata->root_key = glog->continent;

  return 0;
}
#endif

static int
gen_status_code_key (GKeyData * kdata, GLogItem * glog)
{
  const char *status = NULL, *type = NULL;

  if (!glog->status)
    return 1;

  type = verify_status_code_type (glog->status);
  status = verify_status_code (glog->status);

  kdata->data = (char *) status;
  kdata->data_key = (char *) status;

  kdata->root = (char *) type;
  kdata->root_key = (char *) type;

  return 0;
}

static int
gen_visit_time_key (GKeyData * kdata, GLogItem * glog)
{
  char *hmark = NULL;
  char hour[HOUR_LEN] = "";     /* %H */
  if (!glog->time)
    return 1;

  /* if not a timestamp, then it must be a string containing the hour.
   * this is faster than actual date conversion */
  if (!has_timestamp (conf.time_format) && (hmark = strchr (glog->time, ':'))) {
    if ((hmark - glog->time) > 0)
      *hmark = '\0';
    get_kdata (kdata, glog->time, glog->time);
    return 0;
  }

  /* otherwise it attempts to convert the date given a time format,
   * though this is slower */
  memset (hour, 0, sizeof *hour);
  if (convert_date (hour, glog->time, conf.time_format, "%H", HOUR_LEN) != 0)
    return 1;

  if (hour == NULL || hour == '\0')
    return 1;

  free (glog->time);
  glog->time = xstrdup (hour);
  get_kdata (kdata, glog->time, glog->time);

  return 0;
}

static int
include_uniq (GLogItem * glog)
{
  int u = conf.client_err_to_unique_count;

  if (!glog->status || glog->status[0] != '4' || (u && glog->status[0] == '4'))
    return 1;
  return 0;
}

static void
set_datamap (GLogItem * glog, GKeyData * kdata, const GParse * parse)
{
  GModule module;
  module = parse->module;

  /* insert data */
  parse->datamap (kdata->data_nkey, kdata->data, module);

  /* insert root */
  if (parse->rootmap)
    parse->rootmap (kdata->root_nkey, kdata->root, module);
  /* insert hits */
  if (parse->hits)
    parse->hits (kdata->data_nkey, kdata->uniq_nkey, kdata->root_nkey, module);
  /* insert visitors */
  if (parse->visitor && kdata->uniq_nkey != 0)
    parse->visitor (kdata->data_nkey, module);
  /* insert bandwidth */
  if (parse->bw)
    parse->bw (kdata->data_nkey, glog->resp_size, module);
  /* insert averages time served */
  if (parse->avgts)
    parse->avgts (kdata->data_nkey, glog->serve_time, module);
  /* insert method */
  if (parse->method && conf.append_method)
    parse->method (kdata->data_nkey, glog->method, module);
  /* insert protocol */
  if (parse->protocol && conf.append_protocol)
    parse->protocol (kdata->data_nkey, glog->protocol, module);
  /* insert agent */
  if (parse->agent && conf.list_agents)
    parse->agent (kdata->data_nkey, glog->agent_nkey, module);
}

static void
map_log (GLogItem * glog, const GParse * parse, GModule module)
{
  GKeyData kdata;
  char *uniq_key = NULL;

  new_modulekey (&kdata);
  if (parse->key_data (&kdata, glog) == 1)
    return;

  /* each module requires a data key/value */
  if (parse->datamap && kdata.data_key)
    kdata.data_nkey = insert_keymap (kdata.data_key, module);

  /* each module contains a uniq visitor key/value */
  if (parse->visitor && glog->uniq_key && include_uniq (glog)) {
    uniq_key = ints_to_str (glog->uniq_nkey, kdata.data_nkey);
    /* unique key already exists? */
    if ((kdata.uniq_nkey = insert_uniqmap (uniq_key, module)) == 0)
      free (uniq_key);
  }

  /* root keys are optional */
  if (parse->rootmap && kdata.root_key)
    kdata.root_nkey = insert_keymap (kdata.root_key, module);

  /* each module requires a root key/value */
  if (parse->datamap && kdata.data_key)
    set_datamap (glog, &kdata, parse);
}

static void
process_log (GLogItem * glog)
{
  GModule module;

  /* insert one unique visitor key per request to avoid the
   * overhead of storing one key per module */
  glog->uniq_nkey = ht_insert_unique_key (glog->uniq_key);

  /* store unique user agents and retrieve its numeric key.  it maintains two
   * maps, one for key -> value, and another map for value -> key*/
  if (conf.list_agents) {
    glog->agent_nkey = ht_insert_agent_key (glog->agent);
    ht_insert_agent_val (glog->agent_nkey, glog->agent);
  }

  for (module = 0; module < TOTAL_MODULES; module++) {
    const GParse *parse = panel_lookup (module);
    if (!parse)
      continue;
    if (ignore_panel (module))
      continue;
    map_log (glog, parse, module);
  }
}

/* process a line from the log and store it accordingly */
static int
pre_process_log (GLog * logger, char *line, int test)
{
  GLogItem *glog;

  if (valid_line (line)) {
    count_invalid (logger, test);
    return 0;
  }

  count_process (logger, test);
  glog = init_log_item (logger);
  /* parse a line of log, and fill structure with appropriate values */
  if (parse_format (glog, line)) {
    count_invalid (logger, test);
    goto cleanup;
  }

  /* must have the following fields */
  if (glog->host == NULL || glog->date == NULL || glog->req == NULL) {
    count_invalid (logger, test);
    goto cleanup;
  }
  /* agent will be null in cases where %u is not specified */
  if (glog->agent == NULL)
    glog->agent = alloc_string ("-");

  /* testing log only */
  if (test)
    goto cleanup;

  /* ignore host or crawlers */
  if (exclude_ip (logger, glog, test) == 0)
    goto cleanup;
  if (exclude_crawler (glog) == 0)
    goto cleanup;
  if (ignore_referer (glog->site))
    goto cleanup;

  /* check if we need to remove the request's query string */
  if (conf.ignore_qstr)
    strip_qstring (glog->req);

  if (is_404 (glog))
    glog->is_404 = 1;
  else if (is_static (glog))
    glog->is_static = 1;

  glog->uniq_key = get_uniq_visitor_key (glog);

  inc_resp_size (logger, glog->resp_size);
  process_log (glog);

cleanup:
  free_logger (glog);

  return 0;
}

static int
read_log (GLog ** logger, int n)
{
  FILE *fp = NULL;
  char line[LINE_BUFFER] = "";
  int i = 0, test = -1 == n ? 0 : 1;

  /* no log file, assume STDIN */
  if (conf.ifile == NULL) {
    fp = stdin;
    (*logger)->piping = 1;
  }

  /* make sure we can open the log (if not reading from stdin) */
  if (!(*logger)->piping && (fp = fopen (conf.ifile, "r")) == NULL)
    FATAL ("Unable to open the specified log file. %s", strerror (errno));

  while (fgets (line, LINE_BUFFER, fp) != NULL) {
    if (n >= 0 && i++ == n)
      break;

    /* start processing log line */
    if (pre_process_log ((*logger), line, test)) {
      if (!(*logger)->piping)
        fclose (fp);
      return 1;
    }
  }

  /* definitely not portable! */
  if ((*logger)->piping)
    freopen ("/dev/tty", "r", stdin);

  /* close log file if not a pipe */
  if (!(*logger)->piping)
    fclose (fp);

  return 0;
}

void
verify_formats (void)
{
  if (conf.time_format == NULL || *conf.time_format == '\0')
    FATAL ("No time format was found on your conf file.");

  if (conf.date_format == NULL || *conf.date_format == '\0')
    FATAL ("No date format was found on your conf file.");

  if (conf.log_format == NULL || *conf.log_format == '\0')
    FATAL ("No log format was found on your conf file.");
}

/* entry point to parse the log line by line */
int
parse_log (GLog ** logger, char *tail, int n)
{
  int test = -1 == n ? 0 : 1;

  verify_formats ();

  /* process tail data and return */
  if (tail != NULL) {
    if (pre_process_log ((*logger), tail, test))
      return 1;
    return 0;
  }

  return read_log (logger, n);
}

/* make sure we have valid hits */
int
test_format (GLog * logger)
{
  if (parse_log (&logger, NULL, 20))
    FATAL ("Error while processing file");

  if ((logger->process == 0) || (logger->process == logger->invalid))
    return 1;
  return 0;
}
