/**
 * error.c -- error handling
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LIBNCURSESW
#ifdef __FreeBSD__
#include <ncurses.h>
#else
#include <ncursesw/curses.h>
#endif
#else
#include <ncurses.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "commons.h"

static FILE *log_file;

#pragma GCC diagnostic ignored "-Wformat-nonliteral"
void
error_handler (const char *func, const char *file, int line, const char *msg,
               ...)
{
  va_list args;

  (void) endwin ();

  fprintf (stderr, "\nGoAccess - version %s - %s %s\n", GO_VERSION,
           __DATE__, __TIME__);
  fprintf (stderr, "\nAn error has occurred");
  fprintf (stderr, "\nError occured at: %s - %s - %d", file, func, line);
  fprintf (stderr, "\nMessage: ");

  va_start (args, msg);
  vfprintf (stderr, msg, args);
  va_end (args);

  fprintf (stderr, "\n\n");

  exit (EXIT_FAILURE);
}

#pragma GCC diagnostic warning "-Wformat-nonliteral"

void
dbg_log_open (const char *path)
{
  if (path != NULL) {
    log_file = fopen (path, "w");
    if (log_file == NULL)
      return;
  }
}

void
dbg_log_close (void)
{
  if (log_file != NULL)
    fclose (log_file);
}

#pragma GCC diagnostic ignored "-Wformat-nonliteral"
void
dbg_fprintf (const char *fmt, ...)
{
  va_list args;

  if (!log_file)
    return;

  va_start (args, fmt);
  vfprintf (log_file, fmt, args);
  fflush (log_file);
  va_end (args);
}

#pragma GCC diagnostic warning "-Wformat-nonliteral"
