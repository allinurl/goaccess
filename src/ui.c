/**
 * ui.c -- various curses interfaces
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

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#define STDIN_FILENO  0

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <pthread.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "ui.h"
#include "color.h"
#include "error.h"
#include "gkhash.h"
#include "gmenu.h"
#include "goaccess.h"
#include "util.h"
#include "xmalloc.h"

/* *INDENT-OFF* */
/* Determine which metrics should be displayed per module/panel */
static const GOutput outputting[] = {
  {VISITORS        , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 1 , 1} ,
  {REQUESTS        , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0} ,
  {REQUESTS_STATIC , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0} ,
  {NOT_FOUND       , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0} ,
  {HOSTS           , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 1 , 0} ,
  {OS              , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 1 , 1} ,
  {BROWSERS        , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 1 , 1} ,
  {VISIT_TIMES     , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 1 , 1} ,
  {VIRTUAL_HOSTS   , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0 , 0} ,
  {REFERRERS       , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0 , 0} ,
  {REFERRING_SITES , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0 , 0} ,
  {KEYPHRASES      , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0 , 0} ,
  {STATUS_CODES    , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0 , 0} ,
  {REMOTE_USER     , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0 , 0} ,
  {CACHE_STATUS    , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0 , 0} ,
#ifdef HAVE_GEOLOCATION
  {GEO_LOCATION    , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0 , 0} ,
  {ASN             , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0 , 0} ,
#endif
  {MIME_TYPE       , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0 , 0} ,
  {TLS_TYPE        , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0 , 0} ,
};
/* *INDENT-ON* */

/* Structure to display overall statistics */
typedef struct Field_ {
  const char *field;
  /* char due to log, bw, log_file */
  char *value;
  GColors *(*colorlbl) (void);
  GColors *(*colorval) (void);
  short oneliner;
} Field;

/* Determine which metrics to output given a module
 *
 * On error, or if not found, NULL is returned.
 * On success, the panel value is returned. */
const GOutput *
output_lookup (GModule module) {
  int i, num_panels = ARRAY_SIZE (outputting);
  for (i = 0; i < num_panels; i++) {
    if (outputting[i].module == module)
      return &outputting[i];
  }
  return NULL;
}

/* Initialize curses colors */
void
init_colors (int force) {
  /* use default foreground/background colors */
  use_default_colors ();
  /* first set a default normal color */
  set_normal_color ();
  /* then parse custom colors and initialize them */
  set_colors (force);
}

/* Ncurses' window handling */
void
set_input_opts (void) {
  initscr ();
  clear ();
  noecho ();
  halfdelay (10);
  nonl ();
  intrflush (stdscr, FALSE);
  keypad (stdscr, TRUE);
  if (curs_set (0) == ERR)
    LOG_DEBUG (("Unable to change cursor: %s\n", strerror (errno)));
  if (conf.mouse_support)
    mousemask (BUTTON1_CLICKED, NULL);
}

/* Deletes the given window, freeing all memory associated with it. */
void
close_win (WINDOW *w) {
  if (w == NULL)
    return;
  wclear (w);
  wrefresh (w);
  delwin (w);
}

/* Get the current calendar time as a value of type time_t and convert
 * time_t to tm as local time */
void
generate_time (void) {
  if (conf.tz_name)
    set_tz ();
  timestamp = time (NULL);
  localtime_r (&timestamp, &now_tm);
}

/* Set the loading spinner as ended and manage the mutex locking. */
void
end_spinner (void) {
  if (conf.no_parsing_spinner)
    return;
  pthread_mutex_lock (&parsing_spinner->mutex);
  parsing_spinner->state = SPN_END;
  pthread_mutex_unlock (&parsing_spinner->mutex);
  if (!parsing_spinner->curses) {
    /* wait for the ui_spinner thread to finish */
    struct timespec ts = {.tv_sec = 0,.tv_nsec = SPIN_UPDATE_INTERVAL };
    if (nanosleep (&ts, NULL) == -1 && errno != EINTR)
      FATAL ("nanosleep: %s", strerror (errno));
  }
}

/* Set background colors to all windows. */
void
set_wbkgd (WINDOW *main_win, WINDOW *header_win) {
  GColors *color = get_color (COLOR_BG);
  /* background colors */
  wbkgd (main_win, COLOR_PAIR (color->pair->idx));
  wbkgd (header_win, COLOR_PAIR (color->pair->idx));
  wbkgd (stdscr, COLOR_PAIR (color->pair->idx));
  wrefresh (main_win);
}

/* Creates and the new terminal windows and set basic properties to
 * each of them. e.g., background color, enable the reading of
 * function keys. */
void
init_windows (WINDOW **header_win, WINDOW **main_win) {
  int row = 0, col = 0;
  /* init standard screen */
  getmaxyx (stdscr, row, col);
  if (row < MIN_HEIGHT || col < MIN_WIDTH)
    FATAL ("Minimum screen size - 0 columns by 7 lines");

  /* init header screen */
  *header_win = newwin (6, col, 0, 0);
  if (*header_win == NULL)
    FATAL ("Unable to allocate memory for header_win.");
  keypad (*header_win, TRUE);

  /* init main screen */
  *main_win = newwin (row - 8, col, 7, 0);
  if (*main_win == NULL)
    FATAL ("Unable to allocate memory for main_win.");
  keypad (*main_win, TRUE);
  set_wbkgd (*main_win, *header_win);
}

#pragma GCC diagnostic ignored "-Wformat-nonliteral"
/* Draw a generic header with the ability to set a custom text to it. */
void
draw_header (WINDOW *win, const char *s, const char *fmt, int y, int x, int w,
             GColors *(*func) (void)) {
  GColors *color = (*func) ();
  char *buf;
  buf = xmalloc (snprintf (NULL, 0, fmt, s) + 1);
  sprintf (buf, fmt, s);
  wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
  mvwhline (win, y, x, ' ', w);
  mvwaddnstr (win, y, x, buf, w);
  wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));
  free (buf);
}

#pragma GCC diagnostic warning "-Wformat-nonliteral"

/* Determine the actual size of the main window. */
void
term_size (WINDOW *main_win, int *main_win_height) {
  int term_h = 0, term_w = 0;
  getmaxyx (stdscr, term_h, term_w);
  *main_win_height = term_h - (MAX_HEIGHT_HEADER + MAX_HEIGHT_FOOTER);
  wresize (main_win, *main_win_height, term_w);
  wmove (main_win, *main_win_height, 0);
}

/* Get the module/panel label name for the given module enum value.
 *
 * On success, a string containing the label name is returned. */
const char *
module_to_label (GModule module) {
  static const char *const modules[] = {
    VISITORS_LABEL,
    REQUESTS_LABEL,
    REQUESTS_STATIC_LABEL,
    NOT_FOUND_LABEL,
    HOSTS_LABEL,
    OS_LABEL,
    BROWSERS_LABEL,
    VISIT_TIMES_LABEL,
    VIRTUAL_HOSTS_LABEL,
    REFERRERS_LABEL,
    REFERRING_SITES_LABEL,
    KEYPHRASES_LABEL,
    STATUS_CODES_LABEL,
    REMOTE_USER_LABEL,
    CACHE_STATUS_LABEL,
#ifdef HAVE_GEOLOCATION
    GEO_LOCATION_LABEL,
    ASN_LABEL,
#endif
    MIME_TYPE_LABEL,
    TLS_TYPE_LABEL,
  };
  return _(modules[module]);
}

/* Get the module/panel label id for the given module enum value.
 *
 * On success, a string containing the label id is returned. */
const char *
module_to_id (GModule module) {
  static const char *const modules[] = {
    VISITORS_ID,
    REQUESTS_ID,
    REQUESTS_STATIC_ID,
    NOT_FOUND_ID,
    HOSTS_ID,
    OS_ID,
    BROWSERS_ID,
    VISIT_TIMES_ID,
    VIRTUAL_HOSTS_ID,
    REFERRERS_ID,
    REFERRING_SITES_ID,
    KEYPHRASES_ID,
    STATUS_CODES_ID,
    REMOTE_USER_ID,
    CACHE_STATUS_ID,
#ifdef HAVE_GEOLOCATION
    GEO_LOCATION_ID,
    ASN_ID,
#endif
    MIME_TYPE_ID,
    TLS_TYPE_ID,
  };
  return _(modules[module]);
}

/* Get the module/panel label header for the given module enum value.
 *
 * On success, a string containing the label header is returned. */
const char *
module_to_head (GModule module) {
  static const char *modules[] = {
    VISITORS_HEAD,
    REQUESTS_HEAD,
    REQUESTS_STATIC_HEAD,
    NOT_FOUND_HEAD,
    HOSTS_HEAD,
    OS_HEAD,
    BROWSERS_HEAD,
    VISIT_TIMES_HEAD,
    VIRTUAL_HOSTS_HEAD,
    REFERRERS_HEAD,
    REFERRING_SITES_HEAD,
    KEYPHRASES_HEAD,
    STATUS_CODES_HEAD,
    REMOTE_USER_HEAD,
    CACHE_STATUS_HEAD,
#ifdef HAVE_GEOLOCATION
    GEO_LOCATION_HEAD,
    ASN_HEAD,
#endif
    MIME_TYPE_HEAD,
    TLS_TYPE_HEAD,
  };
  if (!conf.ignore_crawlers)
    modules[VISITORS] = VISITORS_HEAD_BOTS;
  return _(modules[module]);
}

/* Get the module/panel label description for the given module enum
 * value.
 *
 * On success, a string containing the label description is returned. */
const char *
module_to_desc (GModule module) {
  static const char *const modules[] = {
    VISITORS_DESC,
    REQUESTS_DESC,
    REQUESTS_STATIC_DESC,
    NOT_FOUND_DESC,
    HOSTS_DESC,
    OS_DESC,
    BROWSERS_DESC,
    VISIT_TIMES_DESC,
    VIRTUAL_HOSTS_DESC,
    REFERRERS_DESC,
    REFERRING_SITES_DESC,
    KEYPHRASES_DESC,
    STATUS_CODES_DESC,
    REMOTE_USER_DESC,
    CACHE_STATUS_DESC,
#ifdef HAVE_GEOLOCATION
    GEO_LOCATION_DESC,
    ASN_DESC,
#endif
    MIME_TYPE_DESC,
    TLS_TYPE_DESC,
  };
  return _(modules[module]);
}

/* Rerender the header window to reflect active module. */
void
update_active_module (WINDOW *header_win, GModule current) {
  GColors *color = get_color (COLOR_ACTIVE_LABEL);
  const char *module = module_to_label (current);
  int col = getmaxx (stdscr);
  char *lbl = xmalloc (snprintf (NULL, 0, T_ACTIVE_PANEL, module) + 1);
  sprintf (lbl, T_ACTIVE_PANEL, module);
  wmove (header_win, 0, 30);
  wattron (header_win, color->attr | COLOR_PAIR (color->pair->idx));
  mvwprintw (header_win, 0, col - strlen (lbl) - 1, "%s", lbl);
  wattroff (header_win, color->attr | COLOR_PAIR (color->pair->idx));
  wrefresh (header_win);
  free (lbl);
}

/* Print out (terminal) an overall field label. e.g., 'Processed Time' */
static void
render_overall_field (WINDOW *win, const char *s, int y, int x, GColors *color) {
  wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
  mvwprintw (win, y, x, "%s", s);
  wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));
}

/* Print out (terminal) an overall field value. e.g., '120 secs' */
static void
render_overall_value (WINDOW *win, const char *s, int y, int x, GColors *color) {
  wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
  mvwprintw (win, y, x, "%s", s);
  wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));
}

/* Convert the number of excluded ips to a string.
 *
 * On success, the number of excluded ips as a string is returned. */
static char *
get_str_excluded_ips (void) {
  return u642str (ht_get_excluded_ips (), 0);
}

/* Convert the number of failed requests to a string.
 *
 * On success, the number of failed requests as a string is returned. */
static char *
get_str_failed_reqs (void) {
  return u642str (ht_get_invalid (), 0);
}

/* Convert the number of processed requests to a string.
 *
 * On success, the number of processed requests as a string is returned. */
static char *
get_str_processed_reqs (void) {
  return u642str (ht_get_processed (), 0);
}

/* Convert the number of valid requests to a string.
 *
 * On success, the number of valid requests as a string is returned. */
static char *
get_str_valid_reqs (void) {
  return u642str (ht_sum_valid (), 0);
}

/* Convert the number of not found requests to a string.
 *
 * On success, the number of not found requests as a string is
 * returned. */
static char *
get_str_notfound_reqs (void) {
  return u642str (ht_get_size_datamap (NOT_FOUND), 0);
}

/* Convert the number of referrers to a string.
 *
 * On success, the number of referrers as a string is returned. */
static char *
get_str_ref_reqs (void) {
  return u642str (ht_get_size_datamap (REFERRERS), 0);
}

/* Convert the number of requests to a string.
 *
 * On success, the number of requests as a string is returned. */
static char *
get_str_reqs (void) {
  return u642str (ht_get_size_datamap (REQUESTS), 0);
}

/* Convert the number of static requests to a string.
 *
 * On success, the number of static requests as a string is returned. */
static char *
get_str_static_reqs (void) {
  return u642str (ht_get_size_datamap (REQUESTS_STATIC), 0);
}

/* Convert the number of unique visitors to a string.
 *
 * On success, the number of unique visitors as a string is returned. */
static char *
get_str_visitors (void) {
  return u642str (ht_get_size_uniqmap (VISITORS), 0);
}

/* Convert the time taken to process the log to a string.
 *
 * On success, the time taken to process the log as a string is
 * returned. */
static char *
get_str_proctime (void) {
  char *s = NULL;
  uint32_t secs = ht_get_processing_time ();
  s = xmalloc (snprintf (NULL, 0, "%us", secs) + 1);
  sprintf (s, "%us", secs);
  return s;
}

/* Get the log file size in a human-readable format.
 *
 * On success, the log file size as a string is returned. */
static char *
get_str_filesize (void) {
  return filesize_str (get_log_sizes ());
}

/* Get the log file path.
 *
 * On success, the log file path as a string is returned. */
static char *
get_str_logfile (void) {
  int col = getmaxx (stdscr), left_padding = 20;
  return get_log_source_str (col - left_padding);
}

/* Get the bandwidth in a human-readable format.
 *
 * On success, the bandwidth as a string is returned. */
static char *
get_str_bandwidth (void) {
  return filesize_str (ht_sum_bw ());
}

/* Get the overall statistics start and end dates.
 *
 * On failure, 1 is returned
 * On success, 0 is returned and an string containing the overall
 * header is returned. */
int
get_start_end_parsing_dates (char **start, char **end, const char *f) {
  uint32_t *dates = NULL;
  uint32_t len = 0;
  const char *sndfmt = "%Y%m%d";
  char s[DATE_LEN];
  char e[DATE_LEN];

  dates = get_sorted_dates (&len);
  sprintf (s, "%u", dates[0]);
  sprintf (e, "%u", dates[len - 1]);

  /* just display the actual dates - no specificity */
  *start = get_visitors_date (s, sndfmt, f);
  *end = get_visitors_date (e, sndfmt, f);
  free (dates);

  return 0;
}

/* Get the overall statistics header (label).
 *
 * On success, an string containing the overall header is returned. */
char *
get_overall_header (GHolder *h) {
  const char *head = T_DASH_HEAD;
  char *hd = NULL, *start = NULL, *end = NULL;

  if (h->idx == 0 || get_start_end_parsing_dates (&start, &end, "%d/%b/%Y"))
    return xstrdup (head);

  hd = xmalloc (snprintf (NULL, 0, "%s (%s - %s)", head, start, end) + 1);
  sprintf (hd, "%s (%s - %s)", head, start, end);
  free (end);
  free (start);

  return hd;
}

/* Print out (terminal dashboard) the overall statistics header. */
static void
render_overall_header (WINDOW *win, GHolder *h) {
  char *hd = get_overall_header (h);
  int col = getmaxx (stdscr);
  draw_header (win, hd, " %s", 0, 0, col, color_panel_header);
  free (hd);
}

/* Render the overall statistics. This will attempt to determine the
 * right X and Y position given the current values. */
static void
render_overall_statistics (WINDOW *win, Field fields[], size_t n) {
  GColors *color = NULL;
  int x_field = 2, x_value;
  size_t i, j, k, max_field = 0, max_value, mod_val, y;

  for (i = 0, k = 0, y = 2; i < n; i++) {
    /* new line every OVERALL_NUM_COLS */
    mod_val = k % OVERALL_NUM_COLS;
    /* reset position & length and increment row */
    if (k > 0 && mod_val == 0) {
      max_field = 0;
      x_field = 2;
      y++;
    }
    /* x pos = max length of field */
    x_field += max_field;
    color = (*fields[i].colorlbl) ();
    render_overall_field (win, fields[i].field, y, x_field, color);

    /* get max length of field in the same column */
    max_field = 0;
    for (j = 0; j < n; j++) {
      size_t len = strlen (fields[j].field);
      if (j % OVERALL_NUM_COLS == mod_val && len > max_field && !fields[j].oneliner)
        max_field = len;
    }
    /* get max length of value in the same column */
    max_value = 0;
    for (j = 0; j < n; j++) {
      size_t len = strlen (fields[j].value);
      if (j % OVERALL_NUM_COLS == mod_val && len > max_value && !fields[j].oneliner)
        max_value = len;
    }
    /* spacers */
    x_value = max_field + x_field + 1;
    max_field += max_value + 2;
    color = (*fields[i].colorval) ();
    render_overall_value (win, fields[i].value, y, x_value, color);
    k += fields[i].oneliner ? OVERALL_NUM_COLS : 1;
  }
}

/* The entry point to render the overall statistics and free its data. */
void
display_general (WINDOW *win, GHolder *h) {
  GColors *(*colorlbl) (void) = color_overall_lbls;
  GColors *(*colorpth) (void) = color_overall_path;
  GColors *(*colorval) (void) = color_overall_vals;
  size_t n, i;

  /* *INDENT-OFF* */
  Field fields[] = {
    {T_REQUESTS        , get_str_processed_reqs () , colorlbl , colorval , 0} ,
    {T_UNIQUE_VISITORS , get_str_visitors ()       , colorlbl , colorval , 0} ,
    {T_UNIQUE_FILES    , get_str_reqs ()           , colorlbl , colorval , 0} ,
    {T_REFERRER        , get_str_ref_reqs ()       , colorlbl , colorval , 0} ,
    {T_VALID           , get_str_valid_reqs ()     , colorlbl , colorval , 0} ,
    {T_GEN_TIME        , get_str_proctime ()       , colorlbl , colorval , 0} ,
    {T_STATIC_FILES    , get_str_static_reqs ()    , colorlbl , colorval , 0} ,
    {T_LOG             , get_str_filesize ()       , colorlbl , colorval , 0} ,
    {T_FAILED          , get_str_failed_reqs ()    , colorlbl , colorval , 0} ,
    {T_EXCLUDE_IP      , get_str_excluded_ips ()   , colorlbl , colorval , 0} ,
    {T_UNIQUE404       , get_str_notfound_reqs ()  , colorlbl , colorval , 0} ,
    {T_BW              , get_str_bandwidth ()      , colorlbl , colorval , 0} ,
    {T_LOG_PATH        , get_str_logfile ()        , colorlbl , colorpth , 1}
  };
  /* *INDENT-ON* */

  werase (win);
  render_overall_header (win, h);
  n = ARRAY_SIZE (fields);
  render_overall_statistics (win, fields, n);
  for (i = 0; i < n; i++) {
    free (fields[i].value);
  }
}

/* Set default string in input field.
 *
 * On success, the newly allocated string is returned. */
char *
set_default_string (WINDOW *win, int pos_y, int pos_x, size_t max_width, const char *str) {
  char *s = xmalloc (max_width + 1), *tmp;
  size_t len = 0;
  size_t size_x = 0, size_y = 0;

  getmaxyx (win, size_y, size_x);
  (void) size_y;
  size_x -= 4;

  /* are we setting a default string */
  if (!str) {
    s[0] = '\0';
    return s;
  }

  len = MIN (max_width, strlen (str));
  memcpy (s, str, len);
  s[len] = '\0';

  /* is the default str length greater than input field? */
  if (strlen (s) > size_x) {
    tmp = xstrdup (&s[0]);
    tmp[size_x] = '\0';
    mvwprintw (win, pos_y, pos_x, "%s", tmp);
    free (tmp);
  } else {
    mvwprintw (win, pos_y, pos_x, "%s", s);
  }

  return s;
}

/* Implement a basic framework to build a field input.
 *
 * On success, the inputted string is returned. */
char *
input_string (WINDOW *win, int pos_y, int pos_x, size_t max_width, const char *str,
              int enable_case, int *toggle_case) {
  char *s = NULL, *tmp;
  size_t i, c, pos = 0, x = 0, quit = 1, size_x = 0, size_y = 0;

  getmaxyx (win, size_y, size_x);
  size_x -= 4;

  s = set_default_string (win, pos_y, pos_x, max_width, str);
  if (str)
    x = pos = 0;

  if (enable_case)
    mvwprintw (win, size_y - 2, 1, " %s", CSENSITIVE);

  wmove (win, pos_y, pos_x + x);
  wrefresh (win);
  curs_set (1);

  while (quit) {
    c = wgetch (stdscr);
    switch (c) {
    case 1:    /* ^a   */
    case 262:  /* HOME */
      pos = x = 0;
      break;
    case 5:
    case 360:  /* END of line */
      if (strlen (s) > size_x) {
        x = size_x;
        pos = strlen (s) - size_x;
      } else {
        pos = 0;
        x = strlen (s);
      }
      break;
    case 7:    /* ^g  */
    case 27:   /* ESC */
      pos = x = 0;
      if (str && *str == '\0')
        s[0] = '\0';
      quit = 0;
      break;
    case 9:    /* TAB   */
      if (!enable_case)
        break;
      *toggle_case = *toggle_case == 0 ? 1 : 0;
      if (*toggle_case)
        mvwprintw (win, size_y - 2, 1, " %s", CISENSITIVE);
      else if (!*toggle_case)
        mvwprintw (win, size_y - 2, 1, " %s", CSENSITIVE);
      break;
    case 21:   /* ^u */
      s[0] = '\0';
      pos = x = 0;
      break;
    case 8:    /* xterm-256color */
    case 127:
    case KEY_BACKSPACE:
      if (pos + x > 0) {
        memmove (&s[(pos + x) - 1], &s[pos + x], (max_width - (pos + x)) + 1);
        if (pos <= 0)
          x--;
        else
          pos--;
      }
      break;
    case KEY_LEFT:
      if (x > 0)
        x--;
      else if (pos > 0)
        pos--;
      break;
    case KEY_RIGHT:
      if ((x + pos) < strlen (s)) {
        if (x < size_x)
          x++;
        else
          pos++;
      }
      break;
    case 0x0a:
    case 0x0d:
    case KEY_ENTER:
      quit = 0;
      break;
    default:
      if (strlen (s) == max_width)
        break;
      if (!isprint ((unsigned char) c))
        break;
      if (strlen (s) == pos) {
        s[pos + x] = c;
        s[pos + x + 1] = '\0';
        waddch (win, c);
      } else {
        memmove (&s[pos + x + 1], &s[pos + x], strlen (&s[pos + x]) + 1);
        s[pos + x] = c;
      }
      if ((x + pos) < max_width) {
        if (x < size_x)
          x++;
        else
          pos++;
      }
    }
    tmp = xstrdup (&s[pos > 0 ? pos : 0]);
    tmp[MIN (strlen (tmp), size_x)] = '\0';
    for (i = strlen (tmp); i < size_x; i++)
      mvwprintw (win, pos_y, pos_x + i, "%s", " ");
    mvwprintw (win, pos_y, pos_x, "%s", tmp);
    free (tmp);
    wmove (win, pos_y, pos_x + x);
    wrefresh (win);
  }
  curs_set (0);

  return s;
}

static const char spin_chars[] = "/-\\|";
#define SPIN_FRAMES_COUNT  (sizeof(spin_chars) / sizeof(spin_chars[0]))

static const char *const goaccess_mini_banner[] = {
  "   ______      ___",
  "  / ____/___  /   | _____________  __________",
  " / / __/ __ \\/ /| |/ ___/ ___/ _ \\/ ___/ ___/",
  "/ /_/ / /_/ / ___ / /__/ /__/  __(__  |__  ) ",
  "\\____/\\____/_/  |_\\___/\\___/\\___/____/____/ ",
  "",
  "The MIT License (MIT) - Copyright (c) 2009-2026 Gerardo Orellana <hello@goaccess.io>",
  NULL
};

/* Helper: format stats string safely */
static void
format_stats (uint64_t processed, int64_t elapsed_sec, char *out, size_t outsz) {
  int64_t rate = (elapsed_sec >= 1) ? (int64_t) (processed / (uint64_t) elapsed_sec) : 0;
  snprintf (out, outsz, "%'13" PRIu64 "   %" PRIi64 "/s", processed, rate);
}

static SpinnerSnapshot
snapshot_state (GSpinner *sp) {
  SpinnerSnapshot snap = { 0 };

  lock_spinner ();

  snap.state = sp->state;
  snap.curses = sp->curses;

  if (sp->processed && *(sp->processed)) {
    snap.processed = **(sp->processed);
  } else {
    snap.processed = 0ULL;
  }

  snap.elapsed_sec = (time (NULL) - sp->start_time);
  snap.filename = (sp->filename && *sp->filename) ? *sp->filename : "processing";

  unlock_spinner ();

  return snap;
}

/* Curses rendering – filename + stats via draw_header, bar in middle, spinner at spin_x */
static void
render_curses (GSpinner *sp, SpinnerSnapshot *snap, const char *stats, int *bounce_pos,
               int *bounce_dir, int bounce_width, int spin_idx) {
  GColors *color = (*sp->color) ();
  char left_buf[256] = { 0 };
  const char *fname = NULL;

  int maxx = getmaxx (sp->win), text_width = 0, bar_start = 0, bar_max = 0, i = 0, fill_start;
  if (maxx < 60)
    maxx = 80;

  /* Prepare left text: filename (basename) + stats */
  fname = basename_only (snap->filename);
  snprintf (left_buf, sizeof (left_buf), "%s  %s", fname, stats);

  /* Draw left text using original draw_header style */
  draw_header (sp->win, left_buf, " %s", sp->y, sp->x, sp->w, sp->color);

  /* Calculate where the bar starts (right after text ends) */
  text_width = strlen (left_buf) + 2; /* + padding */
  bar_start = sp->x + text_width;
  bar_max = maxx - bar_start - 5; /* leave space for spinner */

  if (bar_max < 20) {
    bar_max = 20;
    bar_start = maxx - bar_max - 5;
  }

  /* White bold bouncing blocks */
  fill_start = bar_start + *bounce_pos;
  wattron (sp->win, COLOR_PAIR (color->pair->idx) | A_BOLD);
  for (i = 0; i < bounce_width && fill_start + i < bar_start + bar_max; i++) {
    mvwaddch (sp->win, sp->y, fill_start + i, ACS_BLOCK);
  }
  wattroff (sp->win, COLOR_PAIR (color->pair->idx) | A_BOLD);

  /* Animate bounce */
  *bounce_pos += *bounce_dir * 2;
  if (*bounce_pos >= bar_max - bounce_width)
    *bounce_dir = -1;
  if (*bounce_pos <= 0)
    *bounce_dir = 1;

  /* Spinner at original sp->spin_x position */
  if (!conf.no_progress) {
    wattron (sp->win, COLOR_PAIR (color->pair->idx));
    mvwaddch (sp->win, sp->y, sp->spin_x, spin_chars[spin_idx & 3]);
    wattroff (sp->win, COLOR_PAIR (color->pair->idx));
  }

  wrefresh (sp->win);
}

/* Terminal (colored) rendering – same order */
static void
render_plain (FILE *out, SpinnerSnapshot *snap, const char *stats,
              int *banner_shown, int *bounce_pos, int *bounce_dir, int bounce_width, int spin_idx) {

  int i = 0, barlen = 0, pos = 0;
  const char *fname = basename_only (snap->filename);
  char bar[128] = "";
  char spinner_char;

  if (!*banner_shown) {
    for (i = 0; goaccess_mini_banner[i]; i++) {
      fprintf (out, "\033[1;36m%s\033[0m\n", goaccess_mini_banner[i]);
    }
    fprintf (out, "\n");
    *banner_shown = true;
  }

  /* Build bouncing bar – smooth reverse direction */
  barlen = 40;
  pos = *bounce_pos; // current position

  for (i = 0; i < barlen; i++) {
    if (i >= pos && i < pos + bounce_width) {
      bar[i] = '#';
    } else {
      bar[i] = '-';
    }
  }
  bar[barlen] = '\0';

  spinner_char = spin_chars[spin_idx & 3];

  fprintf (out, "\033[2K\r" "\033[1;37m%s\033[0m " /* filename white */
           "\033[1;34m%s\033[0m " /* stats blue */
           "\033[90m[\033[1;32m%s\033[90m]\033[0m " /* bar green fill */
           "\033[36m%c\033[0m", /* spinner cyan */
           fname, stats, bar, spinner_char);

  fflush (out);

  /* Animate bounce – same logic as curses */
  *bounce_pos += *bounce_dir * 2; // step size 2 for smoother feel

  if (*bounce_pos >= barlen - bounce_width) {
    *bounce_dir = -1;
  }
  if (*bounce_pos <= 0) {
    *bounce_dir = 1;
  }
}

/* Fallback (minimal) – filename → stats → spinner */
static void
render_fallback (FILE *out, SpinnerSnapshot *snap, const char *stats, int spin_idx) {
  const char *fname = basename_only (snap->filename);
  fprintf (out, "\r%s  %s  %c", fname, stats, spin_chars[spin_idx & 3]);
  fflush (out);
}

/* Main spinner loop */
static void
ui_spinner (void *ptr_data) {
  GSpinner *sp = (GSpinner *) ptr_data;

  static int banner_shown = 0;
  static int bounce_pos = 0;
  static int bounce_dir = 1;
  const int bounce_width = 8;
  static int spin_idx = 0;
  struct timespec ts = {.tv_sec = 0,.tv_nsec = SPIN_UPDATE_INTERVAL };

  /* Hide cursor only in interactive terminal */
  int is_interactive = !sp->curses && !conf.no_progress && isatty (fileno (stderr));
  if (is_interactive) {
    fputs ("\033[?25l", stderr);
  }

  time (&sp->start_time);

  while (true) {
    char stats[96] = { 0 };
    SpinnerSnapshot snap = snapshot_state (sp);
    if (snap.state == SPN_END) {
      if (is_interactive) {
        fputs ("\033[?25h\033[2K\n", stderr);
      }
      break;
    }

    /* If no progress is wanted, just sleep and loop */
    if (conf.no_progress) {
      nanosleep (&ts, NULL);
      continue;
    }

    format_stats (snap.processed, snap.elapsed_sec, stats, sizeof (stats));

    spin_idx = (spin_idx + 1) % 4;

    if (snap.curses) {
      render_curses (sp, &snap, stats, &bounce_pos, &bounce_dir, bounce_width, spin_idx);
    } else if (isatty (fileno (stderr))) {
      render_plain (stderr, &snap, stats, &banner_shown, &bounce_pos, &bounce_dir, bounce_width,
                    spin_idx);
    } else {
      render_fallback (stderr, &snap, stats, spin_idx);
    }

    if (nanosleep (&ts, NULL) == -1 && errno != EINTR) {
      FATAL ("nanosleep: %s", strerror (errno));
    }
  }
}

/* Create the processing spinner's thread */
void
ui_spinner_create (GSpinner *spinner) {
  if (conf.no_parsing_spinner)
    return;
  pthread_create (&(spinner->thread), NULL, (void *) &ui_spinner, spinner);
  pthread_detach (spinner->thread);
}

/* Initialize processing spinner data. */
void
set_curses_spinner (GSpinner *spinner) {
  int y, x;
  if (spinner == NULL)
    return;

  getmaxyx (stdscr, y, x);
  spinner->color = color_progress;
  spinner->curses = 1;
  spinner->win = stdscr;
  spinner->x = 0;
  spinner->w = x;
  spinner->spin_x = x - 2;
  spinner->y = y - 1;
}

/* Determine if we need to lock the mutex. */
void
lock_spinner (void) {
  if (parsing_spinner != NULL && parsing_spinner->state == SPN_RUN)
    pthread_mutex_lock (&parsing_spinner->mutex);
}

/* Determine if we need to unlock the mutex. */
void
unlock_spinner (void) {
  if (parsing_spinner != NULL && parsing_spinner->state == SPN_RUN)
    pthread_mutex_unlock (&parsing_spinner->mutex);
}

/* Initialize per-item expanded state for a module.
 * All items start expanded (showing children) by default. */
void
init_item_expanded (GScrollModule *smod, int num_items) {
  free_item_expanded (smod);
  if (num_items <= 0)
    return;
  smod->item_expanded = xcalloc (num_items, sizeof (uint8_t));
  smod->item_expanded_size = num_items;
  memset (smod->item_expanded, 1, num_items);
}

/* Free per-item expanded state array. */
void
free_item_expanded (GScrollModule *smod) {
  if (smod->item_expanded) {
    free (smod->item_expanded);
    smod->item_expanded = NULL;
  }
  smod->item_expanded_size = 0;
}

/* Reset per-item expanded state (frees and nullifies). */
void
reset_item_expanded (GScrollModule *smod) {
  free_item_expanded (smod);
}

/* Allocate memory for a spinner instance and initialize its data.
 *
 * On success, the newly allocated GSpinner is returned. */
GSpinner *
new_gspinner (void) {
  GSpinner *spinner;
  spinner = xcalloc (1, sizeof (GSpinner));
  spinner->label = "Parsing...";
  spinner->state = SPN_RUN;
  spinner->curses = 0;

  if (pthread_mutex_init (&(spinner->mutex), NULL))
    FATAL ("Failed init thread mutex");

  return spinner;
}
