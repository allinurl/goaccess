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
void close_win (WINDOW * w);
void generate_time (void);
void draw_header (WINDOW * win, char *header, int x, int y, int w, int color);
void update_header (WINDOW * header_win, int current);
void term_size (WINDOW * main_win);
void display_general (WINDOW * header_win, struct logger *logger,
                      char *ifile);
void create_graphs (WINDOW * main_win, struct struct_display **s_display,
                    struct logger *logger, int i, int module, int max);
int get_max_value (struct struct_display **s_display, struct logger *logger,
                   int module);
void display_content (WINDOW * main_win, struct struct_display **s_display,
                      struct logger *logger, struct scrolling scrolling);
void do_scrolling (WINDOW * main_win, struct struct_display **s_display,
                   struct logger *logger, struct scrolling *scrolling,
                   int cmd);
void load_help_popup (WINDOW * help_win);
void load_reverse_dns_popup (WINDOW * ip_detail_win, char *addr);
void load_popup (WINDOW * my_menu_win, struct struct_holder **s_holder,
                 struct logger *logger);

#endif
