/**
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef TCABINET_H_INCLUDED
#define TCABINET_H_INCLUDED

#include <tcbdb.h>

#include "commons.h"
#include "parser.h"

#define TC_LCNUM 9000
#define TC_NCNUM 3000
#define TC_LMEMB 128
#define TC_NMEMB 256
#define TC_BNUM 32749

#define DB_BROWSERS "/tmp/db_browsers.tcb"
#define DB_COUNTRIES "/tmp/db_countries.tcb"
#define DB_DATE_BW "/tmp/db_date_bw.tcb"
#define DB_FILE_BW "/tmp/db_file_bw.tcb"
#define DB_FILE_SERVE_USECS "/tmp/db_file_serve_usecs.tcb"
#define DB_HOST_AGENTS "/tmp/db_host_agents.tcb"
#define DB_HOST_BW "/tmp/db_host_bw.tcb"
#define DB_HOSTNAMES "/tmp/db_hostnames.tcb"
#define DB_HOSTS "/tmp/db_hosts.tcb"
#define DB_HOST_SERVE_USECS "/tmp/db_host_serve_usecs.tcb"
#define DB_KEYPHRASES "/tmp/db_keyphrases.tcb"
#define DB_NOT_FOUND_REQUESTS "/tmp/db_not_found_requests.tcb"
#define DB_OS "/tmp/db_os.tcb"
#define DB_REFERRERS "/tmp/db_referrers.tcb"
#define DB_REFERRING_SITES "/tmp/db_referring_sites.tcb"
#define DB_REQUESTS "/tmp/db_requests.tcb"
#define DB_REQUESTS_STATIC "/tmp/db_requests_static.tcb"
#define DB_STATUS_CODE "/tmp/db_status_code.tcb"
#define DB_UNIQUE_VIS "/tmp/db_unique_vis.tcb"
#define DB_UNIQUE_VISITORS "/tmp/db_unique_visitors.tcb"

extern TCBDB *ht_browsers;
extern TCBDB *ht_countries;
extern TCBDB *ht_date_bw;
extern TCBDB *ht_file_bw;
extern TCBDB *ht_file_serve_usecs;
extern TCBDB *ht_host_bw;
extern TCBDB *ht_hostnames;
extern TCBDB *ht_hosts;
extern TCBDB *ht_host_serve_usecs;
extern TCBDB *ht_keyphrases;
extern TCBDB *ht_not_found_requests;
extern TCBDB *ht_os;
extern TCBDB *ht_hosts_agents;
extern TCBDB *ht_referrers;
extern TCBDB *ht_referring_sites;
extern TCBDB *ht_requests;
extern TCBDB *ht_requests_static;
extern TCBDB *ht_status_code;
extern TCBDB *ht_unique_vis;
extern TCBDB *ht_unique_visitors;

#define INT_TO_POINTER(i) ((void *) (long) (i))

GRawData *parse_raw_data (TCBDB * bdb, int ht_size, GModule module);
int process_browser (TCBDB * bdb, const char *k, const char *browser_type);
int process_generic_data (TCBDB * bdb, const char *k);
int process_geolocation (TCBDB * bdb, const char *cntry, const char *cont);
int process_opesys (TCBDB * bdb, const char *k, const char *os_type);
int process_request_meta (TCBDB * bdb, const char *k, uint64_t size);
int process_request (TCBDB * bdb, const char *k, const GLogItem * log);
int process_host_agents (char *host, char *agent);
int tc_db_close (TCBDB * bdb, const char *dbname);
TCBDB *get_ht_by_module (GModule module);
TCBDB *tc_db_create (const char *dbname);
uint64_t get_bandwidth (char *k, GModule module);
uint64_t get_serve_time (const char *k, GModule module);
unsigned int get_ht_size (TCBDB * bdb);
void free_key (BDBCUR * cur, char *key, GO_UNUSED int ksize,
               GO_UNUSED void *user_data);
void free_requests (BDBCUR * cur, char *key, GO_UNUSED int ksize,
                    GO_UNUSED void *user_data);
void tc_db_foreach (TCBDB * bdb,
                    void (*fp) (BDBCUR * cur, char *k, int s, void *u),
                    void *user_data);

#endif
