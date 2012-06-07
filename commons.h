/**
 * Copyright (C) 2009-2012 by Gerardo Orellana <goaccess@prosoftcorp.com>
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_LIBNCURSESW
#  include <ncursesw/menu.h>
#  include <ncursesw/curses.h>
#else
#  include <ncurses.h>
#  include <menu.h>
#endif

#include <glib.h>

#ifndef COMMONS_H_INCLUDED
#define COMMONS_H_INCLUDED

/* Remove the __attribute__ stuff when the compiler is not GCC. */
#if !__GNUC__
#  define __attribute__(x) /**/
#endif
#define BUFFER 			4096
#define BW_HASHTABLES   3
#define DATELEN         12
#define DEBUG
#define GO_UNUSED __attribute__((unused))
#define GO_VERSION 		"0.5.1"
#define MAX_CHOICES 	   300
#define MIN_HEIGHT 		7
#define MIN_WIDTH       0
#define OUTPUT_N        10
#define TOTAL_MODULES 	11
/* max height of footer stdscr (rows) */
#define MAX_HEIGHT_FOOTER 1
/* max height of header window (rows) */
#define MAX_HEIGHT_HEADER 6
#define KB (1024)
#define MB (KB * 1024)
#define GB (MB * 1024)
#define BLACK_CYAN   9
#define BLACK_GREEN  8
#define BLUE_GREEN   7
#define COL_BLACK    4
#define COL_BLUE     1
#define COL_CYAN     5
#define COL_GREEN    11
#define COL_RED      3
#define COL_WHITE    0
#define COL_YELLOW   6
#define WHITE_RED    10
extern enum MODULES modules;
extern enum SCHEMES schemes;

enum MODULES
{
   UNIQUE_VISITORS = 1,
   REQUESTS,
   REQUESTS_STATIC,
   REFERRERS,
   NOT_FOUND,
   OS,
   BROWSERS,
   HOSTS,
   STATUS_CODES,
   REFERRING_SITES,
   KEYPHRASES
};

enum SCHEMES
{
   MONOCHROME = 1,
   STD_GREEN
};

/* to create a new hash value out of a key, which can be NULL */
extern GHashTable *ht_browsers;
extern GHashTable *ht_date_bw;
extern GHashTable *ht_file_bw;
extern GHashTable *ht_host_bw;
extern GHashTable *ht_hosts;
extern GHashTable *ht_hosts_agents;
extern GHashTable *ht_keyphrases;
extern GHashTable *ht_monthly;
extern GHashTable *ht_not_found_requests;
extern GHashTable *ht_os;
extern GHashTable *ht_referrers;
extern GHashTable *ht_referring_sites;
extern GHashTable *ht_requests;
extern GHashTable *ht_requests_static;
extern GHashTable *ht_status_code;
extern GHashTable *ht_unique_vis;
extern GHashTable *ht_unique_visitors;

struct logger
{
   char *agent;
   char *date;
   char *host;
   char *hour;
   char *identd;
   char *ref;
   char *req;
   char *status;
   char *userid;
   int alloc_counter;
   int counter;
   int current_module;
   int max_value;
   int total_invalid;
   int total_process;
   long long resp_size;
};

struct struct_display
{
   char *data;
   int hits;
   int module;
};

struct struct_holder
{
   char *data;
   int hits;
   int curr_module;
};

struct scrolling
{
   size_t init_scrl_main_win;
   size_t scrl_help_win;
   size_t scrl_main_win;
   size_t scrl_agen_win;
};

struct tm *now_tm;

/* Declaration of variables */
/* enable flags */
extern char *ignore_host;
extern int host_agents_list_flag;
extern int outputting;
extern int piping;

/* iteration */
extern int iter_ctr;
extern int iter_module;

/* string processing */
extern long long req_size;

/* Processing time */
extern time_t end_proc;
extern time_t now;
extern time_t start_proc;

/* resizing */
extern size_t real_size_y;
extern size_t term_h;
extern size_t term_w;

/* seetings */
extern char *date_format;
extern char *log_format;
extern char *tmp_date_format;
extern char *tmp_log_format;
extern char APACHE_DATE[];
extern char W3C_DATE[];
extern char COMBINED1[];
extern char COMBINED2[];
extern char COMMON1[];
extern char COMMON2[];
extern char W3C_INTERNET[];
extern int color_scheme;
extern char *conf_keywords[][2];

extern const char *vis_head;
extern const char *vis_desc;
extern const char *req_head;
extern const char *req_desc;
extern const char *static_head;
extern const char *static_desc;
extern const char *ref_head;
extern const char *ref_desc;
extern const char *not_found_head;
extern const char *not_found_desc;
extern const char *os_head;
extern const char *os_desc;
extern const char *browser_head;
extern const char *browser_desc;
extern const char *host_head;
extern const char *host_desc;
extern const char *status_head;
extern const char *status_desc;
extern const char *sites_head;
extern const char *sites_desc;
extern const char *key_head;
extern const char *key_desc;

/* file */
extern char *ifile;

extern char *browsers[][2];
extern char *codes[][2];
extern char *help_main[];
extern char *module_names[];
extern char *os[][2];

size_t browsers_size (void);
size_t codes_size (void);
size_t help_main_size (void);
size_t keywords_size (void);
size_t os_size (void);
void init_colors (void);

#endif
