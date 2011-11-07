/**
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - Ncurses apache weblog analyzer & interactive viewer
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

#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

char *alloc_string (const char *str);
char *clean_date (char *s);
char *clean_date_time (char *s);
char *clean_month (char *s);
char *clean_status (char *s);
char *clean_time (char *s);
char *filesize_str (off_t log_size);
char *convert_date (char *result, char *data, int size);
char *reverse_ip (char *str);
char *substring (const char *str, int begin, int len);
char *trim_str (char *str);
char *verify_browser (char *str);
char *verify_os (char *str);
char *verify_status_code (char *str);
int count_occurrences (const char *s1, char c);
off_t file_size (const char *filename);

#endif
