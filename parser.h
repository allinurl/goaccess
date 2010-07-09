/**
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
 * @version 0.1
 * Last Modified: Wednesday, July 07, 2010
 * Path:  /parser.h
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

#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED

#ifndef COMMONS 
#include "commons.h"
#endif

int struct_cmp_by_hits(const void *a, const void *b);

int struct_cmp(const void *a, const void *b);

void process_unique_data(char *host, char *date, 
						 char *agent, char *status, char *referer);

void process_generic_data(GHashTable *hash_table, const char *key);

int verify_static_content(char *url);

char *parse_req(char *line);

int parse_req_size(char *line, int format); 

int parse_request(struct logger *logger, char *line);

int process_log(struct logger *logger, char *line);

int parse_log(struct logger *logger, char *filename);

void generate_unique_visitors(WINDOW *main_win, struct stu_alloc_holder **sorted_alloc_holder, 
							  struct stu_alloc_all **sorted_alloc_all, struct logger *logger);

void generate_struct_data(GHashTable *hash_table, struct stu_alloc_holder **sorted_alloc_holder, 
						  struct stu_alloc_all **sorted_alloc_all, struct logger *logger, int module);

#endif
