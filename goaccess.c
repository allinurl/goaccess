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
static char short_options[] = "f:e:p:o:acr"; 

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
   {STATUS_CODES    , SORT_BY_HITS, SORT_DESC}, 
   {COUNTRIES       , SORT_BY_HITS, SORT_DESC}, 
   {CONTINENTS      , SORT_BY_HITS, SORT_DESC}, 
};

static GScrolling scrolling = {
   {
      {0, 0}, /* visitors   {scroll, offset} */
      {0, 0}, /* requests   {scroll, offset} */
      {0, 0}, /* req static {scroll, offset} */
      {0, 0}, /* not found  {scroll, offset} */ 
      {0, 0}, /* hosts      {scroll, offset} */ 
      {0, 0}, /* os         {scroll, offset} */ 
      {0, 0}, /* browsers   {scroll, offset} */ 
      {0, 0}, /* status     {scroll, offset} */ 
      {0, 0}, /* referrers  {scroll, offset} */ 
      {0, 0}, /* ref sites  {scroll, offset} */ 
      {0, 0}  /* keywords   {scroll, offset} */
   },
   0,         /* current module */
   0,         /* main dashboard scroll */
   0,         /* expanded flag */
};
/* *INDENT-ON* */
/* frees the memory allocated by the GHashTable */
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
   printf ("goaccess [-e IP_ADDRESS][-a][-r][-c][-o csv][-p CONFFILE] -f log_file\n\n");
   printf ("The following options can also be supplied to the command:\n\n");
   printf (" -f <argument> - Path to input log file.\n");
   printf (" -c            - Prompt log/date configuration window.\n");
   printf (" -r            - Disable IP resolver.\n");
   printf (" -a            - Enable a List of User-Agents by host.\n");
   printf ("                 For faster parsing, don't enable this flag.\n");
   printf (" -e <argument> - Exclude an IP from being counted under the\n");
   printf ("                 HOST module. Disabled by default.\n");
   printf (" -o <argument> - Output format. '-o csv' for CSV.\n");
   printf (" -p <argument> - Custom configuration file.\n\n");
   printf ("Examples can be found by running `man goaccess`.\n\n");
   printf ("For more details visit: http://goaccess.prosoftcorp.com\n");
   printf ("GoAccess Copyright (C) 2009-2013 GNU GPL'd, by Gerardo Orellana");
   printf ("\n\n");
   exit (EXIT_FAILURE);
}

static void
house_keeping (GLog * logger, GDash * dash)
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

   free (logger);
   g_hash_table_destroy (ht_browsers);
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
   g_hash_table_destroy (ht_countries);
   g_hash_table_destroy (ht_continents);
   g_hash_table_destroy (ht_unique_vis);
   g_hash_table_destroy (ht_unique_visitors);
}

/* allocate memory for an instance of holder */
static void
allocate_holder (void)
{
   int ht_size = 0;

   int i;
   GHashTable *ht;
   GModule module;
   GRawData *raw_data;

   holder = new_gholder (TOTAL_MODULES);
   for (i = 0; i < TOTAL_MODULES; i++) {
      module = i;
      switch (module) {
       case VISITORS:
          ht = ht_unique_vis;
          break;
       case REQUESTS:
          ht = ht_requests;
          break;
       case REQUESTS_STATIC:
          ht = ht_requests_static;
          break;
       case NOT_FOUND:
          ht = ht_not_found_requests;
          break;
       case HOSTS:
          ht = ht_hosts;
          break;
       case OS:
          ht = ht_os;
          break;
       case BROWSERS:
          ht = ht_browsers;
          break;
       case REFERRERS:
          ht = ht_referrers;
          break;
       case REFERRING_SITES:
          ht = ht_referring_sites;
          break;
       case KEYPHRASES:
          ht = ht_keyphrases;
          break;
       case STATUS_CODES:
          ht = ht_status_code;
          break;
       case COUNTRIES:
          ht = ht_countries;
          break;
       case CONTINENTS:
          ht = ht_continents;
          break;
      }

      /* extract data from the corresponding hash table */
      ht_size = g_hash_table_size (ht);
      raw_data = parse_raw_data (ht, ht_size, module);
      load_data_to_holder (raw_data, holder + module, module, sort[module]);
   }
}

/* allocate memory for an instance of dashboard */
static void
allocate_data ()
{
   int col_data = DASH_COLLAPSED - DASH_NON_DATA;
   int size = 0, ht_size = 0;

   int i;
   GHashTable *ht;
   GModule module;

   dash = new_gdash ();
   for (i = 0; i < TOTAL_MODULES; i++) {
      module = i;

      switch (module) {
       case VISITORS:
          ht = ht_unique_vis;
          dash->module[module].head = VISIT_HEAD;
          dash->module[module].desc = VISIT_DESC;
          break;
       case REQUESTS:
          ht = ht_requests;
          dash->module[module].head = REQUE_HEAD;
          dash->module[module].desc = REQUE_DESC;
          break;
       case REQUESTS_STATIC:
          ht = ht_requests_static;
          dash->module[module].head = STATI_HEAD;
          dash->module[module].desc = STATI_DESC;
          break;
       case NOT_FOUND:
          ht = ht_not_found_requests;
          dash->module[module].head = FOUND_HEAD;
          dash->module[module].desc = FOUND_DESC;
          break;
       case HOSTS:
          ht = ht_hosts;
          dash->module[module].head = HOSTS_HEAD;
          dash->module[module].desc = HOSTS_DESC;
          break;
       case OS:
          ht = ht_os;
          dash->module[module].head = OPERA_HEAD;
          dash->module[module].desc = OPERA_DESC;
          break;
       case BROWSERS:
          ht = ht_browsers;
          dash->module[module].head = BROWS_HEAD;
          dash->module[module].desc = BROWS_DESC;
          break;
       case REFERRERS:
          ht = ht_referrers;
          dash->module[module].head = REFER_HEAD;
          dash->module[module].desc = REFER_DESC;
          break;
       case REFERRING_SITES:
          ht = ht_referring_sites;
          dash->module[module].head = SITES_HEAD;
          dash->module[module].desc = SITES_DESC;
          break;
       case KEYPHRASES:
          ht = ht_keyphrases;
          dash->module[module].head = KEYPH_HEAD;
          dash->module[module].desc = KEYPH_DESC;
          break;
       case STATUS_CODES:
          ht = ht_status_code;
          dash->module[module].head = CODES_HEAD;
          dash->module[module].desc = CODES_DESC;
          break;
       case COUNTRIES:
          ht = ht_countries;
          dash->module[module].head = CODES_HEAD;
          dash->module[module].desc = CODES_DESC;
       case CONTINENTS:
          ht = ht_continents;
          dash->module[module].head = CODES_HEAD;
          dash->module[module].desc = CODES_DESC;
          break;
      }

      ht_size = g_hash_table_size (ht);
      size = ht_size > col_data ? col_data : ht_size;
      if (scrolling.expanded && module == scrolling.current)
         size = MAX_CHOICES;

      dash->module[module].alloc_data = size;   /* data allocated  */
      dash->module[module].ht_size = ht_size;   /* hash table size */
      dash->module[module].idx_data = 0;

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
void
render_screens (GLog * logger)
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
      display_content (main_win, logger, dash, &scrolling);
   }
}

static void
get_keys (GLog * logger)
{
   int search;
   int c, quit = 1, scroll, offset;
   int *scroll_ptr, *offset_ptr;
   int exp_size = DASH_EXPANDED - DASH_NON_DATA;

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
          scrolling.module[scrolling.current].scroll = 0;
          scrolling.module[scrolling.current].offset = 0;
          scrolling.expanded = 0;
          free_dashboard (dash);
          allocate_data ();
          display_content (main_win, logger, dash, &scrolling);
          break;
       case 8:
       case 265:
          load_help_popup (main_win);
          break;
       case 9:                 /* TAB */
          /* reset expanded module */
          collapse_current_module ();
          scrolling.current++;
          if (scrolling.current == TOTAL_MODULES)
             scrolling.current = 0;
          render_screens (logger);
          break;
       case 353:               /* Shift TAB */
          /* reset expanded module */
          collapse_current_module ();
          if (scrolling.current == 0)
             scrolling.current = TOTAL_MODULES - 1;
          else
             scrolling.current--;
          render_screens (logger);
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
             scroll = offset = 0;
             scroll = dash->module[scrolling.current].idx_data - 1;
             if (scroll >= exp_size && scroll >= offset + exp_size)
                offset = scroll < exp_size - 1 ? 0 : scroll - exp_size + 1;
             scrolling.module[scrolling.current].scroll = scroll;
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

          free_holder (&holder);
          free_dashboard (dash);
          allocate_holder ();
          allocate_data ();

          display_content (main_win, logger, dash, &scrolling);
          break;
       case KEY_DOWN:
          if ((scrolling.dash + real_size_y) < (unsigned) dash->total_alloc) {
             scrolling.dash++;
             display_content (main_win, logger, dash, &scrolling);
          }
          break;
       case 106:               /* j - DOWN expanded module */
       case 519:               /* ^ DOWN - xterm-256color  */
       case 527:               /* ^ DOWN - gnome-terminal  */
       case 66:                /* ^ DOWN - screen-256color */
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
       case 560:               /* ^ UP - xterm-256color  */
       case 568:               /* ^ UP - gnome-terminal  */
       case 65:                /* ^ UP - screen-256color */
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
             render_screens (logger);
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
             render_screens (logger);
          }
          break;
       case 99:                /* c */
          load_schemes_win (main_win);
          free_dashboard (dash);
          allocate_data ();
          render_screens (logger);
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
          render_screens (logger);
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
          render_screens (logger);
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
             render_screens (logger);
             usleep (200000);   /* 0.2 seconds */
          }
          break;
      }
   }
}

int
main (int argc, char *argv[])
{
   extern char *optarg;
   extern int optind, optopt, opterr;
   int row, col, o, index, quit = 0;

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
   for (index = optind; index < argc; index++)
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
   ht_countries =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) free_key_value, NULL);
   ht_continents =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) free_key_value, NULL);
   ht_referring_sites =
      g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) free_key_value, NULL);
   ht_keyphrases =
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

   char *loc_ctype = getenv ("LC_CTYPE");
   if (loc_ctype != NULL)
      setlocale (LC_CTYPE, loc_ctype);
   else if ((loc_ctype = getenv ("LC_ALL")))
      setlocale (LC_CTYPE, loc_ctype);
   else
      setlocale (LC_CTYPE, "");

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
      if( conf.output_format != NULL && *conf.output_format == 'c' )
        output_csv (logger, holder);    
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

   render_screens (logger);
   get_keys (logger);

   attroff (COLOR_PAIR (COL_WHITE));

   /* restore tty modes and reset */
   /* terminal into non-visual mode */
   endwin ();
 done:
   write_conf_file ();
   house_keeping (logger, dash);

   return EXIT_SUCCESS;
}
