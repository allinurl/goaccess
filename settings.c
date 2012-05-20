/**
 * settings.c -- goaccess configuration
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
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

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commons.h"
#include "error.h"
#include "gmenu.h"
#include "parser.h"
#include "ui.h"
#include "util.h"

static void
set_conf_vars (int key, char *val)
{
   switch (key) {
    case 1:
       color_scheme = atoi (val);
       break;
    case 2:
       log_format = alloc_string (val);
       break;
    case 3:
       date_format = alloc_string (val);
       break;
   }
}

/*###TODO: allow extra values for every key
 * separated by a delimeter */
int
parse_conf_file ()
{
   char *val, *c;
   int key = 0;

   char *user_home = getenv ("HOME");
   if (user_home == NULL)
      user_home = "";
   char *path = malloc (snprintf (NULL, 0, "%s/.goaccessrc", user_home) + 1);
   if (path == NULL)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Unable to allocate memory for path.");
   sprintf (path, "%s/.goaccessrc", user_home);

   char *conf_file = path;
   FILE *file = fopen (conf_file, "r");

   if (file == NULL) {
      free (path);
      return -1;
   }

   char line[512];
   while (fgets (line, sizeof line, file) != NULL) {
      int i;
      for (i = 0; i < keywords_size (); i++) {
         if ((strstr (line, conf_keywords[i][1])) != NULL)
            key = atoi (conf_keywords[i][0]);
      }
      if ((val = strchr (line, ' ')) == NULL) {
         free (path);
         return -1;
      }
      for (c = val; *c; c++) {
         /* get everything after space */
         if (!isspace (c[0])) {
            set_conf_vars (key, trim_str (c));
            break;
         }
      }
   }
   fclose (file);
   free (path);
   return 0;
}

void
write_conf_file (void)
{
   char *user_home;
   user_home = getenv ("HOME");
   if (user_home == NULL)
      user_home = "";

   char *path = malloc (snprintf (NULL, 0, "%s/.goaccessrc", user_home) + 1);
   if (path == NULL)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Unable to allocate memory for new window.");
   sprintf (path, "%s/.goaccessrc", user_home);

   FILE *file;
   file = fopen (path, "w");

   /* color scheme */
   fprintf (file, "color_scheme %d\n", color_scheme);

   /* date format */
   if (tmp_date_format)
      fprintf (file, "date_format %s\n", tmp_date_format);
   else
      fprintf (file, "date_format %s\n", date_format);

   /* log format */
   if (tmp_log_format)
      fprintf (file, "log_format %s", tmp_log_format);
   else
      fprintf (file, "log_format %s", log_format);

   fclose (file);
   if (date_format)
      free (date_format);
   if (log_format)
      free (log_format);
   free (path);
}

/* return the index of the matched item,
 * or -1 if no such item exists */
static int
get_selected_format_idx ()
{
   if (log_format == NULL)
      return -1;
   if (strcmp (log_format, COMMON1) == 0)
      return 0;
   else if (strcmp (log_format, COMMON2) == 0)
      return 1;
   else if (strcmp (log_format, COMBINED1) == 0)
      return 2;
   else if (strcmp (log_format, COMBINED2) == 0)
      return 3;
   else if (strcmp (log_format, W3C_INTERNET) == 0)
      return 4;
   else
      return -1;
}

/* return the string of the matched item,
 * or NULL if no such item exists */
static char *
get_selected_format_str (int idx)
{
   char *fmt = NULL;
   switch (idx) {
    case 0:
       fmt = alloc_string (COMMON1);
       break;
    case 1:
       fmt = alloc_string (COMMON2);
       break;
    case 2:
       fmt = alloc_string (COMBINED1);
       break;
    case 3:
       fmt = alloc_string (COMBINED2);
       break;
    case 4:
       fmt = alloc_string (W3C_INTERNET);
       break;
   }
   return fmt;
}

static char *
get_selected_date_str (int idx)
{
   char *fmt = NULL;
   switch (idx) {
    case 0:
    case 1:
    case 2:
    case 3:
       fmt = alloc_string (APACHE_DATE);
       break;
    case 4:
       fmt = alloc_string (W3C_DATE);
       break;
   }
   return fmt;
}

static int
test_format (struct logger *logger)
{
   if (parse_log (logger, ifile, NULL, 20))
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Error while processing file");
   if ((logger->total_process == 0) ||
       (logger->total_process == logger->total_invalid))
      return 1;
   return 0;
}

int
verify_format (struct logger *logger)
{
   int y, x, h = 16, w = 52, c, quit = 1, i, n, invalid = 1,
      w2 = w - 2, w4 = w - 4;
   char *cstm_log, *cstm_date;
   char *choices[] = {
      "Common Log Format (CLF)",
      "Common Log Format (CLF) with Virtual Host",
      "NCSA Combined Log Format",
      "NCSA Combined Log Format with Virtual Host",
      "W3C"
   };
   n = ARRAY_SIZE (choices);

   getmaxyx (stdscr, y, x);
   WINDOW *win = newwin (h, w, (y - h) / 2, (x - w) / 2);
   keypad (win, TRUE);

   draw_header (win, " Log Format Configuration", 1, 1, w2, 1);
   mvwprintw (win, 2, 2, "[SPACE] to toggle - [F10] to proceed");

   wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

   GMenu *menu = new_gmenu (win, 5, w4);
   menu->size = n;
   menu->selectable = 1;

   menu->items = (GItem *) calloc (n, sizeof (GItem));
   for (i = 0; i < n; ++i) {
      menu->items[i].name = alloc_string (choices[i]);
      int sel = get_selected_format_idx ();
      menu->items[i].checked = sel == i ? 1 : 0;
   }
   post_gmenu (menu);

   draw_header (win, " Log Format - [c] to add/edit format", 1, 10, w2, 1);
   if (log_format) {
      tmp_log_format = alloc_string (log_format);
      mvwprintw (win, 11, 2, "%.*s", w4, log_format);
      free (log_format);
      log_format = NULL;
   }

   draw_header (win, " Date Format - [d] to add/edit format", 1, 13, w2, 1);
   if (date_format) {
      tmp_date_format = alloc_string (date_format);
      mvwprintw (win, 14, 2, "%.*s", w4, date_format);
      free (date_format);
      date_format = NULL;
   }

   wrefresh (win);
   while (quit) {
      c = wgetch (stdscr);
      switch (c) {
       case KEY_DOWN:
          gmenu_driver (menu, REQ_DOWN);
          draw_header (win, " ", 2, 3, w4, 0);
          break;
       case KEY_UP:
          gmenu_driver (menu, REQ_UP);
          draw_header (win, " ", 2, 3, w4, 0);
          break;
       case 32:
       case 0x0a:
       case 0x0d:
       case KEY_ENTER:
          gmenu_driver (menu, REQ_SEL);
          if (tmp_log_format)
             free (tmp_log_format);
          if (tmp_date_format)
             free (tmp_date_format);
          for (i = 0; i < n; ++i) {
             if (menu->items[i].checked == 1) {
                tmp_log_format = get_selected_format_str (i);
                tmp_date_format = get_selected_date_str (i);
                draw_header (win, tmp_log_format, 2, 11, w4, 0);
                draw_header (win, tmp_date_format, 2, 14, w4, 0);
                break;
             }
          }
          break;
       case 89:                /* Y/y */
       case 121:
          if (tmp_log_format) {
             log_format = alloc_string (tmp_log_format);
             quit = 0;
          }
          break;
       case 99:                /* c */
          draw_header (win, " ", 2, 3, w4, 0);
          wmove (win, 11, 2);
          cstm_log = input_string (win, 11, 2, 70, tmp_log_format);

          if (cstm_log != NULL && *cstm_log != '\0') {
             if (tmp_log_format)
                free (tmp_log_format);
             tmp_log_format = alloc_string (cstm_log);
             free (cstm_log);
          } else {
             if (cstm_log)
                free (cstm_log);
             if (tmp_log_format) {
                free (tmp_log_format);
                tmp_log_format = NULL;
             }
          }
          break;
       case 100:               /* d */
          draw_header (win, " ", 2, 3, w4, 0);
          wmove (win, 14, 2);
          cstm_date = input_string (win, 14, 2, 14, tmp_date_format);

          if (cstm_date != NULL && *cstm_date != '\0') {
             if (tmp_date_format)
                free (tmp_date_format);
             tmp_date_format = alloc_string (cstm_date);
             free (cstm_date);
          } else {
             if (cstm_date)
                free (cstm_date);
             if (tmp_date_format) {
                free (tmp_date_format);
                tmp_date_format = NULL;
             }
          }
          break;
       case 274:               /* F10 */
          if (tmp_date_format == NULL)
             draw_header (win, " Select a date format.", 2, 3, w4, WHITE_RED);
          if (tmp_log_format == NULL)
             draw_header (win, " Select a log format.", 2, 3, w4, WHITE_RED);

          if (tmp_log_format && tmp_date_format) {
             date_format = alloc_string (tmp_date_format);
             log_format = alloc_string (tmp_log_format);
             if (test_format (logger)) {
                invalid = 1;
                draw_header (win, " No valid hits. 'y' to continue anyway.",
                             2, 3, w4, WHITE_RED);
                free (date_format);
                free (log_format);
                date_format = NULL;
                log_format = NULL;
             } else {
                reset_struct (logger);
                draw_header (win, " Parsing...", 2, 3, w4, BLACK_CYAN);
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
      wrefresh (win);
   }
   for (i = 0; i < n; ++i)
      free (menu->items[i].name);
   free (menu->items);
   free (menu);

   return invalid ? 1 : 0;
}
