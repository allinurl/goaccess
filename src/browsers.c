/**
 6 browsers.c -- functions for dealing with browsers
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "browsers.h"

#include "error.h"
#include "settings.h"
#include "util.h"
#include "xmalloc.h"

/* ###NOTE: The size of the list is proportional to the run time,
 * which makes this pretty slow */

static char ***browsers_hash = NULL;

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
  {"Avant Browser", "Others"},
  /* Internet Explorer */
  {"IEMobile", "MSIE"},
  {"MSIE", "MSIE"},
  /* IE11 */
  {"Trident/7.0", "MSIE"},
  /* Microsoft Edge */
  {"Edg", "Edge"},
  {"Edge", "Edge"},

  /* Surf Browser */
  {"Surf", "Surf"},

  /* Opera */
  {"Opera Mini", "Opera"},
  {"Opera Mobi", "Opera"},
  {"Opera", "Opera"},
  {"OPR", "Opera"},
  {"OPiOS", "Opera"},
  {"Coast", "Opera"},

  /* Others */
  {"Homebrew", "Others"},
  {"APT-", "Others"},
  {"Apt-Cacher", "Others"},
  {"Aptly", "Others"},
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
  {"IBrowse", "Others"},
  {"K-Meleon", "Others"},
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
  {"pacman", "Others"},
  {"Pamac", "Others"},
  {"libwww-perl", "Others"},
  {"python-requests", "Others"},
  {"PackageKit", "Others"},
  {"F-Droid", "Others"},
  {"okhttp", "Others"},
  {"node", "Others"},
  {"PrivacyBrowser", "Others"},
  {"Transmission", "Others"},
  {"libmpv", "Others"},
  {"aria2", "Others"},

  /* Feed-reader-as-a-service */
  {"AppleNewsBot", "Feeds"},
  {"Bloglines", "Feeds"},
  {"Digg Feed Fetcher", "Feeds"},
  {"Feedbin", "Feeds"},
  {"FeedHQ", "Feeds"},
  {"Feedly", "Feeds"},
  {"Flipboard", "Feeds"},
  {"inoreader.com", "Feeds"},
  {"Netvibes", "Feeds"},
  {"NewsBlur", "Feeds"},
  {"PinRSS", "Feeds"},
  {"theoldreader.com", "Feeds"},
  {"WordPress.com Reader", "Feeds"},
  {"YandexBlogs", "Feeds"},
  {"Brainstorm", "Feeds"},
  {"Mastodon", "Feeds"},
  {"Pleroma", "Feeds"},

  /* Google crawlers (some based on Chrome,
   * therefore up on the list) */
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
  {"Waterfox", "Firefox"},
  {"PaleMoon", "Firefox"},
  {"Focus", "Firefox"},
  /* Klar is the name of Firefox Focus in the German market. */
  {"Klar", "Firefox"},
  {"Firefox", "Firefox"},

  /* Based on Chromium */
  {"BeakerBrowser", "Beaker"},
  {"Brave", "Brave"},
  {"Vivaldi", "Vivaldi"},
  {"YaBrowser", "Yandex.Browser"},

  /* Chrome has to go before Safari */
  {"HeadlessChrome", "Chrome"},
  {"Chrome", "Chrome"},
  {"CriOS", "Chrome"},

  /* Crawlers/Bots (Possible Safari based) */
  {"AppleBot", "Crawlers"},
  {"facebookexternalhit", "Crawlers"},
  {"Twitter", "Crawlers"},

  {"Safari", "Safari"},

  /* Crawlers/Bots */
  {"Slack", "Crawlers"},
  {"Sogou", "Crawlers"},
  {"Java", "Crawlers"},
  {"Jakarta Commons-HttpClient", "Crawlers"},
  {"netEstate", "Crawlers"},
  {"PiplBot", "Crawlers"},
  {"IstellaBot", "Crawlers"},
  {"heritrix", "Crawlers"},
  {"PagesInventory", "Crawlers"},
  {"rogerbot", "Crawlers"},
  {"fastbot", "Crawlers"},
  {"yacybot", "Crawlers"},
  {"PycURL", "Crawlers"},
  {"PHP", "Crawlers"},
  {"AndroidDownloadManager", "Crawlers"},
  {"Embedly", "Crawlers"},
  {"ruby", "Crawlers"},
  {"Ruby", "Crawlers"},
  {"python", "Crawlers"},
  {"Python", "Crawlers"},
  {"LinkedIn", "Crawlers"},
  {"Microsoft-WebDAV", "Crawlers"},
  {"DuckDuckGo-Favicons-Bot", "Crawlers"},
  {"bingbot", "Crawlers"},
  {"PetalBot", "Crawlers"},
  {"Discordbot", "Crawlers"},
  {"ZoominfoBot", "Crawlers"},
  {"Googlebot", "Crawlers"},
  {"DotBot", "Crawlers"},
  {"AhrefsBot", "Crawlers"},
  {"SemrushBot", "Crawlers"},
  {"Adsbot", "Crawlers"},
  {"BLEXBot", "Crawlers"},
  {"NetcraftSurveyAgent", "Crawlers"},
  {"Netcraft Web Server Survey", "Crawlers"},
  {"masscan", "Crawlers"},
  {"MJ12bot", "Crawlers"},
  {"Pandalytics", "Crawlers"},
  {"YandexBot", "Crawlers"},
  {"Nimbostratus-Bot", "Crawlers"},
  {"HTTP Banner Detection", "Crawlers"},
  {"Hakai", "Crawlers"},
  {"WinHttp.WinHttpRequest.5", "Crawlers"},
  {"NetSystemsResearch", "Crawlers"},
  {"Nextcloud Server Crawler", "Crawlers"},
  {"CFNetwork", "Crawlers"},
  {"GoScraper", "Crawlers"},
  {"Googlebot-Image", "Crawlers"},
  {"ZmEu", "Crawlers"},
  {"DowntimeDetector", "Crawlers"},
  {"MauiBot", "Crawlers"},
  {"Cloud", "Crawlers"},
  {"stagefright", "Crawlers"},
  {"DataForSeoBot", "Crawlers"},
  {"SeznamBot", "Crawlers"},
  {"coccocbot", "Crawlers"},
  {"Neevabot", "Crawlers"},


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
  {"BTWebClient", "Feeds"},
  {"com.apple.Safari.WebFeedParser", "Feeds"},
  {"FeedDemon", "Feeds"},
  {"Feedy", "Feeds"},
  {"Fever", "Feeds"},
  {"FreshRSS", "Feeds"},
  {"Liferea", "Feeds"},
  {"NetNewsWire", "Feeds"},
  {"RSSOwl", "Feeds"},
  {"Tiny Tiny RSS", "Feeds"},
  {"Thunderbird", "Feeds"},
  {"Winds", "Feeds"},

  {"Pingdom.com", "Uptime"},
  {"jetmon", "Uptime"},
  {"NodeUptime", "Uptime"},
  {"NewRelicPinger", "Uptime"},
  {"StatusCake", "Uptime"},
  {"internetVista", "Uptime"},
  {"Server Density Service Monitoring v2", "Uptime"},

  {"Mozilla", "Others"}
};

/* Free all browser entries from our array of key/value pairs. */
void
free_browsers_hash (void) {
  size_t i;
  int j;

  for (i = 0; i < ARRAY_SIZE (browsers); ++i) {
    free (browsers_hash[i][0]);
    free (browsers_hash[i][1]);
    free (browsers_hash[i]);
  }
  free (browsers_hash);


  for (j = 0; j < conf.browsers_hash_idx; ++j) {
    free (conf.user_browsers_hash[j][0]);
    free (conf.user_browsers_hash[j][1]);
    free (conf.user_browsers_hash[j]);
  }
  if (conf.browsers_file) {
    free (conf.user_browsers_hash);
  }
}

static int
is_dup (char ***list, int len, const char *browser) {
  int i;
  /* check for dups */
  for (i = 0; i < len; ++i) {
    if (strcmp (browser, list[i][0]) == 0)
      return 1;
  }
  return 0;
}

/* Set a browser/type pair into our multidimensional array of browsers.
 *
 * On duplicate functions returns void.
 * Otherwise memory is mallo'd for our array entry. */
static void
set_browser (char ***list, int idx, const char *browser, const char *type) {
  list[idx] = xcalloc (2, sizeof (char *));
  list[idx][0] = xstrdup (browser);
  list[idx][1] = xstrdup (type);
}

/* Parse the key/value pair from the browser list file. */
static void
parse_browser_token (char ***list, char *line, int n) {
  char *val;
  size_t idx = 0;

  /* key */
  idx = strcspn (line, "\t");
  if (strlen (line) == idx)
    FATAL ("Malformed browser name at line: %d", n);

  line[idx] = '\0';

  /* value */
  val = line + (idx + 1);
  idx = strspn (val, "\t");
  if (strlen (val) == idx)
    FATAL ("Malformed browser category at line: %d", n);
  val = val + idx;
  val = trim_str (val);

  if (is_dup (list, conf.browsers_hash_idx, line)) {
    LOG_INVALID (("Duplicate browser entry: %s", line));
    return;
  }

  set_browser (list, conf.browsers_hash_idx, line, val);
  conf.browsers_hash_idx++;
}

/* Parse our default array of browsers and put them on our hash including those
 * from the custom parsed browsers file.
 *
 * On error functions returns void.
 * Otherwise browser entries are put into the hash. */
void
parse_browsers_file (void) {
  char line[MAX_LINE_BROWSERS + 1];
  FILE *file;
  int n = 0;
  size_t i, len = ARRAY_SIZE (browsers);

  browsers_hash = xmalloc (ARRAY_SIZE (browsers) * sizeof (char **));
  /* load hash from the browser's array (default)  */
  for (i = 0; i < len; ++i) {
    set_browser (browsers_hash, i, browsers[i][0], browsers[i][1]);
  }

  if (!conf.browsers_file)
    return;

  /* could not open browsers file */
  if ((file = fopen (conf.browsers_file, "r")) == NULL)
    FATAL ("Unable to open browser's file: %s", strerror (errno));

  conf.user_browsers_hash = xmalloc (MAX_CUSTOM_BROWSERS * sizeof (char **));
  /* load hash from the user's given browsers file  */
  while (fgets (line, sizeof line, file) != NULL) {
    while (line[0] == ' ' || line[0] == '\t')
      memmove (line, line + 1, strlen (line));
    n++;

    if (line[0] == '\n' || line[0] == '\r' || line[0] == '#')
      continue;
    if (conf.browsers_hash_idx >= MAX_CUSTOM_BROWSERS)
      FATAL ("Maximum number of custom browsers has been reached");
    parse_browser_token (conf.user_browsers_hash, line, n);
  }
  fclose (file);
}

/* Determine if the user-agent is a crawler.
 *
 * On error or is not a crawler, 0 is returned.
 * If it is a crawler, 1 is returned . */
int
is_crawler (const char *agent) {
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
parse_opera (char *token) {
  char *val = xmalloc (snprintf (NULL, 0, "Opera%s", token) + 1);
  sprintf (val, "Opera%s", token);

  return val;
}

/* Given the original user agent string, and a partial crawler match, iterate
 * back until the next delimiter is found and return occurrence.
 *
 * On error when attempting to extract crawler, NULL is returned.
 * If a possible crawler string is matched, then possible bot is returned . */
static char *
parse_crawler (char *str, char *match, char *type) {
  char *ptr = NULL;
  int found = 0;

  while (match != str) {
    match--;
    if (*match == ' ' || *match == '+') {
      found = 1;
      break;
    }
  }

  /* same addr */
  if (match == str)
    return NULL;

  /* account for the previous +|space */
  if (found)
    match++;

  if ((ptr = strpbrk (match, "; ")))
    *ptr = '\0';
  /* empty string after parsing it */
  if (*match == '\0')
    return NULL;

  xstrncpy (type, "Crawlers", BROWSER_TYPE_LEN);

  return xstrdup (match);
}

/* If the following string matches are found within user agent, then it's
 * highly likely it's a possible crawler.
 * Note that this could certainly return false positives.
 *
 * If no occurrences are found, NULL is returned.
 * If an occurrence is found, a pointer to the match is returned . */
static char *
check_http_crawler (const char *str) {
  char *match = NULL;

  /* e.g., compatible; bingbot/2.0; +http://www.bing.com/bingbot.htm */
  if ((match = strstr (str, "; +http")))
    return match;
  /* compatible; UptimeRobot/2.0; http://www.uptimerobot.com/ */
  if ((match = strstr (str, "; http")))
    return match;
  /* Slack-ImgProxy (+https://api.slack.com/robots) */
  if ((match = strstr (str, " (+http")))
    return match;
  /*  TurnitinBot/3.0 (http://www.turnitin.com/robot/crawlerinfo.html) */
  if ((match = strstr (str, " (http")))
    return match;
  /* w3c e.g., (compatible;+Googlebot/2.1;++http://www.google.com/bot.html) */
  if ((match = strstr (str, ";++http")))
    return match;
  return NULL;
}

/* Parse the given user agent match and extract the browser string.
 *
 * If no match, the original match is returned.
 * Otherwise the parsed browser is returned. */
static char *
parse_browser (char *match, char *type, int i, char ***hash) {
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
  else if ((ptr = strpbrk (b ? b : match, ";) ")) != NULL) {
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
verify_browser (char *str, char *type) {
  char *match = NULL, *token = NULL;
  int i = 0;
  size_t j = 0;

  if (str == NULL || *str == '\0')
    return NULL;

  /* check user's list */
  for (i = 0; i < conf.browsers_hash_idx; ++i) {
    if ((match = strstr (str, conf.user_browsers_hash[i][0])) == NULL)
      continue;
    return parse_browser (match, type, i, conf.user_browsers_hash);
  }

  if ((match = check_http_crawler (str)) && (token = parse_crawler (str, match, type)))
    return token;

  /* fallback to default browser list */
  for (j = 0; j < ARRAY_SIZE (browsers); ++j) {
    if ((match = strstr (str, browsers_hash[j][0])) == NULL)
      continue;
    return parse_browser (match, type, j, browsers_hash);
  }

  if (conf.unknowns_log)
    LOG_UNKNOWNS (("%-7s%s\n", "[BR]", str));

  xstrncpy (type, "Unknown", BROWSER_TYPE_LEN);

  return alloc_string ("Unknown");
}
