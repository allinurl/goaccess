/** 
 * commons.c -- holds different data types 
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
 * @version 0.1.1
 * Last Modified: Saturday, July 10, 2010
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

#include <time.h>
#include <glib.h>

#include "commons.h"

/* Definitions checked against declarations */
GHashTable *ht_unique_visitors 	= NULL;
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

char *module_names[] 	= { 
	"  Visitors - ^s^ sort by date - ^S^ sort by hits", 
	"  Requests", "  Static Requests", "  Referrers", 
	"  404 - Not Found", "  Operating Systems ordered by unique hits", 
	"  Browsers ordered by unique hits", "  Hosts", 
	"  HTTP Status Codes", "  Top Referring Sites" 
};

const char *os[] 		= { 
	"Windows", "Debian", "Ubuntu", "Fedora", "Android", "Centos", 
	"Gentoo", "Linux", "linux", "iPad", "Macintosh", "BSD", "SunOS", 
	"iPhone", "Amiga", "BlackBerry", "SymbianOS", 
	"PlayStation Portable", "DoCoMo" 
};

const char *browsers[]  = {
    "MSIE 6.0", "MSIE 7.0", "MSIE 8.0", "MSIE",
    "Firefox", "Netscape", "SVN", "Opera"
    "Chrome", "Safari", "Konqueror", "Media Center PC",
    "Wget", "w3m", "Links", "Lynx",
    "Camino", "Dillo", "Kazehakase", "msnbot-media",
    "msnbot", "ia_archiver", "Mediapartners-Google", "TwitterFeed",
    "Googlebot-Image", "Googlebot", "slurp", "Baiduspider",
    "SeaMonkey", "Gigabot", "Twiceler", "Ask Jeeves",
    "Iceape", "Nokia", "Donzilla", "Kazehakase",
    "Galeon", "Exabot", "K-Meleon", "Avant",
    "curl", "archive.org_bot", "BrowseX", "ELinks",
    "Iceweasel","Google Wireless Transcoder", "Google-Sitemaps",
    "Mosaic", "Songbird", "W3C_Validator", "Jigsaw",
    "Flock", "Minefield", "Midori", "Opera Mini"
};

char *codes[][2] = 
{
	{ "100", "Continue - Server has received the request headers" },
	{ "101", "Switching Protocols - Requester has asked the server to switch protocols" },
	{ "200", "OK -  Successful HTTP requests" },
	{ "201", "Created -  The request has been fulfilled and created" },
	{ "202", "Accepted - The request has been accepted for processing" },
	{ "203", "Non-Authoritative Information" },
	{ "204", "No Content - Request is not returning any content" },
	{ "205", "Reset Content - Response requires requester reset the document view" },
	{ "206", "Partial Content - The server is delivering only part of the resource" },
	{ "300", "Multiple Choices - Multiple options for the resource" },
	{ "301", "Moved Permanently - Future requests should be directed to the given URI" },
	{ "302", "302 Found - Moved Temporarily" },
	{ "303", "303 See Other - Response to the request can be found under another URI" },
	{ "304", "Not Modified - Resource has not been modified since last requested" },
	{ "305", "Use Proxy - HTTP/1.1" },
	{ "307", "Temporary Redirect - Request should be repeated with another URI" },
	{ "400", "Bad Request - Request contains bad syntax or cannot be fulfilled" },
	{ "401", "Unauthorized - Authentication has failed or not yet been provided" },
	{ "402", "Payment Required" },
	{ "403", "Forbidden -  Server is refusing to respond to it" },
	{ "404", "Not Found - Requested resource could not be found" },
	{ "405", "Method Not Allowed - Request method not supported by that resource" },
	{ "406", "Not Acceptable - " },
	{ "407", "Proxy Authentication Required" },
	{ "408", "Request Timeout - The server timed out waiting for the request" },
	{ "409", "Conflict - Conflict in the request" },
	{ "410", "Gone - Resource requested is no longer available" },
	{ "411", "Length Required - Request did not specify the length of its content" },
	{ "412", "Precondition Failed - Server does not meet one of the preconditions" },
	{ "413", "Request Entity Too Large - request is larger than the server is able to process" },
	{ "414", "Request-URI Too Long - The URI provided was too long for the server to process" },
	{ "415", "Unsupported Media Type - Media type which the server or resource does not support" },
	{ "416", "Requested Range Not Satisfiable - Cannot supply that portion" },
	{ "417", "Expectation Failed" },
	{ "500", "Internal Server Error" },
	{ "502", "Bad Gateway - Received an invalid response from the upstream server" },
	{ "503", "Service Unavailable - The server is currently unavailable" },
	{ "504", "Gateway Timeout - Did not receive a timely request from the upstream server" },
	{ "505", "HTTP Version Not Supported" }
};

char *help_main[] = {
	"Copyright (C) 2010",
	"by Gerardo Orellana <goaccess@prosoftcorp.com>",
	"http://goaccess.prosoftcorp.com",
	"Released under the GNU GPL. See `man` page for",
	"more details.",
	"",
	"GoAccess is an open source Apache web log analyzer",
	"that provides fast and valuable HTTP statistics",
	"for system administrators that require a visual",
	"report on the fly.",
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
	" ^F1^    main help",
	" ^q^     quit the program/current window",
	" ^o^     open detail view for selected module",
	" ^TAB^   iterate modules (forward)",
	" ^SHIFT^ + ^TAB^ iterate modules (backward)",
	" ^RIGHT ARROW^ open detail view for selected",
	"         module",
	" ^0-9^   select module so the user can open a",
	"         [detail view] with either ^o^ or", 
	"         ^RIGHT ARROW^",
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

size_t os_size()
{
  return sizeof( os ) / sizeof( os[0] );
}

size_t browsers_size()
{
  return sizeof( browsers ) / sizeof( browsers[0] );
}

size_t codes_size()
{
  return sizeof( codes ) / sizeof( codes[0] );
}

size_t help_main_size()
{
  return sizeof( help_main ) / sizeof( help_main[0] );
}
