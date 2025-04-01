/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2025 Gerardo Orellana <hello @ goaccess.io>
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

#ifndef SETTINGS_H_INCLUDED
#define SETTINGS_H_INCLUDED

#include <stdint.h>
#include "commons.h"

#define MAX_LINE_CONF        4096
#define MAX_EXTENSIONS        128
#define MAX_GEOIP_DBS           3
#define MAX_IGNORE_IPS 1024 + 128
#define MAX_IGNORE_REF         64
#define MAX_CUSTOM_COLORS      64
#define MAX_IGNORE_STATUS      64
#define MAX_OUTFORMATS          3
#define MAX_FILENAMES        3072
#define MIN_DATENUM_FMT_LEN     7
#define NO_CONFIG_FILE "No config file used"

typedef enum LOGTYPE {
  COMBINED,
  VCOMBINED,
  COMMON,
  VCOMMON,
  W3C,
  CLOUDFRONT,
  CLOUDSTORAGE,
  AWSELB,
  SQUID,
  AWSS3,
  CADDY,
  AWSALB,
  TRAEFIKCLF,
} GLogType;

/* predefined log times */
typedef struct GPreConfTime_ {
  const char *fmt24;
  const char *usec;
  const char *sec;
} GPreConfTime;

/* predefined log dates */
typedef struct GPreConfDate_ {
  const char *apache;
  const char *w3c;
  const char *usec;
  const char *sec;
} GPreConfDate;

/* predefined log formats */
typedef struct GPreConfLog_ {
  const char *combined;
  const char *vcombined;
  const char *common;
  const char *vcommon;
  const char *w3c;
  const char *cloudfront;
  const char *cloudstorage;
  const char *awselb;
  const char *squid;
  const char *awss3;
  const char *caddy;
  const char *awsalb;
  const char *traefikclf;
} GPreConfLog;

/* *INDENT-OFF* */
/* All configuration properties */
typedef struct GConf_
{
  /* Array options */
  const char *colors[MAX_CUSTOM_COLORS];        /* colors */
  const char *enable_panels[TOTAL_MODULES];     /* array of panels to enable */
  const char *filenames[MAX_FILENAMES];         /* log files */
  const char *hide_referers[MAX_IGNORE_REF];    /* hide referrers from report */
  const char *ignore_ips[MAX_IGNORE_IPS];       /* array of ips to ignore */
  const char *ignore_panels[TOTAL_MODULES];     /* array of panels to ignore */
  const char *ignore_referers[MAX_IGNORE_REF];  /* referrers to ignore */
  int ignore_status[MAX_IGNORE_STATUS]; /* status to ignore */
  const char *output_formats[MAX_OUTFORMATS];   /* output format, e.g. , HTML */
  const char *sort_panels[TOTAL_MODULES];       /* sorting options for each panel */
  const char *static_files[MAX_EXTENSIONS];     /* static extensions */
  const char *geoip_databases[MAX_GEOIP_DBS];    /* geoip db paths */

  /* Log/date/time formats */
  const char *tz_name;                    /* Canonical TZ name, e.g., America/Chicago */
  char *date_time_format;           /* date & time format */
  char *date_format;                /* date format */
  char *date_num_format;            /* numeric date format %Y%m%d */
  char *time_format;                /* time format as given by the user */
  char *spec_date_time_format;      /* date format w/ specificity */
  char *spec_date_time_num_format;  /* numeric date format w/ specificity */
  char *log_format;                 /* log format */
  char *iconfigfile;                /* config file path */
  char ***user_browsers_hash;       /* custom list of browsers */

  const char *debug_log;            /* debug log path */
  const char *html_custom_css;      /* custom CSS */
  const char *html_custom_js;       /* custom JS */
  const char *html_prefs;           /* default HTML JSON preferences */
  const char *html_report_title;    /* report title */
  const char *invalid_requests_log; /* invalid lines log path */
  const char *unknowns_log;         /* unknown browsers/OSs log path */
  const char *pidfile;              /* daemonize pid file path */
  const char *browsers_file;        /* browser's file path */
  const char *db_path;              /* db path to files */
  const char *fname_as_vhost;       /* filenames as vhost/server blocks */

  /* HTML real-time */
  const char *addr;                 /* IP address to bind to */
  const char *fifo_in;              /* path FIFO in (reader) */
  const char *fifo_out;             /* path FIFO out (writer) */
  const char *origin;               /* WebSocket origin */
  const char *port;                 /* port to use */
  const char *sslcert;              /* TLS/SSL path to certificate */
  const char *sslkey;               /* TLS/SSL path to private key */
  const char *ws_url;               /* WebSocket URL */
  const char *ping_interval;        /* WebSocket ping interval in seconds */
  const char *unix_socket;          /* unix socket to bind to */
  const char *ws_auth_url;          /* URL to fetch the initial JWT */
  const char *ws_auth_refresh_url;  /* URL to fetch the refreh JWT */
  char *ws_auth_secret;             /* WebSocket AUTH */
  long ws_auth_expire;              /* WebSocket AUTH JWT expire in seconds */
  int ws_auth_verify_only;          /* WebSocket AUTH verify only */

  /* User flags */
  int all_static_files;             /* parse all static files */
  int anonymize_ip;                 /* anonymize ip addresses */
  int anonymize_level;              /* anonymization level */
  int append_method;                /* append method to the req key */
  int append_protocol;              /* append protocol to the req key */
  int client_err_to_unique_count;   /* count 400s as visitors */
  int code444_as_404;               /* 444 as 404s? */
  int color_scheme;                 /* color scheme */
  int chunk_size;                   /* chunk size for each thread */
  int crawlers_only;                /* crawlers only */
  int daemonize;                    /* run program as a Unix daemon */
  const char *username;             /* user to run program as */
  int double_decode;                /* need to double decode */
  int external_assets;              /* write JS/CSS assets to external files */
  int enable_html_resolver;         /* html/json/csv resolver */
  int geo_db;                       /* legacy geoip db */
  int hl_header;                    /* highlight header on term */
  int ignore_crawlers;              /* ignore crawlers */
  int unknowns_as_crawlers;         /* unknown OS and browsers are classified as crawlers */
  int ignore_qstr;                  /* ignore query string */
  int ignore_statics;               /* ignore static files */
  int jobs;                         /* multi-thread jobs count */
  int json_pretty_print;            /* pretty print JSON data */
  int list_agents;                  /* show list of agents per host */
  int load_conf_dlg;                /* load curses config dialog */
  int load_global_config;           /* use global config file */
  int max_items;                    /* max number of items to output */
  int mouse_support;                /* add curses mouse support */
  int no_color;                     /* no terminal colors */
  int no_strict_status;             /* don't enforce 100-599 status codes */
  int no_column_names;              /* don't show col names on terminal */
  int no_csv_summary;               /* don't show overall metrics */
  int no_html_last_updated;         /* don't show HTML last updated field */
  int no_ip_validation;             /* don't validate client IP addresses */
  int no_parsing_spinner;           /* disable parsing spinner */
  int no_progress;                  /* disable progress metrics */
  int no_tab_scroll;                /* don't scroll dashboard on tab */
  int output_stdout;                /* outputting to stdout */
  int persist;                      /* ensure to persist data on exit */
  int process_and_exit;             /* parse and exit without outputting */
  int real_os;                      /* show real OSs */
  int real_time_html;               /* enable real-time HTML output */
  int restore;                      /* reload data from db-path */
  int skip_term_resolver;           /* no terminal resolver */
  int is_json_log_format;           /* is a json log format */
  uint32_t keep_last;               /* number of days to keep in storage */
  uint32_t num_tests;               /* number of lines to test */
  uint64_t html_refresh;            /* refresh html report every X of seconds */
  uint64_t log_size;                /* log size override */

  /* Internal flags */
  int bandwidth;                    /* is there bandwidth within the req line */
  int date_spec_hr;                 /* date specificity - hour */
  int has_geocity;
  int has_geocountry;
  int has_geoasn;
  int hour_spec_min;                /* hour specificity - min */
  int read_stdin;                   /* read from stdin */
  int serve_usecs;                  /* is there time served within req line */
  int stop_processing;              /* stop all processing */
  int tailing_mode;                 /* in tailing-mode? */

  /* Array indices */
  int color_idx;                    /* colors index */
  int enable_panel_idx;             /* enable panels index */
  int filenames_idx;                /* filenames index */
  int hide_referer_idx;             /* hide referrers index */
  int ignore_ip_idx;                /* ignored ips index */
  int ignore_panel_idx;             /* ignored panels index */
  int ignore_referer_idx;           /* ignored referrers index */
  int ignore_status_idx;            /* ignore status index */
  int output_format_idx;            /* output format index */
  int sort_panel_idx;               /* sort panel index */
  int static_file_idx;              /* static extensions index */
  int geoip_db_idx;                 /* geoip db index */
  int browsers_hash_idx;            /* browsers hash index */

  size_t static_file_max_len;

} GConf;
/* *INDENT-ON* */

char *get_selected_date_str (size_t idx);
char *get_selected_format_str (size_t idx);
char *get_selected_time_str (size_t idx);
const char *verify_formats (void);
int is_json_log_format (const char *fmt);
int parse_json_string (void *userdata, const char *str, int (*cb) (void *, char *, char *));
size_t get_selected_format_idx (void);
void set_date_format_str (const char *optarg);
void set_log_format_str (const char *optarg);
void set_spec_date_format (void);
void set_time_format_str (const char *optarg);

extern GConf conf;

char *get_config_file_path (void);
int parse_conf_file (int *argc, char ***argv);
void free_cmd_args (void);
void free_formats (void);
void set_default_static_files (void);

#endif
