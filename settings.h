/**
 * Copyright (C) 2009-2013 by Gerardo Orellana <goaccess@prosoftcorp.com>
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

#include <limits.h>

/* predefined log dates */
typedef struct GPreConfDate_
{
   char *apache;
   char *w3c;
   char *cloudfront;
} GPreConfDate;

/* predefined log formats */
typedef struct GPreConfLog_
{
   char *combined;
   char *common;
   char *vcombined;
   char *vcommon;
   char *w3c;
   char *cloudfront;
} GPreConfLog;

typedef struct GConfKeyword_
{
   unsigned short key_id;
   char *keyword;
} GConfKeyword;

typedef struct GConf_
{
   char *ifile;
   char iconfigfile[_POSIX_PATH_MAX];
   char *ignore_host;
   char *date_format;
   char *log_format;
   char *output_format;
   int color_scheme;
   int list_agents;
   int load_conf_dlg;
   int skip_resolver;
   int output_html;
   int serve_usecs;
   int serve_secs;
   int bandwidth;
} GConf;

char *get_selected_date_str (size_t idx);
char *get_selected_format_str (size_t idx);
size_t get_selected_format_idx ();

extern GConf conf;
extern char *tmp_date_format;
extern char *tmp_log_format;

int parse_conf_file ();
void write_conf_file ();

#endif
