/**
 * browsers.c -- functions for dealing with browsers
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "browsers.h"

#include "error.h"
#include "settings.h"
#include "util.h"
#include "xmalloc.h"

/* ###NOTE: The size of the list is proportional to the run time,
 * which makes this pretty slow */

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
  {"google-cloud-sdk", "Others"},
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
  {"Go-http-client", "Others"},
  {"curl", "Others"},
  {"midori", "Others"},
  {"w3m", "Others"},
  {"MicroMessenger", "Others"},
  {"Apache", "Others"},
  {"JOSM", "Others"},

  /* Feed-reader-as-a-service */
  {"AppleNewsBot", "Feeds"},
  {"Bloglines", "Feeds"},
  {"Digg Feed Fetcher", "Feeds"},
  {"Feedbin", "Feeds"},
  {"FeedHQ", "Feeds"},
  {"Feedly", "Feeds"},
  {"Flipboard", "Feeds"},
  {"Netvibes", "Feeds"},
  {"NewsBlur", "Feeds"},
  {"PinRSS", "Feeds"},
  {"WordPress.com Reader", "Feeds"},
  {"YandexBlogs", "Feeds"},

  /* Google crawlers (some based on Chrome,
   * therefore up on the list) */
  {"Googlebot", "Crawlers"},
  {"AdsBot-Google", "Crawlers"},
  {"AppEngine-Google", "Crawlers"},
  {"Mediapartners-Google", "Crawlers"},
  {"Google", "Crawlers"},
  {"WhatsApp", "Crawlers"},

  /* Based on Firefox */
  {"Camino", "Others"},
  /* Rebranded Firefox but is really unmodified
   * Firefox (Debian trademark policy) */
  {"Iceweasel", "Firefox"},
  {"Firefox", "Firefox"},

  /* Based on Chromium */
  {"Vivaldi", "Others"},
  {"YaBrowser", "Others"},
  {"Flock", "Others"},
  /* Chrome has to go before Safari */
  {"HeadlessChrome", "Chrome"},
  {"Chrome", "Chrome"},

  {"CriOS", "Chrome"},

  /* Crawlers/Bots (Possible Safari based) */
  {"bingbot", "Crawlers"},
  {"AppleBot", "Crawlers"},
  {"Yandex", "Crawlers"},
  {"msnbot", "Crawlers"},
  {"Baiduspider", "Crawlers"},
  {"Twitter", "Crawlers"},
  {"Slurp", "Crawlers"},
  {"Yahoo", "Crawlers"},
  {"Slack", "Crawlers"},
  {"facebook", "Crawlers"},

  {"Safari", "Safari"},

  /* Crawlers/Bots */
  {"Ezooms", "Crawlers"},
  {"AhrefsBot", "Crawlers"},
  {"Abonti", "Crawlers"},
  {"Mastodon", "Crawlers"},
  {"MJ12bot", "Crawlers"},
  {"SISTRIX", "Crawlers"},
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
  {"spbot", "Crawlers"},
  {"YodaoBot", "Crawlers"},
  {"AddThis", "Crawlers"},
  {"Purebot", "Crawlers"},
  {"ia_archiver", "Crawlers"},
  {"Wotbox", "Crawlers"},
  {"CCBot", "Crawlers"},
  {"BLEXBot", "Crawlers"},
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
  {"keybase-proofs", "Crawlers"},
  {"SemrushBot", "Crawlers"},
  {"CommonCrawler", "Crawlers"},
  {"Mail.RU_Bot", "Crawlers"},
  {"MegaIndex.ru", "Crawlers"},
  {"XoviBot", "Crawlers"},
  {"X-CAD-SE", "Crawlers"},
  {"Safeassign", "Crawlers"},
  {"Nmap Scripting Engine", "Crawlers"},
  {"sqlmap", "Crawlers"},
  {"Jorgee", "Crawlers"},
  {"PxBroker", "Crawlers"},
  {"Seekport", "Crawlers"},
  {"adscanner", "Crawlers"},
  {"linkdexbot", "Crawlers"},
  {"Cliqzbot", "Crawlers"},
  {"AfD-Verbotsverfahren_JETZT!", "Crawlers"},

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
  {"com.apple.Safari.WebFeedParser", "Feeds"},
  {"FeedDemon", "Feeds"},
  {"Feedy", "Feeds"},
  {"Liferea", "Feeds"},
  {"NetNewsWire", "Feeds"},
  {"RSSOwl", "Feeds"},
  {"Thunderbird", "Feeds"},
  {"Vienna", "Feeds"},
  {"Windows-RSS-Platform", "Feeds"},
  {"newsbeuter", "Feeds"},
  {"Wrangler", "Feeds"},
  {"Fever", "Feeds"},
  {"Tiny Tiny RSS", "Feeds"},
  {"FreshRSS", "Feeds"},
  {"KrISS feed agent", "Feeds"},
  {"SimplePie", "Feeds"},
  {"Feedsubs", "Feeds"},
  {"UniversalFeedParser", "Feeds"},

  {"Pingdom.com", "Uptime"},
  {"UptimeRobot", "Uptime"},
  {"jetmon", "Uptime"},
  {"NodeUptime", "Uptime"},
  {"NewRelicPinger", "Uptime"},
  {"StatusCake", "Uptime"},
  {"internetVista", "Uptime"},
  {"Server Density Service Monitoring v2", "Uptime"},

  {"Mozilla", "Others"}
};

/* Set a browser/type pair into our multidimensional array of browsers.
 *
 * On duplicate functions returns void.
 * Otherwise memory is mallo'd for our array entry. */
static void
set_browser_key_value (char ***list, const char *browser, const char *type)
{
  int i;
  /* check for dups */
  for (i = 0; i < conf.browsers_hash_idx; ++i) {
    if (strcmp (browser, conf.browsers_hash[i][0]) == 0)
      return;
  }

  list[conf.browsers_hash_idx] = xcalloc (2, sizeof (char *));
  list[conf.browsers_hash_idx][0] = xstrdup (browser);
  list[conf.browsers_hash_idx][1] = xstrdup (type);
  conf.browsers_hash_idx++;
}

/* Parse the key/value pair from the browser list file. */
static void
parse_browser_token (char ***list, char *line, int n)
{
  char *val;
  size_t idx = 0;

  /* key */
  idx = strcspn (line, " \t");
  if (strlen (line) == idx)
    FATAL ("Malformed browser name at line: %d", n);

  line[idx] = '\0';

  /* value */
  val = line + (idx + 1);
  idx = strspn (val, " \t");
  if (strlen (line) == idx)
    FATAL ("Malformed browser category at line: %d", n);
  val = val + idx;
  val = trim_str (val);

  set_browser_key_value (list, line, val);
}

/* Free all browser entries from our array of key/value pairs. */
void
free_browsers_hash (void)
{
  int i;
  /* check for dups */
  for (i = 0; i < conf.browsers_hash_idx; ++i) {
    free (conf.browsers_hash[i][0]);
    free (conf.browsers_hash[i][1]);
    free (conf.browsers_hash[i]);
  }
  free (conf.browsers_hash);
}

/* Parse our default array of browsers and put them on our hash including those
 * from the custom parsed browsers file.
 *
 * On error functions returns void.
 * Otherwise browser entries are put into the hash. */
void
parse_browsers_file (void)
{
  char line[MAX_LINE_BROWSERS + 1];
  FILE *file;
  int n = 0;
  size_t i;

  conf.browsers_hash = xmalloc (MAX_CUSTOM_BROWSERS * sizeof (char **));

  /* load hash from the browser's array (default)  */
  for (i = 0; i < ARRAY_SIZE (browsers); ++i) {
    if (conf.browsers_hash_idx >= MAX_CUSTOM_BROWSERS)
      continue;
    set_browser_key_value (conf.browsers_hash, browsers[i][0], browsers[i][1]);
  }

  if (!conf.browsers_file)
    return;

  /* could not open browsers file */
  if ((file = fopen (conf.browsers_file, "r")) == NULL)
    FATAL ("Unable to open browser's file: %s", strerror (errno));

  /* load hash from the browser's array (default)  */
  while (fgets (line, sizeof line, file) != NULL) {
    while (line[0] == ' ' || line[0] == '\t')
      memmove (line, line + 1, strlen (line));
    n++;

    if (line[0] == '\n' || line[0] == '\r' || line[0] == '#')
      continue;
    if (conf.browsers_hash_idx < MAX_CUSTOM_BROWSERS)
      parse_browser_token (conf.browsers_hash, line, n);
  }
  fclose (file);
}

/* Determine if the user-agent is a crawler.
 *
 * On error or is not a crawler, 0 is returned.
 * If it is a crawler, 1 is returned . */
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

/* Return the Opera 15 and beyond.
 *
 * On success, the opera string and version is returned. */
static char *
parse_opera (char *token)
{
  char *val = xmalloc (snprintf (NULL, 0, "Opera%s", token) + 1);
  sprintf (val, "Opera%s", token);

  return val;
}

/* Parse the given user agent match and extract the browser string.
 *
 * If no match, the original match is returned.
 * Otherwise the parsed browser is returned. */
static char *
parse_browser (char *match, char *type, int i, char ***hash)
{
  char *b = NULL, *ptr = NULL, *slh = NULL;
  size_t cnt = 0, space = 0;

  /* Check if there are spaces in the token string, that way strpbrk
   * does not stop at the first space within the token string */
  if ((cnt = count_matches (hash[i][0], ' ')) && (b = match)) {
    while (space++ < cnt && (b = strchr (b, ' ')))
      b++;
  } else
    b = match;

  xstrncpy (type, hash[i][1], BROWSER_TYPE_LEN);
  /* Internet Explorer 11 */
  if (strstr (match, "rv:11") && strstr (match, "Trident/7.0")) {
    return alloc_string ("MSIE/11.0");
  }
  /* Opera +15 uses OPR/# */
  if (strstr (match, "OPR") != NULL && (slh = strrchr (match, '/'))) {
    return parse_opera (slh);
  }
  /* Opera has the version number at the end */
  if (strstr (match, "Opera") && (slh = strrchr (match, '/')) && match < slh) {
    memmove (match + 5, slh, strlen (slh) + 1);
  }
  /* IE Old */
  if (strstr (match, "MSIE") != NULL) {
    if ((ptr = strpbrk (match, ";)-")) != NULL)
      *ptr = '\0';
    match = char_replace (match, ' ', '/');
  }
  /* all others */
  else if ((ptr = strpbrk (b, ";) ")) != NULL) {
    *ptr = '\0';
  }

  return alloc_string (match);
}

/* Given a user agent, determine the browser used.
 *
 * ###NOTE: The size of the list is proportional to the run time,
 * which makes this pretty slow
 *
 * On error, NULL is returned.
 * On success, a malloc'd  string containing the browser is returned. */
char *
verify_browser (char *str, char *type)
{
  char *match = NULL;
  int i = 0;

  if (str == NULL || *str == '\0')
    return NULL;

  /* check list */
  for (i = 0; i < conf.browsers_hash_idx; ++i) {
    if ((match = strstr (str, conf.browsers_hash[i][0])) == NULL)
      continue;
    return parse_browser (match, type, i, conf.browsers_hash);
  }

  xstrncpy (type, "Unknown", BROWSER_TYPE_LEN);

  return alloc_string ("Unknown");
}
