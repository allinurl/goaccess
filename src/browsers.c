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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "browsers.h"

#include "util.h"
#include "error.h"
#include "xmalloc.h"

/* {"search string", "belongs to"} */
static const char *browsers[][2] = {
  /* Game systems: most of them are based of major browsers,
   * thus they need to go before. */
  {"Xbox One", "Game Systems"},
  {"Xbox", "Game Systems"},
  {"PlayStation", "Game Systems"},
  {"NintendoBrowser", "Game Systems"},
  {"Valve Steam", "Game Systems"},
  {"Origin", "Game Systems"},
  {"Raptr", "Game Systems"},

  /* Based on Internet Explorer */
  {"America Online Browser", "Others"},
  {"Avant Browser", "Others"},
  /* Internet Explorer */
  {"IEMobile", "MSIE"},
  {"MSIE", "MSIE"},
  /* IE11 */
  {"Trident/7.0", "MSIE"},
  /* Microsoft Edge */
  {"Edge", "MSIE"},

  /* Opera */
  {"Opera Mini", "Opera"},
  {"Opera Mobi", "Opera"},
  {"Opera", "Opera"},
  {"OPR", "Opera"},
  {"OPiOS", "Opera"},
  {"Coast", "Opera"},

  /* Others */
  {"Homebrew", "Others"},
  {"APT-HTTP", "Others"},
  {"Apt-Cacher", "Others"},
  {"Chef Client", "Others"},
  {"Huawei", "Others"},
  {"HUAWEI", "Others"},
  {"BlackBerry", "Others"},
  {"BrowserX", "Others"},
  {"Dalvik", "Others"},
  {"Dillo", "Others"},
  {"ELinks", "Others"},
  {"Epiphany", "Others"},
  {"Firebird", "Others"},
  {"Galeon", "Others"},
  {"GranParadiso", "Others"},
  {"IBrowse", "Others"},
  {"K-Meleon", "Others"},
  {"Kazehakase", "Others"},
  {"Konqueror", "Others"},
  {"Links", "Others"},
  {"Lynx", "Others"},
  {"Midori", "Others"},
  {"Minefield", "Others"},
  {"Mosaic", "Others"},
  {"Netscape", "Others"},
  {"SeaMonkey", "Others"},
  {"UCBrowser", "Others"},
  {"Wget", "Others"},
  {"libfetch", "Others"},
  {"check_http", "Others"},
  {"curl", "Others"},
  {"midori", "Others"},
  {"w3m", "Others"},
  {"Apache", "Others"},

  /* Feed-reader-as-a-service */
  {"Bloglines", "Feeds"},
  {"Feedly", "Feeds"},
  {"Flipboard", "Feeds"},
  {"Netvibes", "Feeds"},
  {"NewsBlur", "Feeds"},
  {"YandexBlogs", "Feeds"},

  /* Based on Firefox */
  {"Camino", "Others"},
  /* Rebranded Firefox but is really unmodified
   * Firefox (Debian trademark policy) */
  {"Iceweasel", "Firefox"},
  /* Mozilla Firefox */
  {"Firefox", "Firefox"},

  /* Based on Chromium */
  {"YaBrowser", "Others"},
  {"Flock", "Others"},
  /* Chrome has to go before Safari */
  {"Chrome", "Chrome"},

  {"CriOS", "Chrome"},
  {"Safari", "Safari"},

  /* Crawlers/Bots */
  {"AdsBot-Google", "Crawlers"},
  {"Mediapartners-Google", "Crawlers"},
  {"AppEngine-Google", "Crawlers"},
  {"Google", "Crawlers"},
  {"bingbot", "Crawlers"},
  {"msnbot", "Crawlers"},
  {"Yandex", "Crawlers"},
  {"Baidu", "Crawlers"},
  {"Ezooms", "Crawlers"},
  {"Twitter", "Crawlers"},
  {"Slurp", "Crawlers"},
  {"Yahoo", "Crawlers"},
  {"AppleBot", "Crawlers"},
  {"AhrefsBot", "Crawlers"},
  {"Abonti", "Crawlers"},
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
  {"ruby", "Crawlers"},
  {"Ruby", "Crawlers"},
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

  /* Podcast fetchers */
  {"Downcast", "Podcasts"},
  {"gPodder", "Podcasts"},
  {"Instacast", "Podcasts"},
  {"iTunes", "Podcasts"},
  {"Miro", "Podcasts"},
  {"Pocket Casts", "Podcasts"},
  {"BashPodder", "Podcasts"},

  /* Feed reader clients */
  {"Akregator", "Feeds"},
  {"Apple-PubSub", "Feeds"},
  {"FeedDemon", "Feeds"},
  {"Feedy", "Feeds"},
  {"Liferea", "Feeds"},
  {"NetNewsWire", "Feeds"},
  {"RSSOwl", "Feeds"},
  {"Thunderbird", "Feeds"},
  {"Vienna", "Feeds"},
  {"Windows-RSS-Platform", "Feeds"},
  {"newsbeuter", "Feeds"},
  {"Fever", "Feeds"},

  {"Pingdom.com", "Uptime"},
  {"UptimeRobot", "Uptime"},
  {"jetmon", "Uptime"},
  {"NodeUptime", "Uptime"},
  {"NewRelicPinger", "Uptime"},
  {"StatusCake", "Uptime"},
  {"internetVista", "Uptime"},

  {"Mozilla", "Others"}
};

int
is_crawler (const char *agent)
{
  char type[BROWSER_TYPE_LEN];
  char *browser, *a;

  if (agent == NULL || *agent == '\0')
    return 0;

  if ((a = xstrdup (agent), browser = verify_browser (a, type)) != NULL)
    free (browser);
  free (a);

  return strcmp (type, "Crawlers") == 0 ? 1 : 0;
}

/* Return the Opera 15 and Beyond */
static char *
parse_opera (char *token)
{
  char *val = xmalloc (snprintf (NULL, 0, "Opera%s", token) + 1);
  sprintf (val, "Opera%s", token);

  return val;
}

char *
verify_browser (char *str, char *type)
{
  char *a, *b, *ptr, *slash;
  size_t i;

  if (str == NULL || *str == '\0')
    return NULL;

  for (i = 0; i < ARRAY_SIZE (browsers); i++) {
    if ((a = strstr (str, browsers[i][0])) == NULL)
      continue;

    /* check if there is a space char in the token string, that way strpbrk
     * does not stop at the first space within the token string */
    if ((strchr (browsers[i][0], ' ')) != NULL && (b = strchr (a, ' ')) != NULL)
      b++;
    else
      b = a;

    xstrncpy (type, browsers[i][1], BROWSER_TYPE_LEN);
    /* Internet Explorer 11 */
    if (strstr (a, "rv:11") && strstr (a, "Trident/7.0")) {
      return alloc_string ("MSIE/11.0");
    }
    /* Opera +15 uses OPR/# */
    if (strstr (a, "OPR") != NULL && (slash = strrchr (a, '/'))) {
      return parse_opera (slash);
    }
    /* Opera has the version number at the end */
    if (strstr (a, "Opera") && (slash = strrchr (a, '/')) && a < slash) {
      memmove (a + 5, slash, strlen (slash) + 1);
    }
    /* IE Old */
    if (strstr (a, "MSIE") != NULL) {
      if ((ptr = strpbrk (a, ";)-")) != NULL)
        *ptr = '\0';
      a = char_replace (a, ' ', '/');
    }
    /* all others */
    else if ((ptr = strpbrk (b, ";) ")) != NULL) {
      *ptr = '\0';
    }

    return alloc_string (a);
  }
  xstrncpy (type, "Unknown", BROWSER_TYPE_LEN);

  return alloc_string ("Unknown");
}
