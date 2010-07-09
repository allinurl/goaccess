/** 
 * parser.c -- web log parsing
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
 * @version 0.1
 * Last Modified: Wednesday, July 07, 2010
 * Path:  /parser.c
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

#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <regex.h>
#include <stdio.h>

#include "commons.h"
#include "parser.h"
#include "util.h"

static int is_agent_present(const char *str)
{
	return (str && *str && str[strlen(str) - 2] == '"') ? 0 : 1;
}

/* qsort struct comparision function (hits field) */
int struct_cmp_by_hits(const void *a, const void *b)
{
	struct stu_alloc_holder *ia = *(struct stu_alloc_holder **)a;
	struct stu_alloc_holder *ib = *(struct stu_alloc_holder **)b;
	return (int)(ib->hits - ia->hits);
	/* integer comparison: returns negative if b > a and positive if a > b */
}
 
int struct_cmp(const void *a, const void *b) 
{
	struct stu_alloc_holder *ia = *(struct stu_alloc_holder **)a;
	struct stu_alloc_holder *ib = *(struct stu_alloc_holder **)b;
	return strcmp(ib->data, ia->data);
}

void process_unique_data(struct logger *logger, char *host, char *date, char *agent, 
						 char *status, char *referer)
{
	char *cat_hold, *p, *k;
	gpointer old_key, old_value;
	gint value;

	/* 
	 * C struct initialization and strptime BUG 
	 * C may not initialize stack structs and arrays to zeros 
	 * so strptime uses struct for output and input as well.
	 */
	struct tm tm = { 0 };
	char buf[9] = "";
	
	if (strptime(date, "%d/%b/%Y", &tm) == NULL) 
			;
	strftime(buf, strlen(buf)-1, "%Y%m%d ", &tm);
	
	size_t h_len = strlen(host);
	size_t d_len = strlen(buf) - 1;
	size_t a_len = strlen(agent);

	if (h_len + d_len + a_len + 3 > BUFFER) {
		endwin();
		fprintf(stderr, "\nAn error has occurred while opening '%s'");
		fprintf(stderr, "\nError: %s - %s - %d'\n\n", __PRETTY_FUNCTION__, __FILE__, __LINE__);
		exit(1);
	}

	cat_hold = malloc (h_len + d_len + a_len + 3);
	if (cat_hold == NULL) {
		endwin();
		fprintf(stderr, "\nAn error has occurred while opening '%s'");
		fprintf(stderr, "\nError: %s - %s - %d'\n\n", __PRETTY_FUNCTION__, __FILE__, __LINE__);
		exit(1);
	}
	memcpy(cat_hold, host, h_len); 
	cat_hold[h_len] = '|';
	memcpy(cat_hold + h_len + 1, buf, d_len + 1); 
	cat_hold[h_len + d_len + 1] = '|';
	memcpy(cat_hold + h_len + d_len + 2, agent, a_len + 1);

	char url[512] = "";
	if (sscanf(referer, "http://%511[^/\n]", url) != 1 ) {
		goto noref;
	}
	if (g_hash_table_lookup_extended(ht_referring_sites, url, &old_key, &old_value)) {
		value = GPOINTER_TO_INT(old_value);
		value = value + 1;
	} else {
		value = 1;
	}
	g_hash_table_replace(ht_referring_sites, g_strdup(url), GINT_TO_POINTER(value));
	noref:;

	if (!http_status_code_flag) {
		goto nohttpstatuscode;
	}
	if (g_hash_table_lookup_extended(ht_status_code, status, &old_key, &old_value)) {
		value = GPOINTER_TO_INT(old_value);
		value = value + 1;
	} else {
		value = 1;
	}
	g_hash_table_replace(ht_status_code, g_strdup(status), GINT_TO_POINTER(value));
	nohttpstatuscode:;

	if (ignore_flag && strcmp(host, ignore_host) == 0) {
		goto avoidhost;
	}
	if (g_hash_table_lookup_extended(ht_hosts, host, &old_key, &old_value)) {
		value = GPOINTER_TO_INT(old_value);
		value = value + 1;
	} else {
		value = 1;
	}
	g_hash_table_replace(ht_hosts, g_strdup(host), GINT_TO_POINTER(value));
	avoidhost:;

	if (g_hash_table_lookup_extended(ht_unique_visitors, cat_hold, &old_key, &old_value)) {
		value = GPOINTER_TO_INT(old_value);
		value = value + 1;
	} else {
		value = 1;
	}
	g_hash_table_replace(ht_unique_visitors, g_strdup(cat_hold), GINT_TO_POINTER(value));

	free(cat_hold);
}

void process_generic_data(GHashTable *hash_table, const char *key)
{
	gpointer old_value, old_key;
	gint value;

	if (g_hash_table_lookup_extended(hash_table,key, &old_key, &old_value)) {
		value = GPOINTER_TO_INT(old_value);
		value = value+1;
	} else {
		value = 1;
	}
	g_hash_table_replace(hash_table, g_strdup(key), GINT_TO_POINTER(value));
}

int verify_static_content(char *url)
{
	char *nul = url + strlen(url);

	if (strlen(url) < 5) return 0;
	if (!memcmp(nul - 4, ".jpg", 4)  || !memcmp(nul - 4, ".JPG", 4)  ||
		!memcmp(nul - 4, ".gif", 4)  || !memcmp(nul - 4, ".GIF", 4)  ||
		!memcmp(nul - 4, ".png", 4)  || !memcmp(nul - 4, ".PNG", 4)  ||
		!memcmp(nul - 4, ".ico", 4)  || !memcmp(nul - 4, ".ICO", 4)  ||
		!memcmp(nul - 5, ".jpeg", 5) || !memcmp(nul - 4, ".JPEG", 5) ||
		!memcmp(nul - 4, ".swf", 4)  || !memcmp(nul - 4, ".SWF", 4)  ||
		!memcmp(nul - 4, ".css", 4)  || !memcmp(nul - 4, ".CSS", 4)  ||
		!memcmp(nul - 3, ".js", 3)   || !memcmp(nul - 3, ".JS", 3))
		return 1;
	return 0;
}

char *parse_req(char *line)
{
	char *reqs, *req_l = NULL, *req_r = NULL, *lookfor = NULL;

	if ((lookfor = "\"GET ",  req_l = strstr(line, lookfor)) != NULL ||
		(lookfor = "\"POST ", req_l = strstr(line, lookfor)) != NULL ||
		(lookfor = "\"HEAD ", req_l = strstr(line, lookfor)) != NULL ||
		(lookfor = "\"get ",  req_l = strstr(line, lookfor)) != NULL ||
		(lookfor = "\"post ", req_l = strstr(line, lookfor)) != NULL ||
		(lookfor = "\"head ", req_l = strstr(line, lookfor)) != NULL) {

		/* The last part of the request is the protocol being used, 
		   at the time of this writing typically HTTP/1.0 or HTTP/1.1. */
		if ((req_r = strstr(line, " HTTP")) == NULL) {
			/* didn't find it :( weird */
			reqs = (char*) malloc (2);
			if (reqs == NULL) {
				endwin(); /* something went wrong */
				fprintf(stderr, "\nError: %s - %s - %d'\n\n", __PRETTY_FUNCTION__, __FILE__, __LINE__);
				exit(1);
			}
			sprintf (reqs, "-");
			return reqs;
		} 
			
		req_l += strlen(lookfor);
		ptrdiff_t req_len = req_r - req_l;

		reqs = malloc(req_len + 1);
		strncpy(reqs, req_l, req_len);
		(reqs)[req_len] = 0;
	} else {
		reqs = (char*) malloc (2);
		if (reqs == NULL) {
			endwin(); /* something went wrong */
			fprintf(stderr, "\nError: %s - %s - %d'\n\n", __PRETTY_FUNCTION__, __FILE__, __LINE__);
			exit(1);
		}
		sprintf (reqs, "-");
	}
	return reqs;
}

int parse_req_size(char *line, int format) 
{
	long size = 0;

	/* Common Log Format */
	if ((strstr(line, " -\n") != NULL))	return -1;

	/* Common Log Format */
	char *c;
	if (format) {
		if (c = strrchr(trim_str(line), ' '))
			size = strtol(c + 1, NULL, 10);
		return size;
	} 
	/* Combined Log Format */
	if ((c = strstr(line, "1.1\" ")) != NULL || 
		(c = strstr(line, "1.0\" ")) != NULL) c++;
	else return -1; /* no protocol used? huh... */

	char *p = NULL;
	if (p = strchr(c + 6, ' '))
		size = strtol(p + 1, NULL, 10);
	else size = -1;

	return size;
}

int parse_request(struct logger *logger, char *line)
{
	char *ptr, *prb = NULL, *fqm = NULL, *sqm = NULL, *agent, *host, *date, *ref, *hour;
	char *cpy_line = strdup(line);
	int format = 0;

	host = line;
	if ((date = strchr(line, '[')) == NULL) return 1;
	date++;

	/* agent */
	if (is_agent_present(line)) {
		format = 1; 
		fqm = "-";
		goto noagent;
	}
	for (prb = line; * prb; prb++) {
		if (*prb != '"') continue;
		else if (fqm == 0) 
			fqm = prb; 
		else if (sqm == 0) 
			sqm = prb; 
		else { 
			fqm = sqm; sqm = prb; 
		}
	}
	noagent:;
	if ((ref = strstr(line, "\"http")) != NULL || 
		(ref = strstr(line, "\"HTTP")) != NULL)	ref++;
	else ref = "";
	
	if (!bandwidth_flag) goto nobanwidth;
	/* bandwidth */
	long long band_size = parse_req_size(cpy_line, format);
	if (band_size != -1)
		req_size = req_size + band_size;
	else req_size = req_size + 0;
	nobanwidth:;

	if ((ptr = strchr(host, ' ')) == NULL) return 1;
	*ptr = '\0';
	if ((ptr = strchr(date, ']')) == NULL) return 1;
	*ptr = '\0';
	if ((ptr = strchr(date, ':')) == NULL) return 1;
	*ptr = '\0';
	if ((ptr = strchr(ref, '"')) == NULL) ref = "-";
	else *ptr = '\0';

	/* req */
	req = parse_req(cpy_line);

	if (!http_status_code_flag) goto nohttpstatuscode;
	char *lookfor = NULL, *s_l;
	if ((lookfor = "1.0\" ",  s_l = strstr(cpy_line, lookfor)) != NULL ||
		(lookfor = "1.1\" ",  s_l = strstr(cpy_line, lookfor)) != NULL) {
		status_code = clean_status(s_l + 5);
	} else {
		/* perhaps something wrong with the log */
		status_code = (char*) malloc (8);
		if (status_code == NULL) exit (1); /* something went wrong */
		sprintf (status_code, "Invalid");
	}
	nohttpstatuscode:;

	logger->host 	= host;
	logger->agent 	= fqm;
	logger->date 	= date;
	logger->hour 	= hour;
	logger->referer = ref;
	logger->request = req;

	if (http_status_code_flag)
		logger->status = status_code;

	free(cpy_line);
	return 0;
}

int process_log(struct logger *logger, char *line)
{
	struct logger log;
	char *cpy_line = strdup(line);
	logger->total_process++;

	if (parse_request(&log, line) == 0) {
		process_unique_data(logger, log.host, log.date, log.agent, log.status, log.referer);
		free(status_code);
		if (verify_static_content(log.request)) {
			if (strstr(cpy_line, "\" 404 ")) process_generic_data(ht_not_found_requests,log.request);
			process_generic_data(ht_requests_static,log.request);
		} else {
			if (strstr(cpy_line, "\" 404 ")) process_generic_data(ht_not_found_requests,log.request);
			process_generic_data(ht_requests, log.request);
		}
		process_generic_data(ht_referers, log.referer);
		free(cpy_line);
	} else {
		free(cpy_line);
		logger->total_invalid++;
		return 0;
	}
	free(req);
	return 0;
}

int parse_log(struct logger *logger, char *filename)
{
	FILE *fp;
	char line[BUFFER];

	if ((fp = fopen(filename, "r")) == NULL) {
		endwin();
		fprintf(stderr, "\nAn error has occurred while opening '%s'", filename);
		fprintf(stderr, "\n%s%s'\n\n", __PRETTY_FUNCTION__, __FILE__);
		exit(1);
	}
	while (fgets(line, BUFFER, fp) != NULL) {
		if (process_log(logger, line)) {
			fclose(fp);
			return 1;
		}
	}
	return 0;
}

void generate_unique_visitors(WINDOW *main_win, struct stu_alloc_holder **sorted_alloc_holder, 
							  struct stu_alloc_all **sorted_alloc_all, struct logger *logger)
{
	int row, col, n = 0, b, e,
		lo, r = 0, j = 0, i, x_bar_pos, w = 0;
	float l_bar = 0;
	char *date, *current_date;
	struct stu_alloc_holder **s_holder;

	GHashTableIter iter;
	gchar *k,*v;   

	/* get the number of rows and columns */
	getmaxyx(stdscr,row,col);

	g_hash_table_iter_init (&iter, ht_unique_visitors);
	while (g_hash_table_iter_next (&iter, (gpointer) &k, (gpointer) &v)) {
		/* ###FIXME: 64 bit portability issues might arise */
		process_generic_data(ht_os, verify_os((gchar *)k));
		process_generic_data(ht_browsers, verify_browser((gchar *)k));
		sorted_alloc_holder[n]->data = (gchar *)k;
		sorted_alloc_holder[n++]->hits = (gint)v;
		logger->counter++;
	}

	for (lo=0; lo<logger->counter; lo++) {
		if ((date = strchr(sorted_alloc_holder[lo]->data, '|')) != NULL) {
			current_date = clean_date(date);
			process_generic_data(ht_unique_vis,current_date);
		}
		free(current_date);
	}
	
	int ct = 0;
	s_holder = (struct stu_alloc_holder **)malloc(sizeof(struct stu_alloc_holder *)*g_hash_table_size(ht_unique_vis));
	g_hash_table_iter_init (&iter, ht_unique_vis);
	while (g_hash_table_iter_next (&iter, (gpointer) &k, (gpointer) &v)) {
		/* ###FIXME: 64 bit portability issues might arise */
		s_holder[w] = (struct stu_alloc_holder *)malloc(sizeof(struct stu_alloc_holder));
		s_holder[w]->data = (gchar *)k;
		s_holder[w++]->hits = (gint)v;
		ct++;
	}

	qsort(s_holder, ct, sizeof(struct stu_alloc_holder*), struct_cmp);
	int t_req = g_hash_table_size(ht_unique_vis) > 6 ? 6 : g_hash_table_size(ht_unique_vis);

	init_pair(3, COLOR_RED, -1);
	init_pair(4, COLOR_GREEN, -1);

	struct tm tm = { 0 };
	char buf[12] = ""; /* max possible size for date type */

	r = 0;
	/* fixed value in here, perhaps it could be dynamic depending on the module */	
	for (lo=0; lo<10; lo++) {
		sorted_alloc_all[logger->alloc_counter]->hits = 0;
		sorted_alloc_all[logger->alloc_counter]->module = 1;

		if (lo == 0)
			sorted_alloc_all[logger->alloc_counter++]->data = alloc_string(" 1 - Unique visitors per day - Including spiders");
		else if (lo == 1) 
			sorted_alloc_all[logger->alloc_counter++]->data = alloc_string(" HTTP requests having the same IP, same date and same agent will be considered a unique visit");
		else if (lo == 2 || lo == 9)
			sorted_alloc_all[logger->alloc_counter++]->data = alloc_string("");
		else if (r < ct) {
			sorted_alloc_all[logger->alloc_counter]->hits = s_holder[r]->hits;
			sorted_alloc_all[logger->alloc_counter++]->data = alloc_string(s_holder[r]->data);
			r++;
		} else sorted_alloc_all[logger->alloc_counter++]->data = alloc_string("");
	}
		
	int f;
	for (f = 0; f < ct; f++)
		free(s_holder[f]);
	free(s_holder);

	for (f=0; f<logger->counter; f++)
		free(sorted_alloc_holder[f]);
	free(sorted_alloc_holder);
	logger->counter = 0;
}

int c = 0;
void generate_struct_data(GHashTable *hash_table, struct stu_alloc_holder **sorted_alloc_holder, struct stu_alloc_all **sorted_alloc_all, struct logger *logger, int module)
{
	int row,col;
	
	/* get the number of rows and columns */
	getmaxyx(stdscr,row,col);
	
	int i = 0;
	GHashTableIter iter;
	gchar *k,*v;
	
	g_hash_table_iter_init (&iter, hash_table);
	while (g_hash_table_iter_next (&iter, (gpointer) &k, (gpointer) &v)) {
		/* ###FIXME: 64 bit portability issues might arise */
		sorted_alloc_holder[i]->data = (gchar *)k;
		sorted_alloc_holder[i++]->hits = (gint)v;
		logger->counter++;
	}
	
	qsort(sorted_alloc_holder, logger->counter, sizeof(struct stu_alloc_holder*), struct_cmp_by_hits);
	
	/* headers & sub-headers */
	char *head = NULL, *desc = NULL;
	switch (module) {
		case 2:
			head = " 2 - Requested files - File requests ordered by hits";
			desc = " Top 6 different files requested";
			break;	
		case 3:
			head = " 3 - Requested static files - Static content (jpg,png,gif,js etc)";
			desc = " Top 6 different static files requested, ordered by hits";
			break;	
		case 4:
			head = " 4 - Referrers URLs";
			desc = " Top 6 different referrers ordered by hits";
			break;	
		case 5:
			head = " 5 - 404 or Not Found request message";
			desc = " Top 6 different 404 ordered by hits";
			break;	
		case 6:
			head = " 6 - Operating Systems";
			desc = " Top 6 common Operating systems";
			break;	
		case 7:
			head = " 7 - Browsers";
			desc = " Top 6 common browsers";
			break;
		case 8:
			head = " 8 - Hosts";
			desc = " Top 6 unique hosts sorted by hits";
			break;	
		case 9:
			head = " 9 - HTTP Status Codes";
			desc = " Top 6 unique status codes sorted by hits";
			break;	
		case 10:
			head = " 10 - Top Referring Sites";
			desc = " Top 6 unique referring sites sorted by hits";
			break;	
	}

	/* r : pos */	
	int f, lo, r = 0, t_req = logger->counter > 6 ? 6 : logger->counter;
	
	init_pair(2, COLOR_BLACK, COLOR_CYAN);
	attron(COLOR_PAIR(2));
	attroff(COLOR_PAIR(2));

	for (lo = 0; lo < 10; lo++) {
		sorted_alloc_all[logger->alloc_counter]->hits = 0;
		sorted_alloc_all[logger->alloc_counter]->module = module;
		if (lo == 0)
			sorted_alloc_all[logger->alloc_counter++]->data = alloc_string(head);
		else if (lo == 1)
			sorted_alloc_all[logger->alloc_counter++]->data = alloc_string(desc);
		else if (lo == 2 || lo == 9)
			sorted_alloc_all[logger->alloc_counter++]->data = alloc_string("");
		else if (r<logger->counter){
			if (strlen(sorted_alloc_holder[r]->data) > col - 15) {
				stripped_str = substring(sorted_alloc_holder[r]->data, 0, col - 15);	
				sorted_alloc_all[logger->alloc_counter]->data = stripped_str;
			} else sorted_alloc_all[logger->alloc_counter]->data = alloc_string(sorted_alloc_holder[r]->data);
			sorted_alloc_all[logger->alloc_counter++]->hits = sorted_alloc_holder[r]->hits;
			r++;
		} else sorted_alloc_all[logger->alloc_counter++]->data = alloc_string("");
	}
	
	for (f = 0; f < g_hash_table_size(hash_table); f++)
		free(sorted_alloc_holder[f]);
	free(sorted_alloc_holder);
	logger->counter = 0;
}
