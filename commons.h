/**
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
 * @version 0.1
 * Last Modified: Wednesday, July 07, 2010
 * Path:  /commons.h
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

#include <glib.h>

#ifndef COMMONS_H_INCLUDED
#define COMMONS_H_INCLUDED

#define DEBUG
#define BUFFER 			4096
#define MAX_CHOICES 	100
#define TOTAL_MODULES 	10
#define GO_VERSION 		"0.1"

#define KB (1024)
#define MB (KB * 1024)
#define GB (MB * 1024)

enum MODULES {
	UNIQUE_VISITORS = 1,
	REQUESTS,
	REQUESTS_STATIC,
	REFERRERS,
	NOT_FOUND,
	OS,
	BROWSERS,
	HOSTS,
	STATUS_CODES,
	REFERRING_SITES
};
extern enum MODULES modules;

/* to create a new hash value out of a key, which can be NULL */
extern GHashTable *ht_unique_visitors;
extern GHashTable *ht_requests; 
extern GHashTable *ht_referers;
extern GHashTable *ht_unique_vis;
extern GHashTable *ht_requests_static;
extern GHashTable *ht_not_found_requests;
extern GHashTable *ht_os;
extern GHashTable *ht_browsers;
extern GHashTable *ht_hosts;
extern GHashTable *ht_status_code;
extern GHashTable *ht_referring_sites;

struct logger {
	char *host;
	char *identd;
	char *userid;
	char *hour;
	char *date;
	char *request;
	char *status;
	char *referer;
	char *agent;
	int total_process;
	int total_invalid;
	int counter;
	int alloc_counter;
	int current_module;
	int max_value;
};
/*extern struct logger logger;  */

struct stu_alloc_all {
	char *data;
	int  hits;
	int module;
};

struct stu_alloc_holder {
	char *data;
	int  hits;
};

struct scrolling {
	int scrl_main_win;
	int scrl_help_win;
};

struct tm *now_tm;
/*extern struct tm tm;*/

/* Declaration of variables */
extern int stripped_flag;
extern int 	bandwidth_flag;
extern int 	ignore_flag;
extern int 	http_status_code_flag;
extern long long req_size;
extern char *req;
extern char *stripped_str;
extern char *status_code;
extern char *ignore_host;

extern time_t now;
extern time_t start_proc;
extern time_t end_proc;

extern char *codes[][2];
extern char *module_names[];
extern const char *os[];
extern const char *browsers[];
extern char *help_main[];

#endif
