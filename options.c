/**
 * options.c -- functions related to parsing program options
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabinet.h"
#endif

#include "options.h"

#include "error.h"
#include "settings.h"
#include "util.h"

static char short_options[] = "f:e:p:o:"
#ifdef DEBUG
  "l:"
#endif
#ifdef HAVE_LIBGEOIP
  "g"
#endif
  "acrmMhHqds";

/* *INDENT-OFF* */
struct option long_opts[] = {
  {"log-file"             , required_argument , 0 , 'f' } ,
  {"agent-list"           , no_argument       , 0 , 'a' } ,
  {"config-dialog"        , no_argument       , 0 , 'c' } ,
  {"config-file"          , required_argument , 0 , 'p' } ,
  {"exclude-ip"           , required_argument , 0 , 'e' } ,
  {"help"                 , no_argument       , 0 , 'h' } ,
  {"http-method"          , no_argument       , 0 , 'M' } ,
  {"http-protocol"        , no_argument       , 0 , 'H' } ,
#ifdef DEBUG
  {"debug-file"           , required_argument , 0 , 'l' } ,
#endif
  {"no-query-string"      , no_argument       , 0 , 'q' } ,
  {"no-term-resolver"     , no_argument       , 0 , 'r' } ,
  {"output-format"        , required_argument , 0 , 'o' } ,
  {"color-scheme"         , required_argument , 0 ,  0  } ,
  {"date-format"          , required_argument , 0 ,  0  } ,
  {"log-format"           , required_argument , 0 ,  0  } ,
  {"real-os"              , no_argument       , 0 ,  0  } ,
  {"no-color"             , no_argument       , 0 ,  0  } ,
  {"no-global-config"     , no_argument       , 0 ,  0  } ,
  {"storage"              , no_argument       , 0 , 's' } ,
  {"no-progress"          , no_argument       , 0 ,  0  } ,
  {"with-mouse"           , no_argument       , 0 , 'm' } ,
  {"with-output-resolver" , no_argument       , 0 , 'd' } ,
#ifdef HAVE_LIBGEOIP
  {"std-geoip"            , no_argument       , 0 , 'g' } ,
  {"geoip-city-data"      , required_argument , 0 ,  0  } ,
#endif
#ifdef TCB_BTREE
  {"db-path"              , required_argument , 0 ,  0  } ,
  {"compression"          , required_argument , 0 ,  0  } ,
  {"xmmap"                , required_argument , 0 ,  0  } ,
  {"cache-lcnum"          , required_argument , 0 ,  0  } ,
  {"cache-ncnum"          , required_argument , 0 ,  0  } ,
  {"tune-lmemb"           , required_argument , 0 ,  0  } ,
  {"tune-nmemb"           , required_argument , 0 ,  0  } ,
  {"tune-bnum"            , required_argument , 0 ,  0  } ,
#endif
  {0, 0, 0, 0}
};
/* *INDENT-ON* */

void
cmd_help (void)
{
  printf ("\nGoAccess - %s\n\n", GO_VERSION);
  printf ("Usage: ");
  printf ("goaccess -f log_file [-c][-r][-m][-h][-q][-d][...]\n\n");
  printf ("The following options can also be supplied to the command:\n\n");
  printf (" -f --log-file=<filename>     ");
  printf ("Path to input log file.\n");
  printf (" -a --agent-list              ");
  printf ("Enable a list of user-agents by host.\n");
  printf ("                              ");
  printf ("For faster parsing, don't enable this flag.\n");
  printf (" -c --config-dialog           ");
  printf ("Prompt log/date configuration window.\n");
  printf (" -d --with-output-resolver    ");
  printf ("Enable IP resolver on HTML|JSON output.\n");
  printf (" -e --exclude-ip=<IP>         ");
  printf ("Exclude an IP from being counted.\n");
#ifdef HAVE_LIBGEOIP
  printf (" -g --std-geoip               ");
  printf ("Standard GeoIP database for less memory usage.\n");
#endif
#ifdef DEBUG
  printf (" -l --debug-file=<filename>   ");
  printf ("Send all debug messages to the specified file.\n");
#endif
  printf (" -h --help                    ");
  printf ("This help.\n");
  printf (" -H --http-protocol           ");
  printf ("Include HTTP request protocol if found.\n");
  printf (" -m --with-mouse              ");
  printf ("Enable mouse support on main dashboard.\n");
  printf (" -M --http-method             ");
  printf ("Include HTTP request method if found.\n");
  printf (" -o --output-format=csv|json  ");
  printf ("Output format:\n");
  printf ("                              ");
  printf ("'-o csv' for CSV.\n");
  printf ("                              ");
  printf ("'-o json' for JSON.\n");
  printf (" -p --config-file=<filename>  ");
  printf ("Custom configuration file.\n");
  printf (" -q --no-query-string         ");
  printf ("Ignore request's query string.\n");
  printf (" -r --no-term-resolver        ");
  printf ("Disable IP resolver on terminal output.\n");
  printf (" -s --storage                 ");
  printf ("Display current storage method. i.e., B+ Tree, Hash.\n");
#ifdef HAVE_LIBGEOIP
  printf (" --geoip-city-data=<path>     ");
  printf ("Specify path to GeoIP City database file. i.e., GeoLiteCity.dat\n");
#endif
#ifdef TCB_BTREE
  printf (" --db-path=<path>             ");
  printf ("Path of the database file. [%s]\n", TC_DBPATH);
  printf (" --xmmap=<number>             ");
  printf ("Set the size in bytes of the extra mapped memory. [%d]\n", TC_MMAP);
  printf (" --cache-lcnum=<number>       ");
  printf ("Max number of leaf nodes to be cached. [%d]\n", TC_LCNUM);
  printf (" --cache-ncnum=<number>       ");
  printf ("Max number of non-leaf nodes to be cached. [%d]\n", TC_NCNUM);
  printf (" --tune-lmemb=<number>        ");
  printf ("Number of members in each leaf page. [%d]\n", TC_LMEMB);
  printf (" --tune-nmemb=<number>        ");
  printf ("Number of members in each non-leaf page. [%d]\n", TC_NMEMB);
  printf (" --tune-bnum=<number>         ");
  printf ("Number of elements of the bucket array. [%d]\n", TC_BNUM);
#if defined(HAVE_ZLIB) || defined(HAVE_BZ2)
  printf (" --compression=<zlib|bz2>     ");
  printf ("Specifies that each page is compressed with ZLIB|BZ2 encoding.\n");
#endif
#endif
  printf (" --no-progress                ");
  printf ("Disable progress metrics.\n");
  printf (" --no-global-config           ");
  printf ("Do not load the global configuration file.\n");
  printf (" --no-color                   ");
  printf ("Disable colored output.\n");
  printf (" --real-os                    ");
  printf ("Display real OS names. e.g, Windows XP, Snow Leopard.\n\n");

  printf ("Examples can be found by running `man goaccess`.\n\n");
  printf ("For more details visit: http://goaccess.prosoftcorp.com\n");
  printf ("GoAccess Copyright (C) 2009-2014 GNU GPL'd, by Gerardo Orellana");
  printf ("\n\n");
  exit (EXIT_FAILURE);
}

void
verify_global_config (int argc, char **argv)
{
  int o, idx = 0;

  conf.load_global_config = 1;
  while ((o = getopt_long (argc, argv, short_options, long_opts, &idx)) >= 0) {
    if (-1 == o || EOF == o)
      break;

    switch (o) {
     case 'p':
       conf.iconfigfile = optarg;
       break;
     case 0:
       if (!strcmp ("no-global-config", long_opts[idx].name))
         conf.load_global_config = 0;
       break;
     case '?':
       exit (EXIT_FAILURE);
    }
  }
  for (idx = optind; idx < argc; idx++)
    cmd_help ();

  /* reset it to 1 */
  optind = 1;
}

void
read_option_args (int argc, char **argv)
{
  int o, idx = 0;

#ifdef HAVE_LIBGEOIP
  conf.geo_db = GEOIP_MEMORY_CACHE;
#endif

  while ((o = getopt_long (argc, argv, short_options, long_opts, &idx)) >= 0) {
    if (-1 == o || EOF == o)
      break;
    switch (o) {
     case 'f':
       conf.ifile = optarg;
       break;
     case 'p':
       /* ignore it */
       break;
#ifdef HAVE_LIBGEOIP
     case 'g':
       conf.geo_db = GEOIP_STANDARD;
       break;
#endif
     case 'e':
       conf.ignore_host = optarg;
       break;
     case 'a':
       conf.list_agents = 1;
       break;
     case 'c':
       conf.load_conf_dlg = 1;
       break;
     case 'q':
       conf.ignore_qstr = 1;
       break;
     case 'o':
       conf.output_format = optarg;
     case 'l':
       conf.debug_log = optarg;
       dbg_log_open (conf.debug_log);
       break;
     case 'r':
       conf.skip_term_resolver = 1;
       break;
     case 'd':
       conf.enable_html_resolver = 1;
       break;
     case 'm':
       conf.mouse_support = 1;
       break;
     case 'M':
       conf.append_method = 1;
       break;
     case 'h':
       cmd_help ();
       break;
     case 'H':
       conf.append_protocol = 1;
       break;
     case 0:

       if (!strcmp ("no-global-config", long_opts[idx].name))
         break; /* ignore it */

       if (!strcmp ("color-scheme", long_opts[idx].name))
         conf.color_scheme = atoi (optarg);
       if (!strcmp ("log-format", long_opts[idx].name))
         conf.log_format = unescape_str (optarg);
       if (!strcmp ("date-format", long_opts[idx].name))
         conf.date_format = unescape_str (optarg);

       if (!strcmp ("real-os", long_opts[idx].name))
         conf.real_os = 1;
       if (!strcmp ("no-color", long_opts[idx].name))
         conf.no_color = 1;
       if (!strcmp ("no-progress", long_opts[idx].name))
         conf.no_progress = 1;

       /* specifies the path of the GeoIP City database file */
       if (!strcmp ("geoip-city-data", long_opts[idx].name))
         conf.geoip_city_data = optarg;

       /* specifies the path of the database file */
       if (!strcmp ("db-path", long_opts[idx].name))
         conf.db_path = optarg;
       /* set the size in bytes of the extra mapped memory */
       if (!strcmp ("xmmap", long_opts[idx].name))
         conf.xmmap = atoi (optarg);
       /* specifies the maximum number of leaf nodes to be cached */
       if (!strcmp ("cache-lcnum", long_opts[idx].name))
         conf.cache_lcnum = atoi (optarg);
       /* specifies the maximum number of non-leaf nodes to be cached */
       if (!strcmp ("cache-ncnum", long_opts[idx].name))
         conf.cache_ncnum = atoi (optarg);
       /* number of members in each leaf page */
       if (!strcmp ("tune-lmemb", long_opts[idx].name))
         conf.tune_lmemb = atoi (optarg);
       /* number of members in each non-leaf page */
       if (!strcmp ("tune-nmemb", long_opts[idx].name))
         conf.tune_nmemb = atoi (optarg);
       /* number of elements of the bucket array */
       if (!strcmp ("tune-bnum", long_opts[idx].name))
         conf.tune_bnum = atoi (optarg);

       /* specifies that each page is compressed with X encoding */
       if (!strcmp ("compression", long_opts[idx].name)) {
#ifdef HAVE_ZLIB
         if (!strcmp ("zlib", optarg))
           conf.compression = TC_ZLIB;
#endif
#ifdef HAVE_BZ2
         if (!strcmp ("bz2", optarg))
           conf.compression = TC_BZ2;
#endif
       }

       break;
     case 's':
       display_storage ();
       exit (EXIT_SUCCESS);
     case '?':
       exit (EXIT_FAILURE);
     default:
       exit (EXIT_FAILURE);
    }
  }

  for (idx = optind; idx < argc; idx++)
    cmd_help ();
}
