/** 
 * goaccess.c -- main log analyzer 
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
 * @version 0.2
 * Last Modified: Saturday, July 10, 2010
 * Path:  /goaccess.c
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

#include <string.h>
#include <stdlib.h>
#include <curses.h>
#include <glib.h>
#include <stdint.h>
#include <sys/types.h>
#include <menu.h>
#include <assert.h>
#include <unistd.h>
#include <ctype.h>

#include "error.h"
#include "alloc.h"
#include "commons.h"
#include "util.h"
#include "ui.h"
#include "parser.h"

static WINDOW *header_win, *main_win, *my_menu_win, *help_win;

static struct logger *logger;
static struct scrolling scrolling;
static struct stu_alloc_all **sorted_alloc_all;
static struct stu_alloc_holder **sorted_alloc_holder;

/* frees the memory allocated by the GHashTable */
static void free_key_value(gpointer old_key, GO_UNUSED gpointer old_value, GO_UNUSED gpointer user_data)
{
	g_free(old_key);
}

static struct logger *init_struct(void)
{
	struct logger *logger;

	if ( (logger = malloc(sizeof(*logger) ) ) == NULL)	return NULL;

	logger->total_process	= 0;
	logger->total_invalid	= 0;
	logger->counter			= 0;
	logger->alloc_counter	= 0;
	logger->current_module	= 1;

	return logger; 
}

static void cmd_help(void)
{
	printf("\nGoAccess - %s\n\n", GO_VERSION);
	printf("Usage: ");
	printf("goaccess [ -b ][ -s ][ -e IP_ADDRESS][ -f log_file ]\n\n");
	printf("The following options can also be supplied to the command:\n\n");
	printf("  -f  - Path to input log <filename> \n");
	printf("  -b  - Enable total bandwidth consumption. To achieve faster parsing, do not enable this flag. \n");
	printf("  -s  - Enable/report HTTP status codes. To avoid overhead while parsing, \n");
	printf(" 	    this has been disabled by default. \n");
	printf("  -e  - Exclude an IP from being counted under the HOST module. \n");
	printf("        This has been disabled by default. \n\n");
	printf("For more details visit: http://goaccess.prosoftcorp.com \n\n");
}

static void house_keeping(struct logger *logger, struct stu_alloc_all **sorted_alloc_all)
{
	int f;
	for (f = 0; f < logger->alloc_counter; f++)
		free(sorted_alloc_all[f]->data);

	for (f = 0; f < 170; f++) 
		free(sorted_alloc_all[f]);

	assert(sorted_alloc_all != 0);
	free(sorted_alloc_all);
	free(logger);

	g_hash_table_destroy(ht_unique_vis);
	g_hash_table_destroy(ht_referers);
	g_hash_table_destroy(ht_requests);
	g_hash_table_destroy(ht_requests_static);
	g_hash_table_destroy(ht_not_found_requests); 
	g_hash_table_destroy(ht_unique_visitors); 
	g_hash_table_destroy(ht_os); 
	g_hash_table_destroy(ht_browsers); 
	g_hash_table_destroy(ht_hosts); 
	g_hash_table_destroy(ht_status_code); 
	g_hash_table_destroy(ht_referring_sites); 
}

static void get_keys(void)
{

	int y, x, c;
	getmaxyx(main_win, y, x);

	while ((c = wgetch(stdscr)) != 'q')	{  
		switch (c) {
			case KEY_DOWN: /* scroll down main_win */
				wmove(main_win, real_size_y - 1, 0);
				do_scrolling(main_win, sorted_alloc_all, logger, &scrolling, 1);
				break;
			case KEY_UP:  /* scroll up main_win */
				wmove(main_win, 0, 0);
				do_scrolling(main_win, sorted_alloc_all, logger, &scrolling, 0);
				break;
			case 9: 	/* tab - increment module counter */
				if (logger->current_module < TOTAL_MODULES)
					logger->current_module++;
				else if (logger->current_module == TOTAL_MODULES)
					logger->current_module = 1;
				update_header(header_win, logger->current_module);	
				break;
			case 353: 	/* tab - increment module counter */
				if (logger->current_module > 1 && logger->current_module <= TOTAL_MODULES)
					logger->current_module--;
				else if (logger->current_module == 1)
					logger->current_module = 10;
				update_header(header_win, logger->current_module);	
				break;
			case KEY_RIGHT:
			case 'o':
			case 'O':
				my_menu_win = create_win(main_win);
				load_popup(my_menu_win, sorted_alloc_holder, logger);
				touchwin(main_win);
				close_win(my_menu_win);	
				break;
			case 49: /* key 1 - module 1 */
				logger->current_module = 1;
				update_header(header_win, logger->current_module);	
				break;
			case 50: /* key 2 - module 2 */
				logger->current_module = 2;
				update_header(header_win, logger->current_module);	
				break;
			case 51: /* key 3 - module 3 */
				logger->current_module = 3;
				update_header(header_win, logger->current_module);	
				break;
			case 52: /* key 4 - module 4 */
				logger->current_module = 4;
				update_header(header_win, logger->current_module);	
				break;
			case 53: /* key 5 - module 5 */
				logger->current_module = 5;
				update_header(header_win, logger->current_module);	
				break;
			case 54: /* key 6 - module 6 */
				logger->current_module = 6;
				update_header(header_win, logger->current_module);	
				break;
			case 55: /* key 7 - module 7 */
				logger->current_module = 7;
				update_header(header_win, logger->current_module);	
				break;
			case 56: /* key 8 - module 8 */
				logger->current_module = 8;
				update_header(header_win, logger->current_module);	
				break;
			case 57: /* key 9 - module 9 */
				if (http_status_code_flag)	
				logger->current_module = 9;
				update_header(header_win, logger->current_module);	
				break;
			case 48: /* key 0 - module 10 */
				logger->current_module = 10;
				update_header(header_win, logger->current_module);	
				break;
			case 265:
				help_win = newwin( y - 12, x - 40, 8, 20);
				if (help_win == NULL)
					error_handler(__PRETTY_FUNCTION__, __FILE__, __LINE__, 
								  "Unable to allocate memory for new window.");
				load_help_popup(help_win);	
				wrefresh(help_win);
				touchwin(main_win);
				close_win(help_win);
				break;
			case 269:
			case KEY_RESIZE:
				term_size(main_win);
				scrolling.scrl_main_win = real_size_y;
				refresh();
				render_screens(); 
				break;
		}
		wrefresh(main_win);
	}
}

void render_screens(void)
{
	int row, col, x, y;

	wclear(main_win);	
	wclear(header_win);
	wclear(stdscr);

	getmaxyx(stdscr, row, col);
	term_size(main_win);

	generate_time();
	mvaddstr(row - 1, 1, "[F1]Help  [O]pen detail view");
	mvprintw(row - 1, col - 64, "Generated: %s", asctime(now_tm));
	mvaddstr(row - 1, col - 23, "[Q]uit Analyzer");
	mvaddstr(row - 1, col - 6, GO_VERSION);

	display_content(main_win, sorted_alloc_all, logger, scrolling);

	refresh();
	/* call general header so we can display it */
	display_general(header_win, logger, ifile);
	wrefresh(header_win);

	wrefresh(main_win);

	/* display active label based on current module */
	init_pair(6, COLOR_BLUE, COLOR_GREEN);
	wattron(header_win, COLOR_PAIR(6));
	wmove(header_win, 0, 30);
	mvwprintw(header_win, 0, col - 20, "[Active Module %d]", logger->current_module);
	wattroff(header_win, COLOR_PAIR(6));
	wrefresh(header_win);
}

int main(int argc, char *argv[]) 
{
	extern char *optarg;
	extern int optind, optopt, opterr;
	int row, col, o, bflag = 0, fflag = 0, sflag = 0;

	if (argc < 2) {
		cmd_help();
		exit(1);
	}
	
	opterr = 0;
	while ((o = getopt (argc, argv, "f:e:sb")) != -1) {
		switch (o) {
			case 'f':
				fflag = 1;
				ifile = optarg;
				break;
			case 'e':
				ignore_flag = 1;
				ignore_host = optarg;
				break;
			case 's':
				sflag = 1;
				http_status_code_flag = 1;
				break;
			case 'b':
				bflag = 1;
				bandwidth_flag = 1;
				break;
			case '?':
				if (isprint(optopt)) 
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				else 
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
				return 1;
			default:
				abort();
		}
	}
	
	if (!fflag) {
		fprintf(stderr, "goaccess version %s\n", GO_VERSION);
		cmd_help();
		exit(1);
	}

	/* initialize hash tables */
	ht_unique_visitors 	  = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) free_key_value, NULL);
	ht_requests 		  = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) free_key_value, NULL);
	ht_referers 		  = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) free_key_value, NULL);
	ht_unique_vis 		  = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) free_key_value, NULL);
	ht_requests_static 	  = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) free_key_value, NULL);
	ht_not_found_requests = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) free_key_value, NULL);
	ht_os 				  = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) free_key_value, NULL);
	ht_browsers			  = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) free_key_value, NULL);
	ht_hosts			  = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) free_key_value, NULL);
	ht_status_code		  = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) free_key_value, NULL);
	ht_referring_sites 	  = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) free_key_value, NULL);
	
	initscr();
	clear();
	if (has_colors() == FALSE)
		error_handler(__PRETTY_FUNCTION__, __FILE__, __LINE__, 
					  "Your terminal does not support color");
	start_color();
	noecho();
	cbreak();
	keypad(stdscr, TRUE);
	use_default_colors();

	init_pair(5, COLOR_WHITE, -1);
	attron(COLOR_PAIR(5));

	getmaxyx(stdscr, row, col);
	if (row < MIN_HEIGHT || col < MIN_WIDTH) 
		error_handler(__PRETTY_FUNCTION__, __FILE__, __LINE__, 
					  "Minimum screen size - 97 columns by 40 lines");

	header_win = newwin(5, col, 0, 0);
	keypad(header_win, TRUE);
	if (header_win == NULL)
		error_handler(__PRETTY_FUNCTION__, __FILE__, __LINE__, 
					  "Unable to allocate memory for new window.");

	main_win = newwin(row - 7, col, 6, 0);
	keypad(main_win, TRUE);
	if (main_win == NULL)
		error_handler(__PRETTY_FUNCTION__, __FILE__, __LINE__, 
					  "Unable to allocate memory for new window.");

	/* main processing event */
	(void) time(&start_proc);	
	logger = init_struct();	
	if (parse_log(logger, ifile)) {
		error_handler(__PRETTY_FUNCTION__, __FILE__, __LINE__, 
					  "Error while processing file");
	}

	/* allocate per module */	
	ALLOCATE_STRUCT(sorted_alloc_all, 170);

	/* note that the order in which we call them, that is the way modules will be displayed */	
	ALLOCATE_STRUCT(sorted_alloc_holder, g_hash_table_size(ht_unique_visitors));
	generate_unique_visitors(main_win,sorted_alloc_holder, sorted_alloc_all, logger);

	ALLOCATE_STRUCT(sorted_alloc_holder, g_hash_table_size(ht_requests));
	generate_struct_data(ht_requests, sorted_alloc_holder, sorted_alloc_all, logger, 2);

	ALLOCATE_STRUCT(sorted_alloc_holder, g_hash_table_size(ht_requests_static));
	generate_struct_data(ht_requests_static, sorted_alloc_holder, sorted_alloc_all, logger, 3);

	ALLOCATE_STRUCT(sorted_alloc_holder, g_hash_table_size(ht_referers));
	generate_struct_data(ht_referers, sorted_alloc_holder, sorted_alloc_all, logger, 4);

	ALLOCATE_STRUCT(sorted_alloc_holder, g_hash_table_size(ht_not_found_requests));
	generate_struct_data(ht_not_found_requests, sorted_alloc_holder, sorted_alloc_all, logger, 5);

	ALLOCATE_STRUCT(sorted_alloc_holder, g_hash_table_size(ht_os));
	generate_struct_data(ht_os, sorted_alloc_holder, sorted_alloc_all, logger, 6);

	ALLOCATE_STRUCT(sorted_alloc_holder, g_hash_table_size(ht_browsers));
	generate_struct_data(ht_browsers, sorted_alloc_holder, sorted_alloc_all, logger, 7);

	ALLOCATE_STRUCT(sorted_alloc_holder, g_hash_table_size(ht_hosts));
	generate_struct_data(ht_hosts, sorted_alloc_holder, sorted_alloc_all, logger, 8);
		
	if (!http_status_code_flag) goto nohttpstatuscode;
	ALLOCATE_STRUCT(sorted_alloc_holder, g_hash_table_size(ht_status_code));
	generate_struct_data(ht_status_code, sorted_alloc_holder, sorted_alloc_all, logger, 9);

	nohttpstatuscode:;

	ALLOCATE_STRUCT(sorted_alloc_holder, g_hash_table_size(ht_referring_sites));
	generate_struct_data(ht_referring_sites, sorted_alloc_holder, sorted_alloc_all, logger, 10);

	(void) time(&end_proc);
	/* draw screens */
	int x, y;
	getmaxyx(main_win,y,x);

	scrolling.scrl_main_win = y;
	scrolling.init_scrl_main_win = 0;
	render_screens();
	get_keys();

	house_keeping(logger, sorted_alloc_all);
	attroff(COLOR_PAIR(5));

	/* restore tty modes and reset 
	 * terminal into non-visual mode */
	endwin();
	return 0;
}
