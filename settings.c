/**
 * settings.c -- goaccess configuration
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

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "settings.h"

#include "commons.h"
#include "error.h"
#include "xmalloc.h"
#include "gmenu.h"
#include "util.h"

char *tmp_log_format = NULL;
char *tmp_date_format = NULL;

/* *INDENT-OFF* */
static GPreConfLog logs = {
   "%h %^[%d:%^] \"%r\" %s %b \"%R\" \"%u\"",       /* CLF           */
   "%h %^[%d:%^] \"%r\" %s %b",                     /* CLF w/ VHost  */
   "%^:%^ %h %^[%d:%^] \"%r\" %s %b \"%R\" \"%u\"", /* NCSA          */
   "%^:%^ %h %^[%d:%^] \"%r\" %s %b",               /* NCSA w/ VHost */
   "%d %^ %h %^ %^ %^ %^ %r %^ %s %b %^ %^ %u %R",  /* W3C           */
   "%d %^ %^ %b %h %^ %^ %r %s %R %u %^"            /* CloudFront    */
};

static GPreConfDate dates = {
   "%d/%b/%Y", /* Apache     */
   "%Y-%m-%d", /* W3C        */
   "%m/%d/%Y"  /* CloudFront */
};
/* *INDENT-ON* */

/* config file keywords */
static GConfKeyword keywords[] = {
   {1, "color_scheme"},
   {2, "log_format"},
   {3, "date_format"}
};

/* set config key/values pair */
static void
set_conf_vars (int key, char *val)
{
   switch (key) {
    case 1:
       conf.color_scheme = atoi (val);
       break;
    case 2:
       conf.log_format = alloc_string (val);
       break;
    case 3:
       conf.date_format = alloc_string (val);
       break;
   }
}

/* ###TODO: allow extra values for every key separated by a delimeter */
int
parse_conf_file ()
{
   char *path = NULL, *conf_file = NULL, *user_home = NULL;
   char *val, *c;
   int key = 0;
   FILE *file;

   user_home = getenv ("HOME");
   if (user_home == NULL)
      user_home = "";

   path = xmalloc (snprintf (NULL, 0, "%s/.goaccessrc", user_home) + 1);
   sprintf (path, "%s/.goaccessrc", user_home);

   conf_file = path;
   file = fopen (conf_file, "r");

   if (file == NULL) {
      free (path);
      return -1;
   }

   char line[512];
   while (fgets (line, sizeof line, file) != NULL) {
      unsigned int i;
      for (i = 0; i < ARRAY_SIZE (keywords); i++) {
         if (strstr (line, keywords[i].keyword) != NULL)
            key = keywords[i].key_id;
      }
      if ((val = strchr (line, ' ')) == NULL) {
         free (path);
         return -1;
      }
      for (c = val; *c; c++) {
         /*
          * get everything after space 
          */
         if (!isspace (c[0])) {
            set_conf_vars (key, trim_str (c));
            break;
         }
      }
   }
   fclose (file);
   free (path);
   return 0;
}

/* write config key/value pair to goaccessrc */
void
write_conf_file ()
{
   char *user_home;
   user_home = getenv ("HOME");
   if (user_home == NULL)
      user_home = "";

   char *path = xmalloc (snprintf (NULL, 0, "%s/.goaccessrc", user_home) + 1);
   sprintf (path, "%s/.goaccessrc", user_home);

   FILE *file;
   file = fopen (path, "w");
   /*
    * no file available 
    */
   if (file == NULL) {
      free (path);
      return;
   }

   /*
    * color scheme 
    */
   fprintf (file, "color_scheme %d\n", conf.color_scheme);

   /*
    * date format 
    */
   if (tmp_date_format)
      fprintf (file, "date_format %s\n", tmp_date_format);
   else
      fprintf (file, "date_format %s\n", conf.date_format);

   /*
    * log format 
    */
   if (tmp_log_format)
      fprintf (file, "log_format %s", tmp_log_format);
   else
      fprintf (file, "log_format %s", conf.log_format);

   fclose (file);
   if (conf.date_format)
      free (conf.date_format);
   if (conf.log_format)
      free (conf.log_format);
   free (path);
}

/* return the index of the matched item, or -1 if no such item exists */
size_t
get_selected_format_idx ()
{
   if (conf.log_format == NULL)
      return -1;
   if (strcmp (conf.log_format, logs.common) == 0)
      return 0;
   else if (strcmp (conf.log_format, logs.vcommon) == 0)
      return 1;
   else if (strcmp (conf.log_format, logs.combined) == 0)
      return 2;
   else if (strcmp (conf.log_format, logs.vcombined) == 0)
      return 3;
   else if (strcmp (conf.log_format, logs.w3c) == 0)
      return 4;
   else if (strcmp (conf.log_format, logs.cloudfront) == 0)
      return 5;
   else
      return -1;
}

/* return the string of the matched item, or NULL if no such item exists */
char *
get_selected_format_str (size_t idx)
{
   char *fmt = NULL;
   switch (idx) {
    case 0:
       fmt = alloc_string (logs.common);
       break;
    case 1:
       fmt = alloc_string (logs.vcommon);
       break;
    case 2:
       fmt = alloc_string (logs.combined);
       break;
    case 3:
       fmt = alloc_string (logs.vcombined);
       break;
    case 4:
       fmt = alloc_string (logs.w3c);
       break;
    case 5:
       fmt = alloc_string (logs.cloudfront);
       break;
   }

   return fmt;
}

char *
get_selected_date_str (size_t idx)
{
   char *fmt = NULL;
   switch (idx) {
    case 0:
    case 1:
    case 2:
    case 3:
       fmt = alloc_string (dates.apache);
       break;
    case 4:
       fmt = alloc_string (dates.w3c);
       break;
    case 5:
       fmt = alloc_string (dates.cloudfront);
       break;
   }

   return fmt;
}
