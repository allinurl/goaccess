/**
 * Copyright (C) 2009-2012 by Gerardo Orellana <goaccess@prosoftcorp.com>
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

#ifndef UI_H_INCLUDED
#define UI_H_INCLUDED

#define T_HEAD "General Summary - Overall Analysed Requests - Unique totals"
#define T_BW         "BW"
#define T_F_REQUESTS "Failed Requests"
#define T_GEN_TIME   "Generation Time"
#define T_LOG        "Log"
#define T_REFERRER   "Referrers"
#define T_REQUESTS   "Total Requests"
#define T_STATIC_FIL "Static Files"
#define T_UNIQUE404  "Unique 404"
#define T_UNIQUE_FIL "Unique Files"
#define T_UNIQUE_VIS "Unique Visitors"

struct struct_agents
{
   char *agents;
};

WINDOW *create_win (WINDOW * main_win);
void set_input_opts (void);
void close_win (WINDOW * w);
void generate_time (void);
void draw_header (WINDOW * win, char *header, int x, int y, int w, int color);
void update_header (WINDOW * header_win, int current);
void term_size (WINDOW * main_win);
char *input_string (WINDOW * win, int pos_y, int pos_x, int max_width,
                    char *str);
void display_general (WINDOW * header_win, struct logger *logger,
                      char *ifile);
char *ht_bw_str (GHashTable * ht, const char *key);
void display_content (WINDOW * main_win, struct struct_display **s_display,
                      struct logger *logger, struct scrolling scrolling);
void do_scrolling (WINDOW * main_win, struct struct_display **s_display,
                   struct logger *logger, struct scrolling *scrolling,
                   int cmd);
void load_help_popup (WINDOW * help_win, size_t startx);
void split_agent_str (char *ptr_value, struct struct_agents *s_agents,
                      size_t max);
void load_schemes_win (WINDOW * schemes_win, size_t startx);
char *get_browser_type (char *line);
void load_popup (WINDOW * my_menu_win, struct struct_holder **s_holder,
                 struct logger *logger);

#endif
