/**
 * xmalloc.c -- *alloc functions with error handling
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

#include <stdio.h>
#if !defined __SUNPRO_C
#include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "xmalloc.h"

/* self-checking wrapper to malloc() */
void *
xmalloc (size_t size)
{
  void *ptr;

  if ((ptr = malloc (size)) == NULL)
    FATAL ("Unable to allocate memory - failed.");

  return (ptr);
}

char *
xstrdup (const char *s)
{
  char *ptr;
  size_t len;

  len = strlen (s) + 1;
  ptr = xmalloc (len);

  strncpy (ptr, s, len);
  return (ptr);
}

/* self-checking wrapper to calloc() */
void *
xcalloc (size_t nmemb, size_t size)
{
  void *ptr;

  if ((ptr = calloc (nmemb, size)) == NULL)
    FATAL ("Unable to calloc memory - failed.");

  return (ptr);
}

/* self-checking wrapper to realloc() */
void *
xrealloc (void *oldptr, size_t size)
{
  void *newptr;

  if ((newptr = realloc (oldptr, size)) == NULL)
    FATAL ("Unable to reallocate memory - failed");

  return (newptr);
}
