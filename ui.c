/** 
 * ui.c -- curses user interface
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
 * @version 0.1
 * Last Modified: Wednesday, July 07, 2010
 * Path:  /ui.c
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * GoAccess is released under the GNU/GPL License.
 * Copy of the GNU General Public License is attached to this source 
 * distribution for its full text.
 *
 * Visit http://goaccess.prosoftcorp.com for new releases.
 */

/* "_XOPEN_SOURCE" is required for the GNU libc to export "strptime(3)"
 * correctly. */
#define _XOPEN_SOURCE 700

#include <string.h>
#include <curses.h>
#include <time.h>
#include <menu.h>
#include <glib.h>
#include <stdlib.h>
#include <GeoIP.h>

#include "parser.h"
#include "alloc.h"
#include "commons.h"
#include "util.h"
#include "ui.h"

static MENU *my_menu = NULL;
static ITEM **items = NULL;

/* creation - ncurses' window handling */
WINDOW *create_win(WINDOW *main_win)
{
	int y, x;
	getmaxyx(main_win,y,x);
	return (newwin( y - 12, x - 40, 8, 20));
}

/* deletion - ncurses' window handling */
void close_win(WINDOW *w)
{
	if (w == NULL)	return;
	wclear(w);
	wrefresh(w);
	delwin(w);
}

/* get the current date-time */
void generate_time(void) 
{
	now = time (NULL);
	now_tm = localtime (&now);
}

void draw_header(WINDOW *win, char* header, int x, int y, int w, int color) 
{
	init_pair(1, COLOR_BLACK, COLOR_GREEN);
	init_pair(2, COLOR_BLACK, COLOR_CYAN);
	
	wattron(win, COLOR_PAIR(color));
	mvwhline(win,y,x,' ', w);
	mvwaddnstr(win, y, x, header, w);
	wattroff(win,COLOR_PAIR(1));
}

void update_header(WINDOW *header_win, int current)
{
	int row = 0, col = 0;

	getmaxyx(stdscr, row, col);
	wattron(header_win,COLOR_PAIR(6));
	wmove(header_win, 0, 30);
	mvwprintw(header_win, 0, col - 20, "[Active Module %d]", current);
	wattroff(header_win,COLOR_PAIR(6));
	wrefresh(header_win);
}

void display_general(WINDOW *header_win, struct logger *logger, char *ifile)
{
	int row,col;
	getmaxyx(stdscr,row,col);
	draw_header(header_win," General Statistics - Information analyzed from log file - Unique totals", 0, 0, col,1);

	/* general stats */
	init_pair(10, COLOR_CYAN, -1);
	wattron(header_win,COLOR_PAIR(10));
	mvwprintw(header_win, 2, 18, "%u", logger->total_process);
	mvwprintw(header_win, 3, 18, "%u", logger->total_invalid);
	mvwprintw(header_win, 4, 18, "%d sec", (int) end_proc - start_proc);
	mvwprintw(header_win, 2, 50, "%d", g_hash_table_size(ht_unique_visitors));
	mvwprintw(header_win, 3, 50, "%d", g_hash_table_size(ht_requests));
	mvwprintw(header_win, 4, 50, "%d", g_hash_table_size(ht_requests_static));
	mvwprintw(header_win, 2, 74, "%d", g_hash_table_size(ht_referers));
	mvwprintw(header_win, 3, 74, "%d", g_hash_table_size(ht_not_found_requests));
	double log_size = ((double) file_size(ifile)) / MB;
	mvwprintw(header_win, 2, 86, "%.2fMB", log_size);

	double tot_bw = (float) req_size / GB;
	(bandwidth_flag) ? mvwprintw(header_win, 3, 86, "%.3f GB", tot_bw) : mvwprintw(header_win, 3, 86, "N/A");
	
	wattroff(header_win, COLOR_PAIR(10));
	wattron(header_win, COLOR_PAIR(7));
	mvwprintw(header_win, 4, 58, "%s", ifile);
	wattroff(header_win, COLOR_PAIR(7));

	/*labels*/
	init_pair(11, COLOR_WHITE, -1);
	wattron(header_win, COLOR_PAIR(11));
	mvwprintw(header_win, 2, 2, "Total hits");
	mvwprintw(header_win, 3, 2, "Invalid entries");
	mvwprintw(header_win, 4, 2, "Generation Time");
	mvwprintw(header_win, 2, 28, "Total Unique Visitors");
	mvwprintw(header_win, 3, 28, "Total Requests");
	mvwprintw(header_win, 4, 28, "Total Static Requests");
	mvwprintw(header_win, 2, 58, "Total Referrers");
	mvwprintw(header_win, 3, 58, "Total 404");
	mvwprintw(header_win, 3, 82, "BW");
	mvwprintw(header_win, 2, 82, "Log");
	wattroff(header_win, COLOR_PAIR(11));
}

void create_graphs(WINDOW *main_win, struct stu_alloc_all **sorted_alloc_all, 
				   struct logger *logger, int i, int module, int max)
{
	int x, y, xx, r, col, row;
	float l_bar, scr_cal, orig_cal;
	struct tm tm;
	char buf[12] = ""; /* max possible size of date */

	memset (&tm, 0, sizeof (tm));
	
	getyx(main_win,y,x);
	getmaxyx(stdscr,row,col);
	
	GHashTable *hash_table;
	
	switch (module)
	{
		case UNIQUE_VISITORS:
			hash_table = ht_unique_visitors;
			break;
		case REQUESTS:
			hash_table = ht_requests;
			break;
		case REQUESTS_STATIC:
			hash_table = ht_requests_static;
			break;
		case REFERRERS:
			hash_table = ht_referers;
			break;
		case NOT_FOUND:
			hash_table = ht_not_found_requests;
			break;
		case OS:
			hash_table = ht_os;
			break;
		case BROWSERS:
			hash_table = ht_browsers;
			break;
		case HOSTS:
			hash_table = ht_hosts;
			break;
		case STATUS_CODES:
			hash_table = ht_status_code;
			break;
		case REFERRING_SITES:
			hash_table = ht_referring_sites;
			break;
	}

	orig_cal 	= (float)(sorted_alloc_all[i]->hits * 100);
	l_bar 		= (float)(sorted_alloc_all[i]->hits * 100);

	orig_cal 	= (module != 8 && module != 9) ? (l_bar / g_hash_table_size(hash_table)) : (orig_cal / logger->total_process);
	l_bar 		= (l_bar / max);

	if (sorted_alloc_all[i]->module == 1) {
		strptime(sorted_alloc_all[i]->data, "%Y%m%d", &tm);
		strftime(buf, sizeof(buf), "%d/%b/%Y", &tm);
		mvwprintw(main_win,y, 18, "%s", buf);
	} else {
		mvwprintw(main_win,y, 18, "%s", sorted_alloc_all[i]->data);
		/* get http status code */
		if (sorted_alloc_all[i]->module == 9) {
			mvwprintw(main_win, y, 23, "%s", verify_status_code(sorted_alloc_all[i]->data));
		}
	}

	init_pair(7, COLOR_YELLOW, -1);	
	mvwprintw(main_win, y, 2, "%d", sorted_alloc_all[i]->hits);
	(sorted_alloc_all[i]->hits == max) ? wattron(main_win,COLOR_PAIR(7)) : wattron(main_win,COLOR_PAIR(3));

	mvwprintw(main_win, y, 10, "%4.2f%%", orig_cal);
	(sorted_alloc_all[i]->hits == max) ? wattroff(main_win,COLOR_PAIR(7)) : wattroff(main_win,COLOR_PAIR(3));

	if (sorted_alloc_all[i]->module == 9) return;	
	scr_cal = (float) ((col - 38));
	scr_cal = (float) scr_cal / 100;
	l_bar = l_bar * scr_cal;
	/*if (orig_cal < 1 || l_bar < 1 ) l_bar = 1;*/

	for (r = 0, xx = 35; r < (int) l_bar; r++, xx++) {
		wattron(main_win,COLOR_PAIR(4));
		mvwprintw(main_win, y, xx, "|");
		wattroff(main_win,COLOR_PAIR(4));
	}
}

int get_max_value(struct stu_alloc_all **sorted_alloc_all, struct logger *logger, int module)
{
	int i, temp = 0;
	for (i = 0; i < logger->alloc_counter; i++) {
		if (sorted_alloc_all[i]->module == module) {
			if (sorted_alloc_all[i]->hits > temp)
				temp = sorted_alloc_all[i]->hits;
		}
	}
	return temp;	
}

/* ###NOTE: Modules 6, 7 are based on module 1 totals 
   this way we avoid the overhead of adding them up */
void display_content(WINDOW *main_win, struct stu_alloc_all **sorted_alloc_all, 
					 struct logger *logger)
{
	int i, x, y, max = 0, until = 0;
	
	getmaxyx(main_win,y,x);
	init_pair(3, COLOR_RED, -1);
	init_pair(4, COLOR_GREEN, -1);

	until = (logger->alloc_counter > y) ? y : logger->alloc_counter;

	for (i = 0; i < until; i++) {
		if (sorted_alloc_all[i]->hits != 0)
			mvwprintw(main_win, i, 2, "%d", sorted_alloc_all[i]->hits);
		/* draw headers */
		if ((i % 10) == 0)
			draw_header(main_win, sorted_alloc_all[i]->data, 0, i, x, 1);
		else if ((i % 10) == 1) {
			draw_header(main_win, sorted_alloc_all[i]->data, 0, i, x, 2);
		} else if (((sorted_alloc_all[i]->module == 1)) &&	
				   ((i % 10 >= 3) && (i % 10 <=8) && (sorted_alloc_all[i]->hits != 0))) {

				max = get_max_value(sorted_alloc_all, logger, 1);
				create_graphs(main_win, sorted_alloc_all, logger, i, 1, max);
		} else if (((sorted_alloc_all[i]->module == 6)) &&	
				   ((i % 10 >= 3) && (i % 10 <=8) && (sorted_alloc_all[i]->hits != 0))) {

				max = get_max_value(sorted_alloc_all, logger, 6);
				create_graphs(main_win, sorted_alloc_all, logger, i, 1, max);
		} else if (((sorted_alloc_all[i]->module == 7)) &&	
				   ((i % 10 >= 3) && (i % 10 <=8) && (sorted_alloc_all[i]->hits != 0))) {

				max = get_max_value(sorted_alloc_all, logger, 7);
				create_graphs(main_win, sorted_alloc_all, logger, i, 1, max);
		} else if (((sorted_alloc_all[i]->module == 8)) &&	
				   ((i % 10 >= 3) && (i % 10 <=8) && (sorted_alloc_all[i]->hits != 0))) {

				max = get_max_value(sorted_alloc_all, logger, 8);
				create_graphs(main_win, sorted_alloc_all, logger, i, 8, max);
		} else if (((sorted_alloc_all[i]->module == 9)) &&	
				   ((i % 10 >= 3) && (i % 10 <=8) && (sorted_alloc_all[i]->hits != 0))) {

				max = get_max_value(sorted_alloc_all, logger, 9);
				create_graphs(main_win, sorted_alloc_all, logger, i, 9, max);
		} else mvwprintw(main_win, i, 10, "%s", sorted_alloc_all[i]->data);
	}
}

/* ###NOTE: Modules 6, 7 are based on module 1 totals 
   this way we avoid the overhead of adding them up */
void do_scrolling(WINDOW *main_win, struct stu_alloc_all **sorted_alloc_all, 
				  struct logger *logger, struct scrolling *scrolling, int cmd)
{
	int cur_y, cur_x, y, x, max = 0;
	getyx(main_win, cur_y, cur_x);
	getmaxyx(main_win,y,x);
	
	switch (cmd)
	{
		/* scroll down main window by repositioning cursor */
		case 1: 
			if (!(scrolling->scrl_main_win < logger->alloc_counter)) return;
			scrollok(main_win, TRUE);
			wscrl(main_win,1);
			scrollok(main_win, FALSE);
			
			if ((scrolling->scrl_main_win % 10) == 0)
				draw_header(main_win,sorted_alloc_all[scrolling->scrl_main_win]->data, 0, cur_y, x,1);
			else if ((scrolling->scrl_main_win % 10) == 1)
				draw_header(main_win,sorted_alloc_all[scrolling->scrl_main_win]->data, 0, cur_y, x,2);
			else if ((scrolling->scrl_main_win % 10) != 2 && (scrolling->scrl_main_win % 10)!=9 && (sorted_alloc_all[scrolling->scrl_main_win]->hits != 0)) {
	
				if (sorted_alloc_all[scrolling->scrl_main_win]->module == 1) {
					max = get_max_value(sorted_alloc_all, logger, 1);
					create_graphs(main_win, sorted_alloc_all, logger, scrolling->scrl_main_win, 1, max);
				} else if (sorted_alloc_all[scrolling->scrl_main_win]->module == 6) {
					max = get_max_value(sorted_alloc_all, logger, 6);
					create_graphs(main_win, sorted_alloc_all, logger, scrolling->scrl_main_win, 1, max);
				} else if (sorted_alloc_all[scrolling->scrl_main_win]->module == 7) {
					max = get_max_value(sorted_alloc_all, logger, 7);
					create_graphs(main_win, sorted_alloc_all, logger, scrolling->scrl_main_win, 1, max);
				} else if (sorted_alloc_all[scrolling->scrl_main_win]->module == 8) {
					max = get_max_value(sorted_alloc_all, logger, 8);
					create_graphs(main_win, sorted_alloc_all, logger, scrolling->scrl_main_win, 8, max);
				} else if (sorted_alloc_all[scrolling->scrl_main_win]->module == 9 && http_status_code_flag) {
					max = get_max_value(sorted_alloc_all, logger, 9);
					create_graphs(main_win, sorted_alloc_all, logger, scrolling->scrl_main_win, 9, max);
				} else {
					mvwprintw(main_win, cur_y, 10, "%s", sorted_alloc_all[scrolling->scrl_main_win]->data);
					mvwprintw(main_win, cur_y, 2, "%d", sorted_alloc_all[scrolling->scrl_main_win]->hits);
				}
			}
			scrolling->scrl_main_win++;
			break;
		/* scroll up main window by repositioning cursor */
		case 0:
			if (!((scrolling->scrl_main_win) - y > 0)) return;
			scrollok(main_win, TRUE);
			wscrl(main_win, -1);
			scrollok(main_win, FALSE);
				
			if (((scrolling->scrl_main_win-y-1) % 10) == 0)
				draw_header(main_win,sorted_alloc_all[scrolling->scrl_main_win-y-1]->data, 0, 0, x,1);
			else if (((scrolling->scrl_main_win-y-1) % 10) == 1)
				draw_header(main_win,sorted_alloc_all[scrolling->scrl_main_win-y-1]->data, 0, 0, x,2);
			else if (((scrolling->scrl_main_win-y-1) % 10)!=2 && ((scrolling->scrl_main_win-y-1) % 10)!=9 && (sorted_alloc_all[scrolling->scrl_main_win-y-1]->hits !=0)) {
	
				if (sorted_alloc_all[scrolling->scrl_main_win-y-1]->module == 1) {
					max = get_max_value(sorted_alloc_all, logger, 1);
					create_graphs(main_win, sorted_alloc_all, logger, scrolling->scrl_main_win-y-1, 1, max);
				} else if (sorted_alloc_all[scrolling->scrl_main_win-y-1]->module == 6) {
					max = get_max_value(sorted_alloc_all, logger, 6);
					create_graphs(main_win, sorted_alloc_all, logger, scrolling->scrl_main_win-y-1, 1, max);
				} else if (sorted_alloc_all[scrolling->scrl_main_win-y-1]->module == 7) {
					max = get_max_value(sorted_alloc_all, logger, 7);
					create_graphs(main_win, sorted_alloc_all, logger, scrolling->scrl_main_win-y-1, 1, max);
				} else if (sorted_alloc_all[scrolling->scrl_main_win-y-1]->module == 8) {
					max = get_max_value(sorted_alloc_all, logger, 8);
					create_graphs(main_win, sorted_alloc_all, logger, scrolling->scrl_main_win-y-1, 8, max);
				} else if (sorted_alloc_all[scrolling->scrl_main_win-y-1]->module == 9 && http_status_code_flag) {
					max = get_max_value(sorted_alloc_all, logger, 9);
					create_graphs(main_win, sorted_alloc_all, logger, scrolling->scrl_main_win-y-1, 9, max);
				} else {
					mvwprintw(main_win, 0, 10, "%s", sorted_alloc_all[scrolling->scrl_main_win-y-1]->data);
					mvwprintw(main_win, 0, 2, "%d", sorted_alloc_all[scrolling->scrl_main_win-y-1]->hits);
				}
			}
			scrolling->scrl_main_win--;
			break;
	}
}

void load_help_popup_content(WINDOW *inner_win, int where, struct scrolling *scrolling)
{
	int y, x;

	getmaxyx(inner_win, y, x);
	switch (where) {
		case 1: /* scroll down */
			if (((size_t) (scrolling->scrl_help_win - 5)) >= help_main_size()) 
				return;	
			scrollok(inner_win, TRUE);
			wscrl(inner_win, 1);
			scrollok(inner_win, FALSE);
			wmove(inner_win, y - 1, 2);
			/* minus help_win offset - 5 */
			mvwaddstr(inner_win, y - 1, 2, help_main[scrolling->scrl_help_win - 5]); 
			scrolling->scrl_help_win++;
			break;
		case 0: /* scroll up */ 
			if ((scrolling->scrl_help_win - y) - 5 <= 0) 
			return;	
			scrollok(inner_win, TRUE);
			wscrl(inner_win, -1);
			scrollok(inner_win, FALSE);
			wmove(inner_win, 0, 2);
			/* minus help_win offset - 6 */
			mvwaddstr(inner_win, 0, 2, help_main[(scrolling->scrl_help_win  - y) - 6]);
			scrolling->scrl_help_win--; 
			break;
	}
	wrefresh(inner_win);
}

void load_help_popup(WINDOW *help_win)
{
	WINDOW *inner_win;
	int y, x, c;
	struct scrolling scrolling;
	
	getmaxyx(help_win, y, x);
	draw_header(help_win,"  Use cursor UP/DOWN - PGUP/PGDOWN to scroll. q:quit", 0, 1, x, 2);
	wborder(help_win, '|', '|', '-', '-', '+', '+', '+', '+');

	inner_win = newwin( y - 5, x - 4, 11, 21); 
 
	int i, m = 0;
	for (i = 0; i < y ; i++, m++)
		mvwaddstr(inner_win, m, 2 ,help_main[i]);

	scrolling.scrl_help_win = y;
	wmove(help_win, y, 0);
	wrefresh(help_win);
	wrefresh(inner_win);

	while ((c = wgetch(stdscr)) != 'q') {
		switch (c) {
			case KEY_DOWN:
				(void) load_help_popup_content(inner_win, 1, &scrolling);
				break;
			case KEY_UP:
				(void) load_help_popup_content(inner_win, 0, &scrolling);
				break;
		}
		wrefresh(help_win);
	}
}

void load_reverse_dns_popup(WINDOW *ip_detail_win, char *addr)
{
	int y, x, c;
	char *my_addr = reverse_ip(addr);
	const char *location;

	getmaxyx(ip_detail_win, y, x);
	draw_header(ip_detail_win,"  Reverse DNS lookup -  q:quit", 0, 1, x-1,2);
	wborder(ip_detail_win, '|', '|', '-', '-', '+', '+', '+', '+');
	mvwprintw(ip_detail_win, 3, 2, "Reverse DNS for address: %s",addr);
	mvwprintw(ip_detail_win, 4, 2, "%s", my_addr);

	/* geolocation data */
	GeoIP * gi;
	gi = GeoIP_new(GEOIP_STANDARD);
	location = GeoIP_country_name_by_name(gi, addr);
	GeoIP_delete(gi);
	if (location == NULL) location = "Not found";
	mvwprintw(ip_detail_win, 5, 2, "Country: %s", location);
	free(my_addr);

	wrefresh(ip_detail_win);
	if ((c = wgetch(stdscr)) != 'q') return;
}

ITEM **get_menu_items(struct stu_alloc_holder **sorted_alloc_holder,
                 struct logger *logger, int choices, int sort)
{
	int i;
	char *hits = NULL;
	char buf[12] = "";
	char *buffer_date = NULL; /* max possible size of date */
	struct tm tm;
	ITEM **items;

	memset (&tm, 0, sizeof (tm));

	/* sort the struct prior to display */
	if (sort)	
		qsort(sorted_alloc_holder, logger->counter, sizeof(struct stu_alloc_holder*), struct_cmp_by_hits);
	else 
		qsort(sorted_alloc_holder, logger->counter, sizeof(struct stu_alloc_holder*), struct_cmp);

	items = (ITEM **) malloc (sizeof (ITEM *) * (choices + 1));
	
	for (i = 0; i < choices; ++i) {
		hits = (char *) malloc (sizeof(char) * 8);
		if (hits == NULL) exit (1); 
		sprintf (hits, "%3i", sorted_alloc_holder[i]->hits);
		if (logger->current_module == 1) {
			buffer_date = (char *) malloc (sizeof(char) * 13); 
			if (buffer_date == NULL) exit (1); 
			strptime(sorted_alloc_holder[i]->data, "%Y%m%d", &tm);
			strftime(buf, sizeof(buf), "%d/%b/%Y", &tm);
			sprintf (buffer_date, "%s", buf);
			items[i] = new_item(hits, buffer_date);
		} else items[i] = new_item(hits, sorted_alloc_holder[i]->data);
	}

	items[i] = (ITEM *) NULL;

	return items;
}

MENU *set_menu(WINDOW *my_menu_win, ITEM ** items, struct logger *logger)
{
	MENU *my_menu = NULL;
	int x = 0;
	int y = 0;

	getmaxyx (my_menu_win, y, x);
	my_menu = new_menu (items);

	/* create the window to be associated with the menu */
	keypad(my_menu_win, TRUE);

	/* set main window and sub window */
	set_menu_win(my_menu, my_menu_win);
	set_menu_sub(my_menu, derwin(my_menu_win, y - 6, x - 2, 4, 1));
	set_menu_format(my_menu, y - 6, 1);

	/* set menu mark to the string " * " */
	set_menu_mark(my_menu, " => ");

	draw_header(my_menu_win,"  Use cursor UP/DOWN - PGUP/PGDOWN to scroll. q:quit", 0, 1, x, 2);
	draw_header(my_menu_win, module_names[logger->current_module - 1], 0, 2, x, 1);
	wborder(my_menu_win, '|', '|', '-', '-', '+', '+', '+', '+');
	return my_menu;
}

void load_popup_content(WINDOW *my_menu_win, int choices,
						struct stu_alloc_holder **sorted_alloc_holder, 
						struct logger *logger, int sort)
{
	wclrtoeol(my_menu_win);
	items = get_menu_items(sorted_alloc_holder, logger, choices, sort);
	my_menu = set_menu(my_menu_win, items, logger);
	post_menu(my_menu);
	wrefresh(my_menu_win);
}

void load_popup_free_items(ITEM ** items, struct logger *logger)
{
	int i;
	char *name = NULL;
	char *description = NULL;
	
	/* clean up stuff */
	i = 0;
	while ((ITEM *) NULL != items[i]) {
		name = (char *) item_name (items[i]);
		free (name);
		if (logger->current_module == 1) {
			description = (char *) item_description (items[i]);
			free (description);
		}
		free_item(items[i]);
		i++;
	}
	free(items);
	items = NULL;
}

static ITEM *search_request(MENU *my_menu, const char *input)
{
	char *str = NULL;
	str = strdup(input);

	ITEM *item_ptr = NULL;

	if (input != NULL) { 
		int i = -1, j = -1, response = 0;
		j = item_index(current_item(my_menu));

		for (i = j + 1; i < item_count(my_menu) && !response; i++) {
			if (strstr(item_description(menu_items(my_menu)[i]), input)) response = 1;
		}
		if (response) item_ptr =  menu_items(my_menu)[i - 1];
	}
	free(str);
	return item_ptr;
}

void load_popup(WINDOW *my_menu_win,  struct stu_alloc_holder **sorted_alloc_holder, struct logger *logger)
{
	WINDOW *ip_detail_win;
	ITEM *query = NULL;	
	
	/*###TODO: perhaps let the user change the size of the popup choices
	 * however, tail implementation may vary  */
	int choices = MAX_CHOICES, c, x, y;
	char input[BUFFER] = "";
	
	GHashTable *hash_table;
	switch (logger->current_module)
	{
		case UNIQUE_VISITORS:
			hash_table = ht_unique_vis;
			break;
		case REQUESTS:
			hash_table = ht_requests;
			break;
		case REQUESTS_STATIC:
			hash_table = ht_requests_static;
			break;
		case REFERRERS:
			hash_table = ht_referers;
			break;
		case NOT_FOUND:
			hash_table = ht_not_found_requests;
			break;
		case OS:
			hash_table = ht_os;
			break;
		case BROWSERS:
			hash_table = ht_browsers;
			break;
		case HOSTS:
			hash_table = ht_hosts;
			break;
		case STATUS_CODES:
			hash_table = ht_status_code;
			break;
		case REFERRING_SITES:
			hash_table = ht_referring_sites;
			break;
	}
	getmaxyx(my_menu_win, y, x);
	ALLOCATE_STRUCT(sorted_alloc_holder, g_hash_table_size(hash_table));
	
	int i = 0;
	GHashTableIter iter;
	gpointer k = NULL;
	gpointer v = NULL;
	
	g_hash_table_iter_init (&iter, hash_table);
	while (g_hash_table_iter_next (&iter, &k, &v)) {
		/* ###FIXME: 64 bit portability issues might arise */
		sorted_alloc_holder[i]->data = (gchar *)k;
		sorted_alloc_holder[i++]->hits = GPOINTER_TO_INT(v);
		logger->counter++;
	}

	/* letting the user to set the max number might be a better way to go */
	choices = (g_hash_table_size(hash_table) > 100) ? MAX_CHOICES : g_hash_table_size(hash_table);
	load_popup_content(my_menu_win, choices, sorted_alloc_holder, logger, 1);

	while ((c = wgetch(stdscr)) != 'q') {
		switch (c) {
			case KEY_DOWN:
				menu_driver(my_menu, REQ_DOWN_ITEM);
				break;
			case KEY_UP:
				menu_driver(my_menu, REQ_UP_ITEM);
				break;
			case KEY_NPAGE:
				menu_driver(my_menu, REQ_SCR_DPAGE);
				break;
			case KEY_PPAGE:
				menu_driver(my_menu, REQ_SCR_UPAGE);
				break;
			case '/':
				/* set the whole ui for search */
				init_pair(12, COLOR_CYAN, -1);
				init_pair(13, COLOR_WHITE, COLOR_RED);

				wattron(my_menu_win, COLOR_PAIR(12));
				mvwhline(my_menu_win, y - 2, 2,' ', x - 4);
				mvwaddnstr(my_menu_win, y - 2, 2, "/", 20);
				wattroff(my_menu_win,COLOR_PAIR(12));
				nocbreak(); 
				echo();
				curs_set(1);
				wattron(my_menu_win, COLOR_PAIR(12));
				wscanw(my_menu_win, "%s", input);
				wattroff(my_menu_win,COLOR_PAIR(12));
				cbreak(); 
				noecho();
				curs_set(0);
				
				query = search_request(my_menu, input);
				if (query != NULL) {
					while (FALSE == item_visible (query))
						menu_driver(my_menu, REQ_SCR_DPAGE);
					set_current_item(my_menu, query);
				} else {
					wattron(my_menu_win, COLOR_PAIR(13));
					mvwhline(my_menu_win,y - 2, 2, ' ', x - 4);
					mvwaddnstr(my_menu_win, y - 2, 2, "Pattern not found", 20);
					wattroff(my_menu_win,COLOR_PAIR(13));
				}
				break;
			case 'n':
				if (strlen(input) == 0) break;
				query = search_request(my_menu, input);
				if (query != NULL) {
					while (FALSE == item_visible (query))
						menu_driver(my_menu, REQ_SCR_DPAGE);
					set_current_item(my_menu, query);
				} else {
					wattron(my_menu_win, COLOR_PAIR(13));
					mvwhline(my_menu_win,y - 2,2,' ', x - 4);
					mvwaddnstr(my_menu_win, y - 2, 2, "search hit BOTTOM", 20);
					wattroff(my_menu_win,COLOR_PAIR(13));
				}
				break;
			case 116:
				menu_driver (my_menu, REQ_FIRST_ITEM);
				break;
			case 98:
				menu_driver (my_menu, REQ_LAST_ITEM);
				break;
			case 's':
				if (logger->current_module != 1) break;
				unpost_menu (my_menu);
				free_menu (my_menu);
				my_menu = NULL;
				load_popup_free_items(items, logger);

				load_popup_content(my_menu_win, choices, sorted_alloc_holder, logger, 0);
				break;
			case 'S':
				if (logger->current_module != 1) break;
				unpost_menu (my_menu);
				free_menu (my_menu);
				my_menu = NULL;
				load_popup_free_items(items, logger);
				
				load_popup_content(my_menu_win, choices, sorted_alloc_holder, logger, 1);
				break;
			case 10:
			case KEY_RIGHT:
				if (logger->current_module != 8) break;
				
				ITEM *cur;
				cur = current_item(my_menu);

				/* no current item here? break it */
				if (cur == NULL) break;
	
				ip_detail_win = newwin( y - 13, x - 5, 10, 21); 
				char addrs [32];
				sprintf (addrs, "%s", item_description(cur));		
				load_reverse_dns_popup(ip_detail_win, addrs);	
				pos_menu_cursor(my_menu);
				wrefresh(ip_detail_win);
				touchwin(my_menu_win);
				close_win(ip_detail_win);	
				break;
		}
		wrefresh(my_menu_win);
	}
	
	int f;
	for (f = 0; f < logger->counter; f++)
		free(sorted_alloc_holder[f]);
	free(sorted_alloc_holder);
	logger->counter = 0;
	
	/* unpost and free all the memory taken up */
	unpost_menu(my_menu);
	free_menu(my_menu);

	/* clean up stuff */
	my_menu = NULL;
	load_popup_free_items(items, logger);
}
