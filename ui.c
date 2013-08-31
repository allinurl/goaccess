/**
 * ui.c -- various curses interfaces
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

/*
 * "_XOPEN_SOURCE" is required for the GNU libc to export "strptime(3)"
 * correctly.
 */
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#define _XOPEN_SOURCE 700
#define STDIN_FILENO  0
#define _BSD_SOURCE

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <pthread.h>
#include <ctype.h>

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "ui.h"

#include "commons.h"
#include "error.h"
#include "xmalloc.h"
#include "gmenu.h"
#include "goaccess.h"
#include "parser.h"
#include "settings.h"
#include "util.h"

static GSortModule module_sort[TOTAL_MODULES] = {
   {1, 1, 1, 0, {"Hits", "Data", "Bandwidth", NULL}},
   {1, 1, 1, 1, {"Hits", "Data", "Bandwidth", "Time Served", NULL}},
   {1, 1, 1, 1, {"Hits", "Data", "Bandwidth", "Time Served", NULL}},
   {1, 1, 1, 1, {"Hits", "Data", "Bandwidth", "Time Served", NULL}},
   {1, 1, 1, 1, {"Hits", "Data", "Bandwidth", "Time Served", NULL}},
   {1, 1, 0, 0, {"Hits", "Data", NULL}},
   {1, 1, 0, 0, {"Hits", "Data", NULL}},
   {1, 1, 0, 0, {"Hits", "Data", NULL}},
   {1, 1, 0, 0, {"Hits", "Data", NULL}},
   {1, 1, 0, 0, {"Hits", "Data", NULL}},
   {1, 1, 0, 0, {"Hits", "Data", NULL}},
};

/* creation - ncurses' window handling */
WINDOW *
create_win (int h, int w, int y, int x)
{
   WINDOW *win = newwin (h, w, y, x);
   if (win == NULL)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Unable to allocate memory for new WINDOW.");
   return win;
}

/* initialize curses colors */
void
init_colors ()
{
   use_default_colors ();

   init_pair (COL_BLUE, COLOR_BLUE, -1);
   if (conf.color_scheme == MONOCHROME)
      init_pair (COL_GREEN, COLOR_WHITE, -1);
   else
      init_pair (COL_GREEN, COLOR_GREEN, -1);

   init_pair (COL_RED, COLOR_RED, -1);
   init_pair (COL_BLACK, COLOR_BLACK, -1);
   init_pair (COL_CYAN, COLOR_CYAN, -1);
   init_pair (COL_YELLOW, COLOR_YELLOW, -1);

   if (conf.color_scheme == MONOCHROME)
      init_pair (BLUE_GREEN, COLOR_BLUE, COLOR_WHITE);
   else
      init_pair (BLUE_GREEN, COLOR_BLUE, COLOR_GREEN);

   init_pair (BLACK_GREEN, COLOR_BLACK, COLOR_GREEN);
   init_pair (BLACK_CYAN, COLOR_BLACK, COLOR_CYAN);
   init_pair (WHITE_RED, COLOR_WHITE, COLOR_RED);
}

/* creation - ncurses' window handling */
void
set_input_opts (void)
{
   noecho ();
   halfdelay (10);
   nonl ();
   intrflush (stdscr, FALSE);
   keypad (stdscr, TRUE);
   curs_set (0);
}

/* delete ncurses window handling */
void
close_win (WINDOW * w)
{
   if (w == NULL)
      return;
   wclear (w);
   wrefresh (w);
   delwin (w);
}

void
generate_time (void)
{
   now = time (NULL);
   now_tm = localtime (&now);
}

/* draw a generic header */
void
draw_header (WINDOW * win, char *header, int x, int y, int w, int color)
{
   char buf[256];
   snprintf (buf, sizeof buf, "%s%s", " ", header);

   if (conf.color_scheme == MONOCHROME) {
      init_pair (1, COLOR_BLACK, COLOR_WHITE);
      init_pair (2, COLOR_WHITE, -1);
   } else {
      init_pair (1, COLOR_BLACK, COLOR_GREEN);
      init_pair (2, COLOR_BLACK, COLOR_CYAN);
   }
   wattron (win, COLOR_PAIR (color));
   mvwhline (win, y, x, ' ', w);
   mvwaddnstr (win, y, x, buf, w);
   wattroff (win, COLOR_PAIR (color));
}

/* determine the actual size of the main window */
void
term_size (WINDOW * main_win)
{
   getmaxyx (stdscr, term_h, term_w);

   real_size_y = term_h - (MAX_HEIGHT_HEADER + MAX_HEIGHT_FOOTER);
   wresize (main_win, real_size_y, term_w);
   wmove (main_win, real_size_y, 0);
}

const char *
module_to_label (GModule module)
{
   static const char *modules[] = {
      "Visitors",
      "Requests",
      "Static Requests",
      "Not Found",
      "Hosts",
      "OS",
      "Browsers",
      "Referrers",
      "Referring Sites",
      "Keyphrases",
      "Status Codes"
   };

   return modules[module];
}

/* rerender header window to reflect active module */
void
update_active_module (WINDOW * header_win, GModule current)
{
   const char *module = module_to_label (current);
   int col = getmaxx (stdscr);

   char *lbl = xmalloc (snprintf (NULL, 0, "[Active Module: %s]", module) + 1);
   sprintf (lbl, "[Active Module: %s]", module);

   wattron (header_win, COLOR_PAIR (BLUE_GREEN));
   wmove (header_win, 0, 30);
   mvwprintw (header_win, 0, col - strlen (lbl) - 1, "%s", lbl);
   wattroff (header_win, COLOR_PAIR (BLUE_GREEN));
   wrefresh (header_win);

   free (lbl);
}

/* render general statistics */
void
display_general (WINDOW * win, char *ifile, int piping, int processed,
                 int invalid, unsigned long long bandwidth)
{
   size_t n, i, j, max_field = 0, max_value = 0, mod_val, y;
   int x_field = 2, x_value = 0;
   char *bw, *size, *log_file;

   werase (win);

   draw_header (win, T_HEAD, 0, 0, getmaxx (stdscr), 1);

   if (!piping && ifile != NULL) {
      size = filesize_str (file_size (ifile));
      log_file = alloc_string (ifile);
   } else {
      size = alloc_string ("N/A");
      log_file = alloc_string ("STDIN");
   }
   bw = filesize_str ((float) bandwidth);

   typedef struct Field_
   {
      char *field;
      char *value;              /* char due to log, bw, log_file */
      int color;
   } Field;

   /* *INDENT-OFF* */
   char *failed       = int_to_str (invalid);
   char *not_found    = int_to_str (g_hash_table_size (ht_not_found_requests));
   char *process      = int_to_str (processed);
   char *ref          = int_to_str (g_hash_table_size (ht_referrers));
   char *req          = int_to_str (g_hash_table_size (ht_requests));
   char *static_files = int_to_str (g_hash_table_size (ht_requests_static));
   char *time         = int_to_str (((int) end_proc - start_proc));
   char *visitors     = int_to_str (g_hash_table_size (ht_unique_visitors));

   Field fields[] = {
      {T_REQUESTS,   process,           COL_CYAN},
      {T_UNIQUE_VIS, visitors,          COL_CYAN},
      {T_REFERRER,   ref,               COL_CYAN},
      {T_LOG,        size,              COL_CYAN},
      {T_F_REQUESTS, failed,            COL_CYAN},
      {T_UNIQUE_FIL, req,               COL_CYAN},
      {T_UNIQUE404,  not_found,         COL_CYAN},
      {T_BW,         bw,                COL_CYAN},
      {T_GEN_TIME,   time,              COL_CYAN},
      {T_STATIC_FIL, static_files,      COL_CYAN},
      {"",           alloc_string (""), COL_CYAN},
      {T_LOG_PATH,   log_file,          COL_YELLOW}
   };
   n = ARRAY_SIZE (fields);

   /* *INDENT-ON* */
   for (i = 0, y = 2; i < n; i++) {
      mod_val = i % 4;
      if (i > 0 && mod_val == 0) {
         max_field = 0;
         max_value = 0;
         x_field = 2;
         x_value = 2;
         y++;
      }

      x_field += max_field;
      mvwprintw (win, y, x_field, "%s", fields[i].field);

      max_field = 0;
      for (j = 0; j < n; j++) {
         size_t len = strlen (fields[j].field);
         if (j % 4 == mod_val && len > max_field)
            max_field = len;
      }

      max_value = 0;
      for (j = 0; j < n; j++) {
         size_t len = strlen (fields[j].value);
         if (j % 4 == mod_val && len > max_value)
            max_value = len;
      }
      x_value = max_field + x_field + 1;
      max_field += max_value + 2;

      wattron (win, A_BOLD | COLOR_PAIR (fields[i].color));
      mvwprintw (win, y, x_value, "%s", fields[i].value);
      wattroff (win, A_BOLD | COLOR_PAIR (fields[i].color));
   }
   for (i = 0; i < n; i++) {
      free (fields[i].value);
   }
}

/* implement basic frame work to build a field input */
char *
input_string (WINDOW * win, int pos_y, int pos_x, size_t max_width, char *str,
              int enable_case, int *toggle_case)
{
   char *s = xmalloc (max_width + 1), *tmp;
   size_t pos = 0, x = 0, quit = 1, c;

   /* window dimensions */
   size_t size_x = 0, size_y = 0;
   getmaxyx (win, size_y, size_x);
   size_x -= 4;

   /* are we setting a default string */
   if (str) {
      size_t len = MIN (max_width, strlen (str));
      memcpy (s, str, len);
      s[len] = '\0';

      x = pos = 0;
      /* is the default str length greater than input field? */
      if (strlen (s) > size_x) {
         tmp = xstrdup (&s[0]);
         tmp[size_x] = '\0';
         mvwprintw (win, pos_y, pos_x, "%s", tmp);
         free (tmp);
      } else
         mvwprintw (win, pos_y, pos_x, "%s", s);
   } else
      s[0] = '\0';

   if (enable_case)
      draw_header (win, "[x] case sensitive", 1, size_y - 2, size_x - 2, 2);

   wmove (win, pos_y, pos_x + x);
   wrefresh (win);

   curs_set (1);
   while (quit) {
      c = wgetch (stdscr);
      switch (c) {
       case 1:                 /* ^a   */
       case 262:               /* HOME */
          pos = x = 0;
          break;
       case 5:
       case 360:               /* END of line */
          if (strlen (s) > size_x) {
             x = size_x;
             pos = strlen (s) - size_x;
          } else {
             pos = 0;
             x = strlen (s);
          }
          break;
       case 7:                 /* ^g  */
       case 27:                /* ESC */
          pos = x = 0;
          if (str && *str == '\0')
             s[0] = '\0';
          quit = 0;
          break;
       case 9:                 /* TAB   */
          if (!enable_case)
             break;
          *toggle_case = *toggle_case == 0 ? 1 : 0;
          if (*toggle_case)
             draw_header (win, "[ ] case sensitive", 1, size_y - 2, size_x - 2,
                          2);
          else if (!*toggle_case)
             draw_header (win, "[x] case sensitive", 1, size_y - 2, size_x - 2,
                          2);
          break;
       case 21:                /* ^u */
          s[0] = '\0';
          pos = x = 0;
          break;
       case 127:
       case KEY_BACKSPACE:
          if (pos + x > 0) {
             memmove (&s[(pos + x) - 1], &s[pos + x],
                      (max_width - (pos + x)) + 1);
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
      size_t i;
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

/* get user-agent string for given key */
char *
ht_bw_str (GHashTable * ht, const char *key)
{
   gpointer value_ptr;

   value_ptr = g_hash_table_lookup (ht, key);
   if (value_ptr != NULL)
      return filesize_str (*(unsigned long long *) value_ptr);
   else
      return alloc_string ("-");
}

/* wrapper - get human-readable bandwidth given X bytes */
char *
bandwidth_string (unsigned long long bw)
{
   return filesize_str (bw);
}

/* allocate memory for a new instace of GAgents */
GAgents *
new_gagents (unsigned int n)
{
   GAgents *agents = xcalloc (n, sizeof (GAgents));
   return agents;
}

/* split agent str if length > max or if '|' is found */
int
split_agent_str (char *ptr_value, GAgents * agents, int max)
{
   char *holder;
   char *p = ptr_value;
   int i = 0, offset = 0;

   agents[i].agents = xmalloc (max + 1);
   holder = agents[i].agents;

   while (*p != '\0') {
      /* add char to holder */
      if (offset < max && *p != '|') {
         *(holder++) = *(p++);
         offset++;
         continue;
      }
      /* split agent string since a delim was found */
      else if (*p == '|')
         (void) *p++;
      offset = 0;
      *holder = '\0';
      agents[++i].agents = xmalloc (max + 1);
      holder = agents[i].agents;
   }
   *holder = '\0';
   return i + 1;
}

/* render a list of agents if available */
void
load_agent_list (WINDOW * main_win, char *addr)
{
   if (!conf.list_agents)
      return;

   char *ptr_value;
   GAgents *agents = NULL;
   gpointer value_ptr = NULL;
   int c, quit = 1, delims = 0;
   int i, n = 0, alloc = 0;
   int y, x, list_h, list_w, menu_w, menu_h;

   getmaxyx (stdscr, y, x);
   list_h = y / 2;              /* list window - height */
   list_w = x - 4;              /* list window - width */
   menu_h = list_h - AGENTS_MENU_Y - 1; /* menu window - height */
   menu_w = list_w - AGENTS_MENU_X - AGENTS_MENU_X;     /* menu window - width */

   value_ptr = g_hash_table_lookup (ht_hosts_agents, addr);
   if (value_ptr != NULL) {
      ptr_value = (char *) value_ptr;
      delims = count_occurrences (ptr_value, '|');

      n = ((strlen (ptr_value) + menu_w - 1) / menu_w) + delims + 1;
      agents = new_gagents (n);
      alloc = split_agent_str (ptr_value, agents, menu_w);
   }

   WINDOW *win = newwin (list_h, list_w, (y - list_h) / 2, (x - list_w) / 2);
   keypad (win, TRUE);
   wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

   /* create a new instance of GMenu and make it selectable */
   GMenu *menu = new_gmenu (win, menu_h, menu_w, AGENTS_MENU_Y, AGENTS_MENU_X);

   /* add items to GMenu */
   menu->items = (GItem *) xcalloc (alloc, sizeof (GItem));
   for (i = 0; i < alloc; ++i) {
      menu->items[i].name = alloc_string (agents[i].agents);
      menu->items[i].checked = 0;
      menu->size++;
   }
   post_gmenu (menu);

   char buf[256];
   snprintf (buf, sizeof buf, "User Agents for %s", addr);
   draw_header (win, buf, 1, 1, list_w - 2, 1);
   mvwprintw (win, 2, 2, "[UP/DOWN] to scroll - [q] to close window");

   wrefresh (win);
   while (quit) {
      c = wgetch (stdscr);
      switch (c) {
       case KEY_DOWN:
          gmenu_driver (menu, REQ_DOWN);
          draw_header (win, "", 2, 3, CONF_MENU_W, 0);
          break;
       case KEY_UP:
          gmenu_driver (menu, REQ_UP);
          draw_header (win, "", 2, 3, CONF_MENU_W, 0);
          break;
       case KEY_RESIZE:
       case 'q':
          quit = 0;
          break;
      }
      wrefresh (win);
   }
   /* clean stuff up */
   for (i = 0; i < alloc; ++i)
      free (menu->items[i].name);
   free (menu->items);
   free (menu);

   for (i = 0; i < alloc; ++i)
      free (agents[i].agents);
   if (agents)
      free (agents);

   touchwin (main_win);
   close_win (win);
   wrefresh (main_win);
}

/* render processing spinner */
void
ui_spinner (void *ptr_data)
{
   GSpinner *spinner = (GSpinner *) ptr_data;
   static char const spin_chars[] = "/-\\|";
   int i = 0;
   while (1) {
      pthread_mutex_lock (&spinner->mutex);
      if (spinner->state == SPN_END)
         break;
      wattron (spinner->win, COLOR_PAIR (spinner->color));
      mvwaddch (spinner->win, spinner->y, spinner->x, spin_chars[i++ & 3]);
      wattroff (spinner->win, COLOR_PAIR (spinner->color));
      wrefresh (spinner->win);
      pthread_mutex_unlock (&spinner->mutex);
      usleep (100000);
   }
   free (spinner);
}

/* create spinner's thread */
void
ui_spinner_create (GSpinner * spinner)
{
   pthread_create (&(spinner->thread), NULL, (void *) &ui_spinner, spinner);
   pthread_detach (spinner->thread);
}

/* allocate memory and initialize data */
GSpinner *
new_gspinner ()
{
   int y, x;
   getmaxyx (stdscr, y, x);

   GSpinner *spinner = xcalloc (1, sizeof (GSpinner));

   spinner->color = HIGHLIGHT;
   spinner->state = SPN_RUN;
   spinner->win = stdscr;
   spinner->x = x - 2;
   spinner->y = y - 1;

   if (pthread_mutex_init (&(spinner->mutex), NULL))
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Failed thread mutex");

   return spinner;
}

/* render config log date/format dialog */
int
verify_format (GLog * logger, GSpinner * spinner)
{
   char *cstm_log, *cstm_date;
   int c, quit = 1;
   size_t i, n;
   int invalid = 1;
   int y, x, h = CONF_WIN_H, w = CONF_WIN_W;
   int w2 = w - 2;

   /* conf dialog menu options */
   char *choices[] = {
      "Common Log Format (CLF)",
      "Common Log Format (CLF) with Virtual Host",
      "NCSA Combined Log Format",
      "NCSA Combined Log Format with Virtual Host",
      "W3C",
      "CloudFront (Download Distribution)"
   };
   n = ARRAY_SIZE (choices);
   getmaxyx (stdscr, y, x);

   WINDOW *win = newwin (h, w, (y - h) / 2, (x - w) / 2);
   keypad (win, TRUE);
   wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

   /* create a new instance of GMenu and make it selectable */
   GMenu *menu =
      new_gmenu (win, CONF_MENU_H, CONF_MENU_W, CONF_MENU_Y, CONF_MENU_X);
   menu->size = n;
   menu->selectable = 1;

   /* add items to GMenu */
   menu->items = (GItem *) xcalloc (n, sizeof (GItem));
   for (i = 0; i < n; ++i) {
      menu->items[i].name = alloc_string (choices[i]);
      size_t sel = get_selected_format_idx ();
      menu->items[i].checked = sel == i ? 1 : 0;
   }
   post_gmenu (menu);

   draw_header (win, "Log Format Configuration", 1, 1, w2, 1);
   mvwprintw (win, 2, 2, "[SPACE] to toggle - [ENTER] to proceed");

   /* set log format from goaccessrc if available */
   draw_header (win, "Log Format - [c] to add/edit format", 1, 11, w2, 1);
   if (conf.log_format) {
      tmp_log_format = alloc_string (conf.log_format);
      mvwprintw (win, 12, 2, "%.*s", CONF_MENU_W, conf.log_format);
      free (conf.log_format);
      conf.log_format = NULL;
   }

   /* set date format from goaccessrc if available */
   draw_header (win, "Date Format - [d] to add/edit format", 1, 14, w2, 1);
   if (conf.date_format) {
      tmp_date_format = alloc_string (conf.date_format);
      mvwprintw (win, 15, 2, "%.*s", CONF_MENU_W, conf.date_format);
      free (conf.date_format);
      conf.date_format = NULL;
   }

   wrefresh (win);
   while (quit) {
      c = wgetch (stdscr);
      switch (c) {
       case KEY_DOWN:
          gmenu_driver (menu, REQ_DOWN);
          draw_header (win, "", 2, 3, CONF_MENU_W, 0);
          break;
       case KEY_UP:
          gmenu_driver (menu, REQ_UP);
          draw_header (win, "", 2, 3, CONF_MENU_W, 0);
          break;
       case 32:                /* space */
          gmenu_driver (menu, REQ_SEL);
          if (tmp_log_format)
             free (tmp_log_format);

          if (tmp_date_format)
             free (tmp_date_format);

          for (i = 0; i < n; ++i) {
             if (menu->items[i].checked != 1)
                continue;

             tmp_log_format = get_selected_format_str (i);
             tmp_date_format = get_selected_date_str (i);
             draw_header (win, tmp_log_format, 1, 12, CONF_MENU_W, 0);
             draw_header (win, tmp_date_format, 1, 15, CONF_MENU_W, 0);
             break;
          }
          break;
       case 89:                /* Y/y */
       case 121:
          if (tmp_log_format) {
             conf.log_format = alloc_string (tmp_log_format);
             quit = 0;
          }
          break;
       case 99:                /* c */
          /* clear top status bar */
          draw_header (win, "", 2, 3, CONF_MENU_W, 0);
          wmove (win, 12, 2);

          /* get input string */
          cstm_log = input_string (win, 12, 2, 70, tmp_log_format, 0, 0);
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
          /* clear top status bar */
          draw_header (win, "", 2, 3, CONF_MENU_W, 0);
          wmove (win, 15, 0);

          /* get input string */
          cstm_date = input_string (win, 15, 2, 14, tmp_date_format, 0, 0);
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
       case 0x0a:
       case 0x0d:
       case KEY_ENTER:
          /* display status bar error messages */
          if (tmp_date_format == NULL)
             draw_header (win, "Select a date format.", 2, 3, CONF_MENU_W,
                          WHITE_RED);
          if (tmp_log_format == NULL)
             draw_header (win, "Select a log format.", 2, 3, CONF_MENU_W,
                          WHITE_RED);

          if (tmp_log_format && tmp_date_format) {
             conf.date_format = alloc_string (tmp_date_format);
             conf.log_format = alloc_string (tmp_log_format);

             /* test log against selected settings */
             if (test_format (logger)) {
                invalid = 1;
                draw_header (win, "No valid hits. 'y' to continue anyway.", 2,
                             3, CONF_MENU_W, WHITE_RED);
                free (conf.date_format);
                free (conf.log_format);

                conf.date_format = NULL;
                conf.log_format = NULL;
             }
             /* valid data, reset logger & start parsing */
             else {
                reset_struct (logger);
                draw_header (win, "Parsing...", 2, 3, CONF_MENU_W, BLACK_CYAN);

                /* start spinner thread */
                spinner->win = win;
                spinner->y = 3;
                spinner->x = w - 4;
                spinner->color = BLACK_CYAN;
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
   /* clean stuff up */
   for (i = 0; i < n; ++i)
      free (menu->items[i].name);
   free (menu->items);
   free (menu);

   return invalid ? 1 : 0;
}

/* get selected scheme */
static void
scheme_chosen (char *name)
{
   if (strcmp ("Monochrome/Default", name) == 0)
      conf.color_scheme = MONOCHROME;
   else
      conf.color_scheme = STD_GREEN;
   init_colors ();
}

/* render schemes dialog */
void
load_schemes_win (WINDOW * main_win)
{
   int c, quit = 1;
   size_t i, n;
   int y, x, h = SCHEME_WIN_H, w = SCHEME_WIN_W;
   int w2 = w - 2;

   char *choices[] = {
      "Monochrome/Default",
      "Green/Original"
   };

   n = ARRAY_SIZE (choices);
   getmaxyx (stdscr, y, x);

   WINDOW *win = newwin (h, w, (y - h) / 2, (x - w) / 2);
   keypad (win, TRUE);
   wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

   /* create a new instance of GMenu and make it selectable */
   GMenu *menu = new_gmenu (win, SCHEME_MENU_H, SCHEME_MENU_W, SCHEME_MENU_Y,
                            SCHEME_MENU_X);
   menu->size = n;

   /* add items to GMenu */
   menu->items = (GItem *) xcalloc (n, sizeof (GItem));
   for (i = 0; i < n; ++i) {
      menu->items[i].name = alloc_string (choices[i]);
      menu->items[i].checked = 0;
   }
   post_gmenu (menu);

   draw_header (win, "Scheme Configuration", 1, 1, w2, 1);
   mvwprintw (win, 2, 2, "[ENTER] to switch scheme");

   wrefresh (win);
   while (quit) {
      c = wgetch (stdscr);
      switch (c) {
       case KEY_DOWN:
          gmenu_driver (menu, REQ_DOWN);
          draw_header (win, "", 2, 3, SCHEME_MENU_W, 0);
          break;
       case KEY_UP:
          gmenu_driver (menu, REQ_UP);
          draw_header (win, "", 2, 3, SCHEME_MENU_W, 0);
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

   touchwin (main_win);
   close_win (win);
   wrefresh (main_win);
}

/* render sort dialog */
void
load_sort_win (WINDOW * main_win, GModule module, GSort * sort)
{
   int c, quit = 1;
   int i = 0, n = 0;
   int y, x, h = SORT_WIN_H, w = SORT_WIN_W;
   int w2 = w - 2;
   getmaxyx (stdscr, y, x);

   /* determine amount of sort choices */
   for (i = 0; NULL != module_sort[module].choices[i]; i++) {
      const char *name = module_sort[module].choices[i];
      if (strcmp ("Time Served", name) == 0 && !conf.serve_usecs &&
          !conf.serve_secs)
         continue;
      else if (strcmp ("Bandwidth", name) == 0 && !conf.bandwidth)
         continue;
      n++;
   }

   WINDOW *win = newwin (h, w, (y - h) / 2, (x - w) / 2);
   keypad (win, TRUE);
   wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

   /* create a new instance of GMenu and make it selectable */
   GMenu *menu =
      new_gmenu (win, SORT_MENU_H, SORT_MENU_W, SORT_MENU_Y, SORT_MENU_X);
   menu->size = n;
   menu->selectable = 1;

   /* add items to GMenu */
   menu->items = (GItem *) xcalloc (n, sizeof (GItem));
   /* set checked option and set index */
   for (i = 0; i < n; ++i) {
      menu->items[i].name = alloc_string (module_sort[module].choices[i]);

      if (sort->field == SORT_BY_HITS &&
          strcmp ("Hits", menu->items[i].name) == 0) {
         menu->items[i].checked = 1;
         menu->idx = i;
      } else if (sort->field == SORT_BY_DATA &&
                 strcmp ("Data", menu->items[i].name) == 0) {
         menu->items[i].checked = 1;
         menu->idx = i;
      } else if (sort->field == SORT_BY_BW &&
                 strcmp ("Bandwidth", menu->items[i].name) == 0) {
         menu->items[i].checked = 1;
         menu->idx = i;
      } else if (sort->field == SORT_BY_USEC &&
                 strcmp ("Time Served", menu->items[i].name) == 0) {
         menu->items[i].checked = 1;
         menu->idx = i;
      }
   }
   post_gmenu (menu);

   draw_header (win, "Sort active module by", 1, 1, w2, 1);
   mvwprintw (win, 2, 2, "[ENTER] to select field - [TAB] sort");
   if (sort->sort == SORT_ASC)
      draw_header (win, "[x] ASC [ ] DESC", 1, SORT_WIN_H - 2, SORT_WIN_W - 2,
                   2);
   else
      draw_header (win, "[ ] ASC [x] DESC", 1, SORT_WIN_H - 2, SORT_WIN_W - 2,
                   2);

   wrefresh (win);
   while (quit) {
      c = wgetch (stdscr);
      switch (c) {
       case KEY_DOWN:
          gmenu_driver (menu, REQ_DOWN);
          draw_header (win, "", 2, 3, SORT_MENU_W, 0);
          break;
       case KEY_UP:
          gmenu_driver (menu, REQ_UP);
          draw_header (win, "", 2, 3, SORT_MENU_W, 0);
          break;
       case 9:                 /* TAB   */
          /* ascending */
          if (sort->sort == SORT_ASC) {
             sort->sort = SORT_DESC;
             draw_header (win, "[ ] ASC [x] DESC", 1, SORT_WIN_H - 2,
                          SORT_WIN_W - 2, 2);
          }
          /* descending */
          else {
             sort->sort = SORT_ASC;
             draw_header (win, "[x] ASC [ ] DESC", 1, SORT_WIN_H - 2,
                          SORT_WIN_W - 2, 2);
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
             else if (strcmp ("Data", menu->items[i].name) == 0)
                sort->field = SORT_BY_DATA;
             else if (strcmp ("Bandwidth", menu->items[i].name) == 0)
                sort->field = SORT_BY_BW;
             else if (strcmp ("Time Served", menu->items[i].name) == 0)
                sort->field = SORT_BY_USEC;
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

static char *help_main[] = {
   "Copyright (C) 2009-2013",
   "by Gerardo Orellana <goaccess@prosoftcorp.com>",
   "http://goaccess.prosoftcorp.com",
   "Released under the GNU GPL. See `man` page for",
   "more details.",
   "",
   "GoAccess is an open source real-time web log",
   "analyzer and interactive viewer that runs in a",
   "terminal in *nix systems. It provides fast and",
   "valuable HTTP statistics for system administrators",
   "that require a visual server report on the fly." "",
   "The data collected based on the parsing of the log",
   "is divided into different modules. Modules are",
   "automatically generated and presented to the user.",
   "",
   "The main dashboard displays general statistics, top",
   "visitors, requests, browsers, operating systems,",
   "hosts, etc.",
   "",
   "The user can make use of the following keys:",
   " ^F1^ or ^CTRL^ + ^h^ [main help]",
   " ^F5^    redraw [main window]",
   " ^q^     quit the program, current window or module",
   " ^o^  or ^ENTER^  expand selected module",
   " ^j^     scroll down within expanded module",
   " ^k^     scroll up within expanded module",
   " ^c^     set or change scheme color",
   " ^CTRL^ + ^f^ scroll forward one screen within",
   "         active module",
   " ^CTRL^ + ^b^ scroll backward one screen within",
   "         active module",
   " ^TAB^   iterate modules (forward)",
   " ^SHIFT^ + ^TAB^ iterate modules (backward)",
   " ^s^     sort options for current module",
   " ^/^     search across all modules",
   " ^n^     find position of the next occurrence",
   " ^g^     move to the first item or top of screen",
   " ^G^     move to the last item or bottom of screen",
   "",
   "Examples can be found by running `man goaccess`.",
   "",
   "If you believe you have found a bug, please drop me",
   "an email with details.",
   "",
   "If you have a medium or high traffic website, it",
   "would be interesting to hear your experience with",
   "GoAccess, such as generating time, visitors or hits.",
   "",
   "Feedback? Just shoot me an email to:",
   "goaccess@prosoftcorp.com",
};

/* render help dialog */
void
load_help_popup (WINDOW * main_win)
{
   int c, quit = 1;
   size_t i, n;
   int y, x, h = HELP_WIN_HEIGHT, w = HELP_WIN_WIDTH;
   int w2 = w - 2;

   n = ARRAY_SIZE (help_main);
   getmaxyx (stdscr, y, x);

   WINDOW *win = newwin (h, w, (y - h) / 2, (x - w) / 2);
   keypad (win, TRUE);
   wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

   /* create a new instance of GMenu and make it selectable */
   GMenu *menu = new_gmenu (win, HELP_MENU_HEIGHT, HELP_MENU_WIDTH, HELP_MENU_Y,
                            HELP_MENU_X);
   menu->size = n;

   /* add items to GMenu */
   menu->items = (GItem *) xcalloc (n, sizeof (GItem));
   for (i = 0; i < n; ++i) {
      menu->items[i].name = alloc_string (help_main[i]);
      menu->items[i].checked = 0;
   }
   post_gmenu (menu);

   draw_header (win, "GoAccess Quick Help", 1, 1, w2, 1);
   mvwprintw (win, 2, 2, "[UP/DOWN] to scroll - [q] to quit");

   wrefresh (win);
   while (quit) {
      c = wgetch (stdscr);
      switch (c) {
       case KEY_DOWN:
          gmenu_driver (menu, REQ_DOWN);
          draw_header (win, "", 2, 3, HELP_MENU_WIDTH, 0);
          break;
       case KEY_UP:
          gmenu_driver (menu, REQ_UP);
          draw_header (win, "", 2, 3, HELP_MENU_WIDTH, 0);
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
