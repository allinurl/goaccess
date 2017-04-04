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
#include <libintl.h>
#define _(String) dgettext (PACKAGE , String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

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

/* Global UI defaults */
#define MIN_HEIGHT        8     /* minimum window height */
#define MIN_WIDTH         0     /* minimum window width */
#define MAX_HEIGHT_FOOTER 1     /* height of the footer window */
#define MAX_HEIGHT_HEADER 7     /* height of the header window */
#define OVERALL_NUM_COLS  4     /* number of columns on the overall stats win */

/* Overall Stats Labels */
#define T_DASH       _("Dashboard")
#define T_HEAD       _("Overall Analyzed Requests")
#define T_DASH_HEAD  _("Dashboard - Overall Analyzed Requests")

#define T_DATETIME   _("Date/Time")
#define T_REQUESTS   _("Total Requests")
#define T_GEN_TIME   _("Init. Proc. Time")
#define T_FAILED     _("Failed Requests")
#define T_VALID      _("Valid Requests")
#define T_UNIQUE_VIS _("Unique Visitors")
#define T_UNIQUE_FIL _("Unique Files")
#define T_EXCLUDE_IP _("Excl. IP Hits")
#define T_REFERRER   _("Referrers")
#define T_UNIQUE404  _("Unique 404")
#define T_STATIC_FIL _("Static Files")
#define T_LOG        _("Log Size")
#define T_BW         _("Bandwidth")
#define T_LOG_PATH   _("Log Source")

/* Spinner Label Format */
#define SPIN_FMT "%s"
#define SPIN_FMTM "%s [%'d] [%'lld/s]"
#define SPIN_LBL 50     /* max length of the progress spinner */

/* Module Labels and Descriptions */
#define VISIT_HEAD  N_("Unique visitors per day")
#define VISIT_HBOTS N_("Unique visitors per day - Including spiders")
#define VISIT_DESC  N_("Hits having the same IP, date and agent are a unique visit.")
#define VISIT_LABEL N_("Visitors")
#define VISIT_ID    "visitors"

#define REQUE_HEAD  N_("Requested Files (URLs)")
#define REQUE_DESC  N_("Top requests sorted by hits [, avgts, cumts, maxts, mthd, proto]")
#define REQUE_LABEL N_("Requests")
#define REQUE_ID    "requests"

#define STATI_HEAD  N_("Static Requests")
#define STATI_DESC  N_("Top static requests sorted by hits [, avgts, cumts, maxts, mthd, proto]")
#define STATI_LABEL N_("Static Requests")
#define STATI_ID    "static_requests"

#define VTIME_HEAD  N_("Time Distribution")
#define VTIME_DESC  N_("Data sorted by hour [, avgts, cumts, maxts]")
#define VTIME_LABEL N_("Time")
#define VTIME_ID    "visit_time"

#define VHOST_HEAD  N_("Virtual Hosts")
#define VHOST_DESC  N_("Data sorted by hits [, avgts, cumts, maxts]")
#define VHOST_LABEL N_("Virtual Hosts")
#define VHOST_ID    "vhosts"

#define RUSER_HEAD  N_("Remote User (HTTP authentication)")
#define RUSER_DESC  N_("Data sorted by hits [, avgts, cumts, maxts]")
#define RUSER_LABEL N_("Remote User")
#define RUSER_ID    "remote_user"

#define FOUND_HEAD  N_("Not Found URLs (404s)")
#define FOUND_DESC  N_("Top not found URLs sorted by hits [, avgts, cumts, maxts, mthd, proto]")
#define FOUND_LABEL N_("Not Found")
#define FOUND_ID    "not_found"

#define HOSTS_HEAD  N_("Visitor Hostnames and IPs")
#define HOSTS_DESC  N_("Top visitor hosts sorted by hits [, avgts, cumts, maxts]")
#define HOSTS_LABEL N_("Hosts")
#define HOSTS_ID    "hosts"

#define OPERA_HEAD  N_("Operating Systems")
#define OPERA_DESC  N_("Top Operating Systems sorted by hits [, avgts, cumts, maxts]")
#define OPERA_LABEL N_("OS")
#define OPERA_ID    "os"

#define BROWS_HEAD  N_("Browsers")
#define BROWS_DESC  N_("Top Browsers sorted by hits [, avgts, cumts, maxts]")
#define BROWS_LABEL N_("Browsers")
#define BROWS_ID    "browsers"

#define REFER_HEAD  N_("Referrers URLs")
#define REFER_DESC  N_("Top Requested Referrers sorted by hits [, avgts, cumts, maxts]")
#define REFER_LABEL N_("Referrers")
#define REFER_ID    "referrers"

#define SITES_HEAD  N_("Referring Sites")
#define SITES_DESC  N_("Top Referring Sites sorted by hits [, avgts, cumts, maxts]")
#define SITES_LABEL N_("Referring Sites")
#define SITES_ID    "referring_sites"

#define KEYPH_HEAD  N_("Keyphrases from Google's search engine")
#define KEYPH_DESC  N_("Top Keyphrases sorted by hits [, avgts, cumts, maxts]")
#define KEYPH_LABEL N_("Keyphrases")
#define KEYPH_ID    "keyphrases"

#define GEOLO_HEAD  N_("Geo Location")
#define GEOLO_DESC  N_("Continent > Country sorted by unique hits [, avgts, cumts, maxts]")
#define GEOLO_LABEL N_("Geo Location")
#define GEOLO_ID    "geolocation"

#define CODES_HEAD  N_("HTTP Status Codes")
#define CODES_DESC  N_("Top HTTP Status Codes sorted by hits [, avgts, cumts, maxts]")
#define CODES_LABEL N_("Status Codes")
#define CODES_ID    "status_codes"

#define GENER_ID   "general"

/* Overall Statistics CSV/JSON Keys */
#define OVERALL_STARTDATE "start_date"
#define OVERALL_ENDDATE   "end_date"
#define OVERALL_DATETIME  "date_time"
#define OVERALL_REQ       "total_requests"
#define OVERALL_VALID     "valid_requests"
#define OVERALL_GENTIME   "generation_time"
#define OVERALL_FAILED    "failed_requests"
#define OVERALL_VISITORS  "unique_visitors"
#define OVERALL_FILES     "unique_files"
#define OVERALL_EXCL_HITS "excluded_hits"
#define OVERALL_REF       "unique_referrers"
#define OVERALL_NOTFOUND  "unique_not_found"
#define OVERALL_STATIC    "unique_static_files"
#define OVERALL_LOGSIZE   "log_size"
#define OVERALL_BANDWIDTH "bandwidth"
#define OVERALL_LOG       "log_path"

/* Metric Labels */
#define MTRC_HITS_LBL            _("Hits")
#define MTRC_VISITORS_LBL        _("Visitors")
#define MTRC_VISITORS_SHORT_LBL  _("Vis.")
#define MTRC_BW_LBL              _("Bandwidth")
#define MTRC_AVGTS_LBL           _("Avg. T.S.")
#define MTRC_CUMTS_LBL           _("Cum. T.S.")
#define MTRC_MAXTS_LBL           _("Max. T.S.")
#define MTRC_METHODS_LBL         _("Method")
#define MTRC_METHODS_SHORT_LBL   _("Mtd")
#define MTRC_PROTOCOLS_LBL       _("Protocol")
#define MTRC_PROTOCOLS_SHORT_LBL _("Proto")
#define MTRC_CITY_LBL            _("City")
#define MTRC_COUNTRY_LBL         _("Country")
#define MTRC_HOSTNAME_LBL        _("Hostname")
#define MTRC_DATA_LBL            _("Data")

/* Find Labels */
#define FIND_HEAD    _("Find pattern in all views")
#define FIND_DESC    _("Regex allowed - ^g to cancel - TAB switch case")

/* CONFIG DIALOG */
#define CONF_MENU_H       6
#define CONF_MENU_W       57
#define CONF_MENU_X       2
#define CONF_MENU_Y       4
#define CONF_WIN_H        20
#define CONF_WIN_W        61

/* FIND DIALOG */
#define FIND_DLG_HEIGHT   8
#define FIND_DLG_WIDTH    50
#define FIND_MAX_MATCHES  1

/* COLOR SCHEME DIALOG */
#define SCHEME_MENU_H     4
#define SCHEME_MENU_W     38
#define SCHEME_MENU_X     2
#define SCHEME_MENU_Y     4
#define SCHEME_WIN_H      10
#define SCHEME_WIN_W      42

/* SORT DIALOG */
#define SORT_MENU_H       6
#define SORT_MENU_W       38
#define SORT_MENU_X       2
#define SORT_MENU_Y       4
#define SORT_WIN_H        13
#define SORT_WIN_W        42
#define SORT_ASC_SEL      "[x] ASC [ ] DESC"
#define SORT_DESC_SEL     "[ ] ASC [x] DESC"

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

/* CONF ERROR DIALOG */
#define ERR_MENU_HEIGHT  10
#define ERR_MENU_WIDTH   57
#define ERR_MENU_X       2
#define ERR_MENU_Y       4
#define ERR_WIN_HEIGHT   15
#define ERR_WIN_WIDTH    61
#define ERR_HEADER _("Format Errors - Verify your log/date/time format")

#define CSENSITIVE   _("[x] case sensitive")
#define CISENSITIVE  _("[ ] case sensitive")

/* Convenient macros */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#include "color.h"
#include "commons.h"
#include "sort.h"

/* Curses dashboard find */
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

/* Each panel contains its own scrolling and offset */
typedef struct GScrollModule_
{
  int scroll;
  int offset;
} GScrollModule;

/* Curses Scrolling */
typedef struct GScroll_
{
  GScrollModule module[TOTAL_MODULES];
  GModule current;
  int dash;
  int expanded;
} GScroll;

/* Spinner or Progress Indicator */
typedef struct GSpinner_
{
  const char *label;
  GColors *(*color) (void);
  int curses;
  int spin_x;
  int w;
  int x;
  int y;
  pthread_mutex_t mutex;
  pthread_t thread;
  unsigned int *processed;
  WINDOW *win;
  enum
  {
    SPN_RUN,
    SPN_END
  } state;
} GSpinner;

/* Controls metric output.
 * i.e., which metrics it should display */
typedef struct GOutput_
{
  GModule module;
  int8_t visitors;
  int8_t hits;
  int8_t percent;
  int8_t bw;
  int8_t avgts;
  int8_t cumts;
  int8_t maxts;
  int8_t protocol;
  int8_t method;
  int8_t data;
  int8_t graph;                 /* display bars when collapsed */
  int8_t sub_graph;             /* display bars upon expanding it */
} GOutput;

/* *INDENT-OFF* */
GOutput *output_lookup (GModule module);
GSpinner *new_gspinner (void);

char *get_browser_type (char *line);
char *get_overall_header (GHolder *h);
char *input_string (WINDOW * win, int pos_y, int pos_x, size_t max_width, const char *str, int enable_case, int *toggle_case);
const char *module_to_desc (GModule module);
const char *module_to_head (GModule module);
const char *module_to_id (GModule module);
const char *module_to_label (GModule module);
int get_start_end_parsing_dates (GHolder * h, char **start, char **end, const char *f);
int render_confdlg (GLog * glog, GSpinner * spinner);
int set_host_agents (const char *addr, void (*func) (void *, void *, int), void *arr);
void close_win (WINDOW * w);
void display_general (WINDOW * win, GLog * glog, GHolder *h);
void draw_header (WINDOW * win, const char *s, const char *fmt, int y, int x, int w, GColors * (*func) (void));
void end_spinner (void);
void generate_time (void);
void init_colors (int force);
void init_windows (WINDOW ** header_win, WINDOW ** main_win);
void load_agent_list (WINDOW * main_win, char *addr);
void load_help_popup (WINDOW * main_win);
void load_schemes_win (WINDOW * main_win);
void load_sort_win (WINDOW * main_win, GModule module, GSort * sort);
void set_curses_spinner (GSpinner *spinner);
void set_input_opts (void);
void set_wbkgd (WINDOW *main_win, WINDOW *header_win);
void term_size (WINDOW * main_win, int *main_win_height);
void ui_spinner_create (GSpinner * spinner);
void update_active_module (WINDOW * header_win, GModule current);
void update_header (WINDOW * header_win, int current);
/* *INDENT-ON* */

#endif
