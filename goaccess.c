/**
 * goaccess.c -- main log analyzer
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

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS    64

#include <assert.h>
#include <ctype.h>
#include <errno.h>

#include <locale.h>

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabinet.h"
#else
#include "glibht.h"
#endif

#ifdef HAVE_LIBGEOIP
#include "geolocation.h"
#endif

#include "commons.h"
#include "csv.h"
#include "error.h"
#include "gdashboard.h"
#include "gdns.h"
#include "json.h"
#include "options.h"
#include "output.h"
#include "parser.h"
#include "settings.h"
#include "sort.h"
#include "ui.h"
#include "util.h"

static WINDOW *header_win, *main_win;

GConf conf = { 0 };

int active_gdns = 0;
static GDash *dash;
static GHolder *holder;
static GLog *logger;
GSpinner *parsing_spinner;

/* *INDENT-OFF* */
static GScrolling scrolling = {
  {
    {0, 0}, /* visitors    {scroll, offset} */
    {0, 0}, /* requests    {scroll, offset} */
    {0, 0}, /* req static  {scroll, offset} */
    {0, 0}, /* not found   {scroll, offset} */
    {0, 0}, /* hosts       {scroll, offset} */
    {0, 0}, /* os          {scroll, offset} */
    {0, 0}, /* browsers    {scroll, offset} */
    {0, 0}, /* referrers   {scroll, offset} */
    {0, 0}, /* ref sites   {scroll, offset} */
    {0, 0}, /* keywords    {scroll, offset} */
#ifdef HAVE_LIBGEOIP
    {0, 0}, /* geolocation {scroll, offset} */
#endif
    {0, 0}, /* status      {scroll, offset} */
  },
  0,         /* current module */
  0,         /* main dashboard scroll */
  0,         /* expanded flag */
};
/* *INDENT-ON* */

static void
house_keeping (void)
{
  /* REVERSE DNS THREAD */
  pthread_mutex_lock (&gdns_thread.mutex);
  /* kill dns pthread */
  active_gdns = 0;
  free_holder (&holder);
  gdns_free_queue ();
  if (ht_hostnames != NULL) {
#ifdef HAVE_LIBTOKYOCABINET
    tc_db_close (ht_hostnames, DB_HOSTNAMES);
#else
    g_hash_table_destroy (ht_hostnames);
#endif
  }
  pthread_mutex_unlock (&gdns_thread.mutex);

  /* DASHBOARD */
  if (dash && !conf.output_html) {
    free_dashboard (dash);
    reset_find ();
  }

  /* GEOLOCATION */
#ifdef HAVE_LIBGEOIP
  if (geo_location_data != NULL)
    GeoIP_delete (geo_location_data);
#endif

  /* STORAGE */
  free_storage ();

  /* LOGGER */
  free (logger);

  /* CONFIGURATION */
  if (conf.debug_log) {
    LOG_DEBUG (("Bye.\n"));
    dbg_log_close ();
  }
  free_cmd_args ();
}

static void
parse_initial_sort (void)
{
  int i;
  char *view;
  char module[9], field[8], order[5];
  for (i = 0; i < conf.sort_view_idx; ++i) {
    view = conf.sort_views[i];
    if (sscanf (view, "%8[^','],%7[^','],%4s", module, field, order) != 3)
      continue;
    set_initial_sort (module, field, order);
  }
}


/* allocate memory for an instance of holder */
static void
allocate_holder_by_module (GModule module)
{
#ifdef TCB_BTREE
  TCBDB *ht = NULL;
#elif TCB_MEMHASH
  TCMDB *ht = NULL;
#else
  GHashTable *ht;
#endif

  GRawData *raw_data;
  unsigned int ht_size = 0;

  /* extract data from the corresponding hash table */
  ht = get_ht_by_module (module);
  ht_size = get_ht_size_by_module (module);
  raw_data = parse_raw_data (ht, ht_size, module);
  load_holder_data (raw_data, holder + module, module, module_sort[module]);
}

/* allocate memory for an instance of holder */
static void
allocate_holder (void)
{
#ifdef TCB_BTREE
  TCBDB *ht = NULL;
#elif TCB_MEMHASH
  TCMDB *ht = NULL;
#else
  GHashTable *ht;
#endif

  GModule module;
  GRawData *raw_data;
  int i;
  unsigned int ht_size = 0;

  holder = new_gholder (TOTAL_MODULES);
  for (i = 0; i < TOTAL_MODULES; i++) {
    module = i;

    /* extract data from the corresponding hash table */
    ht = get_ht_by_module (module);
    ht_size = get_ht_size_by_module (module);
    raw_data = parse_raw_data (ht, ht_size, module);
    load_holder_data (raw_data, holder + module, module, module_sort[module]);
  }
}

/* allocate memory for an instance of dashboard */
static void
allocate_data (void)
{
  int col_data = DASH_COLLAPSED - DASH_NON_DATA;
  int size = 0, ht_size = 0;

  int i;
  GModule module;

  dash = new_gdash ();
  for (i = 0; i < TOTAL_MODULES; i++) {
    module = i;

    switch (module) {
     case VISITORS:
       dash->module[module].head = VISIT_HEAD;
       dash->module[module].desc = VISIT_DESC;
       break;
     case REQUESTS:
       dash->module[module].head = REQUE_HEAD;
       dash->module[module].desc = REQUE_DESC;
       break;
     case REQUESTS_STATIC:
       dash->module[module].head = STATI_HEAD;
       dash->module[module].desc = STATI_DESC;
       break;
     case NOT_FOUND:
       dash->module[module].head = FOUND_HEAD;
       dash->module[module].desc = FOUND_DESC;
       break;
     case HOSTS:
       dash->module[module].head = HOSTS_HEAD;
       dash->module[module].desc = HOSTS_DESC;
       break;
     case OS:
       dash->module[module].head = OPERA_HEAD;
       dash->module[module].desc = OPERA_DESC;
       break;
     case BROWSERS:
       dash->module[module].head = BROWS_HEAD;
       dash->module[module].desc = BROWS_DESC;
       break;
     case REFERRERS:
       dash->module[module].head = REFER_HEAD;
       dash->module[module].desc = REFER_DESC;
       break;
     case REFERRING_SITES:
       dash->module[module].head = SITES_HEAD;
       dash->module[module].desc = SITES_DESC;
       break;
     case KEYPHRASES:
       dash->module[module].head = KEYPH_HEAD;
       dash->module[module].desc = KEYPH_DESC;
       break;
#ifdef HAVE_LIBGEOIP
     case GEO_LOCATION:
       dash->module[module].head = GEOLO_HEAD;
       dash->module[module].desc = GEOLO_DESC;
       break;
#endif
     case STATUS_CODES:
       dash->module[module].head = CODES_HEAD;
       dash->module[module].desc = CODES_DESC;
       break;
    }

    ht_size = get_ht_size_by_module (module);
    size = ht_size > col_data ? col_data : ht_size;
    if ((size > MAX_CHOICES) ||
        (scrolling.expanded && module == scrolling.current))
      size = MAX_CHOICES;

    dash->module[module].alloc_data = size;     /* data allocated  */
    dash->module[module].ht_size = ht_size;     /* hash table size */
    dash->module[module].idx_data = 0;
    dash->module[module].pos_y = 0;

    if (scrolling.expanded && module == scrolling.current)
      dash->module[module].dash_size = DASH_EXPANDED;
    else
      dash->module[module].dash_size = DASH_COLLAPSED;
    dash->total_alloc += dash->module[module].dash_size;

    pthread_mutex_lock (&gdns_thread.mutex);
    load_data_to_dash (&holder[module], dash, module, &scrolling);
    pthread_mutex_unlock (&gdns_thread.mutex);
  }
}

/* render all windows */
static void
render_screens (void)
{
  int row, col, chg = 0;

  getmaxyx (stdscr, row, col);
  term_size (main_win);

  generate_time ();
  chg = logger->process - logger->offset;

  draw_header (stdscr, "", "%s", row - 1, 0, col, 0, 0);
  wattron (stdscr, COLOR_PAIR (COL_WHITE));
  mvaddstr (row - 1, 1, "[F1]Help [O]pen detail view");
  mvprintw (row - 1, 30, "%d - %s", chg, asctime (now_tm));
  mvaddstr (row - 1, col - 21, "[Q]uit GoAccess");
  mvprintw (row - 1, col - 5, "%s", GO_VERSION);
  wattroff (stdscr, COLOR_PAIR (COL_WHITE));
  refresh ();

  /* call general stats header */
  display_general (header_win, conf.ifile, logger);
  wrefresh (header_win);

  /* display active label based on current module */
  update_active_module (header_win, scrolling.current);

  display_content (main_win, logger, dash, &scrolling);
}

/* collapse the current expanded module */
static void
collapse_current_module (void)
{
  if (scrolling.expanded) {
    scrolling.expanded = 0;
    reset_scroll_offsets (&scrolling);
    free_dashboard (dash);
    allocate_data ();
    render_screens ();
  }
}

static void
set_module_to (GScrolling * scrll, GModule module)
{
  /* reset expanded module */
  collapse_current_module ();
  scrll->current = module;
  render_screens ();
}

static void
get_keys (void)
{
  int search;
  int c, quit = 1, scrll, offset, ok_mouse;
  int *scroll_ptr, *offset_ptr;
  int exp_size = DASH_EXPANDED - DASH_NON_DATA;
  MEVENT event;

  char buf[LINE_BUFFER];
  FILE *fp = NULL;
  unsigned long long size1 = 0, size2 = 0;

  if (!logger->piping)
    size1 = file_size (conf.ifile);
  while (quit) {
    c = wgetch (stdscr);
    switch (c) {
     case 'q': /* quit */
       if (!scrolling.expanded) {
         quit = 0;
         break;
       }
       collapse_current_module ();
       break;
     case KEY_F (1):
     case '?':
     case 'h':
       load_help_popup (main_win);
       render_screens ();
       break;
     case 49:  /* 1 */
       /* reset expanded module */
       set_module_to (&scrolling, VISITORS);
       break;
     case 50:  /* 2 */
       /* reset expanded module */
       set_module_to (&scrolling, REQUESTS);
       break;
     case 51:  /* 3 */
       /* reset expanded module */
       set_module_to (&scrolling, REQUESTS_STATIC);
       break;
     case 52:  /* 4 */
       /* reset expanded module */
       set_module_to (&scrolling, NOT_FOUND);
       break;
     case 53:  /* 5 */
       /* reset expanded module */
       set_module_to (&scrolling, HOSTS);
       break;
     case 54:  /* 6 */
       /* reset expanded module */
       set_module_to (&scrolling, OS);
       break;
     case 55:  /* 7 */
       /* reset expanded module */
       set_module_to (&scrolling, BROWSERS);
       break;
     case 56:  /* 8 */
       /* reset expanded module */
       set_module_to (&scrolling, REFERRERS);
       break;
     case 57:  /* 9 */
       /* reset expanded module */
       set_module_to (&scrolling, REFERRING_SITES);
       break;
     case 48:  /* 0 */
       /* reset expanded module */
       set_module_to (&scrolling, KEYPHRASES);
       break;
     case 33:  /* Shift+1 */
       /* reset expanded module */
#ifdef HAVE_LIBGEOIP
       set_module_to (&scrolling, GEO_LOCATION);
#else
       set_module_to (&scrolling, STATUS_CODES);
#endif
       break;
#ifdef HAVE_LIBGEOIP
     case 64:  /* Shift+2 */
       /* reset expanded module */
       set_module_to (&scrolling, STATUS_CODES);
       break;
#endif
     case 9:   /* TAB */
       /* reset expanded module */
       collapse_current_module ();
       scrolling.current++;
       if (scrolling.current == TOTAL_MODULES)
         scrolling.current = 0;
       render_screens ();
       break;
     case 353: /* Shift TAB */
       /* reset expanded module */
       collapse_current_module ();
       if (scrolling.current == 0)
         scrolling.current = TOTAL_MODULES - 1;
       else
         scrolling.current--;
       render_screens ();
       break;
     case 'g': /* g = top */
       if (!scrolling.expanded)
         scrolling.dash = 0;
       else {
         scrolling.module[scrolling.current].scroll = 0;
         scrolling.module[scrolling.current].offset = 0;
       }
       display_content (main_win, logger, dash, &scrolling);
       break;
     case 'G': /* G = down */
       if (!scrolling.expanded)
         scrolling.dash = dash->total_alloc - real_size_y;
       else {
         scrll = offset = 0;
         scrll = dash->module[scrolling.current].idx_data - 1;
         if (scrll >= exp_size && scrll >= offset + exp_size)
           offset = scrll < exp_size - 1 ? 0 : scrll - exp_size + 1;
         scrolling.module[scrolling.current].scroll = scrll;
         scrolling.module[scrolling.current].offset = offset;
       }
       display_content (main_win, logger, dash, &scrolling);
       break;
       /* expand dashboard module */
     case KEY_RIGHT:
     case 0x0a:
     case 0x0d:
     case 32:  /* ENTER */
     case 79:  /* o */
     case 111: /* O */
     case KEY_ENTER:
       if (scrolling.expanded && scrolling.current == HOSTS) {
         /* make sure we have a valid IP */
         int sel = scrolling.module[scrolling.current].scroll;
         if (!invalid_ipaddr (dash->module[HOSTS].data[sel].data))
           load_agent_list (main_win, dash->module[HOSTS].data[sel].data);
         break;
       }
       if (scrolling.expanded)
         break;
       reset_scroll_offsets (&scrolling);
       scrolling.expanded = 1;

       free_holder_by_module (&holder, scrolling.current);
       free_dashboard (dash);
       allocate_holder_by_module (scrolling.current);
       allocate_data ();

       display_content (main_win, logger, dash, &scrolling);
       break;
     case KEY_DOWN:    /* scroll main dashboard */
       if ((scrolling.dash + real_size_y) < (unsigned) dash->total_alloc) {
         scrolling.dash++;
         display_content (main_win, logger, dash, &scrolling);
       }
       break;
     case KEY_MOUSE:   /* handles mouse events */
       ok_mouse = getmouse (&event);
       if (conf.mouse_support && ok_mouse == OK) {
         if (event.bstate & BUTTON1_CLICKED) {
           /* ignore header/footer clicks */
           if (event.y < MAX_HEIGHT_HEADER || event.y == LINES - 1)
             break;

           if (set_module_from_mouse_event (&scrolling, dash, event.y))
             break;
           reset_scroll_offsets (&scrolling);
           scrolling.expanded = 1;

           free_holder_by_module (&holder, scrolling.current);
           free_dashboard (dash);
           allocate_holder_by_module (scrolling.current);
           allocate_data ();

           render_screens ();
         }
       }
       break;
     case 106: /* j - DOWN expanded module */
       scroll_ptr = &scrolling.module[scrolling.current].scroll;
       offset_ptr = &scrolling.module[scrolling.current].offset;

       if (!scrolling.expanded)
         break;
       if (*scroll_ptr >= dash->module[scrolling.current].idx_data - 1)
         break;
       ++(*scroll_ptr);
       if (*scroll_ptr >= exp_size && *scroll_ptr >= *offset_ptr + exp_size)
         ++(*offset_ptr);
       display_content (main_win, logger, dash, &scrolling);
       break;
       /* scroll up main_win */
     case KEY_UP:
       if (scrolling.dash > 0) {
         scrolling.dash--;
         display_content (main_win, logger, dash, &scrolling);
       }
       break;
     case 2:   /* ^ b - page up */
     case 339: /* ^ PG UP */
       scroll_ptr = &scrolling.module[scrolling.current].scroll;
       offset_ptr = &scrolling.module[scrolling.current].offset;

       if (!scrolling.expanded)
         break;
       /* decrease scroll and offset by exp_size */
       *scroll_ptr -= exp_size;
       if (*scroll_ptr < 0)
         *scroll_ptr = 0;

       if (*scroll_ptr < *offset_ptr)
         *offset_ptr -= exp_size;
       if (*offset_ptr <= 0)
         *offset_ptr = 0;
       display_content (main_win, logger, dash, &scrolling);
       break;
     case 6:   /* ^ f - page down */
     case 338: /* ^ PG DOWN */
       scroll_ptr = &scrolling.module[scrolling.current].scroll;
       offset_ptr = &scrolling.module[scrolling.current].offset;

       if (!scrolling.expanded)
         break;

       *scroll_ptr += exp_size;
       if (*scroll_ptr >= dash->module[scrolling.current].idx_data - 1)
         *scroll_ptr = dash->module[scrolling.current].idx_data - 1;
       if (*scroll_ptr >= exp_size && *scroll_ptr >= *offset_ptr + exp_size)
         *offset_ptr += exp_size;
       if (*offset_ptr + exp_size >=
           dash->module[scrolling.current].idx_data - 1)
         *offset_ptr = dash->module[scrolling.current].idx_data - exp_size;
       if (*scroll_ptr < exp_size - 1)
         *offset_ptr = 0;

       display_content (main_win, logger, dash, &scrolling);
       break;
     case 107: /* k - UP expanded module */
       scroll_ptr = &scrolling.module[scrolling.current].scroll;
       offset_ptr = &scrolling.module[scrolling.current].offset;

       if (!scrolling.expanded)
         break;
       if (*scroll_ptr <= 0)
         break;
       --(*scroll_ptr);
       if (*scroll_ptr < *offset_ptr)
         --(*offset_ptr);
       display_content (main_win, logger, dash, &scrolling);
       break;
     case 'n':
       pthread_mutex_lock (&gdns_thread.mutex);
       search = perform_next_find (holder, &scrolling);
       pthread_mutex_unlock (&gdns_thread.mutex);
       if (search == 0) {
         free_dashboard (dash);
         allocate_data ();
         render_screens ();
       }
       break;
     case '/':
       if (render_find_dialog (main_win, &scrolling))
         break;
       pthread_mutex_lock (&gdns_thread.mutex);
       search = perform_next_find (holder, &scrolling);
       pthread_mutex_unlock (&gdns_thread.mutex);
       if (search == 0) {
         free_dashboard (dash);
         allocate_data ();
         render_screens ();
       }
       break;
     case 99:  /* c */
       if (conf.no_color)
         break;
       load_schemes_win (main_win);
       free_dashboard (dash);
       allocate_data ();
       render_screens ();
       break;
     case 115: /* s */
       load_sort_win (main_win, scrolling.current,
                      &module_sort[scrolling.current]);
       pthread_mutex_lock (&gdns_thread.mutex);
       free_holder (&holder);
       pthread_cond_broadcast (&gdns_thread.not_empty);
       pthread_mutex_unlock (&gdns_thread.mutex);
       free_dashboard (dash);
       allocate_holder ();
       allocate_data ();
       render_screens ();
       break;
     case 269:
     case KEY_RESIZE:
       endwin ();
       refresh ();
       werase (header_win);
       werase (main_win);
       werase (stdscr);
       term_size (main_win);
       refresh ();
       render_screens ();
       break;
     default:
       if (logger->piping)
         break;
       size2 = file_size (conf.ifile);

       /* file has changed */
       if (size2 != size1) {
         if (!(fp = fopen (conf.ifile, "r")))
           FATAL ("Unable to read log file %s.", strerror (errno));
         if (!fseeko (fp, size1, SEEK_SET))
           while (fgets (buf, LINE_BUFFER, fp) != NULL)
             parse_log (&logger, buf, -1);
         fclose (fp);

         size1 = size2;
         pthread_mutex_lock (&gdns_thread.mutex);
         free_holder (&holder);
         pthread_cond_broadcast (&gdns_thread.not_empty);
         pthread_mutex_unlock (&gdns_thread.mutex);

         free_dashboard (dash);
         allocate_holder ();
         allocate_data ();

         term_size (main_win);
         render_screens ();
         usleep (200000);       /* 0.2 seconds */
       }
       break;
    }
  }
}

static void
set_general_stats (void)
{
  logger->process = logger->invalid = logger->exclude_ip = 0;
#ifdef TCB_BTREE
  logger->exclude_ip = tc_db_get_int (ht_general_stats, "exclude_ip");
  logger->invalid = tc_db_get_int (ht_general_stats, "failed_requests");
  logger->process = tc_db_get_int (ht_general_stats, "total_requests");
  logger->resp_size = tc_db_get_uint64 (ht_general_stats, "bandwidth");
  if (logger->resp_size > 0)
    conf.bandwidth = 1;
  if (get_ht_size (ht_file_serve_usecs) > 0)
    conf.serve_usecs = 1;
#endif
}

#ifdef HAVE_LIBGEOIP
static void
init_geoip (void)
{
  /* open custom GeoIP database */
  if (conf.geoip_city_data != NULL)
    geo_location_data = geoip_open_db (conf.geoip_city_data);
  /* fall back to legacy GeoIP database */
  else
    geo_location_data = GeoIP_new (conf.geo_db);
}
#endif

static void
set_locale (void)
{
  char *loc_ctype;

  loc_ctype = getenv ("LC_CTYPE");
  if (loc_ctype != NULL)
    setlocale (LC_CTYPE, loc_ctype);
  else if ((loc_ctype = getenv ("LC_ALL")))
    setlocale (LC_CTYPE, loc_ctype);
  else
    setlocale (LC_CTYPE, "");
}

static void
parse_cmd_line (int argc, char **argv)
{
  read_option_args (argc, argv);

  if (!isatty (STDOUT_FILENO) || conf.output_format != NULL)
    conf.output_html = 1;
  if (conf.ifile != NULL && !isatty (STDIN_FILENO) && !conf.output_html)
    cmd_help ();
  if (conf.ifile == NULL && isatty (STDIN_FILENO) && conf.output_format == NULL)
    cmd_help ();
}

#if defined(__GLIBC__)
static void
setup_signal_handlers (void)
{
  struct sigaction act;
  sigemptyset (&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = sigsegv_handler;

  sigaction (SIGSEGV, &act, NULL);
}
#endif

int
main (int argc, char **argv)
{
  int quit = 0;

#if defined(__GLIBC__)
  setup_signal_handlers ();
#endif

  /* command line/config options */
  verify_global_config (argc, argv);
  parse_conf_file (&argc, &argv);
  parse_cmd_line (argc, argv);

  /* initialize storage */
  init_storage ();
  /* setup to use the current locale */
  set_locale ();

#ifdef HAVE_LIBGEOIP
  init_geoip ();
#endif

  /* init logger */
  logger = init_log ();
  /* init parsing spinner */
  parsing_spinner = new_gspinner ();
  parsing_spinner->process = &logger->process;

  /* outputting to stdout */
  if (conf.output_html) {
    ui_spinner_create (parsing_spinner);
    goto out;
  }

  /* init curses */
  set_input_opts ();
  if (conf.no_color || has_colors () == FALSE) {
    conf.color_scheme = NO_COLOR;
    conf.no_color = 1;
  } else {
    start_color ();
  }
  init_colors ();
  init_windows (&header_win, &main_win);
  set_curses_spinner (parsing_spinner);

  /* configuration dialog */
  if (isatty (STDIN_FILENO) && (conf.log_format == NULL || conf.load_conf_dlg)) {
    refresh ();
    quit = verify_format (logger, parsing_spinner);
  }
  /* straight parsing */
  else {
    ui_spinner_create (parsing_spinner);
  }

out:

  /* main processing event */
  time (&start_proc);
  if (conf.load_from_disk)
    set_general_stats ();
  else if (!quit && parse_log (&logger, NULL, -1))
    FATAL ("Error while processing file");

  logger->offset = logger->process;

  /* no valid entries to process from the log */
  if ((logger->process == 0) || (logger->process == logger->invalid))
    FATAL ("Nothing valid to process.");

  /* init reverse lookup thread */
  gdns_init ();
  parse_initial_sort ();
  allocate_holder ();

  end_spinner ();
  time (&end_proc);

  /* stdout */
  if (conf.output_html) {
    /* CSV */
    if (conf.output_format && strcmp ("csv", conf.output_format) == 0)
      output_csv (logger, holder);
    /* JSON */
    else if (conf.output_format && strcmp ("json", conf.output_format) == 0)
      output_json (logger, holder);
    /* HTML */
    else
      output_html (logger, holder);
  }
  /* curses */
  else {
    allocate_data ();
    if (!conf.skip_term_resolver)
      gdns_thread_create ();

    render_screens ();
    get_keys ();

    attroff (COLOR_PAIR (COL_WHITE));
    /* restore tty modes and reset
     * terminal into non-visual mode */
    endwin ();
  }
  /* clean */
  house_keeping ();

  return EXIT_SUCCESS;
}
