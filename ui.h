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

#ifndef UI_H_INCLUDED
#define UI_H_INCLUDED

#ifdef HAVE_LIBGEOIP
#include <GeoIP.h>
#endif

#ifdef HAVE_LIBNCURSESW
#include <ncursesw/curses.h>
#else
#include <ncurses.h>
#endif

#ifdef HAVE_LIBGLIB_2_0
#include <glib.h>
#endif

#ifdef HAVE_LIBPTHREAD
#include <pthread.h>
#endif

#define T_BW         "Bandwidth"
#define T_F_REQUESTS "Failed Requests"
#define T_GEN_TIME   "Generation Time"
#define T_HEAD       "General Dashboard - Overall Analyzed Requests"
#define T_LOG        "Log Size"
#define T_REFERRER   "Referrers"
#define T_REQUESTS   "Total Requests"
#define T_STATIC_FIL "Static Files"
#define T_UNIQUE404  "Unique 404"
#define T_UNIQUE_FIL "Unique Files"
#define T_LOG_PATH   "Log File"
#define T_UNIQUE_VIS "Unique Visitors"

#define FIND_HEAD    "Find pattern in all modules"
#define FIND_DESC    "Regex allowed - ^g to cancel - TAB switch case"

#define MAX_CHOICES       300
#define MIN_HEIGHT        7
#define MIN_WIDTH         0
#define MAX_HEIGHT_FOOTER 1
#define MAX_HEIGHT_HEADER 6

#define CONF_MENU_H       6
#define CONF_MENU_W       48
#define CONF_MENU_X       2
#define CONF_MENU_Y       4
#define CONF_WIN_H        17
#define CONF_WIN_W        52

#define FIND_DLG_HEIGHT   8
#define FIND_DLG_WIDTH    50
#define FIND_MAX_MATCHES  1

#define SCHEME_MENU_H     2
#define SCHEME_MENU_W     38
#define SCHEME_MENU_X     2
#define SCHEME_MENU_Y     4
#define SCHEME_WIN_H      8
#define SCHEME_WIN_W      42

#define SORT_MENU_H       4
#define SORT_MENU_W       38
#define SORT_MENU_X       2
#define SORT_MENU_Y       4
#define SORT_WIN_H        11
#define SORT_WIN_W        42

#define AGENTS_MENU_X     2
#define AGENTS_MENU_Y     4

#define HELP_MENU_HEIGHT  12
#define HELP_MENU_WIDTH   52
#define HELP_MENU_X       2
#define HELP_MENU_Y       4
#define HELP_WIN_HEIGHT   17
#define HELP_WIN_WIDTH    56

#define BLACK_CYAN        9
#define BLACK_GREEN       8
#define BLUE_GREEN        7
#define COL_BLACK         4
#define COL_BLUE          1
#define COL_CYAN          5
#define COL_GREEN         11
#define COL_RED           3
#define COL_WHITE         0
#define COL_YELLOW        6
#define WHITE_RED         10

#define HIGHLIGHT         1

#define TOTAL_MODULES     13

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#include "parser.h"

typedef enum MODULES
{
   VISITORS,
   REQUESTS,
   REQUESTS_STATIC,
   NOT_FOUND,
   HOSTS,
   OS,
   BROWSERS,
   REFERRERS,
   REFERRING_SITES,
   KEYPHRASES,
   STATUS_CODES,
   COUNTRIES,
   CONTINENTS,
} GModule;

typedef enum SCHEMES
{
   MONOCHROME = 1,
   STD_GREEN
} GShemes;

typedef struct GFind_
{
   GModule module;
   char *pattern;
   int next_idx;
   int next_parent_idx;
   int next_sub_idx;
   int look_in_sub;
   int done;
   int icase;
} GFind;

typedef struct GSortModule_
{
   int hits;
   int data;
   int bw;
   int usecs;
   const char *choices[5];
} GSortModule;

typedef struct GSort_
{
   GModule module;
   enum
   {
      SORT_BY_HITS,
      SORT_BY_DATA,
      SORT_BY_BW,
      SORT_BY_USEC,
   } field;
   enum
   {
      SORT_ASC,
      SORT_DESC
   } sort;
} GSort;

typedef struct GScrollModule_
{
   int scroll;
   int offset;
} GScrollModule;

typedef struct GScrolling_
{
   GScrollModule module[TOTAL_MODULES];
   GModule current;
   int dash;
   int expanded;
} GScrolling;

typedef struct GAgents_
{
   char *agents;
   int size;
} GAgents;

typedef struct GSpinner_
{
   pthread_t thread;
   pthread_mutex_t mutex;
   WINDOW *win;
   int y;
   int x;
   int color;
   enum
   {
      SPN_RUN,
      SPN_END
   } state;
} GSpinner;

extern GeoIP *geo_location_data;

/* *INDENT-OFF* */
char *get_browser_type (char *line);
char *ht_bw_str (GHashTable * ht, const char *key);
char *input_string (WINDOW * win, int pos_y, int pos_x, size_t max_width, char *str, int enable_case, int *toggle_case);
GSpinner *new_gspinner ();
int split_agent_str (char *ptr_value, GAgents * agents, int max);
int verify_format (GLog * logger, GSpinner * spinner);
void close_win (WINDOW * w);
void display_general (WINDOW * header_win, char *ifile, int piping, int process, int invalid, unsigned long long bandwidth);
void draw_header (WINDOW * win, char *header, int x, int y, int w, int color);
void generate_time (void);
void init_colors ();
void load_agent_list (WINDOW * main_win, char *addr);
void load_help_popup (WINDOW * main_win);
void load_schemes_win (WINDOW * main_win);
void load_sort_win (WINDOW * main_win, GModule module, GSort * sort);
void set_input_opts (void);
void term_size (WINDOW * main_win);
void ui_spinner_create (GSpinner * spinner);
void update_active_module (WINDOW * header_win, GModule current);
void update_header (WINDOW * header_win, int current);
WINDOW *create_win (int h, int w, int y, int x);
/* *INDENT-ON* */

#endif
