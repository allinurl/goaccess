/**
 * ui.h -- various curses interfaces
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2026 Gerardo Orellana <hello @ goaccess.io>
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

#include "labels.h"
#include "commons.h"

/* Global UI defaults */
#define MIN_HEIGHT        8
#define MIN_WIDTH         0
#define MAX_HEIGHT_FOOTER 1
#define MAX_HEIGHT_HEADER 7
#define OVERALL_NUM_COLS  4

/* Spinner Label Format */
#define SPIN_FMT "%s"
#define SPIN_FMTM "[%s %s] {%'"PRIu64"} @ {%'lld/s}"
#define SPIN_LBL 256
#define SPIN_UPDATE_INTERVAL 100000000

/* Module JSON keys */
#define VISITORS_ID        "visitors"
#define REQUESTS_ID        "requests"
#define REQUESTS_STATIC_ID "static_requests"
#define VISIT_TIMES_ID     "visit_time"
#define VIRTUAL_HOSTS_ID   "vhosts"
#define REMOTE_USER_ID     "remote_user"
#define CACHE_STATUS_ID    "cache_status"
#define NOT_FOUND_ID       "not_found"
#define HOSTS_ID           "hosts"
#define OS_ID              "os"
#define BROWSERS_ID        "browsers"
#define REFERRERS_ID       "referrers"
#define REFERRING_SITES_ID "referring_sites"
#define KEYPHRASES_ID      "keyphrases"
#define GEO_LOCATION_ID    "geolocation"
#define ASN_ID             "asn"
#define STATUS_CODES_ID    "status_codes"
#define GENER_ID           "general"
#define MIME_TYPE_ID       "mime_type"
#define TLS_TYPE_ID        "tls_type"

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

/* CONFIG DIALOG */
#define CONF_MENU_H       6
#define CONF_MENU_W       67
#define CONF_MENU_X       2
#define CONF_MENU_Y       4
#define CONF_WIN_H        20
#define CONF_WIN_W        71
#define CONF_MAX_LEN_DLG  512

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
#define ERR_MENU_HEIGHT   10
#define ERR_MENU_WIDTH    67
#define ERR_MENU_X        2
#define ERR_MENU_Y        4
#define ERR_WIN_HEIGHT    15
#define ERR_WIN_WIDTH     71

/* ORDER PANELS */
#define PANELS_MENU_X     2
#define PANELS_MENU_Y     4
#define PANELS_WIN_H      22
#define PANELS_WIN_W      50

#include "color.h"
#include "sort.h"

typedef struct GFind_ {
  GModule module;
  char *pattern;
  int next_idx;
  int next_parent_idx;
  int next_sub_idx;
  int look_in_sub;
  int done;
  int icase;
} GFind;

/* Helper: snapshot current spinner state (avoids holding mutex too long) */
typedef struct {
  int state;
  bool curses;
  uint64_t processed;
  int64_t elapsed_sec;
  const char *filename;
} SpinnerSnapshot;

typedef struct GScrollModule_ {
  int scroll;
  int offset;
  int current_metric;
  int use_log_scale;
  int reverse_bars;
  /* Per-item expand/collapse state for hierarchical navigation.
   * When NULL, all items are expanded (legacy behavior).
   * Otherwise, item_expanded[i] == 1 means root item i's children are visible. */
  uint8_t *item_expanded;
  int item_expanded_size;
} GScrollModule;

typedef struct GScroll_ {
  GScrollModule module[TOTAL_MODULES];
  GModule current;
  int dash;
  int expanded;
} GScroll;

typedef struct GSpinner_ {
  const char *label;
  GColors *(*color) (void);
  int curses;
  int spin_x;
  int w;
  int x;
  int y;
  pthread_mutex_t mutex;
  pthread_t thread;
  uint64_t **processed;
  char **filename;
  time_t start_time;
  WINDOW *win;
  enum {
    SPN_RUN,
    SPN_END
  } state;
} GSpinner;

typedef struct GOutput_ {
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
  int8_t graph;
  int8_t sub_graph;
} GOutput;

/* Core UI functions */
const GOutput *output_lookup (GModule module);
GSpinner *new_gspinner (void);
char *get_overall_header (GHolder * h);
char *input_string (WINDOW * win, int pos_y, int pos_x, size_t max_width, const char *str,
                    int enable_case, int *toggle_case);
char *set_default_string (WINDOW * win, int pos_y, int pos_x, size_t max_width, const char *str);
const char *module_to_desc (GModule module);
const char *module_to_head (GModule module);
const char *module_to_id (GModule module);
const char *module_to_label (GModule module);
int get_start_end_parsing_dates (char **start, char **end, const char *f);
void close_win (WINDOW * w);
void display_general (WINDOW * win, GHolder * h);
void draw_header (WINDOW * win, const char *s, const char *fmt, int y, int x, int w,
                  GColors * (*func) (void));
void end_spinner (void);
void generate_time (void);
void init_colors (int force);
void init_windows (WINDOW ** header_win, WINDOW ** main_win);
void lock_spinner (void);
void set_curses_spinner (GSpinner * spinner);
void set_input_opts (void);
void set_wbkgd (WINDOW * main_win, WINDOW * header_win);
void term_size (WINDOW * main_win, int *main_win_height);
void ui_spinner_create (GSpinner * spinner);
void unlock_spinner (void);
void update_active_module (WINDOW * header_win, GModule current);

/* Per-item expand/collapse state management */
void init_item_expanded (GScrollModule * smod, int num_items);
void free_item_expanded (GScrollModule * smod);
void reset_item_expanded (GScrollModule * smod);

#endif
