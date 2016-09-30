/**
 * parser.c -- web log parsing
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
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabdb.h"
#include "tcbtdb.h"
#else
#include "gkhash.h"
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
static int gen_visitor_key (GKeyData * kdata, GLogItem * logitem);
static int gen_404_key (GKeyData * kdata, GLogItem * logitem);
static int gen_browser_key (GKeyData * kdata, GLogItem * logitem);
static int gen_host_key (GKeyData * kdata, GLogItem * logitem);
static int gen_keyphrase_key (GKeyData * kdata, GLogItem * logitem);
static int gen_os_key (GKeyData * kdata, GLogItem * logitem);
static int gen_vhost_key (GKeyData * kdata, GLogItem * logitem);
static int gen_referer_key (GKeyData * kdata, GLogItem * logitem);
static int gen_ref_site_key (GKeyData * kdata, GLogItem * logitem);
static int gen_request_key (GKeyData * kdata, GLogItem * logitem);
static int gen_static_request_key (GKeyData * kdata, GLogItem * logitem);
static int gen_status_code_key (GKeyData * kdata, GLogItem * logitem);
static int gen_visit_time_key (GKeyData * kdata, GLogItem * logitem);
#ifdef HAVE_LIBGEOIP
static int gen_geolocation_key (GKeyData * kdata, GLogItem * logitem);
#endif

/* insertion routines */
static void insert_data (int data_nkey, const char *data, GModule module);
static void insert_rootmap (int root_nkey, const char *root, GModule module);

/* insertion metric routines */
static void insert_hit (int data_nkey, GModule module);
static void insert_visitor (int uniq_nkey, GModule module);
static void insert_bw (int data_nkey, uint64_t size, GModule module);
static void insert_cumts (int data_nkey, uint64_t ts, GModule module);
static void insert_maxts (int data_nkey, uint64_t ts, GModule module);
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
    insert_cumts,
    insert_maxts,
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
    insert_cumts,
    insert_maxts,
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
    insert_cumts,
    insert_maxts,
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
    insert_cumts,
    insert_maxts,
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
    insert_cumts,
    insert_maxts,
    NULL,
    NULL,
    insert_agent,
  }, {
    OS,
    gen_os_key,
    insert_data,
    insert_rootmap,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_cumts,
    insert_maxts,
    insert_method,
    insert_protocol,
    NULL,
  }, {
    BROWSERS,
    gen_browser_key,
    insert_data,
    insert_rootmap,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_cumts,
    insert_maxts,
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
    insert_cumts,
    insert_maxts,
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
    insert_cumts,
    insert_maxts,
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
    insert_cumts,
    insert_maxts,
    NULL,
    NULL,
    NULL,
  },
#ifdef HAVE_LIBGEOIP
  {
    GEO_LOCATION,
    gen_geolocation_key,
    insert_data,
    insert_rootmap,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_cumts,
    insert_maxts,
    NULL,
    NULL,
    NULL,
  },
#endif
  {
    STATUS_CODES,
    gen_status_code_key,
    insert_data,
    insert_rootmap,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_cumts,
    insert_maxts,
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
    insert_cumts,
    insert_maxts,
    NULL,
    NULL,
    NULL,
  }, {
    VIRTUAL_HOSTS,
    gen_vhost_key,
    insert_data,
    NULL,
    insert_hit,
    insert_visitor,
    insert_bw,
    insert_cumts,
    insert_maxts,
    NULL,
    NULL,
    NULL,
  },
};
/* *INDENT-ON* */

/* Initialize a new GKeyData instance */
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

/* Get a panel from the GParse structure given a module.
 *
 * On error, or if not found, NULL is returned.
 * On success, the panel value is returned. */
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

/* Allocate memory for a new GRawData instance.
 *
 * On success, the newly allocated GRawData is returned . */
GRawData *
new_grawdata (void)
{
  GRawData *raw_data = xmalloc (sizeof (*raw_data));
  memset (raw_data, 0, sizeof *raw_data);

  return raw_data;
}

/* Allocate memory for a new GRawDataItem instance.
 *
 * On success, the newly allocated GRawDataItem is returned . */
GRawDataItem *
new_grawdata_item (unsigned int size)
{
  GRawDataItem *item = xcalloc (size, sizeof (*item));
  return item;
}

#ifdef HAVE_LIBTOKYOCABINET
/* This is due to an additional allocation on tokyo's value
 * retrieval :( on tcadbget()  */
static void
free_raw_data_str_value (GRawData * raw_data)
{
  int i = 0;
  char *str = NULL;

  for (i = 0; i < raw_data->size; ++i) {
    str = raw_data->items[i].value.svalue;
    if (str)
      free (str);
  }
}
#endif

/* Free memory allocated for a GRawData and GRawDataItem instance. */
void
free_raw_data (GRawData * raw_data)
{
#ifdef HAVE_LIBTOKYOCABINET
  if (raw_data->type == STRING)
    free_raw_data_str_value (raw_data);
#endif
  free (raw_data->items);
  free (raw_data);
}

/* Reset an instance of GLog structure. */
void
reset_struct (GLog * glog)
{
  glog->invalid = 0;
  glog->processed = 0;
  glog->resp_size = 0LL;
  glog->valid = 0;
}

/* Allocate memory for a new GLog instance.
 *
 * On success, the newly allocated GLog is returned . */
GLog *
init_log (void)
{
  GLog *logitem = xmalloc (sizeof (GLog));
  memset (logitem, 0, sizeof *logitem);

  return logitem;
}

/* Initialize a new GLogItem instance.
 *
 * On success, the new GLogItem instance is returned. */
GLogItem *
init_log_item (GLog * glog)
{
  GLogItem *logitem;
  glog->items = xmalloc (sizeof (GLogItem));
  logitem = glog->items;
  memset (logitem, 0, sizeof *logitem);

  logitem->agent = NULL;
  logitem->browser = NULL;
  logitem->browser_type = NULL;
  logitem->continent = NULL;
  logitem->country = NULL;
  logitem->date = NULL;
  logitem->host = NULL;
  logitem->keyphrase = NULL;
  logitem->method = NULL;
  logitem->os = NULL;
  logitem->os_type = NULL;
  logitem->protocol = NULL;
  logitem->qstr = NULL;
  logitem->ref = NULL;
  logitem->req_key = NULL;
  logitem->req = NULL;
  logitem->status = NULL;
  logitem->time = NULL;
  logitem->uniq_key = NULL;
  logitem->vhost = NULL;

  logitem->resp_size = 0LL;
  logitem->serve_time = 0;

  memset (logitem->site, 0, sizeof (logitem->site));

  return logitem;
}

/* Free all members of a GLogItem */
static void
free_glog (GLogItem * logitem)
{
  if (logitem->agent != NULL)
    free (logitem->agent);
  if (logitem->browser != NULL)
    free (logitem->browser);
  if (logitem->browser_type != NULL)
    free (logitem->browser_type);
  if (logitem->continent != NULL)
    free (logitem->continent);
  if (logitem->country != NULL)
    free (logitem->country);
  if (logitem->date != NULL)
    free (logitem->date);
  if (logitem->host != NULL)
    free (logitem->host);
  if (logitem->keyphrase != NULL)
    free (logitem->keyphrase);
  if (logitem->method != NULL)
    free (logitem->method);
  if (logitem->os != NULL)
    free (logitem->os);
  if (logitem->os_type != NULL)
    free (logitem->os_type);
  if (logitem->protocol != NULL)
    free (logitem->protocol);
  if (logitem->qstr != NULL)
    free (logitem->qstr);
  if (logitem->ref != NULL)
    free (logitem->ref);
  if (logitem->req_key != NULL)
    free (logitem->req_key);
  if (logitem->req != NULL)
    free (logitem->req);
  if (logitem->status != NULL)
    free (logitem->status);
  if (logitem->time != NULL)
    free (logitem->time);
  if (logitem->uniq_key != NULL)
    free (logitem->uniq_key);
  if (logitem->vhost != NULL)
    free (logitem->vhost);

  free (logitem);
}

/* Decodes the given URL-encoded string.
 *
 * On success, the decoded string is assigned to the output buffer. */
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

/* Entry point to decode the given URL-encoded string.
 *
 * On success, the decoded trimmed string is assigned to the output
 * buffer. */
static char *
decode_url (char *url)
{
  char *out, *decoded;

  if ((url == NULL) || (*url == '\0'))
    return NULL;

  out = decoded = xstrdup (url);
  decode_hex (url, out);
  /* double encoded URL? */
  if (conf.double_decode)
    decode_hex (decoded, out);
  strip_newlines (out);

  return trim_str (out);
}

/* Process keyphrases from Google search, cache, and translate.
 * Note that the referer hasn't been decoded at the entry point
 * since there could be '&' within the search query.
 *
 * On error, 1 is returned.
 * On success, the extracted keyphrase is assigned and 0 is returned. */
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
/* Extract geolocation for the given host.
 *
 * On error, 1 is returned.
 * On success, the extracted continent and country are set and 0 is
 * returned. */
static int
extract_geolocation (GLogItem * logitem, char *continent, char *country)
{
  if (geo_location_data == NULL)
    return 1;

  geoip_get_country (logitem->host, country, logitem->type_ip);
  geoip_get_continent (logitem->host, continent, logitem->type_ip);

  return 0;
}
#endif


/* Parse a URI and extracts the *host* part from it
 * i.e., //www.example.com/path?googleguy > www.example.com
 *
 * On error, 1 is returned.
 * On success, the extracted referer is set and 0 is returned. */
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
    len = REF_SITE_LEN;

  memcpy (host, begin, len);
  host[len] = '\0';
  free (url);
  return 0;
clean:
  free (url);

  return 1;
}

/* Determine if the given request is static (e.g., jpg, css, js, etc).
 *
 * On error, or if not static, 0 is returned.
 * On success, the 1 is returned. */
static int
verify_static_content (char *req)
{
  char *nul = req + strlen (req);
  const char *ext = NULL, *pch = NULL;
  int elen = 0, i;

  if (strlen (req) < conf.static_file_max_len)
    return 0;

  for (i = 0; i < conf.static_file_idx; ++i) {
    ext = conf.static_files[i];
    if (ext == NULL || *ext == '\0')
      continue;

    elen = strlen (ext);
    if (conf.all_static_files && (pch = strchr (req, '?')) != NULL &&
        pch - req > elen) {
      pch -= elen;
      if (0 == strncasecmp (ext, pch, elen))
        return 1;
      continue;
    }

    if (!strncasecmp (nul - elen, ext, elen))
      return 1;
  }

  return 0;
}

/* Extract the HTTP method.
 *
 * On error, or if not found, NULL is returned.
 * On success, the HTTP method is returned. */
static const char *
extract_method (const char *token)
{
  const char *lookfor = NULL;

  if ((lookfor = "OPTIONS", !strncmp (token, lookfor, 7)) ||
      (lookfor = "GET", !strncmp (token, lookfor, 3)) ||
      (lookfor = "HEAD", !strncmp (token, lookfor, 4)) ||
      (lookfor = "POST", !strncmp (token, lookfor, 4)) ||
      (lookfor = "PUT", !strncmp (token, lookfor, 3)) ||
      (lookfor = "DELETE", !strncmp (token, lookfor, 6)) ||
      (lookfor = "TRACE", !strncmp (token, lookfor, 5)) ||
      (lookfor = "CONNECT", !strncmp (token, lookfor, 7)) ||
      (lookfor = "PATCH", !strncmp (token, lookfor, 5)) ||
      (lookfor = "options", !strncmp (token, lookfor, 7)) ||
      (lookfor = "get", !strncmp (token, lookfor, 3)) ||
      (lookfor = "head", !strncmp (token, lookfor, 4)) ||
      (lookfor = "post", !strncmp (token, lookfor, 4)) ||
      (lookfor = "put", !strncmp (token, lookfor, 3)) ||
      (lookfor = "delete", !strncmp (token, lookfor, 6)) ||
      (lookfor = "trace", !strncmp (token, lookfor, 5)) ||
      (lookfor = "connect", !strncmp (token, lookfor, 7)) ||
      (lookfor = "patch", !strncmp (token, lookfor, 5)) ||
      /* WebDAV */
      (lookfor = "PROPFIND", !strncmp (token, lookfor, 8)) ||
      (lookfor = "PROPPATCH", !strncmp (token, lookfor, 9)) ||
      (lookfor = "MKCOL", !strncmp (token, lookfor, 5)) ||
      (lookfor = "COPY", !strncmp (token, lookfor, 4)) ||
      (lookfor = "MOVE", !strncmp (token, lookfor, 4)) ||
      (lookfor = "LOCK", !strncmp (token, lookfor, 4)) ||
      (lookfor = "UNLOCK", !strncmp (token, lookfor, 6)) ||
      (lookfor = "VERSION-CONTROL", !strncmp (token, lookfor, 15)) ||
      (lookfor = "REPORT", !strncmp (token, lookfor, 6)) ||
      (lookfor = "CHECKOUT", !strncmp (token, lookfor, 8)) ||
      (lookfor = "CHECKIN", !strncmp (token, lookfor, 7)) ||
      (lookfor = "UNCHECKOUT", !strncmp (token, lookfor, 10)) ||
      (lookfor = "MKWORKSPACE", !strncmp (token, lookfor, 11)) ||
      (lookfor = "UPDATE", !strncmp (token, lookfor, 6)) ||
      (lookfor = "LABEL", !strncmp (token, lookfor, 5)) ||
      (lookfor = "MERGE", !strncmp (token, lookfor, 5)) ||
      (lookfor = "BASELINE-CONTROL", !strncmp (token, lookfor, 16)) ||
      (lookfor = "MKACTIVITY", !strncmp (token, lookfor, 10)) ||
      (lookfor = "ORDERPATCH", !strncmp (token, lookfor, 10)) ||
      (lookfor = "propfind", !strncmp (token, lookfor, 8)) ||
      (lookfor = "propwatch", !strncmp (token, lookfor, 9)) ||
      (lookfor = "mkcol", !strncmp (token, lookfor, 5)) ||
      (lookfor = "copy", !strncmp (token, lookfor, 4)) ||
      (lookfor = "move", !strncmp (token, lookfor, 4)) ||
      (lookfor = "lock", !strncmp (token, lookfor, 4)) ||
      (lookfor = "unlock", !strncmp (token, lookfor, 6)) ||
      (lookfor = "version-control", !strncmp (token, lookfor, 15)) ||
      (lookfor = "report", !strncmp (token, lookfor, 6)) ||
      (lookfor = "checkout", !strncmp (token, lookfor, 8)) ||
      (lookfor = "checkin", !strncmp (token, lookfor, 7)) ||
      (lookfor = "uncheckout", !strncmp (token, lookfor, 10)) ||
      (lookfor = "mkworkspace", !strncmp (token, lookfor, 11)) ||
      (lookfor = "update", !strncmp (token, lookfor, 6)) ||
      (lookfor = "label", !strncmp (token, lookfor, 5)) ||
      (lookfor = "merge", !strncmp (token, lookfor, 5)) ||
      (lookfor = "baseline-control", !strncmp (token, lookfor, 16)) ||
      (lookfor = "mkactivity", !strncmp (token, lookfor, 10)) ||
      (lookfor = "orderpatch", !strncmp (token, lookfor, 10)))
    return lookfor;
  return NULL;
}

/* Determine if time-served data was stored on-disk. */
static void
contains_usecs (void)
{
  if (conf.serve_usecs)
    return;

#ifdef TCB_BTREE
  ht_insert_genstats ("serve_usecs", 1);
#endif
  conf.serve_usecs = 1; /* flag */
}

/* Determine if the given token is a valid HTTP protocol.
 *
 * If not valid, 1 is returned.
 * If valid, 0 is returned. */
static int
invalid_protocol (const char *token)
{
  const char *lookfor;

  return !((lookfor = "HTTP/1.0", !strncmp (token, lookfor, 8)) ||
           (lookfor = "HTTP/1.1", !strncmp (token, lookfor, 8)) ||
           (lookfor = "HTTP/2", !strncmp (token, lookfor, 6)));
}

/* Parse a request containing the method and protocol.
 *
 * On error, or unable to parse, NULL is returned.
 * On success, the HTTP request is returned and the method and
 * protocol are assigned to the corresponding buffers. */
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
        (proto = strstr (line, " HTTP/1.1")) == NULL &&
        (proto = strstr (line, " HTTP/2")) == NULL) {
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

/* Extract the next delimiter given a log format and copy the
 * delimiter(s) to the destination buffer.
 * Note that it's possible to store up to two delimiters.
 *
 * On error, the function returns.
 * On success, the delimiter(s) are stored in the dest buffer and the
 * number of extra delimiters is returned. */
static int
get_delim (char *dest, const char *p)
{
  /* done, nothing to do */
  if (p[0] == '\0' || p[1] == '\0') {
    dest[0] = '\0';
    return 0;
  }
  /* add the first delim */
  dest[0] = *(p + 1);
  /* check if there's another possible delim */
  if (p[2] == '|' && p[3] != '\0') {
    dest[1] = *(p + 3);
    return 1;
  }
  return 0;
}

/* Extract and malloc a token given the parsed rule.
 *
 * On success, the malloc'd token is returned. */
static char *
parsed_string (const char *pch, char **str)
{
  char *p;
  size_t len = (pch - *str + 1);

  p = xmalloc (len);
  memcpy (p, *str, (len - 1));
  p[len - 1] = '\0';
  *str += len - 1;

  return trim_str (p);
}

/* Find and extract a token given a log format rule.
 *
 * On error, or unable to parse it, NULL is returned.
 * On success, the malloc'd token is returned. */
static char *
parse_string (char **str, const char *delims, int cnt)
{
  int idx = 0;
  char *pch = *str, *p = NULL;
  char end;

  if ((*delims != 0x0) && (p = strpbrk (*str, delims)) == NULL)
    return NULL;

  end = !*delims ? 0x0 : *p;
  do {
    /* match number of delims */
    if (*pch == end)
      idx++;
    /* delim found, parse string then */
    if ((*pch == end && cnt == idx) || *pch == '\0')
      return parsed_string (pch, str);
    /* advance to the first unescaped delim */
    if (*pch == '\\')
      pch++;
  } while (*pch++);

  return NULL;
}

/* Move forward through the log string until a non-space (!isspace)
 * char is found. */
static void
find_alpha (char **str)
{
  char *s = *str;
  while (*s) {
    if (isspace (*s))
      s++;
    else
      break;
  }
  *str += s - *str;
}

#pragma GCC diagnostic ignored "-Wformat-nonliteral"
/* Format the the broken-down time tm to a numeric date format.
 *
 * On error, or unable to format the given tm, 1 is returned.
 * On success, a malloc'd format is returned. */
static int
set_date (char **fdate, struct tm tm)
{
  char buf[DATE_LEN] = "";      /* Ymd */

  memset (buf, 0, sizeof (buf));
  if (strftime (buf, DATE_LEN, conf.date_num_format, &tm) <= 0)
    return 1;
  *fdate = xstrdup (buf);

  return 0;
}

#pragma GCC diagnostic warning "-Wformat-nonliteral"

/* Parse the log string given log format rule.
 *
 * On error, or unable to parse it, 1 is returned.
 * On success, the malloc'd token is assigned to a GLogItem member. */
static int
parse_specifier (GLogItem * logitem, char **str, const char *p, const char *end)
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
    if (logitem->date)
      return 1;
    /* parse date format including dates containing spaces,
     * i.e., syslog date format (Jul 15 20:10:56) */
    tkn = parse_string (&(*str), end, count_matches (dfmt, ' ') + 1);
    if (tkn == NULL)
      return 1;
    if (str_to_time (tkn, dfmt, &tm) != 0 || set_date (&logitem->date, tm) != 0) {
      free (tkn);
      return 1;
    }
    free (tkn);
    break;
    /* time */
  case 't':
    if (logitem->time)
      return 1;
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL)
      return 1;
    if (str_to_time (tkn, tfmt, &tm) != 0) {
      free (tkn);
      return 1;
    }
    logitem->time = tkn;
    break;
    /* date/time as decimal, i.e., timestamps, ms/us  */
  case 'x':
    if (logitem->time && logitem->date)
      return 1;
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL)
      return 1;
    if (str_to_time (tkn, tfmt, &tm) != 0 || set_date (&logitem->date, tm) != 0) {
      free (tkn);
      return 1;
    }
    logitem->time = tkn;
    break;
    /* Virtual Host */
  case 'v':
    if (logitem->vhost)
      return 1;
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL || *tkn == '\0')
      return 1;
    logitem->vhost = tkn;
    break;
    /* remote hostname (IP only) */
  case 'h':
    if (logitem->host)
      return 1;
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL)
      return 1;
    if (invalid_ipaddr (tkn, &logitem->type_ip)) {
      free (tkn);
      return 1;
    }
    logitem->host = tkn;
    break;
    /* request method */
  case 'm':
    if (logitem->method)
      return 1;
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL)
      return 1;
    if (!extract_method (tkn)) {
      free (tkn);
      return 1;
    }
    logitem->method = tkn;
    break;
    /* request not including method or protocol */
  case 'U':
    if (logitem->req)
      return 1;
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL || *tkn == '\0')
      return 1;
    if ((logitem->req = decode_url (tkn)) == NULL) {
      free (tkn);
      return 1;
    }
    free (tkn);
    break;
    /* query string alone, e.g., ?param=goaccess&tbm=shop */
  case 'q':
    if (logitem->qstr)
      return 1;
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL || *tkn == '\0')
      return 0;
    if ((logitem->qstr = decode_url (tkn)) == NULL) {
      free (tkn);
      return 1;
    }
    free (tkn);
    break;
    /* request protocol */
  case 'H':
    if (logitem->protocol)
      return 1;
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL)
      return 1;
    if (invalid_protocol (tkn)) {
      free (tkn);
      return 1;
    }
    logitem->protocol = tkn;
    break;
    /* request, including method + protocol */
  case 'r':
    if (logitem->req)
      return 1;
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL)
      return 1;
    logitem->req = parse_req (tkn, &logitem->method, &logitem->protocol);
    free (tkn);
    break;
    /* Status Code */
  case 's':
    if (logitem->status)
      return 1;
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL)
      return 1;
    strtol (tkn, &sEnd, 10);
    if (tkn == sEnd || *sEnd != '\0' || errno == ERANGE) {
      free (tkn);
      return 1;
    }
    logitem->status = tkn;
    break;
    /* size of response in bytes - excluding HTTP headers */
  case 'b':
    if (logitem->resp_size)
      return 1;
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL)
      return 1;
    bandw = strtoull (tkn, &bEnd, 10);
    if (tkn == bEnd || *bEnd != '\0' || errno == ERANGE)
      bandw = 0;
    logitem->resp_size = bandw;
    conf.bandwidth = 1;
    free (tkn);
    break;
    /* referrer */
  case 'R':
    if (logitem->ref)
      return 1;
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL)
      tkn = alloc_string ("-");
    if (tkn != NULL && *tkn == '\0') {
      free (tkn);
      tkn = alloc_string ("-");
    }
    if (strcmp (tkn, "-") != 0) {
      extract_keyphrase (tkn, &logitem->keyphrase);
      extract_referer_site (tkn, logitem->site);
    }
    logitem->ref = tkn;
    break;
    /* user agent */
  case 'u':
    if (logitem->agent)
      return 1;
    tkn = parse_string (&(*str), end, 1);
    if (tkn != NULL && *tkn != '\0') {
      /* Make sure the user agent is decoded (i.e.: CloudFront)
       * and replace all '+' with ' ' (i.e.: w3c) */
      logitem->agent = char_replace (decode_url (tkn), '+', ' ');
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
    logitem->agent = tkn;
    break;
    /* time taken to serve the request, in milliseconds as a decimal number */
  case 'L':
    /* ignore it if we already have served time */
    if (logitem->serve_time)
      break;

    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL)
      return 1;
    serve_secs = strtoull (tkn, &bEnd, 10);

    if (tkn == bEnd || *bEnd != '\0' || errno == ERANGE)
      serve_secs = 0;
    /* convert it to microseconds */
    logitem->serve_time = (serve_secs > 0) ? serve_secs * MILS : 0;

    contains_usecs ();  /* set flag */
    free (tkn);
    break;
    /* time taken to serve the request, in seconds with a milliseconds
     * resolution */
  case 'T':
    /* ignore it if we already have served time */
    if (logitem->serve_time)
      break;

    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL)
      return 1;
    if (strchr (tkn, '.') != NULL)
      serve_secs = strtod (tkn, &bEnd);
    else
      serve_secs = strtoull (tkn, &bEnd, 10);

    if (tkn == bEnd || *bEnd != '\0' || errno == ERANGE)
      serve_secs = 0;
    /* convert it to microseconds */
    logitem->serve_time = (serve_secs > 0) ? serve_secs * SECS : 0;

    contains_usecs ();  /* set flag */
    free (tkn);
    break;
    /* time taken to serve the request, in microseconds */
  case 'D':
    /* ignore it if we already have served time */
    if (logitem->serve_time)
      break;

    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL)
      return 1;
    serve_time = strtoull (tkn, &bEnd, 10);
    if (tkn == bEnd || *bEnd != '\0' || errno == ERANGE)
      serve_time = 0;
    logitem->serve_time = serve_time;

    contains_usecs ();  /* set flag */
    free (tkn);
    break;
    /* move forward through str until not a space */
  case '~':
    find_alpha (&(*str));
    break;
    /* everything else skip it */
  default:
    if ((pch = strchr (*str, p[1])) != NULL)
      *str += pch - *str;
  }

  return 0;
}

/* Iterate over the given log format.
 *
 * On error, or unable to parse it, 1 is returned.
 * On success, the malloc'd token is assigned to a GLogItem member and
 * 0 is returned. */
static int
parse_format (GLogItem * logitem, char *str)
{
  char end[2 + 1] = { 0 };
  const char *p;
  const char *lfmt = conf.log_format;
  int special = 0, optdelim = 0;

  if (str == NULL || *str == '\0')
    return 1;

  /* iterate over the log format */
  for (p = lfmt; *p; p++) {
    /* advance to the first unescaped delim */
    if (*p == '\\')
      continue;
    if (*p == '%') {
      special++;
      continue;
    }
    if (special && *p != '\0') {
      if ((str == NULL) || (*str == '\0'))
        return 0;

      memset (end, 0, sizeof end);
      optdelim = get_delim (end, p);
      /* attempt to parse format specifiers */
      if (parse_specifier (logitem, &str, p, end) == 1)
        return 1;
      /* account for the extra delimiter */
      if (optdelim)
        p++;
      special = 0;
    } else if (special && isspace (p[0])) {
      return 1;
    } else {
      str++;
    }
  }

  return 0;
}

/* Determine if the log string is valid and if it's not a comment.
 *
 * On error, or invalid, 1 is returned.
 * On success, or valid line, 0 is returned. */
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

/* Determine if we need to lock the mutex. */
static void
lock_spinner (void)
{
  if (parsing_spinner != NULL && parsing_spinner->state == SPN_RUN)
    pthread_mutex_lock (&parsing_spinner->mutex);
}

/* Determine if we need to unlock the mutex. */
static void
unlock_spinner (void)
{
  if (parsing_spinner != NULL && parsing_spinner->state == SPN_RUN)
    pthread_mutex_unlock (&parsing_spinner->mutex);
}

/* Ignore request's query string. e.g.,
 * /index.php?timestamp=1454385289 */
static void
strip_qstring (char *req)
{
  char *qmark;
  if ((qmark = strchr (req, '?')) != NULL) {
    if ((qmark - req) > 0)
      *qmark = '\0';
  }
}

/* Increment the overall bandwidth. */
static void
inc_resp_size (GLog * glog, uint64_t resp_size)
{
  glog->resp_size += resp_size;
#ifdef TCB_BTREE
  ht_insert_genstats_bw ("bandwidth", resp_size);
#endif
}

/* Keep track of all invalid log strings. */
static void
count_invalid (GLog * glog, const char *line, int test)
{
  glog->invalid++;
#ifdef TCB_BTREE
  if (!test)
    ht_insert_genstats ("failed_requests", 1);
#else
  (void) test;
#endif
  if (conf.invalid_requests_log)
    LOG_INVALID (("%s", line));
}

/* Keep track of all valid log strings. */
static void
count_valid (GLog * glog, int test)
{
  lock_spinner ();
  glog->valid++;
#ifdef TCB_BTREE
  if (!test)
    ht_insert_genstats ("valid_requests", 1);
#else
  (void) test;
#endif
  unlock_spinner ();
}

/* Keep track of all valid and processed log strings. */
static void
count_process (GLog * glog, int test)
{
  lock_spinner ();
  glog->processed++;
#ifdef TCB_BTREE
  if (!test)
    ht_insert_genstats ("total_requests", 1);
#else
  (void) test;
#endif
  unlock_spinner ();
}

/* Keep track of all excluded log strings (IPs).
 *
 * If IP not range, 1 is returned.
 * If IP is excluded, 0 is returned. */
static int
excluded_ip (GLog * glog, GLogItem * logitem, int test)
{
  if (conf.ignore_ip_idx && ip_in_range (logitem->host)) {
    glog->excluded_ip++;
#ifdef TCB_BTREE
    if (!test)
      ht_insert_genstats ("excluded_ip", 1);
#else
    (void) test;
#endif
    return 0;
  }
  return 1;
}

/* Determine if the request is from a robot or spider.
 *
 * If not from a robot, 1 is returned.
 * If from a robot, 0 is returned. */
static int
exclude_crawler (GLogItem * logitem)
{
  return conf.ignore_crawlers && is_crawler (logitem->agent) ? 0 : 1;
}

/* Determine if the request of the given status code needs to be
 * ignored.
 *
 * If the status code is not within the ignore-array, 0 is returned.
 * If the status code is within the ignore-array, 1 is returned. */
static int
ignore_status_code (const char *status)
{
  if (conf.ignore_status_idx == 0)
    return 0;

  if (str_inarray (status, conf.ignore_status, conf.ignore_status_idx))
    return 1;
  return 0;
}

/* A wrapper function to determine if a log line needs to be ignored.
 *
 * If the request line is not ignored, 0 is returned.
 * If the request line is ignored, 1 is returned. */
static int
ignore_line (GLog * glog, GLogItem * logitem, int test)
{
  if (excluded_ip (glog, logitem, test) == 0)
    return 1;
  if (exclude_crawler (logitem) == 0)
    return 1;
  if (ignore_referer (logitem->site))
    return 1;
  if (ignore_status_code (logitem->status))
    return 1;

  /* check if we need to remove the request's query string */
  if (conf.ignore_qstr)
    strip_qstring (logitem->req);

  return 0;
}

/* A wrapper function to determine if the request is static.
 *
 * If the request is not static, 0 is returned.
 * If the request is static, 1 is returned. */
static int
is_static (GLogItem * logitem)
{
  return verify_static_content (logitem->req);
}

/* Determine if the request status code is a 404.
 *
 * If the request is not a 404, 0 is returned.
 * If the request is a 404, 1 is returned. */
static int
is_404 (GLogItem * logitem)
{
  /* is this a 404? */
  if (logitem->status && !memcmp (logitem->status, "404", 3))
    return 1;
  /* treat 444 as 404? */
  else if (logitem->status && !memcmp (logitem->status, "444", 3) &&
           conf.code444_as_404)
    return 1;
  return 0;
}

/* A wrapper function to insert a keymap string key.
 *
 * If the given key exists, its value is returned.
 * On error, -1 is returned.
 * On success the value of the key inserted is returned */
static int
insert_keymap (char *key, GModule module)
{
  return ht_insert_keymap (module, key);
}

/* A wrapper function to insert a datamap int key and string value. */
static void
insert_data (int nkey, const char *data, GModule module)
{
  ht_insert_datamap (module, nkey, data);
}

/* A wrapper function to insert a uniqmap string key.
 *
 * If the given key exists, 0 is returned.
 * On error, -1 is returned.
 * On success the value of the key inserted is returned */
static int
insert_uniqmap (char *uniq_key, GModule module)
{
  return ht_insert_uniqmap (module, uniq_key);
}

/* A wrapper function to insert a rootmap int key from the keymap
 * store mapped to its string value. */
static void
insert_rootmap (int root_nkey, const char *root, GModule module)
{
  ht_insert_rootmap (module, root_nkey, root);
}

/* A wrapper function to insert a data int key mapped to the
 * corresponding int root key. */
static void
insert_root (int data_nkey, int root_nkey, GModule module)
{
  ht_insert_root (module, data_nkey, root_nkey);
}

/* A wrapper function to increase hits counter from an int key. */
static void
insert_hit (int data_nkey, GModule module)
{
  ht_insert_hits (module, data_nkey, 1);
  ht_insert_meta_data (module, "hits", 1);
}

/* A wrapper function to increase visitors counter from an int
 * key. */
static void
insert_visitor (int uniq_nkey, GModule module)
{
  ht_insert_visitor (module, uniq_nkey, 1);
  ht_insert_meta_data (module, "visitors", 1);
}

/* A wrapper function to increases bandwidth counter from an int
 * key. */
static void
insert_bw (int data_nkey, uint64_t size, GModule module)
{
  ht_insert_bw (module, data_nkey, size);
  ht_insert_meta_data (module, "bytes", size);
}

/* A wrapper call to increases cumulative time served counter
 * from an int key. */
static void
insert_cumts (int data_nkey, uint64_t ts, GModule module)
{
  ht_insert_cumts (module, data_nkey, ts);
  ht_insert_meta_data (module, "cumts", ts);
}

/* A wrapper call to insert the maximum time served counter from
 * an int key. */
static void
insert_maxts (int data_nkey, uint64_t ts, GModule module)
{
  ht_insert_maxts (module, data_nkey, ts);
  ht_insert_meta_data (module, "maxts", ts);
}

static void
insert_method (int nkey, const char *data, GModule module)
{
  ht_insert_method (module, nkey, data ? data : "---");
}

/* A wrapper call to insert a method given an int key and string
 * value. */
static void
insert_protocol (int nkey, const char *data, GModule module)
{
  ht_insert_protocol (module, nkey, data ? data : "---");
}

/* A wrapper call to insert an agent for a hostname given an int
 * key and int value.  */
static void
insert_agent (int data_nkey, int agent_nkey, GModule module)
{
  ht_insert_agent (module, data_nkey, agent_nkey);
}

/* The following generates a unique key to identity unique visitors.
 * The key is made out of the IP, date, and user agent.
 * Note that for readability, doing a simple snprintf/sprintf should
 * suffice, however, memcpy is the fastest solution
 *
 * On success the new unique visitor key is returned */
static char *
get_uniq_visitor_key (GLogItem * logitem)
{
  char *ua, *key;
  size_t s1, s2, s3;

  ua = deblank (xstrdup (logitem->agent));

  s1 = strlen (logitem->host);
  s2 = strlen (logitem->date);
  s3 = strlen (ua);

  /* includes terminating null */
  key = xmalloc (s1 + s2 + s3 + 3);

  memcpy (key, logitem->host, s1);

  key[s1] = '|';
  memcpy (key + s1 + 1, logitem->date, s2 + 1);

  key[s1 + s2 + 1] = '|';
  memcpy (key + s1 + s2 + 2, ua, s3 + 1);

  free (ua);
  return key;
}

/* The following generates a unique key to identity unique requests.
 * The key is made out of the actual request, and if available, the
 * method and the protocol.  Note that for readability, doing a simple
 * snprintf/sprintf should suffice, however, memcpy is the fastest
 * solution
 *
 * On success the new unique request key is returned */
static char *
gen_unique_req_key (GLogItem * logitem)
{
  char *key = NULL;
  size_t s1 = 0, s2 = 0, s3 = 0, nul = 1, sep = 0;

  /* nothing to do */
  if (!conf.append_method && !conf.append_protocol)
    return xstrdup (logitem->req);
  /* still nothing to do */
  if (!logitem->method && !logitem->protocol)
    return xstrdup (logitem->req);

  s1 = strlen (logitem->req);
  if (logitem->method && conf.append_method) {
    s2 = strlen (logitem->method);
    nul++;
  }
  if (logitem->protocol && conf.append_protocol) {
    s3 = strlen (logitem->protocol);
    nul++;
  }

  /* includes terminating null */
  key = xmalloc (s1 + s2 + s3 + nul);
  /* append request */
  memcpy (key, logitem->req, s1);

  if (logitem->method && conf.append_method) {
    key[s1] = '|';
    sep++;
    memcpy (key + s1 + sep, logitem->method, s2 + 1);
  }
  if (logitem->protocol && conf.append_protocol) {
    key[s1 + s2 + sep] = '|';
    sep++;
    memcpy (key + s1 + s2 + sep, logitem->protocol, s3 + 1);
  }

  return key;
}

/* Append the query string to the request, and therefore, it modifies
 * the original logitem->req */
static void
append_query_string (char **req, const char *qstr)
{
  char *r;
  size_t s1, s2, qm = 0;

  s1 = strlen (*req);
  s2 = strlen (qstr);

  /* add '?' between the URL and the query string */
  if (*qstr != '?')
    qm = 1;

  r = xmalloc (s1 + s2 + qm + 1);
  memcpy (r, *req, s1);
  if (qm)
    r[s1] = '?';
  memcpy (r + s1 + qm, qstr, s2 + 1);

  free (*req);
  *req = r;
}

/* A wrapper to assign the given data key and the data item to the key
 * data structure */
static void
get_kdata (GKeyData * kdata, char *data_key, char *data)
{
  /* inserted in keymap */
  kdata->data_key = data_key;
  /* inserted in datamap */
  kdata->data = data;
}

/* Generate a visitor's key given the date specificity. For instance,
 * if the specificity if set to hours, then a generated key would
 * look like: 03/Jan/2016:09 */
static void
set_spec_visitor_key (char **fdate, const char *ftime)
{
  size_t dlen = 0, tlen = 0;
  char *key = NULL, *tkey = NULL, *pch = NULL;

  tkey = xstrdup (ftime);
  if (conf.date_spec_hr && (pch = strchr (tkey, ':')) && (pch - tkey) > 0)
    *pch = '\0';

  dlen = strlen (*fdate);
  tlen = strlen (tkey);

  key = xmalloc (dlen + tlen + 1);
  memcpy (key, *fdate, dlen);
  memcpy (key + dlen, tkey, tlen + 1);

  free (*fdate);
  free (tkey);
  *fdate = key;
}

/* Generate a unique key for the visitors panel from the given logitem
 * structure and assign it to out key data structure.
 *
 * On error, or if no date is found, 1 is returned.
 * On success, the date key is assigned to our key data structure.
 */
static int
gen_visitor_key (GKeyData * kdata, GLogItem * logitem)
{
  if (!logitem->date || !logitem->time)
    return 1;

  /* Append time specificity to date */
  if (conf.date_spec_hr)
    set_spec_visitor_key (&logitem->date, logitem->time);

  get_kdata (kdata, logitem->date, logitem->date);

  return 0;
}

/* Generate a unique key for the requests panel from the given logitem
 * structure and assign it to out key data structure.
 *
 * On success, the generated request key is assigned to our key data
 * structure.
 */
static int
gen_req_key (GKeyData * kdata, GLogItem * logitem)
{
  if (logitem->req && logitem->qstr)
    append_query_string (&logitem->req, logitem->qstr);
  logitem->req_key = gen_unique_req_key (logitem);

  get_kdata (kdata, logitem->req_key, logitem->req);

  return 0;
}

/* A wrapper to generate a unique key for the request panel.
 *
 * On error, or if the request is static or a 404, 1 is returned.
 * On success, the generated request key is assigned to our key data
 * structure.
 */
static int
gen_request_key (GKeyData * kdata, GLogItem * logitem)
{
  if (!logitem->req || logitem->is_404 || logitem->is_static)
    return 1;

  return gen_req_key (kdata, logitem);
}

/* A wrapper to generate a unique key for the request panel.
 *
 * On error, or if the request is not a 404, 1 is returned.
 * On success, the generated request key is assigned to our key data
 * structure. */
static int
gen_404_key (GKeyData * kdata, GLogItem * logitem)
{
  if (logitem->req && logitem->is_404)
    return gen_req_key (kdata, logitem);
  return 1;
}

/* A wrapper to generate a unique key for the request panel.
 *
 * On error, or if the request is not a static request, 1 is returned.
 * On success, the generated request key is assigned to our key data
 * structure. */
static int
gen_static_request_key (GKeyData * kdata, GLogItem * logitem)
{
  if (logitem->req && logitem->is_static)
    return gen_req_key (kdata, logitem);
  return 1;
}

/* A wrapper to generate a unique key for the virtual host panel.
 *
 * On error, 1 is returned.
 * On success, the generated vhost key is assigned to our key data
 * structure. */
static int
gen_vhost_key (GKeyData * kdata, GLogItem * logitem)
{
  if (!logitem->vhost)
    return 1;

  get_kdata (kdata, logitem->vhost, logitem->vhost);

  return 0;
}

/* A wrapper to generate a unique key for the hosts panel.
 *
 * On error, 1 is returned.
 * On success, the generated host key is assigned to our key data
 * structure. */
static int
gen_host_key (GKeyData * kdata, GLogItem * logitem)
{
  if (!logitem->host)
    return 1;

  get_kdata (kdata, logitem->host, logitem->host);

  return 0;
}

/* Generate a browser unique key for the browser's panel given a user
 * agent and assign the browser type/category as a root element.
 *
 * On error, 1 is returned.
 * On success, the generated browser key is assigned to our key data
 * structure. */
static int
gen_browser_key (GKeyData * kdata, GLogItem * logitem)
{
  char *agent = NULL;
  char browser_type[BROWSER_TYPE_LEN] = "";

  if (logitem->agent == NULL || *logitem->agent == '\0')
    return 1;

  agent = xstrdup (logitem->agent);
  logitem->browser = verify_browser (agent, browser_type);
  logitem->browser_type = xstrdup (browser_type);

  /* e.g., Firefox 11.12 */
  kdata->data = logitem->browser;
  kdata->data_key = logitem->browser;

  /* Firefox */
  kdata->root = logitem->browser_type;
  kdata->root_key = logitem->browser_type;

  free (agent);

  return 0;
}

/* Generate an operating system unique key for the OS' panel given a
 * user agent and assign the OS type/category as a root element.
 *
 * On error, 1 is returned.
 * On success, the generated OS key is assigned to our key data
 * structure. */
static int
gen_os_key (GKeyData * kdata, GLogItem * logitem)
{
  char *agent = NULL;
  char os_type[OPESYS_TYPE_LEN] = "";

  if (logitem->agent == NULL || *logitem->agent == '\0')
    return 1;

  agent = xstrdup (logitem->agent);
  logitem->os = verify_os (agent, os_type);
  logitem->os_type = xstrdup (os_type);

  /* e.g., Linux,Ubuntu 10.12 */
  kdata->data = logitem->os;
  kdata->data_key = logitem->os;

  /* Linux */
  kdata->root = logitem->os_type;
  kdata->root_key = logitem->os_type;

  free (agent);

  return 0;
}

/* A wrapper to generate a unique key for the referrers panel.
 *
 * On error, 1 is returned.
 * On success, the generated referrer key is assigned to our key data
 * structure. */
static int
gen_referer_key (GKeyData * kdata, GLogItem * logitem)
{
  if (!logitem->ref)
    return 1;

  get_kdata (kdata, logitem->ref, logitem->ref);

  return 0;
}

/* A wrapper to generate a unique key for the referring sites panel.
 *
 * On error, 1 is returned.
 * On success, the generated referring site key is assigned to our key data
 * structure. */
static int
gen_ref_site_key (GKeyData * kdata, GLogItem * logitem)
{
  if (logitem->site[0] == '\0')
    return 1;

  get_kdata (kdata, logitem->site, logitem->site);

  return 0;
}

/* A wrapper to generate a unique key for the keyphrases panel.
 *
 * On error, 1 is returned.
 * On success, the generated keyphrase key is assigned to our key data
 * structure. */
static int
gen_keyphrase_key (GKeyData * kdata, GLogItem * logitem)
{
  if (!logitem->keyphrase)
    return 1;

  get_kdata (kdata, logitem->keyphrase, logitem->keyphrase);

  return 0;
}

/* A wrapper to generate a unique key for the geolocation panel.
 *
 * On error, 1 is returned.
 * On success, the generated geolocation key is assigned to our key
 * data structure. */
#ifdef HAVE_LIBGEOIP
static int
gen_geolocation_key (GKeyData * kdata, GLogItem * logitem)
{
  char continent[CONTINENT_LEN] = "";
  char country[COUNTRY_LEN] = "";

  if (extract_geolocation (logitem, continent, country) == 1)
    return 1;

  if (country[0] != '\0')
    logitem->country = xstrdup (country);

  if (continent[0] != '\0')
    logitem->continent = xstrdup (continent);

  kdata->data_key = logitem->country;
  kdata->data = logitem->country;

  kdata->root = logitem->continent;
  kdata->root_key = logitem->continent;

  return 0;
}
#endif

/* A wrapper to generate a unique key for the status code panel.
 *
 * On error, 1 is returned.
 * On success, the generated status code key is assigned to our key
 * data structure. */
static int
gen_status_code_key (GKeyData * kdata, GLogItem * logitem)
{
  const char *status = NULL, *type = NULL;

  if (!logitem->status)
    return 1;

  type = verify_status_code_type (logitem->status);
  status = verify_status_code (logitem->status);

  kdata->data = (char *) status;
  kdata->data_key = (char *) status;

  kdata->root = (char *) type;
  kdata->root_key = (char *) type;

  return 0;
}

/* Given a time string containing at least %H:%M, extract either the
 * tenth of a minute or an hour.
 *
 * On error, the given string is not modified.
 * On success, the conf specificity is extracted. */
static void
parse_time_specificity_string (char *hmark, char *ftime)
{
  /* tenth of a minute specificity - e.g., 18:2 */
  if (conf.hour_spec_min && hmark[1] != '\0') {
    hmark[2] = '\0';
    return;
  }

  /* hour specificity (default) */
  if ((hmark - ftime) > 0)
    *hmark = '\0';
}

/* A wrapper to generate a unique key for the time distribution panel.
 *
 * On error, 1 is returned.
 * On success, the generated time key is assigned to our key data
 * structure. */
static int
gen_visit_time_key (GKeyData * kdata, GLogItem * logitem)
{
  char *hmark = NULL;
  char hour[HRMI_LEN] = "";     /* %H:%M */
  if (!logitem->time)
    return 1;

  /* if not a timestamp, then it must be a string containing the hour.
   * this is faster than actual date conversion */
  if (!has_timestamp (conf.time_format) &&
      (hmark = strchr (logitem->time, ':'))) {
    parse_time_specificity_string (hmark, logitem->time);
    get_kdata (kdata, logitem->time, logitem->time);
    return 0;
  }

  /* otherwise it attempts to convert the date given a time format,
   * though this is slower */
  memset (hour, 0, sizeof *hour);
  if (convert_date (hour, logitem->time, conf.time_format, "%H:%M", HRMI_LEN) !=
      0)
    return 1;

  if (hour == '\0')
    return 1;

  if ((hmark = strchr (hour, ':')))
    parse_time_specificity_string (hmark, hour);

  free (logitem->time);
  logitem->time = xstrdup (hour);
  get_kdata (kdata, logitem->time, logitem->time);

  return 0;
}

/* Determine if 404s need to be added to the unique visitors count.
 *
 * If it needs to be added, 0 is returned else 1 is returned. */
static int
include_uniq (GLogItem * logitem)
{
  int u = conf.client_err_to_unique_count;

  if (!logitem->status || logitem->status[0] != '4' ||
      (u && logitem->status[0] == '4'))
    return 1;
  return 0;
}

/* Determine which data metrics need to be set and set them. */
static void
set_datamap (GLogItem * logitem, GKeyData * kdata, const GParse * parse)
{
  GModule module;
  module = parse->module;

  /* insert data */
  parse->datamap (kdata->data_nkey, kdata->data, module);

  /* insert rootmap and root-data map */
  if (parse->rootmap) {
    parse->rootmap (kdata->root_nkey, kdata->root, module);
    insert_root (kdata->data_nkey, kdata->root_nkey, module);
  }
  /* insert hits */
  if (parse->hits)
    parse->hits (kdata->data_nkey, module);
  /* insert visitors */
  if (parse->visitor && kdata->uniq_nkey != 0)
    parse->visitor (kdata->data_nkey, module);
  /* insert bandwidth */
  if (parse->bw)
    parse->bw (kdata->data_nkey, logitem->resp_size, module);
  /* insert averages time served */
  if (parse->cumts)
    parse->cumts (kdata->data_nkey, logitem->serve_time, module);
  /* insert averages time served */
  if (parse->maxts)
    parse->maxts (kdata->data_nkey, logitem->serve_time, module);
  /* insert method */
  if (parse->method && conf.append_method)
    parse->method (kdata->data_nkey, logitem->method, module);
  /* insert protocol */
  if (parse->protocol && conf.append_protocol)
    parse->protocol (kdata->data_nkey, logitem->protocol, module);
  /* insert agent */
  if (parse->agent && conf.list_agents)
    parse->agent (kdata->data_nkey, logitem->agent_nkey, module);
}

/* Set data mapping and metrics. */
static void
map_log (GLogItem * logitem, const GParse * parse, GModule module)
{
  GKeyData kdata;
  char *uniq_key = NULL;

  new_modulekey (&kdata);
  if (parse->key_data (&kdata, logitem) == 1)
    return;

  /* each module requires a data key/value */
  if (parse->datamap && kdata.data_key)
    kdata.data_nkey = insert_keymap (kdata.data_key, module);

  /* each module contains a uniq visitor key/value */
  if (parse->visitor && logitem->uniq_key && include_uniq (logitem)) {
    uniq_key = ints_to_str (logitem->uniq_nkey, kdata.data_nkey);
    /* unique key already exists? */
    kdata.uniq_nkey = insert_uniqmap (uniq_key, module);
    free (uniq_key);
  }

  /* root keys are optional */
  if (parse->rootmap && kdata.root_key)
    kdata.root_nkey = insert_keymap (kdata.root_key, module);

  /* each module requires a root key/value */
  if (parse->datamap && kdata.data_key)
    set_datamap (logitem, &kdata, parse);
}

/* Process a log line and set the data into the corresponding data
 * structure. */
static void
process_log (GLogItem * logitem)
{
  GModule module;
  const GParse *parse = NULL;
  size_t idx = 0;

  /* Insert one unique visitor key per request to avoid the
   * overhead of storing one key per module */
  logitem->uniq_nkey = ht_insert_unique_key (logitem->uniq_key);

  /* If we need to store user agents per IP, then we store them and retrieve
   * its numeric key.
   * It maintains two maps, one for key -> value, and another
   * map for value -> key*/
  if (conf.list_agents) {
    /* insert UA key and get a numeric value */
    logitem->agent_nkey = ht_insert_agent_key (logitem->agent);
    /* insert a numeric key and map it to a UA string */
    ht_insert_agent_value (logitem->agent_nkey, logitem->agent);
  }

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    if (!(parse = panel_lookup (module)))
      continue;
    map_log (logitem, parse, module);
  }
}

/* Process a line from the log and store it accordingly taking into
 * account multiple parsing options prior to setting data into the
 * corresponding data structure.
 *
 * On success, 0 is returned */
static int
pre_process_log (GLog * glog, char *line, int test)
{
  GLogItem *logitem;

  if (valid_line (line)) {
    count_invalid (glog, line, test);
    return 0;
  }

  count_process (glog, test);
  logitem = init_log_item (glog);
  /* parse a line of log, and fill structure with appropriate values */
  if (parse_format (logitem, line)) {
    count_invalid (glog, line, test);
    goto cleanup;
  }

  /* must have the following fields */
  if (logitem->host == NULL || logitem->date == NULL || logitem->req == NULL) {
    count_invalid (glog, line, test);
    goto cleanup;
  }
  /* agent will be null in cases where %u is not specified */
  if (logitem->agent == NULL)
    logitem->agent = alloc_string ("-");

  /* testing log only */
  if (test) {
    count_valid (glog, test);
    goto cleanup;
  }

  /* ignore line */
  if (ignore_line (glog, logitem, test))
    goto cleanup;

  if (is_404 (logitem))
    logitem->is_404 = 1;
  else if (is_static (logitem))
    logitem->is_static = 1;

  logitem->uniq_key = get_uniq_visitor_key (logitem);

  inc_resp_size (glog, logitem->resp_size);
  process_log (logitem);
  count_valid (glog, test);

cleanup:
  free_glog (logitem);

  return 0;
}

/* Iterate over the log and read line by line (uses a buffer of fixed
 * size).
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
#ifndef WITH_GETLINE
static int
read_line (FILE * fp, int lines2test, GLog ** glog)
{
  char line[LINE_BUFFER] = { 0 };
  int i = 0, test = -1 == lines2test ? 0 : 1;

  while (fgets (line, LINE_BUFFER, fp) != NULL) {
    if (conf.stop_processing)
      break;
    if (lines2test >= 0 && i++ == lines2test)
      break;

    /* start processing log line */
    if (pre_process_log ((*glog), line, test)) {
      if (!(*glog)->piping)
        fclose (fp);
      return 1;
    }
  }

  return 0;
}
#endif

/* Iterate over the log and read line by line (use GNU get_line to
 * parse the whole line).
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
#ifdef WITH_GETLINE
static int
read_line (FILE * fp, int lines2test, GLog ** glog)
{
  char *line = NULL;
  size_t len = LINE_BUFFER;
  int i = 0, test = -1 == lines2test ? 0 : 1;

  line = xcalloc (LINE_BUFFER, sizeof (*line));
  while (getline (&line, &len, fp) != -1) {
    if (conf.stop_processing)
      break;
    if (lines2test >= 0 && i++ == lines2test)
      break;

    /* start processing log line */
    if (pre_process_log ((*glog), line, test)) {
      if (!(*glog)->piping)
        fclose (fp);
      free (line);
      return 1;
    }
  }
  free (line);

  return 0;
}
#endif

/* Read the given log line by line and process its data.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
static int
read_log (GLog ** glog, int lines2test)
{
  FILE *fp = NULL;

  /* no data piped, no log passed, load from disk only then */
  if (conf.load_from_disk && !conf.ifile && isatty (STDIN_FILENO)) {
    (*glog)->load_from_disk_only = 1;
    return 0;
  }

  /* no log passed, but data piped */
  if (!isatty (STDIN_FILENO) && !conf.ifile) {
    fp = stdin;
    (*glog)->piping = 1;
  }

  /* make sure we can open the log (if not reading from stdin) */
  if (!(*glog)->piping && (fp = fopen (conf.ifile, "r")) == NULL)
    FATAL ("Unable to open the specified log file. %s", strerror (errno));

  /* read line by line */
  if (read_line (fp, lines2test, glog))
    return 1;

  /* definitely not portable! */
  if ((*glog)->piping)
    freopen ("/dev/tty", "r", stdin);

  /* close log file if not a pipe */
  if (!(*glog)->piping)
    fclose (fp);

  return 0;
}

/* Entry point to parse the log line by line.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
int
parse_log (GLog ** glog, char *tail, int lines2test)
{
  const char *err_log = NULL;
  int test = -1 == lines2test ? 0 : 1;

  /* process tail data and return */
  if (tail != NULL) {
    if (pre_process_log ((*glog), tail, test))
      return 1;
    return 0;
  }

  /* verify that we have the required formats */
  if ((err_log = verify_formats ()))
    FATAL ("%s", err_log);

  /* the first run */
  return read_log (glog, lines2test);
}

/* Ensure we have valid hits
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
int
test_format (GLog * glog)
{
  if (parse_log (&glog, NULL, NUM_TESTS))
    FATAL ("Error while processing file");

  /* it did not process any records, and since we're loading the dataset from
   * disk, then it is safe to assume is right */
  if (glog->load_from_disk_only)
    return 0;

  if (glog->valid == 0)
    return 1;
  return 0;
}
