/**
 * Copyright (C) 2009-2014 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An Ncurses apache weblog analyzer & interactive viewer
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

#ifndef SETTINGS_H_INCLUDED
#define SETTINGS_H_INCLUDED

#include <stdint.h>
#include "commons.h"

#define MAX_LINE_CONF 512
#define MAX_EXTENSIONS 64
#define MAX_IGNORE_IPS 64
#define MAX_IGNORE_REF 64
#define NO_CONFIG_FILE "No config file used"

typedef enum
{
  COMBINED,
  VCOMBINED,
  COMMON,
  VCOMMON,
  W3C,
  CLOUDFRONT,
  CLOUDSTORAGE,
} LOGTYPE;

/* predefined log dates */
typedef struct GPreConfTime_
{
  const char *fmt24;
  const char *usec;
} GPreConfTime;

/* predefined log dates */
typedef struct GPreConfDate_
{
  const char *apache;
  const char *w3c;
  const char *usec;
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
} GPreConfLog;

typedef struct GConfKeyword_
{
  const unsigned short key_id;
  const char *keyword;
} GConfKeyword;

typedef struct GConf_
{
  char *date_format;
  char *debug_log;
  char *geoip_database;
  char *html_report_title;
  char *iconfigfile;
  char *ifile;
  char *ignore_ips[MAX_IGNORE_IPS];
  char *ignore_panels[TOTAL_MODULES];
  char *ignore_referers[MAX_IGNORE_REF];
  char *log_format;
  char *output_format;
  char *sort_panels[TOTAL_MODULES];
  char *static_files[MAX_EXTENSIONS];
  char *time_format;

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
  int no_csv_summary;
  int no_progress;
  int output_html;
  int real_os;
  int serve_usecs;
  int skip_term_resolver;

  int ignore_ip_idx;
  int ignore_panel_idx;
  int ignore_referer_idx;
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
char *get_selected_time_str (size_t idx);
char *get_selected_format_str (size_t idx);
size_t get_selected_format_idx (void);

extern GConf conf;

int parse_conf_file (int *argc, char ***argv);
int ignore_panel (GModule mod);
void free_cmd_args (void);

#endif
