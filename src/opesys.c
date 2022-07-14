/**
 * opesys.c -- functions for dealing with operating systems
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2022 Gerardo Orellana <hello @ goaccess.io>
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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "opesys.h"

#include "error.h"
#include "settings.h"
#include "util.h"
#include "xmalloc.h"

/* ###NOTE: The size of the list is proportional to the run time,
 * which makes this pretty slow */

/* {"search string", "belongs to"} */
static const char *os[][2] = {
  {"Android", "Android"},
  {"Windows NT 10.0", "Windows"},
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
  {"Mastodon", "Unix-like"},
  {"bingbot", "Windows"},

  {"iPad", "iOS"},
  {"iPod", "iOS"},
  {"iPhone", "iOS"},
  {"CFNetwork", "iOS"},
  {"AppleTV", "iOS"},
  {"iTunes", "Macintosh"},
  {"OS X", "Macintosh"},
  {"Darwin", "Darwin"},

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
  {"Arch", "Linux"},
  {"Parabola", "Linux"},

  {"FreeBSD", "BSD"},
  {"NetBSD", "BSD"},
  {"OpenBSD", "BSD"},
  {"DragonFly", "BSD"},

  {"PlayStation", "BSD"},

  {"Linux", "Linux"},
  {"linux", "Linux"},

  {"CrOS", "Chrome OS"},
  {"QNX", "Unix-like"},
  {"BB10", "Unix-like"},

  {"AIX", "Unix"},
  {"SunOS", "Unix"},

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

/* Get the Android code name (if applicable).
 *
 * On error, the given name is allocated and returned.
 * On success, the matching Android codename is allocated and
 * returned. */
static char *
get_real_android (const char *droid) {
  if (strstr (droid, "11"))
    return alloc_string ("Android 11");
  else if (strstr (droid, "10"))
    return alloc_string ("Android 10");
  else if (strstr (droid, "9"))
    return alloc_string ("Pie 9");
  else if (strstr (droid, "8.1"))
    return alloc_string ("Oreo 8.1");
  else if (strstr (droid, "8.0"))
    return alloc_string ("Oreo 8.0");
  else if (strstr (droid, "7.1"))
    return alloc_string ("Nougat 7.1");
  else if (strstr (droid, "7.0"))
    return alloc_string ("Nougat 7.0");
  else if (strstr (droid, "6.0.1"))
    return alloc_string ("Marshmallow 6.0.1");
  else if (strstr (droid, "6.0"))
    return alloc_string ("Marshmallow 6.0");
  else if (strstr (droid, "5.1"))
    return alloc_string ("Lollipop 5.1");
  else if (strstr (droid, "5.0"))
    return alloc_string ("Lollipop 5.0");
  else if (strstr (droid, "4.4"))
    return alloc_string ("KitKat 4.4");
  else if (strstr (droid, "4.3"))
    return alloc_string ("Jelly Bean 4.3");
  else if (strstr (droid, "4.2"))
    return alloc_string ("Jelly Bean 4.2");
  else if (strstr (droid, "4.1"))
    return alloc_string ("Jelly Bean 4.1");
  else if (strstr (droid, "4.0"))
    return alloc_string ("Ice Cream Sandwich 4.0");
  else if (strstr (droid, "3."))
    return alloc_string ("Honeycomb 3");
  else if (strstr (droid, "2.3"))
    return alloc_string ("Gingerbread 2.3");
  else if (strstr (droid, "2.2"))
    return alloc_string ("Froyo 2.2");
  else if (strstr (droid, "2.0") || strstr (droid, "2.1"))
    return alloc_string ("Eclair 2");
  else if (strstr (droid, "1.6"))
    return alloc_string ("Donut 1.6");
  else if (strstr (droid, "1.5"))
    return alloc_string ("Cupcake 1.5");
  return alloc_string (droid);
}

/* Get the Windows marketing name (if applicable).
 *
 * On error, the given name is allocated and returned.
 * On success, the matching Windows marketing name is allocated and
 * returned. */
static char *
get_real_win (const char *win) {
  if (strstr (win, "10.0"))
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

/* Get the Mac OS X code name (if applicable).
 *
 * On error, the given name is allocated and returned.
 * On success, the matching Mac OS X codename is allocated and
 * returned. */
static char *
get_real_mac_osx (const char *osx) {
  if (strstr (osx, "13.0"))
    return alloc_string ("macOS 12 Ventura");
  else if (strstr (osx, "12.0"))
    return alloc_string ("macOS 12 Monterey");
  else if (strstr (osx, "11.0"))
    return alloc_string ("macOS 11 Big Sur");
  else if (strstr (osx, "10.15"))
    return alloc_string ("macOS 10.15 Catalina");
  else if (strstr (osx, "10.14"))
    return alloc_string ("macOS 10.14 Mojave");
  else if (strstr (osx, "10.13"))
    return alloc_string ("macOS 10.13 High Sierra");
  else if (strstr (osx, "10.12"))
    return alloc_string ("macOS 10.12 Sierra");
  else if (strstr (osx, "10.11"))
    return alloc_string ("OS X 10.11 El Capitan");
  else if (strstr (osx, "10.10"))
    return alloc_string ("OS X 10.10 Yosemite");
  else if (strstr (osx, "10.9"))
    return alloc_string ("OS X 10.9 Mavericks");
  else if (strstr (osx, "10.8"))
    return alloc_string ("OS X 10.8 Mountain Lion");
  else if (strstr (osx, "10.7"))
    return alloc_string ("OS X 10.7 Lion");
  else if (strstr (osx, "10.6"))
    return alloc_string ("OS X 10.6 Snow Leopard");
  else if (strstr (osx, "10.5"))
    return alloc_string ("OS X 10.5 Leopard");
  else if (strstr (osx, "10.4"))
    return alloc_string ("OS X 10.4 Tiger");
  else if (strstr (osx, "10.3"))
    return alloc_string ("OS X 10.3 Panther");
  else if (strstr (osx, "10.2"))
    return alloc_string ("OS X 10.2 Jaguar");
  else if (strstr (osx, "10.1"))
    return alloc_string ("OS X 10.1 Puma");
  else if (strstr (osx, "10.0"))
    return alloc_string ("OS X 10.0 Cheetah");
  return alloc_string (osx);
}

/* Parse all other operating systems.
 *
 * On error, the given name is returned.
 * On success, the parsed OS is returned. */
static char *
parse_others (char *agent, int spaces) {
  char *p;
  int space = 0;
  p = agent;
  /* assume the following chars are within the given agent */
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

/* Parse iOS string including version number.
 *
 * On error, the matching token is returned (no version).
 * On success, the parsed iOS is returned. */
static char *
parse_ios (char *agent, int tlen) {
  char *p = NULL, *q = NULL;
  ptrdiff_t offset;

  if ((p = strstr (agent, " OS ")) == NULL)
    goto out;

  if ((offset = p - agent) <= 0)
    goto out;

  if ((q = strstr (p, " like Mac")) == NULL)
    goto out;

  *q = 0;
  memmove (agent + tlen, agent + offset, offset);
  return char_replace (agent, '_', '.');

out:
  agent[tlen] = 0;
  return agent;
}

/* Parse a Mac OS X string.
 *
 * On error, the given name is returned.
 * On success, the parsed Mac OS X is returned. */
static char *
parse_osx (char *agent) {
  int space = 0;
  char *p;

  p = agent;
  /* assume the following chars are within the given agent */
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

/* Parse an Android string.
 *
 * On error, the given name is returned.
 * On success, the parsed Android is returned. */
static char *
parse_android (char *agent) {
  char *p;
  p = agent;
  /* assume the following chars are within the given agent */
  while (*p != ';' && *p != ')' && *p != '(' && *p != '\0')
    p++;
  *p = 0;

  return agent;
}

/* Attempt to parse specific OS.
 *
 * On success, a malloc'd string containing the OS is returned. */
static char *
parse_os (char *str, char *tkn, char *os_type, int idx) {
  char *b;
  int spaces = 0;

  xstrncpy (os_type, os[idx][1], OPESYS_TYPE_LEN);
  /* Windows */
  if ((strstr (str, "Windows")) != NULL)
    return conf.real_os && (b = get_real_win (tkn)) ? b : xstrdup (os[idx][0]);
  /* Android */
  if ((strstr (tkn, "Android")) != NULL) {
    tkn = parse_android (tkn);
    return conf.real_os ? get_real_android (tkn) : xstrdup (tkn);
  }
  /* iOS */
  if ((strstr (tkn, "CFNetwork")) != NULL) {
    if ((b = strchr (str, ' ')))
      *b = 0;
    return xstrdup (str);
  }
  if (strstr (tkn, "iPad") || strstr (tkn, "iPod"))
    return xstrdup (parse_ios (tkn, 4));
  if (strstr (tkn, "iPhone"))
    return xstrdup (parse_ios (tkn, 6));
  /* Mac OS X */
  if ((strstr (tkn, "OS X")) != NULL) {
    tkn = parse_osx (tkn);
    return conf.real_os ? get_real_mac_osx (tkn) : xstrdup (tkn);
  }
  /* Darwin - capture the first part of agents such as:
   * Slack/248000 CFNetwork/808.0.2 Darwin/16.0.0 */
  if ((strstr (tkn, "Darwin")) != NULL) {
    if ((b = strchr (str, ' ')))
      *b = 0;
    return xstrdup (str);
  }
  /* all others */
  spaces = count_matches (os[idx][0], ' ');

  return alloc_string (parse_others (tkn, spaces));
}

/* Given a user agent, determine the operating system used.
 *
 * ###NOTE: The size of the list is proportional to the run time,
 * which makes this pretty slow
 *
 * On error, NULL is returned.
 * On success, a malloc'd  string containing the OS is returned. */
char *
verify_os (char *str, char *os_type) {
  char *a;
  size_t i;

  if (str == NULL || *str == '\0')
    return NULL;

  str = char_replace (str, '+', ' ');
  for (i = 0; i < ARRAY_SIZE (os); i++) {
    if ((a = strstr (str, os[i][0])) != NULL)
      return parse_os (str, a, os_type, i);
  }
  xstrncpy (os_type, "Unknown", OPESYS_TYPE_LEN);

  if (conf.unknowns_log)
    LOG_UNKNOWNS (("%-7s%s\n", "[OS]", str));

  return alloc_string ("Unknown");
}
