/**
 * tcbtdb.c -- Tokyo Cabinet database functions
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

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <tcutil.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "tcbtdb.h"
#include "tcabdb.h"

#ifdef HAVE_GEOLOCATION
#include "geoip1.h"
#endif

#include "error.h"
#include "util.h"
#include "xmalloc.h"

#ifdef TCB_BTREE

/* Get the on-disk databases path.
 *
 * On success, the databases path string is returned. */
char *
tc_db_set_path (const char *dbname, int module)
{
  struct stat info;
  static char default_path[ sizeof(TC_DBPATH "01234567890/") ];
  static char *db_path;
  char *path;
  char fname[RAND_FN];
  int cx;

  /* db_path is either specified explicitly, or gets the default (pid appended) */
  if (conf.db_path != NULL)
    db_path = (char *)conf.db_path;
  else {
    sprintf(default_path, "%s%d", TC_DBPATH, getpid());
    db_path = default_path;
    mkdir(db_path, TC_DBPMODE);
  }

  /* sanity check: Is db_path accessible and a directory? */
  if (stat (db_path, &info) != 0)
    FATAL ("Unable to access database path: %s", strerror (errno));
  if (!(info.st_mode & S_IFDIR))
    FATAL ("Database path is not a directory.");

  /* for tc_db_rmdir(), return the pure folder path (or NULL to keep) */
  if (dbname == NULL)
    return conf.db_path == NULL ? db_path : NULL;

  memset (fname, 0, sizeof (fname));
  /* tcadbopen requires the db name suffix to be ".tcb" and thus we
   * don't use mkstemp(3) */
  genstr (fname, RAND_FN - 1);

  cx = snprintf (NULL, 0, "%s/%dm%s%s", db_path, module, fname, dbname) + 1;
  path = xmalloc (cx);
  sprintf (path, "%s/%dm%s%s", db_path, module, fname, dbname);

  return path;
}

/* delete db folder if we used the customized (pid appended) default */
void
tc_db_rmdir()
{
  const char *db_path = NULL;

  db_path = tc_db_set_path (NULL, 0);
  if (db_path != NULL)
    if (rmdir(db_path))
      LOG_DEBUG (("Unable to remove custom db folder: %s\n", db_path));
}


/* Set the given database parameter into the parameters buffer.
 *
 * On error, a negative number is returned.
 * On success, the number of characters that would have been written is
 * returned. */
static int
set_dbparam (char *params, int len, const char *fmt, ...)
{
  int n;
  va_list args;

  va_start (args, fmt);
  n = vsnprintf (params + len, DB_PARAMS - len, fmt, args);
  va_end (args);

  if (n < 0) {
    n = 0;
    LOG_DEBUG (("Output error is encountered on set_dbparam\n"));
  } else if (n >= DB_PARAMS - len) {
    LOG_DEBUG (("Output truncated on set_dbparam\n"));
    n = DB_PARAMS - len;
  }

  return n;
}

/* Set the on-disk database parameters from enabled options in the config file. */
void
tc_db_get_params (char *params, const char *path)
{
  int len = 0;
  long xmmap = conf.xmmap;
  uint32_t lcnum, ncnum, lmemb, nmemb, bnum;

  /* copy path name to buffer */
  len += set_dbparam (params, len, "%s", path);

  /* caching parameters of a B+ tree database object */
  lcnum = conf.cache_lcnum > 0 ? conf.cache_lcnum : TC_LCNUM;
  len += set_dbparam (params, len, "#%s=%d", "lcnum", lcnum);

  ncnum = conf.cache_ncnum > 0 ? conf.cache_ncnum : TC_NCNUM;
  len += set_dbparam (params, len, "#%s=%d", "ncnum", ncnum);

  /* set the size of the extra mapped memory */
  if (xmmap > 0)
    len += set_dbparam (params, len, "#%s=%ld", "xmsiz", xmmap);

  lmemb = conf.tune_lmemb > 0 ? conf.tune_lmemb : TC_LMEMB;
  len += set_dbparam (params, len, "#%s=%d", "lmemb", lmemb);

  nmemb = conf.tune_nmemb > 0 ? conf.tune_nmemb : TC_NMEMB;
  len += set_dbparam (params, len, "#%s=%d", "nmemb", nmemb);

  bnum = conf.tune_bnum > 0 ? conf.tune_bnum : TC_BNUM;
  len += set_dbparam (params, len, "#%s=%d", "bnum", bnum);

  /* compression */
  len += set_dbparam (params, len, "#%s=%c", "opts", 'l');

  if (conf.compression == TC_BZ2) {
    len += set_dbparam (params, len, "%c", 'b');
  } else if (conf.compression == TC_ZLIB) {
    len += set_dbparam (params, len, "%c", 'd');
  }

  /* open flags. create a new database if not exist, otherwise read it */
  len += set_dbparam (params, len, "#%s=%s", "mode", "wc");
  /* if not loading from disk, truncate regardless if a db file exists */
  if (!conf.load_from_disk)
    len += set_dbparam (params, len, "%c", 't');

  LOG_DEBUG (("%s\n", path));
  LOG_DEBUG (("params: %s\n", params));
}

/* Open the database handle.
 *
 * On error, the program will exit.
 * On success, the opened on-disk database is returned. */
TCBDB *
tc_bdb_create (char *dbpath)
{
  TCBDB *bdb;
  int ecode;
  uint32_t lcnum, ncnum, lmemb, nmemb, bnum, flags;

  bdb = tcbdbnew ();

  lcnum = conf.cache_lcnum > 0 ? conf.cache_lcnum : TC_LCNUM;
  ncnum = conf.cache_ncnum > 0 ? conf.cache_ncnum : TC_NCNUM;

  /* set the caching parameters of a B+ tree database object */
  if (!tcbdbsetcache (bdb, lcnum, ncnum)) {
    free (dbpath);
    FATAL ("Unable to set TCB cache");
  }

  /* set the size of the extra mapped memory */
  if (conf.xmmap > 0 && !tcbdbsetxmsiz (bdb, conf.xmmap)) {
    free (dbpath);
    FATAL ("Unable to set TCB xmmap.");
  }

  lmemb = conf.tune_lmemb > 0 ? conf.tune_lmemb : TC_LMEMB;
  nmemb = conf.tune_nmemb > 0 ? conf.tune_nmemb : TC_NMEMB;
  bnum = conf.tune_bnum > 0 ? conf.tune_bnum : TC_BNUM;

  /* compression */
  flags = BDBTLARGE;
  if (conf.compression == TC_BZ2) {
    flags |= BDBTBZIP;
  } else if (conf.compression == TC_ZLIB) {
    flags |= BDBTDEFLATE;
  }

  /* set the tuning parameters */
  tcbdbtune (bdb, lmemb, nmemb, bnum, 8, 10, flags);

  /* open flags */
  flags = BDBOWRITER | BDBOCREAT;
  if (!conf.load_from_disk)
    flags |= BDBOTRUNC;

  /* attempt to open the database */
  if (!tcbdbopen (bdb, dbpath, flags)) {
    free (dbpath);
    ecode = tcbdbecode (bdb);

    FATAL ("%s", tcbdberrmsg (ecode));
  }

  return bdb;
}

/* Close the database handle.
 *
 * On error, 1 is returned.
 * On success or the database is closed, 0 is returned. */
int
tc_bdb_close (void *db, char *dbname)
{
  TCBDB *bdb = db;
  int ecode;

  if (bdb == NULL)
    return 1;

  /* close the database */
  if (!tcbdbclose (bdb)) {
    ecode = tcbdbecode (bdb);
    FATAL ("%s", tcbdberrmsg (ecode));
  }
  /* delete the object */
  tcbdbdel (bdb);

  /* remove database file */
  if (!conf.keep_db_files && !tcremovelink (dbname))
    LOG_DEBUG (("Unable to remove DB: %s\n", dbname));
  free (dbname);

  return 0;
}

/* Compare the given integer value with one in the list object.
 *
 * If values are the equal, 1 is returned else 0 is returned. */
static int
find_int_key_in_list (void *data, void *needle)
{
  return (*(int *) data) == (*(int *) needle) ? 1 : 0;
}

/* Iterate over the list object and compare the current value with the given
 * value.
 *
 * If the value exists, 1 is returned else 0 is returned. */
static int
is_value_in_tclist (TCLIST * tclist, void *value)
{
  int i, sz;
  int *val;

  if (!tclist)
    return 0;

  for (i = 0; i < tclistnum (tclist); ++i) {
    val = (int *) tclistval (tclist, i, &sz);
    if (find_int_key_in_list (value, val))
      return 1;
  }

  return 0;
}

/* Insert a string key and the corresponding string value.
 * Note: If the key exists, the value is not replaced.
 *
 * On error, or if found, 1 is returned.
 * On success, or if not in the list, 0 is returned. */
int
ins_igsl (void *hash, int key, int value)
{
  TCLIST *list;
  int in_list = 0;

  if (!hash)
    return -1;

  /* key found, check if key exists within the list */
  if ((list = tcbdbget4 (hash, &key, sizeof (int))) != NULL) {
    if (is_value_in_tclist (list, &value))
      in_list = 1;
    tclistdel (list);
  }
  /* if not on the list, add it */
  if (!in_list && tcbdbputdup (hash, &key, sizeof (int), &value, sizeof (int)))
    return 0;

  return -1;
}
#endif
