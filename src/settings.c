/**
 * settings.c -- goaccess configuration
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

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "settings.h"

#include "error.h"
#include "util.h"
#include "xmalloc.h"

static char **nargv;
static int nargc = 0;

/* *INDENT-OFF* */
static const GPreConfLog logs = {
  "%h %^[%d:%t %^] \"%r\" %s %b \"%R\" \"%u\"",                 /* NCSA          */
  "%^:%^ %h %^[%d:%t %^] \"%r\" %s %b \"%R\" \"%u\"",           /* NCSA + VHost  */
  "%h %^[%d:%t %^] \"%r\" %s %b",                               /* CLF           */
  "%^:%^ %h %^[%d:%t %^] \"%r\" %s %b",                         /* CLF+VHost     */
  "%d %t %h %^ %^ %^ %m %r %^ %s %b %^ %^ %u %R",               /* W3C           */
  "%d\\t%t\\t%^\\t%b\\t%h\\t%m\\t%^\\t%r\\t%s\\t%R\\t%u\\t%^",  /* CloudFront    */
  "\"%x\",\"%h\",%^,%^,\"%m\",\"%U\",\"%s\",%^,\"%b\",\"%D\",%^,\"%R\",\"%u\"", /* Cloud Storage */
};

static const GPreConfDate dates = {
  "%d/%b/%Y", /* Apache     */
  "%Y-%m-%d", /* W3C        */
  "%f",       /* Cloud Storage*/
};

static const GPreConfTime times = {
  "%H:%M:%S",
  "%f",       /* Cloud Storage*/
};
/* *INDENT-ON* */

/* Ignore the following options */
static const char *ignore_cmd_opts[] = {
  "help",
  "storage",
  "version",
};

static int
in_ignore_cmd_opts (const char *val)
{
  size_t i;
  for (i = 0; i < ARRAY_SIZE (ignore_cmd_opts); i++) {
    if (strstr (val, ignore_cmd_opts[i]) != NULL)
      return 1;
  }
  return 0;
}

static char *
get_config_file_path (void)
{
  char *upath = NULL, *rpath = NULL;

  /* determine which config file to open, default or custom */
  if (conf.iconfigfile != NULL) {
    rpath = realpath (conf.iconfigfile, NULL);
    if (rpath == NULL)
      FATAL ("Unable to open the specified config file. %s", strerror (errno));
  }
  /* otherwise, fallback to global config file, or
   * attempt to use the user's config file */
  else {
    /* global config file */
    if (conf.load_global_config) {
      upath = get_global_config ();
      rpath = realpath (upath, NULL);
      if (upath) {
        free (upath);
      }
    }
    /* user's config file */
    if (rpath == NULL) {
      upath = get_home ();
      rpath = realpath (upath, NULL);
      if (upath) {
        free (upath);
      }
    }
  }

  return rpath;
}

/* clean command line arguments */
void
free_cmd_args (void)
{
  int i;
  if (nargc == 0)
    return;
  for (i = 0; i < nargc; i++)
    free (nargv[i]);
  free (nargv);
}

/* append extra value to argv */
static void
append_to_argv (int *argc, char ***argv, char *val)
{
  char **_argv = xrealloc (*argv, (*argc + 2) * sizeof (*_argv));
  _argv[*argc] = val;
  _argv[*argc + 1] = '\0';
  (*argc)++;
  *argv = _argv;
}

/* parses configuration file to feed getopt_long */
int
parse_conf_file (int *argc, char ***argv)
{
  char line[MAX_LINE_CONF + 1];
  char *path = NULL, *val, *opt, *p;
  FILE *file;
  int i;
  size_t idx;

  /* assumes program name is on argv[0], though, it is not guaranteed */
  append_to_argv (&nargc, &nargv, xstrdup ((char *) *argv[0]));

  /* determine which config file to open, default or custom */
  path = get_config_file_path ();
  if (path == NULL)
    return ENOENT;

  /* could not open conf file, if so prompt conf dialog */
  if ((file = fopen (path, "r")) == NULL) {
    free (path);
    return ENOENT;
  }

  while (fgets (line, sizeof line, file) != NULL) {
    if (line[0] == '\n' || line[0] == '\r' || line[0] == '#')
      continue;

    /* key */
    idx = strcspn (line, " \t");
    if (strlen (line) == idx)
      FATAL ("Malformed config key at line: %s", line);

    line[idx] = '\0';

    /* make old config options backwards compatible by
     * substituting underscores with dashes */
    while ((p = strpbrk (line, "_")) != NULL)
      *p = '-';

    /* Ignore the following options when reading the config file */
    if (in_ignore_cmd_opts (line))
      continue;

    /* value */
    val = line + (idx + 1);
    idx = strspn (val, " \t");
    if (strlen (line) == idx)
      FATAL ("Malformed config value at line: %s", line);
    val = val + idx;
    val = trim_str (val);

    if (strcmp ("false", val) == 0)
      continue;

    /* set it as command line options */
    opt = xmalloc (snprintf (NULL, 0, "--%s", line) + 1);
    sprintf (opt, "--%s", line);

    append_to_argv (&nargc, &nargv, opt);
    if (strcmp ("true", val) != 0)
      append_to_argv (&nargc, &nargv, xstrdup (val));
  }

  /* give priority to command line arguments */
  for (i = 1; i < *argc; i++)
    append_to_argv (&nargc, &nargv, xstrdup ((char *) (*argv)[i]));

  *argc = nargc;
  *argv = (char **) nargv;

  fclose (file);

  if (conf.iconfigfile == NULL)
    conf.iconfigfile = xstrdup (path);

  free (path);
  return 0;
}

/* return the index of the matched item, or -1 if no such item exists */
size_t
get_selected_format_idx (void)
{
  if (conf.log_format == NULL)
    return -1;
  if (strcmp (conf.log_format, logs.common) == 0)
    return COMMON;
  else if (strcmp (conf.log_format, logs.vcommon) == 0)
    return VCOMMON;
  else if (strcmp (conf.log_format, logs.combined) == 0)
    return COMBINED;
  else if (strcmp (conf.log_format, logs.vcombined) == 0)
    return VCOMBINED;
  else if (strcmp (conf.log_format, logs.w3c) == 0)
    return W3C;
  else if (strcmp (conf.log_format, logs.cloudfront) == 0)
    return CLOUDFRONT;
  else if (strcmp (conf.log_format, logs.cloudstorage) == 0)
    return CLOUDSTORAGE;
  else
    return -1;
}

/* return the string of the matched item, or NULL if no such item exists */
char *
get_selected_format_str (size_t idx)
{
  char *fmt = NULL;
  switch (idx) {
  case COMMON:
    fmt = alloc_string (logs.common);
    break;
  case VCOMMON:
    fmt = alloc_string (logs.vcommon);
    break;
  case COMBINED:
    fmt = alloc_string (logs.combined);
    break;
  case VCOMBINED:
    fmt = alloc_string (logs.vcombined);
    break;
  case W3C:
    fmt = alloc_string (logs.w3c);
    break;
  case CLOUDFRONT:
    fmt = alloc_string (logs.cloudfront);
    break;
  case CLOUDSTORAGE:
    fmt = alloc_string (logs.cloudstorage);
    break;
  }

  return fmt;
}

char *
get_selected_date_str (size_t idx)
{
  char *fmt = NULL;
  switch (idx) {
  case COMMON:
  case VCOMMON:
  case COMBINED:
  case VCOMBINED:
    fmt = alloc_string (dates.apache);
    break;
  case CLOUDFRONT:
  case W3C:
    fmt = alloc_string (dates.w3c);
    break;
  case CLOUDSTORAGE:
    fmt = alloc_string (dates.usec);
    break;
  }

  return fmt;
}

char *
get_selected_time_str (size_t idx)
{
  char *fmt = NULL;
  switch (idx) {
  case COMMON:
  case VCOMMON:
  case COMBINED:
  case VCOMBINED:
  case W3C:
  case CLOUDFRONT:
    fmt = alloc_string (times.fmt24);
    break;
  case CLOUDSTORAGE:
    fmt = alloc_string (times.usec);
    break;
  }

  return fmt;
}

int
ignore_panel (GModule mod)
{
  int i;
  int module;
  char *view;
  for (i = 0; i < conf.ignore_panel_idx; ++i) {
    view = conf.ignore_panels[i];
    if ((module = get_module_enum (view)) == -1)
      continue;
    if (mod == (unsigned int) module)
      return 1;
  }

  return 0;
}
