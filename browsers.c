/**
 * browsers.c -- functions for dealing with browsers
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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "browsers.h"

#include "util.h"
#include "error.h"
#include "xmalloc.h"

/* {"search string", "belongs to"} */
static const char *browsers[][2] = {
  /* browsers & offline browsers */
  {"Avant Browser", "Others"},
  {"America Online Browser", "Others"},
  {"MSIE", "MSIE"},
  {"Trident/7.0", "MSIE"},
  {"Flock", "Others"},
  {"Epiphany", "Others"},
  {"SeaMonkey", "Others"},
  {"Iceweasel", "Others"},
  {"Minefield", "Others"},
  {"GranParadiso", "Others"},
  {"YaBrowser", "Others"},
  {"Firefox", "Firefox"},
  {"Opera Mini", "Opera"},
  {"Opera", "Opera"},
  {"OPR", "Opera"},
  {"Netscape", "Others"},
  {"Konqueror", "Others"},
  {"Wget", "Others"},
  {"w3m", "Others"},
  {"ELinks", "Others"},
  {"Links", "Others"},
  {"Lynx", "Others"},
  {"curl", "Others"},
  {"Camino", "Others"},
  {"Dillo", "Others"},
  {"Kazehakase", "Others"},
  {"Iceape", "Others"},
  {"K-Meleon", "Others"},
  {"Galeon", "Others"},
  {"BrowserX", "Others"},
  {"IBrowse", "Others"},
  {"Mosaic", "Others"},
  {"midori", "Others"},
  {"Midori", "Others"},
  {"Firebird", "Others"},
  {"BlackBerry", "Others"},
  {"HUAWEI", "Others"},

  /* Chrome has to go before Safari */
  {"Chrome", "Chrome"},
  {"Safari", "Safari"},

  {"Flipboard", "Crawlers"},
  {"Feed", "Crawlers"},
  {"AdsBot-Google", "Crawlers"},
  {"Mediapartners-Google", "Crawlers"},
  {"Google", "Crawlers"},
  {"bingbot", "Crawlers"},
  {"msnbot", "Crawlers"},
  {"Yandex", "Crawlers"},
  {"Baidu", "Crawlers"},
  {"Ezooms", "Crawlers"},
  {"Twitter", "Crawlers"},
  {"Slurp", "Crawlers"},
  {"Yahoo", "Crawlers"},
  {"AhrefsBot", "Crawlers"},
  {"MJ12bot", "Crawlers"},
  {"SISTRIX", "Crawlers"},
  {"facebook", "Crawlers"},
  {"DotBot", "Crawlers"},
  {"Speedy Spider", "Crawlers"},
  {"Sosospider", "Crawlers"},
  {"BPImageWalker", "Crawlers"},
  {"Sogou", "Crawlers"},
  {"Java", "Crawlers"},
  {"Jakarta Commons-HttpClient", "Crawlers"},
  {"WBSearchBot", "Crawlers"},
  {"SeznamBot", "Crawlers"},
  {"DoCoMo", "Crawlers"},
  {"TurnitinBot", "Crawlers"},
  {"GSLFbot", "Crawlers"},
  {"YodaoBot", "Crawlers"},
  {"AddThis", "Crawlers"},
  {"Apple-PubSub", "Crawlers"},
  {"Purebot", "Crawlers"},
  {"ia_archiver", "Crawlers"},
  {"Wotbox", "Crawlers"},
  {"CCBot", "Crawlers"},
  {"findlinks", "Crawlers"},
  {"Yeti", "Crawlers"},
  {"ichiro", "Crawlers"},
  {"Linguee Bot", "Crawlers"},
  {"Gigabot", "Crawlers"},
  {"BacklinkCrawler", "Crawlers"},
  {"netEstate", "Crawlers"},
  {"distilator", "Crawlers"},
  {"Aboundex", "Crawlers"},
  {"UnwindFetchor", "Crawlers"},
  {"SEOkicks-Robot", "Crawlers"},
  {"psbot", "Crawlers"},
  {"SBIder", "Crawlers"},
  {"TestNutch", "Crawlers"},
  {"DomainCrawler", "Crawlers"},
  {"NextGenSearchBot", "Crawlers"},
  {"SEOENGWorldBot", "Crawlers"},
  {"PiplBot", "Crawlers"},
  {"IstellaBot", "Crawlers"},
  {"Cityreview", "Crawlers"},
  {"heritrix", "Crawlers"},
  {"PagePeeker", "Crawlers"},
  {"JS-Kit", "Crawlers"},
  {"ScreenerBot", "Crawlers"},
  {"PagesInventory", "Crawlers"},
  {"ShowyouBot", "Crawlers"},
  {"SolomonoBot", "Crawlers"},
  {"rogerbot", "Crawlers"},
  {"fastbot", "Crawlers"},
  {"Domnutch", "Crawlers"},
  {"MaxPoint", "Crawlers"},
  {"NCBot", "Crawlers"},
  {"TosCrawler", "Crawlers"},
  {"Updownerbot", "Crawlers"},
  {"urlwatch", "Crawlers"},
  {"IstellaBot", "Crawlers"},
  {"OpenWebSpider", "Crawlers"},
  {"AppEngine-Google", "Crawlers"},
  {"WordPress", "Crawlers"},
  {"yacybot", "Crawlers"},
  {"PEAR", "Crawlers"},
  {"ZumBot", "Crawlers"},
  {"YisouSpider", "Crawlers"},
  {"W3C", "Crawlers"},
  {"vcheck", "Crawlers"},
  {"PycURL", "Crawlers"},
  {"PHP", "Crawlers"},
  {"PercolateCrawler", "Crawlers"},
  {"NING", "Crawlers"},
  {"gvfs", "Crawlers"},
  {"Crowsnest", "Crawlers"},
  {"CatchBot", "Crawlers"},
  {"Combine", "Crawlers"},
  {"Dalvik", "Crawlers"},
  {"A6-Indexer", "Crawlers"},
  {"Altresium", "Crawlers"},
  {"AndroidDownloadManager", "Crawlers"},
  {"Apache-HttpClient", "Crawlers"},
  {"Comodo", "Crawlers"},
  {"crawler4j", "Crawlers"},
  {"Cricket", "Crawlers"},
  {"EC2LinkFinder", "Crawlers"},
  {"Embedly", "Crawlers"},
  {"envolk", "Crawlers"},
  {"libwww-perl", "Crawlers"},
  {"python", "Crawlers"},
  {"Python", "Crawlers"},
  {"LinkedIn", "Crawlers"},
  {"GeoHasher", "Crawlers"},
  {"HTMLParser", "Crawlers"},
  {"MLBot", "Crawlers"},
  {"Jaxified Bot", "Crawlers"},
  {"LinkWalker", "Crawlers"},
  {"Microsoft-WebDAV", "Crawlers"},
  {"nutch", "Crawlers"},
  {"PostRank", "Crawlers"},
  {"Image", "Crawlers"},

  {"Mozilla", "Others"}
};

char *
verify_browser (const char *str, char *browser_type)
{
  char *a, *b, *p, *ptr, *slash;
  size_t i;

  if (str == NULL || *str == '\0')
    return NULL;

  for (i = 0; i < ARRAY_SIZE (browsers); i++) {
    if ((a = strstr (str, browsers[i][0])) == NULL)
      continue;

    if (!(b = a))
      return NULL;
    ptr = a;

    /* Opera +15 uses OPR/# */
    if (strstr (b, "OPR") != NULL) {
      if ((slash = strrchr (b, '/')) != NULL) {
        char *val = xmalloc (snprintf (NULL, 0, "Opera%s", slash) + 1);
        sprintf (val, "Opera%s", slash);

        xstrncpy (browser_type, "Opera", BROWSER_TYPE_LEN);
        return val;
      }
    }
    /* Opera has the version number at the end */
    if (strstr (a, "Opera") != NULL) {
      if ((slash = strrchr (b, '/')) != NULL && a < slash)
        memmove (a + 5, slash, strlen (slash) + 1);
    }
    /* MSIE */
    if (strstr (a, "MSIE") != NULL) {
      while (*ptr != ';' && *ptr != ')' && *ptr != '-' && *ptr != '\0') {
        if (*ptr == ' ')
          *ptr = '/';
        ptr++;
      }
    }
    /* IE11 */
    if (strstr (a, "rv:11") != NULL && strstr (a, "Trident/7.0") != NULL) {
      xstrncpy (browser_type, "MSIE", BROWSER_TYPE_LEN);
      return alloc_string ("MSIE/11.0");
    }
    /* everything else is parsed here */
    for (p = a; *p; p++) {
      if (isalnum (p[0]) || *p == '.' || *p == '/' || *p == '_' || *p == '-') {
        a++;
        continue;
      } else {
        break;
      }
    }
    *p = 0;

    xstrncpy (browser_type, browsers[i][1], BROWSER_TYPE_LEN);
    return alloc_string (b);
  }
  xstrncpy (browser_type, "Unknown", BROWSER_TYPE_LEN);

  return alloc_string ("Unknown");
}
