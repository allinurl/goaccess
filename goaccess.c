/**
 * goaccess.c -- main log analyzer
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

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS    64

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <glib.h>
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

#include "commons.h"
#include "error.h"
#include "gdns.h"
#include "gdashboard.h"
#include "output.h"
#include "parser.h"
#include "settings.h"
#include "ui.h"
#include "util.h"

static WINDOW *header_win, *main_win;
#ifdef HAVE_LIBGEOIP
static char short_options[] = "f:e:p:o:acrmg";
#else
static char short_options[] = "f:e:p:o:acrm";
#endif

GConf conf = { 0 };

int active_gdns = 0;
static GDash *dash;
static GHolder *holder;
static GLog *logger;
static GSpinner *parsing_spinner;

/* *INDENT-OFF* */
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

#ifdef HAVE_LIBGEOIP
static void
free_countries (GO_UNUSED gpointer old_key, gpointer old_value,
                GO_UNUSED gpointer user_data)
{
   GLocation *loc = old_value;
   free (loc);
}
#endif

/* free memory allocated by g_hash_table_new_full() function */
static void
free_key_value (gpointer old_key, GO_UNUSED gpointer old_value,
                GO_UNUSED gpointer user_data)
{
   g_free (old_key);
}

static void
cmd_help (void)
{
   printf ("\nGoAccess - %s\n\n", GO_VERSION);
   printf ("Usage: ");
   printf
      ("goaccess -f log_file [-c][-r][-m][-g][-a][-o csv|json][-e IP_ADDRESS][-p CONFFILE]\n\n");
   printf ("The following options can also be supplied to the command:\n\n");
   printf (" -f <argument> - Path to input log file.\n");
   printf (" -c            - Prompt log/date configuration window.\n");
   printf (" -r            - Disable IP resolver.\n");
   printf (" -m            - Enable mouse support on main dashboard.\n");
   printf (" -g            - Standard GeoIP database for less memory usage.\n");
   printf (" -a            - Enable a list of User-Agents by host.\n");
   printf ("                 For faster parsing, don't enable this flag.\n");
   printf (" -e <argument> - Exclude an IP from being counted under the\n");
   printf ("                 HOST module. Disabled by default.\n");
   printf
      (" -o <argument> - Output format. '-o csv' for CSV, '-o json' for JSON.\n");
   printf (" -p <argument> - Custom configuration file.\n\n");
   printf ("Examples can be found by running `man goaccess`.\n\n");
   printf ("For more details visit: http://goaccess.prosoftcorp.com\n");
   printf ("GoAccess Copyright (C) 2009-2013 GNU GPL'd, by Gerardo Orellana");
   printf ("\n\n");
   exit (EXIT_FAILURE);
}

static void
house_keeping (void)
{
   pthread_mutex_lock (&gdns_thread.mutex);
   active_gdns = 0;             /* kill dns pthread */
   free_holder (&holder);
   gdns_free_queue ();
   g_hash_table_destroy (ht_hostnames);
   pthread_mutex_unlock (&gdns_thread.mutex);

   if (!conf.output_html) {
      free_dashboard (dash);
      reset_find ();
   }
#ifdef HAVE_LIBGEOIP
   if (geo_location_data != NULL)
      GeoIP_delete (geo_location_data);
   g_hash_table_foreach (ht_countries, free_countries, NULL);
#endif

   free (logger);

   g_hash_table_destroy (ht_browsers);
   g_hash_table_destroy (ht_countries);
   g_hash_table_destroy (ht_date_bw);
   g_hash_table_destroy (ht_file_bw);
   g_hash_table_destroy (ht_host_bw);
   g_hash_table_destroy (ht_hosts);
   g_hash_table_destroy (ht_keyphrases);
   g_hash_table_destroy (ht_monthly);
   g_hash_table_destroy (ht_not_found_requests);
   g_hash_table_destroy (ht_os);
   g_hash_table_destroy (ht_referrers);
   g_hash_table_destroy (ht_referring_sites);
   g_hash_table_destroy (ht_requests);
   g_hash_table_destroy (ht_requests_static);
   g_hash_table_destroy (ht_status_code);
   g_hash_table_destroy (ht_unique_vis);
   g_hash_table_destroy (ht_unique_visitors);
}

/* allocate memory for an instance of holder */
static void
allocate_holder_by_module (GModule module)
{
   GHashTable *ht;
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
   GHashTable *ht;
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

      dash->module[module].alloc_data = size;   /* data allocated  */
      dash->module[module].ht_size = ht_size;   /* hash table size */
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

   draw_header (stdscr, "", 0, row - 1, col, 0);
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
       case 'q':               /* quit */
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
       case 49:                /* 1 */
          /* reset expanded module */
          set_module_to (&scrolling, VISITORS);
          break;
       case 50:                /* 2 */
          /* reset expanded module */
          set_module_to (&scrolling, REQUESTS);
          break;
       case 51:                /* 3 */
          /* reset expanded module */
          set_module_to (&scrolling, REQUESTS_STATIC);
          break;
       case 52:                /* 4 */
          /* reset expanded module */
          set_module_to (&scrolling, NOT_FOUND);
          break;
       case 53:                /* 5 */
          /* reset expanded module */
          set_module_to (&scrolling, HOSTS);
          break;
       case 54:                /* 6 */
          /* reset expanded module */
          set_module_to (&scrolling, OS);
          break;
       case 55:                /* 7 */
          /* reset expanded module */
          set_module_to (&scrolling, BROWSERS);
          break;
       case 56:                /* 8 */
          /* reset expanded module */
          set_module_to (&scrolling, REFERRERS);
          break;
       case 57:                /* 9 */
          /* reset expanded module */
          set_module_to (&scrolling, REFERRING_SITES);
          break;
       case 48:                /* 0 */
          /* reset expanded module */
          set_module_to (&scrolling, KEYPHRASES);
          break;
       case 33:                /* Shift+1 */
          /* reset expanded module */
#ifdef HAVE_LIBGEOIP
          set_module_to (&scrolling, GEO_LOCATION);
#else
          set_module_to (&scrolling, STATUS_CODES);
#endif
          break;
#ifdef HAVE_LIBGEOIP
       case 64:                /* Shift+2 */
          /* reset expanded module */
          set_module_to (&scrolling, STATUS_CODES);
          break;
#endif
       case 9:                 /* TAB */
          /* reset expanded module */
          collapse_current_module ();
          scrolling.current++;
          if (scrolling.current == TOTAL_MODULES)
             scrolling.current = 0;
          render_screens ();
          break;
       case 353:               /* Shift TAB */
          /* reset expanded module */
          collapse_current_module ();
          if (scrolling.current == 0)
             scrolling.current = TOTAL_MODULES - 1;
          else
             scrolling.current--;
          render_screens ();
          break;
       case 'g':               /* g = top */
          if (!scrolling.expanded)
             scrolling.dash = 0;
          else {
             scrolling.module[scrolling.current].scroll = 0;
             scrolling.module[scrolling.current].offset = 0;
          }
          display_content (main_win, logger, dash, &scrolling);
          break;
       case 'G':               /* G = down */
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
       case 32:                /* ENTER */
       case 79:                /* o */
       case 111:               /* O */
       case KEY_ENTER:
          if (scrolling.expanded && scrolling.current == HOSTS) {
             /* make sure we have a valid IP */
             int sel = scrolling.module[scrolling.current].scroll;
             if (!invalid_ipaddr (dash->module[HOSTS].data[sel].data))
                load_agent_list (main_win, dash->module[HOSTS].data[sel].data);
             break;
          }
          reset_scroll_offsets (&scrolling);
          scrolling.expanded = 1;

          free_holder_by_module (&holder, scrolling.current);
          free_dashboard (dash);
          allocate_holder_by_module (scrolling.current);
          allocate_data ();

          display_content (main_win, logger, dash, &scrolling);
          break;
       case KEY_DOWN:          /* scroll main dashboard */
          if ((scrolling.dash + real_size_y) < (unsigned) dash->total_alloc) {
             scrolling.dash++;
             display_content (main_win, logger, dash, &scrolling);
          }
          break;
       case KEY_MOUSE:         /* handles mouse events */
          ok_mouse = getmouse (&event);
          if (conf.mouse_support && ok_mouse == OK) {
             if (event.bstate & BUTTON1_CLICKED) {
                /* ignore header/footer clicks */
                if (event.y < MAX_HEIGHT_HEADER || event.y == LINES - 1)
                   break;

                set_module_from_mouse_event (&scrolling, dash, event.y);
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
       case 106:               /* j - DOWN expanded module */
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
       case 2:                 /* ^ b - page up */
       case 339:               /* ^ PG UP */
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
       case 6:                 /* ^ f - page down */
       case 338:               /* ^ PG DOWN */
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
       case 107:               /* k - UP expanded module */
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
       case 99:                /* c */
          load_schemes_win (main_win);
          free_dashboard (dash);
          allocate_data ();
          render_screens ();
          break;
       case 115:               /* s */
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
             usleep (200000);   /* 0.2 seconds */
          }
          break;
      }
   }
}

int
main (int argc, char *argv[])
{
   char *loc_ctype;
   int row = 0, col = 0, o, idx, quit = 0;

#ifdef HAVE_LIBGEOIP
   conf.geo_db = GEOIP_MEMORY_CACHE;
#endif

   opterr = 0;
   while ((o = getopt (argc, argv, short_options)) != -1) {
      switch (o) {
       case 'f':
          conf.ifile = optarg;
          break;
       case 'p':
          if (realpath (optarg, conf.iconfigfile) == 0)
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
       case 'o':
          conf.output_format = optarg;
          break;
       case 'r':
          conf.skip_resolver = 1;
          break;
       case 'm':
          conf.mouse_support = 1;
          break;
       case '?':
          if (optopt == 'f' || optopt == 'e' || optopt == 'p' || optopt == 'o')
             fprintf (stderr, "Option -%c requires an argument.\n", optopt);
          else if (isprint (optopt))
             fprintf (stderr, "Unknown option `-%c'.\n", optopt);
          else
             fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
          return EXIT_FAILURE;
       default:
          abort ();
      }
   }
   if (!isatty (STDOUT_FILENO))
      conf.output_html = 1;
   if (conf.ifile != NULL && !isatty (STDIN_FILENO))
      cmd_help ();
   if (conf.ifile == NULL && isatty (STDIN_FILENO))
      cmd_help ();
   for (idx = optind; idx < argc; idx++)
      cmd_help ();

   /*
    * initialize hash tables
    */
   ht_unique_visitors =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) free_key_value, NULL);
   ht_requests =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) free_key_value, NULL);
   ht_referrers =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) free_key_value, NULL);
   ht_unique_vis =
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
   ht_hosts =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) free_key_value, NULL);
   ht_status_code =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) free_key_value, NULL);
   ht_referring_sites =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) free_key_value, NULL);
   ht_keyphrases =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) free_key_value, NULL);
   ht_countries =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) free_key_value, NULL);
   ht_file_bw =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) g_free, g_free);
   ht_host_bw =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) g_free, g_free);
   ht_date_bw =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) g_free, g_free);
   ht_monthly =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) g_free, g_free);
   ht_hosts_agents =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) g_free, g_free);
   ht_hostnames =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) g_free, g_free);
   ht_file_serve_usecs =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) g_free, g_free);
   ht_host_serve_usecs =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) g_free, g_free);

   loc_ctype = getenv ("LC_CTYPE");
   if (loc_ctype != NULL)
      setlocale (LC_CTYPE, loc_ctype);
   else if ((loc_ctype = getenv ("LC_ALL")))
      setlocale (LC_CTYPE, loc_ctype);
   else
      setlocale (LC_CTYPE, "");

#ifdef HAVE_LIBGEOIP
   /* Geolocation data */
   geo_location_data = GeoIP_new (conf.geo_db);
#endif

   parse_conf_file ();

   if (conf.output_html)
      goto out;

   initscr ();
   clear ();
   if (has_colors () == FALSE)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Your terminal does not support color");
   set_input_opts ();
   start_color ();
   init_colors ();

   attron (COLOR_PAIR (COL_WHITE));
   getmaxyx (stdscr, row, col);
   if (row < MIN_HEIGHT || col < MIN_WIDTH)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Minimum screen size - 0 columns by 7 lines");

   header_win = newwin (5, col, 0, 0);
   keypad (header_win, TRUE);
   if (header_win == NULL)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Unable to allocate memory for header_win.");

   main_win = newwin (row - 7, col, 6, 0);
   keypad (main_win, TRUE);
   if (main_win == NULL)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Unable to allocate memory for main_win.");

   parsing_spinner = new_gspinner ();
 out:

   logger = init_log ();
   if (isatty (STDIN_FILENO) && (conf.log_format == NULL || conf.load_conf_dlg)
       && !conf.output_html) {
      refresh ();
      quit = verify_format (logger, parsing_spinner);
   } else if (!conf.output_html) {
      draw_header (stdscr, "Parsing...", 0, row - 1, col, HIGHLIGHT);
      ui_spinner_create (parsing_spinner);
   }

   /* main processing event */
   time (&start_proc);
   if (!quit && parse_log (&logger, NULL, -1))
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Error while processing file");
   time (&end_proc);

   gdns_init ();
   logger->offset = logger->process;
   if (conf.output_html) {
      /* no valid entries to process from the log */
      if ((logger->process == 0) || (logger->process == logger->invalid))
         goto done;
      allocate_holder ();
      if (conf.output_format != NULL && strcmp ("csv", conf.output_format) == 0)
         output_csv (logger, holder);
      else if (conf.output_format != NULL &&
               strcmp ("json", conf.output_format) == 0)
         output_json (logger, holder);
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

   if (!conf.skip_resolver) {
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
