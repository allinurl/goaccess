/** 
 * util.c -- a set of handy functions to help parsing 
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
 * @version 0.2
 * Last Modified: Saturday, July 10, 2010
 * Path:  /util.c
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

#define _LARGEFILE64_SOURCE 1
#define _FILE_OFFSET_BITS 64

#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <stropts.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#include "util.h"
#include "commons.h"
#include "error.h"

/* helper functions */
char *substring(const char *str, int begin, int len) 
{
	char *buffer;
	if (str == NULL) return NULL;
	if (begin < 0)
		begin = strlen (str) + begin;
	if (begin < 0)
		begin = 0;
	if (len < 0)
		len = 0;
	if (((size_t) begin) > strlen (str))
		begin = strlen (str);
	if (((size_t) len) > strlen (&str[begin]))
		len = strlen (&str[begin]);
	if ((buffer = malloc (len + 1)) == NULL)
		return NULL;
	memcpy (buffer, &(str[begin]), len);
	buffer[len] = '\0';
	
	return buffer;
}

char *alloc_string(const char* str)
{
    char *new = malloc(strlen(str) + 1);
	if (new == NULL)	
		error_handler(__PRETTY_FUNCTION__, __FILE__, __LINE__, 
					  "Unable to allocate memory");
    strcpy(new, str);        
    return new;
}

char *reverse_ip(char *str)
{
	char *p;
	struct hostent *hent;
	int is_valid = 1, dots = 0;
	unsigned int addr = 0;

	if (str == NULL || *str == '\0') return (NULL);

	/* double check and make sure we got an ip in here */	
	for (p = str; *p; p++) {
		if (*p == '.') {
			dots++;
			continue;
		} else if (!isdigit(*p)) {			
			is_valid = 0;
			break;
		}
	}
	if (dots != 3)	is_valid = 0;

	if (is_valid) {
		addr = inet_addr(str);
		hent = gethostbyaddr((char *) &addr, sizeof(unsigned int), AF_INET);
	} else hent = gethostbyname(str);

	return (hent != NULL ? strdup(hent->h_name) : NULL);
}

/* off_t becomes 64 bit aware */
off_t file_size(const char *filename) 
{
    struct stat st;

    if (stat(filename, &st) == 0)
        return st.st_size;
    
	fprintf(stderr, "Cannot determine size of %s: %s\n", filename, strerror(errno));

    return -1;
}

const char *verify_os(char * str) 
{
	size_t i;
	for (i = 0; i < os_size(); i++)
		if (strstr(str, os[i]) != NULL) return os[i];
	return "Unknown";
}

const char *verify_browser(char * str) 
{
	size_t i;
	for (i = 0; i < browsers_size(); i++)
		if (strstr(str, browsers[i]) != NULL) return browsers[i];
	return "Unknown";
}

char *verify_status_code(char *str)
{
	size_t i;
	for (i = 0; i < codes_size(); i++)
		if (strstr(str, codes[i][0]) != NULL) return codes[i][1];
	return "Unknown";
}

char *trim_str(char *str) 
{
    char *p;
    if (!str)
        return NULL; 
    if (!*str)
        return str;
    for (p = str + strlen(str) - 1; (p >= str) && isspace(*p); --p);
	    p[1] = '\0';
    return str;
}

char *clean_date_time(char *s)
{
	return substring(s + 1, 0, 20);
}

char *clean_date(char *s)
{
	return substring(s + 1, 0, 8);
}

char *clean_time(char *s)
{
	return substring(s, 13, 8);
}

char *clean_status(char *s)
{
	return substring(s, 0, 3);
}
