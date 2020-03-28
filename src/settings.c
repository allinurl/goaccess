/**
 * settings.c -- goaccess configuration
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2020 Gerardo Orellana <hello @ goaccess.io>
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

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "settings.h"

#include "error.h"
#include "labels.h"
#include "util.h"
#include "xmalloc.h"

static char **nargv;
static int nargc = 0;

/* *INDENT-OFF* */
static GEnum LOGTYPE[] = {
  {"COMBINED"     , COMBINED}     ,
  {"VCOMBINED"    , VCOMBINED}    ,
  {"COMMON"       , COMMON}       ,
  {"VCOMMON"      , VCOMMON}      ,
  {"W3C"          , W3C}          ,
  {"SQUID"        , SQUID}        ,
  {"CLOUDFRONT"   , CLOUDFRONT}   ,
  {"CLOUDSTORAGE" , CLOUDSTORAGE} ,
  {"AWSELB"       , AWSELB}       ,
  {"AWSS3"        , AWSS3}        ,
};

static const GPreConfLog logs = {
  "%h %^[%d:%t %^] \"%r\" %s %b \"%R\" \"%u\"",                 /* NCSA */
  "%v:%^ %h %^[%d:%t %^] \"%r\" %s %b \"%R\" \"%u\"",           /* NCSA + VHost  */
  "%h %^[%d:%t %^] \"%r\" %s %b",                               /* CLF */
  "%v:%^ %h %^[%d:%t %^] \"%r\" %s %b",                         /* CLF+VHost */
  "%d %t %^ %m %U %q %^ %^ %h %u %R %s %^ %^ %L",               /* W3C */
  "%d\\t%t\\t%^\\t%b\\t%h\\t%m\\t%^\\t%r\\t%s\\t%R\\t%u\\t%^",  /* CloudFront */
  "\"%x\",\"%h\",%^,%^,\"%m\",\"%U\",\"%s\",%^,\"%b\",\"%D\",%^,\"%R\",\"%u\"", /* Cloud Storage */
  "%^ %dT%t.%^ %v %h:%^ %^ %T %^ %^ %s %^ %b %^ \"%r\" \"%u\" %^",    /* AWS Elastic Load Balancing */
  "%^ %^ %^ %v %^: %x.%^ %~%L %h %^/%s %b %m %U",               /* Squid Native */
  "%^ %v [%d:%t %^] %h %^\"%r\" %s %^ %b %^ %L %^ \"%R\" \"%u\"", /* Amazon S3 */
};

static const GPreConfTime times = {
  "%H:%M:%S",
  "%f",       /* Cloud Storage (usec) */
  "%s",       /* Squid (sec) */
};

static const GPreConfDate dates = {
  "%d/%b/%Y", /* Apache */
  "%Y-%m-%d", /* W3C */
  "%f",       /* Cloud Storage (usec) */
  "%s",       /* Squid (sec) */
};
/* *INDENT-ON* */

/* Ignore the following options */
static const char *ignore_cmd_opts[] = {
  "help",
  "storage",
  "version",
};

/* Determine if the given command line option needs to be ignored.
 *
 * If needs to be ignored, 1 is returned.
 * If not within the list of ignored command line options, 0 is returned. */
static int
in_ignore_cmd_opts (const char *val) {
  size_t i;
  for (i = 0; i < ARRAY_SIZE (ignore_cmd_opts); i++) {
    if (strstr (val, ignore_cmd_opts[i]) != NULL)
      return 1;
  }
  return 0;
}

/* Get the location of the configuration file.
 *
 * By default, it attempts to read it from the user supplied path, else it will
 * try to open the global config file path (sysconfdir) or from the HOME
 * environment variable (~/.goaccessrc).
 *
 * On success, the path to the configuration file is returned. */
char *
get_config_file_path (void) {
  char *upath = NULL, *rpath = NULL;
  FILE *file;

  /* determine which config file to open, default or custom */
  if (conf.iconfigfile != NULL) {
    rpath = realpath (conf.iconfigfile, NULL);
    if (rpath == NULL)
      FATAL ("Unable to open the specified config file. %s", strerror (errno));
    return rpath;
  }

  /* attempt to use the user's config file */
  upath = get_home ();
  rpath = realpath (upath, NULL);       /* malloc'd */

  /* failure, e.g. if the file does not exist */
  if (rpath == NULL) {
    LOG_DEBUG (("Unable to open default config file %s %s", upath,
                strerror (errno)));
    free (upath);
    return NULL;
  }
  if (upath) {
    free (upath);
  }

  /* otherwise, fallback to global config file */
  if ((file = fopen (rpath, "r")) == NULL && conf.load_global_config) {
    upath = get_global_config ();
    rpath = realpath (upath, NULL);
    if (upath) {
      free (upath);
    }
  } else {
    fclose (file);
  }

  return rpath;
}

/* Use predefined static files when no config file is used. Note that
 * the order in which are listed is from the most to the least common
 * (most cases). */
void
set_default_static_files (void) {
  size_t i;
  const char *exts[] = {
    ".css",
    ".js ",
    ".jpg",
    ".png",
    ".gif",
    ".ico",
    ".jpeg",
    ".pdf",
    ".txt",
    ".csv",
    ".mpeg",
    ".mpg",
    ".swf",
    ".woff",
    ".woff2",
    ".xls",
    ".xlsx",
    ".doc ",
    ".docx",
    ".ppt ",
    ".pptx",
    ".zip",
    ".mp3",
    ".mp4",
    ".exe",
    ".iso ",
    ".gz  ",
    ".rar ",
    ".svg ",
    ".bmp ",
    ".tar ",
    ".tgz ",
    ".tiff",
    ".tif ",
    ".ttf ",
    ".flv ",
  };

  if (conf.static_file_idx > 0)
    return;

  for (i = 0; i < ARRAY_SIZE (exts); i++) {
    if (conf.static_file_max_len < strlen (exts[i]))
      conf.static_file_max_len = strlen (exts[i]);
    conf.static_files[conf.static_file_idx++] = exts[i];
  }
}

/* Clean malloc'd log/date/time escaped formats. */
void
free_formats (void) {
  free (conf.log_format);
  free (conf.date_format);
  free (conf.date_num_format);
  free (conf.spec_date_time_format);
  free (conf.spec_date_time_num_format);
  free (conf.time_format);
}

/* Clean malloc'd command line arguments. */
void
free_cmd_args (void) {
  int i;
  if (nargc == 0)
    return;
  for (i = 0; i < nargc; i++)
    free (nargv[i]);
  free (nargv);
  free (conf.iconfigfile);
}

/* Append extra value to argv */
static void
append_to_argv (int *argc, char ***argv, char *val) {
  char **_argv = xrealloc (*argv, (*argc + 2) * sizeof (*_argv));
  _argv[*argc] = val;
  _argv[*argc + 1] = (char *) '\0';
  (*argc)++;
  *argv = _argv;
}

/* Parses the configuration file to feed getopt_long.
 *
 * On error, ENOENT error code is returned.
 * On success, 0 is returned and config file enabled options are appended to
 * argv. */
int
parse_conf_file (int *argc, char ***argv) {
  char line[MAX_LINE_CONF + 1];
  char *path = NULL, *val, *opt, *p;
  FILE *file;
  int i, n = 0;
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
    while (line[0] == ' ' || line[0] == '\t')
      memmove (line, line + 1, strlen (line));
    n++;
    if (line[0] == '\n' || line[0] == '\r' || line[0] == '#')
      continue;

    /* key */
    idx = strcspn (line, " \t");
    if (strlen (line) == idx)
      FATAL ("Malformed config key at line: %d", n);

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
      FATAL ("Malformed config value at line: %d", n);
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

/* Get the enumerated log format given its equivalent format string.
 *
 * On error, -1 is returned.
 * On success, the enumerated format is returned. */
static int
get_log_format_item_enum (const char *str) {
  return str2enum (LOGTYPE, ARRAY_SIZE (LOGTYPE), str);
}

/* Determine the selected log format from the config file or command line
 * option.
 *
 * On error, -1 is returned.
 * On success, the index of the matched item is returned. */
size_t
get_selected_format_idx (void) {
  if (conf.log_format == NULL)
    return (size_t) -1;
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
  else if (strcmp (conf.log_format, logs.awselb) == 0)
    return AWSELB;
  else if (strcmp (conf.log_format, logs.squid) == 0)
    return SQUID;
  else if (strcmp (conf.log_format, logs.awss3) == 0)
    return AWSS3;
  else
    return (size_t) -1;
}

/* Determine the selected log format from the config file or command line
 * option.
 *
 * On error, NULL is returned.
 * On success, an allocated string containing the log format is returned. */
char *
get_selected_format_str (size_t idx) {
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
  case AWSELB:
    fmt = alloc_string (logs.awselb);
    break;
  case SQUID:
    fmt = alloc_string (logs.squid);
    break;
  case AWSS3:
    fmt = alloc_string (logs.awss3);
    break;
  }

  return fmt;
}

/* Determine the selected date format from the config file or command line
 * option.
 *
 * On error, NULL is returned.
 * On success, an allocated string containing the date format is returned. */
char *
get_selected_date_str (size_t idx) {
  char *fmt = NULL;
  switch (idx) {
  case COMMON:
  case VCOMMON:
  case COMBINED:
  case VCOMBINED:
  case AWSS3:
    fmt = alloc_string (dates.apache);
    break;
  case AWSELB:
  case CLOUDFRONT:
  case W3C:
    fmt = alloc_string (dates.w3c);
    break;
  case CLOUDSTORAGE:
    fmt = alloc_string (dates.usec);
    break;
  case SQUID:
    fmt = alloc_string (dates.sec);
    break;
  }

  return fmt;
}

/* Determine the selected time format from the config file or command line
 * option.
 *
 * On error, NULL is returned.
 * On success, an allocated string containing the time format is returned. */
char *
get_selected_time_str (size_t idx) {
  char *fmt = NULL;
  switch (idx) {
  case AWSELB:
  case CLOUDFRONT:
  case COMBINED:
  case COMMON:
  case VCOMBINED:
  case VCOMMON:
  case W3C:
  case AWSS3:
    fmt = alloc_string (times.fmt24);
    break;
  case CLOUDSTORAGE:
    fmt = alloc_string (times.usec);
    break;
  case SQUID:
    fmt = alloc_string (times.sec);
    break;
  }

  return fmt;
}

/* Determine if the log/date/time were set, otherwise exit the program
 * execution. */
const char *
verify_formats (void) {
  if (conf.time_format == NULL || *conf.time_format == '\0')
    return ERR_FORMAT_NO_TIME_FMT;

  if (conf.date_format == NULL || *conf.date_format == '\0')
    return ERR_FORMAT_NO_DATE_FMT;

  if (conf.log_format == NULL || *conf.log_format == '\0')
    return ERR_FORMAT_NO_LOG_FMT;

  return NULL;
}

/* A wrapper function to concat the given specificity to the date
 * format. */
static char *
append_spec_date_format (const char *date_format, const char *spec_format) {
  char *s = xmalloc (snprintf (NULL, 0, "%s%s", date_format, spec_format) + 1);
  sprintf (s, "%s%s", date_format, spec_format);

  return s;
}

/* Iterate over the given format and clean unwanted chars and keep all
 * date/time specifiers such as %b%Y%d%M%S.
 *
 * On error NULL is returned.
 * On success, a clean format containing only date/time specifiers is
 * returned. */
static char *
clean_date_time_format (const char *format) {
  char *fmt = NULL, *pr = NULL, *pw = NULL;
  int special = 0;

  if (format == NULL || *format == '\0')
    return NULL;

  fmt = xstrdup (format);
  pr = fmt;
  pw = fmt;
  while (*pr) {
    *pw = *pr++;
    if (*pw == '%' || special) {
      special = !special;
      pw++;
    }
  }
  *pw = '\0';

  return fmt;
}

/* Determine if the given specifier character is an abbreviated type
 * of date.
 *
 * If it is, 1 is returned, otherwise, 0 is returned. */
static int
is_date_abbreviated (const char *fdate) {
  if (strpbrk (fdate, "cDF"))
    return 1;

  return 0;
}

/* A wrapper to extract time specifiers from a time format.
 *
 * On error NULL is returned.
 * On success, a clean format containing only time specifiers is
 * returned. */
static char *
set_format_time (void) {
  char *ftime = NULL;

  if (has_timestamp (conf.date_format) || !strcmp ("%T", conf.time_format))
    ftime = xstrdup ("%H%M%S");
  else
    ftime = clean_date_time_format (conf.time_format);

  return ftime;
}

/* A wrapper to extract date specifiers from a date format.
 *
 * On error NULL is returned.
 * On success, a clean format containing only date specifiers is
 * returned. */
static char *
set_format_date (void) {
  char *fdate = NULL;

  if (has_timestamp (conf.date_format))
    fdate = xstrdup ("%Y%m%d");
  else
    fdate = clean_date_time_format (conf.date_format);

  return fdate;
}

/* Once we have a numeric date format, we attempt to read the time
 * format and construct a date_time numeric specificity format (if any
 * specificity is given). The result may look like Ymd[HM].
 *
 * On success, the numeric date time specificity format is set. */
static void
set_spec_date_time_num_format (void) {
  char *buf = NULL, *tf = set_format_time ();
  const char *df = conf.date_num_format;

  if (!df || !tf)
    return;

  if (conf.date_spec_hr && strchr (tf, 'H'))
    buf = append_spec_date_format (df, "%H");
  else
    buf = xstrdup (df);

  conf.spec_date_time_num_format = buf;
  free (tf);
}

/* Set a human readable specificity date and time format.
 *
 * On success, the human readable date time specificity format is set. */
static void
set_spec_date_time_format (void) {
  char *buf = NULL;
  const char *fmt = conf.spec_date_time_num_format;
  int buflen = 0, flen = 0;

  if (!fmt)
    return;

  flen = (strlen (fmt) * 2) + 1;
  buf = xcalloc (flen, sizeof (char));

  if (strchr (fmt, 'd'))
    buflen += snprintf (buf + buflen, flen - buflen, "%%d/");
  if (strchr (fmt, 'm'))
    buflen += snprintf (buf + buflen, flen - buflen, "%%b/");
  if (strchr (fmt, 'Y'))
    buflen += snprintf (buf + buflen, flen - buflen, "%%Y");
  if (strchr (fmt, 'H'))
    buflen += snprintf (buf + buflen, flen - buflen, ":%%H");

  conf.spec_date_time_format = buf;
}

/* Normalize the date format from the date format given by the user to
 * Ymd so it can be sorted out properly afterwards.
 *
 * On error or unable to determine the format, 1 is returned.
 * On success, the numeric date format as Ymd is set and 0 is
 * returned. */
static int
set_date_num_format (void) {
  char *fdate = NULL, *buf = NULL;
  int buflen = 0, flen = 0;

  fdate = set_format_date ();
  if (!fdate)
    return 1;

  if (is_date_abbreviated (fdate)) {
    free (fdate);
    conf.date_num_format = xstrdup ("%Y%m%d");
    return 0;
  }

  flen = strlen (fdate) + 1;
  buf = xcalloc (flen, sizeof (char));

  if (strpbrk (fdate, "Yy"))
    buflen += snprintf (buf + buflen, flen - buflen, "%%Y");
  if (strpbrk (fdate, "hbmB"))
    buflen += snprintf (buf + buflen, flen - buflen, "%%m");
  if (strpbrk (fdate, "de"))
    buflen += snprintf (buf + buflen, flen - buflen, "%%d");

  conf.date_num_format = buf;
  free (fdate);

  return buflen == 0 ? 1 : 0;
}

/* If specificity is supplied, then determine which value we need to
 * append to the date format. */
void
set_spec_date_format (void) {
  if (verify_formats ())
    return;

  if (conf.date_num_format)
    free (conf.date_num_format);
  if (conf.spec_date_time_format)
    free (conf.spec_date_time_format);
  if (conf.spec_date_time_num_format)
    free (conf.spec_date_time_num_format);

  if (set_date_num_format () == 0) {
    set_spec_date_time_num_format ();
    set_spec_date_time_format ();
  }
}

/* Attempt to set the date format given a command line option
 * argument. The supplied optarg can be either an actual format string
 * or the enumerated value such as VCOMBINED */
void
set_date_format_str (const char *oarg) {
  char *fmt = NULL;
  int type = get_log_format_item_enum (oarg);

  /* free date format if it was previously set by set_log_format_str() */
  if (conf.date_format)
    free (conf.date_format);

  /* type not found, use whatever was given by the user then */
  if (type == -1) {
    conf.date_format = unescape_str (oarg);
    return;
  }

  /* attempt to get the format string by the enum value */
  if ((fmt = get_selected_date_str (type)) == NULL) {
    LOG_DEBUG (("Unable to set date format from enum: %s\n", oarg));
    return;
  }

  conf.date_format = fmt;
}

/* Attempt to set the time format given a command line option
 * argument. The supplied optarg can be either an actual format string
 * or the enumerated value such as VCOMBINED */
void
set_time_format_str (const char *oarg) {
  char *fmt = NULL;
  int type = get_log_format_item_enum (oarg);

  /* free time format if it was previously set by set_log_format_str() */
  if (conf.time_format)
    free (conf.time_format);

  /* type not found, use whatever was given by the user then */
  if (type == -1) {
    conf.time_format = unescape_str (oarg);
    return;
  }

  /* attempt to get the format string by the enum value */
  if ((fmt = get_selected_time_str (type)) == NULL) {
    LOG_DEBUG (("Unable to set time format from enum: %s\n", oarg));
    return;
  }

  conf.time_format = fmt;
}

/* Attempt to set the log format given a command line option argument.
 * The supplied optarg can be either an actual format string or the
 * enumerated value such as VCOMBINED */
void
set_log_format_str (const char *oarg) {
  char *fmt = NULL;
  int type = get_log_format_item_enum (oarg);

  /* free log format if it was previously set */
  if (conf.log_format)
    free (conf.log_format);

  /* type not found, use whatever was given by the user then */
  if (type == -1) {
    conf.log_format = unescape_str (oarg);
    return;
  }

  /* attempt to get the format string by the enum value */
  if ((fmt = get_selected_format_str (type)) == NULL) {
    LOG_DEBUG (("Unable to set log format from enum: %s\n", oarg));
    return;
  }

  conf.log_format = unescape_str (fmt);
  /* assume we are using the default date/time formats */
  set_time_format_str (oarg);
  set_date_format_str (oarg);
  free (fmt);
}
