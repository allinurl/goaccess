/**
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
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

#include <glib.h>
#include <curses.h>

#ifndef COMMONS_H_INCLUDED
#define COMMONS_H_INCLUDED

/* Remove the __attribute__ stuff when the compiler is not GCC. */
#if !__GNUC__
# define __attribute__(x) /**/
#endif
#define GO_UNUSED __attribute__((unused))
#define DEBUG
#define BUFFER 			4096
#define MAX_CHOICES 	100
#define TOTAL_MODULES 	11
#define GO_VERSION 		"0.3"
#define MIN_HEIGHT 		40
#define MIN_WIDTH       97
/* max height of header window (rows) */
#define MAX_HEIGHT_HEADER 6
/* max height of footer stdscr (rows) */
#define MAX_HEIGHT_FOOTER 1
#define KB (1024)
#define MB (KB * 1024)
#define GB (MB * 1024)
#define COL_WHITE    0
#define COL_BLUE     1
#define COL_GREEN    11
#define COL_RED      3
#define COL_BLACK    4
#define COL_CYAN     5
#define COL_YELLOW   6
#define BLUE_GREEN   7
#define BLACK_GREEN  8
#define BLACK_CYAN   9
#define WHITE_RED    10
   enum MODULES
{
   UNIQUE_VISITORS = 1,
   REQUESTS,
   REQUESTS_STATIC,
   REFERRERS,
   NOT_FOUND,
   OS,
   BROWSERS,
   HOSTS,
   STATUS_CODES,
   REFERRING_SITES,
   KEYPHRASES
};

extern enum MODULES modules;

/* to create a new hash value out of a key, which can be NULL */
extern GHashTable *ht_unique_visitors;
extern GHashTable *ht_requests;
extern GHashTable *ht_referrers;
extern GHashTable *ht_unique_vis;
extern GHashTable *ht_requests_static;
extern GHashTable *ht_not_found_requests;
extern GHashTable *ht_os;
extern GHashTable *ht_browsers;
extern GHashTable *ht_hosts;
extern GHashTable *ht_status_code;
extern GHashTable *ht_referring_sites;
extern GHashTable *ht_keyphrases;
extern GHashTable *ht_file_bw;

struct logger
{
   char *host;
   char *identd;
   char *userid;
   char *hour;
   char *date;
   char *request;
   char *status;
   char *referrer;
   char *agent;
   long long resp_size;
   int total_process;
   int total_invalid;
   int counter;
   int alloc_counter;
   int current_module;
   int max_value;
};

struct struct_display
{
   char *data;
   int hits;
   int module;
};

struct struct_holder
{
   char *data;
   int hits;
};

struct scrolling
{
   size_t scrl_main_win;
   size_t scrl_help_win;
   size_t init_scrl_main_win;
};

struct tm *now_tm;

/* Declaration of variables */
extern int bandwidth_flag;
extern int ignore_flag;
extern int http_status_code_flag;
extern long long req_size;
extern char *req;
extern char *status_code;
extern char *ignore_host;

extern time_t now;
extern time_t start_proc;
extern time_t end_proc;

extern size_t term_h;
extern size_t term_w;
extern size_t real_size_y;

extern char *ifile;

extern char *codes[][2];
extern char *module_names[];
extern char *os[][2];
extern char *browsers[][2];
extern char *help_main[];

void init_colors (void);
size_t os_size (void);
size_t browsers_size (void);
size_t codes_size (void);
size_t help_main_size (void);

#endif
