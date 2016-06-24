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

#ifndef UI_H_INCLUDED
#define UI_H_INCLUDED

#ifdef HAVE_NCURSESW_NCURSES_H
#include <ncursesw/ncurses.h>
#elif HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#elif HAVE_CURSES_H
#include <curses.h>
#endif

#ifdef HAVE_LIBPTHREAD
#include <pthread.h>
#endif

/* Global UI defaults */
#define MIN_HEIGHT        8     /* minimum window height */
#define MIN_WIDTH         0     /* minimum window width */
#define MAX_HEIGHT_FOOTER 1     /* height of the footer window */
#define MAX_HEIGHT_HEADER 7     /* height of the header window */
#define OVERALL_NUM_COLS  4     /* number of columns on the overall stats win */

/* Overall Stats Labels */
#define T_DASH       "Tableau de bord"
#define T_HEAD       "Requètes analysées, vue d'ensemble"

#define T_DATETIME   "Date/Heure"
#define T_REQUESTS   "Nb total de requètes"
#define T_GEN_TIME   "Temps de calcul"
#define T_FAILED     "Requètes échouées"
#define T_VALID      "Requètes valides"
#define T_UNIQUE_VIS "Visiteurs uniques"
#define T_UNIQUE_FIL "Fichiers uniques"
#define T_EXCLUDE_IP "Excl. IP Hits"
#define T_REFERRER   "Origine"
#define T_UNIQUE404  "Unique 404"
#define T_STATIC_FIL "Fichiers statiques"
#define T_LOG        "Taille du log"
#define T_BW         "Bande passante"
#define T_LOG_PATH   "Fichier de log"

/* Spinner Label Format */
#define SPIN_FMT "%s"
#define SPIN_FMTM "%s [%'d] [%'lld/s]"
#define SPIN_LBL 50     /* max length of the progress spinner */

#define INCLUDE_BOTS " - robots inclus"

/* Module Labels and Descriptions */
#define VISIT_HEAD  "Visiteurs uniques par jour"
#define VISIT_DESC  "Les hits depuis la même IP, date et user-agent comptent comme visite unique"
#define VISIT_ID    "visitors"
#define VISIT_LABEL "Visiteurs"

#define REQUE_HEAD  "Fichiers demandés (URLs)"
#define REQUE_DESC  "Top des requètes triés par hits [, avgts, cumts, maxts, mthd, proto]"
#define REQUE_ID    "requests"
#define REQUE_LABEL "Requètes"

#define STATI_HEAD  "Requètes statiques"
#define STATI_DESC  "Top des requètes statiques trié par hits [, avgts, cumts, maxts, mthd, proto]"
#define STATI_ID    "static_requests"
#define STATI_LABEL "Requètes Statiques"

#define VTIME_HEAD  "Distribution temporelle"
#define VTIME_DESC  "Données triée par heure [, avgts, cumts, maxts]"
#define VTIME_ID    "visit_time"
#define VTIME_LABEL "Temps"

#define VHOST_HEAD  "Hôtes virtuels"
#define VHOST_DESC  "Données triées par hits [, avgts, cumts, maxts]"
#define VHOST_ID    "vhosts"
#define VHOST_LABEL "Hôtes virtuels"

#define FOUND_HEAD  "URLs Non trouvées (404s)"
#define FOUND_DESC  "Top des URLs non trouvées trié par hits [, avgts, cumts, maxts, mthd, proto]"
#define FOUND_ID    "not_found"
#define FOUND_LABEL "Non Trouvé"

#define HOSTS_HEAD  "Nom de machine et IPs des visiteurs"
#define HOSTS_DESC  "Top des visiteurs triés par hits [, avgts, cumts, maxts]"
#define HOSTS_ID    "hosts"
#define HOSTS_LABEL "Hôtes"

#define OPERA_HEAD  "Systèmes d'exploitation"
#define OPERA_DESC  "Top des SE trié par hits [, avgts, cumts, maxts]"
#define OPERA_ID    "os"
#define OPERA_LABEL "SE"

#define BROWS_HEAD  "Navigateurs"
#define BROWS_DESC  "Top des Navigateurs triés par hits [, avgts, cumts, maxts]"
#define BROWS_ID    "browsers"
#define BROWS_LABEL "Navigateurs"

#define REFER_HEAD  "URLs d'origine"
#define REFER_DESC  "Top des URLs d'origine trié par hits [, avgts, cumts, maxts]"
#define REFER_ID    "referrers"
#define REFER_LABEL "Origine"

#define SITES_HEAD  "Sites d'origine"
#define SITES_DESC  "Top des sites d'origine trié par hits [, avgts, cumts, maxts]"
#define SITES_ID    "referring_sites"
#define SITES_LABEL "sites d'origine"

#define KEYPH_HEAD  "Mot-clés Google"
#define KEYPH_DESC  "Top des Mot-clés trié par hits [, avgts, cumts, maxts]"
#define KEYPH_ID    "keyphrases"
#define KEYPH_LABEL "Mot-clés"

#define GEOLO_HEAD  "Geo Localisation"
#define GEOLO_DESC  "Continent > Pays trié par hits unique [, avgts, cumts, maxts]"
#define GEOLO_ID    "geolocation"
#define GEOLO_LABEL "Geolocalisation"

#define CODES_HEAD  "Status HTTP"
#define CODES_DESC  "Top des status HTTP trié par hits [, avgts, cumts, maxts]"
#define CODES_ID    "status_codes"
#define CODES_LABEL "Status HTTP"

#define GENER_ID   "general"

/* Overall Statistics CSV/JSON Keys */
#define OVERALL_DATETIME  "date_time"
#define OVERALL_REQ       "total_requests"
#define OVERALL_VALID     "valid_requests"
#define OVERALL_GENTIME   "generation_time"
#define OVERALL_FAILED    "failed_requests"
#define OVERALL_VISITORS  "unique_visitors"
#define OVERALL_FILES     "unique_files"
#define OVERALL_EXCL_HITS "excluded_hits"
#define OVERALL_REF       "unique_referrers"
#define OVERALL_NOTFOUND  "unique_not_found"
#define OVERALL_STATIC    "unique_static_files"
#define OVERALL_LOGSIZE   "log_size"
#define OVERALL_BANDWIDTH "bandwidth"
#define OVERALL_LOG       "log_path"

/* Metric Labels */
#define MTRC_HITS_LBL            "Hits"
#define MTRC_VISITORS_LBL        "Visitors"
#define MTRC_VISITORS_SHORT_LBL  "Vis."
#define MTRC_BW_LBL              "Bandwith"
#define MTRC_AVGTS_LBL           "Avg. T.S."
#define MTRC_CUMTS_LBL           "Cum. T.S."
#define MTRC_MAXTS_LBL           "Max. T.S."
#define MTRC_METHODS_LBL         "Method"
#define MTRC_METHODS_SHORT_LBL   "Mtd"
#define MTRC_PROTOCOLS_LBL       "Protocol"
#define MTRC_PROTOCOLS_SHORT_LBL "Proto"
#define MTRC_CITY_LBL            "City"
#define MTRC_COUNTRY_LBL         "Country"
#define MTRC_HOSTNAME_LBL        "Hostname"
#define MTRC_DATA_LBL            "Data"

/* Find Labels */
#define FIND_HEAD    "Trouver tous les motifs dans toutes les vues"
#define FIND_DESC    "Regex OK - ^g pour annuler - TAB pour changer la casse"

/* CONFIG DIALOG */
#define CONF_MENU_H       6
#define CONF_MENU_W       57
#define CONF_MENU_X       2
#define CONF_MENU_Y       4
#define CONF_WIN_H        20
#define CONF_WIN_W        61

/* FIND DIALOG */
#define FIND_DLG_HEIGHT   8
#define FIND_DLG_WIDTH    50
#define FIND_MAX_MATCHES  1

/* COLOR SCHEME DIALOG */
#define SCHEME_MENU_H     4
#define SCHEME_MENU_W     38
#define SCHEME_MENU_X     2
#define SCHEME_MENU_Y     4
#define SCHEME_WIN_H      10
#define SCHEME_WIN_W      42

/* SORT DIALOG */
#define SORT_MENU_H       6
#define SORT_MENU_W       38
#define SORT_MENU_X       2
#define SORT_MENU_Y       4
#define SORT_WIN_H        13
#define SORT_WIN_W        42
#define SORT_ASC_SEL      "[x] ASC [ ] DESC"
#define SORT_DESC_SEL     "[ ] ASC [x] DESC"

/* AGENTS DIALOG */
#define AGENTS_MENU_X     2
#define AGENTS_MENU_Y     4

/* HELP DIALOG */
#define HELP_MENU_HEIGHT  12
#define HELP_MENU_WIDTH   60
#define HELP_MENU_X       2
#define HELP_MENU_Y       4
#define HELP_WIN_HEIGHT   17
#define HELP_WIN_WIDTH    64

#define CSENSITIVE    "[x] sensible à la casse"
#define CISENSITIVE    "[ ] sensible à la casse"

/* Convenient macros */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#include "color.h"
#include "commons.h"
#include "sort.h"

/* Curses dashboard find */
typedef struct GFind_
{
  GModule module;
  char *pattern;
  int next_idx;
  int next_parent_idx;
  int next_sub_idx;
  int look_in_sub;
  int done;
  int icase;
} GFind;

/* Each panel contains its own scrolling and offset */
typedef struct GScrollModule_
{
  int scroll;
  int offset;
} GScrollModule;

/* Curses Scrolling */
typedef struct GScroll_
{
  GScrollModule module[TOTAL_MODULES];
  GModule current;
  int dash;
  int expanded;
} GScroll;

/* Spinner or Progress Indicator */
typedef struct GSpinner_
{
  const char *label;
  GColors *(*color) (void);
  int curses;
  int spin_x;
  int w;
  int x;
  int y;
  pthread_mutex_t mutex;
  pthread_t thread;
  unsigned int *processed;
  WINDOW *win;
  enum
  {
    SPN_RUN,
    SPN_END
  } state;
} GSpinner;

/* Controls metric output.
 * i.e., which metrics it should display */
typedef struct GOutput_
{
  GModule module;
  int8_t visitors;
  int8_t hits;
  int8_t percent;
  int8_t bw;
  int8_t avgts;
  int8_t cumts;
  int8_t maxts;
  int8_t protocol;
  int8_t method;
  int8_t data;
  int8_t graph;                 /* display bars when collapsed */
  int8_t sub_graph;             /* display bars upon expanding it */
} GOutput;

/* *INDENT-OFF* */
GOutput *output_lookup (GModule module);
GSpinner *new_gspinner (void);

char *get_browser_type (char *line);
char *get_overall_header (GHolder *h);
char *input_string (WINDOW * win, int pos_y, int pos_x, size_t max_width, const char *str, int enable_case, int *toggle_case);
const char *module_to_desc (GModule module);
const char *module_to_head (GModule module);
const char *module_to_id (GModule module);
const char *module_to_label (GModule module);
int render_confdlg (GLog * logger, GSpinner * spinner);
int set_host_agents (const char *addr, void (*func) (void *, void *, int), void *arr);
void close_win (WINDOW * w);
void display_general (WINDOW * win, GLog * logger, GHolder *h);
void draw_header (WINDOW * win, const char *s, const char *fmt, int y, int x, int w, GColors * (*func) (void));
void end_spinner (void);
void generate_time (void);
void init_colors (int force);
void init_windows (WINDOW ** header_win, WINDOW ** main_win);
void load_agent_list (WINDOW * main_win, char *addr);
void load_help_popup (WINDOW * main_win);
void load_schemes_win (WINDOW * main_win);
void load_sort_win (WINDOW * main_win, GModule module, GSort * sort);
void set_curses_spinner (GSpinner *spinner);
void set_input_opts (void);
void set_wbkgd (WINDOW *main_win, WINDOW *header_win);
void term_size (WINDOW * main_win, int *main_win_height);
void ui_spinner_create (GSpinner * spinner);
void update_active_module (WINDOW * header_win, GModule current);
void update_header (WINDOW * header_win, int current);

/* *INDENT-ON* */
#endif
