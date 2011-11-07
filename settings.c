/**
 * settings.c -- goaccess configuration
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - Ncurses apache weblog analyzer & interactive viewer
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "commons.h"

static void
set_conf_vars (int key, char *val)
{
   switch (key) {
    case 1:
       color_scheme = atoi (val);
       break;
   }
}

/*###TODO: allow extra values for every key
 * separated by a delimeter */
int
parse_conf_file ()
{
   char *val, *c;
   int key = 0;
   char *user_home;

   user_home = getenv ("HOME");
   if (user_home == NULL)
      user_home = "";
   char *path = malloc (snprintf (NULL, 0, "%s/.goaccessrc", user_home) + 1);
   if (path == NULL)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Unable to allocate memory for new window.");
   sprintf (path, "%s/.goaccessrc", user_home);

   char *conf_file = path;
   FILE *file = fopen (conf_file, "r");

   if (file == NULL) {
      free (path);
      return -1;
   }

   char line[512];
   while (fgets (line, sizeof line, file) != NULL) {
      int i;
      for (i = 0; i < keywords_size (); i++) {
         if ((strstr (line, conf_keywords[i][1])) != NULL)
            key = atoi (conf_keywords[i][0]);
      }
      if ((val = strchr (line, ' ')) == NULL) {
         free (path);
         return -1;
      }
      for (c = val; *c; c++) {
         /* get everything after space */
         if (!isspace (c[0])) {
            set_conf_vars (key, c);
            break;
         }
      }
   }

   fclose (file);
   free (path);
   return 0;
}

void
write_conf_file ()
{
   char *user_home;
   user_home = getenv ("HOME");
   if (user_home == NULL)
      user_home = "";

   char *path = malloc (snprintf (NULL, 0, "%s/.goaccessrc", user_home) + 1);
   if (path == NULL)
      error_handler (__PRETTY_FUNCTION__, __FILE__, __LINE__,
                     "Unable to allocate memory for new window.");
   sprintf (path, "%s/.goaccessrc", user_home);

   FILE *file;
   file = fopen (path, "w");

   /* key->value(s) */
   fprintf (file, "color_scheme %d", color_scheme);

   fclose (file);
   free (path);
}
