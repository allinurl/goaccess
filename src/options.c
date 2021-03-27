/**
 * options.c -- functions related to parsing program options
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#ifdef HAVE_LIBGEOIP
#include <GeoIP.h>
#endif

#include "options.h"

#include "error.h"
#include "labels.h"
#include "util.h"

#include "xmalloc.h"

static char short_options[] = "f:e:p:o:l:H:M:S:b:"
#ifdef HAVE_LIBGEOIP
  "g"
#endif
  "acirmMhHqdsV";

/* *INDENT-OFF* */
struct option long_opts[] = {
  {"agent-list"           , no_argument       , 0 , 'a' } ,
  {"browsers-file"        , required_argument , 0 , 'b' } ,
  {"config-dialog"        , no_argument       , 0 , 'c' } ,
  {"config-file"          , required_argument , 0 , 'p' } ,
  {"debug-file"           , required_argument , 0 , 'l' } ,
  {"exclude-ip"           , required_argument , 0 , 'e' } ,
#ifdef HAVE_LIBGEOIP
  {"std-geoip"            , no_argument       , 0 , 'g' } ,
#endif
  {"help"                 , no_argument       , 0 , 'h' } ,
  {"hl-header"            , no_argument       , 0 , 'i' } ,
  {"http-method"          , required_argument , 0 , 'M' } ,
  {"http-protocol"        , required_argument , 0 , 'H' } ,
  {"log-file"             , required_argument , 0 , 'f' } ,
  {"log-size"             , required_argument , 0 , 'S' } ,
  {"no-query-string"      , no_argument       , 0 , 'q' } ,
  {"no-term-resolver"     , no_argument       , 0 , 'r' } ,
  {"output-format"        , required_argument , 0 , 'o' } ,
  {"storage"              , no_argument       , 0 , 's' } ,
  {"version"              , no_argument       , 0 , 'V' } ,
  {"with-mouse"           , no_argument       , 0 , 'm' } ,
  {"with-output-resolver" , no_argument       , 0 , 'd' } ,
  {"444-as-404"           , no_argument       , 0 , 0  }  ,
  {"4xx-to-unique-count"  , no_argument       , 0 , 0  }  ,
  {"addr"                 , required_argument , 0 , 0  }  ,
  {"unix-socket"          , required_argument , 0 , 0  }  ,
  {"all-static-files"     , no_argument       , 0 , 0  }  ,
  {"anonymize-ip"         , no_argument       , 0 , 0  }  ,
  {"color"                , required_argument , 0 , 0  }  ,
  {"color-scheme"         , required_argument , 0 , 0  }  ,
  {"crawlers-only"        , no_argument       , 0 , 0  }  ,
  {"daemonize"            , no_argument       , 0 , 0  }  ,
  {"date-format"          , required_argument , 0 , 0  }  ,
  {"date-spec"            , required_argument , 0 , 0  }  ,
  {"db-path"              , required_argument , 0 , 0  }  ,
  {"dcf"                  , no_argument       , 0 , 0  }  ,
  {"double-decode"        , no_argument       , 0 , 0  }  ,
  {"enable-panel"         , required_argument , 0 , 0  }  ,
  {"fifo-in"              , required_argument , 0 , 0  }  ,
  {"fifo-out"             , required_argument , 0 , 0  }  ,
  {"hide-referer"         , required_argument , 0 , 0  }  ,
  {"hour-spec"            , required_argument , 0 , 0  }  ,
  {"html-custom-css"      , required_argument , 0 , 0  }  ,
  {"html-custom-js"       , required_argument , 0 , 0  }  ,
  {"html-prefs"           , required_argument , 0 , 0  }  ,
  {"html-report-title"    , required_argument , 0 , 0  }  ,
  {"ignore-crawlers"      , no_argument       , 0 , 0  }  ,
  {"ignore-panel"         , required_argument , 0 , 0  }  ,
  {"ignore-referer"       , required_argument , 0 , 0  }  ,
  {"ignore-statics"       , required_argument , 0 , 0  }  ,
  {"ignore-status"        , required_argument , 0 , 0  }  ,
  {"invalid-requests"     , required_argument , 0 , 0  }  ,
  {"unknowns-log"         , required_argument , 0 , 0  }  ,
  {"json-pretty-print"    , no_argument       , 0 , 0  }  ,
  {"keep-last"            , required_argument , 0 , 0  }  ,
  {"html-refresh"         , required_argument , 0 , 0  }  ,
  {"log-format"           , required_argument , 0 , 0  }  ,
  {"max-items"            , required_argument , 0 , 0  }  ,
  {"no-color"             , no_argument       , 0 , 0  }  ,
  {"no-strict-status"     , no_argument       , 0 , 0  }  ,
  {"no-column-names"      , no_argument       , 0 , 0  }  ,
  {"no-csv-summary"       , no_argument       , 0 , 0  }  ,
  {"no-global-config"     , no_argument       , 0 , 0  }  ,
  {"no-html-last-updated" , no_argument       , 0 , 0  }  ,
  {"no-ip-validation"     , no_argument       , 0 , 0  }  ,
  {"no-parsing-spinner"   , no_argument       , 0 , 0  }  ,
  {"no-progress"          , no_argument       , 0 , 0  }  ,
  {"no-tab-scroll"        , no_argument       , 0 , 0  }  ,
  {"num-tests"            , required_argument , 0 , 0  }  ,
  {"origin"               , required_argument , 0 , 0  }  ,
  {"output"               , required_argument , 0 , 0  }  ,
  {"persist"              , no_argument       , 0 , 0  }  ,
  {"pid-file"             , required_argument , 0 , 0  }  ,
  {"port"                 , required_argument , 0 , 0  }  ,
  {"process-and-exit"     , no_argument       , 0 , 0  }  ,
  {"real-os"              , no_argument       , 0 , 0  }  ,
  {"real-time-html"       , no_argument       , 0 , 0  }  ,
  {"restore"              , no_argument       , 0 , 0  }  ,
  {"sort-panel"           , required_argument , 0 , 0  }  ,
  {"static-file"          , required_argument , 0 , 0  }  ,
  {"user-name"            , required_argument , 0 , 0  }  ,
#ifdef HAVE_LIBSSL
  {"ssl-cert"             , required_argument , 0 ,  0  } ,
  {"ssl-key"              , required_argument , 0 ,  0  } ,
#endif
  {"time-format"          , required_argument , 0 ,  0  } ,
  {"ws-url"               , required_argument , 0 ,  0  } ,
#ifdef HAVE_GEOLOCATION
  {"geoip-database"       , required_argument , 0 ,  0  } ,
#endif
  {0, 0, 0, 0}
};

/* Command line help. */
void
cmd_help (void)
{
  printf ("\nGoAccess - %s\n\n", GO_VERSION);
  printf (
  "Usage: "
  "goaccess [filename] [ options ... ] [-c][-M][-H][-S][-q][-d][...]\n"
  "%s:\n\n", INFO_HELP_FOLLOWING_OPTS);

  printf (
  /* Log & Date Format Options */
  CYN "LOG & DATE FORMAT OPTIONS\n\n" RESET
  "  --date-format=<dateformat>      - Specify log date format. e.g., %%d/%%b/%%Y\n"
  "  --log-format=<logformat>        - Specify log format. Inner quotes need escaping, or use single quotes.\n"
  "  --time-format=<timeformat>      - Specify log time format. e.g., %%H:%%M:%%S\n\n"
  "\n"
  /* User Interface Options */
  CYN "USER INTERFACE OPTIONS\n\n" RESET
  "  -c --config-dialog              - Prompt log/date/time configuration window.\n"
  "  -i --hl-header                  - Color highlight active panel.\n"
  "  -m --with-mouse                 - Enable mouse support on main dashboard.\n"
  "  --color=<fg:bg[attrs, PANEL]>   - Specify custom colors. See manpage for more details.\n"
  "  --color-scheme=<1|2|3>          - Schemes: 1 => Grey, 2 => Green, 3 => Monokai.\n"
  "  --html-custom-css=<path.css>    - Specify a custom CSS file in the HTML report.\n"
  "  --html-custom-js=<path.js>      - Specify a custom JS file in the HTML report.\n"
  "  --html-prefs=<json_obj>         - Set default HTML report preferences.\n"
  "  --html-report-title=<title>     - Set HTML report page title and header.\n"
  "  --html-refresh=<secs>           - Refresh HTML report every X seconds (>=1 or <=60).\n"
  "  --json-pretty-print             - Format JSON output w/ tabs & newlines.\n"
  "  --max-items                     - Maximum number of items to show per panel. See man page for limits.\n"
  "  --no-color                      - Disable colored output.\n"
  "  --no-column-names               - Don't write column names in term output.\n"
  "  --no-csv-summary                - Disable summary metrics on the CSV output.\n"
  "  --no-html-last-updated          - Hide HTML last updated field.\n"
  "  --no-parsing-spinner            - Disable progress metrics and parsing spinner.\n"
  "  --no-progress                   - Disable progress metrics.\n"
  "  --no-tab-scroll                 - Disable scrolling through panels on TAB.\n"
  "\n"
  ""
  /* Server Options */
  CYN "SERVER OPTIONS\n\n" RESET
  "  --addr=<addr>                   - Specify IP address to bind server to.\n"
  "  --unix-socket=<addr>            - Specify UNIX-domain socket address to bind server to.\n"
  "  --daemonize                     - Run as daemon (if --real-time-html enabled).\n"
  "  --fifo-in=<path>                - Path to read named pipe (FIFO).\n"
  "  --fifo-out=<path>               - Path to write named pipe (FIFO).\n"
  "  --origin=<addr>                 - Ensure clients send this origin header upon the WebSocket handshake.\n"
  "  --pid-file=<path>               - Write PID to a file when --daemonize is used.\n"
  "  --port=<port>                   - Specify the port to use.\n"
  "  --real-time-html                - Enable real-time HTML output.\n"
  "  --ssl-cert=<cert.crt>           - Path to TLS/SSL certificate.\n"
  "  --ssl-key=<priv.key>            - Path to TLS/SSL private key.\n"
  "  --user-name=<username>          - Run as the specified user.\n"
  "  --ws-url=<url>                  - URL to which the WebSocket server responds.\n"
  "\n"
  ""
  /* File Options */
  CYN "FILE OPTIONS\n\n" RESET
  "  -                               - The log file to parse is read from stdin.\n"
  "  -f --log-file=<filename>        - Path to input log file.\n"
  "  -l --debug-file=<filename>      - Send all debug messages to the specified file.\n"
  "  -p --config-file=<filename>     - Custom configuration file.\n"
  "  -S --log-size=<number>          - Specify the log size, useful when piping in logs.\n"
  "  --invalid-requests=<filename>   - Log invalid requests to the specified file.\n"
  "  --no-global-config              - Don't load global configuration file.\n"
  "  --unknowns-log=<filename>       - Log unknown browsers and OSs to the specified file.\n"
  "\n"
  ""
  /* Parse Options */
  CYN "PARSE OPTIONS\n\n" RESET
  "  -a --agent-list                 - Enable a list of user-agents by host.\n"
  "  -b --browsers-file=<path>       - Use additional custom list of browsers.\n"
  "  -d --with-output-resolver       - Enable IP resolver on HTML|JSON output.\n"
  "  -e --exclude-ip=<IP>            - Exclude one or multiple IPv4/6. Allows IP ranges\n"
  "                                    e.g. 192.168.0.1-192.168.0.10\n"
  "  -H --http-protocol=<yes|no>     - Set/unset HTTP request protocol if found.\n"
  "  -M --http-method=<yes|no>       - Set/unset HTTP request method if found.\n"
  "  -o --output=file.html|json|csv  - Output either an HTML, JSON or a CSV file.\n"
  "  -q --no-query-string            - Strip request's query string. This can decrease memory consumption.\n"
  "  -r --no-term-resolver           - Disable IP resolver on terminal output.\n"
  "  --444-as-404                    - Treat non-standard status code 444 as 404.\n"
  "  --4xx-to-unique-count           - Add 4xx client errors to the unique visitors count.\n"
  "  --all-static-files              - Include static files with a query string.\n"
  "  --anonymize-ip                  - Anonymize IP addresses before outputting to report.\n"
  "  --crawlers-only                 - Parse and display only crawlers.\n"
  "  --date-spec=<date|hr>           - Date specificity. Possible values: `date` (default), or `hr`.\n"
  "  --double-decode                 - Decode double-encoded values.\n"
  "  --enable-panel=<PANEL>          - Enable parsing/displaying the given panel.\n"
  "  --hide-referer=<NEEDLE>         - Hide a referer but still count it. Wild cards are allowed.\n"
  "                                    i.e., *.bing.com\n"
  "  --hour-spec=<hr|min>            - Hour specificity. Possible values: `hr` (default),\n"
  "                                    or `min` (tenth of a min).\n"
  "  --ignore-crawlers               - Ignore crawlers.\n"
  "  --ignore-panel=<PANEL>          - Ignore parsing/displaying the given panel.\n"
  "  --ignore-referer=<NEEDLE>       - Ignore a referer from being counted. Wild cards are allowed.\n"
  "                                    i.e., *.bing.com\n"
  "  --ignore-statics=<req|panel>    - Ignore static requests.\n"
  "                                    req => Ignore from valid requests.\n"
  "                                    panel => Ignore from valid requests and panels.\n"
  "  --ignore-status=<CODE>          - Ignore parsing the given status code.\n"
  "  --keep-last=<NDAYS>             - Keep the last NDAYS in storage.\n"
  "  --no-ip-validation              - Disable client IPv4/6  validation.\n"
  "  --no-strict-status              - Disable HTTP status code validation.\n"
  "  --num-tests=<number>            - Number of lines to test. >= 0 (10 default)\n"
  "  --persist                       - Persist data to disk on exit to the given --db-path or to /tmp.\n"
  "  --process-and-exit              - Parse log and exit without outputting data.\n"
  "  --real-os                       - Display real OS names. e.g, Windows XP, Snow Leopard.\n"
  "  --restore                       - Restore data from disk from the given --db-path or from /tmp.\n"
  "  --sort-panel=PANEL,METRIC,ORDER - Sort panel on initial load. e.g., --sort-panel=VISITORS,BY_HITS,ASC.\n"
  "                                    See manpage for a list of panels/fields.\n"
  "  --static-file=<extension>       - Add static file extension. e.g.: .mp3. Extensions are case sensitive.\n"
  "\n"

/* GeoIP Options */
#ifdef HAVE_GEOLOCATION
  CYN "GEOIP OPTIONS\n\n" RESET
#ifdef HAVE_LIBGEOIP
  "  -g --std-geoip                  - Standard GeoIP database for less memory consumption.\n"
#endif
  "  --geoip-database=<path>         - Specify path to GeoIP database file.\n"
  "                                    i.e., GeoLiteCity.dat, GeoIPv6.dat ...\n"
  "\n"
#endif

/* Other Options */
  CYN "OTHER OPTIONS\n\n" RESET
  "  -h --help                       - This help.\n"
  "  -s --storage                    - Display current storage method. e.g., Hash.\n"
  "  -V --version                    - Display version information and exit.\n"
  "  --dcf                           - Display the path of the default config file when `-p` is not used.\n"
  "\n"

  "%s `man goaccess`.\n\n"
  "%s: %s\n"
  "GoAccess Copyright (C) 2009-2020 by Gerardo Orellana"
  "\n\n"
  , INFO_HELP_EXAMPLES, INFO_MORE_INFO, GO_WEBSITE
  );
  exit (EXIT_FAILURE);
}
/* *INDENT-ON* */

/* Push a command line option to the given array if within bounds and if it's
 * not in the array. */
static void
set_array_opt (const char *oarg, const char *arr[], int *size, int max) {
  if (str_inarray (oarg, arr, *size) < 0 && *size < max)
    arr[(*size)++] = oarg;
}

/* Parse command line long options. */
static void
parse_long_opt (const char *name, const char *oarg) {
  if (!strcmp ("no-global-config", name))
    return;

  /* LOG & DATE FORMAT OPTIONS
   * ========================= */
  /* log format */
  if (!strcmp ("log-format", name))
    set_log_format_str (oarg);

  /* time format */
  if (!strcmp ("time-format", name))
    set_time_format_str (oarg);

  /* date format */
  if (!strcmp ("date-format", name))
    set_date_format_str (oarg);

  /* USER INTERFACE OPTIONS
   * ========================= */
  /* colors */
  if (!strcmp ("color", name))
    set_array_opt (oarg, conf.colors, &conf.color_idx, MAX_CUSTOM_COLORS);

  /* color scheme */
  if (!strcmp ("color-scheme", name))
    conf.color_scheme = atoi (oarg);

  /* html custom CSS */
  if (!strcmp ("html-custom-css", name))
    conf.html_custom_css = oarg;

  /* html custom JS */
  if (!strcmp ("html-custom-js", name))
    conf.html_custom_js = oarg;

  /* html JSON object containing default preferences */
  if (!strcmp ("html-prefs", name))
    conf.html_prefs = oarg;

  /* html report title */
  if (!strcmp ("html-report-title", name))
    conf.html_report_title = oarg;

  /* json pretty print */
  if (!strcmp ("json-pretty-print", name))
    conf.json_pretty_print = 1;

  /* max items */
  if (!strcmp ("max-items", name)) {
    char *sEnd;
    int max = strtol (oarg, &sEnd, 10);
    if (oarg == sEnd || *sEnd != '\0' || errno == ERANGE)
      conf.max_items = 1;
    else
      conf.max_items = max;
  }

  /* no color */
  if (!strcmp ("no-color", name))
    conf.no_color = 1;

  /* no strict status */
  if (!strcmp ("no-strict-status", name))
    conf.no_strict_status = 1;

  /* no columns */
  if (!strcmp ("no-column-names", name))
    conf.no_column_names = 1;

  /* no csv summary */
  if (!strcmp ("no-csv-summary", name))
    conf.no_csv_summary = 1;

  /* no parsing spinner */
  if (!strcmp ("no-parsing-spinner", name))
    conf.no_parsing_spinner = 1;

  /* no progress */
  if (!strcmp ("no-progress", name))
    conf.no_progress = 1;

  /* no tab scroll */
  if (!strcmp ("no-tab-scroll", name))
    conf.no_tab_scroll = 1;

  /* no html last updated field */
  if (!strcmp ("no-html-last-updated", name))
    conf.no_html_last_updated = 1;

  /* SERVER OPTIONS
   * ========================= */
  /* address to bind to */
  if (!strcmp ("addr", name))
    conf.addr = oarg;

  /* unix socket to use */
  if (!strcmp ("unix-socket", name))
    conf.unix_socket = oarg;

  /* FIFO in (read) */
  if (!strcmp ("fifo-in", name))
    conf.fifo_in = oarg;

  /* FIFO out (write) */
  if (!strcmp ("fifo-out", name))
    conf.fifo_out = oarg;

  /* run program as a Unix daemon */
  if (!strcmp ("daemonize", name))
    conf.daemonize = 1;

  if (!strcmp ("user-name", name))
    conf.username = oarg;

  /* WebSocket origin */
  if (!strcmp ("origin", name))
    conf.origin = oarg;

  /* PID file to write */
  if (!strcmp ("pid-file", name))
    conf.pidfile = oarg;

  /* port to use */
  if (!strcmp ("port", name)) {
    int port = strtol (oarg, NULL, 10);
    if (port < 0 || port > 65535)
      LOG_DEBUG (("Invalid port."));
    else
      conf.port = oarg;
  }

  /* real time HTML */
  if (!strcmp ("real-time-html", name))
    conf.real_time_html = 1;

  /* persist data to disk */
  if (!strcmp ("persist", name))
    conf.persist = 1;

  /* restore data from disk */
  if (!strcmp ("restore", name))
    conf.restore = 1;

  /* TLS/SSL certificate */
  if (!strcmp ("ssl-cert", name))
    conf.sslcert = oarg;

  /* TLS/SSL private key */
  if (!strcmp ("ssl-key", name))
    conf.sslkey = oarg;

  /* URL to which the WebSocket server responds. */
  if (!strcmp ("ws-url", name))
    conf.ws_url = oarg;

  /* FILE OPTIONS
   * ========================= */
  /* invalid requests */
  if (!strcmp ("invalid-requests", name)) {
    conf.invalid_requests_log = oarg;
    invalid_log_open (conf.invalid_requests_log);
  }

  /* unknowns */
  if (!strcmp ("unknowns-log", name)) {
    conf.unknowns_log = oarg;
    unknowns_log_open (conf.unknowns_log);
  }

  /* output file */
  if (!strcmp ("output", name))
    set_array_opt (oarg, conf.output_formats, &conf.output_format_idx, MAX_OUTFORMATS);

  /* PARSE OPTIONS
   * ========================= */
  /* 444 as 404 */
  if (!strcmp ("444-as-404", name))
    conf.code444_as_404 = 1;

  /* 4xx to unique count */
  if (!strcmp ("4xx-to-unique-count", name))
    conf.client_err_to_unique_count = 1;

  /* anonymize ip */
  if (!strcmp ("anonymize-ip", name))
    conf.anonymize_ip = 1;

  /* all static files */
  if (!strcmp ("all-static-files", name))
    conf.all_static_files = 1;

  /* crawlers only */
  if (!strcmp ("crawlers-only", name))
    conf.crawlers_only = 1;

  /* date specificity */
  if (!strcmp ("date-spec", name) && !strcmp (oarg, "hr"))
    conf.date_spec_hr = 1;

  /* double decode */
  if (!strcmp ("double-decode", name))
    conf.double_decode = 1;

  /* enable panel */
  if (!strcmp ("enable-panel", name))
    set_array_opt (oarg, conf.enable_panels, &conf.enable_panel_idx, TOTAL_MODULES);

  /* hour specificity */
  if (!strcmp ("hour-spec", name) && !strcmp (oarg, "min"))
    conf.hour_spec_min = 1;

  /* ignore crawlers */
  if (!strcmp ("ignore-crawlers", name))
    conf.ignore_crawlers = 1;

  /* ignore panel */
  if (!strcmp ("ignore-panel", name))
    set_array_opt (oarg, conf.ignore_panels, &conf.ignore_panel_idx, TOTAL_MODULES);

  /* ignore referer */
  if (!strcmp ("ignore-referer", name))
    set_array_opt (oarg, conf.ignore_referers, &conf.ignore_referer_idx, MAX_IGNORE_REF);

  /* client IP validation */
  if (!strcmp ("no-ip-validation", name))
    conf.no_ip_validation = 1;

  /* hide referer from report (e.g. within same site) */
  if (!strcmp ("hide-referer", name))
    set_array_opt (oarg, conf.hide_referers, &conf.hide_referer_idx, MAX_IGNORE_REF);

  /* ignore status code */
  if (!strcmp ("ignore-status", name))
    set_array_opt (oarg, conf.ignore_status, &conf.ignore_status_idx, MAX_IGNORE_STATUS);

  /* ignore static requests */
  if (!strcmp ("ignore-statics", name)) {
    if (!strcmp ("req", oarg))
      conf.ignore_statics = IGNORE_LEVEL_REQ;
    else if (!strcmp ("panel", oarg))
      conf.ignore_statics = IGNORE_LEVEL_PANEL;
    else
      LOG_DEBUG (("Invalid statics ignore option."));
  }

  /* number of line tests */
  if (!strcmp ("num-tests", name)) {
    char *sEnd;
    int tests = strtol (oarg, &sEnd, 10);
    if (oarg == sEnd || *sEnd != '\0' || errno == ERANGE)
      return;
    conf.num_tests = tests >= 0 ? tests : 0;
  }

  /* number of days to keep in storage */
  if (!strcmp ("keep-last", name)) {
    char *sEnd;
    int keeplast = strtol (oarg, &sEnd, 10);
    if (oarg == sEnd || *sEnd != '\0' || errno == ERANGE)
      return;
    conf.keep_last = keeplast >= 0 ? keeplast : 0;
  }

  /* refresh html every X seconds */
  if (!strcmp ("html-refresh", name)) {
    char *sEnd;
    uint64_t ref = strtoull (oarg, &sEnd, 10);
    if (oarg == sEnd || *sEnd != '\0' || errno == ERANGE)
      return;
    conf.html_refresh = ref >= 1 && ref <= 60 ? ref : 0;
  }

  /* specifies the path of the database file */
  if (!strcmp ("db-path", name))
    conf.db_path = oarg;

  /* process and exit */
  if (!strcmp ("process-and-exit", name))
    conf.process_and_exit = 1;

  /* real os */
  if (!strcmp ("real-os", name))
    conf.real_os = 1;

  /* sort view */
  if (!strcmp ("sort-panel", name))
    set_array_opt (oarg, conf.sort_panels, &conf.sort_panel_idx, TOTAL_MODULES);

  /* static file */
  if (!strcmp ("static-file", name) && conf.static_file_idx < MAX_EXTENSIONS) {
    if (conf.static_file_max_len < strlen (oarg))
      conf.static_file_max_len = strlen (oarg);
    set_array_opt (oarg, conf.static_files, &conf.static_file_idx, MAX_EXTENSIONS);
  }

  /* GEOIP OPTIONS
   * ========================= */
  /* specifies the path of the GeoIP City database file */
  if (!strcmp ("geoip-database", name))
    conf.geoip_database = oarg;

  /* default config file --dwf */
  if (!strcmp ("dcf", name)) {
    display_default_config_file ();
    exit (EXIT_SUCCESS);
  }
}

/* Determine if the '--no-global-config' command line option needs to be
 * enabled or not. */
void
verify_global_config (int argc, char **argv) {
  int o, idx = 0;

  conf.load_global_config = 1;
  while ((o = getopt_long (argc, argv, short_options, long_opts, &idx)) >= 0) {
    if (-1 == o || EOF == o)
      break;

    switch (o) {
    case 'p':
      conf.iconfigfile = xstrdup (optarg);
      break;
    case 0:
      if (!strcmp ("no-global-config", long_opts[idx].name))
        conf.load_global_config = 0;
      break;
    case '?':
      exit (EXIT_FAILURE);
    }
  }

  /* reset it to 1 */
  optind = 1;
}

/* Attempt to add - to the array of filenames if it hasn't been added it yet. */
void
add_dash_filename (void) {
  int i;
  // pre-scan for '-' and don't add if already exists: github.com/allinurl/goaccess/issues/907
  for (i = 0; i < conf.filenames_idx; ++i) {
    if (conf.filenames[i][0] == '-' && conf.filenames[i][1] == '\0')
      return;
  }

  if (conf.filenames_idx < MAX_FILENAMES && !conf.read_stdin) {
    conf.read_stdin = 1;
    conf.filenames[conf.filenames_idx++] = "-";
  }
}

/* Read the user's supplied command line options. */
void
read_option_args (int argc, char **argv) {
  int o, idx = 0;

#ifdef HAVE_LIBGEOIP
  conf.geo_db = GEOIP_MEMORY_CACHE;
#endif

  while ((o = getopt_long (argc, argv, short_options, long_opts, &idx)) >= 0) {
    if (-1 == o || EOF == o)
      break;
    switch (o) {
    case 'f':
      if (conf.filenames_idx < MAX_FILENAMES)
        conf.filenames[conf.filenames_idx++] = optarg;
      break;
    case 'S':
      if (strchr (optarg, '-')) {
        printf ("[ERROR] log-size must be a positive integer\n");
        exit (EXIT_FAILURE);
      }
      conf.log_size = (uint64_t) atoll (optarg);
      break;
    case 'p':
      /* ignore it */
      break;
#ifdef HAVE_LIBGEOIP
    case 'g':
      conf.geo_db = GEOIP_STANDARD;
      break;
#endif
    case 'e':
      if (conf.ignore_ip_idx < MAX_IGNORE_IPS)
        conf.ignore_ips[conf.ignore_ip_idx++] = optarg;
      else
        LOG_DEBUG (("Max num of (%d) IPs to ignore reached.", MAX_IGNORE_IPS));
      break;
    case 'a':
      conf.list_agents = 1;
      break;
    case 'b':
      conf.browsers_file = optarg;
      break;
    case 'c':
      conf.load_conf_dlg = 1;
      break;
    case 'i':
      conf.hl_header = 1;
      break;
    case 'q':
      conf.ignore_qstr = 1;
      break;
    case 'o':
      if (!valid_output_type (optarg)) {
        printf
          ("[ERROR] Invalid filename extension used, must be any of .csv, .json, or .html\n");
        exit (EXIT_FAILURE);
      }
      if (conf.output_format_idx < MAX_OUTFORMATS)
        conf.output_formats[conf.output_format_idx++] = optarg;
      break;
    case 'l':
      conf.debug_log = optarg;
      dbg_log_open (conf.debug_log);
      break;
    case 'r':
      conf.skip_term_resolver = 1;
      break;
    case 'd':
      conf.enable_html_resolver = 1;
      break;
    case 'm':
      conf.mouse_support = 1;
      break;
    case 'M':
      if (strcmp ("no", optarg) == 0)
        conf.append_method = 0;
      else
        conf.append_method = 1;
      break;
    case 'h':
      cmd_help ();
      break;
    case 'H':
      if (strcmp ("no", optarg) == 0)
        conf.append_protocol = 0;
      else
        conf.append_protocol = 1;
      break;
    case 'V':
      display_version ();
      exit (EXIT_SUCCESS);
      break;
    case 0:
      parse_long_opt (long_opts[idx].name, optarg);
      break;
    case 's':
      display_storage ();
      exit (EXIT_SUCCESS);
    case '?':
      exit (EXIT_FAILURE);
    default:
      exit (EXIT_FAILURE);
    }
  }

  for (idx = optind; idx < argc; ++idx) {
    /* read from standard input */
    if (!conf.read_stdin && strcmp ("-", argv[idx]) == 0)
      add_dash_filename ();
    /* read filenames */
    else {
      if (conf.filenames_idx < MAX_FILENAMES)
        conf.filenames[conf.filenames_idx++] = argv[idx];
    }
  }
}
