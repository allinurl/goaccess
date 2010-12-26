/** 
 * goaccess.c -- main log analyzer 
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
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
#define _FILE_OFFSET_BITS 64

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <glib.h>
#include <locale.h>

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_LIBNCURSESW
#  include <ncursesw/menu.h>
#  include <ncursesw/curses.h>
#else
#  include <ncurses.h>
#  include <menu.h>
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "alloc.h"
#include "commons.h"
#include "error.h"
#include "parser.h"
#include "settings.h"
#include "ui.h"
#include "util.h"

static WINDOW *header_win, *main_win, *my_menu_win, *help_win, *schemes_win;

static int initial_reqs = 0;
static struct logger *logger;
static struct scrolling scrolling;
static struct struct_display **s_display;
static struct struct_holder **s_holder;

/* frees the memory allocated by the GHashTable */
static void
free_key_value (gpointer old_key, GO_UNUSED gpointer old_value,
                GO_UNUSED gpointer user_data)
{
   g_free (old_key);
}

static struct logger *
init_struct (void)
{
   struct logger *logger;

   if ((logger = malloc (sizeof (*logger))) == NULL)
      return NULL;

   logger->alloc_counter = 0;
   logger->counter = 0;
   logger->current_module = 1;
   logger->total_invalid = 0;
   logger->total_process = 0;

   return logger;
}

static void
cmd_help (void)
{
   printf ("\nGoAccess - %s\n\n", GO_VERSION);
   printf ("Usage: ");
   printf ("goaccess [ -b ][ -s ][ -e IP_ADDRESS][ - a ]< -f log_file >\n\n");
   printf ("The following options can also be supplied to the command:\n\n");
   printf (" -f <argument> - Path to input log file.\n");
   printf (" -b            - Enable total bandwidth consumption.\n");
   printf ("                 For faster parsing, don't enable this flag.\n");
   printf (" -s            - Enable HTTP status codes report.\n");
   printf ("                 For faster parsing, don't enable this flag.\n");
   printf (" -a            - Enable a List of User-Agents by host.\n");
   printf ("                 For faster parsing, don't enable this flag.\n");
   printf (" -e <argument> - Exclude an IP from being counted under the\n");
   printf ("                 HOST module. Disabled by default.\n\n");
   printf ("Examples can be found by running `man goaccess`.\n\n");
   printf ("For more details visit: http://goaccess.prosoftcorp.com\n");
   printf ("GoAccess Copyright (C) 2009-2010 GNU GPL'd, by Gerardo Orellana");
   printf ("\n\n");
   exit (EXIT_FAILURE);
}

static void
house_keeping (struct logger *logger, struct struct_display **s_display)
{
   int f;
   for (f = 0; f < logger->alloc_counter; f++)
      free (s_display[f]->data);

   for (f = 0; f < 170; f++)
      free (s_display[f]);

   assert (s_display != 0);
   free (s_display);
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
   g_hash_table_destroy (ht_unique_vis);
   g_hash_table_destroy (ht_unique_visitors);
}

void
allocate_structs (int free_me)
{
   if (free_me) {
      int f;
      for (f = 0; f < logger->alloc_counter; f++)
         free (s_display[f]->data);

      for (f = 0; f < 170; f++)
         free (s_display[f]);

      assert (s_display != 0);
      free (s_display);

      logger->counter = 0;
      logger->alloc_counter = 0;
   }

   /* allocate 10 per module */
   MALLOC_STRUCT (s_display, 170);

   /* note that the order in which we call them, 
    * is the way modules will be displayed */
   generate_unique_visitors (s_display, logger);

   MALLOC_STRUCT (s_holder, g_hash_table_size (ht_requests));
   generate_struct_data (ht_requests, s_holder, s_display, logger, 2);

   MALLOC_STRUCT (s_holder, g_hash_table_size (ht_requests_static));
   generate_struct_data (ht_requests_static, s_holder, s_display, logger, 3);

   MALLOC_STRUCT (s_holder, g_hash_table_size (ht_referrers));
   generate_struct_data (ht_referrers, s_holder, s_display, logger, 4);

   MALLOC_STRUCT (s_holder, g_hash_table_size (ht_not_found_requests));
   generate_struct_data (ht_not_found_requests, s_holder, s_display, logger,
                         5);

   MALLOC_STRUCT (s_holder, g_hash_table_size (ht_os));
   generate_struct_data (ht_os, s_holder, s_display, logger, 6);

   MALLOC_STRUCT (s_holder, g_hash_table_size (ht_browsers));
   generate_struct_data (ht_browsers, s_holder, s_display, logger, 7);

   MALLOC_STRUCT (s_holder, g_hash_table_size (ht_hosts));
   generate_struct_data (ht_hosts, s_holder, s_display, logger, 8);

   if (!http_status_code_flag)
      goto nohttpstatuscode;

   MALLOC_STRUCT (s_holder, g_hash_table_size (ht_status_code));
   generate_struct_data (ht_status_code, s_holder, s_display, logger, 9);

 nohttpstatuscode:;

   MALLOC_STRUCT (s_holder, g_hash_table_size (ht_referring_sites));
   generate_struct_data (ht_referring_sites, s_holder, s_display, logger, 10);

   MALLOC_STRUCT (s_holder, g_hash_table_size (ht_keyphrases));
   generate_struct_data (ht_keyphrases, s_holder, s_display, logger, 11);
}

void
render_screens (void)
{
   int row, col, chg = 0;

   werase (main_win);
   getmaxyx (stdscr, row, col);
   term_size (main_win);

   wattron (stdscr, COLOR_PAIR (COL_WHITE));
   generate_time ();
   chg = logger->total_process - initial_reqs;
   mvaddstr (row - 1, 1, "[F1]Help [O]pen detail view");
   mvprintw (row - 1, 30, "%d - %s", chg, asctime (now_tm));
   mvaddstr (row - 1, col - 21, "[Q]uit Analyzer");

   mvprintw (row - 1, col - 5, "%s", GO_VERSION);
   wattroff (stdscr, COLOR_PAIR (COL_WHITE));
   display_content (main_win, s_display, logger, scrolling);

   refresh ();
   /* call general stats header */
   display_general (header_win, logger, ifile);
   wrefresh (header_win);
   wrefresh (main_win);

   /* display active label based on current module */
   wattron (header_win, COLOR_PAIR (BLUE_GREEN));
   wmove (header_win, 0, 30);
   mvwprintw (header_win, 0, col - 19, "[Active Module %d]",
              logger->current_module);
   wattroff (header_win, COLOR_PAIR (BLUE_GREEN));
   wrefresh (header_win);
   /* no valid entries to process from the log */
   if ((logger->total_process == 0)
       || (logger->total_process == logger->total_invalid))
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Nothing valid to process.");
}

static void
get_keys (void)
{
   int y, x, c;

   char buf[BUFFER];
   FILE *fp = NULL;
   unsigned long long size1 = 0, size2 = 0;

   if (!piping)
      size1 = file_size (ifile);
   while (((c = wgetch (stdscr)) != 'q')) {
      getmaxyx (main_win, y, x);
      switch (c) {
          /* scroll down main_win */
       case KEY_DOWN:
          wmove (main_win, real_size_y - 1, 0);
          do_scrolling (main_win, s_display, logger, &scrolling, 1);
          break;
          /* scroll up main_win */
       case KEY_UP:
          wmove (main_win, 0, 0);
          do_scrolling (main_win, s_display, logger, &scrolling, 0);
          break;
          /* tab - increment module counter */
       case 9:
          if (logger->current_module < TOTAL_MODULES)
             logger->current_module++;
          else if (logger->current_module == TOTAL_MODULES)
             logger->current_module = 1;
          update_header (header_win, logger->current_module);
          break;
          /* tab - increment module counter */
       case 353:
          if (logger->current_module > 1
              && logger->current_module <= TOTAL_MODULES)
             logger->current_module--;
          else if (logger->current_module == 1)
             logger->current_module = TOTAL_MODULES;
          update_header (header_win, logger->current_module);
          break;
       case KEY_RIGHT:
       case 'o':
       case 'O':
          if ((x < 75) || (y < 12))
             break;             /* stdscr too small to create subwin */
          my_menu_win = create_win (main_win);
          load_popup (my_menu_win, s_holder, logger);
          touchwin (main_win);
          close_win (my_menu_win);
          break;
          /* key 1 - module 1 */
       case 49:
          logger->current_module = 1;
          update_header (header_win, logger->current_module);
          break;
          /* key 2 - module 2 */
       case 50:
          logger->current_module = 2;
          update_header (header_win, logger->current_module);
          break;
          /* key 3 - module 3 */
       case 51:
          logger->current_module = 3;
          update_header (header_win, logger->current_module);
          break;
          /* key 4 - module 4 */
       case 52:
          logger->current_module = 4;
          update_header (header_win, logger->current_module);
          break;
          /* key 5 - module 5 */
       case 53:
          logger->current_module = 5;
          update_header (header_win, logger->current_module);
          break;
          /* key 6 - module 6 */
       case 54:
          logger->current_module = 6;
          update_header (header_win, logger->current_module);
          break;
          /* key 7 - module 7 */
       case 55:
          logger->current_module = 7;
          update_header (header_win, logger->current_module);
          break;
          /* key 8 - module 8 */
       case 56:
          logger->current_module = 8;
          update_header (header_win, logger->current_module);
          break;
          /* key 9 - module 9 */
       case 57:
          if (http_status_code_flag)
             logger->current_module = 9;
          update_header (header_win, logger->current_module);
          break;
          /* key 0 - module 10 */
       case 48:
          logger->current_module = 10;
          update_header (header_win, logger->current_module);
          break;
          /* key ^SHIFT^ + 1 - module 11 */
       case 33:
          logger->current_module = 11;
          update_header (header_win, logger->current_module);
          break;
       case 8:
       case 265:
          if ((x < 65) || (y < 12))
             break;             /* stdscr too small to create win */
          help_win = newwin (y - 3, 57, 5, ((x - 57) / 2));
          if (help_win == NULL)
             error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                            "Unable to allocate memory for new window.");
          load_help_popup (help_win, ((x - 57) / 2));
          wrefresh (help_win);
          touchwin (main_win);
          close_win (help_win);
          break;
       case 99:
          if ((x < 75) || (y < 12))
             break;             /* stdscr too small to create win */
          schemes_win = newwin (7, 57, 8, ((x - 57) / 2));
          if (schemes_win == NULL)
             error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                            "Unable to allocate memory for new window.");
          load_schemes_win (schemes_win, ((x - 57) / 2));
          wrefresh (schemes_win);
          touchwin (main_win);
          close_win (schemes_win);
          break;
       case 269:
       case KEY_RESIZE:
          endwin ();
          refresh ();
          werase (header_win);
          werase (main_win);
          werase (stdscr);
          term_size (main_win);
          scrolling.scrl_main_win = real_size_y;
          refresh ();
          render_screens ();
          break;
       default:
          if (piping)
             break;
          size2 = file_size (ifile);
          /* file has changed */
          if (size2 != size1) {
             if (!(fp = fopen (ifile, "r")))
                error_handler (__PRETTY_FUNCTION__, __FILE__,
                               __LINE__, "Unable to read log file.");
             if (!fseeko (fp, size1, SEEK_SET))
                while (fgets (buf, BUFFER, fp) != NULL)
                   parse_log (logger, ifile, buf);
             fclose (fp);
             size1 = size2;
             allocate_structs (1);
             term_size (main_win);
             scrolling.scrl_main_win = real_size_y;
             refresh ();
             render_screens ();
             usleep (200000);   /* 0.2 seconds */
          }
          break;
      }
      wrefresh (main_win);
   }
}

int
main (int argc, char *argv[])
{
   extern char *optarg;
   extern int optind, optopt, opterr;
   int row, col, o, index;

   opterr = 0;
   while ((o = getopt (argc, argv, "f:e:sba")) != -1) {
      switch (o) {
       case 'f':
          ifile = optarg;
          break;
       case 'e':
          ignore_host = optarg;
          break;
       case 's':
          http_status_code_flag = 1;
          break;
       case 'b':
          bandwidth_flag = 1;
          break;
       case 'a':
          host_agents_list_flag = 1;
          break;
       case '?':
          if (optopt == 'f' || optopt == 'e')
             fprintf (stderr, "Option -%c requires an argument.\n", optopt);
          else if (isprint (optopt))
             fprintf (stderr, "Unknown option `-%c'.\n", optopt);
          else
             fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
          return 1;
       default:
          abort ();
      }
   }
   if (ifile != NULL && !isatty (STDIN_FILENO))
      cmd_help ();
   if (ifile == NULL && isatty (STDIN_FILENO))
      cmd_help ();
   for (index = optind; index < argc; index++)
      cmd_help ();

   /* initialize hash tables */
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

    /** 'should' work on UTF-8 terminals as long as the 
     ** user did set it to *._UTF-8 locale **/
    /** ###TODO: real UTF-8 support needs to be done **/
   setlocale (LC_CTYPE, "");
   initscr ();
   clear ();
   if (has_colors () == FALSE)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Your terminal does not support color");
   set_input_opts ();
   start_color ();
   parse_conf_file ();
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
                     "Unable to allocate memory for new window.");

   main_win = newwin (row - 7, col, 6, 0);
   keypad (main_win, TRUE);
   if (main_win == NULL)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Unable to allocate memory for new window.");

   /* main processing event */
   (void) time (&start_proc);
   logger = init_struct ();
   if (parse_log (logger, ifile, NULL))
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Error while processing file");
   allocate_structs (0);

   (void) time (&end_proc);

   initial_reqs = logger->total_process;
   /* draw screens */
   int x, y;
   getmaxyx (main_win, y, x);

   scrolling.scrl_main_win = y;
   scrolling.init_scrl_main_win = 0;
   render_screens ();
   get_keys ();

   write_conf_file ();
   house_keeping (logger, s_display);
   attroff (COLOR_PAIR (COL_WHITE));

   /* restore tty modes and reset 
    * terminal into non-visual mode */
   endwin ();
   return 0;
}
