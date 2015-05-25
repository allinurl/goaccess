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

#ifndef TCBTDB_H_INCLUDED
#define TCBTDB_H_INCLUDED

#include <tcbdb.h>

#include "commons.h"
#include "gstorage.h"
#include "parser.h"

#define TC_MMAP  0
#define TC_LCNUM 1024
#define TC_NCNUM 512
#define TC_LMEMB 128
#define TC_NMEMB 256
#define TC_BNUM  32749
#define TC_DBPATH "/tmp/"
#define TC_ZLIB 1
#define TC_BZ2  2

/* B+ Tree - on-disk databases */
#define DB_AGENT_KEYS  "db_agent_keys.tcb"
#define DB_AGENT_VALS  "db_agent_vals.tcb"
#define DB_GEN_STATS   "db_gen_stats.tcb"
#define DB_HOSTNAMES   "db_hostnames.tcb"
#define DB_UNIQUE_KEYS "db_unique_keys.tcb"

#define DB_KEYMAP    "db_keymap.tcb"
#define DB_DATAMAP   "db_datamap.tcb"
#define DB_ROOTMAP   "db_rootmap.tcb"
#define DB_UNIQMAP   "db_uniqmap.tcb"
#define DB_VISITORS  "db_visitors.tcb"
#define DB_HITS      "db_hits.tcb"
#define DB_BW        "db_bw.tcb"
#define DB_AVGTS     "db_avgts.tcb"
#define DB_METHODS   "db_methods.tcb"
#define DB_PROTOCOLS "db_protocols.tcb"
#define DB_AGENTS    "db_agents.tcb"

/* *INDENT-OFF* */
TCBDB * tc_bdb_create (const char *dbname, int module);

char * tc_db_set_path (const char *dbname, int module);
int tc_bdb_close (void *db, char *dbname);
void tc_db_get_params (char *params, const char *path);

#ifdef TCB_BTREE
int ht_insert_host_agent (TCBDB * bdb, int data_nkey, int agent_nkey);
#endif
/* *INDENT-ON* */

#endif
