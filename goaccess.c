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
#include <getopt.h>

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

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabinet.h"
#else
#include "glibht.h"
#endif

#include "commons.h"
#include "csv.h"
#include "error.h"
#include "gdashboard.h"
#include "gdns.h"
#include "json.h"
#include "output.h"
#include "parser.h"
#include "settings.h"
#include "ui.h"
#include "util.h"

static WINDOW *header_win, *main_win;

static char short_options[] = "f:e:p:o:"
#ifdef DEBUG
  "l:"
#endif
#ifdef HAVE_LIBGEOIP
  "g"
#endif
  "acrmMhHqd";

GConf conf = { 0 };

int active_gdns = 0;
static GDash *dash;
static GHolder *holder;
static GLog *logger;
GSpinner *parsing_spinner;

/* *INDENT-OFF* */
struct option long_opts[] = {
  {"agent-list"           , no_argument       , 0 , 'a' } ,
  {"conf-dialog"          , no_argument       , 0 , 'c' } ,
  {"conf-file"            , required_argument , 0 , 'p' } ,
  {"exclude-ip"           , required_argument , 0 , 'e' } ,
  {"help"                 , no_argument       , 0 , 'h' } ,
  {"http-method"          , no_argument       , 0 , 'M' } ,
  {"http-protocol"        , no_argument       , 0 , 'H' } ,
  {"log-file"             , required_argument , 0 , 'l' } ,
  {"no-query-string"      , no_argument       , 0 , 'q' } ,
  {"no-term-resolver"     , no_argument       , 0 , 'r' } ,
  {"output-format"        , required_argument , 0 , 'o' } ,
  {"real-os"              , no_argument       , 0 , 0   } ,
  {"no-color"             , no_argument       , 0 , 0   } ,
  {"std-geip"             , no_argument       , 0 , 'g' } ,
  {"with-mouse"           , no_argument       , 0 , 'm' } ,
  {"with-output-resolver" , no_argument       , 0 , 'd' } ,
#ifdef HAVE_LIBTOKYOCABINET
  {"cache-lcnum"          , required_argument , 0 , 0   } ,
  {"cache-ncnum"          , required_argument , 0 , 0   } ,
  {"tune-lmemb"           , required_argument , 0 , 0   } ,
  {"tune-nmemb"           , required_argument , 0 , 0   } ,
  {"tune-bnum"            , required_argument , 0 , 0   } ,
#endif
  {0, 0, 0, 0}
};

static GSort sort[TOTAL_MODULES] = {
  {VISITORS        , SORT_BY_DATA, SORT_DESC},
  {REQUESTS        , SORT_BY_HITS, SORT_DESC},
  {REQUESTS_STATIC , SORT_BY_HITS, SORT_DESC},
  {NOT_FOUND       , SORT_BY_HITS, SORT_DESC},
  {HOSTS           , SORT_BY_HITS, SORT_DESC},
  {OS              , SORT_BY_HITS, SORT_DESC},
  {BROWSERS        , SORT_BY_HITS, SORT_DESC},
  {REFERRERS       , SORT_BY_HITS, SORT_DESC},
  {REFERRING_SITES , SORT_BY_HITS, SORT_DESC},
  {KEYPHRASES      , SORT_BY_HITS, SORT_DESC},
#ifdef HAVE_LIBGEOIP
  {GEO_LOCATION    , SORT_BY_HITS, SORT_DESC},
#endif
  {STATUS_CODES    , SORT_BY_HITS, SORT_DESC},
};

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
cmd_help (void)
{
  printf ("\nGoAccess - %s\n\n", GO_VERSION);
  printf ("Usage: ");
  printf ("goaccess -f log_file [-c][-r][-m][-h][-q][-d][...]\n\n");
  printf ("The following options can also be supplied to the command:\n\n");
  printf (" -f <filename>                ");
  printf ("Path to input log file.\n");
  printf (" -a --agent-list              ");
  printf ("Enable a list of user-agents by host.\n");
  printf ("                              ");
  printf ("For faster parsing, don't enable this flag.\n");
  printf (" -c --conf-dialog             ");
  printf ("Prompt log/date configuration window.\n");
  printf (" -d --with-output-resolver    ");
  printf ("Enable IP resolver on HTML|JSON output.\n");
  printf (" -e --exclude-ip=<IP>         ");
  printf ("Exclude an IP from being counted.\n");
#ifdef HAVE_LIBGEOIP
  printf (" -g --std-geoip               ");
  printf ("Standard GeoIP database for less memory usage.\n");
#endif
#ifdef DEBUG
  printf (" -l --log-file=<filename>     ");
  printf ("Send all debug messages to the specified file.\n");
#endif
  printf (" -h --help                    ");
  printf ("This help.\n");
  printf (" -H --http-protocol           ");
  printf ("Include HTTP request protocol if found.\n");
  printf (" -m --with-mouse              ");
  printf ("Enable mouse support on main dashboard.\n");
  printf (" -M --http-method             ");
  printf ("Include HTTP request method if found.\n");
  printf (" -o --output-format=csv|json  ");
  printf ("Output format:\n");
  printf ("                              ");
  printf ("'-o csv' for CSV.\n");
  printf ("                              ");
  printf ("'-o json' for JSON.\n");
  printf (" -p --conf-file=<filename>    ");
  printf ("Custom configuration file.\n");
  printf (" -q --no-query-string         ");
  printf ("Ignore request's query string.\n");
  printf (" -r --no-term-resolver        ");
  printf ("Disable IP resolver on terminal output.\n");
#ifdef HAVE_LIBTOKYOCABINET
  printf (" --cache-lcnum                ");
  printf ("Max number of leaf nodes to be cached. Default %d\n", TC_LCNUM);
  printf (" --cache-ncnum                ");
  printf ("Max number of non-leaf nodes to be cached. Default %d\n",
          TC_NCNUM);
  printf (" --tune-lmemb                 ");
  printf ("Number of members in each leaf page. Default %d\n", TC_LMEMB);
  printf (" --tune-nmemb                 ");
  printf ("Number of members in each non-leaf page. Default %d\n", TC_NMEMB);
  printf (" --tune-bnum                  ");
  printf ("Number of elements of the bucket array. Default %d\n", TC_BNUM);
#endif
  printf (" --no-color                   ");
  printf ("Disable colored output.\n");
  printf (" --real-os                    ");
  printf ("Display real OS names. e.g, Windows XP, Snow Leopard.\n\n");

  printf ("Examples can be found by running `man goaccess`.\n\n");
  printf ("For more details visit: http://goaccess.prosoftcorp.com\n");
  printf ("GoAccess Copyright (C) 2009-2014 GNU GPL'd, by Gerardo Orellana");
  printf ("\n\n");
  exit (EXIT_FAILURE);
}

static void
house_keeping (void)
{
  pthread_mutex_lock (&gdns_thread.mutex);
  active_gdns = 0;              /* kill dns pthread */
  free_holder (&holder);
  gdns_free_queue ();

#ifdef HAVE_LIBTOKYOCABINET
  if (ht_hostnames != NULL)
    tc_db_close (ht_hostnames, DB_HOSTNAMES);
#else
  g_hash_table_destroy (ht_hostnames);
#endif

  pthread_mutex_unlock (&gdns_thread.mutex);

  if (dash && !conf.output_html) {
    free_dashboard (dash);
    reset_find ();
  }
#ifdef HAVE_LIBGEOIP
  if (geo_location_data != NULL)
    GeoIP_delete (geo_location_data);
#endif

#ifdef HAVE_LIBTOKYOCABINET

  tc_db_foreach (ht_requests, free_requests, NULL);
  tc_db_foreach (ht_requests_static, free_requests, NULL);
  tc_db_foreach (ht_not_found_requests, free_requests, NULL);

#ifdef HAVE_LIBGEOIP
  tc_db_close (ht_countries, DB_COUNTRIES);
#endif
  tc_db_close (ht_browsers, DB_BROWSERS);
  tc_db_close (ht_date_bw, DB_DATE_BW);
  tc_db_close (ht_file_bw, DB_FILE_BW);
  tc_db_close (ht_file_serve_usecs, DB_FILE_SERVE_USECS);
  tc_db_close (ht_host_bw, DB_HOST_BW);
  tc_db_close (ht_hosts, DB_HOSTS);
  tc_db_close (ht_hosts_agents, DB_HOST_AGENTS);
  tc_db_close (ht_host_serve_usecs, DB_HOST_SERVE_USECS);
  tc_db_close (ht_keyphrases, DB_KEYPHRASES);
  tc_db_close (ht_not_found_requests, DB_NOT_FOUND_REQUESTS);
  tc_db_close (ht_os, DB_OS);
  tc_db_close (ht_referrers, DB_REFERRERS);
  tc_db_close (ht_referring_sites, DB_REFERRING_SITES);
  tc_db_close (ht_requests, DB_REQUESTS);
  tc_db_close (ht_requests_static, DB_REQUESTS_STATIC);
  tc_db_close (ht_status_code, DB_STATUS_CODE);
  tc_db_close (ht_unique_vis, DB_UNIQUE_VIS);
  tc_db_close (ht_unique_visitors, DB_UNIQUE_VISITORS);

#else

#ifdef HAVE_LIBGEOIP
  g_hash_table_foreach (ht_countries, free_countries, NULL);
#endif

  g_hash_table_foreach (ht_requests, free_requests, NULL);
  g_hash_table_foreach (ht_requests_static, free_requests, NULL);
  g_hash_table_foreach (ht_not_found_requests, free_requests, NULL);

  g_hash_table_foreach (ht_os, free_os, NULL);
  g_hash_table_foreach (ht_browsers, free_browser, NULL);

  g_hash_table_destroy (ht_browsers);
  g_hash_table_destroy (ht_countries);
  g_hash_table_destroy (ht_date_bw);
  g_hash_table_destroy (ht_file_bw);
  g_hash_table_destroy (ht_host_bw);
  g_hash_table_destroy (ht_hosts);
  g_hash_table_destroy (ht_keyphrases);
  g_hash_table_destroy (ht_not_found_requests);
  g_hash_table_destroy (ht_os);
  g_hash_table_destroy (ht_referrers);
  g_hash_table_destroy (ht_referring_sites);
  g_hash_table_destroy (ht_requests);
  g_hash_table_destroy (ht_requests_static);
  g_hash_table_destroy (ht_status_code);
  g_hash_table_destroy (ht_unique_vis);
  g_hash_table_destroy (ht_unique_visitors);

#endif

  free (logger);

  /* realpath() uses malloc */
  if (conf.iconfigfile)
    free (conf.iconfigfile);

  if (conf.debug_log) {
    LOG_DEBUG (("Bye.\n"));
    dbg_log_close ();
    free (conf.debug_log);
  }
}

/* allocate memory for an instance of holder */
static void
allocate_holder_by_module (GModule module)
{
#ifdef HAVE_LIBTOKYOCABINET
  TCBDB *ht;
#else
  GHashTable *ht;
#endif

  GRawData *raw_data;
  unsigned int ht_size = 0;

  /* extract data from the corresponding hash table */
  ht = get_ht_by_module (module);
  ht_size = get_ht_size_by_module (module);
  raw_data = parse_raw_data (ht, ht_size, module);
  load_data_to_holder (raw_data, holder + module, module, sort[module]);
}

/* allocate memory for an instance of holder */
static void
allocate_holder (void)
{
#ifdef HAVE_LIBTOKYOCABINET
  TCBDB *ht;
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
    load_data_to_holder (raw_data, holder + module, module, sort[module]);
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

  draw_header (stdscr, "", "%s", row - 1, 0, col, 0);
  wattron (stdscr, COLOR_PAIR (COL_WHITE));
  mvaddstr (row - 1, 1, "[F1]Help [O]pen detail view");
  mvprintw (row - 1, 30, "%d - %s", chg, asctime (now_tm));
  mvaddstr (row - 1, col - 21, "[Q]uit GoAccess");
  mvprintw (row - 1, col - 5, "%s", GO_VERSION);
  wattroff (stdscr, COLOR_PAIR (COL_WHITE));
  refresh ();

  /* call general stats header */
  display_general (header_win, conf.ifile, logger->piping, logger->process,
                   logger->invalid, logger->resp_size);
  wrefresh (header_win);

  /* display active label based on current module */
  update_active_module (header_win, scrolling.current);

  display_content (main_win, logger, dash, &scrolling);
  /* no valid entries to process from the log */
  if ((logger->process == 0) || (logger->process == logger->invalid))
    error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                   "Nothing valid to process.");
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
     case 'q':                 /* quit */
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
     case 49:                  /* 1 */
       /* reset expanded module */
       set_module_to (&scrolling, VISITORS);
       break;
     case 50:                  /* 2 */
       /* reset expanded module */
       set_module_to (&scrolling, REQUESTS);
       break;
     case 51:                  /* 3 */
       /* reset expanded module */
       set_module_to (&scrolling, REQUESTS_STATIC);
       break;
     case 52:                  /* 4 */
       /* reset expanded module */
       set_module_to (&scrolling, NOT_FOUND);
       break;
     case 53:                  /* 5 */
       /* reset expanded module */
       set_module_to (&scrolling, HOSTS);
       break;
     case 54:                  /* 6 */
       /* reset expanded module */
       set_module_to (&scrolling, OS);
       break;
     case 55:                  /* 7 */
       /* reset expanded module */
       set_module_to (&scrolling, BROWSERS);
       break;
     case 56:                  /* 8 */
       /* reset expanded module */
       set_module_to (&scrolling, REFERRERS);
       break;
     case 57:                  /* 9 */
       /* reset expanded module */
       set_module_to (&scrolling, REFERRING_SITES);
       break;
     case 48:                  /* 0 */
       /* reset expanded module */
       set_module_to (&scrolling, KEYPHRASES);
       break;
     case 33:                  /* Shift+1 */
       /* reset expanded module */
#ifdef HAVE_LIBGEOIP
       set_module_to (&scrolling, GEO_LOCATION);
#else
       set_module_to (&scrolling, STATUS_CODES);
#endif
       break;
#ifdef HAVE_LIBGEOIP
     case 64:                  /* Shift+2 */
       /* reset expanded module */
       set_module_to (&scrolling, STATUS_CODES);
       break;
#endif
     case 9:                   /* TAB */
       /* reset expanded module */
       collapse_current_module ();
       scrolling.current++;
       if (scrolling.current == TOTAL_MODULES)
         scrolling.current = 0;
       render_screens ();
       break;
     case 353:                 /* Shift TAB */
       /* reset expanded module */
       collapse_current_module ();
       if (scrolling.current == 0)
         scrolling.current = TOTAL_MODULES - 1;
       else
         scrolling.current--;
       render_screens ();
       break;
     case 'g':                 /* g = top */
       if (!scrolling.expanded)
         scrolling.dash = 0;
       else {
         scrolling.module[scrolling.current].scroll = 0;
         scrolling.module[scrolling.current].offset = 0;
       }
       display_content (main_win, logger, dash, &scrolling);
       break;
     case 'G':                 /* G = down */
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
     case 32:                  /* ENTER */
     case 79:                  /* o */
     case 111:                 /* O */
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
     case KEY_DOWN:            /* scroll main dashboard */
       if ((scrolling.dash + real_size_y) < (unsigned) dash->total_alloc) {
         scrolling.dash++;
         display_content (main_win, logger, dash, &scrolling);
       }
       break;
     case KEY_MOUSE:           /* handles mouse events */
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
     case 106:                 /* j - DOWN expanded module */
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
     case 2:                   /* ^ b - page up */
     case 339:                 /* ^ PG UP */
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
     case 6:                   /* ^ f - page down */
     case 338:                 /* ^ PG DOWN */
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
     case 107:                 /* k - UP expanded module */
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
     case 99:                  /* c */
       if (conf.no_color)
         break;
       load_schemes_win (main_win);
       free_dashboard (dash);
       allocate_data ();
       render_screens ();
       break;
     case 115:                 /* s */
       load_sort_win (main_win, scrolling.current, &sort[scrolling.current]);
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
           error_handler (__PRETTY_FUNCTION__, __FILE__,
                          __LINE__, "Unable to read log file.");
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

int
main (int argc, char *argv[])
{
  char *loc_ctype;
  int row = 0, col = 0, o, idx = 0, quit = 0;

#ifdef HAVE_LIBGEOIP
  conf.geo_db = GEOIP_MEMORY_CACHE;
#endif

  conf.iconfigfile = NULL;
  while ((o = getopt_long (argc, argv, short_options, long_opts, &idx)) >= 0) {
    if (-1 == o || EOF == o)
      break;
    switch (o) {
     case 'f':
       conf.ifile = optarg;
       break;
     case 'p':
       conf.iconfigfile = realpath (optarg, NULL);
       if (conf.iconfigfile == NULL)
         error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                        strerror (errno));
       break;
#ifdef HAVE_LIBGEOIP
     case 'g':
       conf.geo_db = GEOIP_STANDARD;
       break;
#endif
     case 'e':
       conf.ignore_host = optarg;
       break;
     case 'a':
       conf.list_agents = 1;
       break;
     case 'c':
       conf.load_conf_dlg = 1;
       break;
     case 'q':
       conf.ignore_qstr = 1;
       break;
     case 'o':
       conf.output_format = optarg;
       break;
     case 'l':
       conf.debug_log = alloc_string (optarg);
       dbg_log_open (conf.debug_log);
       break;
     case 'r':
       conf.skip_term_resolver = 1;
       break;
     case 'd':
       conf.enable_html_resolver = 1;
       break;
     case 'm':
       conf.mouse_support = 1;
       break;
     case 'M':
       conf.append_method = 1;
       break;
     case 'h':
       cmd_help ();
       break;
     case 'H':
       conf.append_protocol = 1;
       break;
     case 0:
       if (!strcmp ("real-os", long_opts[idx].name))
         conf.real_os = 1;
       if (!strcmp ("no-color", long_opts[idx].name))
         conf.no_color = 1;
       /* specifies the maximum number of leaf nodes to be cached */
       if (!strcmp ("cache-lcnum", long_opts[idx].name))
         conf.cache_lcnum = atoi (optarg);
       /* specifies the maximum number of non-leaf nodes to be cached */
       if (!strcmp ("cache-ncnum", long_opts[idx].name))
         conf.cache_ncnum = atoi (optarg);
       /* number of members in each leaf page */
       if (!strcmp ("tune-lmemb", long_opts[idx].name))
         conf.tune_lmemb = atoi (optarg);
       /* number of members in each non-leaf page */
       if (!strcmp ("tune-nmemb", long_opts[idx].name))
         conf.tune_nmemb = atoi (optarg);
       /* number of elements of the bucket array */
       if (!strcmp ("tune-bnum", long_opts[idx].name))
         conf.tune_bnum = atoi (optarg);
       break;
     case '?':
       return EXIT_FAILURE;
     default:
       return EXIT_FAILURE;
    }
  }

  parse_conf_file ();

  if (!isatty (STDOUT_FILENO) || conf.output_format != NULL)
    conf.output_html = 1;
  if (conf.ifile != NULL && !isatty (STDIN_FILENO) && !conf.output_html)
    cmd_help ();
  if (conf.ifile == NULL && isatty (STDIN_FILENO)
      && conf.output_format == NULL)
    cmd_help ();
  for (idx = optind; idx < argc; idx++)
    cmd_help ();

  /* Initialize hash tables */
#ifdef HAVE_LIBTOKYOCABINET

/* *INDENT-OFF* */
  ht_browsers           = tc_db_create (DB_BROWSERS);
  ht_countries          = tc_db_create (DB_COUNTRIES);
  ht_date_bw            = tc_db_create (DB_DATE_BW);
  ht_file_bw            = tc_db_create (DB_FILE_BW);
  ht_file_serve_usecs   = tc_db_create (DB_FILE_SERVE_USECS);
  ht_host_bw            = tc_db_create (DB_HOST_BW);
  ht_hostnames          = tc_db_create (DB_HOSTNAMES);
  ht_hosts_agents       = tc_db_create (DB_HOST_AGENTS);
  ht_host_serve_usecs   = tc_db_create (DB_HOST_SERVE_USECS);
  ht_hosts              = tc_db_create (DB_HOSTS);
  ht_keyphrases         = tc_db_create (DB_KEYPHRASES);
  ht_not_found_requests = tc_db_create (DB_NOT_FOUND_REQUESTS);
  ht_os                 = tc_db_create (DB_OS);
  ht_referrers          = tc_db_create (DB_REFERRERS);
  ht_referring_sites    = tc_db_create (DB_REFERRING_SITES);
  ht_requests_static    = tc_db_create (DB_REQUESTS_STATIC);
  ht_requests           = tc_db_create (DB_REQUESTS);
  ht_status_code        = tc_db_create (DB_STATUS_CODE);
  ht_unique_visitors    = tc_db_create (DB_UNIQUE_VISITORS);
  ht_unique_vis         = tc_db_create (DB_UNIQUE_VIS);
/* *INDENT-ON* */

#else

  ht_unique_visitors =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           (GDestroyNotify) free_key_value, g_free);
  ht_referrers =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           (GDestroyNotify) free_key_value, g_free);
  ht_unique_vis =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           (GDestroyNotify) free_key_value, g_free);
  ht_hosts =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           (GDestroyNotify) free_key_value, g_free);
  ht_status_code =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           (GDestroyNotify) free_key_value, g_free);
  ht_referring_sites =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           (GDestroyNotify) free_key_value, g_free);
  ht_keyphrases =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           (GDestroyNotify) free_key_value, g_free);
  ht_file_bw =
    g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free,
                           g_free);
  ht_host_bw =
    g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free,
                           g_free);
  ht_date_bw =
    g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free,
                           g_free);
  ht_hosts_agents =
    g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free,
                           g_free);
  ht_hostnames =
    g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free,
                           g_free);
  ht_file_serve_usecs =
    g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free,
                           g_free);
  ht_host_serve_usecs =
    g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free,
                           g_free);

  /* The following tables contain s structure as their value, thus we
     use a special iterator to free its value */
  ht_requests =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           (GDestroyNotify) free_key_value, NULL);
  ht_requests_static =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           (GDestroyNotify) free_key_value, NULL);
  ht_not_found_requests =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           (GDestroyNotify) free_key_value, NULL);
  ht_os =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           (GDestroyNotify) free_key_value, NULL);
  ht_browsers =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           (GDestroyNotify) free_key_value, NULL);
  ht_countries =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           (GDestroyNotify) free_key_value, NULL);
#endif

  loc_ctype = getenv ("LC_CTYPE");
  if (loc_ctype != NULL)
    setlocale (LC_CTYPE, loc_ctype);
  else if ((loc_ctype = getenv ("LC_ALL")))
    setlocale (LC_CTYPE, loc_ctype);
  else
    setlocale (LC_CTYPE, "");
  setlocale (LC_NUMERIC, "");

#ifdef HAVE_LIBGEOIP
  /* Geolocation data */
  geo_location_data = GeoIP_new (conf.geo_db);
#endif

  logger = init_log ();
  parsing_spinner = new_gspinner ();
  parsing_spinner->process = &logger->process;

  if (conf.output_html) {
    ui_spinner_create (parsing_spinner);
    goto out;
  }

  initscr ();
  clear ();
  set_input_opts ();

  if (conf.no_color || has_colors () == FALSE) {
    conf.color_scheme = NO_COLOR;
    conf.no_color = 1;
  } else {
    start_color ();
  }
  init_colors ();

  /* init standard screen */
  attron (COLOR_PAIR (COL_WHITE));
  getmaxyx (stdscr, row, col);
  if (row < MIN_HEIGHT || col < MIN_WIDTH)
    error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                   "Minimum screen size - 0 columns by 7 lines");

  /* init header screen */
  header_win = newwin (5, col, 0, 0);
  keypad (header_win, TRUE);
  if (header_win == NULL)
    error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                   "Unable to allocate memory for header_win.");

  /* init main screen */
  main_win = newwin (row - 7, col, 6, 0);
  keypad (main_win, TRUE);
  if (main_win == NULL)
    error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                   "Unable to allocate memory for main_win.");

  set_curses_spinner (parsing_spinner);

  /* configuration dialog */
  if (isatty (STDIN_FILENO) && (conf.log_format == NULL || conf.load_conf_dlg)
      && !conf.output_html) {
    refresh ();
    quit = verify_format (logger, parsing_spinner);
  }
  /* straight parsing */
  else if (!conf.output_html) {
    ui_spinner_create (parsing_spinner);
  }

out:

  /* main processing event */
  time (&start_proc);
  if (!quit && parse_log (&logger, NULL, -1))
    error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                   "Error while processing file");
  time (&end_proc);

  gdns_init ();
  logger->offset = logger->process;

  /* STDOUT */
  if (conf.output_html) {
    /* no valid entries to process from the log */
    if ((logger->process == 0) || (logger->process == logger->invalid))
      goto done;

    allocate_holder ();
    /* csv */
    if (conf.output_format != NULL && strcmp ("csv", conf.output_format) == 0)
      output_csv (logger, holder);
    /* json */
    else if (conf.output_format != NULL
             && strcmp ("json", conf.output_format) == 0)
      output_json (logger, holder);
    /* HTML */
    else
      output_html (logger, holder);
    goto done;
  }

  pthread_mutex_lock (&parsing_spinner->mutex);
  werase (parsing_spinner->win);
  parsing_spinner->state = SPN_END;
  pthread_mutex_unlock (&parsing_spinner->mutex);

  allocate_holder ();
  allocate_data ();

  if (!conf.skip_term_resolver) {
    active_gdns = 1;
    gdns_thread_create ();
  }

  render_screens ();
  get_keys ();

  attroff (COLOR_PAIR (COL_WHITE));

  /* restore tty modes and reset */
  /* terminal into non-visual mode */
  endwin ();
done:
  write_conf_file ();
  house_keeping ();

  return EXIT_SUCCESS;
}
