/**
 * Copyright (C) 2009-2014 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An Ncurses apache weblog analyzer & interactive viewer
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
#ifndef GHOLDER_H_INCLUDED
#define GHOLDER_H_INCLUDED

#define MTRC_ID_COUNTRY  0
#define MTRC_ID_CITY     1
#define MTRC_ID_HOSTNAME 2

#include "commons.h"
#include "sort.h"

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabdb.h"
#else
#include "glibht.h"
#endif

/* Function Prototypes */
GHolder *new_gholder (uint32_t size);
void *add_hostname_node (void *ptr_holder);
void free_holder_by_module (GHolder ** holder, GModule module);
void free_holder (GHolder ** holder);
void load_holder_data (GRawData * raw_data, GHolder * h, GModule module,
                       GSort sort);
void load_host_to_holder (GHolder * h, char *ip);

#endif // for #ifndef GHOLDER_H
