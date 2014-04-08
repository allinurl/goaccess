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

#ifndef GLIBHT_H_INCLUDED
#define GLIBHT_H_INCLUDED

#ifdef HAVE_LIBGLIB_2_0
#include <glib.h>
#endif

#include "parser.h"

extern GHashTable *ht_browsers;
extern GHashTable *ht_countries;
extern GHashTable *ht_date_bw;
extern GHashTable *ht_file_bw;
extern GHashTable *ht_file_serve_usecs;
extern GHashTable *ht_host_bw;
extern GHashTable *ht_hostnames;
extern GHashTable *ht_hosts;
extern GHashTable *ht_hosts_agents;
extern GHashTable *ht_host_serve_usecs;
extern GHashTable *ht_keyphrases;
extern GHashTable *ht_monthly;
extern GHashTable *ht_not_found_requests;
extern GHashTable *ht_os;
extern GHashTable *ht_referrers;
extern GHashTable *ht_referring_sites;
extern GHashTable *ht_requests;
extern GHashTable *ht_requests_static;
extern GHashTable *ht_status_code;
extern GHashTable *ht_unique_vis;
extern GHashTable *ht_unique_visitors;

GHashTable *get_ht_by_module (GModule module);
GRawData *parse_raw_data (GHashTable * ht, int ht_size, GModule module);
int process_browser (GHashTable * ht, const char *key,
                     const char *browser_type);
int process_generic_data (GHashTable * ht, const char *key);
int process_geolocation (GHashTable * ht, const char *cntry,
                         const char *cont);
int process_host_agents (char *host, char *agent);
int process_opesys (GHashTable * ht, const char *key, const char *os_type);
int process_request (GHashTable * ht, const char *key, const GLogItem * glog);
int process_request_meta (GHashTable * ht, char *key, uint64_t size);
uint32_t get_ht_size (GHashTable * ht);
uint64_t get_bandwidth (const char *k, GModule module);
uint64_t get_serve_time (const char *key, GModule module);
void free_browser (GO_UNUSED gpointer old_key, gpointer old_value,
                   GO_UNUSED gpointer user_data);
void free_countries (GO_UNUSED gpointer old_key, gpointer old_value,
                     GO_UNUSED gpointer user_data);
void free_key_value (gpointer old_key, GO_UNUSED gpointer old_value,
                     GO_UNUSED gpointer user_data);
void free_os (GO_UNUSED gpointer old_key, gpointer old_value,
              GO_UNUSED gpointer user_data);
void free_requests (GO_UNUSED gpointer old_key, gpointer old_value,
                    GO_UNUSED gpointer user_data);

#endif
