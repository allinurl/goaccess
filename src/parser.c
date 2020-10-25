/**
 * parser.c -- web log parsing
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2020 Gerardo Orellana <hello @ goaccess.io>
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

#include "gkhash.h"

#ifdef HAVE_GEOLOCATION
#include "geoip1.h"
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
static int gen_remote_user_key (GKeyData * kdata, GLogItem * logitem);
static int gen_cache_status_key (GKeyData * kdata, GLogItem * logitem);
static int gen_referer_key (GKeyData * kdata, GLogItem * logitem);
static int gen_ref_site_key (GKeyData * kdata, GLogItem * logitem);
static int gen_request_key (GKeyData * kdata, GLogItem * logitem);
static int gen_static_request_key (GKeyData * kdata, GLogItem * logitem);
static int gen_status_code_key (GKeyData * kdata, GLogItem * logitem);
static int gen_visit_time_key (GKeyData * kdata, GLogItem * logitem);
#ifdef HAVE_GEOLOCATION
static int gen_geolocation_key (GKeyData * kdata, GLogItem * logitem);
#endif

/* insertion metric routines */
static void insert_data (GModule module, GKeyData * kdata);
static void insert_rootmap (GModule module, GKeyData * kdata);
static void insert_root (GModule module, GKeyData * kdata);
static void insert_hit (GModule module, GKeyData * kdata);
static void insert_visitor (GModule module, GKeyData * kdata);
static void insert_bw (GModule module, GKeyData * kdata, uint64_t size);
static void insert_cumts (GModule module, GKeyData * kdata, uint64_t ts);
static void insert_maxts (GModule module, GKeyData * kdata, uint64_t ts);
static void insert_method (GModule module, GKeyData * kdata, const char *data);
static void insert_protocol (GModule module, GKeyData * kdata, const char *data);
static void insert_agent (GModule module, GKeyData * kdata, uint32_t agent_nkey);

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
#ifdef HAVE_GEOLOCATION
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
  }, {
    REMOTE_USER,
    gen_remote_user_key,
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
    CACHE_STATUS,
    gen_cache_status_key,
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
new_modulekey (GKeyData * kdata) {
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
panel_lookup (GModule module) {
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
new_grawdata (void) {
  GRawData *raw_data = xmalloc (sizeof (*raw_data));
  memset (raw_data, 0, sizeof *raw_data);

  return raw_data;
}

/* Allocate memory for a new GRawDataItem instance.
 *
 * On success, the newly allocated GRawDataItem is returned . */
GRawDataItem *
new_grawdata_item (unsigned int size) {
  GRawDataItem *item = xcalloc (size, sizeof (*item));
  return item;
}

/* Free memory allocated for a GRawData and GRawDataItem instance. */
void
free_raw_data (GRawData * raw_data) {
  free (raw_data->items);
  free (raw_data);
}

/* Reset an instance of GLog structure. */
void
reset_struct (GLog * glog) {
  glog->invalid = 0;
  glog->processed = 0;
}

/* Allocate memory for a new GLog instance.
 *
 * On success, the newly allocated GLog is returned . */
GLog *
init_log (void) {
  GLog *glog = xmalloc (sizeof (GLog));
  memset (glog, 0, sizeof *glog);

  return glog;
}

/* Initialize a new GLogItem instance.
 *
 * On success, the new GLogItem instance is returned. */
GLogItem *
init_log_item (GLog * glog) {
  time_t now = time (0);
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
  logitem->errstr = NULL;
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
  logitem->resp_size = 0LL;
  logitem->serve_time = 0;
  logitem->status = NULL;
  logitem->time = NULL;
  logitem->uniq_key = NULL;
  logitem->vhost = NULL;
  logitem->userid = NULL;
  logitem->cache_status = NULL;

  memset (logitem->site, 0, sizeof (logitem->site));
  localtime_r (&now, &logitem->dt);

  return logitem;
}

/* Free all members of a GLogItem */
static void
free_glog (GLogItem * logitem) {
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
  if (logitem->errstr != NULL)
    free (logitem->errstr);
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
  if (logitem->userid != NULL)
    free (logitem->userid);
  if (logitem->cache_status != NULL)
    free (logitem->cache_status);
  if (logitem->vhost != NULL)
    free (logitem->vhost);

  free (logitem);
}

/* Decodes the given URL-encoded string.
 *
 * On success, the decoded string is assigned to the output buffer. */
#define B16210(x) (((x) >= '0' && (x) <= '9') ? ((x) - '0') : (toupper((x)) - 'A' + 10))
static void
decode_hex (char *url, char *out) {
  char *ptr;
  const char *c;

  for (c = url, ptr = out; *c; c++) {
    if (*c != '%' || !isxdigit (c[1]) || !isxdigit (c[2])) {
      *ptr++ = *c;
    } else {
      *ptr++ = (char) ((B16210 (c[1]) * 16) + (B16210 (c[2])));
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
decode_url (char *url) {
  char *out, *decoded;

  if ((url == NULL) || (*url == '\0'))
    return NULL;

  out = decoded = xstrdup (url);
  decode_hex (url, out);
  /* double encoded URL? */
  if (conf.double_decode)
    decode_hex (decoded, out);
  strip_newlines (out);

  return trim_str (char_replace (out, '+', ' '));
}

/* Process keyphrases from Google search, cache, and translate.
 * Note that the referer hasn't been decoded at the entry point
 * since there could be '&' within the search query.
 *
 * On error, 1 is returned.
 * On success, the extracted keyphrase is assigned and 0 is returned. */
static int
extract_keyphrase (char *ref, char **keyphrase) {
  char *r, *ptr, *pch, *referer;
  int encoded = 0;

  if (!(strstr (ref, "http://www.google.")) &&
      !(strstr (ref, "http://webcache.googleusercontent.com/")) &&
      !(strstr (ref, "http://translate.googleusercontent.com/")) &&
      !(strstr (ref, "https://www.google.")) &&
      !(strstr (ref, "https://webcache.googleusercontent.com/")) &&
      !(strstr (ref, "https://translate.googleusercontent.com/")))
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
  else if ((r = strstr (ref, "&q=")) != NULL || (r = strstr (ref, "?q=")) != NULL)
    r += 3;
  else if ((r = strstr (ref, "%26q%3D")) != NULL || (r = strstr (ref, "%3Fq%3D")) != NULL)
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

#ifdef HAVE_GEOLOCATION
/* Extract geolocation for the given host.
 *
 * On error, 1 is returned.
 * On success, the extracted continent and country are set and 0 is
 * returned. */
static int
extract_geolocation (GLogItem * logitem, char *continent, char *country) {
  if (!is_geoip_resource ())
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
extract_referer_site (const char *referer, char *host) {
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
verify_static_content (const char *req) {
  const char *nul = req + strlen (req);
  const char *ext = NULL, *pch = NULL;
  int elen = 0, i;

  if (strlen (req) < conf.static_file_max_len)
    return 0;

  for (i = 0; i < conf.static_file_idx; ++i) {
    ext = conf.static_files[i];
    if (ext == NULL || *ext == '\0')
      continue;

    elen = strlen (ext);
    if (conf.all_static_files && (pch = strchr (req, '?')) != NULL && pch - req > elen) {
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
extract_method (const char *token) {
  const char *methods[] = {
    "OPTIONS", "GET", "HEAD", "POST", "PUT",
    "DELETE", "TRACE", "CONNECT", "PATCH", "options",
    "get", "head", "post", "put", "delete",
    "trace", "connect", "patch",
    /* WebDAV */
    "PROPFIND", "PROPPATCH", "MKCOL", "COPY", "MOVE",
    "LOCK", "UNLOCK", "VERSION-CONTROL", "REPORT", "CHECKOUT",
    "CHECKIN", "UNCHECKOUT", "MKWORKSPACE", "UPDATE", "LABEL",
    "MERGE", "BASELINE-CONTROL", "MKACTIVITY", "ORDERPATCH", "propfind",
    "propwatch", "mkcol", "copy", "move", "lock",
    "unlock", "version-control", "report", "checkout", "checkin",
    "uncheckout", "mkworkspace", "update", "label", "merge",
    "baseline-control", "mkactivity", "orderpatch"
  };

  const int methods_count = sizeof (methods) / sizeof (*methods);

  int i;
  /* Length of every string in list */
  static int list_length[sizeof (methods) / sizeof (*methods)] = { -1 };
  /* Only calculate length on first time */
  if (list_length[0] == -1) {
    for (i = 0; i < methods_count; i++) {
      list_length[i] = strlen (methods[i]);
    }
  }

  for (i = 0; i < methods_count; i++) {
    if (strncmp (token, methods[i], list_length[i]) == 0) {
      return methods[i];
    }
  }
  return NULL;
}

/* Determine if time-served data was stored on-disk. */
static void
contains_usecs (void) {
  if (conf.serve_usecs)
    return;
  conf.serve_usecs = 1; /* flag */
}

static int
is_cache_hit (const char *tkn) {
  if (strcasecmp ("MISS", tkn) == 0)
    return 1;
  else if (strcasecmp ("BYPASS", tkn) == 0)
    return 1;
  else if (strcasecmp ("EXPIRED", tkn) == 0)
    return 1;
  else if (strcasecmp ("STALE", tkn) == 0)
    return 1;
  else if (strcasecmp ("UPDATING", tkn) == 0)
    return 1;
  else if (strcasecmp ("REVALIDATED", tkn) == 0)
    return 1;
  else if (strcasecmp ("HIT", tkn) == 0)
    return 1;
  return 0;
}

/* Determine if the given token is a valid HTTP protocol.
 *
 * If not valid, 1 is returned.
 * If valid, 0 is returned. */
static const char *
extract_protocol (const char *token) {
  const char *lookfor;

  if ((lookfor = "HTTP/1.0", !strncmp (token, lookfor, 8)) ||
      (lookfor = "HTTP/1.1", !strncmp (token, lookfor, 8)) ||
      (lookfor = "HTTP/2", !strncmp (token, lookfor, 6)))
    return lookfor;
  return NULL;
}

/* Parse a request containing the method and protocol.
 *
 * On error, or unable to parse, NULL is returned.
 * On success, the HTTP request is returned and the method and
 * protocol are assigned to the corresponding buffers. */
static char *
parse_req (char *line, char **method, char **protocol) {
  char *req = NULL, *request = NULL, *dreq = NULL, *ptr = NULL;
  const char *meth, *proto;
  ptrdiff_t rlen;

  meth = extract_method (line);

  /* couldn't find a method, so use the whole request line */
  if (meth == NULL) {
    request = xstrdup (line);
  }
  /* method found, attempt to parse request */
  else {
    req = line + strlen (meth);
    if (!(ptr = strrchr (req, ' ')) || !(proto = extract_protocol (++ptr)))
      return alloc_string ("-");

    req++;
    if ((rlen = ptr - req) <= 0)
      return alloc_string ("-");

    request = xmalloc (rlen + 1);
    strncpy (request, req, rlen);
    request[rlen] = 0;

    if (conf.append_method)
      (*method) = strtoupper (xstrdup (meth));

    if (conf.append_protocol)
      (*protocol) = strtoupper (xstrdup (proto));
  }

  if (!(dreq = decode_url (request)))
    return request;
  else if (*dreq == '\0') {
    free (dreq);
    return request;
  }

  free (request);
  return dreq;
}

/* Extract the next delimiter given a log format and copy the
 * delimiter(s) to the destination buffer.
 * Note that it's possible to store up to two delimiters.
 *
 * On error, the function returns.
 * On success, the delimiter(s) are stored in the dest buffer and the
 * number of extra delimiters is returned. */
static int
get_delim (char *dest, const char *p) {
  /* done, nothing to do */
  if (p[0] == '\0' || p[1] == '\0') {
    dest[0] = '\0';
    return 0;
  }
  /* add the first delim */
  dest[0] = *(p + 1);
  /* check if there's another possible delim */
  if (p[2] == '|' && p[3] != '%' && p[3] != '\0') {
    dest[1] = *(p + 3);
    return 1;
  }
  return 0;
}

/* Extract and malloc a token given the parsed rule.
 *
 * On success, the malloc'd token is returned. */
static char *
parsed_string (const char *pch, char **str, int move_ptr) {
  char *p;
  size_t len = (pch - *str + 1);

  p = xmalloc (len);
  memcpy (p, *str, (len - 1));
  p[len - 1] = '\0';
  if (move_ptr)
    *str += len - 1;

  return trim_str (p);
}

/* Find and extract a token given a log format rule.
 *
 * On error, or unable to parse it, NULL is returned.
 * On success, the malloc'd token is returned. */
static char *
parse_string (char **str, const char *delims, int cnt) {
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
      return parsed_string (pch, str, 1);
    /* advance to the first unescaped delim */
    if (*pch == '\\')
      pch++;
  } while (*pch++);

  return NULL;
}

/* Move forward through the log string until a non-space (!isspace)
 * char is found. */
static void
find_alpha (char **str) {
  char *s = *str;
  while (*s) {
    if (isspace (*s))
      s++;
    else
      break;
  }
  *str += s - *str;
}

/* Format the broken-down time tm to a numeric date format.
 *
 * On error, or unable to format the given tm, 1 is returned.
 * On success, a malloc'd format is returned. */
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
static int
set_date (char **fdate, struct tm tm) {
  char buf[DATE_LEN] = "";      /* Ymd */

  memset (buf, 0, sizeof (buf));
  if (strftime (buf, DATE_LEN, conf.date_num_format, &tm) <= 0)
    return 1;
  *fdate = xstrdup (buf);

  return 0;
}

/* Format the broken-down time tm to a numeric time format.
 *
 * On error, or unable to format the given tm, 1 is returned.
 * On success, a malloc'd format is returned. */
static int
set_time (char **ftime, struct tm tm) {
  char buf[TIME_LEN] = "";

  memset (buf, 0, sizeof (buf));
  if (strftime (buf, TIME_LEN, "%H:%M:%S", &tm) <= 0)
    return 1;
  *ftime = xstrdup (buf);

  return 0;
}

/* Determine the parsing specifier error and construct a message out
 * of it.
 *
 * On success, a malloc'd error message is assigned to the log
 * structure and 1 is returned. */
static int
spec_err (GLogItem * logitem, int code, const char spec, const char *tkn) {
  char *err = NULL;
  const char *fmt = NULL;

  switch (code) {
  case SPEC_TOKN_INV:
    fmt = "Token '%s' doesn't match specifier '%%%c'";
    err = xmalloc (snprintf (NULL, 0, fmt, (tkn ? tkn : "-"), spec) + 1);
    sprintf (err, fmt, (tkn ? tkn : "-"), spec);
    break;
  case SPEC_TOKN_SET:
    fmt = "Token already set for '%%%c' specifier.";
    err = xmalloc (snprintf (NULL, 0, fmt, spec) + 1);
    sprintf (err, fmt, spec);
    break;
  case SPEC_TOKN_NUL:
    fmt = "Token for '%%%c' specifier is NULL.";
    err = xmalloc (snprintf (NULL, 0, fmt, spec) + 1);
    sprintf (err, fmt, spec);
    break;
  case SPEC_SFMT_MIS:
    fmt = "Missing braces '%s' and ignore chars for specifier '%%%c'";
    err = xmalloc (snprintf (NULL, 0, fmt, (tkn ? tkn : "-"), spec) + 1);
    sprintf (err, fmt, (tkn ? tkn : "-"), spec);
    break;
  }
  logitem->errstr = err;

  return 1;
}

static void
set_tm_dt_logitem (GLogItem * logitem, struct tm tm) {
  logitem->dt.tm_year = tm.tm_year;
  logitem->dt.tm_mon = tm.tm_mon;
  logitem->dt.tm_mday = tm.tm_mday;
}

static void
set_tm_tm_logitem (GLogItem * logitem, struct tm tm) {
  logitem->dt.tm_hour = tm.tm_hour;
  logitem->dt.tm_min = tm.tm_min;
  logitem->dt.tm_sec = tm.tm_sec;
}

static void
set_numeric_date (uint32_t * numdate, const char *date) {
  int res = 0;
  if ((res = str2int (date)) == -1)
    FATAL ("Unable to parse date to integer %s", date);
  *numdate = res;
}

#pragma GCC diagnostic warning "-Wformat-nonliteral"

/* Parse the log string given log format rule.
 *
 * On error, or unable to parse it, 1 is returned.
 * On success, the malloc'd token is assigned to a GLogItem member. */
static int
parse_specifier (GLogItem * logitem, char **str, const char *p, const char *end) {
  struct tm tm;
  const char *dfmt = conf.date_format;
  const char *tfmt = conf.time_format;

  char *pch, *sEnd, *bEnd, *tkn = NULL;
  double serve_secs = 0.0;
  uint64_t bandw = 0, serve_time = 0;
  long status = 0L;

  errno = 0;
  memset (&tm, 0, sizeof (tm));

  switch (*p) {
    /* date */
  case 'd':
    if (logitem->date)
      return spec_err (logitem, SPEC_TOKN_SET, *p, NULL);
    /* parse date format including dates containing spaces,
     * i.e., syslog date format (Jul 15 20:10:56) */
    if (!(tkn = parse_string (&(*str), end, count_matches (dfmt, ' ') + 1)))
      return spec_err (logitem, SPEC_TOKN_NUL, *p, NULL);

    if (str_to_time (tkn, dfmt, &tm) != 0 || set_date (&logitem->date, tm) != 0) {
      spec_err (logitem, SPEC_TOKN_INV, *p, tkn);
      free (tkn);
      return 1;
    }
    set_numeric_date (&logitem->numdate, logitem->date);
    set_tm_dt_logitem (logitem, tm);
    free (tkn);
    break;
    /* time */
  case 't':
    if (logitem->time)
      return spec_err (logitem, SPEC_TOKN_SET, *p, NULL);
    if (!(tkn = parse_string (&(*str), end, 1)))
      return spec_err (logitem, SPEC_TOKN_NUL, *p, NULL);

    if (str_to_time (tkn, tfmt, &tm) != 0 || set_time (&logitem->time, tm) != 0) {
      spec_err (logitem, SPEC_TOKN_INV, *p, tkn);
      free (tkn);
      return 1;
    }
    set_tm_tm_logitem (logitem, tm);
    free (tkn);
    break;
    /* date/time as decimal, i.e., timestamps, ms/us  */
  case 'x':
    if (logitem->time && logitem->date)
      return spec_err (logitem, SPEC_TOKN_SET, *p, NULL);
    if (!(tkn = parse_string (&(*str), end, 1)))
      return spec_err (logitem, SPEC_TOKN_NUL, *p, NULL);

    if (str_to_time (tkn, tfmt, &tm) != 0 || set_date (&logitem->date, tm) != 0 ||
        set_time (&logitem->time, tm) != 0) {
      spec_err (logitem, SPEC_TOKN_INV, *p, tkn);
      free (tkn);
      return 1;
    }
    set_numeric_date (&logitem->numdate, logitem->date);
    set_tm_dt_logitem (logitem, tm);
    set_tm_tm_logitem (logitem, tm);
    break;
    /* Virtual Host */
  case 'v':
    if (logitem->vhost)
      return spec_err (logitem, SPEC_TOKN_SET, *p, NULL);
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL)
      return spec_err (logitem, SPEC_TOKN_NUL, *p, NULL);
    logitem->vhost = tkn;
    break;
    /* remote user */
  case 'e':
    if (logitem->userid)
      return spec_err (logitem, SPEC_TOKN_SET, *p, NULL);
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL)
      return spec_err (logitem, SPEC_TOKN_NUL, *p, NULL);
    logitem->userid = tkn;
    break;
    /* cache status */
  case 'C':
    if (logitem->cache_status)
      return spec_err (logitem, SPEC_TOKN_SET, *p, NULL);
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL)
      return spec_err (logitem, SPEC_TOKN_NUL, *p, NULL);
    if (is_cache_hit (tkn))
      logitem->cache_status = tkn;
    else
      free (tkn);
    break;
    /* remote hostname (IP only) */
  case 'h':
    if (logitem->host)
      return spec_err (logitem, SPEC_TOKN_SET, *p, NULL);
    if (!(tkn = parse_string (&(*str), end, 1)))
      return spec_err (logitem, SPEC_TOKN_NUL, *p, NULL);

    if (!conf.no_ip_validation && invalid_ipaddr (tkn, &logitem->type_ip)) {
      spec_err (logitem, SPEC_TOKN_INV, *p, tkn);
      free (tkn);
      return 1;
    }
    logitem->host = tkn;
    break;
    /* request method */
  case 'm':
    if (logitem->method)
      return spec_err (logitem, SPEC_TOKN_SET, *p, NULL);
    if (!(tkn = parse_string (&(*str), end, 1)))
      return spec_err (logitem, SPEC_TOKN_NUL, *p, NULL);

    if (!extract_method (tkn)) {
      spec_err (logitem, SPEC_TOKN_INV, *p, tkn);
      free (tkn);
      return 1;
    }
    logitem->method = tkn;
    break;
    /* request not including method or protocol */
  case 'U':
    if (logitem->req)
      return spec_err (logitem, SPEC_TOKN_SET, *p, NULL);
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL || *tkn == '\0')
      return spec_err (logitem, SPEC_TOKN_NUL, *p, NULL);

    if ((logitem->req = decode_url (tkn)) == NULL) {
      spec_err (logitem, SPEC_TOKN_INV, *p, tkn);
      free (tkn);
      return 1;
    }
    free (tkn);
    break;
    /* query string alone, e.g., ?param=goaccess&tbm=shop */
  case 'q':
    if (logitem->qstr)
      return spec_err (logitem, SPEC_TOKN_SET, *p, NULL);
    tkn = parse_string (&(*str), end, 1);
    if (tkn == NULL || *tkn == '\0')
      return 0;

    if ((logitem->qstr = decode_url (tkn)) == NULL) {
      spec_err (logitem, SPEC_TOKN_INV, *p, tkn);
      free (tkn);
      return 1;
    }
    free (tkn);
    break;
    /* request protocol */
  case 'H':
    if (logitem->protocol)
      return spec_err (logitem, SPEC_TOKN_SET, *p, NULL);
    if (!(tkn = parse_string (&(*str), end, 1)))
      return spec_err (logitem, SPEC_TOKN_NUL, *p, NULL);

    if (!extract_protocol (tkn)) {
      spec_err (logitem, SPEC_TOKN_INV, *p, tkn);
      free (tkn);
      return 1;
    }
    logitem->protocol = tkn;
    break;
    /* request, including method + protocol */
  case 'r':
    if (logitem->req)
      return spec_err (logitem, SPEC_TOKN_SET, *p, NULL);
    if (!(tkn = parse_string (&(*str), end, 1)))
      return spec_err (logitem, SPEC_TOKN_NUL, *p, NULL);

    logitem->req = parse_req (tkn, &logitem->method, &logitem->protocol);
    free (tkn);
    break;
    /* Status Code */
  case 's':
    if (logitem->status)
      return spec_err (logitem, SPEC_TOKN_SET, *p, NULL);
    if (!(tkn = parse_string (&(*str), end, 1)))
      return spec_err (logitem, SPEC_TOKN_NUL, *p, NULL);

    status = strtol (tkn, &sEnd, 10);
    if (tkn == sEnd || *sEnd != '\0' || errno == ERANGE || status < 100 || status > 599) {
      spec_err (logitem, SPEC_TOKN_INV, *p, tkn);
      free (tkn);
      return 1;
    }
    logitem->status = tkn;
    break;
    /* size of response in bytes - excluding HTTP headers */
  case 'b':
    if (logitem->resp_size)
      return spec_err (logitem, SPEC_TOKN_SET, *p, NULL);
    if (!(tkn = parse_string (&(*str), end, 1)))
      return spec_err (logitem, SPEC_TOKN_NUL, *p, NULL);

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
      return spec_err (logitem, SPEC_TOKN_SET, *p, NULL);

    if (!(tkn = parse_string (&(*str), end, 1)))
      tkn = alloc_string ("-");
    if (*tkn == '\0') {
      free (tkn);
      tkn = alloc_string ("-");
    }
    if (strcmp (tkn, "-") != 0) {
      extract_keyphrase (tkn, &logitem->keyphrase);
      extract_referer_site (tkn, logitem->site);

      /* hide referrers from report */
      if (hide_referer (logitem->site)) {
        logitem->site[0] = '\0';
        free (tkn);
      } else
        logitem->ref = tkn;
      break;
    }
    logitem->ref = tkn;

    break;
    /* user agent */
  case 'u':
    if (logitem->agent)
      return spec_err (logitem, SPEC_TOKN_SET, *p, NULL);

    tkn = parse_string (&(*str), end, 1);
    if (tkn != NULL && *tkn != '\0') {
      /* Make sure the user agent is decoded (i.e.: CloudFront)
       * and replace all '+' with ' ' (i.e.: w3c) */
      logitem->agent = decode_url (tkn);
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
      return 0;
    if (!(tkn = parse_string (&(*str), end, 1)))
      return spec_err (logitem, SPEC_TOKN_NUL, *p, NULL);

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
      return 0;
    if (!(tkn = parse_string (&(*str), end, 1)))
      return spec_err (logitem, SPEC_TOKN_NUL, *p, NULL);

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
      return 0;
    if (!(tkn = parse_string (&(*str), end, 1)))
      return spec_err (logitem, SPEC_TOKN_NUL, *p, NULL);

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

/* Parse the special host specifier and extract the characters that
 * need to be rejected when attempting to parse the XFF field.
 *
 * If no unable to find both curly braces (boundaries), NULL is returned.
 * On success, the malloc'd reject set is returned. */
static char *
extract_braces (char **p) {
  char *b1 = NULL, *b2 = NULL, *ret = NULL, *s = *p;
  int esc = 0;
  ptrdiff_t len = 0;

  /* iterate over the log format */
  for (; *s; s++) {
    if (*s == '\\') {
      esc = 1;
    } else if (*s == '{' && !esc) {
      b1 = s;
    } else if (*s == '}' && !esc) {
      b2 = s;
      break;
    } else {
      esc = 0;
    }
  }

  if ((!b1) || (!b2))
    return NULL;
  if ((len = b2 - (b1 + 1)) <= 0)
    return NULL;

  /* Found braces, extract 'reject' character set. */
  ret = xmalloc (len + 1);
  memcpy (ret, b1 + 1, len);
  ret[len] = '\0';
  (*p) = b2 + 1;

  return ret;
}

/* Attempt to extract the client IP from an X-Forwarded-For (XFF) field.
 *
 * If no IP is found, 1 is returned.
 * On success, the malloc'd token is assigned to a GLogItem->host and
 * 0 is returned. */
static int
find_xff_host (GLogItem * logitem, char **str, char **p) {
  char *ptr = NULL, *tkn = NULL, *skips = NULL;
  int invalid_ip = 1, len = 0, type_ip = TYPE_IPINV;

  if (!(skips = extract_braces (p)))
    return spec_err (logitem, SPEC_SFMT_MIS, **p, "{}");

  ptr = *str;
  while (*ptr != '\0') {
    if ((len = strcspn (ptr, skips)) == 0) {
      len++, ptr++;
      goto move;
    }

    ptr += len;
    /* extract possible IP */
    if (!(tkn = parsed_string (ptr, str, 0)))
      break;

    invalid_ip = invalid_ipaddr (tkn, &type_ip);
    /* done, already have IP and current token is not a host */
    if (logitem->host && invalid_ip) {
      free (tkn);
      break;
    }
    if (!logitem->host && !invalid_ip) {
      logitem->host = xstrdup (tkn);
      logitem->type_ip = type_ip;
    }
    free (tkn);

  move:
    *str += len;
  }

  free (skips);

  return logitem->host == NULL;
}

/* Handle special specifiers.
 *
 * On error, or unable to parse it, 1 is returned.
 * On success, the malloc'd token is assigned to a GLogItem member and
 * 0 is returned. */
static int
special_specifier (GLogItem * logitem, char **str, char **p) {
  switch (**p) {
    /* XFF remote hostname (IP only) */
  case 'h':
    if (logitem->host)
      return spec_err (logitem, SPEC_TOKN_SET, **p, NULL);
    if (find_xff_host (logitem, str, p))
      return spec_err (logitem, SPEC_TOKN_NUL, 'h', NULL);
    break;
  }

  return 0;
}

/* Iterate over the given log format.
 *
 * On error, or unable to parse it, 1 is returned.
 * On success, the malloc'd token is assigned to a GLogItem member and
 * 0 is returned. */
static int
parse_format (GLogItem * logitem, char *str) {
  char end[2 + 1] = { 0 };
  char *lfmt = conf.log_format, *p = NULL;
  int perc = 0, tilde = 0, optdelim = 0;

  if (str == NULL || *str == '\0')
    return 1;

  /* iterate over the log format */
  for (p = lfmt; *p; p++) {
    /* advance to the first unescaped delim */
    if (*p == '\\')
      continue;
    if (*p == '%') {
      perc++;
      continue;
    }
    if (*p == '~' && perc == 0) {
      tilde++;
      continue;
    }

    if (tilde && *p != '\0') {
      if ((str == NULL) || (*str == '\0'))
        return 0;
      if (special_specifier (logitem, &str, &p) == 1)
        return 1;
      tilde = 0;
    }
    /* %h */
    else if (perc && *p != '\0') {
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
      perc = 0;
    } else if (perc && isspace (p[0])) {
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
valid_line (char *line) {
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
lock_spinner (void) {
  if (parsing_spinner != NULL && parsing_spinner->state == SPN_RUN)
    pthread_mutex_lock (&parsing_spinner->mutex);
}

/* Determine if we need to unlock the mutex. */
static void
unlock_spinner (void) {
  if (parsing_spinner != NULL && parsing_spinner->state == SPN_RUN)
    pthread_mutex_unlock (&parsing_spinner->mutex);
}

/* Ignore request's query string. e.g.,
 * /index.php?timestamp=1454385289 */
static void
strip_qstring (char *req) {
  char *qmark;
  if ((qmark = strchr (req, '?')) != NULL) {
    if ((qmark - req) > 0)
      *qmark = '\0';
  }
}

/* Increment the overall bandwidth. */
static void
count_bw (int numdate, uint64_t resp_size) {
  ht_inc_cnt_bw (numdate, resp_size);
}

/* Output all log errors stored during parsing. */
void
output_logerrors (GLog * glog) {
  int i;

  if (!glog->log_erridx)
    return;

  fprintf (stderr, ERR_PARSED_NLINES, glog->log_erridx);
  fprintf (stderr, " %s:\n\n", ERR_PARSED_NLINES_DESC);

  for (i = 0; i < glog->log_erridx; ++i)
    fprintf (stderr, "%s\n", glog->errors[i]);
  fprintf (stderr, "\n%s\n", ERR_FORMAT_HEADER);
}

/* Free all log errors stored during parsing. */
void
free_logerrors (GLog * glog) {
  int i;

  if (!glog->log_erridx)
    return;

  for (i = 0; i < glog->log_erridx; ++i)
    free (glog->errors[i]);
  free (glog->errors);

  glog->errors = NULL;
  glog->log_erridx = 0;
}

/* Ensure we have the following fields. */
static int
verify_missing_fields (GLogItem * logitem) {
  /* must have the following fields */
  if (logitem->host == NULL)
    logitem->errstr = xstrdup ("IPv4/6 is required.");
  else if (logitem->date == NULL)
    logitem->errstr = xstrdup ("A valid date is required.");
  else if (logitem->req == NULL)
    logitem->errstr = xstrdup ("A request is required.");

  return logitem->errstr != NULL;
}

/* Keep track of all invalid log strings. */
static void
count_invalid (GLog * glog, const char *line) {
  glog->invalid++;
  ht_inc_cnt_overall ("failed_requests", 1);

  if (conf.invalid_requests_log) {
    LOG_INVALID (("%s", line));
  }

  if (glog->items->errstr && glog->invalid < MAX_LOG_ERRORS) {
    if (glog->errors == NULL)
      glog->errors = xcalloc (MAX_LOG_ERRORS, sizeof (char *));
    glog->errors[glog->log_erridx++] = xstrdup (glog->items->errstr);
  }
}

/* Count down the number of invalids hits.
 * Note: Upon performing a log test, invalid hits are counted, since
 * no valid records were found, then we count down by the number of
 * tests ran.
*/
static void
uncount_invalid (GLog * glog) {
  if (glog->invalid > conf.num_tests)
    glog->invalid -= conf.num_tests;
  else
    glog->invalid = 0;
}

/* Count down the number of processed hits.
 * Note: Upon performing a log test, processed hits are counted, since
 * no valid records were found, then we count down by the number of
 * tests ran.
*/
static void
uncount_processed (GLog * glog) {
  if (glog->processed > conf.num_tests)
    glog->processed -= conf.num_tests;
  else
    glog->processed = 0;
}

/* Keep track of all valid log strings. */
static void
count_valid (int numdate) {
  lock_spinner ();
  ht_inc_cnt_valid (numdate, 1);
  unlock_spinner ();
}

/* Keep track of all valid and processed log strings. */
static void
count_process (GLog * glog) {
  lock_spinner ();
  glog->processed++;
  ht_inc_cnt_overall ("total_requests", 1);
  unlock_spinner ();
}

/* Keep track of all excluded log strings (IPs).
 *
 * If IP not range, 1 is returned.
 * If IP is excluded, 0 is returned. */
static int
excluded_ip (GLogItem * logitem) {
  if (conf.ignore_ip_idx && ip_in_range (logitem->host)) {
    ht_inc_cnt_overall ("excluded_ip", 1);
    return 0;
  }
  return 1;
}

/* Determine if the request is from a robot or spider and check if we
 * need to ignore or show crawlers only.
 *
 * If the request line is not ignored, 0 is returned.
 * If the request line is ignored, 1 is returned. */
static int
handle_crawler (const char *agent) {
  int bot = 0;

  if (!conf.ignore_crawlers && !conf.crawlers_only)
    return 1;

  bot = is_crawler (agent);
  return (conf.ignore_crawlers && bot) || (conf.crawlers_only && !bot) ? 0 : 1;
}

/* A wrapper function to determine if the request is static.
 *
 * If the request is not static, 0 is returned.
 * If the request is static, 1 is returned. */
static int
is_static (const char *req) {
  return verify_static_content (req);
}

/* Determine if the request of the given status code needs to be
 * ignored.
 *
 * If the status code is not within the ignore-array, 0 is returned.
 * If the status code is within the ignore-array, 1 is returned. */
static int
ignore_status_code (const char *status) {
  if (conf.ignore_status_idx == 0)
    return 0;

  if (str_inarray (status, conf.ignore_status, conf.ignore_status_idx) != -1)
    return 1;
  return 0;
}

/* Determine if static file request should be ignored
    *
    * If the request line is not ignored, 0 is returned.
    * If the request line is ignored, 1 is returned. */
static int
ignore_static (const char *req) {
  if (conf.ignore_statics && is_static (req))
    return 1;
  return 0;
}

/* Determine if the request status code is a 404.
 *
 * If the request is not a 404, 0 is returned.
 * If the request is a 404, 1 is returned. */
static int
is_404 (GLogItem * logitem) {
  /* is this a 404? */
  if (logitem->status && !memcmp (logitem->status, "404", 3))
    return 1;
  /* treat 444 as 404? */
  else if (logitem->status && !memcmp (logitem->status, "444", 3) && conf.code444_as_404)
    return 1;
  return 0;
}

/* A wrapper function to determine if a log line needs to be ignored.
 *
 * If the request line is not ignored, 0 is returned.
 * If the request line is ignored, IGNORE_LEVEL_PANEL is returned.
 * If the request line is only not counted as valid, IGNORE_LEVEL_REQ is returned. */
static int
ignore_line (GLogItem * logitem) {
  if (excluded_ip (logitem) == 0)
    return IGNORE_LEVEL_PANEL;
  if (handle_crawler (logitem->agent) == 0)
    return IGNORE_LEVEL_PANEL;
  if (ignore_referer (logitem->ref))
    return IGNORE_LEVEL_PANEL;
  if (ignore_status_code (logitem->status))
    return IGNORE_LEVEL_PANEL;
  if (ignore_static (logitem->req))
    return conf.ignore_statics; // IGNORE_LEVEL_PANEL or IGNORE_LEVEL_REQ

  /* check if we need to remove the request's query string */
  if (conf.ignore_qstr)
    strip_qstring (logitem->req);

  return 0;
}

/* A wrapper function to insert a data keymap string key.
 *
 * If the given key exists, its value is returned.
 * On error, -1 is returned.
 * On success the value of the key inserted is returned */
static int
insert_dkeymap (GModule module, GKeyData * kdata) {
  return ht_insert_keymap (module, kdata->numdate, kdata->data_key, &kdata->cdnkey);
}

/* A wrapper function to insert a root keymap string key.
 *
 * If the given key exists, its value is returned.
 * On error, -1 is returned.
 * On success the value of the key inserted is returned */
static int
insert_rkeymap (GModule module, GKeyData * kdata) {
  return ht_insert_keymap (module, kdata->numdate, kdata->root_key, &kdata->crnkey);
}

/* A wrapper function to insert a datamap uint32_t key and string value. */
static void
insert_data (GModule module, GKeyData * kdata) {
  ht_insert_datamap (module, kdata->numdate, kdata->data_nkey, kdata->data, kdata->cdnkey);
}

/* A wrapper function to insert a uniqmap string key.
 *
 * If the given key exists, 0 is returned.
 * On error, -1 is returned.
 * On success the value of the key inserted is returned */
static int
insert_uniqmap (GModule module, GKeyData * kdata, uint32_t uniq_nkey) {
  return ht_insert_uniqmap (module, kdata->numdate, kdata->data_nkey, uniq_nkey);
}

/* A wrapper function to insert a rootmap uint32_t key from the keymap
 * store mapped to its string value. */
static void
insert_rootmap (GModule module, GKeyData * kdata) {
  ht_insert_rootmap (module, kdata->numdate, kdata->root_nkey, kdata->root, kdata->crnkey);
}

/* A wrapper function to insert a data uint32_t key mapped to the
 * corresponding uint32_t root key. */
static void
insert_root (GModule module, GKeyData * kdata) {
  ht_insert_root (module, kdata->numdate, kdata->data_nkey, kdata->root_nkey, kdata->cdnkey,
                  kdata->crnkey);
}

/* A wrapper function to increase hits counter from an uint32_t key. */
static void
insert_hit (GModule module, GKeyData * kdata) {
  ht_insert_hits (module, kdata->numdate, kdata->data_nkey, 1, kdata->cdnkey);
  ht_insert_meta_data (module, kdata->numdate, "hits", 1);
}

/* A wrapper function to increase visitors counter from an uint32_t
 * key. */
static void
insert_visitor (GModule module, GKeyData * kdata) {
  ht_insert_visitor (module, kdata->numdate, kdata->data_nkey, 1, kdata->cdnkey);
  ht_insert_meta_data (module, kdata->numdate, "visitors", 1);
}

/* A wrapper function to increases bandwidth counter from an uint32_t
 * key. */
static void
insert_bw (GModule module, GKeyData * kdata, uint64_t size) {
  ht_insert_bw (module, kdata->numdate, kdata->data_nkey, size, kdata->cdnkey);
  ht_insert_meta_data (module, kdata->numdate, "bytes", size);
}

/* A wrapper call to increases cumulative time served counter
 * from an uint32_t key. */
static void
insert_cumts (GModule module, GKeyData * kdata, uint64_t ts) {
  ht_insert_cumts (module, kdata->numdate, kdata->data_nkey, ts, kdata->cdnkey);
  ht_insert_meta_data (module, kdata->numdate, "cumts", ts);
}

/* A wrapper call to insert the maximum time served counter from
 * an uint32_t key. */
static void
insert_maxts (GModule module, GKeyData * kdata, uint64_t ts) {
  ht_insert_maxts (module, kdata->numdate, kdata->data_nkey, ts, kdata->cdnkey);
  ht_insert_meta_data (module, kdata->numdate, "maxts", ts);
}

static void
insert_method (GModule module, GKeyData * kdata, const char *data) {
  ht_insert_method (module, kdata->numdate, kdata->data_nkey, data ? data : "---",
                    kdata->cdnkey);
}

/* A wrapper call to insert a method given an uint32_t key and string
 * value. */
static void
insert_protocol (GModule module, GKeyData * kdata, const char *data) {
  ht_insert_protocol (module, kdata->numdate, kdata->data_nkey, data ? data : "---",
                      kdata->cdnkey);
}

/* A wrapper call to insert an agent for a hostname given an uint32_t
 * key and uint32_t value.  */
static void
insert_agent (GModule module, GKeyData * kdata, uint32_t agent_nkey) {
  ht_insert_agent (module, kdata->numdate, kdata->data_nkey, agent_nkey);
}

/* The following generates a unique key to identity unique visitors.
 * The key is made out of the IP, date, and user agent.
 * Note that for readability, doing a simple snprintf/sprintf should
 * suffice, however, memcpy is the fastest solution
 *
 * On success the new unique visitor key is returned */
static char *
get_uniq_visitor_key (GLogItem * logitem) {
  char *ua = NULL, *key = NULL;
  size_t s1, s2, s3;

  ua = deblank (xstrdup (logitem->agent));

  s1 = strlen (logitem->date);
  s2 = strlen (logitem->host);
  s3 = strlen (ua);

  /* includes terminating null */
  key = xcalloc (s1 + s2 + s3 + 3, sizeof (char));

  memcpy (key, logitem->date, s1);

  key[s1] = '|';
  memcpy (key + s1 + 1, logitem->host, s2 + 1);

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
gen_unique_req_key (GLogItem * logitem) {
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
  key = xcalloc (s1 + s2 + s3 + nul, sizeof (char));
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
append_query_string (char **req, const char *qstr) {
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
get_kdata (GKeyData * kdata, char *data_key, char *data) {
  /* inserted in keymap */
  kdata->data_key = data_key;
  /* inserted in datamap */
  kdata->data = data;
}

/* Generate a visitor's key given the date specificity. For instance,
 * if the specificity if set to hours, then a generated key would
 * look like: 03/Jan/2016:09 */
static void
set_spec_visitor_key (char **fdate, const char *ftime) {
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
 * structure and assign it to the output key data structure.
 *
 * On error, or if no date is found, 1 is returned.
 * On success, the date key is assigned to our key data structure.
 */
static int
gen_visitor_key (GKeyData * kdata, GLogItem * logitem) {
  if (!logitem->date || !logitem->time)
    return 1;

  /* Append time specificity to date */
  if (conf.date_spec_hr)
    set_spec_visitor_key (&logitem->date, logitem->time);

  get_kdata (kdata, logitem->date, logitem->date);
  kdata->numdate = logitem->numdate;

  return 0;
}

/* Generate a unique key for the requests panel from the given logitem
 * structure and assign it to out key data structure.
 *
 * On success, the generated request key is assigned to our key data
 * structure.
 */
static int
gen_req_key (GKeyData * kdata, GLogItem * logitem) {
  if (logitem->req && logitem->qstr)
    append_query_string (&logitem->req, logitem->qstr);
  logitem->req_key = gen_unique_req_key (logitem);

  get_kdata (kdata, logitem->req_key, logitem->req);
  kdata->numdate = logitem->numdate;

  return 0;
}

/* A wrapper to generate a unique key for the request panel.
 *
 * On error, or if the request is static or a 404, 1 is returned.
 * On success, the generated request key is assigned to our key data
 * structure.
 */
static int
gen_request_key (GKeyData * kdata, GLogItem * logitem) {
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
gen_404_key (GKeyData * kdata, GLogItem * logitem) {
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
gen_static_request_key (GKeyData * kdata, GLogItem * logitem) {
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
gen_vhost_key (GKeyData * kdata, GLogItem * logitem) {
  if (!logitem->vhost)
    return 1;

  get_kdata (kdata, logitem->vhost, logitem->vhost);
  kdata->numdate = logitem->numdate;

  return 0;
}

/* A wrapper to generate a unique key for the virtual host panel.
 *
 * On error, 1 is returned.
 * On success, the generated userid key is assigned to our key data
 * structure. */
static int
gen_remote_user_key (GKeyData * kdata, GLogItem * logitem) {
  if (!logitem->userid)
    return 1;

  get_kdata (kdata, logitem->userid, logitem->userid);
  kdata->numdate = logitem->numdate;

  return 0;
}

/* A wrapper to generate a unique key for the cache status panel.
 *
 * On error, 1 is returned.
 * On success, the generated cache status key is assigned to our key data
 * structure. */
static int
gen_cache_status_key (GKeyData * kdata, GLogItem * logitem) {
  if (!logitem->cache_status)
    return 1;

  get_kdata (kdata, logitem->cache_status, logitem->cache_status);
  kdata->numdate = logitem->numdate;

  return 0;
}

/* A wrapper to generate a unique key for the hosts panel.
 *
 * On error, 1 is returned.
 * On success, the generated host key is assigned to our key data
 * structure. */
static int
gen_host_key (GKeyData * kdata, GLogItem * logitem) {
  if (!logitem->host)
    return 1;

  get_kdata (kdata, logitem->host, logitem->host);
  kdata->numdate = logitem->numdate;

  return 0;
}

/* Generate a browser unique key for the browser's panel given a user
 * agent and assign the browser type/category as a root element.
 *
 * On error, 1 is returned.
 * On success, the generated browser key is assigned to our key data
 * structure. */
static int
gen_browser_key (GKeyData * kdata, GLogItem * logitem) {
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
  kdata->numdate = logitem->numdate;

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
gen_os_key (GKeyData * kdata, GLogItem * logitem) {
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
  kdata->numdate = logitem->numdate;

  free (agent);

  return 0;
}

/* A wrapper to generate a unique key for the referrers panel.
 *
 * On error, 1 is returned.
 * On success, the generated referrer key is assigned to our key data
 * structure. */
static int
gen_referer_key (GKeyData * kdata, GLogItem * logitem) {
  if (!logitem->ref)
    return 1;

  get_kdata (kdata, logitem->ref, logitem->ref);
  kdata->numdate = logitem->numdate;

  return 0;
}

/* A wrapper to generate a unique key for the referring sites panel.
 *
 * On error, 1 is returned.
 * On success, the generated referring site key is assigned to our key data
 * structure. */
static int
gen_ref_site_key (GKeyData * kdata, GLogItem * logitem) {
  if (logitem->site[0] == '\0')
    return 1;

  get_kdata (kdata, logitem->site, logitem->site);
  kdata->numdate = logitem->numdate;

  return 0;
}

/* A wrapper to generate a unique key for the keyphrases panel.
 *
 * On error, 1 is returned.
 * On success, the generated keyphrase key is assigned to our key data
 * structure. */
static int
gen_keyphrase_key (GKeyData * kdata, GLogItem * logitem) {
  if (!logitem->keyphrase)
    return 1;

  get_kdata (kdata, logitem->keyphrase, logitem->keyphrase);
  kdata->numdate = logitem->numdate;

  return 0;
}

/* A wrapper to generate a unique key for the geolocation panel.
 *
 * On error, 1 is returned.
 * On success, the generated geolocation key is assigned to our key
 * data structure. */
#ifdef HAVE_GEOLOCATION
static int
gen_geolocation_key (GKeyData * kdata, GLogItem * logitem) {
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
  kdata->numdate = logitem->numdate;

  return 0;
}
#endif

/* A wrapper to generate a unique key for the status code panel.
 *
 * On error, 1 is returned.
 * On success, the generated status code key is assigned to our key
 * data structure. */
static int
gen_status_code_key (GKeyData * kdata, GLogItem * logitem) {
  const char *status = NULL, *type = NULL;

  if (!logitem->status)
    return 1;

  type = verify_status_code_type (logitem->status);
  status = verify_status_code (logitem->status);

  kdata->data = (char *) status;
  kdata->data_key = (char *) status;

  kdata->root = (char *) type;
  kdata->root_key = (char *) type;
  kdata->numdate = logitem->numdate;

  return 0;
}

/* Given a time string containing at least %H:%M, extract either the
 * tenth of a minute or an hour.
 *
 * On error, the given string is not modified.
 * On success, the conf specificity is extracted. */
static void
parse_time_specificity_string (char *hmark, char *ftime) {
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
gen_visit_time_key (GKeyData * kdata, GLogItem * logitem) {
  char *hmark = NULL;
  char hour[HRMI_LEN] = "";     /* %H:%M */
  if (!logitem->time)
    return 1;

  /* if not a timestamp, then it must be a string containing the hour.
   * this is faster than actual date conversion */
  if (!has_timestamp (conf.time_format) && (hmark = strchr (logitem->time, ':'))) {
    parse_time_specificity_string (hmark, logitem->time);

    kdata->numdate = logitem->numdate;
    get_kdata (kdata, logitem->time, logitem->time);
    return 0;
  }

  /* otherwise it attempts to convert the date given a time format,
   * though this is slower */
  memset (hour, 0, sizeof *hour);
  if (convert_date (hour, logitem->time, "%T", "%H:%M", HRMI_LEN) != 0)
    return 1;

  if (*hour == '\0')
    return 1;

  if ((hmark = strchr (hour, ':')))
    parse_time_specificity_string (hmark, hour);

  free (logitem->time);
  logitem->time = xstrdup (hour);

  get_kdata (kdata, logitem->time, logitem->time);
  kdata->numdate = logitem->numdate;

  return 0;
}

/* Determine if 404s need to be added to the unique visitors count.
 *
 * If it needs to be added, 0 is returned else 1 is returned. */
static int
include_uniq (GLogItem * logitem) {
  int u = conf.client_err_to_unique_count;

  if (!logitem->status || logitem->status[0] != '4' || (u && logitem->status[0] == '4'))
    return 1;
  return 0;
}

/* Determine which data metrics need to be set and set them. */
static void
set_datamap (GLogItem * logitem, GKeyData * kdata, const GParse * parse) {
  GModule module;
  module = parse->module;

  /* insert data */
  parse->datamap (module, kdata);

  /* insert rootmap and root-data map */
  if (parse->rootmap) {
    parse->rootmap (module, kdata);
    insert_root (module, kdata);
  }
  /* insert hits */
  if (parse->hits)
    parse->hits (module, kdata);
  /* insert visitors */
  if (parse->visitor && kdata->uniq_nkey == 1)
    parse->visitor (module, kdata);
  /* insert bandwidth */
  if (parse->bw)
    parse->bw (module, kdata, logitem->resp_size);
  /* insert averages time served */
  if (parse->cumts)
    parse->cumts (module, kdata, logitem->serve_time);
  /* insert averages time served */
  if (parse->maxts)
    parse->maxts (module, kdata, logitem->serve_time);
  /* insert method */
  if (parse->method && conf.append_method)
    parse->method (module, kdata, logitem->method);
  /* insert protocol */
  if (parse->protocol && conf.append_protocol)
    parse->protocol (module, kdata, logitem->protocol);
  /* insert agent */
  if (parse->agent && conf.list_agents)
    parse->agent (module, kdata, logitem->agent_nkey);
}

/* Set data mapping and metrics. */
static void
map_log (GLogItem * logitem, const GParse * parse, GModule module) {
  GKeyData kdata;

  new_modulekey (&kdata);
  /* set key data into out structure */
  if (parse->key_data (&kdata, logitem) == 1)
    return;

  /* each module requires a data key/value */
  if (parse->datamap && kdata.data_key)
    kdata.data_nkey = insert_dkeymap (module, &kdata);

  /* each module contains a uniq visitor key/value */
  if (parse->visitor && logitem->uniq_key && include_uniq (logitem))
    kdata.uniq_nkey = insert_uniqmap (module, &kdata, logitem->uniq_nkey);

  /* root keys are optional */
  if (parse->rootmap && kdata.root_key)
    kdata.root_nkey = insert_rkeymap (module, &kdata);

  /* each module requires a root key/value */
  if (parse->datamap && kdata.data_key)
    set_datamap (logitem, &kdata, parse);
}

static void
ins_agent_key_val (GLogItem * logitem, uint32_t numdate) {
  logitem->agent_nkey = ht_insert_agent_key (numdate, logitem->agent);
  /* insert UA key and get a numeric value */
  if (logitem->agent_nkey != 0) {
    /* insert a numeric key and map it to a UA string */
    ht_insert_agent_value (numdate, logitem->agent_nkey, logitem->agent);
  }
}

static int
clean_old_data_by_date (uint32_t numdate) {
  uint32_t *dates = NULL;
  uint32_t idx, len = 0;

  if (ht_get_size_dates () < conf.keep_last)
    return 1;

  dates = get_sorted_dates (&len);

  /* If currently parsed date is in the set of dates, keep inserting it.
   * We count down since more likely the currently parsed date is at the last pos */
  for (idx = len; idx-- > 0;) {
    if (dates[idx] == numdate) {
      free (dates);
      return 1;
    }
  }

  /* ignore older dates */
  if (dates[0] > numdate) {
    free (dates);
    return -1;
  }

  /* invalidate the first date we inserted then */
  invalidate_date (dates[0]);
  /* rebuild all existing dates and let new data
   * be added upon existing cache */
  rebuild_rawdata_cache ();
  free (dates);

  return 0;
}

/* Process a log line and set the data into the corresponding data
 * structure. */
static void
process_log (GLogItem * logitem) {
  GModule module;
  const GParse *parse = NULL;
  size_t idx = 0;
  uint32_t numdate = logitem->numdate;

  if (conf.keep_last > 0 && clean_old_data_by_date (numdate) == -1)
    return;

  /* insert date and start partitioning tables */
  if (ht_insert_date (numdate) == -1)
    return;

  /* Insert one unique visitor key per request to avoid the
   * overhead of storing one key per module */
  if ((logitem->uniq_nkey = ht_insert_unique_key (numdate, logitem->uniq_key)) == 0)
    return;

  /* If we need to store user agents per IP, then we store them and retrieve
   * its numeric key.
   * It maintains two maps, one for key -> value, and another
   * map for value -> key*/
  if (conf.list_agents)
    ins_agent_key_val (logitem, numdate);

  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    if (!(parse = panel_lookup (module)))
      continue;
    map_log (logitem, parse, module);
  }

  count_bw (numdate, logitem->resp_size);
  /* don't ignore line but neither count as valid */
  if (logitem->ignorelevel != IGNORE_LEVEL_REQ)
    count_valid (numdate);
}

/* Determine if we are restoring from disk or should continue to parse requests */
static int
should_restore_from_disk (GLog * glog) {
  GLastParse lp = { 0 };

  if (!conf.restore)
    return 0;

  lp = ht_get_last_parse (glog->inode);

  /* restoring from disk, and no inode (probably a pipe), compare timestamps then
   * (exclusive) */
  if (!glog->inode && lp.ts >= glog->lp.ts)
    return 1;

  /* no inode nor last parse size, continue parsing */
  if (!glog->inode || !lp.size)
    return 0;

  /* If timestamp is greater than last parsed, read the line then */
  if (glog->lp.ts > lp.ts)
    return 0;
  /* Check if current log size is smaller than the one last parsed, if it is,
   * it was possibly truncated and thus it may be smaller, so fallback to
   * timestamp even if they are equal to the last parsed timestamp */
  else if (glog->size < lp.size && glog->lp.ts == lp.ts)
    return 0;
  /* If our current line is greater or equal (zero indexed) to the last parsed
   * line and have equal timestamps, then keep parsing then */
  else if (glog->size > lp.size && glog->read >= lp.line && glog->lp.ts == lp.ts)
    return 0;
  /* Everything else we ignore it. For instance, we check if current log size is
   * greater than the one last parsed, if the timestamp are equal, we ignore the
   * request.
   *
   * **NOTE* We try to play safe here as we would rather miss a few lines
   * than double-count a few. */
  return 1;
}

/* Process a line from the log and store it accordingly taking into
 * account multiple parsing options prior to setting data into the
 * corresponding data structure.
 *
 * On success, 0 is returned */
static int
pre_process_log (GLog * glog, char *line, int dry_run) {
  GLogItem *logitem;
  int ret = 0;

  /* soft ignore these lines */
  if (valid_line (line))
    return -1;

  logitem = init_log_item (glog);
  /* Parse a line of log, and fill structure with appropriate values */
  if (parse_format (logitem, line) || verify_missing_fields (logitem)) {
    ret = 1;
    count_process (glog);
    count_invalid (glog, line);
    goto cleanup;
  }

  glog->lp.ts = mktime (&logitem->dt);

  if (should_restore_from_disk (glog))
    goto cleanup;

  count_process (glog);

  /* agent will be null in cases where %u is not specified */
  if (logitem->agent == NULL)
    logitem->agent = alloc_string ("-");

  /* testing log only */
  if (dry_run)
    goto cleanup;

  logitem->ignorelevel = ignore_line (logitem);
  /* ignore line */
  if (logitem->ignorelevel == IGNORE_LEVEL_PANEL)
    goto cleanup;

  if (is_404 (logitem))
    logitem->is_404 = 1;
  else if (is_static (logitem->req))
    logitem->is_static = 1;

  logitem->uniq_key = get_uniq_visitor_key (logitem);

  process_log (logitem);

cleanup:
  free_glog (logitem);

  ht_insert_last_parse (0, glog->lp);

  return ret;
}

/* Entry point to process the given live from the log.
 *
 * On error, 1 is returned.
 * On success or soft ignores, 0 is returned. */
static int
read_line (GLog * glog, char *line, int *test, int *cnt, int dry_run) {
  int ret = 0;

  /* start processing log line */
  if ((ret = pre_process_log (glog, line, dry_run)) == 0 && *test)
    *test = 0;

  /* soft ignores */
  if (ret == -1)
    return 0;

  /* reached num of lines to test and no valid records were found, log
   * format is likely not matching */
  if (conf.num_tests && ++(*cnt) == (int) conf.num_tests && *test) {
    uncount_processed (glog);
    uncount_invalid (glog);
    return 1;
  }

  return 0;
}

/* A replacement for GNU getline() to dynamically expand fgets buffer.
 *
 * On error, NULL is returned.
 * On success, the malloc'd line is returned. */
char *
fgetline (FILE * fp) {
  char buf[LINE_BUFFER] = { 0 };
  char *line = NULL, *tmp = NULL;
  size_t linelen = 0, len = 0;

  while (1) {
    if (!fgets (buf, sizeof (buf), fp)) {
      if (conf.process_and_exit && errno == EAGAIN) {
        nanosleep ((const struct timespec[]) { {0, 100000000L} }, NULL);
        continue;
      } else
        break;
    }

    len = strlen (buf);

    /* overflow check */
    if (SIZE_MAX - len - 1 < linelen)
      break;

    if ((tmp = realloc (line, linelen + len + 1)) == NULL)
      break;

    line = tmp;
    /* append */
    strcpy (line + linelen, buf);
    linelen += len;

    if (feof (fp) || buf[len - 1] == '\n')
      return line;
  }
  free (line);

  return NULL;
}

/* Iterate over the log and read line by line (use GNU get_line to parse the
 * whole line).
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
#ifdef WITH_GETLINE
static int
read_lines (FILE * fp, GLog ** glog, int dry_run) {
  char *line = NULL;
  int ret = 0, cnt = 0, test = conf.num_tests > 0 ? 1 : 0;

  (*glog)->bytes = 0;
  while ((line = fgetline (fp)) != NULL) {
    /* handle SIGINT */
    if (conf.stop_processing)
      goto out;
    if ((ret = read_line ((*glog), line, &test, &cnt, dry_run)))
      goto out;
    if (dry_run && NUM_TESTS == cnt)
      goto out;
    (*glog)->bytes += strlen (line);
    free (line);
    (*glog)->read++;
  }

  /* if no data was available to read from (probably from a pipe) and
   * still in test mode, we simply return until data becomes available */
  if (!line && (errno == EAGAIN || errno == EWOULDBLOCK) && test)
    return 0;

  return (line && test) || ret || (!line && test && (*glog)->processed);

out:
  free (line);
  /* fails if
     - we're still reading the log but the test flag was still set
     - ret flag is not 0, read_line failed
     - reached the end of file, test flag was still set and we processed lines */
  return test || ret || (test && (*glog)->processed);
}
#endif

/* Iterate over the log and read line by line (uses a buffer of fixed size).
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
#ifndef WITH_GETLINE
static int
read_lines (FILE * fp, GLog ** glog, int dry_run) {
  char *s = NULL;
  char line[LINE_BUFFER] = { 0 };
  int ret = 0, cnt = 0, test = conf.num_tests > 0 ? 1 : 0;

  (*glog)->bytes = 0;
  while ((s = fgets (line, LINE_BUFFER, fp)) != NULL) {
    /* handle SIGINT */
    if (conf.stop_processing)
      break;
    if ((ret = read_line ((*glog), line, &test, &cnt, dry_run)))
      break;
    if (dry_run && NUM_TESTS == cnt)
      break;
    (*glog)->bytes += strlen (line);
    (*glog)->read++;
  }

  /* if no data was available to read from (probably from a pipe) and
   * still in test mode, we simply return until data becomes available */
  if (!s && (errno == EAGAIN || errno == EWOULDBLOCK) && test)
    return 0;

  /* fails if
     - we're still reading the log but the test flag was still set
     - ret flag is not 0, read_line failed
     - reached the end of file, test flag was still set and we processed lines */
  return (s && test) || ret || (!s && test && (*glog)->processed);
}
#endif

/* Read the given log line by line and process its data.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
static int
read_log (GLog ** glog, const char *fn, int dry_run) {
  FILE *fp = NULL;
  int piping = 0;
  struct stat fdstat;

  /* Ensure we have a valid pipe to read from stdin. Only checking for
   * conf.read_stdin without verifying for a valid FILE pointer would certainly
   * lead to issues. */
  if (fn[0] == '-' && fn[1] == '\0' && (*glog)->pipe) {
    fp = (*glog)->pipe;
    (*glog)->piping = piping = 1;
  }

  /* make sure we can open the log (if not reading from stdin) */
  if (!piping && (fp = fopen (fn, "r")) == NULL)
    FATAL ("Unable to open the specified log file. %s", strerror (errno));

  /* grab the inode of the file being parsed */
  if (!piping && stat (fn, &fdstat) == 0) {
    (*glog)->inode = fdstat.st_ino;
    (*glog)->size = (*glog)->lp.size = fdstat.st_size;
  }

  /* read line by line */
  if (read_lines (fp, glog, dry_run)) {
    if (!piping)
      fclose (fp);
    return 1;
  }

  /* insert the inode of the file parsed and the last line parsed */
  if ((*glog)->inode) {
    (*glog)->lp.line = (*glog)->read;
    ht_insert_last_parse ((*glog)->inode, (*glog)->lp);
  }

  /* close log file if not a pipe */
  if (!piping)
    fclose (fp);

  return 0;
}

/* Entry point to parse the log line by line.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
int
parse_log (GLog ** glog, char *tail, int dry_run) {
  const char *err_log = NULL;
  int i;

  /* no data piped, no logs passed, load from disk only then */
  if (conf.restore && !(*glog)->restored)
    (*glog)->restored = rebuild_rawdata_cache ();

  /* process tail data and return */
  if (tail != NULL) {
    /* no line testing on tail */
    if (pre_process_log ((*glog), tail, dry_run))
      return 1;
    return 0;
  }

  /* verify that we have the required formats */
  if ((err_log = verify_formats ()))
    FATAL ("%s", err_log);

  /* no data piped, no logs passed, load from disk only then */
  if (conf.restore && !conf.filenames_idx && !conf.read_stdin) {
    (*glog)->load_from_disk_only = 1;
    return 0;
  }

  for (i = 0; i < conf.filenames_idx; ++i) {
    if (read_log (glog, conf.filenames[i], dry_run)) {
      fprintf (stderr, "%s\n", conf.filenames[i]);
      return 1;
    }
    (*glog)->filesizes[i] = (*glog)->bytes;
  }

  return 0;
}

/* Ensure we have valid hits
 *
 * On error, an array of pointers containing the error strings.
 * On success, NULL is returned. */
char **
test_format (GLog * glog, int *len) {
  char **errors = NULL;
  int i;

  if (parse_log (&glog, NULL, 1) == 0)
    goto clean;

  errors = xcalloc (glog->log_erridx, sizeof (char *));
  for (i = 0; i < glog->log_erridx; ++i)
    errors[i] = xstrdup (glog->errors[i]);
  *len = glog->log_erridx;

clean:
  free_logerrors (glog);

  return errors;
}
