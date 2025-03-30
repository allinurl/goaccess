/**
 * options.c -- functions related to parsing program options
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2024 Gerardo Orellana <hello @ goaccess.io>
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
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
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
#include "wsauth.h"

#include "xmalloc.h"

static const char *short_options = "b:e:f:j:l:o:p:H:M:S:"
#ifdef HAVE_LIBGEOIP
  "g"
#endif
  "acdhimqrsV";

/* *INDENT-OFF* */
static const struct option long_opts[] = {
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
  {"jobs"                 , required_argument , 0 , 'j' } ,
  {"log-file"             , required_argument , 0 , 'f' } ,
  {"log-size"             , required_argument , 0 , 'S' } ,
  {"no-query-string"      , no_argument       , 0 , 'q' } ,
  {"no-term-resolver"     , no_argument       , 0 , 'r' } ,
  {"output"               , required_argument , 0 , 'o' } ,
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
  {"anonymize-level"      , required_argument , 0 , 0  }  ,
  {"color"                , required_argument , 0 , 0  }  ,
  {"color-scheme"         , required_argument , 0 , 0  }  ,
  {"crawlers-only"        , no_argument       , 0 , 0  }  ,
  {"chunk-size"           , required_argument , 0 , 0  }  ,
  {"daemonize"            , no_argument       , 0 , 0  }  ,
  {"datetime-format"      , required_argument , 0 , 0  }  ,
  {"date-format"          , required_argument , 0 , 0  }  ,
  {"date-spec"            , required_argument , 0 , 0  }  ,
  {"db-path"              , required_argument , 0 , 0  }  ,
  {"fname-as-vhost"       , required_argument , 0 , 0  }  ,
  {"dcf"                  , no_argument       , 0 , 0  }  ,
  {"double-decode"        , no_argument       , 0 , 0  }  ,
  {"external-assets"      , no_argument       , 0 , 0  }  ,
  {"enable-panel"         , required_argument , 0 , 0  }  ,
  {"fifo-in"              , required_argument , 0 , 0  }  ,
  {"fifo-out"             , required_argument , 0 , 0  }  ,
  {"hide-referrer"        , required_argument , 0 , 0  }  ,
  {"hour-spec"            , required_argument , 0 , 0  }  ,
  {"html-custom-css"      , required_argument , 0 , 0  }  ,
  {"html-custom-js"       , required_argument , 0 , 0  }  ,
  {"html-prefs"           , required_argument , 0 , 0  }  ,
  {"html-report-title"    , required_argument , 0 , 0  }  ,
  {"ignore-crawlers"      , no_argument       , 0 , 0  }  ,
  {"ignore-panel"         , required_argument , 0 , 0  }  ,
  {"ignore-referrer"      , required_argument , 0 , 0  }  ,
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
  {"output-format"        , required_argument , 0 , 0  }  ,
  {"persist"              , no_argument       , 0 , 0  }  ,
  {"pid-file"             , required_argument , 0 , 0  }  ,
  {"port"                 , required_argument , 0 , 0  }  ,
  {"process-and-exit"     , no_argument       , 0 , 0  }  ,
  {"real-os"              , no_argument       , 0 , 0  }  ,
  {"real-time-html"       , no_argument       , 0 , 0  }  ,
  {"restore"              , no_argument       , 0 , 0  }  ,
  {"sort-panel"           , required_argument , 0 , 0  }  ,
  {"static-file"          , required_argument , 0 , 0  }  ,
  {"tz"                   , required_argument , 0 , 0  }  ,
  {"unknowns-as-crawlers" , no_argument       , 0 , 0  }  ,
  {"user-name"            , required_argument , 0 , 0  }  ,
#ifdef HAVE_LIBSSL
  {"ssl-cert"             , required_argument , 0 ,  0  } ,
  {"ssl-key"              , required_argument , 0 ,  0  } ,
#endif
  {"time-format"          , required_argument , 0 ,  0  } ,
  {"ws-url"               , required_argument , 0 ,  0  } ,
#ifdef HAVE_LIBSSL
  {"ws-auth"              , required_argument , 0 ,  0  } ,
  {"ws-auth-expire"       , required_argument , 0 ,  0  } ,
  {"ws-auth-url"          , required_argument , 0 ,  0  } ,
  {"ws-auth-refresh-url"  , required_argument , 0 ,  0  } ,
#endif
  {"ping-interval"        , required_argument , 0 ,  0  } ,
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
  CYN "LOG & DATE FORMAT OPTIONS" RESET
  "\n\n"
  "  --log-format=<logformat>        - Specify log format. Inner quotes need\n"
  "                                    escaping, or use single quotes.\n"
  "  --date-format=<dateformat>      - Specify log date format. e.g., %%d/%%b/%%Y\n"
  "  --time-format=<timeformat>      - Specify log time format. e.g., %%H:%%M:%%S\n"
  "  --datetime-format=<dt-format>   - Specify log date and time format. e.g.,\n"
  "                                    %%d/%%b/%%Y %%H:%%M:%%S %%z\n"
  "\n"
  /* User Interface Options */
  CYN "USER INTERFACE OPTIONS" RESET
  "\n\n"
  "  -c --config-dialog              - Prompt log/date/time configuration window.\n"
  "  -i --hl-header                  - Color highlight active panel.\n"
  "  -m --with-mouse                 - Enable mouse support on main dashboard.\n"
  "  --color=<fg:bg[attrs, PANEL]>   - Specify custom colors. See manpage for more\n"
  "                                    details.\n"
  "  --color-scheme=<1|2|3>          - Schemes: 1 => Grey, 2 => Green, 3 =>\n"
  "                                    Monokai.\n"
  "  --html-custom-css=<path.css>    - Specify a custom CSS file in the HTML\n"
  "                                    report.\n"
  "  --html-custom-js=<path.js>      - Specify a custom JS file in the HTML\n"
  "                                    report.\n"
  "  --html-prefs=<json_obj>         - Set default HTML report preferences.\n"
  "  --html-report-title=<title>     - Set HTML report page title and header.\n"
  "  --html-refresh=<secs>           - Refresh HTML report every X seconds (>=1 or\n"
  "                                    <=60).\n"
  "  --json-pretty-print             - Format JSON output w/ tabs & newlines.\n"
  "  --max-items                     - Maximum number of items to show per panel.\n"
  "                                    See man page for limits.\n"
  "  --no-color                      - Disable colored output.\n"
  "  --no-column-names               - Don't write column names in term output.\n"
  "  --no-csv-summary                - Disable summary metrics on the CSV output.\n"
  "  --no-html-last-updated          - Hide HTML last updated field.\n"
  "  --no-parsing-spinner            - Disable progress metrics and parsing\n"
  "                                    spinner.\n"
  "  --no-progress                   - Disable progress metrics.\n"
  "  --no-tab-scroll                 - Disable scrolling through panels on TAB.\n"
  "  --tz=<timezone>                 - Use the specified timezone (canonical name,\n"
  "                                    e.g., America/Chicago).\n"
  "\n"
  ""
  /* Server Options */
  CYN "SERVER OPTIONS" RESET
  "\n\n"
  "  --addr=<addr>                   - Specify IP address to bind server to.\n"
  "  --unix-socket=<addr>            - Specify UNIX-domain socket address to bind\n"
  "                                    server to.\n"
  "  --daemonize                     - Run as daemon (if --real-time-html\n"
  "                                    enabled).\n"
  "  --fifo-in=<path>                - Path to read named pipe (FIFO).\n"
  "  --fifo-out=<path>               - Path to write named pipe (FIFO).\n"
  "  --origin=<addr>                 - Ensure clients send this origin header upon\n"
  "                                    the WebSocket handshake.\n"
  "  --pid-file=<path>               - Write PID to a file when --daemonize is\n"
  "                                    used.\n"
  "  --port=<port>                   - Specify the port to use.\n"
  "  --real-time-html                - Enable real-time HTML output.\n"
  "  --ssl-cert=<cert.crt>           - Path to TLS/SSL certificate.\n"
  "  --ssl-key=<priv.key>            - Path to TLS/SSL private key.\n"
  "  --user-name=<username>          - Run as the specified user.\n"
  "  --ws-url=<url>                  - URL to which the WebSocket server responds.\n"
#ifdef HAVE_LIBSSL
  "  --ws-auth=<jwt[:secret]>        - Enables WebSocket authentication using a\n"
  "                                    JSON Web Token (JWT). Optionally, a secret key\n"
  "                                    can be provided for verification.\n"
  "  --ws-auth-expire=<secs>         - Time after which the JWT expires.\n"
  "  --ws-auth-url=<url>             - URL to fetch the initial JWT .\n"
  "                                    e.g., https://page.com/api/get-auth-token\n"
  "  --ws-auth-refresh-url=<url>     - URL to fetch a new JWT when initial expires.\n"
  "                                    e.g., https://page.com/api/refresh-token\n"
#endif
  "  --ping-interval=<secs>          - Enable WebSocket ping with specified\n"
  "                                    interval in seconds.\n"
  "\n"
  ""
  /* File Options */
  CYN "FILE OPTIONS" RESET
  "\n\n"
  "  -                               - The log file to parse is read from stdin.\n"
  "  -f --log-file=<filename>        - Path to input log file.\n"
  "  -l --debug-file=<filename>      - Send all debug messages to the specified\n"
  "                                    file.\n"
  "  -p --config-file=<filename>     - Custom configuration file.\n"
  "  -S --log-size=<number>          - Specify the log size, useful when piping in\n"
  "                                    logs.\n"
  "  --external-assets               - Output HTML assets to external JS/CSS files.\n"
  "  --invalid-requests=<filename>   - Log invalid requests to the specified file.\n"
  "  --no-global-config              - Don't load global configuration file.\n"
  "  --unknowns-log=<filename>       - Log unknown browsers and OSs to the\n"
  "                                    specified file.\n"
  "\n"
  ""
  /* Parse Options */
  CYN "PARSE OPTIONS" RESET
  "\n\n"
  "  -a --agent-list                 - Enable a list of user-agents by host.\n"
  "  -b --browsers-file=<path>       - Use additional custom list of browsers.\n"
  "  -d --with-output-resolver       - Enable IP resolver on HTML|JSON output.\n"
  "  -e --exclude-ip=<IP>            - Exclude one or multiple IPv4/6. Allows IP\n"
  "                                    ranges. e.g., 192.168.0.1-192.168.0.10\n"
  "  -j --jobs=<1-6>                 - Thread count for parsing log. Defaults to 1.\n"
  "                                    The use of 2-4 threads is recommended.\n"
  "  -H --http-protocol=<yes|no>     - Set/unset HTTP request protocol if found.\n"
  "  -M --http-method=<yes|no>       - Set/unset HTTP request method if found.\n"
  "  -o --output=<format|filename>   - Output to stdout or the specified file.\n"
  "                                    e.g., -o csv, -o out.json, --output=report.html\n"
  "  -q --no-query-string            - Strip request's query string. This can\n"
  "                                    decrease memory consumption.\n"
  "  -r --no-term-resolver           - Disable IP resolver on terminal output.\n"
  "  --444-as-404                    - Treat non-standard status code 444 as 404.\n"
  "  --4xx-to-unique-count           - Add 4xx client errors to the unique\n"
  "                                    visitors count.\n"
  "  --all-static-files              - Include static files with a query string.\n"
  "  --anonymize-ip                  - Anonymize IP addresses before outputting to\n"
  "                                    report.\n"
  "  --anonymize-level=<1|2|3>       - Anonymization levels: 1 => default, 2 =>\n"
  "                                    strong, 3 => pedantic.\n"
  "  --chunk-size=<256-32768>        - Number of lines processed in each data chunk\n"
  "                                    for parallel execution. Default is 1024.\n"
  "  --crawlers-only                 - Parse and display only crawlers.\n"
  "  --date-spec=<date|hr|min>       - Date specificity. Possible values: `date`\n"
  "                                    (default), `hr` or `min`.\n"
  "  --db-path=<path>                - Persist data to disk on exit to the given\n"
  "                                    path or /tmp as default.\n"
  "  --double-decode                 - Decode double-encoded values.\n"
  "  --enable-panel=<PANEL>          - Enable parsing/displaying the given panel.\n"
  "  --fname-as-vhost=<regex>        - Use log filename(s) as virtual host(s).\n"
  "                                    POSIX regex is passed to extract virtual\n"
  "                                    host.\n"
  "  --hide-referrer=<NEEDLE>        - Hide a referrer but still count it. Wild\n"
  "                                    cards are allowed. i.e., *.bing.com\n"
  "  --hour-spec=<hr|min>            - Hour specificity. Possible values: `hr`\n"
  "                                    (default) or `min` (tenth of a min).\n"
  "  --ignore-crawlers               - Ignore crawlers.\n"
  "  --ignore-panel=<PANEL>          - Ignore parsing/displaying the given panel.\n"
  "  --ignore-referrer=<NEEDLE>      - Ignore a referrer from being counted. Wild\n"
  "                                    cards are allowed. i.e., *.bing.com\n"
  "  --ignore-statics=<req|panel>    - Ignore static requests.\n"
  "                                    req => Ignore from valid requests.\n"
  "                                    panel => Ignore from valid requests and\n"
  "                                    panels.\n"
  "  --ignore-status=<CODE>          - Ignore parsing the given status code.\n"
  "  --keep-last=<NDAYS>             - Keep the last NDAYS in storage.\n"
  "  --no-ip-validation              - Disable client IPv4/6  validation.\n"
  "  --no-strict-status              - Disable HTTP status code validation.\n"
  "  --num-tests=<number>            - Number of lines to test. >= 0 (10 default)\n"
  "  --persist                       - Persist data to disk on exit to the given\n"
  "                                    --db-path or to /tmp.\n"
  "  --process-and-exit              - Parse log and exit without outputting data.\n"
  "  --real-os                       - Display real OS names. e.g, Windows XP,\n"
  "                                    Snow Leopard.\n"
  "  --restore                       - Restore data from disk from the given\n"
  "                                    --db-path or from /tmp.\n"
  "  --sort-panel=PANEL,METRIC,ORDER - Sort panel on initial load. e.g.,\n"
  "                                    --sort-panel=VISITORS,BY_HITS,ASC.\n"
  "                                    See manpage for a list of panels/fields.\n"
  "  --static-file=<extension>       - Add static file extension. e.g.: .mp3.\n"
  "                                    Extensions are case sensitive.\n"
  "  --unknowns-as-crawlers          - Classify unknown OS and browsers as crawlers.\n"
  "\n"

/* GeoIP Options */
#ifdef HAVE_GEOLOCATION
  CYN "GEOIP OPTIONS" RESET
  "\n\n"
#ifdef HAVE_LIBGEOIP
  "  -g --std-geoip                  - Standard GeoIP database for less memory\n"
  "                                    consumption (legacy DB).\n"
#endif
  "  --geoip-database=<path>         - Specify path to GeoIP database file.\n"
  "                                    i.e., GeoLiteCity.dat, GeoIPv6.dat ...\n"
  "\n"
#endif

/* Other Options */
  CYN "OTHER OPTIONS" RESET
  "\n\n"
  "  -h --help                       - This help.\n"
  "  -s --storage                    - Display current storage method. e.g., Hash.\n"
  "  -V --version                    - Display version information and exit.\n"
  "  --dcf                           - Display the path of the default config file\n"
  "                                    when `-p` is not used.\n"
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

#ifdef HAVE_LIBSSL
/*
 * parse_ws_auth_expire_option:
 *   Parses a time duration string and converts it to seconds.
 *
 * Supported formats:
 *   "3600"       -> 3600 seconds
 *   "24h"        -> 24 hours = 24 * 3600 seconds
 *   "10m"        -> 10 minutes = 10 * 60 seconds
 *   "10d"        -> 10 days = 10 * 86400 seconds
 *
 * Returns 0 on success, nonzero on failure.
 */
static int
parse_ws_auth_expire_option (const char *input) {
  char *endptr = NULL;
  double value = 0, multiplier = 0, total_seconds = 0;

  if (!input || *input == '\0')
    return -1;  // Empty or NULL string

  value = strtod (input, &endptr);
  if (endptr == input) {
    // No conversion could be performed
    return -1;
  }

  multiplier = 1.0;
  /* Skip any whitespace after the number */
  while (isspace ((unsigned char) *endptr))
    endptr++;

  if (*endptr != '\0') {
    /* Expect a single unit character; any extra characters are not allowed */
    char unit = *endptr;
    endptr++;
    while (isspace ((unsigned char) *endptr))
      endptr++;

    if (*endptr != '\0') {
      // Extra unexpected characters
      return -1;
    }

    switch (unit) {
    case 's':
    case 'S':
      multiplier = 1.0;
      break;
    case 'm':
    case 'M':
      multiplier = 60.0;
      break;
    case 'h':
    case 'H':
      multiplier = 3600.0;
      break;
    case 'd':
    case 'D':
      multiplier = 86400.0;
      break;
    default:
      // Unknown unit
      return -1;
    }
  }

  total_seconds = value * multiplier;
  if (total_seconds < 0)
    return -1;  // Negative durations are not allowed

  /* Store the result in your global configuration */
  conf.ws_auth_expire = (long) total_seconds;
  return 0;
}

 /**
 * Reads a JWT secret from a file path
 * @param path File path containing the secret
 * @return 0 on success, 1 on failure
 */
static int
parse_jwt_from_file (const char *path) {
  conf.ws_auth_secret = read_secret_from_file (path);
  if (conf.ws_auth_secret == NULL) {
    fprintf (stderr, "Failed to read secret from file: %s\n", path);
    return 1;
  }
  conf.ws_auth_verify_only = 1;
  return 0;
}

/**
 * Retrieves a JWT secret from an environment variable
 * @param env_var Environment variable name (without '$' prefix)
 * @return 0 on success, 1 on failure
 */
static int
parse_jwt_from_env (const char *env_var) {
  const char *env_secret = getenv (env_var);
  if (env_secret != NULL) {
    conf.ws_auth_secret = strdup (env_secret);
    conf.ws_auth_verify_only = 1;
    return 0;
  }
  fprintf (stderr, "Environment variable %s not found\n", env_var);
  return 1;
}

/**
 * Handles the 'verify:' prefix for JWT authentication
 * @param secret_spec Secret specification string
 * @return 0 on success, 1 on failure
 */
static int
parse_jwt_verify_option (const char *secret_spec) {
  /* First try to read from file */
  if (access (secret_spec, F_OK) == 0) {
    return parse_jwt_from_file (secret_spec);
  }

  /* Then try environment variable if path contains '$' */
  if (*secret_spec == '$') {
    return parse_jwt_from_env (secret_spec + 1);
  }

  /* Finally use as direct secret string */
  conf.ws_auth_verify_only = 1;
  conf.ws_auth_secret = strdup (secret_spec);
  return 0;
}

/**
 * Handles the legacy JWT format without 'verify:' prefix
 * @param remainder String after 'jwt:' prefix
 * @return 0 on success
 */
static int
parse_legacy_jwt_option (const char *remainder) {
  if (access (remainder, F_OK) == 0) {
    conf.ws_auth_secret = read_secret_from_file (remainder);
  } else {
    conf.ws_auth_secret = strdup (remainder);
  }
  return 0;
}

/**
 * Handles the plain 'jwt' option with no additional parameters
 * @return 0 on success
 */
static int
parse_plain_jwt_option (void) {
  /* Try to get secret from environment variable */
  const char *env_secret = getenv ("GOACCESS_WSAUTH_SECRET");
  if (env_secret != NULL) {
    conf.ws_auth_secret = strdup (env_secret);
  } else {
    /* Fallback to generating a secret */
    conf.ws_auth_secret = generate_ws_auth_secret ();
  }
  return 0;
}

/**
 * Options:
 * # Read secret from file
 * --ws-auth="jwt:verify:/etc/ws_secret.txt"
 *
 * # Get secret from environment variable
 * --ws-auth="jwt:verify:$GOACCESS_WSAUTH_SECRET"
 *
 * # Use direct secret string
 * --ws-auth="jwt:verify:my_super_secret_key"
 *
 * # Legacy formats still work
 * --ws-auth="jwt:/path/to/secret"
 * --ws-auth="jwt:super_secret_key"
 * --ws-auth="jwt" // generate one
 *
 * Parses websocket authentication options
 * @param optarg Option argument string
 * @return 0 on success, 1 on failure/invalid option
 */
static int
parse_ws_auth_option (const char *optarg) {
  /* Handle cases where it starts with "jwt:" */
  const char *remainder = NULL;
  /* First check for plain "jwt" option (no colon) */
  if (strcmp (optarg, "jwt") == 0)
    return parse_plain_jwt_option ();

  /* If it doesn't start with "jwt:", it's invalid */
  if (strncmp (optarg, "jwt:", 4) != 0)
    return 1;

  /* Handle cases where it starts with "jwt:" */
  remainder = optarg + 4;

  /* If the remainder is exactly "verify", that's reserved – error out */
  if (strcmp (remainder, "verify") == 0)
    return 1;

  /* Check for "verify:" prefix */
  if (strncmp (remainder, "verify:", 7) == 0) {
    // Ensure that there is a secret specification after "verify:"
    if (remainder[7] == '\0') {
      return 1; /* "verify:" with nothing following is invalid */
    }
    return parse_jwt_verify_option (remainder + 7);
  }

  /* Legacy jwt:secret format */
  return parse_legacy_jwt_option (remainder);
}

static int
validate_url_basic (const char *url) {
  /* Check for NULL */
  if (!url)
    return 0;

  /* Check for http:// or https:// protocol */
  if (strncmp (url, "http://", 7) == 0) {
    /* Make sure there's something after http:// */
    return url[7] != '\0';
  } else if (strncmp (url, "https://", 8) == 0) {
    /* Make sure there's something after https:// */
    return url[8] != '\0';
  }

  return 0;     /* No valid protocol */
}
#endif

/* Parse command line long options. */
static void
parse_long_opt (const char *name, const char *oarg) {
  if (!strcmp ("no-global-config", name))
    return;

  /* LOG & DATE FORMAT OPTIONS
   * ========================= */

  /* datetime format */
  if (!strcmp ("datetime-format", name) && !conf.date_format && !conf.time_format) {
    set_date_format_str (oarg);
    set_time_format_str (oarg);
  }

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
  if (!strcmp ("html-custom-css", name)) {
    if (strpbrk (oarg, "&\"'<>"))
      FATAL ("Invalid filename. The following chars are not allowed in filename: [\"'&<>]\n");
    conf.html_custom_css = oarg;
  }

  /* html custom JS */
  if (!strcmp ("html-custom-js", name)) {
    if (strpbrk (oarg, "&\"'<>"))
      FATAL ("Invalid filename. The following chars are not allowed in filename: [\"'&<>]\n");
    conf.html_custom_js = oarg;
  }

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

  /* timezone */
  if (!strcmp ("tz", name))
    conf.tz_name = oarg;

  /* URL to which the WebSocket server responds. */
  if (!strcmp ("ws-url", name))
    conf.ws_url = oarg;

#ifdef HAVE_LIBSSL
  /* WebSocket auth */
  if (!strcmp ("ws-auth", name) && parse_ws_auth_option (oarg) != 0)
    FATAL ("Invalid --ws-auth option.");

  /* WebSocket auth JWT expires */
  if (!strcmp ("ws-auth-expire", name) && parse_ws_auth_expire_option (oarg) != 0)
    FATAL ("Invalid --ws-auth-expire option.");

  /* WebSocket auth JWT URL */
  if (!strcmp ("ws-auth-url", name)) {
    if (validate_url_basic (oarg) == 0)
      FATAL ("Invalid --ws-auth-url option (check URL validity).");
    conf.ws_auth_url = oarg;
  }

  /* WebSocket auth JWT URL refresh */
  if (!strcmp ("ws-auth-refresh-url", name)) {
    if (validate_url_basic (oarg) == 0)
      FATAL ("Invalid --ws-auth-refresh-url option (check URL validity).");
    conf.ws_auth_refresh_url = oarg;
  }
#endif

  /* WebSocket ping interval in seconds */
  if (!strcmp ("ping-interval", name))
    conf.ping_interval = oarg;

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
  if (!strcmp ("output-format", name))
    FATAL ("The option --output-format is deprecated, please use --output instead.");

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

  /* anonymization level */
  if (!strcmp ("anonymize-level", name))
    conf.anonymize_level = atoi (oarg);

  /* all static files */
  if (!strcmp ("all-static-files", name))
    conf.all_static_files = 1;

  /* chunk size */
  if (!strcmp ("chunk-size", name)) {
    /* Recommended chunk size is 256 - 32768, hard limit is 32 - 1048576. */
    conf.chunk_size = atoi (oarg);
    if (conf.chunk_size < 32)
      FATAL ("The hard lower limit of --chunk-size is 32.");
    if (conf.chunk_size > 1048576)
      FATAL ("The hard limit of --chunk-size is 1048576.");
  }

  /* crawlers only */
  if (!strcmp ("crawlers-only", name))
    conf.crawlers_only = 1;

  /* date specificity */
  if (!strcmp ("date-spec", name) && !strcmp (oarg, "hr"))
    conf.date_spec_hr = 1;
  /* date specificity */
  if (!strcmp ("date-spec", name) && !strcmp (oarg, "min"))
    conf.date_spec_hr = 2;

  /* double decode */
  if (!strcmp ("double-decode", name))
    conf.double_decode = 1;

  /* enable panel */
  if (!strcmp ("enable-panel", name))
    set_array_opt (oarg, conf.enable_panels, &conf.enable_panel_idx, TOTAL_MODULES);

  /* external assets */
  if (!strcmp ("external-assets", name))
    conf.external_assets = 1;

  /* hour specificity */
  if (!strcmp ("hour-spec", name) && !strcmp (oarg, "min"))
    conf.hour_spec_min = 1;

  /* ignore crawlers */
  if (!strcmp ("ignore-crawlers", name))
    conf.ignore_crawlers = 1;

  /* ignore panel */
  if (!strcmp ("ignore-panel", name))
    set_array_opt (oarg, conf.ignore_panels, &conf.ignore_panel_idx, TOTAL_MODULES);

  /* ignore referrer */
  if (!strcmp ("ignore-referrer", name))
    set_array_opt (oarg, conf.ignore_referers, &conf.ignore_referer_idx, MAX_IGNORE_REF);

  /* client IP validation */
  if (!strcmp ("no-ip-validation", name))
    conf.no_ip_validation = 1;

  /* hide referrer from report (e.g. within same site) */
  if (!strcmp ("hide-referrer", name))
    set_array_opt (oarg, conf.hide_referers, &conf.hide_referer_idx, MAX_IGNORE_REF);

  /* ignore status code */
  if (!strcmp ("ignore-status", name))
    if (conf.ignore_status_idx < MAX_IGNORE_STATUS)
      conf.ignore_status[conf.ignore_status_idx++] = atoi (oarg);

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

  /* specifies the regex to extract the virtual host */
  if (!strcmp ("fname-as-vhost", name) && oarg && *oarg != '\0')
    conf.fname_as_vhost = oarg;

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

  /* classify unknowns as crawlers */
  if (!strcmp ("unknowns-as-crawlers", name))
    conf.unknowns_as_crawlers = 1;

  /* GEOIP OPTIONS
   * ========================= */
  /* specifies the path of the GeoIP City database file */
  if (!strcmp ("geoip-database", name) && conf.geoip_db_idx < MAX_GEOIP_DBS)
    set_array_opt (oarg, conf.geoip_databases, &conf.geoip_db_idx, MAX_GEOIP_DBS);

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
    case 'j':
      /* Recommended 4 threads, soft limit is 6, hard limit is 12. */
      conf.jobs = atoi (optarg);
      if (conf.jobs > 12)
        FATAL ("The hard limit of --jobs is 12.");
      break;
    case 'q':
      conf.ignore_qstr = 1;
      break;
    case 'o':
      if (!valid_output_type (optarg))
        FATAL ("Invalid filename extension. It must be any of .csv, .json, or .html\n");
      if (!is_writable_path (optarg))
        FATAL ("Invalid or unwritable path.");
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

  /* Final validation:
   * When using JWT verify mode, the ws-auth-url must be provided.
   * And if a ws-auth-refresh-url is set, then both ws_auth_verify_only and ws_auth_url must be set.
   */
  if (conf.ws_auth_verify_only && !conf.ws_auth_url) {
    FATAL ("--ws-auth with verify requires --ws-auth-url to be set.");
  }

  if (conf.ws_auth_refresh_url) {
    if (!conf.ws_auth_verify_only || !conf.ws_auth_url) {
      FATAL
        ("--ws-auth-refresh-url requires both --ws-auth with verify and --ws-auth-url to be set.");
    }
  }
}
