/**
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
 * @version 0.2
 * Last Modified: Sunday, July 25, 2010
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

int struct_cmp_by_hits (const void *a, const void *b);

int struct_cmp_desc (const void *a, const void *b);
int struct_cmp_asc (const void *a, const void *b);

int parse_log (struct logger *logger, char *filename, char *tail);

void generate_unique_visitors (struct struct_display **s_display,
                               struct logger *logger);

void generate_struct_data (GHashTable * hash_table,
                           struct struct_holder **s_holder,
                           struct struct_display **s_display,
                           struct logger *logger, int module);

#endif
