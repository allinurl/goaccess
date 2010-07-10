/** 
 * error.c -- error handling
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
 * @version 0.1
 * Last Modified: Saturday, July 10, 2010
 * Path:  /error.c
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

#include <curses.h>
#include <stdio.h>
#include <stdlib.h> 

#include "error.h"
#include "commons.h"

void error_handler(const char *func, char *file, int line, char *msg)
{
	(void) endwin();
	
	fprintf(stderr, "\nGoAccess - version %s %s %s\n", GO_VERSION, __DATE__, __TIME__);
	fprintf(stderr, "\nAn error has occurred");
	fprintf(stderr, "\nError occured at: %s - %s - %d", file, func, line);
	fprintf(stderr, "\nMessage: %s\n\n", msg);
	abort();
}
