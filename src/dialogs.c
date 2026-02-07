/**
 * dialogs.c -- UI dialog windows
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2025 Gerardo Orellana <hello @ goaccess.io>
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

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "dialogs.h"
#include "ui.h"
#include "color.h"
#include "error.h"
#include "gkhash.h"
#include "gmenu.h"
#include "util.h"
#include "xmalloc.h"

/* Add the given user agent value into our array of GAgents.
 *
 * On error, 1 is returned.
 * On success, the user agent is added to the array and 0 is returned. */
static int
set_agents (void *val, void *user_data) {
  GAgents *agents = user_data;
  GAgentItem *tmp = NULL;
  char *agent = NULL;
  int newlen = 0, i;

  if (!(agent = ht_get_host_agent_val (*(uint32_t *) val)))
    return 1;

  if (agents->size - 1 == agents->idx) {
    newlen = agents->size + 4;
    if (!(tmp = realloc (agents->items, newlen * sizeof (GAgentItem))))
      FATAL ("Unable to realloc agents");
    agents->items = tmp;
    agents->size = newlen;
  }

  for (i = 0; i < agents->idx; ++i) {
    if (strcmp (agent, agents->items[i].agent) == 0) {
      free (agent);
      return 0;
    }
  }

  agents->items[agents->idx++].agent = agent;
  return 0;
}

/* Iterate over the list of agents */
GAgents *
load_host_agents (const char *addr) {
  GAgents *agents = NULL;
  GSLList *keys = NULL, *list = NULL;
  void *data = NULL;
  uint32_t items = 4, key = djb2 ((const unsigned char *) addr);

  keys = ht_get_keymap_list_from_key (HOSTS, key);
  if (!keys)
    return NULL;

  agents = new_gagents (items);
  /* *INDENT-OFF* */
  GSLIST_FOREACH (keys, data, {
    if ((list = ht_get_host_agent_list (HOSTS, (*(uint32_t *) data)))) {
      list_foreach (list, set_agents, agents);
      list_remove_nodes (list);
    }
  });
  /* *INDENT-ON* */
  list_remove_nodes (keys);

  return agents;
}

/* Fill the given terminal dashboard menu with user agent data.
 *
 * On error, the 1 is returned.
 * On success, 0 is returned. */
static int
fill_host_agents_gmenu (GMenu *menu, GAgents *agents) {
  int i;

  if (agents == NULL)
    return 1;

  menu->items = xcalloc (agents->idx, sizeof (GItem));
  for (i = 0; i < agents->idx; ++i) {
    menu->items[i].name = xstrdup (agents->items[i].agent);
    menu->items[i].checked = 0;
    menu->size++;
  }

  return 0;
}

/* Render a list of agents if available for the selected host/IP. */
void
load_agent_list (WINDOW *main_win, char *addr) {
  GMenu *menu;
  GAgents *agents = NULL;
  WINDOW *win;
  char buf[256];
  int c, quit = 1, i;
  int y, x, list_h, list_w, menu_w, menu_h;

  if (!conf.list_agents)
    return;

  getmaxyx (stdscr, y, x);
  list_h = y / 2;
  list_w = x - 4;
  menu_h = list_h - AGENTS_MENU_Y - 1;
  menu_w = list_w - AGENTS_MENU_X - AGENTS_MENU_X;

  win = newwin (list_h, list_w, (y - list_h) / 2, (x - list_w) / 2);
  keypad (win, TRUE);
  wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

  menu = new_gmenu (win, menu_h, menu_w, AGENTS_MENU_Y, AGENTS_MENU_X);
  if (!(agents = load_host_agents (addr)))
    goto out;
  if (fill_host_agents_gmenu (menu, agents) != 0)
    goto out;

  post_gmenu (menu);
  snprintf (buf, sizeof buf, AGENTSDLG_HEAD, addr);
  draw_header (win, buf, " %s", 1, 1, list_w - 2, color_panel_header);
  mvwprintw (win, 2, 2, AGENTSDLG_DESC);
  wrefresh (win);

  while (quit) {
    c = wgetch (stdscr);
    switch (c) {
    case KEY_DOWN:
      gmenu_driver (menu, REQ_DOWN);
      break;
    case KEY_UP:
      gmenu_driver (menu, REQ_UP);
      break;
    case KEY_RESIZE:
    case 'q':
      quit = 0;
      break;
    }
    wrefresh (win);
  }

  touchwin (main_win);
  close_win (win);
  win = NULL;
  wrefresh (main_win);

out:
  for (i = 0; i < menu->size; ++i)
    free (menu->items[i].name);
  if (menu->items)
    free (menu->items);
  free (menu);
  free_agents_array (agents);
  close_win (win);
}

/* Help menu data */
static const char *help_main[] = {
  "",
  "Copyright (C) 2009-2024 by Gerardo Orellana",
  "https://goaccess.io - <hello@goaccess.io>",
  "Released under the MIT License",
  "",
  "GoAccess is an open source real-time web log analyzer and",
  "interactive viewer that runs in a terminal in *nix systems or",
  "through your browser.",
  "",
  "KEYS:",
  "",
  "1-9,0     Jump to panel by position (1st, 2nd, ... 10th)",
  "TAB       Forward module",
  "SHIFT+TAB Backward module",
  "^f        Scroll forward inside expanded module",
  "^b        Scroll backward inside expanded module",
  "s         Sort options for current module",
  "p         Reorder panels",
  "/         Search across all modules (regex allowed)",
  "n         Find next occurrence",
  "g         Move to the top/beginning of screen",
  "G         Move to the bottom/end of screen",
  "j         Scroll down within expanded module",
  "k         Scroll up within expanded module",
  "ENTER     Expand selected module",
  "o/O       Expand selected module",
  "q         Quit (or collapse if inside module)",
  "c         Set/change color scheme",
  "m/M       Cycle through chart metrics (forward/backward)",
  "l/L       Toggle logarithmic scale for current panel",
  "r/R       Toggle reverse chronological order in charts",
  "?         This help",
  "",
  "Examples can be found by running 'man goaccess'.",
  "",
  "[Press any key to continue]"
};

/* Render the help dialog. */
void
load_help_popup (WINDOW *main_win) {
  int c, quit = 1;
  size_t i, n;
  int y, x, h = HELP_WIN_HEIGHT, w = HELP_WIN_WIDTH;
  int w2 = w - 2;
  WINDOW *win;
  GMenu *menu;

  n = ARRAY_SIZE (help_main);
  getmaxyx (stdscr, y, x);

  win = newwin (h, w, (y - h) / 2, (x - w) / 2);
  keypad (win, TRUE);
  wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

  menu = new_gmenu (win, HELP_MENU_HEIGHT, HELP_MENU_WIDTH, HELP_MENU_Y, HELP_MENU_X);
  menu->size = n;

  menu->items = (GItem *) xcalloc (n, sizeof (GItem));
  for (i = 0; i < n; ++i) {
    menu->items[i].name = alloc_string (help_main[i]);
    menu->items[i].checked = 0;
  }

  post_gmenu (menu);
  draw_header (win, HELPDLG_HEAD, " %s", 1, 1, w2, color_panel_header);
  mvwprintw (win, 2, 2, HELPDLG_DESC);
  wrefresh (win);

  while (quit) {
    c = wgetch (stdscr);
    switch (c) {
    case KEY_DOWN:
      gmenu_driver (menu, REQ_DOWN);
      break;
    case KEY_UP:
      gmenu_driver (menu, REQ_UP);
      break;
    case KEY_RESIZE:
    case 'q':
      quit = 0;
      break;
    }
    wrefresh (win);
  }

  for (i = 0; i < n; ++i)
    free (menu->items[i].name);
  free (menu->items);
  free (menu);

  touchwin (main_win);
  close_win (win);
  wrefresh (main_win);
}

/* Render the sort dialog. */
void
load_sort_win (WINDOW *main_win, GModule module, GSort *sort) {
  GMenu *menu;
  WINDOW *win;
  GSortField opts[SORT_MAX_OPTS];
  int c, quit = 1;
  int i = 0, k, n = 0;
  int y, x, h = SORT_WIN_H, w = SORT_WIN_W;
  int w2 = w - 2;

  getmaxyx (stdscr, y, x);

  for (i = 0, k = 0; -1 != sort_choices[module][i]; i++) {
    GSortField field = sort_choices[module][i];
    if (SORT_BY_CUMTS == field && !conf.serve_usecs)
      continue;
    else if (SORT_BY_MAXTS == field && !conf.serve_usecs)
      continue;
    else if (SORT_BY_AVGTS == field && !conf.serve_usecs)
      continue;
    else if (SORT_BY_BW == field && !conf.bandwidth)
      continue;
    else if (SORT_BY_PROT == field && !conf.append_protocol)
      continue;
    else if (SORT_BY_MTHD == field && !conf.append_method)
      continue;
    opts[k++] = field;
    n++;
  }

  win = newwin (h, w, (y - h) / 2, (x - w) / 2);
  keypad (win, TRUE);
  wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

  menu = new_gmenu (win, SORT_MENU_H, SORT_MENU_W, SORT_MENU_Y, SORT_MENU_X);
  menu->size = n;
  menu->selectable = 1;

  menu->items = (GItem *) xcalloc (n, sizeof (GItem));

  for (i = 0; i < n; ++i) {
    GSortField field = sort_choices[module][opts[i]];
    if (SORT_BY_HITS == field) {
      menu->items[i].name = alloc_string (MTRC_HITS_LBL);
      if (sort->field == SORT_BY_HITS) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    } else if (SORT_BY_VISITORS == field) {
      menu->items[i].name = alloc_string (MTRC_VISITORS_LBL);
      if (sort->field == SORT_BY_VISITORS) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    } else if (SORT_BY_DATA == field) {
      menu->items[i].name = alloc_string (MTRC_DATA_LBL);
      if (sort->field == SORT_BY_DATA) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    } else if (SORT_BY_BW == field) {
      menu->items[i].name = alloc_string (MTRC_BW_LBL);
      if (sort->field == SORT_BY_BW) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    } else if (SORT_BY_AVGTS == field) {
      menu->items[i].name = alloc_string (MTRC_AVGTS_LBL);
      if (sort->field == SORT_BY_AVGTS) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    } else if (SORT_BY_CUMTS == field) {
      menu->items[i].name = alloc_string (MTRC_CUMTS_LBL);
      if (sort->field == SORT_BY_CUMTS) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    } else if (SORT_BY_MAXTS == field) {
      menu->items[i].name = alloc_string (MTRC_MAXTS_LBL);
      if (sort->field == SORT_BY_MAXTS) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    } else if (SORT_BY_PROT == field) {
      menu->items[i].name = alloc_string (MTRC_PROTOCOLS_LBL);
      if (sort->field == SORT_BY_PROT) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    } else if (SORT_BY_MTHD == field) {
      menu->items[i].name = alloc_string (MTRC_METHODS_LBL);
      if (sort->field == SORT_BY_MTHD) {
        menu->items[i].checked = 1;
        menu->idx = i;
      }
    }
  }

  post_gmenu (menu);
  draw_header (win, SORTDLG_HEAD, " %s", 1, 1, w2, color_panel_header);
  mvwprintw (win, 2, 2, SORTDLG_DESC);

  if (sort->sort == SORT_ASC)
    mvwprintw (win, SORT_WIN_H - 2, 1, " %s", SORT_ASC_SEL);
  else
    mvwprintw (win, SORT_WIN_H - 2, 1, " %s", SORT_DESC_SEL);

  wrefresh (win);

  while (quit) {
    c = wgetch (stdscr);
    switch (c) {
    case KEY_DOWN:
      gmenu_driver (menu, REQ_DOWN);
      break;
    case KEY_UP:
      gmenu_driver (menu, REQ_UP);
      break;
    case 9:
      if (sort->sort == SORT_ASC) {
        sort->sort = SORT_DESC;
        mvwprintw (win, SORT_WIN_H - 2, 1, " %s", SORT_DESC_SEL);
      } else {
        sort->sort = SORT_ASC;
        mvwprintw (win, SORT_WIN_H - 2, 1, " %s", SORT_ASC_SEL);
      }
      break;
    case 32:
    case 0x0a:
    case 0x0d:
    case KEY_ENTER:
      gmenu_driver (menu, REQ_SEL);
      for (i = 0; i < n; ++i) {
        if (menu->items[i].checked != 1)
          continue;
        if (strcmp ("Hits", menu->items[i].name) == 0)
          sort->field = SORT_BY_HITS;
        else if (strcmp ("Visitors", menu->items[i].name) == 0)
          sort->field = SORT_BY_VISITORS;
        else if (strcmp ("Data", menu->items[i].name) == 0)
          sort->field = SORT_BY_DATA;
        else if (strcmp ("Tx. Amount", menu->items[i].name) == 0)
          sort->field = SORT_BY_BW;
        else if (strcmp ("Avg. T.S.", menu->items[i].name) == 0)
          sort->field = SORT_BY_AVGTS;
        else if (strcmp ("Cum. T.S.", menu->items[i].name) == 0)
          sort->field = SORT_BY_CUMTS;
        else if (strcmp ("Max. T.S.", menu->items[i].name) == 0)
          sort->field = SORT_BY_MAXTS;
        else if (strcmp ("Protocol", menu->items[i].name) == 0)
          sort->field = SORT_BY_PROT;
        else if (strcmp ("Method", menu->items[i].name) == 0)
          sort->field = SORT_BY_MTHD;
        quit = 0;
        break;
      }
      break;
    case KEY_RESIZE:
    case 'q':
      quit = 0;
      break;
    }
    wrefresh (win);
  }

  for (i = 0; i < n; ++i)
    free (menu->items[i].name);
  free (menu->items);
  free (menu);

  touchwin (main_win);
  close_win (win);
  wrefresh (main_win);
}

static void
clear_confdlg_status_bar (WINDOW *win, int y, int x, int w) {
  draw_header (win, "", "%s", y, x, w + 1, color_default);
}

static char *
get_input_date_format (void) {
  char *date_format = NULL;
  if (conf.date_format)
    date_format = escape_str (conf.date_format);
  return date_format;
}

static char *
get_input_time_format (void) {
  char *time_format = NULL;
  if (conf.time_format)
    time_format = escape_str (conf.time_format);
  return time_format;
}

static char *
get_input_log_format (void) {
  char *log_format = NULL;
  if (conf.log_format)
    log_format = escape_str (conf.log_format);
  return log_format;
}

static void
draw_formats (WINDOW *win, int w2) {
  char *date_format = NULL, *log_format = NULL, *time_format = NULL;

  draw_header (win, CONFDLG_HEAD, " %s", 1, 1, w2, color_panel_header);
  mvwprintw (win, 2, 2, CONFDLG_KEY_HINTS);

  draw_header (win, CONFDLG_LOG_FORMAT, " %s", 11, 1, w2, color_panel_header);
  if ((log_format = get_input_log_format ())) {
    mvwprintw (win, 12, 2, "%.*s", CONF_MENU_W, log_format);
    free (log_format);
  }

  draw_header (win, CONFDLG_DATE_FORMAT, " %s", 14, 1, w2, color_panel_header);
  if ((date_format = get_input_date_format ())) {
    mvwprintw (win, 15, 2, "%.*s", CONF_MENU_W, date_format);
    free (date_format);
  }

  draw_header (win, CONFDLG_TIME_FORMAT, " %s", 17, 1, w2, color_panel_header);
  if ((time_format = get_input_time_format ())) {
    mvwprintw (win, 18, 2, "%.*s", CONF_MENU_W, time_format);
    free (time_format);
  }
}

static const char *
set_formats (char *date_format, char *log_format, char *time_format) {
  if (!time_format && !conf.time_format)
    return ERR_FORMAT_NO_TIME_FMT_DLG;
  if (!date_format && !conf.date_format)
    return ERR_FORMAT_NO_DATE_FMT_DLG;
  if (!log_format && !conf.log_format)
    return ERR_FORMAT_NO_LOG_FMT_DLG;

  if (time_format) {
    free (conf.time_format);
    conf.time_format = unescape_str (time_format);
  }
  if (date_format) {
    free (conf.date_format);
    conf.date_format = unescape_str (date_format);
  }
  if (log_format) {
    free (conf.log_format);
    conf.log_format = unescape_str (log_format);
  }

  if (is_json_log_format (conf.log_format))
    conf.is_json_log_format = 1;

  set_spec_date_format ();
  return NULL;
}

static void
load_confdlg_error (WINDOW *parent_win, char **errors, int nerrors) {
  int c, quit = 1, i = 0;
  int y, x, h = ERR_WIN_HEIGHT, w = ERR_WIN_WIDTH;
  WINDOW *win;
  GMenu *menu;

  getmaxyx (stdscr, y, x);

  win = newwin (h, w, (y - h) / 2, (x - w) / 2);
  keypad (win, TRUE);
  wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

  menu = new_gmenu (win, ERR_MENU_HEIGHT, ERR_MENU_WIDTH, ERR_MENU_Y, ERR_MENU_X);
  menu->size = nerrors;

  menu->items = (GItem *) xcalloc (nerrors, sizeof (GItem));
  for (i = 0; i < nerrors; ++i) {
    menu->items[i].name = alloc_string (errors[i]);
    menu->items[i].checked = 0;
    free (errors[i]);
  }
  free (errors);

  post_gmenu (menu);
  draw_header (win, ERR_FORMAT_HEADER, " %s", 1, 1, w - 2, color_error);
  mvwprintw (win, 2, 2, CONFDLG_DESC);
  wrefresh (win);

  while (quit) {
    c = wgetch (stdscr);
    switch (c) {
    case KEY_DOWN:
      gmenu_driver (menu, REQ_DOWN);
      break;
    case KEY_UP:
      gmenu_driver (menu, REQ_UP);
      break;
    case KEY_RESIZE:
    case 'q':
      quit = 0;
      break;
    }
    wrefresh (win);
  }

  for (i = 0; i < nerrors; ++i)
    free (menu->items[i].name);
  free (menu->items);
  free (menu);

  touchwin (parent_win);
  close_win (win);
  wrefresh (parent_win);
}

int
render_confdlg (Logs *logs, GSpinner *spinner) {
  GMenu *menu;
  WINDOW *win;
  const char *log_err = NULL;
  char *date_format = NULL, *log_format = NULL, *time_format = NULL;
  char *cstm_log, *cstm_date, *cstm_time;
  int c, quit = 1, invalid = 1, y, x, h = CONF_WIN_H, w = CONF_WIN_W;
  int w2 = w - 2;
  size_t i, n, sel;

  static const char *const choices[] = {
    "NCSA Combined Log Format",
    "NCSA Combined Log Format with Virtual Host",
    "Common Log Format (CLF)",
    "Common Log Format (CLF) with Virtual Host",
    "W3C",
    "CloudFront (Download Distribution)",
    "Google Cloud Storage",
    "AWS Elastic Load Balancing (HTTP/S)",
    "Squid Native Format",
    "AWS Simple Storage Service (S3)",
    "CADDY JSON Structured",
    "AWS Application Load Balancer",
    "Traefik CLF flavor"
  };
  n = ARRAY_SIZE (choices);

  getmaxyx (stdscr, y, x);

  win = newwin (h, w, (y - h) / 2, (x - w) / 2);
  keypad (win, TRUE);
  wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

  menu = new_gmenu (win, CONF_MENU_H, CONF_MENU_W, CONF_MENU_Y, CONF_MENU_X);
  menu->size = n;
  menu->selectable = 1;

  menu->items = (GItem *) xcalloc (n, sizeof (GItem));
  for (i = 0; i < n; ++i) {
    menu->items[i].name = alloc_string (choices[i]);
    sel = get_selected_format_idx ();
    menu->items[i].checked = sel == i ? 1 : 0;
  }

  post_gmenu (menu);
  draw_formats (win, w2);
  wrefresh (win);

  while (quit) {
    c = wgetch (stdscr);
    switch (c) {
    case KEY_DOWN:
      gmenu_driver (menu, REQ_DOWN);
      clear_confdlg_status_bar (win, 3, 2, CONF_MENU_W);
      break;
    case KEY_UP:
      gmenu_driver (menu, REQ_UP);
      clear_confdlg_status_bar (win, 3, 2, CONF_MENU_W);
      break;
    case 32:
      gmenu_driver (menu, REQ_SEL);
      clear_confdlg_status_bar (win, 12, 1, CONF_MENU_W);
      clear_confdlg_status_bar (win, 15, 1, CONF_MENU_W);
      clear_confdlg_status_bar (win, 18, 1, CONF_MENU_W);
      if (time_format)
        free (time_format);
      if (date_format)
        free (date_format);
      if (log_format)
        free (log_format);
      for (i = 0; i < n; ++i) {
        if (menu->items[i].checked != 1)
          continue;
        date_format = get_selected_date_str (i);
        log_format = get_selected_format_str (i);
        time_format = get_selected_time_str (i);
        free (set_default_string (win, 12, 2, CONF_MENU_W, log_format));
        free (set_default_string (win, 15, 2, CONF_MENU_W, date_format));
        free (set_default_string (win, 18, 2, CONF_MENU_W, time_format));
        break;
      }
      break;
    case 99:
      clear_confdlg_status_bar (win, 3, 2, CONF_MENU_W);
      wmove (win, 12, 2);
      if (!log_format)
        log_format = get_input_log_format ();
      cstm_log = input_string (win, 12, 2, CONF_MAX_LEN_DLG, log_format, 0, 0);
      if (cstm_log != NULL && *cstm_log != '\0') {
        if (log_format)
          free (log_format);
        log_format = alloc_string (cstm_log);
        free (cstm_log);
      } else {
        if (cstm_log)
          free (cstm_log);
        if (log_format) {
          free (log_format);
          log_format = NULL;
        }
      }
      break;
    case 100:
      clear_confdlg_status_bar (win, 3, 2, CONF_MENU_W);
      wmove (win, 15, 0);
      if (!date_format)
        date_format = get_input_date_format ();
      cstm_date = input_string (win, 15, 2, 14, date_format, 0, 0);
      if (cstm_date != NULL && *cstm_date != '\0') {
        if (date_format)
          free (date_format);
        date_format = alloc_string (cstm_date);
        free (cstm_date);
      } else {
        if (cstm_date)
          free (cstm_date);
        if (date_format) {
          free (date_format);
          date_format = NULL;
        }
      }
      break;
    case 116:
      clear_confdlg_status_bar (win, 3, 2, CONF_MENU_W);
      wmove (win, 15, 0);
      if (!time_format)
        time_format = get_input_time_format ();
      cstm_time = input_string (win, 18, 2, 14, time_format, 0, 0);
      if (cstm_time != NULL && *cstm_time != '\0') {
        if (time_format)
          free (time_format);
        time_format = alloc_string (cstm_time);
        free (cstm_time);
      } else {
        if (cstm_time)
          free (cstm_time);
        if (time_format) {
          free (time_format);
          time_format = NULL;
        }
      }
      break;
    case 274:
    case 0x0a:
    case 0x0d:
    case KEY_ENTER:
      if ((log_err = set_formats (date_format, log_format, time_format)))
        draw_header (win, log_err, " %s", 3, 2, CONF_MENU_W, color_error);
      if (!log_err) {
        char **errors = NULL;
        int nerrors = 0;
        if ((errors = test_format (logs, &nerrors))) {
          invalid = 1;
          load_confdlg_error (win, errors, nerrors);
        } else {
          reset_struct (logs);
          spinner->win = win;
          spinner->y = 3;
          spinner->x = 2;
          spinner->spin_x = CONF_MENU_W;
          spinner->w = CONF_MENU_W;
          spinner->color = color_progress;
          ui_spinner_create (spinner);
          invalid = 0;
          quit = 0;
        }
      }
      break;
    case KEY_RESIZE:
    case 'q':
      quit = 0;
      break;
    }
    pthread_mutex_lock (&spinner->mutex);
    wrefresh (win);
    pthread_mutex_unlock (&spinner->mutex);
  }

  free (time_format);
  free (date_format);
  free (log_format);

  for (i = 0; i < n; ++i)
    free (menu->items[i].name);
  free (menu->items);
  free (menu);

  return invalid ? 1 : 0;
}

static void
scheme_chosen (const char *name) {
  int force = 0;

  free_color_lists ();
  if (strcmp ("Green", name) == 0) {
    conf.color_scheme = STD_GREEN;
    force = 1;
  } else if (strcmp ("Monochrome", name) == 0) {
    conf.color_scheme = MONOCHROME;
    force = 1;
  } else if (strcmp ("Monokai", name) == 0) {
    conf.color_scheme = MONOKAI;
    force = 1;
  } else if (strcmp ("Custom Scheme", name) == 0) {
    force = 0;
  }
  init_colors (force);
}

static const char **
get_color_schemes (size_t *size) {
  const char *choices[] = {
    "Monokai",
    "Monochrome",
    "Green",
    "Custom Scheme"
  };
  int i, j, n = ARRAY_SIZE (choices);
  const char **opts = xmalloc (sizeof (char *) * n);

  for (i = 0, j = 0; i < n; ++i) {
    if (!conf.color_idx && !strcmp ("Custom Scheme", choices[i]))
      continue;
    if (COLORS < 256 && !strcmp ("Monokai", choices[i]))
      continue;
    opts[j++] = choices[i];
  }
  *size = j;

  return opts;
}

void
load_schemes_win (WINDOW *main_win) {
  GMenu *menu;
  WINDOW *win;
  int c, quit = 1;
  size_t i, n = 0;
  int y, x, h = SCHEME_WIN_H, w = SCHEME_WIN_W;
  int w2 = w - 2;
  const char **choices = get_color_schemes (&n);

  getmaxyx (stdscr, y, x);

  win = newwin (h, w, (y - h) / 2, (x - w) / 2);
  keypad (win, TRUE);
  wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

  menu = new_gmenu (win, SCHEME_MENU_H, SCHEME_MENU_W, SCHEME_MENU_Y, SCHEME_MENU_X);
  menu->size = n;

  menu->items = (GItem *) xcalloc (n, sizeof (GItem));
  for (i = 0; i < n; ++i) {
    menu->items[i].name = alloc_string (choices[i]);
    menu->items[i].checked = 0;
  }

  post_gmenu (menu);
  draw_header (win, SCHEMEDLG_HEAD, " %s", 1, 1, w2, color_panel_header);
  mvwprintw (win, 2, 2, SCHEMEDLG_DESC);
  wrefresh (win);

  while (quit) {
    c = wgetch (stdscr);
    switch (c) {
    case KEY_DOWN:
      gmenu_driver (menu, REQ_DOWN);
      break;
    case KEY_UP:
      gmenu_driver (menu, REQ_UP);
      break;
    case 32:
    case 0x0a:
    case 0x0d:
    case KEY_ENTER:
      gmenu_driver (menu, REQ_SEL);
      for (i = 0; i < n; ++i) {
        if (menu->items[i].checked != 1)
          continue;
        scheme_chosen (choices[i]);
        break;
      }
      quit = 0;
      break;
    case KEY_RESIZE:
    case 'q':
      quit = 0;
      break;
    }
    wrefresh (win);
  }

  for (i = 0; i < n; ++i)
    free (menu->items[i].name);
  free (menu->items);
  free (menu);
  free (choices);

  touchwin (main_win);
  close_win (win);
  wrefresh (main_win);
}

static void
swap_modules (int idx1, int idx2) {
  int tmp = module_list[idx1];
  module_list[idx1] = module_list[idx2];
  module_list[idx2] = tmp;
}

void
load_panels_win (WINDOW *main_win) {
  GMenu *menu;
  WINDOW *win;
  int c, quit = 1;
  int i, n = 0;
  int y, x, h = PANELS_WIN_H, w = PANELS_WIN_W;
  int w2 = w - 2;
  int menu_h;
  char *tmp_name = NULL;
  size_t idx = 0;

  getmaxyx (stdscr, y, x);

  FOREACH_MODULE (idx, module_list) {
    n++;
  }

  menu_h = n < 14 ? n : 14;

  win = newwin (h, w, (y - h) / 2, (x - w) / 2);
  keypad (win, TRUE);
  wborder (win, '|', '|', '-', '-', '+', '+', '+', '+');

  menu = new_gmenu (win, menu_h, w - 4, PANELS_MENU_Y, PANELS_MENU_X);
  menu->size = n;
  menu->selectable = 0;

  menu->items = (GItem *) xcalloc (n, sizeof (GItem));

  idx = 0;
  i = 0;
  FOREACH_MODULE (idx, module_list) {
    GModule module = module_list[idx];
    const char *label = module_to_label (module);
    int pos = i + 1;

    menu->items[i].name = xmalloc (snprintf (NULL, 0, "%d. %s", pos, label) + 1);
    sprintf (menu->items[i].name, "%d. %s", pos, label);
    i++;
  }

  post_gmenu (menu);
  draw_header (win, "Reorder Panels", " %s", 1, 1, w2, color_panel_header);
  mvwprintw (win, 2, 2, "Numbers shown are keyboard shortcuts");
  mvwprintw (win, h - 2, 2, "[Up/Down] Navigate [Enter] Move Down [q] Close");
  wrefresh (win);

  while (quit) {
    c = wgetch (stdscr);
    switch (c) {
    case KEY_DOWN:
      gmenu_driver (menu, REQ_DOWN);
      break;
    case KEY_UP:
      gmenu_driver (menu, REQ_UP);
      break;
    case 'w':
    case 'W':
    case 575:
      if (menu->idx > 0) {
        swap_modules (menu->idx, menu->idx - 1);
        tmp_name = menu->items[menu->idx].name;
        menu->items[menu->idx].name = menu->items[menu->idx - 1].name;
        menu->items[menu->idx - 1].name = tmp_name;
        menu->idx--;
        post_gmenu (menu);
      }
      break;
    case 's':
    case 'S':
    case 534:
      if (menu->idx < n - 1) {
        swap_modules (menu->idx, menu->idx + 1);
        tmp_name = menu->items[menu->idx].name;
        menu->items[menu->idx].name = menu->items[menu->idx + 1].name;
        menu->items[menu->idx + 1].name = tmp_name;
        menu->idx++;
        post_gmenu (menu);
      }
      break;
    case KEY_RESIZE:
    case 'q':
      quit = 0;
      break;
    }
    wrefresh (win);
  }

  for (i = 0; i < n; ++i)
    free (menu->items[i].name);
  free (menu->items);
  free (menu);

  touchwin (main_win);
  close_win (win);
  wrefresh (main_win);
}
