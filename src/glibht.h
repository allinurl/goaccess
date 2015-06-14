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

#include <stdint.h>
#include "parser.h"
#include "gstorage.h"

/* tables for the whole app */
extern GHashTable *ht_agent_keys;
extern GHashTable *ht_agent_vals;
extern GHashTable *ht_hostnames;
extern GHashTable *ht_unique_keys;

/* *INDENT-OFF* */
GRawData *parse_raw_data (GHashTable * ht, int ht_size, GModule module);

uint32_t get_ht_size_by_metric (GModule module, GMetric metric);
uint32_t get_ht_size (GHashTable * ht);

int ht_inc_int_from_int_key (GHashTable * ht, int data_nkey, int inc);
int ht_inc_int_from_str_key (GHashTable * ht, char *key, int inc);
int ht_inc_u64_from_int_key (GHashTable * ht, int data_nkey, uint64_t inc);
int ht_insert_agent(const char *key);
int ht_insert_hit (GHashTable * ht, int data_nkey, int uniq_nkey, int root_nkey);
int ht_insert_keymap(GHashTable *ht,  const char *value);
int ht_insert_nkey_nval (GHashTable * ht, int nkey, int nval);
int ht_insert_str_from_int_key (GHashTable * ht, int nkey, const char *value);
int ht_insert_uniqmap (GHashTable * ht, char *uniq_key);
int ht_insert_unique_key (const char *key);
int ht_insert_host_agent (GHashTable * ht, int data_nkey, int agent_nkey);
int ht_insert_agent_key (const char *key);
int ht_insert_agent_val (int nkey, const char *key);

char *get_host_agent_val (int agent_nkey);
char *get_hostname (const char *host);
char *get_node_from_key (int data_nkey, GModule module, GMetric metric);
char *get_root_from_key (int root_nkey, GModule module);
char * get_str_from_int_key (GHashTable *ht, int nkey);
int get_int_from_keymap (const char *key, GModule module);
int get_int_from_str_key (GHashTable * ht, const char *key);
int get_num_from_key (int data_nkey, GModule module, GMetric metric);
int process_host_agents (char *host, char *agent);
uint64_t get_cumulative_from_key (int data_nkey, GModule module, GMetric metric);
unsigned int get_uint_from_str_key (GHashTable *ht, const char *key);
void free_storage (void);
void *get_host_agent_list (int data_nkey);
void init_storage (void);

void free_agent_list (void);

/* *INDENT-ON* */

#endif
