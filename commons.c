/** 
 * commons.c -- holds different data types 
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
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

#include <curses.h>
#include <glib.h>
#include <time.h>

#include "commons.h"

#define BLACK_CYAN   9
#define BLACK_GREEN  8
#define BLUE_GREEN   7
#define COL_BLACK    4
#define COL_BLUE     1
#define COL_CYAN     5
#define COL_GREEN    11
#define COL_RED      3
#define COL_WHITE    0
#define COL_YELLOW   6
#define WHITE_RED    10

/* Definitions checked against declarations */
GHashTable *ht_browsers = NULL;
GHashTable *ht_date_bw = NULL;
GHashTable *ht_file_bw = NULL;
GHashTable *ht_host_bw = NULL;
GHashTable *ht_hosts_agents = NULL;
GHashTable *ht_hosts = NULL;
GHashTable *ht_keyphrases = NULL;
GHashTable *ht_monthly = NULL;
GHashTable *ht_not_found_requests = NULL;
GHashTable *ht_os = NULL;
GHashTable *ht_referrers = NULL;
GHashTable *ht_referring_sites = NULL;
GHashTable *ht_requests = NULL;
GHashTable *ht_requests_static = NULL;
GHashTable *ht_status_code = NULL;
GHashTable *ht_unique_visitors = NULL;
GHashTable *ht_unique_vis = NULL;

enum MODULES modules;
enum SCHEMES schemes;

/* enable flags */
char *ignore_host;
int bandwidth_flag = 0;
int host_agents_list_flag = 0;
int http_status_code_flag = 0;

/* hash iteration */
int iter_ctr = 0;
int iter_module = 0;

/* string processing */
long long req_size = 0;

/* processing time */
time_t end_proc;
time_t now;
time_t start_proc;

/* resizing/scheme */
int color_scheme = 1;
size_t real_size_y = 0;
size_t term_h = 0;
size_t term_w = 0;

/* file */
char *ifile = NULL;
char *conf_keywords[][2] = {
   {"1", "color_scheme"},
};

char *module_names[] = {
   "  Visitors - ^s^ sort by date - ^S^ sort by hits",
   "  Requests sorted by hits",
   "  Static Requests sorted by hits",
   "  Referrers sorted by hits",
   "  404 - Not Found sorted by hits",
   "  Operating Systems sorted by unique visitors",
   "  Browsers sorted by unique visitors",
   "  Hosts sorted by hits",
   "  HTTP Status Codes sorted by hits",
   "  Top Referring Sites sorted by hits",
   "  Top Search Keyphrases (Google, Cache, Translate)"
};

/* {"search string", "belongs to"} */
char *os[][2] = {
   /* WINDOWS PLATFORM TOKENS */
   {"Windows NT 5.1", "Windows"}, {"Windows NT 6.0", "Windows"},
   {"Windows NT 6.1", "Windows"}, {"Windows NT 5.01", "Windows"},
   {"Windows NT 5.0", "Windows"}, {"Windows NT 4.0", "Windows"},
   {"Win 9x 4.90", "Windows"}, {"Windows 98", "Windows"},
   {"Windows 95", "Windows"}, {"Windows CE", "Windows"},
   /* LINUX, MAC & OTHERS */
   {"Debian", "Linux"}, {"Ubuntu", "Linux"}, {"Fedora", "Linux"},
   {"Mint", "Linux"}, {"SUSE", "Linux"}, {"Mandriva", "Linux"},
   {"Red Hat", "Linux"}, {"Gentoo", "Linux"}, {"CentOS", "Linux"},
   {"Android", "Linux"}, {"PCLinuxOS", "Linux"}, {"Linux", "Linux"},
   {"iPad", "Macintosh"}, {"iPhone", "Macintosh"}, {"iPod", "Macintosh"},
   {"iTunes", "Macintosh"}, {"Mac OS X", "Macintosh"},
   {"Mac OS", "Macintosh"}, {"FreeBSD", "BSD"}, {"NetBSD", "BSD"},
   {"OpenBSD", "BSD"}, {"SunOS", "Others"}, {"AmigaOS", "Others"},
   {"BlackBerry", "Others"}, {"Symbian OS", "Others"}, {"Xbox", "Others"},
   {"Nokia", "Others"}, {"PlayStation", "Others"}
};

/* {"search string", "belongs to"} */
char *browsers[][2] = {
   /* BROWSERS & OFFLINE BROWSERS */
   {"Avant Browser", "Others"},
   {"America Online Browser", "Others"},
   {"MSIE 6.", "MSIE"}, {"MSIE 7.", "MSIE"}, {"MSIE 8.", "MSIE"},
   {"MSIE 5.", "MSIE"}, {"MSIE 4.", "MSIE"},
   {"Flock", "Others"}, {"Epiphany", "Others"},
   {"SeaMonkey", "Others"}, {"Iceweasel", "Others"},
   {"Minefield", "Others"}, {"GranParadiso", "Others"},
   {"Firefox", "Firefox"}, {"Chrome", "Chrome"}, {"Safari", "Safari"},
   {"Opera Mini", "Opera"}, {"Opera", "Opera"}, {"Netscape", "Others"},
   {"Konqueror", "Others"}, {"Wget", "Others"}, {"w3m", "Others"},
   {"ELinks", "Others"}, {"Links", "Others"}, {"Lynx", "Others"},
   {"Camino", "Others"}, {"Dillo", "Others"}, {"Kazehakase", "Others"},
   {"Iceape", "Others"}, {"K-Meleon", "Others"}, {"Galeon", "Others"},
   {"BrowserX", "Others"}, {"IBrowse", "Others"},
   {"Mosaic", "Others"}, {"midori", "Others"}, {"Midori", "Others"},
   {"Firebird", "Others"},
   /* CRAWLERS & VALIDATORS */
   {"W3C_Validator", "Crawlers"},
   {"W3C_CSS_Validator", "Crawlers"}, {"facebook", "Crawlers"},
   {"msnbot-media", "Crawlers"}, {"msnbot", "Crawlers"},
   {"ia_archiver", "Crawlers"}, {"Mediapartners-Google", "Crawlers"},
   {"Googlebot-Image", "Crawlers"}, {"Googlebot", "Crawlers"},
   {"slurp", "Crawlers"}, {"Baiduspider", "Crawlers"},
   {"YandexBot", "Crawlers"}, {"FeedFetcher-Google", "Crawlers"},
   {"Speedy Spider", "Crawlers"}, {"Java", "Crawlers"},
   {"Gigabot", "Crawlers"}, {"Twiceler", "Crawlers"},
   {"YoudaoBot", "Crawlers"}, {"Turnitin", "Crawlers"},
   {"Ask Jeeves", "Crawlers"}, {"Exabot", "Crawlers"},
   {"archive.org_bot", "Crawlers"}, {"Google-Sitemaps", "Crawlers"},
   {"PostRank", "Crawlers"}, {"KaloogaBot", "Crawlers"},
   {"Twitter", "Crawlers"}, {"yacy", "Crawlers"}, {"Nutch", "Crawlers"},
   {"ichiro", "Crawlers"}, {"Sogou", "Crawlers"}, {"wikiwix", "Crawlers"},
   {"KaloogaBot", "Crawlers"}, {"Mozilla", "Others"}
};

char *codes[][2] = {
   {"100", "Continue - Server has received the request headers"},
   {"101", "Switching Protocols - Client asked to switch protocols"},
   {"200", "OK -  The request sent by the client was successful"},
   {"201", "Created -  The request has been fulfilled and created"},
   {"202", "Accepted - The request has been accepted for processing"},
   {"203", "Non-Authoritative Information"},
   {"204", "No Content - Request is not returning any content"},
   {"205", "Reset Content - User-Agent should reset the document"},
   {"206", "Partial Content - The partial GET has been successful"},
   {"300", "Multiple Choices - Multiple options for the resource"},
   {"301", "Moved Permanently - Resource has permanently moved"},
   {"302", "Moved Temporarily (redirect)"},
   {"303", "See Other Document - The response is at a different URI"},
   {"304", "Not Modified - Resource has not been modified"},
   {"305", "Use Proxy - Can only be accessed through the proxy"},
   {"307", "Temporary Redirect - Resource temporarily moved"},
   {"400", "Bad Request - The syntax of the request is invalid"},
   {"401", "Unauthorized - Request needs user authentication"},
   {"402", "Payment Required"},
   {"403", "Forbidden -  Server is refusing to respond to it"},
   {"404", "Document Not Found - Requested resource could not be found"},
   {"405", "Method Not Allowed - Request method not supported"},
   {"406", "Not Acceptable"},
   {"407", "Proxy Authentication Required"},
   {"408", "Request Timeout - The server timed out waiting for the request"},
   {"409", "Conflict - Conflict in the request"},
   {"410", "Gone - Resource requested is no longer available"},
   {"411", "Length Required - Invalid Content-Length"},
   {"412", "Precondition Failed - Server does not meet preconditions"},
   {"413", "Requested Entity Too Long"},
   {"414", "Requested Filename Too Long"},
   {"415", "Unsupported Media Type - Media type is not supported"},
   {"416", "Requested Range Not Satisfiable - Cannot supply that portion"},
   {"417", "Expectation Failed"},
   {"500", "Internal Server Error"},
   {"501", "Not Implemented"},
   {"502", "Bad Gateway - Received an invalid response from the upstream"},
   {"503", "Service Unavailable - The server is currently unavailable"},
   {"504", "Gateway Timeout - The upstream server failed to send request"},
   {"505", "HTTP Version Not Supported"}
};

char *help_main[] = {
   "Copyright (C) 2010",
   "by Gerardo Orellana <goaccess@prosoftcorp.com>",
   "http://goaccess.prosoftcorp.com",
   "Released under the GNU GPL. See `man` page for",
   "more details.",
   "",
   "GoAccess is an open source and real-time Apache",
   "web log analyzer that provides fast and valuable",
   "HTTP statistics for system administrators that ",
   "require a visual and interactive report on the fly.",
   "",
   "The data collected based on the parsing of the log",
   "is divided into different modules. Modules are",
   "automatically generated and presented to the user.",
   "",
   "The main window displays general statistics, top ",
   "visitors, requests, browsers, operating systems,",
   "hosts, etc.",
   "",
   "The user can make use of the following keys:",
   " ^F1^ or ^CTRL^ + ^h^ [main help]",
   " ^F5^    redraw [main window]",
   " ^q^     quit the program/current window",
   " ^o^     open detail view for selected module",
   " ^c^     set or change scheme color",
   " ^RIGHT ARROW^ open detail view for selected",
   "         module",
   " ^TAB^   iterate modules (forward)",
   " ^SHIFT^ + ^TAB^ iterate modules (backward)",
   " ^0-9^   select module so the user can open a",
   "         [detail view] with either ^o^ or",
   "         ^RIGHT ARROW^",
   " ^SHIFT^ + ^0-9^ select module above 10",
   " ^s^     [detail view] sort unique visitors by date",
   " ^S^     [detail view] sort unique visitors by hits",
   " ^/^     [detail view] search forward for the ",
   "         occurrence of pattern",
   " ^n^     detail view - find the position of the next",
   "         occurrence",
   " ^t^     [detail view] move to the first item",
   " ^b^     [detail view] move to the last item",
   "",
   "If you believe you have found a bug, please drop me",
   "an email with details.",
   "",
   "If you have a medium or high traffic website, it",
   "would be interesting to hear your experience with",
   "GoAccess, such as generating time, visitors or hits.",
   "",
   "Feedback? Just shoot me an email to:",
   "goaccess@prosoftcorp.com",
};

void
init_colors (void)
{
   use_default_colors ();
   init_pair (COL_BLUE, COLOR_BLUE, -1);
   if (color_scheme == MONOCHROME)
      init_pair (COL_GREEN, COLOR_WHITE, -1);
   else
      init_pair (COL_GREEN, COLOR_GREEN, -1);
   init_pair (COL_RED, COLOR_RED, -1);
   init_pair (COL_BLACK, COLOR_BLACK, -1);
   init_pair (COL_CYAN, COLOR_CYAN, -1);
   init_pair (COL_YELLOW, COLOR_YELLOW, -1);
   if (color_scheme == MONOCHROME)
      init_pair (BLUE_GREEN, COLOR_BLUE, COLOR_WHITE);
   else
      init_pair (BLUE_GREEN, COLOR_BLUE, COLOR_GREEN);
   init_pair (BLACK_GREEN, COLOR_BLACK, COLOR_GREEN);
   init_pair (BLACK_CYAN, COLOR_BLACK, COLOR_CYAN);
   init_pair (WHITE_RED, COLOR_WHITE, COLOR_RED);
}

size_t
os_size ()
{
   return sizeof (os) / sizeof (os[0]);
}

size_t
browsers_size ()
{
   return sizeof (browsers) / sizeof (browsers[0]);
}

size_t
codes_size ()
{
   return sizeof (codes) / sizeof (codes[0]);
}

size_t
help_main_size ()
{
   return sizeof (help_main) / sizeof (help_main[0]);
}

size_t
keywords_size ()
{
   return sizeof (conf_keywords) / sizeof (conf_keywords[0]);
}
