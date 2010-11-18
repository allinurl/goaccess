/**
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

#ifndef UI_H_INCLUDED
#define UI_H_INCLUDED

WINDOW *create_win (WINDOW * main_win);

struct struct_agents
{
   char *agents;
};

int get_max_value (struct struct_display **s_display, struct logger *logger,
                   int module);
void close_win (WINDOW * w);
void create_graphs (WINDOW * main_win, struct struct_display **s_display,
                    struct logger *logger, int i, int module, int max);
void display_content (WINDOW * main_win, struct struct_display **s_display,
                      struct logger *logger, struct scrolling scrolling);
void display_general (WINDOW * header_win, struct logger *logger,
                      char *ifile);
void do_scrolling (WINDOW * main_win, struct struct_display **s_display,
                   struct logger *logger, struct scrolling *scrolling,
                   int cmd);
void draw_header (WINDOW * win, char *header, int x, int y, int w, int color);
void generate_time (void);
void load_help_popup (WINDOW * help_win);
void load_popup (WINDOW * my_menu_win, struct struct_holder **s_holder,
                 struct logger *logger);
void load_schemes_win (WINDOW * schemes_win);
void term_size (WINDOW * main_win);
void update_header (WINDOW * header_win, int current);

#endif
