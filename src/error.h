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

#ifndef ERROR_H_INCLUDED
#define ERROR_H_INCLUDED

#ifndef COMMONS
#include "commons.h"
#endif

#ifdef HAVE_NCURSESW_NCURSES_H
#include <ncursesw/ncurses.h>
#elif HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#elif HAVE_CURSES_H
#include <curses.h>
#endif

#include <settings.h>

#define TRACE_SIZE 128

#define FATAL(fmt, ...) do {                                                  \
  (void) endwin ();                                                           \
  fprintf (stderr, "\nGoAccess - version %s - %s %s\n", GO_VERSION, __DATE__, \
           __TIME__);                                                         \
  fprintf (stderr, "Config file: %s\n", conf.iconfigfile ?: NO_CONFIG_FILE);  \
  fprintf (stderr, "\nFatal error has occurred");                             \
  fprintf (stderr, "\nError occured at: %s - %s - %d\n", __FILE__,            \
           __FUNCTION__, __LINE__);                                           \
  fprintf (stderr, fmt, ##__VA_ARGS__);                                       \
  fprintf (stderr, "\n\n");                                                   \
  LOG_DEBUG ((fmt, ##__VA_ARGS__));                                           \
  exit(EXIT_FAILURE);                                                         \
} while (0)


void dbg_fprintf (const char *fmt, ...);
void dbg_log_close (void);
void dbg_log_open (const char *file);
void set_signal_data (void *p);

#if defined(__GLIBC__)
void sigsegv_handler (int sig);
#endif

#endif
