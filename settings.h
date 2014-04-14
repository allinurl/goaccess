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

/* predefined log dates */
typedef struct GPreConfDate_
{
  const char *apache;
  const char *w3c;
  const char *cloudfront;
} GPreConfDate;

/* predefined log formats */
typedef struct GPreConfLog_
{
  const char *combined;
  const char *common;
  const char *vcombined;
  const char *vcommon;
  const char *w3c;
  const char *cloudfront;
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
  char *iconfigfile;
  char *ifile;
  char *ignore_host;
  char *log_format;
  char *output_format;
  int append_method;
  int append_protocol;
  int bandwidth;
  int color_scheme;
  int enable_html_resolver;
  int geo_db;
  int ignore_qstr;
  int list_agents;
  int load_conf_dlg;
  int mouse_support;
  int no_color;
  int output_html;
  int real_os;
  int serve_secs;
  int serve_usecs;
  int skip_term_resolver;

  /* TokyoCabinet */
  char *db_path;
  int cache_lcnum;
  int cache_ncnum;
  int tune_lmemb;
  int tune_nmemb;
  int tune_bnum;
  int64_t xmmap;
} GConf;

char *get_selected_date_str (size_t idx);
char *get_selected_format_str (size_t idx);
size_t get_selected_format_idx (void);

extern GConf conf;
extern char *tmp_log_format;
extern char *tmp_date_format;

int parse_conf_file (void);
int write_conf_file (void);

#endif
