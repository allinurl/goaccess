/**
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

#ifndef LABELS_H_INCLUDED
#define LABELS_H_INCLUDED

#include <libintl.h>

#define _(String) dgettext (PACKAGE , String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

/* global lang attribute */
#define DOC_LANG                 _( "en")

/* General */
#define GEN_EXPAND_PANEL         _( "Exp. Panel")
#define GEN_HELP                 _( "Help")
#define GEN_QUIT                 _( "Quit")
#define GEN_TOTAL                _( "Total")

/* Sort Labels */
#define SORT_ASC_SEL             _( "[x] ASC [ ] DESC")
#define SORT_DESC_SEL            _( "[ ] ASC [x] DESC")

/* Overall Stats Labels */
#define T_ACTIVE_PANEL           _("[Active Panel: %1$s]")
#define T_QUIT                   _("[q]uit GoAccess")
#define T_HELP_ENTER             _("[?] Help [Enter] Exp. Panel")
#define T_DASH                   _( "Dashboard")
#define T_DASH_HEAD              _( "Dashboard - Overall Analyzed Requests")
#define T_HEAD                   N_( "Overall Analyzed Requests")

#define T_BW                     _( "Tx. Amount")
#define T_DATETIME               _( "Date/Time")
#define T_EXCLUDE_IP             _( "Excl. IP Hits")
#define T_FAILED                 _( "Failed Requests")
#define T_GEN_TIME               _( "Init. Proc. Time")
#define T_LOG                    _( "Log Size")
#define T_LOG_PATH               _( "Log Source")
#define T_REFERRER               _( "Referrers")
#define T_REQUESTS               _( "Total Requests")
#define T_STATIC_FILES           _( "Static Files")
#define T_UNIQUE404              _( "Not Found")
#define T_UNIQUE_FILES           _( "Requested Files")
#define T_UNIQUE_VISITORS        _( "Unique Visitors")
#define T_VALID                  _( "Valid Requests")

/* Metric Labels */
#define MTRC_HITS_LBL            _( "Hits")
#define MTRC_HITS_PERC_LBL       _( "h%")
#define MTRC_VISITORS_LBL        _( "Visitors")
#define MTRC_VISITORS_SHORT_LBL  _( "Vis.")
#define MTRC_VISITORS_PERC_LBL   _( "v%")
#define MTRC_BW_LBL              _( "Tx. Amount")
#define MTRC_AVGTS_LBL           _( "Avg. T.S.")
#define MTRC_CUMTS_LBL           _( "Cum. T.S.")
#define MTRC_MAXTS_LBL           _( "Max. T.S.")
#define MTRC_METHODS_LBL         _( "Method")
#define MTRC_METHODS_SHORT_LBL   _( "Mtd")
#define MTRC_PROTOCOLS_LBL       _( "Protocol")
#define MTRC_PROTOCOLS_SHORT_LBL _( "Proto")
#define MTRC_CITY_LBL            _( "City")
#define MTRC_COUNTRY_LBL         _( "Country")
#define MTRC_HOSTNAME_LBL        _( "Hostname")
#define MTRC_DATA_LBL            _( "Data")

#define HTML_PLOT_HITS_VIS       _( "Hits/Visitors")

/* Panel Labels and Descriptions */
#define VISITORS_HEAD                  \
  N_("Unique visitors per day")
#define VISITORS_HEAD_BOTS             \
  N_("Unique visitors per day - Including spiders")
#define VISITORS_DESC                  \
  N_("Hits having the same IP, date and agent are a unique visit.")
#define VISITORS_LABEL                 \
  N_("Visitors")

#define REQUESTS_HEAD                  \
  N_("Requested Files (URLs)")
#define REQUESTS_DESC                  \
  N_("Top requests sorted by hits [, avgts, cumts, maxts, mthd, proto]")
#define REQUESTS_LABEL                 \
  N_("Requests")

#define REQUESTS_STATIC_HEAD           \
  N_("Static Requests")
#define REQUESTS_STATIC_DESC           \
  N_("Top static requests sorted by hits [, avgts, cumts, maxts, mthd, proto]")
#define REQUESTS_STATIC_LABEL          \
  N_("Static Requests")

#define VISIT_TIMES_HEAD               \
  N_("Time Distribution")
#define VISIT_TIMES_DESC               \
  N_("Data sorted by hour [, avgts, cumts, maxts]")
#define VISIT_TIMES_LABEL              \
  N_("Time")

#define VIRTUAL_HOSTS_HEAD             \
  N_("Virtual Hosts")
#define VIRTUAL_HOSTS_DESC             \
  N_("Data sorted by hits [, avgts, cumts, maxts]")
#define VIRTUAL_HOSTS_LABEL            \
  N_("Virtual Hosts")

#define REMOTE_USER_HEAD               \
  N_("Remote User (HTTP authentication)")
#define REMOTE_USER_DESC               \
  N_("Data sorted by hits [, avgts, cumts, maxts]")
#define REMOTE_USER_LABEL              \
  N_("Remote User")

#define CACHE_STATUS_HEAD               \
  N_("The cache status of the object served")
#define CACHE_STATUS_DESC               \
  N_("Data sorted by hits [, avgts, cumts, maxts]")
#define CACHE_STATUS_LABEL              \
  N_("Cache Status")

#define NOT_FOUND_HEAD                 \
  N_("Not Found URLs (404s)")
#define NOT_FOUND_DESC                 \
  N_("Top not found URLs sorted by hits [, avgts, cumts, maxts, mthd, proto]")
#define NOT_FOUND_LABEL                \
  N_("Not Found")

#define HOSTS_HEAD                     \
  N_("Visitor Hostnames and IPs")
#define HOSTS_DESC                     \
  N_("Top visitor hosts sorted by hits [, avgts, cumts, maxts]")
#define HOSTS_LABEL                    \
  N_("Hosts")

#define OS_HEAD                        \
  N_("Operating Systems")
#define OS_DESC                        \
  N_("Top Operating Systems sorted by hits [, avgts, cumts, maxts]")
#define OS_LABEL                       \
  N_("OS")

#define BROWSERS_HEAD                  \
  N_("Browsers")
#define BROWSERS_DESC                  \
  N_("Top Browsers sorted by hits [, avgts, cumts, maxts]")
#define BROWSERS_LABEL                 \
  N_("Browsers")

#define REFERRERS_HEAD                 \
  N_("Referrers URLs")
#define REFERRERS_DESC                 \
  N_("Top Requested Referrers sorted by hits [, avgts, cumts, maxts]")
#define REFERRERS_LABEL                \
  N_("Referrers")

#define REFERRING_SITES_HEAD           \
  N_("Referring Sites")
#define REFERRING_SITES_DESC           \
  N_("Top Referring Sites sorted by hits [, avgts, cumts, maxts]")
#define REFERRING_SITES_LABEL          \
  N_("Referring Sites")

#define KEYPHRASES_HEAD                \
  N_("Keyphrases from Google's search engine")
#define KEYPHRASES_DESC                \
  N_("Top Keyphrases sorted by hits [, avgts, cumts, maxts]")
#define KEYPHRASES_LABEL               \
  N_("Keyphrases")

#define GEO_LOCATION_HEAD              \
  N_("Geo Location")
#define GEO_LOCATION_DESC              \
  N_("Continent > Country sorted by unique hits [, avgts, cumts, maxts]")
#define GEO_LOCATION_LABEL             \
  N_("Geo Location")

#define STATUS_CODES_HEAD              \
  N_("HTTP Status Codes")
#define STATUS_CODES_DESC              \
  N_("Top HTTP Status Codes sorted by hits [, avgts, cumts, maxts]")
#define STATUS_CODES_LABEL             \
  N_("Status Codes")

/* Find Labels */
#define CISENSITIVE                    \
  _("[ ] case sensitive")
#define CSENSITIVE                     \
  _("[x] case sensitive")
#define FIND_DESC                      \
  _("Regex allowed - ^g to cancel - TAB switch case")
#define FIND_HEAD                      \
  _("Find pattern in all views")

/* Config Dialog */
#define CONFDLG_HEAD                   \
  _("Log Format Configuration")
#define CONFDLG_KEY_HINTS              \
  _("[SPACE] to toggle - [ENTER] to proceed - [q] to quit")
#define CONFDLG_LOG_FORMAT             \
  _("Log Format - [c] to add/edit format")
#define CONFDLG_DATE_FORMAT            \
  _("Date Format - [d] to add/edit format")
#define CONFDLG_TIME_FORMAT            \
  _("Time Format - [t] to add/edit format")
#define CONFDLG_DESC                   \
  _("[UP/DOWN] to scroll - [q] to close window")

/* Agents Dialog */
#define AGENTSDLG_DESC                 \
  _("[UP/DOWN] to scroll - [q] to close window")
#define AGENTSDLG_HEAD                 \
  _("User Agents for %1$s")

/* Color Scheme Dialog */
#define SCHEMEDLG_HEAD                 \
  _("Scheme Configuration")
#define SCHEMEDLG_DESC                 \
  _("[ENTER] to use scheme - [q]uit")

/* Sort Dialog */
#define SORTDLG_HEAD                   \
  _("Sort active module by")
#define SORTDLG_DESC                   \
  _("[ENTER] select - [TAB] sort - [q]uit")

/* Help TUI Dialog */
#define HELPDLG_HEAD                   \
  _("GoAccess Quick Help")
#define HELPDLG_DESC                   \
  _("[UP/DOWN] to scroll - [q] to quit")

/* Storage Built-in Option */
#define BUILT_WITH_DEFHASH             \
  _("In-Memory with On-Disk Persistent Storage.")

/* Common UI Errors */
#define ERR_FORMAT_HEADER              \
  _("Format Errors - Verify your log/date/time format")
#define ERR_FORMAT_NO_DATE_FMT         \
  _("No date format was found on your conf file.")
#define ERR_FORMAT_NO_LOG_FMT          \
  _("No log format was found on your conf file.")
#define ERR_FORMAT_NO_TIME_FMT         \
  _("No time format was found on your conf file.")
#define ERR_NODEF_CONF_FILE            \
  _("No default config file found.")
#define ERR_NODEF_CONF_FILE_DESC       \
  _("You may specify one with")
#define ERR_PARSED_NLINES_DESC         \
  _("producing the following errors")
#define ERR_PARSED_NLINES              \
  _("Parsed %1$d lines")
#define ERR_PLEASE_REPORT              \
  _("Please report it by opening an issue on GitHub")
#define ERR_FORMAT_NO_TIME_FMT_DLG     \
  _("Select a time format.")
#define ERR_FORMAT_NO_DATE_FMT_DLG     \
  _("Select a date format.")
#define ERR_FORMAT_NO_LOG_FMT_DLG      \
  _("Select a log format.")
#define ERR_PANEL_DISABLED             \
  _("'%1$s' panel is disabled")

/* Other */
#define INFO_MORE_INFO                 \
  _("For more details visit")
#define INFO_LAST_UPDATED              \
  _("Last Updated")
#define INFO_WS_READY_FOR_CONN         \
  _("WebSocket server ready to accept new client connections")

#define INFO_HELP_FOLLOWING_OPTS       \
  _("The following options can also be supplied to the command")
#define INFO_HELP_EXAMPLES             \
  _("Examples can be found by running")

#define HTML_REPORT_TITLE              \
  _( "Server Statistics")
#define HTML_REPORT_NAV_THEME          \
  N_("Theme")
#define HTML_REPORT_NAV_DARK_GRAY      \
  N_("Dark Gray")
#define HTML_REPORT_NAV_BRIGHT         \
  N_("Bright")
#define HTML_REPORT_NAV_DARK_BLUE      \
  N_("Dark Blue")
#define HTML_REPORT_NAV_DARK_PURPLE    \
  N_("Dark Purple")
#define HTML_REPORT_NAV_PANELS         \
  N_("Panels")
#define HTML_REPORT_NAV_ITEMS_PER_PAGE \
  N_("Items per Page")
#define HTML_REPORT_NAV_TABLES         \
  N_("Tables")
#define HTML_REPORT_NAV_DISPLAY_TABLES \
  N_("Display Tables")
#define HTML_REPORT_NAV_AH_SMALL       \
  N_("Auto-Hide on Small Devices")
#define HTML_REPORT_NAV_AH_SMALL_TITLE \
  N_("Automatically hide tables on small screen devices")
#define HTML_REPORT_NAV_LAYOUT         \
  N_("Layout")
#define HTML_REPORT_NAV_HOR            \
  N_("Horizontal")
#define HTML_REPORT_NAV_VER            \
  N_("Vertical")
#define HTML_REPORT_NAV_WIDE           \
  N_("WideScreen")
#define HTML_REPORT_NAV_FILE_OPTS      \
  N_("File Options")
#define HTML_REPORT_NAV_EXPORT_JSON    \
  N_("Export as JSON")
#define HTML_REPORT_PANEL_PANEL_OPTS   \
  N_("Panel Options")
#define HTML_REPORT_PANEL_PREVIOUS     \
  N_("Previous")
#define HTML_REPORT_PANEL_NEXT         \
  N_("Next")
#define HTML_REPORT_PANEL_FIRST        \
  N_("First")
#define HTML_REPORT_PANEL_LAST         \
  N_("Last")
#define HTML_REPORT_PANEL_CHART_OPTS   \
  N_("Chart Options")
#define HTML_REPORT_PANEL_CHART        \
  N_("Chart")
#define HTML_REPORT_PANEL_TYPE         \
  N_("Type")
#define HTML_REPORT_PANEL_AREA_SPLINE  \
  N_("Area Spline")
#define HTML_REPORT_PANEL_BAR          \
  N_("Bar")
#define HTML_REPORT_PANEL_PLOT_METRIC  \
  N_("Plot Metric")
#define HTML_REPORT_PANEL_TABLE_COLS   \
  N_("Table Columns")

/* Status Codes */
#define STATUS_CODE_1XX               \
  N_("1xx Informational")
#define STATUS_CODE_2XX               \
  N_("2xx Success")
#define STATUS_CODE_3XX               \
  N_("3xx Redirection")
#define STATUS_CODE_4XX               \
  N_("4xx Client Errors")
#define STATUS_CODE_5XX               \
  N_("5xx Server Errors")

#define STATUS_CODE_100               \
  N_("100 - Continue: Server received the initial part of the request")
#define STATUS_CODE_101               \
  N_("101 - Switching Protocols: Client asked to switch protocols")
#define STATUS_CODE_200               \
  N_("200 - OK: The request sent by the client was successful")
#define STATUS_CODE_201               \
  N_("201 - Created: The request has been fulfilled and created")
#define STATUS_CODE_202               \
  N_("202 - Accepted: The request has been accepted for processing")
#define STATUS_CODE_203               \
  N_("203 - Non-authoritative Information: Response from a third party")
#define STATUS_CODE_204               \
  N_("204 - No Content: Request did not return any content")
#define STATUS_CODE_205               \
  N_("205 - Reset Content: Server asked the client to reset the document")
#define STATUS_CODE_206               \
  N_("206 - Partial Content: The partial GET has been successful")
#define STATUS_CODE_207               \
  N_("207 - Multi-Status: WebDAV; RFC 4918")
#define STATUS_CODE_208               \
  N_("208 - Already Reported: WebDAV; RFC 5842")
#define STATUS_CODE_300               \
  N_("300 - Multiple Choices: Multiple options for the resource")
#define STATUS_CODE_301               \
  N_("301 - Moved Permanently: Resource has permanently moved")
#define STATUS_CODE_302               \
  N_("302 - Moved Temporarily (redirect)")
#define STATUS_CODE_303               \
  N_("303 - See Other Document: The response is at a different URI")
#define STATUS_CODE_304               \
  N_("304 - Not Modified: Resource has not been modified")
#define STATUS_CODE_305               \
  N_("305 - Use Proxy: Can only be accessed through the proxy")
#define STATUS_CODE_307               \
  N_("307 - Temporary Redirect: Resource temporarily moved")
#define STATUS_CODE_400               \
  N_("400 - Bad Request: The syntax of the request is invalid")
#define STATUS_CODE_401               \
  N_("401 - Unauthorized: Request needs user authentication")
#define STATUS_CODE_402               \
  N_("402 - Payment Required")
#define STATUS_CODE_403               \
  N_("403 - Forbidden: Server is refusing to respond to it")
#define STATUS_CODE_404               \
  N_("404 - Not Found: Requested resource could not be found")
#define STATUS_CODE_405               \
  N_("405 - Method Not Allowed: Request method not supported")
#define STATUS_CODE_406               \
  N_("406 - Not Acceptable")
#define STATUS_CODE_407               \
  N_("407 - Proxy Authentication Required")
#define STATUS_CODE_408               \
  N_("408 - Request Timeout: Server timed out waiting for the request")
#define STATUS_CODE_409               \
  N_("409 - Conflict: Conflict in the request")
#define STATUS_CODE_410               \
  N_("410 - Gone: Resource requested is no longer available")
#define STATUS_CODE_411               \
  N_("411 - Length Required: Invalid Content-Length")
#define STATUS_CODE_412               \
  N_("412 - Precondition Failed: Server does not meet preconditions")
#define STATUS_CODE_413               \
  N_("413 - Payload Too Large")
#define STATUS_CODE_414               \
  N_("414 - Request-URI Too Long")
#define STATUS_CODE_415               \
  N_("415 - Unsupported Media Type: Media type is not supported")
#define STATUS_CODE_416               \
  N_("416 - Requested Range Not Satisfiable: Cannot supply that portion")
#define STATUS_CODE_417               \
  N_("417 - Expectation Failed")
#define STATUS_CODE_418               \
  N_("418 - I’m a teapot")
#define STATUS_CODE_421               \
  N_("421 - Misdirected Request")
#define STATUS_CODE_422               \
  N_("422 - Unprocessable Entity due to semantic errors: WebDAV")
#define STATUS_CODE_423               \
  N_("423 - The resource that is being accessed is locked")
#define STATUS_CODE_424               \
  N_("424 - Failed Dependency: WebDAV")
#define STATUS_CODE_426               \
  N_("426 - Upgrade Required: Client should switch to a different protocol")
#define STATUS_CODE_428               \
  N_("428 - Precondition Required")
#define STATUS_CODE_429               \
  N_("429 - Too Many Requests: The user has sent too many requests")
#define STATUS_CODE_431               \
  N_("431 - Request Header Fields Too Large")
#define STATUS_CODE_451               \
  N_("451 - Unavailable For Legal Reasons")
#define STATUS_CODE_444               \
  N_("444 - (Nginx) Connection closed without sending any headers")
#define STATUS_CODE_494               \
  N_("494 - (Nginx) Request Header Too Large")
#define STATUS_CODE_495               \
  N_("495 - (Nginx) SSL client certificate error")
#define STATUS_CODE_496               \
  N_("496 - (Nginx) Client didn't provide certificate")
#define STATUS_CODE_497               \
  N_("497 - (Nginx) HTTP request sent to HTTPS port")
#define STATUS_CODE_499               \
  N_("499 - (Nginx) Connection closed by client while processing request")
#define STATUS_CODE_500               \
  N_("500 - Internal Server Error")
#define STATUS_CODE_501               \
  N_("501 - Not Implemented")
#define STATUS_CODE_502               \
  N_("502 - Bad Gateway: Received an invalid response from the upstream")
#define STATUS_CODE_503               \
  N_("503 - Service Unavailable: The server is currently unavailable")
#define STATUS_CODE_504               \
  N_("504 - Gateway Timeout: The upstream server failed to send request")
#define STATUS_CODE_505               \
  N_("505 - HTTP Version Not Supported")
#define STATUS_CODE_520               \
  N_("520 - CloudFlare - Web server is returning an unknown error")
#define STATUS_CODE_521               \
  N_("521 - CloudFlare - Web server is down")
#define STATUS_CODE_522               \
  N_("522 - CloudFlare - Connection timed out")
#define STATUS_CODE_523               \
  N_("523 - CloudFlare - Origin is unreachable")
#define STATUS_CODE_524               \
  N_("524 - CloudFlare - A timeout occurred")

#endif // for #ifndef LABELS_H
