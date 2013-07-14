/**
 * Copyright (C) 2009-2013 by Gerardo Orellana <goaccess@prosoftcorp.com>
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef COMMONS_H_INCLUDED
#define COMMONS_H_INCLUDED

/* Remove the __attribute__ stuff when the compiler is not GCC. */
#if !__GNUC__
#define __attribute__(x) /**/
#endif
#define GO_UNUSED __attribute__((unused))
#define GO_VERSION 		"0.6"
struct tm *now_tm;

/* Processing time */
extern time_t end_proc;
extern time_t now;
extern time_t start_proc;

/* resizing */
extern size_t real_size_y;
extern size_t term_h;
extern size_t term_w;

#endif
