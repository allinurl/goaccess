/**
 * error.c -- error handling
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2016 Gerardo Orellana <hello @ goaccess.io>
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#if defined(__GLIBC__)
#include <execinfo.h>
#endif
#include <sys/types.h>
#include <unistd.h>

#include "error.h"
#include "parser.h"

static FILE *access_log;
static FILE *log_file;
static FILE *log_invalid;
static GLog *log_data;

/* Open a debug file whose name is specified in the given path. */
void
dbg_log_open (const char *path)
{
  if (path != NULL) {
    log_file = fopen (path, "w");
    if (log_file == NULL)
      return;
  }
}

/* Close the debug file. */
void
dbg_log_close (void)
{
  if (log_file != NULL)
    fclose (log_file);
}

/* Open the invalid requests log file whose name is specified in the
 * given path. */
void
invalid_log_open (const char *path)
{
  if (path != NULL) {
    log_invalid = fopen (path, "w");
    if (log_invalid == NULL)
      return;
  }
}

/* Close the invalid requests log file. */
void
invalid_log_close (void)
{
  if (log_invalid != NULL)
    fclose (log_invalid);
}

/* Set current overall parsed log data. */
void
set_signal_data (void *p)
{
  log_data = p;
}

/* Open a access file whose name is specified in the given path. */
int
access_log_open (const char *path)
{
  if (path == NULL)
    return 0;

  if (access (path, F_OK) != -1)
    access_log = fopen (path, "a");
  else
    access_log = fopen (path, "w");
  if (access_log == NULL)
    return 1;

  return 0;
}

/* Close the access log file. */
void
access_log_close (void)
{
  if (access_log != NULL)
    fclose (access_log);
}

/* Dump to the standard output the values of the overall parsed log
 * data. */
static void
dump_struct (FILE * fp)
{
  int pid = getpid ();
  if (!log_data)
    return;

  fprintf (fp, "==%d== VALUES AT CRASH POINT\n", pid);
  fprintf (fp, "==%d==\n", pid);
  fprintf (fp, "==%d== Line number: %u\n", pid, log_data->processed);
  fprintf (fp, "==%d== Offset: %u\n", pid, log_data->offset);
  fprintf (fp, "==%d== Invalid data: %u\n", pid, log_data->invalid);
  fprintf (fp, "==%d== Piping: %d\n", pid, log_data->piping);
  fprintf (fp, "==%d== Response size: %llu bytes\n", pid, log_data->resp_size);
  fprintf (fp, "==%d==\n", pid);
}

/* Custom SIGSEGV handler. */
void
sigsegv_handler (int sig)
{
  FILE *fp = stderr;
  int pid = getpid ();

#if defined(__GLIBC__)
  char **messages;
  size_t size, i;
  void *trace_stack[TRACE_SIZE];
#endif

  (void) endwin ();
  fprintf (fp, "\n");
  fprintf (fp, "==%d== GoAccess %s crashed by Sig %d\n", pid, GO_VERSION, sig);
  fprintf (fp, "==%d==\n", pid);

  dump_struct (fp);

#if defined(__GLIBC__)
  size = backtrace (trace_stack, TRACE_SIZE);
  messages = backtrace_symbols (trace_stack, size);

  fprintf (fp, "==%d== STACK TRACE:\n", pid);
  fprintf (fp, "==%d==\n", pid);

  for (i = 0; i < size; i++)
    fprintf (fp, "==%d== %zu %s\n", pid, i, messages[i]);
#endif

  fprintf (fp, "==%d==\n", pid);
  fprintf (fp, "==%d== Please report it by opening an issue on GitHub:\n", pid);
  fprintf (fp, "==%d== https://github.com/allinurl/goaccess/issues\n\n", pid);
  exit (EXIT_FAILURE);
}

#pragma GCC diagnostic ignored "-Wformat-nonliteral"
/* Write formatted debug log data to the logfile. */
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

/* Write formatted invalid requests log data to the logfile. */
void
invalid_fprintf (const char *fmt, ...)
{
  va_list args;

  if (!log_invalid)
    return;

  va_start (args, fmt);
  vfprintf (log_invalid, fmt, args);
  fflush (log_invalid);
  va_end (args);
}

/* Debug otuput */
void
dbg_printf (const char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  vfprintf (stderr, fmt, args);
  va_end (args);
}

/* Write formatted access log data to the logfile. */
void
access_fprintf (const char *fmt, ...)
{
  va_list args;

  if (!access_log)
    return;

  va_start (args, fmt);
  vfprintf (access_log, fmt, args);
  fflush (access_log);
  va_end (args);
}

#pragma GCC diagnostic warning "-Wformat-nonliteral"
