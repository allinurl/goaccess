/**
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
 * @version 0.2
 * Last Modified: Sunday, July 25, 2010
 * Path:  /util.h
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

#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

char *substring (const char *str, int begin, int len);

char *alloc_string (const char *str);

char *reverse_ip (char *str);

off_t file_size (const char *filename);

const char *verify_os (char *str);

const char *verify_browser (char *str);

char *verify_status_code (char *str);

char *trim_str (char *str);

char *clean_date_time (char *s);

char *clean_date (char *s);

char *clean_time (char *s);

char *clean_status (char *s);

#endif
