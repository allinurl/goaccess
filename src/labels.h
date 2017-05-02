/**
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

#ifndef LABELS_H_INCLUDED
#define LABELS_H_INCLUDED

#include <libintl.h>

#define _(String) dgettext (PACKAGE , String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

/* General */
#define GEN_EXPAND_PANEL         _( "Exp. Panel")
#define GEN_HELP                 _( "Help")
#define GEN_QUIT                 _( "Quit")
#define GEN_TOTAL                _( "Total")

/* Sort Labels */
#define SORT_ASC_SEL             _( "[x] ASC [ ] DESC")
#define SORT_DESC_SEL            _( "[ ] ASC [x] DESC")

/* Overall Stats Labels */
#define T_DASH                   _( "Dashboard")
#define T_DASH_HEAD              _( "Dashboard - Overall Analyzed Requests")
#define T_HEAD                   N_( "Overall Analyzed Requests")

#define T_DATETIME               _( "Date/Time")
#define T_REQUESTS               _( "Total Requests")
#define T_GEN_TIME               _( "Init. Proc. Time")
#define T_FAILED                 _( "Failed Requests")
#define T_VALID                  _( "Valid Requests")
#define T_UNIQUE_VISITORS        _( "Unique Visitors")
#define T_UNIQUE_FILES           _( "Unique Files")
#define T_EXCLUDE_IP             _( "Excl. IP Hits")
#define T_REFERRER               _( "Referrers")
#define T_UNIQUE404              _( "Unique 404")
#define T_STATIC_FILES           _( "Static Files")
#define T_LOG                    _( "Log Size")
#define T_BW                     _( "Bandwidth")
#define T_LOG_PATH               _( "Log Source")

/* Metric Labels */
#define MTRC_HITS_LBL            _( "Hits")
#define MTRC_HITS_PERC_LBL       _( "h%")
#define MTRC_VISITORS_LBL        _( "Visitors")
#define MTRC_VISITORS_SHORT_LBL  _( "Vis.")
#define MTRC_VISITORS_PERC_LBL   _( "v%")
#define MTRC_BW_LBL              _( "Bandwidth")
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
  _(""[UP/DOWN] to scroll - [q] to close window")

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
#define BUILT_WITH_TCBTREE             \
  _("Built using Tokyo Cabinet on-disk B+ Tree.")
#define BUILT_WITH_TCMEMHASH           \
  _("Built using Tokyo Cabinet in-memory hash database.")
#define BUILT_WITH_DEFHASH             \
  _("Built using the default in-memory hash database.")

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

#endif // for #ifndef LABELS_H
