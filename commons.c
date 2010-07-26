/** 
 * commons.c -- holds different data types 
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
 * @version 0.2
 * Last Modified: Sunday, July 25, 2010
 * Path:  /commons.c
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * GoAccess is released under the GNU/GPL License.
 * Copy of the GNU General Public License is attached to this source 
 * distribution for its full text.
 *
 * Visit http://goaccess.prosoftcorp.com for new releases.
 */

#include <curses.h>
#include <time.h>
#include <glib.h>

#include "commons.h"

#define COL_WHITE    0
#define COL_BLUE     1
#define COL_GREEN    11
#define COL_RED      3
#define COL_BLACK    4
#define COL_CYAN     5
#define COL_YELLOW   6
#define BLUE_GREEN   7
#define BLACK_GREEN  8
#define BLACK_CYAN   9
#define WHITE_RED    10

/* Definitions checked against declarations */
GHashTable *ht_unique_visitors = NULL;
GHashTable *ht_requests = NULL;
GHashTable *ht_referers = NULL;
GHashTable *ht_unique_vis = NULL;
GHashTable *ht_requests_static = NULL;
GHashTable *ht_not_found_requests = NULL;
GHashTable *ht_os = NULL;
GHashTable *ht_browsers = NULL;
GHashTable *ht_hosts = NULL;
GHashTable *ht_status_code = NULL;
GHashTable *ht_referring_sites = NULL;
GHashTable *ht_keyphrases = NULL;

enum MODULES modules;

/* enable flags */
int stripped_flag = 0;
int bandwidth_flag = 0;
int ignore_flag = 0;
int http_status_code_flag = 0;
long long req_size = 0;

/* string processing */
char *req;
char *stripped_str = NULL;
char *status_code;
char *ignore_host;

/* Processing time */
time_t start_proc;
time_t end_proc;
time_t now;

/* resizing */
size_t term_h = 0;
size_t term_w = 0;
size_t real_size_y = 0;

/* file */
char *ifile = NULL;

char *module_names[] = {
    "  Visitors - ^s^ sort by date - ^S^ sort by hits",
    "  Requests", "  Static Requests", "  Referrers",
    "  404 - Not Found", "  Operating Systems ordered by unique hits",
    "  Browsers ordered by unique hits", "  Hosts",
    "  HTTP Status Codes", "  Top Referring Sites",
    "  Top Search Keyphrases (Google, Cache, Translate)"
};

const char *os[] = {
    "Windows", "Debian", "Ubuntu", "Fedora", "Android", "Centos",
    "Gentoo", "Linux", "linux", "iPad", "Macintosh", "BSD", "SunOS",
    "iPhone", "Amiga", "BlackBerry", "SymbianOS",
    "PlayStation Portable", "DoCoMo"
};

const char *browsers[] = {
    "MSIE 6.0", "MSIE 7.0", "MSIE 8.0", "MSIE",
    "Firefox", "Netscape", "SVN", "Opera",
    "Chrome", "Safari", "Konqueror", "Media Center PC",
    "Wget", "w3m", "Links", "Lynx",
    "Camino", "Dillo", "Kazehakase", "msnbot-media",
    "msnbot", "ia_archiver", "Mediapartners-Google", "TwitterFeed",
    "Googlebot-Image", "Googlebot", "slurp", "Baiduspider",
    "SeaMonkey", "Gigabot", "Twiceler", "Ask Jeeves",
    "Iceape", "Nokia", "Donzilla", "Kazehakase",
    "Galeon", "Exabot", "K-Meleon", "Avant",
    "curl", "archive.org_bot", "BrowseX", "ELinks",
    "Iceweasel", "Google Wireless Transcoder", "Google-Sitemaps",
    "Mosaic", "Songbird", "W3C_Validator", "Jigsaw",
    "Flock", "Minefield", "Midori", "Opera Mini"
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
    {"302", "302 Found - Moved Temporarily"},
    {"303", "303 See Other - The response is at a different URI"},
    {"304", "Not Modified - Resource has not been modified"},
    {"305", "Use Proxy - Can only be accessed through the proxy"},
    {"307", "Temporary Redirect - Resource temporarily moved"},
    {"400", "Bad Request - The syntax of the request is invalid"},
    {"401", "Unauthorized - Request needs user authentication"},
    {"402", "Payment Required"},
    {"403", "Forbidden -  Server is refusing to respond to it"},
    {"404", "Not Found - Requested resource could not be found"},
    {"405", "Method Not Allowed - Request method not supported"},
    {"406", "Not Acceptable"},
    {"407", "Proxy Authentication Required"},
    {"408", "Request Timeout - The server timed out waiting for the request"},
    {"409", "Conflict - Conflict in the request"},
    {"410", "Gone - Resource requested is no longer available"},
    {"411", "Length Required - Invalid Content-Length"},
    {"412", "Precondition Failed - Server does not meet preconditions"},
    {"413", "Request entity Too Long"},
    {"414", "Request-URI Too Long"},
    {"415", "Unsupported Media Type - Media type is not supported"},
    {"416", "Requested Range Not Satisfiable - Cannot supply that portion"},
    {"417", "Expectation Failed"},
    {"500", "Internal Server Error"},
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
    "Send your questions, comments and suggestions to ",
    "goaccess@prosoftcorp.com",
};

void
init_colors (void)
{
    use_default_colors ();
    init_pair (COL_BLUE, COLOR_BLUE, -1);
    init_pair (COL_GREEN, COLOR_GREEN, -1);
    init_pair (COL_RED, COLOR_RED, -1);
    init_pair (COL_BLACK, COLOR_BLACK, -1);
    init_pair (COL_CYAN, COLOR_CYAN, -1);
    init_pair (COL_YELLOW, COLOR_YELLOW, -1);
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
