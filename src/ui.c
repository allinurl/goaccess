/**
 * ui.c -- various curses interfaces
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2023 Gerardo Orellana <hello @ goaccess.io>
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
#define _BSD_SOURCE     /* include stuff from 4.3 BSD */
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
static GOutput outputting[] = {
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
GOutput *
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
close_win (WINDOW * w) {
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
set_wbkgd (WINDOW * main_win, WINDOW * header_win) {
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
init_windows (WINDOW ** header_win, WINDOW ** main_win) {
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
draw_header (WINDOW * win, const char *s, const char *fmt, int y, int x, int w,
             GColors * (*func) (void)) {
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
term_size (WINDOW * main_win, int *main_win_height) {
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
  static const char *modules[] = {
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
  static const char *modules[] = {
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
  static const char *modules[] = {
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
update_active_module (WINDOW * header_win, GModule current) {
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
render_overall_field (WINDOW * win, const char *s, int y, int x, GColors * color) {
  wattron (win, color->attr | COLOR_PAIR (color->pair->idx));
  mvwprintw (win, y, x, "%s", s);
  wattroff (win, color->attr | COLOR_PAIR (color->pair->idx));
}

/* Print out (terminal) an overall field value. e.g., '120 secs' */
static void
render_overall_value (WINDOW * win, const char *s, int y, int x, GColors * color) {
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

/* Get the log file size in a human readable format.
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

/* Get the bandwidth in a human readable format.
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
get_overall_header (GHolder * h) {
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
render_overall_header (WINDOW * win, GHolder * h) {
  char *hd = get_overall_header (h);
  int col = getmaxx (stdscr);

  draw_header (win, hd, " %s", 0, 0, col, color_panel_header);
  free (hd);
}

/* Render the overall statistics. This will attempt to determine the
 * right X and Y position given the current values. */
static void
render_overall_statistics (WINDOW * win, Field fields[], size_t n) {
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
display_general (WINDOW * win, GHolder * h) {
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

static char *
set_default_string (WINDOW * win, int pos_y, int pos_x, size_t max_width, const char *str) {
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
input_string (WINDOW * win, int pos_y, int pos_x, size_t max_width, const char *str,
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
      if (!isprint (c))
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

/* Add the given user agent value into our array of GAgents.
 *
 * On error, 1 is returned.
 * On success, the user agent is added to the array and 0 is returned. */
static int
set_agents (void *val, void *user_data) {
  GAgents *agents = user_data;
  GAgentItem *tmp = NULL;
  char *agent = NULL;
  int newlen = 0, i;

  if (!(agent = ht_get_host_agent_val (*(uint32_t *) val)))
    return 1;

  if (agents->size - 1 == agents->idx) {
    newlen = agents->size + 4;
    if (!(tmp = realloc (agents->items, newlen * sizeof (GAgentItem))))
      FATAL ("Unable to realloc agents");

    agents->items = tmp;
    agents->size = newlen;
  }

  for (i = 0; i < agents->idx; ++i) {
    if (strcmp (agent, agents->items[i].agent) == 0) {
      free (agent);
      return 0;
    }
  }
  agents->items[agents->idx++].agent = agent;

  return 0;
}

/* Iterate over the list of agents */
GAgents *
load_host_agents (const char *addr) {
  GAgents *agents = NULL;
  GSLList *keys = NULL, *list = NULL;
  void *data = NULL;
  uint32_t items = 4, key = djb2 ((const unsigned char *) addr);

  keys = ht_get_keymap_list_from_key (HOSTS, key);
  if (!keys)
    return NULL;

  agents = new_gagents (items);

  /* *INDENT-OFF* */
  GSLIST_FOREACH (keys, data, {
    if ((list = ht_get_host_agent_list (HOSTS, (*(uint32_t *) data)))) {
      list_foreach (list, set_agents, agents);
      list_remove_nodes (list);
    }
  });
  /* *INDENT-ON* */
  list_remove_nodes (keys);

  return agents;
}

/* Fill the given terminal dashboard menu with user agent data.
 *
 * On error, the 1 is returned.
 * On success, 0 is returned. */
static int
fill_host_agents_gmenu (GMenu * menu, GAgents * agents) {
  int i;

  if (agents == NULL)
    return 1;

  menu->items = xcalloc (agents->idx, sizeof (GItem));
  for (i = 0; i < agents->idx; ++i) {
    menu->items[i].name = xstrdup (agents->items[i].agent);
    menu->items[i].checked = 0;
    menu->size++;
  }

  return 0;
}

/* Render a list of agents if available for the selected host/IP. */
void
load_agent_list (WINDOW * main_win, char *addr) {
  GMenu *menu;
  GAgents *agents = NULL;
  WINDOW *win;

  char buf[256];
  int c, quit = 1, i;
  int y, x, list_h, list_w, menu_w, menu_h;

  if (!conf.list_agents)
    return;

  getmaxyx (stdscr, y, x);
  list_h = y / 2;       /* list window - height */
  list_w = x - 4;       /* list window - width */
  menu_h = list_h - AGENTS_MENU_Y - 1;  /* menu window - height */
  menu_w = list_w - AGENTS_MENU_X - AGENTS_MENU_X;      /* menu window - width */

  win = newwin (list_h, list_w, (y - list_h) / 2, (x - list_w) / 2);
  keypad (win, TRUE);
  wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

  /* create a new instance of GMenu and make it selectable */
  menu = new_gmenu (win, menu_h, menu_w, AGENTS_MENU_Y, AGENTS_MENU_X);
  if (!(agents = load_host_agents (addr)))
    goto out;
  if (fill_host_agents_gmenu (menu, agents) != 0)
    goto out;

  post_gmenu (menu);
  snprintf (buf, sizeof buf, AGENTSDLG_HEAD, addr);
  draw_header (win, buf, " %s", 1, 1, list_w - 2, color_panel_header);
  mvwprintw (win, 2, 2, AGENTSDLG_DESC);
  wrefresh (win);

  while (quit) {
    c = wgetch (stdscr);
    switch (c) {
    case KEY_DOWN:
      gmenu_driver (menu, REQ_DOWN);
      break;
    case KEY_UP:
      gmenu_driver (menu, REQ_UP);
      break;
    case KEY_RESIZE:
    case 'q':
      quit = 0;
      break;
    }
    wrefresh (win);
  }

  touchwin (main_win);
  close_win (win);
  win = NULL;
  wrefresh (main_win);

out:

  /* clean stuff up */
  for (i = 0; i < menu->size; ++i)
    free (menu->items[i].name);
  if (menu->items)
    free (menu->items);
  free (menu);
  free_agents_array (agents);
  close_win (win);
}

/* Render the processing spinner. This runs within its own thread. */
static void
ui_spinner (void *ptr_data) {
  GSpinner *sp = (GSpinner *) ptr_data;
  GColors *color = NULL;

  static char const spin_chars[] = "/-\\|";
  char buf[SPIN_LBL];
  const char *fn = NULL;
  int i = 0;
  long long tdiff = 0, psec = 0;
  time_t begin;
  struct timespec ts = {.tv_sec = 0,.tv_nsec = SPIN_UPDATE_INTERVAL };

  if (sp->curses)
    color = (*sp->color) ();

  time (&begin);
  while (1) {
    pthread_mutex_lock (&sp->mutex);
    if (sp->state == SPN_END) {
      if (!sp->curses && !conf.no_progress)
        fprintf (stderr, "\n");

      pthread_mutex_unlock (&sp->mutex);
      return;
    }

    setlocale (LC_NUMERIC, "");
    if (conf.no_progress) {
      snprintf (buf, sizeof buf, SPIN_FMT, sp->label);
    } else {
      fn = *sp->filename ? *sp->filename : "restoring";
      tdiff = (long long) (time (NULL) - begin);
      psec = tdiff >= 1 ? **(sp->processed) / tdiff : 0;
      snprintf (buf, sizeof buf, SPIN_FMTM, sp->label, fn, **(sp->processed), psec);
    }
    setlocale (LC_NUMERIC, "POSIX");

    if (sp->curses) {
      /* CURSES */
      draw_header (sp->win, buf, " %s", sp->y, sp->x, sp->w, sp->color);
      /* caret */
      wattron (sp->win, COLOR_PAIR (color->pair->idx));
      mvwaddch (sp->win, sp->y, sp->spin_x, spin_chars[i++ & 3]);
      wattroff (sp->win, COLOR_PAIR (color->pair->idx));
      wrefresh (sp->win);
    } else if (!conf.no_progress) {
      /* STDOUT */
      fprintf (stderr, " %s\r", buf);
    }

    pthread_mutex_unlock (&sp->mutex);
    if (nanosleep (&ts, NULL) == -1 && errno != EINTR)
      FATAL ("nanosleep: %s", strerror (errno));
  }
}

/* Create the processing spinner's thread */
void
ui_spinner_create (GSpinner * spinner) {
  if (conf.no_parsing_spinner)
    return;

  pthread_create (&(spinner->thread), NULL, (void *) &ui_spinner, spinner);
  pthread_detach (spinner->thread);
}

/* Initialize processing spinner data. */
void
set_curses_spinner (GSpinner * spinner) {
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
  //if (conf.load_from_disk)
  //  conf.no_progress = 1;

  if (pthread_mutex_init (&(spinner->mutex), NULL))
    FATAL ("Failed init thread mutex");

  return spinner;
}

/* A wrapper call to clear the status bar on the config dialog
 * (terminal output). */
static void
clear_confdlg_status_bar (WINDOW * win, int y, int x, int w) {
  draw_header (win, "", "%s", y, x, w + 1, color_default);
}

/* Escape a date format string.
 *
 * If no conf.date_format is given, NULL is returned.
 * On success, the newly escaped allocated string is returned. */
static char *
get_input_date_format (void) {
  char *date_format = NULL;

  if (conf.date_format)
    date_format = escape_str (conf.date_format);
  return date_format;
}

/* Escape a time format string.
 *
 * If no conf.time_format is given, NULL is returned.
 * On success, the newly escaped allocated string is returned. */
static char *
get_input_time_format (void) {
  char *time_format = NULL;

  if (conf.time_format)
    time_format = escape_str (conf.time_format);
  return time_format;
}

/* Escape a log format string.
 *
 * If no conf.log_format is given, NULL is returned.
 * On success, the newly escaped allocated string is returned. */
static char *
get_input_log_format (void) {
  char *log_format = NULL;

  if (conf.log_format)
    log_format = escape_str (conf.log_format);
  return log_format;
}

static void
draw_formats (WINDOW * win, int w2) {
  char *date_format = NULL, *log_format = NULL, *time_format = NULL;

  draw_header (win, CONFDLG_HEAD, " %s", 1, 1, w2, color_panel_header);
  mvwprintw (win, 2, 2, CONFDLG_KEY_HINTS);

  /* set log format from config file if available */
  draw_header (win, CONFDLG_LOG_FORMAT, " %s", 11, 1, w2, color_panel_header);
  if ((log_format = get_input_log_format ())) {
    mvwprintw (win, 12, 2, "%.*s", CONF_MENU_W, log_format);

    free (log_format);
  }

  /* set log format from config file if available */
  draw_header (win, CONFDLG_DATE_FORMAT, " %s", 14, 1, w2, color_panel_header);
  if ((date_format = get_input_date_format ())) {
    mvwprintw (win, 15, 2, "%.*s", CONF_MENU_W, date_format);

    free (date_format);
  }

  /* set log format from config file if available */
  draw_header (win, CONFDLG_TIME_FORMAT, " %s", 17, 1, w2, color_panel_header);
  if ((time_format = get_input_time_format ())) {
    mvwprintw (win, 18, 2, "%.*s", CONF_MENU_W, time_format);

    free (time_format);
  }
}

static const char *
set_formats (char *date_format, char *log_format, char *time_format) {
  /* display status bar error messages */
  if (!time_format && !conf.time_format)
    return ERR_FORMAT_NO_TIME_FMT_DLG;
  if (!date_format && !conf.date_format)
    return ERR_FORMAT_NO_DATE_FMT_DLG;
  if (!log_format && !conf.log_format)
    return ERR_FORMAT_NO_LOG_FMT_DLG;

  if (time_format) {
    free (conf.time_format);
    conf.time_format = unescape_str (time_format);
  }

  if (date_format) {
    free (conf.date_format);
    conf.date_format = unescape_str (date_format);
  }

  if (log_format) {
    free (conf.log_format);
    conf.log_format = unescape_str (log_format);
  }

  if (is_json_log_format (conf.log_format))
    conf.is_json_log_format = 1;

  set_spec_date_format ();

  return NULL;
}

/* Render the help dialog. */
static void
load_confdlg_error (WINDOW * parent_win, char **errors, int nerrors) {
  int c, quit = 1, i = 0;
  int y, x, h = ERR_WIN_HEIGHT, w = ERR_WIN_WIDTH;
  WINDOW *win;
  GMenu *menu;

  getmaxyx (stdscr, y, x);

  win = newwin (h, w, (y - h) / 2, (x - w) / 2);
  keypad (win, TRUE);
  wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

  /* create a new instance of GMenu and make it selectable */
  menu = new_gmenu (win, ERR_MENU_HEIGHT, ERR_MENU_WIDTH, ERR_MENU_Y, ERR_MENU_X);
  menu->size = nerrors;

  /* add items to GMenu */
  menu->items = (GItem *) xcalloc (nerrors, sizeof (GItem));
  for (i = 0; i < nerrors; ++i) {
    menu->items[i].name = alloc_string (errors[i]);
    menu->items[i].checked = 0;
    free (errors[i]);
  }
  free (errors);
  post_gmenu (menu);

  draw_header (win, ERR_FORMAT_HEADER, " %s", 1, 1, w - 2, color_error);
  mvwprintw (win, 2, 2, CONFDLG_DESC);

  wrefresh (win);
  while (quit) {
    c = wgetch (stdscr);
    switch (c) {
    case KEY_DOWN:
      gmenu_driver (menu, REQ_DOWN);
      break;
    case KEY_UP:
      gmenu_driver (menu, REQ_UP);
      break;
    case KEY_RESIZE:
    case 'q':
      quit = 0;
      break;
    }
    wrefresh (win);
  }
  /* clean stuff up */
  for (i = 0; i < nerrors; ++i)
    free (menu->items[i].name);
  free (menu->items);
  free (menu);

  touchwin (parent_win);
  close_win (win);
  wrefresh (parent_win);
}

/* Render the config log date/format dialog.
 *
 * On error, or if the selected format is invalid, 1 is returned.
 * On success, 0 is returned. */
int
render_confdlg (Logs * logs, GSpinner * spinner) {
  GMenu *menu;
  WINDOW *win;

  const char *log_err = NULL;
  char *date_format = NULL, *log_format = NULL, *time_format = NULL;
  char *cstm_log, *cstm_date, *cstm_time;
  int c, quit = 1, invalid = 1, y, x, h = CONF_WIN_H, w = CONF_WIN_W;
  int w2 = w - 2;
  size_t i, n, sel;

  /* conf dialog menu options */
  const char *choices[] = {
    "NCSA Combined Log Format",
    "NCSA Combined Log Format with Virtual Host",
    "Common Log Format (CLF)",
    "Common Log Format (CLF) with Virtual Host",
    "W3C",
    "CloudFront (Download Distribution)",
    "Google Cloud Storage",
    "AWS Elastic Load Balancing (HTTP/S)",
    "Squid Native Format",
    "AWS Simple Storage Service (S3)",
    "CADDY JSON Structured",
    "AWS Application Load Balancer",
  };
  n = ARRAY_SIZE (choices);
  getmaxyx (stdscr, y, x);

  win = newwin (h, w, (y - h) / 2, (x - w) / 2);
  keypad (win, TRUE);
  wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

  /* create a new instance of GMenu and make it selectable */
  menu = new_gmenu (win, CONF_MENU_H, CONF_MENU_W, CONF_MENU_Y, CONF_MENU_X);
  menu->size = n;
  menu->selectable = 1;

  /* add items to GMenu */
  menu->items = (GItem *) xcalloc (n, sizeof (GItem));
  for (i = 0; i < n; ++i) {
    menu->items[i].name = alloc_string (choices[i]);
    sel = get_selected_format_idx ();
    menu->items[i].checked = sel == i ? 1 : 0;
  }
  post_gmenu (menu);
  draw_formats (win, w2);

  wrefresh (win);
  while (quit) {
    c = wgetch (stdscr);
    switch (c) {
    case KEY_DOWN:
      gmenu_driver (menu, REQ_DOWN);
      clear_confdlg_status_bar (win, 3, 2, CONF_MENU_W);
      break;
    case KEY_UP:
      gmenu_driver (menu, REQ_UP);
      clear_confdlg_status_bar (win, 3, 2, CONF_MENU_W);
      break;
    case 32:   /* space */
      gmenu_driver (menu, REQ_SEL);
      clear_confdlg_status_bar (win, 12, 1, CONF_MENU_W);
      clear_confdlg_status_bar (win, 15, 1, CONF_MENU_W);
      clear_confdlg_status_bar (win, 18, 1, CONF_MENU_W);

      if (time_format)
        free (time_format);
      if (date_format)
        free (date_format);
      if (log_format)
        free (log_format);

      for (i = 0; i < n; ++i) {
        if (menu->items[i].checked != 1)
          continue;

        date_format = get_selected_date_str (i);
        log_format = get_selected_format_str (i);
        time_format = get_selected_time_str (i);

        free (set_default_string (win, 12, 2, CONF_MENU_W, log_format));
        free (set_default_string (win, 15, 2, CONF_MENU_W, date_format));
        free (set_default_string (win, 18, 2, CONF_MENU_W, time_format));
        break;
      }
      break;
    case 99:   /* c */
      /* clear top status bar */
      clear_confdlg_status_bar (win, 3, 2, CONF_MENU_W);
      wmove (win, 12, 2);

      /* get input string */
      if (!log_format)
        log_format = get_input_log_format ();

      cstm_log =
        input_string (win, 12, 2, log_format ? strlen (log_format) : CONF_MAX_LEN_DLG,
                      log_format, 0, 0);
      if (cstm_log != NULL && *cstm_log != '\0') {
        if (log_format)
          free (log_format);

        log_format = alloc_string (cstm_log);
        free (cstm_log);
      }
      /* did not set an input string */
      else {
        if (cstm_log)
          free (cstm_log);
        if (log_format) {
          free (log_format);
          log_format = NULL;
        }
      }
      break;
    case 100:  /* d */
      /* clear top status bar */
      clear_confdlg_status_bar (win, 3, 2, CONF_MENU_W);
      wmove (win, 15, 0);

      /* get input string */
      if (!date_format)
        date_format = get_input_date_format ();

      cstm_date = input_string (win, 15, 2, 14, date_format, 0, 0);
      if (cstm_date != NULL && *cstm_date != '\0') {
        if (date_format)
          free (date_format);

        date_format = alloc_string (cstm_date);
        free (cstm_date);
      }
      /* did not set an input string */
      else {
        if (cstm_date)
          free (cstm_date);
        if (date_format) {
          free (date_format);
          date_format = NULL;
        }
      }
      break;
    case 116:  /* t */
      /* clear top status bar */
      clear_confdlg_status_bar (win, 3, 2, CONF_MENU_W);
      wmove (win, 15, 0);

      /* get input string */
      if (!time_format)
        time_format = get_input_time_format ();

      cstm_time = input_string (win, 18, 2, 14, time_format, 0, 0);
      if (cstm_time != NULL && *cstm_time != '\0') {
        if (time_format)
          free (time_format);

        time_format = alloc_string (cstm_time);
        free (cstm_time);
      }
      /* did not set an input string */
      else {
        if (cstm_time)
          free (cstm_time);
        if (time_format) {
          free (time_format);
          time_format = NULL;
        }
      }
      break;
    case 274:  /* F10 */
    case 0x0a:
    case 0x0d:
    case KEY_ENTER:
      if ((log_err = set_formats (date_format, log_format, time_format)))
        draw_header (win, log_err, " %s", 3, 2, CONF_MENU_W, color_error);

      if (!log_err) {
        char **errors = NULL;
        int nerrors = 0;

        /* test log against selected settings */
        if ((errors = test_format (logs, &nerrors))) {
          invalid = 1;
          load_confdlg_error (win, errors, nerrors);
        }
        /* valid data, reset glog & start parsing */
        else {
          reset_struct (logs);
          /* start spinner thread */
          spinner->win = win;
          spinner->y = 3;
          spinner->x = 2;
          spinner->spin_x = CONF_MENU_W;
          spinner->w = CONF_MENU_W;
          spinner->color = color_progress;
          ui_spinner_create (spinner);

          invalid = 0;
          quit = 0;
        }
      }
      break;
    case KEY_RESIZE:
    case 'q':
      quit = 0;
      break;
    }

    pthread_mutex_lock (&spinner->mutex);
    wrefresh (win);
    pthread_mutex_unlock (&spinner->mutex);
  }

  free (time_format);
  free (date_format);
  free (log_format);

  /* clean stuff up */
  for (i = 0; i < n; ++i)
    free (menu->items[i].name);
  free (menu->items);
  free (menu);

  return invalid ? 1 : 0;
}

/* Given the name of the selected scheme, set it under our config
 * options. */
static void
scheme_chosen (const char *name) {
  int force = 0;

  free_color_lists ();
  if (strcmp ("Green", name) == 0) {
    conf.color_scheme = STD_GREEN;
    force = 1;
  } else if (strcmp ("Monochrome", name) == 0) {
    conf.color_scheme = MONOCHROME;
    force = 1;
  } else if (strcmp ("Monokai", name) == 0) {
    conf.color_scheme = MONOKAI;
    force = 1;
  } else if (strcmp ("Custom Scheme", name) == 0) {
    force = 0;
  }
  init_colors (force);
}

static const char **
get_color_schemes (size_t *size) {
  const char *choices[] = {
    "Monokai",
    "Monochrome",
    "Green",
    "Custom Scheme"
  };
  int i, j, n = ARRAY_SIZE (choices);
  const char **opts = xmalloc (sizeof (char *) * n);

  for (i = 0, j = 0; i < n; ++i) {
    if (!conf.color_idx && !strcmp ("Custom Scheme", choices[i]))
      continue;
    if (COLORS < 256 && !strcmp ("Monokai", choices[i]))
      continue;
    opts[j++] = choices[i];
  }
  *size = j;

  return opts;
}

/* Render the schemes dialog. */
void
load_schemes_win (WINDOW * main_win) {
  GMenu *menu;
  WINDOW *win;
  int c, quit = 1;
  size_t i, n = 0;
  int y, x, h = SCHEME_WIN_H, w = SCHEME_WIN_W;
  int w2 = w - 2;
  const char **choices = get_color_schemes (&n);

  getmaxyx (stdscr, y, x);

  win = newwin (h, w, (y - h) / 2, (x - w) / 2);
  keypad (win, TRUE);
  wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

  /* create a new instance of GMenu and make it selectable */
  menu = new_gmenu (win, SCHEME_MENU_H, SCHEME_MENU_W, SCHEME_MENU_Y, SCHEME_MENU_X);
  /* remove custom color option if no custom scheme used */
  menu->size = n;

  /* add items to GMenu */
  menu->items = (GItem *) xcalloc (n, sizeof (GItem));
  for (i = 0; i < n; ++i) {
    menu->items[i].name = alloc_string (choices[i]);
    menu->items[i].checked = 0;
  }
  post_gmenu (menu);

  draw_header (win, SCHEMEDLG_HEAD, " %s", 1, 1, w2, color_panel_header);
  mvwprintw (win, 2, 2, SCHEMEDLG_DESC);

  wrefresh (win);
  while (quit) {
    c = wgetch (stdscr);
    switch (c) {
    case KEY_DOWN:
      gmenu_driver (menu, REQ_DOWN);
      break;
    case KEY_UP:
      gmenu_driver (menu, REQ_UP);
      break;
    case 32:
    case 0x0a:
    case 0x0d:
    case KEY_ENTER:
      gmenu_driver (menu, REQ_SEL);
      for (i = 0; i < n; ++i) {
        if (menu->items[i].checked != 1)
          continue;
        scheme_chosen (choices[i]);
        break;
      }
      quit = 0;
      break;
    case KEY_RESIZE:
    case 'q':
      quit = 0;
      break;
    }
    wrefresh (win);
  }
  /* clean stuff up */
  for (i = 0; i < n; ++i)
    free (menu->items[i].name);
  free (menu->items);
  free (menu);
  free (choices);

  touchwin (main_win);
  close_win (win);
  wrefresh (main_win);
}

/* Render the sort dialog. */
void
load_sort_win (WINDOW * main_win, GModule module, GSort * sort) {
  GMenu *menu;
  WINDOW *win;
  GSortField opts[SORT_MAX_OPTS];

  int c, quit = 1;
  int i = 0, k, n = 0;
  int y, x, h = SORT_WIN_H, w = SORT_WIN_W;
  int w2 = w - 2;

  getmaxyx (stdscr, y, x);

  /* determine amount of sort choices */
  for (i = 0, k = 0; -1 != sort_choices[module][i]; i++) {
    GSortField field = sort_choices[module][i];
    if (SORT_BY_CUMTS == field && !conf.serve_usecs)
      continue;
    else if (SORT_BY_MAXTS == field && !conf.serve_usecs)
      continue;
    else if (SORT_BY_AVGTS == field && !conf.serve_usecs)
      continue;
    else if (SORT_BY_BW == field && !conf.bandwidth)
      continue;
    else if (SORT_BY_PROT == field && !conf.append_protocol)
      continue;
    else if (SORT_BY_MTHD == field && !conf.append_method)
      continue;
    opts[k++] = field;
    n++;
  }

  win = newwin (h, w, (y - h) / 2, (x - w) / 2);
  keypad (win, TRUE);
  wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

  /* create a new instance of GMenu and make it selectable */
  menu = new_gmenu (win, SORT_MENU_H, SORT_MENU_W, SORT_MENU_Y, SORT_MENU_X);
  menu->size = n;
  menu->selectable = 1;

  /* add items to GMenu */
  menu->items = (GItem *) xcalloc (n, sizeof (GItem));

  /* set choices, checked option and index */
  for (i = 0; i < n; ++i) {
    GSortField field = sort_choices[module][opts[i]];
    if (SORT_BY_HITS == field) {
      menu->items[i].name = alloc_string (MTRC_HITS_LBL);
      if (sort->field == SORT_BY_HITS) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    } else if (SORT_BY_VISITORS == field) {
      menu->items[i].name = alloc_string (MTRC_VISITORS_LBL);
      if (sort->field == SORT_BY_VISITORS) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    } else if (SORT_BY_DATA == field) {
      menu->items[i].name = alloc_string (MTRC_DATA_LBL);
      if (sort->field == SORT_BY_DATA) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    } else if (SORT_BY_BW == field) {
      menu->items[i].name = alloc_string (MTRC_BW_LBL);
      if (sort->field == SORT_BY_BW) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    } else if (SORT_BY_AVGTS == field) {
      menu->items[i].name = alloc_string (MTRC_AVGTS_LBL);
      if (sort->field == SORT_BY_AVGTS) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    } else if (SORT_BY_CUMTS == field) {
      menu->items[i].name = alloc_string (MTRC_CUMTS_LBL);
      if (sort->field == SORT_BY_CUMTS) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    } else if (SORT_BY_MAXTS == field) {
      menu->items[i].name = alloc_string (MTRC_MAXTS_LBL);
      if (sort->field == SORT_BY_MAXTS) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    } else if (SORT_BY_PROT == field) {
      menu->items[i].name = alloc_string (MTRC_PROTOCOLS_LBL);
      if (sort->field == SORT_BY_PROT) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    } else if (SORT_BY_MTHD == field) {
      menu->items[i].name = alloc_string (MTRC_METHODS_LBL);
      if (sort->field == SORT_BY_MTHD) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    }
  }
  post_gmenu (menu);

  draw_header (win, SORTDLG_HEAD, " %s", 1, 1, w2, color_panel_header);
  mvwprintw (win, 2, 2, SORTDLG_DESC);

  if (sort->sort == SORT_ASC)
    mvwprintw (win, SORT_WIN_H - 2, 1, " %s", SORT_ASC_SEL);
  else
    mvwprintw (win, SORT_WIN_H - 2, 1, " %s", SORT_DESC_SEL);

  wrefresh (win);
  while (quit) {
    c = wgetch (stdscr);
    switch (c) {
    case KEY_DOWN:
      gmenu_driver (menu, REQ_DOWN);
      break;
    case KEY_UP:
      gmenu_driver (menu, REQ_UP);
      break;
    case 9:    /* TAB */
      if (sort->sort == SORT_ASC) {
        /* ascending */
        sort->sort = SORT_DESC;
        mvwprintw (win, SORT_WIN_H - 2, 1, " %s", SORT_DESC_SEL);
      } else {
        /* descending */
        sort->sort = SORT_ASC;
        mvwprintw (win, SORT_WIN_H - 2, 1, " %s", SORT_ASC_SEL);
      }
      break;
    case 32:
    case 0x0a:
    case 0x0d:
    case KEY_ENTER:
      gmenu_driver (menu, REQ_SEL);
      for (i = 0; i < n; ++i) {
        if (menu->items[i].checked != 1)
          continue;
        if (strcmp ("Hits", menu->items[i].name) == 0)
          sort->field = SORT_BY_HITS;
        else if (strcmp ("Visitors", menu->items[i].name) == 0)
          sort->field = SORT_BY_VISITORS;
        else if (strcmp ("Data", menu->items[i].name) == 0)
          sort->field = SORT_BY_DATA;
        else if (strcmp ("Tx. Amount", menu->items[i].name) == 0)
          sort->field = SORT_BY_BW;
        else if (strcmp ("Avg. T.S.", menu->items[i].name) == 0)
          sort->field = SORT_BY_AVGTS;
        else if (strcmp ("Cum. T.S.", menu->items[i].name) == 0)
          sort->field = SORT_BY_CUMTS;
        else if (strcmp ("Max. T.S.", menu->items[i].name) == 0)
          sort->field = SORT_BY_MAXTS;
        else if (strcmp ("Protocol", menu->items[i].name) == 0)
          sort->field = SORT_BY_PROT;
        else if (strcmp ("Method", menu->items[i].name) == 0)
          sort->field = SORT_BY_MTHD;
        quit = 0;
        break;
      }
      break;
    case KEY_RESIZE:
    case 'q':
      quit = 0;
      break;
    }
    wrefresh (win);
  }

  /* clean stuff up */
  for (i = 0; i < n; ++i)
    free (menu->items[i].name);
  free (menu->items);
  free (menu);

  touchwin (main_win);
  close_win (win);
  wrefresh (main_win);
}

/* Help menu data (F1/h). */
static const char *help_main[] = {
  "Copyright (C) 2009-2021 by Gerardo Orellana",
  "https://goaccess.io - <hello@goaccess.io>",
  "Released under the MIT License.",
  "",
  "See `man` page for more details",
  "",
  "GoAccess is an open source real-time web log analyzer and",
  "interactive viewer that runs in a terminal in *nix systems.",
  "It provides fast and valuable HTTP statistics for system",
  "administrators that require a visual server report on the",
  "fly.",
  "",
  "The data collected based on the parsing of the log is",
  "divided into different modules. Modules are automatically",
  "generated and presented to the user.",
  "",
  "The main dashboard displays general statistics, top",
  "visitors, requests, browsers, operating systems,",
  "hosts, etc.",
  "",
  "The user can make use of the following keys:",
  " ^F1^  or ^h^    Main help",
  " ^F5^            Redraw [main window]",
  " ^q^             Quit the program, current window or module",
  " ^o^ or ^ENTER^  Expand selected module",
  " ^[Shift]0-9^    Set selected module to active",
  " ^Up^ arrow      Scroll up main dashboard",
  " ^Down^ arrow    Scroll down main dashboard",
  " ^j^             Scroll down within expanded module",
  " ^k^             Scroll up within expanded module",
  " ^c^             Set or change scheme color",
  " ^CTRL^ + ^f^    Scroll forward one screen within",
  "                 active module",
  " ^CTRL^ + ^b^    Scroll backward one screen within",
  "                 active module",
  " ^TAB^           Iterate modules (forward)",
  " ^SHIFT^ + ^TAB^ Iterate modules (backward)",
  " ^s^             Sort options for current module",
  " ^/^             Search across all modules",
  " ^n^             Find position of the next occurrence",
  " ^g^             Move to the first item or top of screen",
  " ^G^             Move to the last item or bottom of screen",
  "",
  "Examples can be found by running `man goaccess`.",
  "",
  "If you believe you have found a bug, please drop me",
  "an email with details.",
  "",
  "Feedback? Just shoot me an email to:",
  "hello@goaccess.io",
};

/* Render the help dialog. */
void
load_help_popup (WINDOW * main_win) {
  int c, quit = 1;
  size_t i, n;
  int y, x, h = HELP_WIN_HEIGHT, w = HELP_WIN_WIDTH;
  int w2 = w - 2;
  WINDOW *win;
  GMenu *menu;

  n = ARRAY_SIZE (help_main);
  getmaxyx (stdscr, y, x);

  win = newwin (h, w, (y - h) / 2, (x - w) / 2);
  keypad (win, TRUE);
  wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

  /* create a new instance of GMenu and make it selectable */
  menu = new_gmenu (win, HELP_MENU_HEIGHT, HELP_MENU_WIDTH, HELP_MENU_Y, HELP_MENU_X);
  menu->size = n;

  /* add items to GMenu */
  menu->items = (GItem *) xcalloc (n, sizeof (GItem));
  for (i = 0; i < n; ++i) {
    menu->items[i].name = alloc_string (help_main[i]);
    menu->items[i].checked = 0;
  }
  post_gmenu (menu);

  draw_header (win, HELPDLG_HEAD, " %s", 1, 1, w2, color_panel_header);
  mvwprintw (win, 2, 2, HELPDLG_DESC);

  wrefresh (win);
  while (quit) {
    c = wgetch (stdscr);
    switch (c) {
    case KEY_DOWN:
      gmenu_driver (menu, REQ_DOWN);
      break;
    case KEY_UP:
      gmenu_driver (menu, REQ_UP);
      break;
    case KEY_RESIZE:
    case 'q':
      quit = 0;
      break;
    }
    wrefresh (win);
  }
  /* clean stuff up */
  for (i = 0; i < n; ++i)
    free (menu->items[i].name);
  free (menu->items);
  free (menu);

  touchwin (main_win);
  close_win (win);
  wrefresh (main_win);
}
