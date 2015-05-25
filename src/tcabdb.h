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

#ifndef TCABDB_H_INCLUDED
#define TCABDB_H_INCLUDED

#include <tcadb.h>

#include "commons.h"
#include "gstorage.h"
#include "parser.h"

#define DB_PARAMS 256

/* tables for the whole app */
extern TCADB *ht_agent_keys;
extern TCADB *ht_agent_vals;
extern TCADB *ht_general_stats;
extern TCADB *ht_hostnames;
extern TCADB *ht_unique_keys;

/* *INDENT-OFF* */
GRawData *parse_raw_data (void *db, int ht_size, GModule module);

uint32_t get_ht_size_by_metric (GModule module, GMetric metric);
uint32_t get_ht_size (TCADB *adb);

int agent_list_to_store(void);
int find_host_agent_in_list (void *data, void *needle);
int ht_inc_int_from_int_key (TCADB * adb, int data_nkey, int inc);
int ht_inc_int_from_str_key (TCADB * adb, const char *key, int inc);
int ht_inc_u64_from_int_key (TCADB * adb, int data_nkey, uint64_t inc);
int ht_inc_u64_from_str_key (TCADB * adb, const char *key, uint64_t inc);
int ht_insert_agent_key (const char *key);
int ht_insert_agent_val(int nkey, const char *key);
int ht_insert_hit (TCADB *adb, int data_nkey, int uniq_nkey, int root_nkey);
int ht_insert_keymap (TCADB * adb, const char *value);
int ht_insert_nkey_nval (TCADB * adb, int nkey, int nval);
int ht_insert_str_from_int_key (TCADB *adb, int nkey, const char *value);
int ht_insert_uniqmap (TCADB *adb, char *uniq_key);
int ht_insert_unique_key (const char *key);

char *get_host_agent_val (int agent_nkey);
char *get_hostname (const char *host);
char *get_node_from_key (int data_nkey, GModule module, GMetric metric);
char *get_root_from_key (int root_nkey, GModule module);
char *get_str_from_int_key (TCADB *adb, int nkey);
int get_int_from_keymap (const char * key, GModule module);
int get_int_from_str_key (TCADB *adb, const char *key);
int get_num_from_key (int data_nkey, GModule module, GMetric metric);
uint64_t get_cumulative_from_key (int data_nkey, GModule module, GMetric metric);
unsigned int get_uint_from_str_key (TCADB * adb, const char *key);
void *get_host_agent_list(int agent_nkey);

void free_agent_list(void);
void free_db_key (TCADB *adb);
void free_storage (void);
void init_storage (void);

#ifdef TCB_MEMHASH
int ht_insert_host_agent (TCADB * adb, int data_nkey, int agent_nkey);
#endif
GSLList * tclist_to_gsllist (TCLIST * tclist);

/* *INDENT-ON* */

#endif
