/**
 * output.c -- output to the standard output stream
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

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>

#include "output.h"

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabinet.h"
#else
#include "glibht.h"
#endif

#ifdef HAVE_LIBGEOIP
#include "geolocation.h"
#endif

#include "commons.h"
#include "error.h"
#include "gdns.h"
#include "settings.h"
#include "ui.h"
#include "util.h"
#include "xmalloc.h"

/* *INDENT-OFF* */

static void print_html_visitors (FILE * fp, GHolder * h, int processed, const GOutput * panel);
static void print_html_requests (FILE * fp, GHolder * h, int processed, const GOutput * panel);
static void print_html_common (FILE * fp, GHolder * h, int processed, const GOutput * panel);

static GOutput paneling[] = {
  {VISITORS        , print_html_visitors , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 1, 0} ,
  {REQUESTS        , print_html_requests , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0, 0} ,
  {REQUESTS_STATIC , print_html_requests , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0, 0} ,
  {NOT_FOUND       , print_html_requests , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 0, 0} ,
  {HOSTS           , print_html_common   , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 1, 0} ,
  {OS              , print_html_common   , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 1, 1} ,
  {BROWSERS        , print_html_common   , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 1, 1} ,
  {REFERRERS       , print_html_common   , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0, 0} ,
  {REFERRING_SITES , print_html_common   , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0, 0} ,
  {KEYPHRASES      , print_html_common   , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0, 0} ,
#ifdef HAVE_LIBGEOIP
  {GEO_LOCATION    , print_html_common   , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0, 0} ,
#endif
  {STATUS_CODES    , print_html_common   , 1 , 1 , 1 , 1 , 1 , 0 , 0 , 1 , 0, 0} ,
};

/* base64 expand/collapse icons */
unsigned char icons [] = {
  0x64, 0x30, 0x39, 0x47, 0x52, 0x67, 0x41, 0x42, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x56, 0x38, 0x41, 0x41, 0x73, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x42, 0x54, 0x41, 0x41, 0x41, 0x51, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x42, 0x50,
  0x55, 0x79, 0x38, 0x79, 0x41, 0x41, 0x41, 0x42, 0x43, 0x41, 0x41, 0x41,
  0x41, 0x47, 0x41, 0x41, 0x41, 0x41, 0x42, 0x67, 0x44, 0x78, 0x49, 0x4e,
  0x6b, 0x47, 0x4e, 0x74, 0x59, 0x58, 0x41, 0x41, 0x41, 0x41, 0x46, 0x6f,
  0x41, 0x41, 0x41, 0x41, 0x54, 0x41, 0x41, 0x41, 0x41, 0x45, 0x77, 0x50,
  0x38, 0x4f, 0x45, 0x69, 0x5a, 0x32, 0x46, 0x7a, 0x63, 0x41, 0x41, 0x41,
  0x41, 0x62, 0x51, 0x41, 0x41, 0x41, 0x41, 0x49, 0x41, 0x41, 0x41, 0x41,
  0x43, 0x41, 0x41, 0x41, 0x41, 0x42, 0x42, 0x6e, 0x62, 0x48, 0x6c, 0x6d,
  0x41, 0x41, 0x41, 0x42, 0x76, 0x41, 0x41, 0x41, 0x41, 0x62, 0x51, 0x41,
  0x41, 0x41, 0x47, 0x30, 0x52, 0x52, 0x42, 0x73, 0x6c, 0x47, 0x68, 0x6c,
  0x59, 0x57, 0x51, 0x41, 0x41, 0x41, 0x4e, 0x77, 0x41, 0x41, 0x41, 0x41,
  0x4e, 0x67, 0x41, 0x41, 0x41, 0x44, 0x59, 0x45, 0x6c, 0x7a, 0x43, 0x5a,
  0x61, 0x47, 0x68, 0x6c, 0x59, 0x51, 0x41, 0x41, 0x41, 0x36, 0x67, 0x41,
  0x41, 0x41, 0x41, 0x6b, 0x41, 0x41, 0x41, 0x41, 0x4a, 0x41, 0x63, 0x77,
  0x41, 0x38, 0x64, 0x6f, 0x62, 0x58, 0x52, 0x34, 0x41, 0x41, 0x41, 0x44,
  0x7a, 0x41, 0x41, 0x41, 0x41, 0x42, 0x67, 0x41, 0x41, 0x41, 0x41, 0x59,
  0x43, 0x67, 0x41, 0x41, 0x42, 0x32, 0x78, 0x76, 0x59, 0x32, 0x45, 0x41,
  0x41, 0x41, 0x50, 0x6b, 0x41, 0x41, 0x41, 0x41, 0x44, 0x67, 0x41, 0x41,
  0x41, 0x41, 0x34, 0x42, 0x41, 0x67, 0x43, 0x51, 0x62, 0x57, 0x46, 0x34,
  0x63, 0x41, 0x41, 0x41, 0x41, 0x2f, 0x51, 0x41, 0x41, 0x41, 0x41, 0x67,
  0x41, 0x41, 0x41, 0x41, 0x49, 0x41, 0x41, 0x4a, 0x41, 0x45, 0x4a, 0x75,
  0x59, 0x57, 0x31, 0x6c, 0x41, 0x41, 0x41, 0x45, 0x46, 0x41, 0x41, 0x41,
  0x41, 0x55, 0x55, 0x41, 0x41, 0x41, 0x46, 0x46, 0x56, 0x78, 0x6d, 0x6d,
  0x37, 0x6e, 0x42, 0x76, 0x63, 0x33, 0x51, 0x41, 0x41, 0x41, 0x56, 0x63,
  0x41, 0x41, 0x41, 0x41, 0x49, 0x41, 0x41, 0x41, 0x41, 0x43, 0x41, 0x41,
  0x41, 0x77, 0x41, 0x41, 0x41, 0x41, 0x4d, 0x45, 0x41, 0x41, 0x47, 0x51,
  0x41, 0x41, 0x55, 0x41, 0x41, 0x41, 0x4b, 0x5a, 0x41, 0x73, 0x77, 0x41,
  0x41, 0x41, 0x43, 0x50, 0x41, 0x70, 0x6b, 0x43, 0x7a, 0x41, 0x41, 0x41,
  0x41, 0x65, 0x73, 0x41, 0x4d, 0x77, 0x45, 0x4a, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x52, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x51, 0x41, 0x41, 0x41, 0x38, 0x47, 0x59, 0x44,
  0x77, 0x50, 0x2f, 0x41, 0x41, 0x45, 0x41, 0x44, 0x77, 0x41, 0x42, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x51, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x49, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x67, 0x41, 0x41, 0x41, 0x41, 0x4d, 0x41,
  0x41, 0x41, 0x41, 0x55, 0x41, 0x41, 0x4d, 0x41, 0x41, 0x51, 0x41, 0x41,
  0x41, 0x42, 0x51, 0x41, 0x42, 0x41, 0x41, 0x34, 0x41, 0x41, 0x41, 0x41,
  0x43, 0x67, 0x41, 0x49, 0x41, 0x41, 0x49, 0x41, 0x41, 0x67, 0x41, 0x42,
  0x41, 0x43, 0x44, 0x77, 0x5a, 0x76, 0x2f, 0x39, 0x2f, 0x2f, 0x38, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x43, 0x44, 0x77, 0x5a, 0x66, 0x2f, 0x39,
  0x2f, 0x2f, 0x38, 0x41, 0x41, 0x66, 0x2f, 0x6a, 0x44, 0x35, 0x38, 0x41,
  0x41, 0x77, 0x41, 0x42, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x42, 0x41, 0x41, 0x48, 0x2f,
  0x2f, 0x77, 0x41, 0x50, 0x41, 0x41, 0x45, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x43, 0x41, 0x41, 0x41, 0x33,
  0x4f, 0x51, 0x45, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x51, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x49, 0x41,
  0x41, 0x44, 0x63, 0x35, 0x41, 0x51, 0x41, 0x41, 0x41, 0x41, 0x41, 0x42,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x67, 0x41, 0x41, 0x4e, 0x7a, 0x6b, 0x42, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x49, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x32, 0x34, 0x44,
  0x62, 0x67, 0x41, 0x66, 0x41, 0x44, 0x38, 0x41, 0x41, 0x41, 0x45, 0x55,
  0x44, 0x77, 0x45, 0x58, 0x46, 0x68, 0x55, 0x55, 0x42, 0x77, 0x59, 0x6a,
  0x49, 0x53, 0x49, 0x6e, 0x4a, 0x6a, 0x55, 0x52, 0x4e, 0x44, 0x63, 0x32,
  0x4d, 0x7a, 0x49, 0x66, 0x41, 0x54, 0x63, 0x32, 0x4d, 0x7a, 0x49, 0x66,
  0x41, 0x52, 0x59, 0x56, 0x41, 0x52, 0x45, 0x55, 0x42, 0x77, 0x59, 0x6a,
  0x49, 0x69, 0x38, 0x42, 0x42, 0x77, 0x59, 0x6a, 0x49, 0x69, 0x38, 0x42,
  0x4a, 0x6a, 0x55, 0x30, 0x50, 0x77, 0x45, 0x6e, 0x4a, 0x6a, 0x55, 0x30,
  0x4e, 0x7a, 0x59, 0x7a, 0x49, 0x54, 0x49, 0x58, 0x46, 0x68, 0x55, 0x42,
  0x72, 0x77, 0x57, 0x2b, 0x55, 0x67, 0x73, 0x4c, 0x43, 0x77, 0x37, 0x2f,
  0x41, 0x41, 0x38, 0x4c, 0x43, 0x77, 0x73, 0x4c, 0x44, 0x77, 0x34, 0x4c,
  0x55, 0x37, 0x30, 0x47, 0x42, 0x77, 0x67, 0x47, 0x51, 0x51, 0x55, 0x42,
  0x76, 0x77, 0x73, 0x4c, 0x44, 0x77, 0x38, 0x4c, 0x55, 0x72, 0x34, 0x46,
  0x43, 0x41, 0x63, 0x47, 0x51, 0x51, 0x59, 0x47, 0x76, 0x6c, 0x4d, 0x4b,
  0x43, 0x67, 0x73, 0x50, 0x41, 0x51, 0x41, 0x50, 0x43, 0x77, 0x73, 0x42,
  0x57, 0x77, 0x63, 0x47, 0x76, 0x56, 0x4d, 0x4c, 0x44, 0x67, 0x38, 0x4c,
  0x43, 0x77, 0x73, 0x4c, 0x44, 0x77, 0x45, 0x41, 0x44, 0x67, 0x73, 0x4c,
  0x43, 0x31, 0x4b, 0x2b, 0x42, 0x51, 0x56, 0x42, 0x42, 0x67, 0x67, 0x42,
  0x37, 0x76, 0x38, 0x41, 0x44, 0x77, 0x73, 0x4b, 0x43, 0x6c, 0x4f, 0x2b,
  0x42, 0x67, 0x5a, 0x42, 0x42, 0x67, 0x63, 0x49, 0x42, 0x62, 0x35, 0x53,
  0x43, 0x77, 0x38, 0x50, 0x43, 0x77, 0x73, 0x4c, 0x43, 0x77, 0x38, 0x41,
  0x41, 0x67, 0x41, 0x48, 0x41, 0x41, 0x63, 0x44, 0x5a, 0x67, 0x4e, 0x6d,
  0x41, 0x42, 0x38, 0x41, 0x50, 0x77, 0x41, 0x41, 0x41, 0x52, 0x45, 0x55,
  0x42, 0x77, 0x59, 0x6a, 0x49, 0x69, 0x38, 0x42, 0x42, 0x77, 0x59, 0x6a,
  0x49, 0x69, 0x38, 0x42, 0x4a, 0x6a, 0x55, 0x30, 0x50, 0x77, 0x45, 0x6e,
  0x4a, 0x6a, 0x55, 0x30, 0x4e, 0x7a, 0x59, 0x7a, 0x49, 0x54, 0x49, 0x58,
  0x46, 0x68, 0x55, 0x42, 0x46, 0x41, 0x38, 0x42, 0x46, 0x78, 0x59, 0x56,
  0x46, 0x41, 0x63, 0x47, 0x49, 0x79, 0x45, 0x69, 0x4a, 0x79, 0x59, 0x31,
  0x45, 0x54, 0x51, 0x33, 0x4e, 0x6a, 0x4d, 0x79, 0x48, 0x77, 0x45, 0x33,
  0x4e, 0x6a, 0x4d, 0x79, 0x48, 0x77, 0x45, 0x57, 0x46, 0x51, 0x47, 0x33,
  0x43, 0x77, 0x73, 0x50, 0x44, 0x77, 0x70, 0x54, 0x76, 0x51, 0x59, 0x49,
  0x42, 0x77, 0x5a, 0x42, 0x42, 0x67, 0x61, 0x2b, 0x55, 0x67, 0x73, 0x4c,
  0x43, 0x67, 0x38, 0x42, 0x41, 0x41, 0x38, 0x4c, 0x43, 0x77, 0x47, 0x76,
  0x42, 0x62, 0x35, 0x53, 0x43, 0x77, 0x73, 0x4c, 0x44, 0x2f, 0x38, 0x41,
  0x44, 0x67, 0x73, 0x4c, 0x43, 0x77, 0x73, 0x4f, 0x44, 0x77, 0x74, 0x53,
  0x76, 0x67, 0x59, 0x48, 0x43, 0x41, 0x56, 0x43, 0x42, 0x51, 0x47, 0x53,
  0x2f, 0x77, 0x41, 0x50, 0x43, 0x67, 0x73, 0x4c, 0x55, 0x72, 0x34, 0x47,
  0x42, 0x6b, 0x45, 0x47, 0x42, 0x77, 0x67, 0x47, 0x76, 0x56, 0x4d, 0x4b,
  0x44, 0x77, 0x38, 0x4c, 0x43, 0x77, 0x73, 0x4c, 0x44, 0x77, 0x47, 0x41,
  0x42, 0x77, 0x61, 0x2b, 0x55, 0x67, 0x73, 0x50, 0x44, 0x67, 0x73, 0x4c,
  0x43, 0x77, 0x73, 0x4f, 0x41, 0x51, 0x41, 0x50, 0x43, 0x77, 0x73, 0x4c,
  0x55, 0x72, 0x34, 0x46, 0x42, 0x55, 0x49, 0x46, 0x43, 0x41, 0x41, 0x42,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x51, 0x41, 0x41, 0x6d, 0x6a, 0x47, 0x6e,
  0x66, 0x46, 0x38, 0x50, 0x50, 0x50, 0x55, 0x41, 0x43, 0x77, 0x51, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x4e, 0x45, 0x46, 0x64, 0x68, 0x63, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x30, 0x51, 0x56, 0x32, 0x46, 0x77, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x44, 0x62, 0x67, 0x4e, 0x75, 0x41, 0x41, 0x41, 0x41,
  0x43, 0x41, 0x41, 0x43, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x45, 0x41, 0x41, 0x41, 0x50, 0x41, 0x2f, 0x38, 0x41, 0x41,
  0x41, 0x41, 0x51, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x4e, 0x75,
  0x41, 0x41, 0x45, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x47,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x67, 0x41, 0x41, 0x41, 0x41, 0x51, 0x41,
  0x41, 0x41, 0x41, 0x45, 0x41, 0x41, 0x41, 0x48, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x4b, 0x41, 0x42, 0x51, 0x41, 0x48, 0x67, 0x42, 0x38,
  0x41, 0x4e, 0x6f, 0x41, 0x41, 0x41, 0x41, 0x42, 0x41, 0x41, 0x41, 0x41,
  0x42, 0x67, 0x42, 0x41, 0x41, 0x41, 0x49, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x49, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x44, 0x67, 0x43, 0x75, 0x41, 0x41, 0x45, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x45, 0x41, 0x44, 0x67, 0x41, 0x41,
  0x41, 0x41, 0x45, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x49, 0x41,
  0x44, 0x67, 0x42, 0x48, 0x41, 0x41, 0x45, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x4d, 0x41, 0x44, 0x67, 0x41, 0x6b, 0x41, 0x41, 0x45, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x51, 0x41, 0x44, 0x67, 0x42, 0x56,
  0x41, 0x41, 0x45, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x55, 0x41,
  0x46, 0x67, 0x41, 0x4f, 0x41, 0x41, 0x45, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x59, 0x41, 0x42, 0x77, 0x41, 0x79, 0x41, 0x41, 0x45, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x6f, 0x41, 0x4e, 0x41, 0x42, 0x6a,
  0x41, 0x41, 0x4d, 0x41, 0x41, 0x51, 0x51, 0x4a, 0x41, 0x41, 0x45, 0x41,
  0x44, 0x67, 0x41, 0x41, 0x41, 0x41, 0x4d, 0x41, 0x41, 0x51, 0x51, 0x4a,
  0x41, 0x41, 0x49, 0x41, 0x44, 0x67, 0x42, 0x48, 0x41, 0x41, 0x4d, 0x41,
  0x41, 0x51, 0x51, 0x4a, 0x41, 0x41, 0x4d, 0x41, 0x44, 0x67, 0x41, 0x6b,
  0x41, 0x41, 0x4d, 0x41, 0x41, 0x51, 0x51, 0x4a, 0x41, 0x41, 0x51, 0x41,
  0x44, 0x67, 0x42, 0x56, 0x41, 0x41, 0x4d, 0x41, 0x41, 0x51, 0x51, 0x4a,
  0x41, 0x41, 0x55, 0x41, 0x46, 0x67, 0x41, 0x4f, 0x41, 0x41, 0x4d, 0x41,
  0x41, 0x51, 0x51, 0x4a, 0x41, 0x41, 0x59, 0x41, 0x44, 0x67, 0x41, 0x35,
  0x41, 0x41, 0x4d, 0x41, 0x41, 0x51, 0x51, 0x4a, 0x41, 0x41, 0x6f, 0x41,
  0x4e, 0x41, 0x42, 0x6a, 0x41, 0x47, 0x6b, 0x41, 0x59, 0x77, 0x42, 0x76,
  0x41, 0x47, 0x30, 0x41, 0x62, 0x77, 0x42, 0x76, 0x41, 0x47, 0x34, 0x41,
  0x56, 0x67, 0x42, 0x6c, 0x41, 0x48, 0x49, 0x41, 0x63, 0x77, 0x42, 0x70,
  0x41, 0x47, 0x38, 0x41, 0x62, 0x67, 0x41, 0x67, 0x41, 0x44, 0x45, 0x41,
  0x4c, 0x67, 0x41, 0x77, 0x41, 0x47, 0x6b, 0x41, 0x59, 0x77, 0x42, 0x76,
  0x41, 0x47, 0x30, 0x41, 0x62, 0x77, 0x42, 0x76, 0x41, 0x47, 0x35, 0x70,
  0x59, 0x32, 0x39, 0x74, 0x62, 0x32, 0x39, 0x75, 0x41, 0x47, 0x6b, 0x41,
  0x59, 0x77, 0x42, 0x76, 0x41, 0x47, 0x30, 0x41, 0x62, 0x77, 0x42, 0x76,
  0x41, 0x47, 0x34, 0x41, 0x55, 0x67, 0x42, 0x6c, 0x41, 0x47, 0x63, 0x41,
  0x64, 0x51, 0x42, 0x73, 0x41, 0x47, 0x45, 0x41, 0x63, 0x67, 0x42, 0x70,
  0x41, 0x47, 0x4d, 0x41, 0x62, 0x77, 0x42, 0x74, 0x41, 0x47, 0x38, 0x41,
  0x62, 0x77, 0x42, 0x75, 0x41, 0x45, 0x59, 0x41, 0x62, 0x77, 0x42, 0x75,
  0x41, 0x48, 0x51, 0x41, 0x49, 0x41, 0x42, 0x6e, 0x41, 0x47, 0x55, 0x41,
  0x62, 0x67, 0x42, 0x6c, 0x41, 0x48, 0x49, 0x41, 0x59, 0x51, 0x42, 0x30,
  0x41, 0x47, 0x55, 0x41, 0x5a, 0x41, 0x41, 0x67, 0x41, 0x47, 0x49, 0x41,
  0x65, 0x51, 0x41, 0x67, 0x41, 0x45, 0x6b, 0x41, 0x59, 0x77, 0x42, 0x76,
  0x41, 0x45, 0x30, 0x41, 0x62, 0x77, 0x42, 0x76, 0x41, 0x47, 0x34, 0x41,
  0x4c, 0x67, 0x41, 0x41, 0x41, 0x41, 0x41, 0x44, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
  0x0a
};

static void
print_html_header (FILE * fp, char *now)
{
  fprintf (fp, "<!DOCTYPE html>\n");
  fprintf (fp, "<html lang=\"en\"><head>\n");
  fprintf (fp, "<title>Server Statistics - %s</title>\n", now);
  fprintf (fp, "<meta charset=\"UTF-8\" />");
  fprintf (fp, "<meta name=\"robots\" content=\"noindex, nofollow\" />");

  fprintf (fp, "<script type=\"text/javascript\">\n");
  fprintf (fp, "function t(c){for(var b=c.parentNode.parentNode.parentNode");
  fprintf (fp, ".parentNode.getElementsByTagName('tr'),a=0;a<b.length;a++)");
  fprintf (fp, "b[a].classList.contains('hide')?(b[a].classList.add('show'),");
  fprintf (fp, "b[a].classList.remove('hide'),c.classList.remove('icon-expand'),");
  fprintf (fp, "c.classList.add('icon-compress')):b[a].classList.contains('show')&&");
  fprintf (fp, "(b[a].classList.add('hide'),b[a].classList.remove('show'),");
  fprintf (fp, "c.classList.remove('icon-compress'),c.classList.add('icon-expand'))};");


  fprintf (fp, "</script>\n");

  fprintf (fp, "<style type=\"text/css\">");
  fprintf (fp,
  "html {"
  "    font-size: 100%%;"
  "    -ms-text-size-adjust: 100%%;"
  "    -webkit-text-size-adjust: 100%%;"
  "}"
  "html {"
  "    font-family: sans-serif"
  "}"
  "body {"
  "    font-size: 80%%;"
  "    color: #777;"
  "    margin: 0;"
  "}"
  "a:focus {"
  "    outline: thin dotted"
  "}"
  "a:active,"
  "a:hover {"
  "    outline: 0"
  "}"
  "p {"
  "    margin: 0 0 1em 0"
  "}"
  "ul {"
  "    margin: 1em 0"
  "}"
  "ul {"
  "    padding: 0 0 0 40px"
  "}"
  "table {"
  "    border-collapse: collapse;"
  "    border-spacing: 0;"
  "}"
  "h2 {"
  "    font-weight: 700;"
  "    color: #4b4b4b;"
  "    font-size: 1.2em;"
  "    margin: .83em 0 .20em 0;"
  "}"
  ".agent-hide,"
  ".hide {"
  "    display: none"
  "}"
  ".r,"
  ".s {"
  "    cursor: pointer"
  "}"
  ".r {"
  "    float: right"
  "}"
  "thead th {"
  "    text-align: center"
  "}"
  ".max {"
  "    color: #D20B2C;"
  "    font-weight: 700;"
  "}"
  ".fr {"
  "    width:100%%;"
  "    text-align:right;"
  "}"
  "#layout {"
  "    padding-left: 225px;"
  "    left: 0;"
  "}"
  ".l-box {"
  "    padding: 0 1.3em 1.3em 1.3em"
  "}"
  ".graph {"
  "    height: 1.529411765em;"
  "    margin-bottom: .470588235em;"
  "    overflow: hidden;"
  "    background-color: #e5e5e5;"
  "    border-radius: .071428571em;"
  "    text-align: center;"
  "}"
  ".graph .bar {"
  "    -moz-box-sizing: border-box;"
  "    -webkit-box-sizing: border-box;"
  "    background-color: #777;"
  "    border: 1px solid #FFF;"
  "    box-sizing: border-box;"
  "    color: #fff;"
  "    float: left;"
  "    height: 100%%;"
  "    outline: 1px solid #777;"
  "    width: 0;"
  "}"
  ".graph .light {"
  "    background-color: #BBB"
  "}"
  "#menu {"
  "    -webkit-overflow-scroll: touch;"
  "    -webkit-transition: left 0.75s, -webkit-transform 0.75s;"
  "    background: #242424;"
  "    border-right: 1px solid #3E444C;"
  "    bottom: 0;"
  "    left: 225px;"
  "    margin-left: -225px;"
  "    outline: 1px solid #101214;"
  "    overflow-y: auto;"
  "    position: fixed;"
  "    text-shadow: 0px -1px 0px #000;"
  "    top: 0;"
  "    transition: left 0.75s, -webkit-transform 0.75s, transform 0.75s;"
  "    width: 225px;"
  "    z-index: 1000;"
  "}"
  "#menu a {"
  "    border: 0;"
  "    border-bottom: 1px solid #111;"
  "    box-shadow: 0 1px 0 #383838;"
  "    color: #999;"
  "    padding: .6em 0 .6em .6em;"
  "    white-space: normal;"
  "}"
  "#menu p {"
  "    color: #eee;"
  "    padding: .6em;"
  "    font-size: 85%%;"
  "}"
  "#menu .pure-menu-open {"
  "    background: transparent;"
  "    border: 0;"
  "}"
  "#menu .pure-menu ul {"
  "    border: 0;"
  "    background: transparent;"
  "}"
  "#menu .pure-menu li a:hover,"
  "#menu .pure-menu li a:focus {"
  "    background: #333"
  "}"
  "#menu .pure-menu-heading:hover,"
  "#menu .pure-menu-heading:focus {"
  "    color: #999"
  "}"
  "#menu .pure-menu-heading {"
  "    color: #FFF;"
  "    font-size: 110%%;"
  "    font-weight: bold;"
  "}"
  ".pure-u {"
  "    display: inline-block;"
  "    *display: inline;"
  "    zoom: 1;"
  "    letter-spacing: normal;"
  "    word-spacing: normal;"
  "    vertical-align: top;"
  "    text-rendering: auto;"
  "}"
  ".pure-u-1 {"
  "    display: inline-block;"
  "    *display: inline;"
  "    zoom: 1;"
  "    letter-spacing: normal;"
  "    word-spacing: normal;"
  "    vertical-align: top;"
  "    text-rendering: auto;"
  "}"
  ".pure-u-1 {"
  "    width: 100%%"
  "}"
  ".pure-g-r {"
  "    letter-spacing: -.31em;"
  "    *letter-spacing: normal;"
  "    *word-spacing: -.43em;"
  "    font-family: sans-serif;"
  "    display: -webkit-flex;"
  "    -webkit-flex-flow: row wrap;"
  "    display: -ms-flexbox;"
  "    -ms-flex-flow: row wrap;"
  "}"
  ".pure-g-r {"
  "    word-spacing: -.43em"
  "}"
  ".pure-g-r [class *=pure-u] {"
  "    font-family: sans-serif"
  "}"
  "@media (max-width:480px) { "
  "    .pure-g-r>.pure-u,"
  "    .pure-g-r>[class *=pure-u-] {"
  "        width: 100%%"
  "    }"
  "}"
  "@media (max-width:767px) { "
  "    .pure-g-r>.pure-u,"
  "    .pure-g-r>[class *=pure-u-] {"
  "        width: 100%%"
  "    }"
  "}"
  ".pure-menu ul {"
  "    position: absolute;"
  "    visibility: hidden;"
  "}"
  ".pure-menu.pure-menu-open {"
  "    visibility: visible;"
  "    z-index: 2;"
  "    width: 100%%;"
  "}"
  ".pure-menu ul {"
  "    left: -10000px;"
  "    list-style: none;"
  "    margin: 0;"
  "    padding: 0;"
  "    top: -10000px;"
  "    z-index: 1;"
  "}"
  ".pure-menu>ul {"
  "    position: relative"
  "}"
  ".pure-menu-open>ul {"
  "    left: 0;"
  "    top: 0;"
  "    visibility: visible;"
  "}"
  ".pure-menu-open>ul:focus {"
  "    outline: 0"
  "}"
  ".pure-menu li {"
  "    position: relative"
  "}"
  ".pure-menu a,"
  ".pure-menu .pure-menu-heading {"
  "    display: block;"
  "    color: inherit;"
  "    line-height: 1.5em;"
  "    padding: 5px 20px;"
  "    text-decoration: none;"
  "    white-space: nowrap;"
  "}"
  ".pure-menu li a {"
  "    padding: 5px 20px"
  "}"
  ".pure-menu.pure-menu-open {"
  "    background: #fff;"
  "    border: 1px solid #b7b7b7;"
  "}"
  ".pure-menu a {"
  "    border: 1px solid transparent;"
  "    border-left: 0;"
  "    border-right: 0;"
  "}"
  ".pure-menu a {"
  "    color: #777"
  "}"
  ".pure-menu li a:hover,"
  ".pure-menu li a:focus {"
  "    background: #eee"
  "}"
  ".pure-menu .pure-menu-heading {"
  "    color: #565d64;"
  "    font-size: 90%%;"
  "    margin-top: .5em;"
  "    border-bottom-width: 1px;"
  "    border-bottom-style: solid;"
  "    border-bottom-color: #dfdfdf;"
  "}"
  ".pure-table {"
  "    animation: float 5s infinite;"
  "    border: 1px solid #cbcbcb;"
  "    border-collapse: collapse;"
  "    border-spacing: 0;"
  "    box-shadow: 0 5px 10px rgba(0, 0, 0, 0.1);"
  "    empty-cells: show;"
  "    border-radius:3px;"
  "}"
  ".pure-table td,"
  ".pure-table th {"
  "    border-left: 1px solid #cbcbcb;"
  "    border-width: 0 0 0 1px;"
  "    font-size: inherit;"
  "    margin: 0;"
  "    overflow: visible;"
  "    padding: 6px 12px;"
  "}"
  ".pure-table th:last-child {"
  "    padding-right: 0;"
  "}"
  ".pure-table th:last-child span {"
  "    margin: 1px 5px 0 15px;"
  "}"
  ".pure-table th {"
  "    border-bottom:4px solid #9ea7af;"
  "    border-right: 1px solid #343a45;"
  "}"
  ".pure-table td:first-child,"
  ".pure-table th:first-child {"
  "    border-left-width: 0"
  "}"
  ".pure-table td:last-child {"
  "    white-space: normal;"
  "    width: auto;"
  "    word-break: break-all;"
  "    word-wrap: break-word;"
  "}"
  ".pure-table thead {"
  "    background: #242424;"
  "    color: #FFF;"
  "    text-align: left;"
  "    text-shadow: 0px -1px 0px #000;"
  "    vertical-align: bottom;"
  "}"
  ".pure-table td {"
  "    background-color: #FFF"
  "}"
  ".pure-table td.number {"
  "    text-align: right"
  "}"
  ".pure-table .sub td {"
  "    background-color: #F2F2F2;"
  "}"
  ".pure-table tbody tr:hover,"
  ".pure-table-striped tr:nth-child(2n-1) td {"
  "    background-color: #f4f4f4"
  "}"
  ".pure-table tr {"
  "    border-bottom: 1px solid #ddd;"
  "}"
  "@font-face {"
  "    font-family: 'icomoon';"
  "    src: url(data:application/font-woff;charset=utf-8;base64,%s) format('woff');"
  "    font-weight: normal;"
  "    font-style: normal;"
  "}"
  "[class^=\"icon-\"], [class*=\" icon-\"] {"
  "    font-family: 'icomoon';"
  "    speak: none;"
  "    font-style: normal;"
  "    font-weight: normal;"
  "    font-variant: normal;"
  "    text-transform: none;"
  "    line-height: 1;"
  "    -webkit-font-smoothing: antialiased;"
  "    -moz-osx-font-smoothing: grayscale;"
  "}"
  ".icon-expand:before {"
  "    content: '\\f065';"
  "}"
  ".icon-compress:before {"
  "    content: '\\f066';"
  "}"
  "@media (max-width: 974px) {"
  "    #layout {"
  "        position: relative;"
  "        padding-left: 0;"
  "    }"
  "    #layout.active {"
  "        position: relative;"
  "        left: 200px;"
  "    }"
  "    #layout.active #menu {"
  "        left: 200px;"
  "        width: 200px;"
  "    }"
  "    #menu {"
  "        left: 0"
  "    }"
  "    .pure-menu-link {"
  "        position: fixed;"
  "        left: 0;"
  "        display: block;"
  "    }"
  "    #layout.active .pure-menu-link {"
  "        left: 200px"
  "    }"
  "}", icons);

  fprintf (fp, "</style>\n");
  fprintf (fp, "</head>\n");
  fprintf (fp, "<body>\n");

  fprintf (fp, "<div class=\"pure-g-r\" id=\"layout\">");
}

/* *INDENT-ON* */

static GOutput *
panel_lookup (GModule module)
{
  int i, num_panels = ARRAY_SIZE (paneling);

  for (i = 0; i < num_panels; i++) {
    if (paneling[i].module == module)
      return &paneling[i];
  }
  return NULL;
}

static int
get_max_hit (GHolder * h)
{
  int i, max = 0;
  for (i = 0; i < h->idx; i++) {
    int cur = h->items[i].metrics->hits;
    if (cur > max)
      max = cur;
  }

  return max;
}

/* sanitize output with html entities for special chars */
static void
clean_output (FILE * fp, char *s)
{
  while (*s) {
    switch (*s) {
    case '\'':
      fprintf (fp, "&#39;");
      break;
    case '"':
      fprintf (fp, "&#34;");
      break;
    case '&':
      fprintf (fp, "&amp;");
      break;
    case '<':
      fprintf (fp, "&lt;");
      break;
    case '>':
      fprintf (fp, "&gt;");
      break;
    case ' ':
      fprintf (fp, "&nbsp;");
      break;
    default:
      fputc (*s, fp);
      break;
    }
    s++;
  }
}


static void
print_pure_menu (FILE * fp, char *now)
{
  const char *img = ""
    "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"
    "8H8BM/XVggoDbGIAAAAASUVORK5CYII=";

  fprintf (fp, "<div id=\"menu\" class=\"pure-u\">");
  fprintf (fp, "<div class=\"pure-menu pure-menu-open\">");
  fprintf (fp, "<a class=\"pure-menu-heading\" href=\"%s\">", GO_WEBSITE);
  fprintf (fp, "<img src='data:image/png;base64,%s'/>", img);
  fprintf (fp, "</a>");
  fprintf (fp, "<ul>");
  fprintf (fp, "<li><a href=\"#\">Overall</a></li>");
  fprintf (fp, "<li><a href=\"#%s\">Unique visitors</a></li>", VISIT_ID);
  fprintf (fp, "<li><a href=\"#%s\">Requested files</a></li>", REQUE_ID);
  fprintf (fp, "<li><a href=\"#%s\">Requested static files</a></li>", STATI_ID);
  fprintf (fp, "<li><a href=\"#%s\">Not found URLs</a></li>", FOUND_ID);
  fprintf (fp, "<li><a href=\"#%s\">Hosts</a></li>", HOSTS_ID);
  fprintf (fp, "<li><a href=\"#%s\">Operating Systems</a></li>", OPERA_ID);
  fprintf (fp, "<li><a href=\"#%s\">Browsers</a></li>", BROWS_ID);
  fprintf (fp, "<li><a href=\"#%s\">Referrers URLs</a></li>", REFER_ID);
  fprintf (fp, "<li><a href=\"#%s\">Referring sites</a></li>", SITES_ID);
  fprintf (fp, "<li><a href=\"#%s\">Keyphrases</a></li>", KEYPH_ID);
#ifdef HAVE_LIBGEOIP
  fprintf (fp, "<li><a href=\"#%s\">Geo Location</a></li>", GEOLO_ID);
#endif
  fprintf (fp, "<li><a href=\"#%s\">Status codes</a></li>", CODES_ID);
  fprintf (fp, "<li class=\"menu-item-divided\"></li>");

  fprintf (fp, "</ul>");
  fprintf (fp, "<p>Generated by<br />GoAccess %s<br />—<br />%s</p>",
           GO_VERSION, now);
  fprintf (fp, "</div>");
  fprintf (fp, "</div> <!-- menu -->");

  fprintf (fp, "<div id=\"main\" class=\"pure-u-1\">");
  fprintf (fp, "<div class=\"l-box\">");
}

static void
print_html_footer (FILE * fp)
{
  fprintf (fp, "</div> <!-- l-box -->\n");
  fprintf (fp, "</div> <!-- main -->\n");
  fprintf (fp, "</div> <!-- layout -->\n");
  fprintf (fp, "</body>\n");
  fprintf (fp, "</html>");
}

static void
print_html_h2 (FILE * fp, const char *title, const char *id)
{
  if (id)
    fprintf (fp, "<h2 id=\"%s\">%s</h2>", id, title);
  else
    fprintf (fp, "<h2>%s</h2>", title);
}

static void
print_p (FILE * fp, const char *paragraph)
{
  fprintf (fp, "<p>%s</p>", paragraph);
}

static void
print_html_begin_table (FILE * fp)
{
  fprintf (fp, "<table class=\"pure-table\">\n");
}

static void
print_html_end_table (FILE * fp)
{
  fprintf (fp, "</table>\n");
}

static void
print_html_begin_thead (FILE * fp)
{
  fprintf (fp, "<thead>\n");
}

static void
print_html_end_thead (FILE * fp)
{
  fprintf (fp, "</thead>\n");
}

static void
print_html_begin_tbody (FILE * fp)
{
  fprintf (fp, "<tbody>\n");
}

static void
print_html_end_tbody (FILE * fp)
{
  fprintf (fp, "</tbody>\n");
}

static void
print_html_begin_tr (FILE * fp, int hide, int sub)
{
  if (hide)
    fprintf (fp, "<tr class='hide %s'>", (sub ? "sub" : "root"));
  else
    fprintf (fp, "<tr class='%s'>", (sub ? "sub" : "root"));
}

static void
print_html_end_tr (FILE * fp)
{
  fprintf (fp, "</tr>");
}

/* *indent-on* */

static void
print_graph (FILE * fp, int max, int hits)
{
  float l = get_percentage (max, hits);
  l = l < 1 ? 1 : l;

  fprintf (fp, "<td class='graph'>");
  fprintf (fp, "<div class='bar' style='width:%f%%'></div>", l);
  fprintf (fp, "</td>\n");
}

static void
print_table_head (FILE * fp, GModule module)
{
  print_html_h2 (fp, module_to_head (module), module_to_id (module));
  print_p (fp, module_to_desc (module));
}

static void
print_metric_hits (FILE * fp, GMetrics * nmetrics)
{
  fprintf (fp, "<td class='number'>%d</td>", nmetrics->hits);
}

static void
print_metric_visitors (FILE * fp, GMetrics * nmetrics)
{
  fprintf (fp, "<td class='number'>%d</td>", nmetrics->visitors);
}

static void
print_metric_percent (FILE * fp, GMetrics * nmetrics)
{
  fprintf (fp, "<td class='number'>%4.2f%%</td>", nmetrics->percent);
}

static void
print_metric_bw (FILE * fp, GMetrics * nmetrics)
{
  char *bw = filesize_str (nmetrics->bw.nbw);

  fprintf (fp, "<td class='number'>");
  clean_output (fp, bw);
  fprintf (fp, "</td>");

  free (bw);
}

static void
print_metric_avgts (FILE * fp, GMetrics * nmetrics)
{
  char *ts = NULL;
  if (!conf.serve_usecs)
    return;

  ts = usecs_to_str (nmetrics->avgts.nts);
  fprintf (fp, "<td class='number'>");
  clean_output (fp, ts);
  fprintf (fp, "</td>");

  free (ts);
}

static void
print_metric_data (FILE * fp, GMetrics * nmetrics)
{
  fprintf (fp, "<td>");
  clean_output (fp, nmetrics->data);
  fprintf (fp, "</td>");
}

static void
print_metric_protocol (FILE * fp, GMetrics * nmetrics)
{
  if (!conf.append_protocol)
    return;

  fprintf (fp, "<td>");
  clean_output (fp, nmetrics->protocol);
  fprintf (fp, "</td>");
}

static void
print_metric_method (FILE * fp, GMetrics * nmetrics)
{
  if (!conf.append_method)
    return;

  fprintf (fp, "<td>");
  clean_output (fp, nmetrics->method);
  fprintf (fp, "</td>");
}

static void
print_metrics (FILE * fp, GMetrics * nmetrics, int max, int sub,
               const GOutput * panel)
{
  if (panel->visitors)
    print_metric_visitors (fp, nmetrics);
  if (panel->hits)
    print_metric_hits (fp, nmetrics);
  if (panel->percent)
    print_metric_percent (fp, nmetrics);
  if (panel->bw)
    print_metric_bw (fp, nmetrics);
  if (panel->avgts)
    print_metric_avgts (fp, nmetrics);
  if (panel->protocol)
    print_metric_protocol (fp, nmetrics);
  if (panel->method)
    print_metric_method (fp, nmetrics);
  if (panel->data)
    print_metric_data (fp, nmetrics);

  if (panel->graph && max && !panel->sub_graph && sub)
    fprintf (fp, "<td></td>");
  else if (panel->graph && max)
    print_graph (fp, max, nmetrics->hits);
}

static void
print_html_sub_items (FILE * fp, GHolder * h, int idx, int processed, int max,
                      const GOutput * panel)
{
  GMetrics *nmetrics;
  GSubItem *iter;
  GSubList *sub_list = h->items[idx].sub_list;
  int i = 0;

  if (sub_list == NULL)
    return;

  for (iter = sub_list->head; iter; iter = iter->next, i++) {
    set_data_metrics (iter->metrics, &nmetrics, processed);

    print_html_begin_tr (fp, 1, 1);
    print_metrics (fp, nmetrics, max, 1, panel);
    print_html_end_tr (fp);

    free (nmetrics);
  }
}

static void
print_html_data (FILE * fp, GHolder * h, int processed, int max,
                 const GOutput * panel)
{
  GMetrics *nmetrics;
  int i;

  for (i = 0; i < h->idx; i++) {
    set_data_metrics (h->items[i].metrics, &nmetrics, processed);

    print_html_begin_tr (fp, (i > OUTPUT_N), 0);
    print_metrics (fp, nmetrics, max, 0, panel);
    print_html_end_tr (fp);

    if (h->sub_items_size)
      print_html_sub_items (fp, h, i, processed, max, panel);

    free (nmetrics);
  }
}

static void
print_html_visitors (FILE * fp, GHolder * h, int processed,
                     const GOutput * panel)
{
  int max = 0;

  print_table_head (fp, h->module);
  print_html_begin_table (fp);
  print_html_begin_thead (fp);

  fprintf (fp, "<tr>");
  fprintf (fp, "<th>Visitors</th>");
  fprintf (fp, "<th>Hits</th>");
  fprintf (fp, "<th>%%</th>");
  fprintf (fp, "<th>Bandwidth</th>");
  if (conf.serve_usecs)
    fprintf (fp, "<th>Time&nbsp;served</th>");
  fprintf (fp, "<th>Date</th>");
  fprintf (fp,
           "<th class='fr'>&nbsp;<span class='r icon-expand' onclick='t(this)'></span></th>");
  fprintf (fp, "</tr>");

  print_html_end_thead (fp);
  print_html_begin_tbody (fp);

  max = get_max_hit (h);
  print_html_data (fp, h, processed, max, panel);

  print_html_end_tbody (fp);
  print_html_end_table (fp);
}

static void
print_html_requests (FILE * fp, GHolder * h, int processed,
                     const GOutput * panel)
{
  print_table_head (fp, h->module);
  print_html_begin_table (fp);
  print_html_begin_thead (fp);

  fprintf (fp, "<tr>");
  fprintf (fp, "<th>Visitors</th>");
  fprintf (fp, "<th>Hits</th>");
  fprintf (fp, "<th>%%</th>");
  fprintf (fp, "<th>Bandwidth</th>");
  if (conf.serve_usecs)
    fprintf (fp, "<th>Time&nbsp;served</th>");
  if (conf.append_protocol)
    fprintf (fp, "<th>Protocol</th>");
  if (conf.append_method)
    fprintf (fp, "<th>Method</th>");
  fprintf (fp, "<th>");
  fprintf (fp, "Request <span class='r icon-expand' onclick='t(this)'></span>");
  fprintf (fp, "</th>");
  fprintf (fp, "</tr>");

  print_html_end_thead (fp);
  print_html_begin_tbody (fp);

  print_html_data (fp, h, processed, 0, panel);

  print_html_end_tbody (fp);
  print_html_end_table (fp);
}

static void
print_html_common (FILE * fp, GHolder * h, int processed, const GOutput * panel)
{
  int max = 0;
  const char *lbl = module_to_label (h->module);

  if (panel->graph)
    max = get_max_hit (h);

  print_table_head (fp, h->module);
  print_html_begin_table (fp);
  print_html_begin_thead (fp);

  fprintf (fp, "<tr>");
  fprintf (fp, "<th>Visitors</th>");
  fprintf (fp, "<th>Hits</th>");
  fprintf (fp, "<th>%%</th>");
  fprintf (fp, "<th>Bandwidth</th>");
  if (conf.serve_usecs)
    fprintf (fp, "<th>Time&nbsp;served</th>");
  if (max)
    fprintf (fp, "<th>%s</th>", lbl);
  fprintf (fp, "<th class='%s'>%s", (max ? "fr" : ""), (max ? "" : lbl));
  fprintf (fp, "<span class='r icon-expand' onclick='t(this)'>&#8199;</span>");
  fprintf (fp, "</th>");
  fprintf (fp, "</tr>");

  print_html_end_thead (fp);
  print_html_begin_tbody (fp);

  print_html_data (fp, h, processed, max, panel);

  print_html_end_tbody (fp);
  print_html_end_table (fp);
}

/* entry point to generate a report writing it to the fp */
void
output_html (GLog * logger, GHolder * holder)
{
  GModule module;
  char now[DATE_TIME];
  FILE *fp = stdout;

  generate_time ();
  strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S", now_tm);

  print_html_header (fp, now);
  print_pure_menu (fp, now);

  for (module = 0; module < TOTAL_MODULES; module++) {
    const GOutput *panel = panel_lookup (module);
    if (!panel)
      continue;
    panel->render (fp, holder + module, logger->process, panel);
  }

  print_html_footer (fp);

  fclose (fp);
}
