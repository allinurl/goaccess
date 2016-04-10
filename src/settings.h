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

#ifndef SETTINGS_H_INCLUDED
#define SETTINGS_H_INCLUDED

#include <stdint.h>
#include "commons.h"

#define MAX_LINE_CONF     512
#define MAX_EXTENSIONS    128
#define MAX_IGNORE_IPS     64
#define MAX_IGNORE_REF     64
#define MAX_CUSTOM_COLORS  64
#define MAX_IGNORE_STATUS  64
#define NO_CONFIG_FILE "No config file used"

typedef enum
{
  COMBINED,
  VCOMBINED,
  COMMON,
  VCOMMON,
  W3C,
  SQUID,
  CLOUDFRONT,
  CLOUDSTORAGE,
  AWSELB,
} LOGTYPE;

/* predefined log times */
typedef struct GPreConfTime_
{
  const char *fmt24;
  const char *usec;
  const char *sec;
} GPreConfTime;

/* predefined log dates */
typedef struct GPreConfDate_
{
  const char *apache;
  const char *w3c;
  const char *usec;
  const char *sec;
} GPreConfDate;

/* predefined log formats */
typedef struct GPreConfLog_
{
  const char *combined;
  const char *vcombined;
  const char *common;
  const char *vcommon;
  const char *w3c;
  const char *cloudfront;
  const char *cloudstorage;
  const char *awselb;
  const char *squid;
} GPreConfLog;

/* All configuration properties */
typedef struct GConf_
{
  char *date_format;            /* date format */
  char *debug_log;              /* debug log path */
  char *geoip_database;         /* geoip db path */
  char *html_report_title;      /* report title */
  char *iconfigfile;            /* config file path */
  char *ifile;                  /* log file */
  char *ignore_ips[MAX_IGNORE_IPS];     /* array of ips to ignore */
  char *ignore_referers[MAX_IGNORE_REF];        /* referrers to ignore */
  char *invalid_requests_log;   /* invalid lines log path */
  char *log_format;             /* log format */
  char *output_format;          /* output format, e.g., HTML, JSON */
  char *sort_panels[TOTAL_MODULES];     /* sorting options for each panel */
  char *time_format;            /* time format */
  const char *colors[MAX_CUSTOM_COLORS];        /* colors */
  const char *ignore_panels[TOTAL_MODULES];     /* array of panels to ignore */
  const char *ignore_status[MAX_IGNORE_STATUS]; /* status to ignore */
  const char *static_files[MAX_EXTENSIONS];     /* static extensions */

  int all_static_files;
  int append_method;
  int append_protocol;
  int bandwidth;
  int client_err_to_unique_count;
  int code444_as_404;
  int color_scheme;
  int double_decode;
  int enable_html_resolver;
  int geo_db;
  int hl_header;
  int ignore_crawlers;
  int ignore_qstr;
  int list_agents;
  int load_conf_dlg;
  int load_global_config;
  int mouse_support;
  int no_color;
  int no_column_names;
  int no_csv_summary;
  int no_progress;
  int no_tab_scroll;
  int output_html;
  int real_os;
  int serve_usecs;
  int skip_term_resolver;

  int color_idx;
  int ignore_ip_idx;
  int ignore_panel_idx;
  int ignore_referer_idx;
  int ignore_status_idx;
  int sort_panel_idx;
  int static_file_idx;

  size_t static_file_max_len;

  /* TokyoCabinet */
  char *db_path;
  int64_t xmmap;
  int cache_lcnum;
  int cache_ncnum;
  int compression;
  int keep_db_files;
  int load_from_disk;
  int tune_bnum;
  int tune_lmemb;
  int tune_nmemb;
} GConf;

char *get_selected_date_str (size_t idx);
char *get_selected_format_str (size_t idx);
char *get_selected_time_str (size_t idx);
size_t get_selected_format_idx (void);

extern GConf conf;

char *get_config_file_path (void);
int parse_conf_file (int *argc, char ***argv);
void free_cmd_args (void);
void set_default_static_files (void);

#endif
