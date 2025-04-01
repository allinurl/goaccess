/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2025 Gerardo Orellana <hello @ goaccess.io>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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

#include <stdio.h>
#include "settings.h"

#define TRACE_SIZE 128

#define FATAL(fmt, ...) do {                                                                 \
  (void) endwin ();                                                                          \
  fprintf (stderr, "\nGoAccess - version %s - %s %s\n", GO_VERSION, __DATE__, __TIME__);     \
  fprintf (stderr, "Config file: %s\n", conf.iconfigfile ?: NO_CONFIG_FILE);                 \
  fprintf (stderr, "\nFatal error has occurred");                                            \
  fprintf (stderr, "\nError occurred at: %s - %s - %d\n", __FILE__, __FUNCTION__, __LINE__); \
  fprintf (stderr, fmt, ##__VA_ARGS__);                                                      \
  fprintf (stderr, "\n\n");                                                                  \
  LOG_DEBUG ((fmt, ##__VA_ARGS__));                                                          \
  exit(EXIT_FAILURE);                                                                        \
} while (0)

#ifdef DEBUG
#define DEBUG_TEST 1
#else
#define DEBUG_TEST 0
#endif

/* access requests log */
#define ACCESS_LOG(x, ...) do { access_fprintf x; } while (0)
/* debug log */
#define LOG_DEBUG(x, ...) do { dbg_fprintf x; } while (0)
/* invalid requests log */
#define LOG_INVALID(x, ...) do { invalid_fprintf x; } while (0)
/* unknown browser log */
#define LOG_UNKNOWNS(x, ...) do { unknowns_fprintf x; } while (0)
/* log debug wrapper */
#define LOG(x) do { if (DEBUG_TEST) dbg_printf x; } while (0)

int access_log_open (const char *path);
void access_fprintf (const char *fmt, ...) __attribute__((format (printf, 1, 2)));
void access_log_close (void);
void dbg_fprintf (const char *fmt, ...) __attribute__((format (printf, 1, 2)));
void dbg_log_close (void);
void dbg_log_open (const char *file);
void dbg_printf (const char *fmt, ...) __attribute__((format (printf, 1, 2)));
void invalid_fprintf (const char *fmt, ...) __attribute__((format (printf, 1, 2)));
void unknowns_fprintf (const char *fmt, ...) __attribute__((format (printf, 1, 2)));
void invalid_log_close (void);
void invalid_log_open (const char *path);
void set_signal_data (void *p);
void setup_sigsegv_handler (void);
void sigsegv_handler (int sig);
void unknowns_log_close (void);
void unknowns_log_open (const char *path);

#endif
