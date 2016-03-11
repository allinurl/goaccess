/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2016 Gerardo Orellana <hello @ goaccess.io>
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
#define DB_ROOT      "db_root.tcb"
#define DB_HITS      "db_hits.tcb"
#define DB_BW        "db_bw.tcb"
#define DB_CUMTS     "db_cumts.tcb"
#define DB_MAXTS     "db_maxts.tcb"
#define DB_METHODS   "db_methods.tcb"
#define DB_PROTOCOLS "db_protocols.tcb"
#define DB_AGENTS    "db_agents.tcb"
#define DB_METADATA  "db_metadata.tcb"

/* *INDENT-OFF* */
TCBDB *tc_bdb_create (const char *dbname, int module);

char *tc_db_set_path (const char *dbname, int module);
int tc_bdb_close (void *db, char *dbname);
void tc_db_get_params (char *params, const char *path);

#ifdef TCB_BTREE
int ins_igsl (void *hash, int key, int value);
#endif
/* *INDENT-ON* */

#endif
