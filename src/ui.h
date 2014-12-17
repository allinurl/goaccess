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

#ifndef UI_H_INCLUDED
#define UI_H_INCLUDED

#ifdef HAVE_NCURSESW_NCURSES_H
#include <ncursesw/ncurses.h>
#elif HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#elif HAVE_CURSES_H
#include <curses.h>
#endif

#ifdef HAVE_LIBPTHREAD
#include <pthread.h>
#endif

/* overall stats */
#define T_BW         "Bandwidth"
#define T_EXCLUDE_IP "Excl. IP Hits"
#define T_F_REQUESTS "Failed Requests"
#define T_GEN_TIME   "Generation Time"
#define T_HEAD       "Dashboard - Overall Analyzed Requests"
#define T_LOG        "Log Size"
#define T_LOG_PATH   "Log File"
#define T_REFERRER   "Referrers"
#define T_REQUESTS   "Total Requests"
#define T_STATIC_FIL "Static Files"
#define T_UNIQUE404  "Unique 404"
#define T_UNIQUE_FIL "Unique Files"
#define T_UNIQUE_VIS "Unique Visitors"

/* spinner label format */
#define SPIN_FMT "%s"
#define SPIN_FMTM "%s [%'d] [%'lld/s]"
#define SPIN_LBL 50

/* modules */
#define VISIT_HEAD "Unique visitors per day - Including spiders"
#define VISIT_DESC "Hits having the same IP, date and agent are a unique visit."
#define VISIT_ID   "visitors"
#define REQUE_HEAD "Top requests (URLs)"
#define REQUE_DESC "Top requests sorted by hits - [time served] [protocol] [method]"
#define REQUE_ID   "requests"
#define STATI_HEAD "Top static requests (e.g. jpg, png, js, css..)"
#define STATI_DESC "Top static requests sorted by hits - [time served] [protocol] [method]"
#define STATI_ID   "static_requests"
#define FOUND_HEAD "HTTP 404 Not Found URLs"
#define FOUND_DESC "Top 404 Not Found URLs sorted by hits - [time served] [protocol] [method]"
#define FOUND_ID   "not_found"
#define HOSTS_HEAD "Visitor hostnames and IPs"
#define HOSTS_DESC "Top visitor hosts sorted by hits - [bandwidth] [time served]"
#define HOSTS_ID   "hosts"
#define OPERA_HEAD "Operating Systems"
#define OPERA_DESC "Top Operating Systems sorted by unique visitors"
#define OPERA_ID   "os"
#define BROWS_HEAD "Browsers"
#define BROWS_DESC "Top Browsers sorted by unique visitors"
#define BROWS_ID   "browsers"
#define REFER_HEAD "Referrers URLs"
#define REFER_DESC "Top Requested Referrers sorted by hits"
#define REFER_ID   "referrers"
#define SITES_HEAD "Referring Sites"
#define SITES_DESC "Top Referring Sites sorted by hits"
#define SITES_ID   "referring_sites"
#define KEYPH_HEAD "Keyphrases from Google's search engine"
#define KEYPH_DESC "Top Keyphrases sorted by hits"
#define KEYPH_ID   "keyphrases"
#define GEOLO_HEAD "Geo Location"
#define GEOLO_DESC "Continent > Country sorted by unique visitors"
#define GEOLO_ID   "geolocation"
#define CODES_HEAD "HTTP Status Codes"
#define CODES_DESC "Top HTTP Status Codes sorted by hits"
#define CODES_ID   "status_codes"
#define GENER_ID   "general"

#define FIND_HEAD    "Find pattern in all modules"
#define FIND_DESC    "Regex allowed - ^g to cancel - TAB switch case"

#define MAX_CHOICES       300
#define MIN_HEIGHT        7
#define MIN_WIDTH         0
#define MAX_HEIGHT_FOOTER 1
#define MAX_HEIGHT_HEADER 6

/* CONFIG DIALOG */
#define CONF_MENU_H       6
#define CONF_MENU_W       48
#define CONF_MENU_X       2
#define CONF_MENU_Y       4
#define CONF_WIN_H        17
#define CONF_WIN_W        52

/* FIND DIALOG */
#define FIND_DLG_HEIGHT   8
#define FIND_DLG_WIDTH    50
#define FIND_MAX_MATCHES  1

/* COLOR SCHEME DIALOG */
#define SCHEME_MENU_H     2
#define SCHEME_MENU_W     38
#define SCHEME_MENU_X     2
#define SCHEME_MENU_Y     4
#define SCHEME_WIN_H      8
#define SCHEME_WIN_W      42

/* SORT DIALOG */
#define SORT_MENU_H       6
#define SORT_MENU_W       38
#define SORT_MENU_X       2
#define SORT_MENU_Y       4
#define SORT_WIN_H        13
#define SORT_WIN_W        42

/* AGENTS DIALOG */
#define AGENTS_MENU_X     2
#define AGENTS_MENU_Y     4

/* HELP DIALOG */
#define HELP_MENU_HEIGHT  12
#define HELP_MENU_WIDTH   60
#define HELP_MENU_X       2
#define HELP_MENU_Y       4
#define HELP_WIN_HEIGHT   17
#define HELP_WIN_WIDTH    64

/* COLORS */
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

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#include "commons.h"
#include "sort.h"

typedef enum SCHEMES
{
  NO_COLOR,
  MONOCHROME,
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
  const char *label;
  int color;
  int curses;
  int spin_x;
  int w;
  int x;
  int y;
  pthread_mutex_t mutex;
  pthread_t thread;
  unsigned int *process;
  WINDOW *win;
  enum
  {
    SPN_RUN,
    SPN_END
  } state;
} GSpinner;

/* *INDENT-OFF* */
char *get_browser_type (char *line);
char *input_string (WINDOW * win, int pos_y, int pos_x, size_t max_width, const char *str, int enable_case, int *toggle_case);
GSpinner *new_gspinner (void);
int split_agent_str (char *ptr_value, GAgents * agents, int max);
int verify_format (GLog * logger, GSpinner * spinner);
void close_win (WINDOW * w);
void display_general (WINDOW * header_win, char *ifile, GLog *logger);
void draw_header (WINDOW * win, const char *s, const char *fmt, int y, int x, int w, int color, int max_width);
void end_spinner (void);
void generate_time (void);
void init_colors (void);
void init_windows (WINDOW ** header_win, WINDOW ** main_win);
void load_agent_list (WINDOW * main_win, char *addr);
void load_help_popup (WINDOW * main_win);
void load_schemes_win (WINDOW * main_win);
void load_sort_win (WINDOW * main_win, GModule module, GSort * sort);
void set_curses_spinner (GSpinner *spinner);
void set_input_opts (void);
void term_size (WINDOW * main_win);
void ui_spinner_create (GSpinner * spinner);
void update_active_module (WINDOW * header_win, GModule current);
void update_header (WINDOW * header_win, int current);
WINDOW *create_win (int h, int w, int y, int x);
/* *INDENT-ON* */

#endif
