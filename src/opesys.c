/**
 * opesys.c -- functions for dealing with operating systems
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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opesys.h"

#include "settings.h"
#include "util.h"
#include "xmalloc.h"

/* {"search string", "belongs to"} */
static const char *os[][2] = {
  {"Android", "Android"},
  {"Windows NT 6.4", "Windows"},
  {"Windows NT 6.3; ARM", "Windows"},
  {"Windows NT 6.3", "Windows"},
  {"Windows NT 6.2; ARM", "Windows"},
  {"Windows NT 6.2", "Windows"},
  {"Windows NT 6.1", "Windows"},
  {"Windows NT 6.0", "Windows"},
  {"Windows NT 5.2", "Windows"},
  {"Windows NT 5.1", "Windows"},
  {"Windows NT 5.01", "Windows"},
  {"Windows NT 5.0", "Windows"},
  {"Windows NT 4.0", "Windows"},
  {"Windows NT", "Windows"},
  {"Win 9x 4.90", "Windows"},
  {"Windows 98", "Windows"},
  {"Windows 95", "Windows"},
  {"Windows CE", "Windows"},
  {"Windows Phone 8.1", "Windows"},
  {"Windows Phone 8.0", "Windows"},

  {"Googlebot", "Unix-like"},
  {"bingbot", "Windows"},

  {"iPad", "iOS"},
  {"iPod", "iOS"},
  {"iPhone", "iOS"},
  {"AppleTV", "iOS"},
  {"iTunes", "Macintosh"},
  {"OS X", "Macintosh"},

  {"Debian", "Linux"},
  {"Ubuntu", "Linux"},
  {"Fedora", "Linux"},
  {"Mint", "Linux"},
  {"SUSE", "Linux"},
  {"Mandriva", "Linux"},
  {"Red Hat", "Linux"},
  {"Gentoo", "Linux"},
  {"CentOS", "Linux"},
  {"PCLinuxOS", "Linux"},
  {"Linux", "Linux"},

  {"FreeBSD", "BSD"},
  {"NetBSD", "BSD"},
  {"OpenBSD", "BSD"},
  {"PlayStation", "BSD"},

  {"CrOS", "Chrome OS"},
  {"SunOS", "Unix-like"},
  {"QNX", "Unix-like"},
  {"BB10", "Unix-like"},

  {"BlackBerry", "Others"},
  {"Sony", "Others"},
  {"AmigaOS", "Others"},
  {"SymbianOS", "Others"},
  {"Nokia", "Others"},
  {"Nintendo", "Others"},
  {"Apache", "Others"},
  {"Xbox One", "Windows"},
  {"Xbox", "Windows"},
};

/* get Android Codename */
static char *
get_real_android (const char *droid)
{
  if (strstr (droid, "5.0") || strstr (droid, "5.1"))
    return alloc_string ("Lollipop");
  else if (strstr (droid, "4.4"))
    return alloc_string ("KitKat");
  else if (strstr (droid, "4.3") || strstr (droid, "4.2") ||
           strstr (droid, "4.1"))
    return alloc_string ("Jelly Bean");
  else if (strstr (droid, "4.0"))
    return alloc_string ("Ice Cream Sandwich");
  else if (strstr (droid, "3."))
    return alloc_string ("Honeycomb");
  else if (strstr (droid, "2.3"))
    return alloc_string ("Gingerbread");
  else if (strstr (droid, "2.2"))
    return alloc_string ("Froyo");
  else if (strstr (droid, "2.0") || strstr (droid, "2.1"))
    return alloc_string ("Eclair");
  else if (strstr (droid, "1.6"))
    return alloc_string ("Donut");
  else if (strstr (droid, "1.5"))
    return alloc_string ("Cupcake");
  return alloc_string (droid);
}

/* get Windows marketing name */
static char *
get_real_win (const char *win)
{
  if (strstr (win, "6.4"))
    return alloc_string ("Windows 10");
  else if (strstr (win, "6.3"))
    return alloc_string ("Windows 8.1");
  else if (strstr (win, "6.3; ARM"))
    return alloc_string ("Windows RT");
  else if (strstr (win, "6.2; ARM"))
    return alloc_string ("Windows RT");
  else if (strstr (win, "6.2"))
    return alloc_string ("Windows 8");
  else if (strstr (win, "6.1"))
    return alloc_string ("Windows 7");
  else if (strstr (win, "6.0"))
    return alloc_string ("Windows Vista");
  else if (strstr (win, "5.2"))
    return alloc_string ("Windows XP x64");
  else if (strstr (win, "5.1"))
    return alloc_string ("Windows XP");
  else if (strstr (win, "5.0"))
    return alloc_string ("Windows 2000");
  return NULL;
}

/* get Mac OS X Codename */
static char *
get_real_mac_osx (const char *osx)
{
  if (strstr (osx, "10.10"))
    return alloc_string ("OS X Yosemite");
  else if (strstr (osx, "10.9"))
    return alloc_string ("OS X Mavericks");
  else if (strstr (osx, "10.8"))
    return alloc_string ("OS X Mountain Lion");
  else if (strstr (osx, "10.7"))
    return alloc_string ("OS X Lion");
  else if (strstr (osx, "10.6"))
    return alloc_string ("OS X Snow Leopard");
  else if (strstr (osx, "10.5"))
    return alloc_string ("OS X Leopard");
  else if (strstr (osx, "10.4"))
    return alloc_string ("OS X Tiger");
  else if (strstr (osx, "10.3"))
    return alloc_string ("OS X Panther");
  else if (strstr (osx, "10.2"))
    return alloc_string ("OS X Jaguar");
  else if (strstr (osx, "10.1"))
    return alloc_string ("OS X Puma");
  else if (strstr (osx, "10.0"))
    return alloc_string ("OS X Cheetah");
  return alloc_string (osx);
}

static char *
parse_others (char *agent, int spaces)
{
  char *p;
  int space = 0;
  p = agent;
  while (*p != ';' && *p != ')' && *p != '(' && *p != '\0') {
    if (*p == ' ')
      space++;
    if (space > spaces)
      break;
    p++;
  }
  *p = 0;

  return agent;
}

static char *
parse_osx (char *agent)
{
  int space = 0;
  char *p;

  p = agent;
  while (*p != ';' && *p != ')' && *p != '(' && *p != '\0') {
    if (*p == '_')
      *p = '.';
    if (*p == ' ')
      space++;
    if (space > 3)
      break;
    p++;
  }
  *p = 0;

  return agent;
}

static char *
parse_android (char *agent)
{
  char *p;
  p = agent;
  while (*p != ';' && *p != ')' && *p != '(' && *p != '\0')
    p++;
  *p = 0;

  return agent;
}

char *
verify_os (const char *str, char *os_type)
{
  char *a, *b;
  int spaces = 0;
  size_t i;

  if (str == NULL || *str == '\0')
    return NULL;

  for (i = 0; i < ARRAY_SIZE (os); i++) {
    if ((a = strstr (str, os[i][0])) == NULL)
      continue;

    xstrncpy (os_type, os[i][1], OPESYS_TYPE_LEN);
    /* Windows */
    if ((strstr (str, "Windows")) != NULL) {
      return conf.real_os && (b = get_real_win (a)) ? b : xstrdup (os[i][0]);
    }
    /* Android */
    if ((strstr (a, "Android")) != NULL) {
      a = parse_android (a);
      return conf.real_os ? get_real_android (a) : xstrdup (a);
    }
    /* Mac OS X */
    if ((strstr (a, "OS X")) != NULL) {
      a = parse_osx (a);
      return conf.real_os ? get_real_mac_osx (a) : xstrdup (a);
    }
    /* all others */
    spaces = count_matches (os[i][0], ' ');
    return alloc_string (parse_others (a, spaces));
  }
  xstrncpy (os_type, "Unknown", OPESYS_TYPE_LEN);

  return alloc_string ("Unknown");
}
