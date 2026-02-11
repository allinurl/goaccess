/**
 * goaccess.c -- main log analyzer
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
#define _FILE_OFFSET_BITS    64

#include <assert.h>
#include <ctype.h>
#include <errno.h>

#include <locale.h>

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <fcntl.h>
#include <grp.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>

#include "gkhash.h"

#ifdef HAVE_GEOLOCATION
#include "geoip1.h"
#endif

#include "browsers.h"
#include "csv.h"
#include "error.h"
#include "gdashboard.h"
#include "gdns.h"
#include "gchart.h"
#include "gholder.h"
#include "goaccess.h"
#include "gwsocket.h"
#include "json.h"
#include "options.h"
#include "output.h"
#include "util.h"
#include "websocket.h"
#include "xmalloc.h"

GConf conf = {
  .append_method = 1,
  .append_protocol = 1,
  .chunk_size = 1024,
  .hl_header = 1,
  .jobs = 1,
  .num_tests = 10,
};

/* Loading/Spinner */
GSpinner *parsing_spinner;
/* active reverse dns flag */
int active_gdns = 0;

/* WebSocket server - writer and reader threads */
static GWSWriter *gwswriter;
static GWSReader *gwsreader;
/* Dashboard data structure */
static GDash *dash;
/* Data holder structure */
static GHolder *holder;
/* Old signal mask */
static sigset_t oldset;
/* Curses windows */
static WINDOW *header_win, *main_win;

static int main_win_height = 0;

/* *INDENT-OFF* */
static GScroll gscroll = {
  {
     {0, 0, 0, 0, 1, NULL, 0}, /* VISITORS - note the 1 at the end! */
     {0, 0, 0, 0, 0, NULL, 0}, /* REQUESTS */
     {0, 0, 0, 0, 0, NULL, 0}, /* REQUESTS_STATIC */
     {0, 0, 0, 0, 0, NULL, 0}, /* NOT_FOUND */
     {0, 0, 0, 0, 0, NULL, 0}, /* HOSTS */
     {0, 0, 0, 0, 0, NULL, 0}, /* OS */
     {0, 0, 0, 0, 0, NULL, 0}, /* BROWSERS */
     {0, 0, 0, 0, 0, NULL, 0}, /* VISIT_TIMES */
     {0, 0, 0, 0, 0, NULL, 0}, /* VIRTUAL_HOSTS */
     {0, 0, 0, 0, 0, NULL, 0}, /* REFERRERS */
     {0, 0, 0, 0, 0, NULL, 0}, /* REFERRING_SITES */
     {0, 0, 0, 0, 0, NULL, 0}, /* KEYPHRASES */
     {0, 0, 0, 0, 0, NULL, 0}, /* STATUS_CODES */
     {0, 0, 0, 0, 0, NULL, 0}, /* REMOTE_USER */
     {0, 0, 0, 0, 0, NULL, 0}, /* CACHE_STATUS */
#ifdef HAVE_GEOLOCATION
     {0, 0, 0, 0, 0, NULL, 0}, /* GEO_LOCATION */
     {0, 0, 0, 0, 0, NULL, 0}, /* ASN */
#endif
     {0, 0, 0, 0, 0, NULL, 0}, /* MIME_TYPE */
     {0, 0, 0, 0, 0, NULL, 0}, /* TLS_TYPE */
  },
  0,         /* current module */
  0,         /* main dashboard scroll */
  0,         /* expanded flag */
};
/* *INDENT-ON* */

/* Free malloc'd holder */
static void
house_keeping_holder (void) {
  /* REVERSE DNS THREAD */
  pthread_mutex_lock (&gdns_thread.mutex);

  /* kill dns pthread */
  active_gdns = 0;
  /* clear holder structure */
  free_holder (&holder);
  /* clear reverse dns queue */
  gdns_free_queue ();
  /* clear the whole storage */
  free_storage ();

  pthread_mutex_unlock (&gdns_thread.mutex);
}

/* Free per-item expand state for all modules */
static void
free_scroll_state (void) {
  GModule module;
  size_t idx = 0;
  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    free_item_expanded (&gscroll.module[module]);
  }
}

/* Free malloc'd data across the whole program */
static void
house_keeping (void) {
  house_keeping_holder ();

  /* SCROLL STATE */
  free_scroll_state ();

  /* DASHBOARD */
  if (dash && !conf.output_stdout) {
    free_dashboard (dash);
    reset_find ();
  }

  /* GEOLOCATION */
#ifdef HAVE_GEOLOCATION
  geoip_free ();
  free_country_continent_map ();
#endif

  /* INVALID REQUESTS */
  if (conf.invalid_requests_log) {
    LOG_DEBUG (("Closing invalid requests log.\n"));
    invalid_log_close ();
  }

  /* UNKNOWNS */
  if (conf.unknowns_log) {
    LOG_DEBUG (("Closing unknowns log.\n"));
    unknowns_log_close ();
  }

  /* CONFIGURATION */
  free_formats ();
  free_browsers_hash ();
  if (conf.debug_log) {
    LOG_DEBUG (("Bye.\n"));
    dbg_log_close ();
  }
  if (conf.fifo_in)
    free ((char *) conf.fifo_in);
  if (conf.fifo_out)
    free ((char *) conf.fifo_out);

  /* clear spinner */
  free (parsing_spinner);
  /* free colors */
  free_color_lists ();
  /* free cmd arguments */
  free_cmd_args ();
  /* WebSocket writer */
  free (gwswriter);
  /* WebSocket reader */
  free (gwsreader);
}

static void
cleanup (int ret) {
  /* done, restore tty modes and reset terminal into
   * non-visual mode */
  if (!conf.output_stdout)
    endwin ();

  if (!conf.no_progress)
    fprintf (stdout, "Cleaning up resources...\n");

  /* unable to process valid data */
  if (ret)
    output_logerrors ();

  house_keeping ();
}

/* Drop permissions to the user specified. */
static void
drop_permissions (void) {
  struct passwd *pw;

  errno = 0;
  if ((pw = getpwnam (conf.username)) == NULL) {
    if (errno == 0)
      FATAL ("No such user %s", conf.username);
    FATAL ("Unable to retrieve user %s: %s", conf.username, strerror (errno));
  }

  if (setgroups (1, &pw->pw_gid) == -1)
    FATAL ("setgroups: %s", strerror (errno));
  if (setgid (pw->pw_gid) == -1)
    FATAL ("setgid: %s", strerror (errno));
  if (setuid (pw->pw_uid) == -1)
    FATAL ("setuid: %s", strerror (errno));
}

/* Open the pidfile whose name is specified in the given path and write
 * the daemonized given pid. */
static void
write_pid_file (const char *path, pid_t pid) {
  FILE *pidfile;

  if (!path)
    return;

  if ((pidfile = fopen (path, "w"))) {
    fprintf (pidfile, "%d", pid);
    fclose (pidfile);
  } else {
    FATAL ("Unable to open the specified pid file. %s", strerror (errno));
  }
}

/* Set GoAccess to run as a daemon */
static void
daemonize (void) {
  pid_t pid, sid;
  int fd;

  /* Clone ourselves to make a child */
  pid = fork ();

  if (pid < 0)
    exit (EXIT_FAILURE);
  if (pid > 0) {
    write_pid_file (conf.pidfile, pid);
    printf ("Daemonized GoAccess: %d\n", pid);
    exit (EXIT_SUCCESS);
  }

  umask (0);
  /* attempt to create our own process group */
  sid = setsid ();
  if (sid < 0) {
    LOG_DEBUG (("Unable to setsid: %s.\n", strerror (errno)));
    exit (EXIT_FAILURE);
  }

  /* set the working directory to the root directory.
   * requires the user to specify absolute paths */
  if (chdir ("/") < 0) {
    LOG_DEBUG (("Unable to set chdir: %s.\n", strerror (errno)));
    exit (EXIT_FAILURE);
  }

  /* redirect fd's 0,1,2 to /dev/null */
  /* Note that the user will need to use --debug-file for log output */
  if ((fd = open ("/dev/null", O_RDWR, 0)) == -1) {
    LOG_DEBUG (("Unable to open /dev/null: %s.\n", strerror (errno)));
    exit (EXIT_FAILURE);
  }

  dup2 (fd, STDIN_FILENO);
  dup2 (fd, STDOUT_FILENO);
  dup2 (fd, STDERR_FILENO);
  if (fd > STDERR_FILENO) {
    close (fd);
  }
}

/* Extract data from the given module hash structure and allocate +
 * load data from the hash table into an instance of GHolder */
static void
allocate_holder_by_module (GModule module) {
  GRawData *raw_data;
  uint32_t max_choices = get_max_choices ();
  uint32_t max_choices_sub = get_max_choices_sub ();

  /* extract data from the corresponding hash table */
  raw_data = parse_raw_data (module);
  if (!raw_data) {
    LOG_DEBUG (("raw data is NULL for module: %d.\n", module));
    return;
  }

  load_holder_data (raw_data, holder + module, module, module_sort[module], max_choices, max_choices_sub);
}

/* Iterate over all modules/panels and extract data from hash
 * structures and load it into an instance of GHolder */
static void
allocate_holder (void) {
  size_t idx = 0;

  holder = new_gholder (TOTAL_MODULES);
  FOREACH_MODULE (idx, module_list) {
    allocate_holder_by_module (module_list[idx]);
  }
}

/* Extract data from the modules GHolder structure and load it into
 * the terminal dashboard */
static void
allocate_data_by_module (GModule module, uint32_t col_data) {
  uint32_t size = 0;
  uint32_t max_choices = get_max_choices ();

  dash->module[module].head = module_to_head (module);
  dash->module[module].desc = module_to_desc (module);

  size = holder[module].idx;
  if (gscroll.expanded && module == gscroll.current) {
    size = size > max_choices ? max_choices : holder[module].idx;
  } else {
    size = holder[module].idx > col_data ? col_data : holder[module].idx;
  }

  dash->module[module].alloc_data = size; /* data allocated  */
  dash->module[module].ht_size = holder[module].ht_size; /* hash table size */
  dash->module[module].idx_data = 0;
  dash->module[module].pos_y = 0;

  if (gscroll.expanded && module == gscroll.current)
    dash->module[module].dash_size = DASH_EXPANDED;
  else
    dash->module[module].dash_size = DASH_COLLAPSED;
  dash->total_alloc += dash->module[module].dash_size;

  pthread_mutex_lock (&gdns_thread.mutex);
  load_data_to_dash (&holder[module], dash, module, &gscroll);
  pthread_mutex_unlock (&gdns_thread.mutex);
}

/* Iterate over all modules/panels and extract data from GHolder
 * structure and load it into the terminal dashboard */
static void
allocate_data (void) {
  GModule module;
  uint32_t col_data = get_num_collapsed_data_rows ();
  size_t idx = 0;

  dash = new_gdash ();
  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    allocate_data_by_module (module, col_data);
  }
}

static void
clean_stdscrn (void) {
  int row, col;

  getmaxyx (stdscr, row, col);
  draw_header (stdscr, "", "%s", row - 1, 0, col, color_default);
}

/* A wrapper to render all windows within the dashboard. */
static void
render_screens (uint32_t offset) {
  GColors *color = get_color (COLOR_DEFAULT);
  int row, col;
  char time_str_buf[32];

  getmaxyx (stdscr, row, col);
  term_size (main_win, &main_win_height);

  generate_time ();
  strftime (time_str_buf, sizeof (time_str_buf), "%d/%b/%Y:%T", &now_tm);

  draw_header (stdscr, "", "%s", row - 1, 0, col, color_default);

  wattron (stdscr, color->attr | COLOR_PAIR (color->pair->idx));
  mvaddstr (row - 1, 1, T_HELP_ENTER);
  mvprintw (row - 1, col / 2 - 10, "%" PRIu32 "/r - %s", offset, time_str_buf);
  mvaddstr (row - 1, col - 6 - strlen (T_QUIT), T_QUIT);
  mvprintw (row - 1, col - 5, "%s", GO_VERSION);
  wattroff (stdscr, color->attr | COLOR_PAIR (color->pair->idx));

  refresh ();

  /* call general stats header */
  display_general (header_win, holder);
  wrefresh (header_win);

  /* display active label based on current module */
  update_active_module (header_win, gscroll.current);

  display_content (main_win, dash, &gscroll, holder);
}

/* Collapse the current expanded module */
static int
collapse_current_module (void) {
  if (!gscroll.expanded)
    return 1;

  /* Reset per-item expand state before collapsing */
  reset_item_expanded (&gscroll.module[gscroll.current]);
  gscroll.expanded = 0;
  reset_scroll_offsets (&gscroll);
  free_dashboard (dash);
  allocate_data ();

  return 0;
}

/* Display message at the bottom of the terminal dashboard that panel
 * is disabled */
static void
disabled_panel_msg (GModule module) {
  const char *lbl = module_to_label (module);
  int row, col;

  getmaxyx (stdscr, row, col);
  draw_header (stdscr, lbl, ERR_PANEL_DISABLED, row - 1, 0, col, color_error);
}

/* Set the current module/panel */
static int
set_module_to (GScroll *scrll, GModule module) {
  if (get_module_index (module) == -1) {
    disabled_panel_msg (module);
    return 1;
  }

  /* scroll to panel */
  if (!conf.no_tab_scroll)
    gscroll.dash = get_module_index (module) * DASH_COLLAPSED;

  /* reset expanded module */
  collapse_current_module ();
  scrll->current = module;
  return 0;
}

/* Scroll expanded module or terminal dashboard to the top */
static void
scroll_to_first_line (void) {
  if (!gscroll.expanded)
    gscroll.dash = 0;
  else {
    gscroll.module[gscroll.current].scroll = 0;
    gscroll.module[gscroll.current].offset = 0;
  }
}

/* Scroll expanded module or terminal dashboard to the last row */
static void
scroll_to_last_line (void) {
  int exp_size = get_num_expanded_data_rows ();
  int scrll = 0, offset = 0;

  if (!gscroll.expanded)
    gscroll.dash = dash->total_alloc - main_win_height;
  else {
    scrll = dash->module[gscroll.current].idx_data - 1;
    if (scrll >= exp_size && scrll >= offset + exp_size)
      offset = scrll < exp_size - 1 ? 0 : scrll - exp_size + 1;
    gscroll.module[gscroll.current].scroll = scrll;
    gscroll.module[gscroll.current].offset = offset;
  }
}

/* Load the user-agent window given the selected IP */
static void
load_ip_agent_list (void) {
  int type_ip = 0;
  /* make sure we have a valid IP */
  int sel = gscroll.module[gscroll.current].scroll;
  GDashData item = { 0 };

  if (dash->module[HOSTS].holder_size == 0)
    return;

  item = dash->module[HOSTS].data[sel];
  if (!invalid_ipaddr (item.metrics->data, &type_ip))
    load_agent_list (main_win, item.metrics->data);
}

/* Toggle expand/collapse of the selected item's children within expanded panel.
 * direction: 1 = expand (show children), 0 = collapse (hide children) */
static void
toggle_selected_item_expand (int direction) {
  GModule mod = gscroll.current;
  int scroll_pos = gscroll.module[mod].scroll;
  GDashModule *dmod = &dash->module[mod];
  int nfi;
  uint8_t new_state;

  if (scroll_pos < 0 || scroll_pos >= dmod->idx_data)
    return;

  nfi = dmod->data[scroll_pos].node_full_idx;

  /* Only toggle if the item actually has children */
  if (!dmod->data[scroll_pos].has_children)
    return;

  if (nfi < 0 || nfi >= gscroll.module[mod].item_expanded_size)
    return;

  new_state = direction ? 1 : 0;
  if (gscroll.module[mod].item_expanded[nfi] == new_state)
    return;

  gscroll.module[mod].item_expanded[nfi] = new_state;

  /* When collapsing, keep scroll on the same item (which stays visible).
   * After rebuild, the selected item will have moved to a new flat position
   * because collapsed children are removed. Find its new position. */
  {
    int old_scroll = scroll_pos;
    int target_nfi = nfi;

    /* Rebuild dashboard to reflect changed visibility */
    free_dashboard (dash);
    allocate_data ();

    /* Find the new flat position of the toggled item */
    dmod = &dash->module[mod];
    {
      int k;
      for (k = 0; k < dmod->idx_data; k++) {
        if (dmod->data[k].node_full_idx == target_nfi) {
          gscroll.module[mod].scroll = k;
          if (k < gscroll.module[mod].offset)
            gscroll.module[mod].offset = k;
          return;
        }
      }
    }
    /* Fallback: clamp to last item */
    if (dmod->idx_data > 0) {
      gscroll.module[mod].scroll = dmod->idx_data - 1;
      (void) old_scroll;
    }
  }
}

/* Expand the selected module */
static void
expand_current_module (void) {
  if (gscroll.expanded && gscroll.current == HOSTS) {
    load_ip_agent_list ();
    return;
  }

  /* Already expanded -- toggle expand on the selected item */
  if (gscroll.expanded) {
    toggle_selected_item_expand (1);
    return;
  }

  reset_scroll_offsets (&gscroll);
  gscroll.expanded = 1;

  free_holder_by_module (&holder, gscroll.current);
  free_dashboard (dash);
  allocate_holder_by_module (gscroll.current);

  /* Initialize per-node expand state -- all expanded by default.
   * Size = total nodes in the tree (roots + all sub-items). */
  {
    int total_nodes = holder[gscroll.current].idx + holder[gscroll.current].sub_items_size;
    init_item_expanded (&gscroll.module[gscroll.current], total_nodes);
  }

  allocate_data ();
}

/* Expand the clicked module/panel given the Y event coordinate. */
static int
expand_module_from_ypos (int y) {
  /* ignore header/footer clicks */
  if (y < MAX_HEIGHT_HEADER || y == LINES - 1)
    return 1;

  if (set_module_from_mouse_event (&gscroll, dash, y))
    return 1;

  reset_scroll_offsets (&gscroll);
  gscroll.expanded = 1;

  free_holder_by_module (&holder, gscroll.current);
  free_dashboard (dash);
  allocate_holder_by_module (gscroll.current);

  /* Initialize per-node expand state -- all expanded by default */
  {
    int total_nodes = holder[gscroll.current].idx + holder[gscroll.current].sub_items_size;
    init_item_expanded (&gscroll.module[gscroll.current], total_nodes);
  }

  allocate_data ();

  return 0;
}

/* Expand the clicked module/panel */
static int
expand_on_mouse_click (void) {
  int ok_mouse;
  MEVENT event;

  ok_mouse = getmouse (&event);
  if (!conf.mouse_support || ok_mouse != OK)
    return 1;

  if (event.bstate & BUTTON1_CLICKED)
    return expand_module_from_ypos (event.y);
  return 1;
}

/* Scroll up terminal dashboard */
static void
scroll_up_dashboard (void) {
  gscroll.dash--;
}

/* Scroll down expanded module to the last row */
static void
scroll_down_expanded_module (void) {
  int exp_size = get_num_expanded_data_rows ();
  int *scroll_ptr, *offset_ptr;
  int max_scroll;

  scroll_ptr = &gscroll.module[gscroll.current].scroll;
  offset_ptr = &gscroll.module[gscroll.current].offset;

  if (!gscroll.expanded)
    return;

  max_scroll = dash->module[gscroll.current].idx_data - 1;

  /* Don't scroll past the last item */
  if (*scroll_ptr >= max_scroll)
    return;

  /* Increment scroll position */
  ++(*scroll_ptr);

  /* Adjust offset if we're scrolling beyond the visible area
   * Keep the selection visible by ensuring it's within the window */
  if (*scroll_ptr >= *offset_ptr + exp_size) {
    ++(*offset_ptr);
  }
}

/* Scroll up expanded module */
static void
scroll_up_expanded_module (void) {
  int *scroll_ptr, *offset_ptr;

  scroll_ptr = &gscroll.module[gscroll.current].scroll;
  offset_ptr = &gscroll.module[gscroll.current].offset;

  if (!gscroll.expanded)
    return;

  if (*scroll_ptr <= 0)
    return;

  --(*scroll_ptr);

  /* Adjust offset if selection goes above visible area */
  if (*scroll_ptr < *offset_ptr)
    --(*offset_ptr);
}

/* Page down expanded module */
static void
page_down_module (void) {
  int exp_size = get_num_expanded_data_rows ();
  int *scroll_ptr, *offset_ptr;
  int max_scroll;

  scroll_ptr = &gscroll.module[gscroll.current].scroll;
  offset_ptr = &gscroll.module[gscroll.current].offset;

  if (!gscroll.expanded)
    return;

  max_scroll = dash->module[gscroll.current].idx_data - 1;

  /* Move down by page */
  *scroll_ptr += exp_size;

  /* Clamp to maximum */
  if (*scroll_ptr > max_scroll)
    *scroll_ptr = max_scroll;

  /* Adjust offset to keep selection visible */
  if (*scroll_ptr >= *offset_ptr + exp_size) {
    *offset_ptr = *scroll_ptr - exp_size + 1;
  }

  /* Make sure offset doesn't go beyond valid range */
  if (*offset_ptr + exp_size > max_scroll + 1) {
    *offset_ptr = max_scroll - exp_size + 1;
    if (*offset_ptr < 0)
      *offset_ptr = 0;
  }
}

/* Page up expanded module */
static void
page_up_module (void) {
  int exp_size = get_num_expanded_data_rows ();
  int *scroll_ptr, *offset_ptr;

  scroll_ptr = &gscroll.module[gscroll.current].scroll;
  offset_ptr = &gscroll.module[gscroll.current].offset;

  if (!gscroll.expanded)
    return;

  /* Move up by page */
  *scroll_ptr -= exp_size;

  /* Clamp to minimum */
  if (*scroll_ptr < 0)
    *scroll_ptr = 0;

  /* Adjust offset to keep selection visible */
  if (*scroll_ptr < *offset_ptr) {
    *offset_ptr = *scroll_ptr;
  }
}

/* Create a new find dialog window and render it. Upon closing the
 * window, dashboard is refreshed. */
static int
render_search_dialog (int search) {
  if (render_find_dialog (main_win, &gscroll))
    return 1;

  pthread_mutex_lock (&gdns_thread.mutex);
  search = perform_next_find (holder, &gscroll);
  pthread_mutex_unlock (&gdns_thread.mutex);
  if (search != 0)
    return 1;

  free_dashboard (dash);
  allocate_data ();

  return 0;
}

/* Search for the next occurrence within the dashboard structure */
static int
search_next_match (int search) {
  pthread_mutex_lock (&gdns_thread.mutex);
  search = perform_next_find (holder, &gscroll);
  pthread_mutex_unlock (&gdns_thread.mutex);
  if (search != 0)
    return 1;

  free_dashboard (dash);
  allocate_data ();
  return 0;
}

/* Update holder structure and dashboard screen */
static void
tail_term (void) {
  pthread_mutex_lock (&gdns_thread.mutex);
  free_holder (&holder);
  pthread_cond_broadcast (&gdns_thread.not_empty);
  pthread_mutex_unlock (&gdns_thread.mutex);

  free_dashboard (dash);
  allocate_holder ();
  allocate_data ();

  term_size (main_win, &main_win_height);
}

static void
tail_html (void) {
  char *json = NULL;

  pthread_mutex_lock (&gdns_thread.mutex);
  free_holder (&holder);
  pthread_cond_broadcast (&gdns_thread.not_empty);
  pthread_mutex_unlock (&gdns_thread.mutex);

  allocate_holder ();

  pthread_mutex_lock (&gdns_thread.mutex);
  json = get_json (holder, 1);
  pthread_mutex_unlock (&gdns_thread.mutex);

  if (json == NULL)
    return;

  pthread_mutex_lock (&gwswriter->mutex);
  broadcast_holder (gwswriter->fd, json, strlen (json));
  pthread_mutex_unlock (&gwswriter->mutex);
  free (json);
}

/* Fast-forward latest JSON data when client connection is opened. */
static void
fast_forward_client (int listener) {
  char *json = NULL;

  pthread_mutex_lock (&gdns_thread.mutex);
  json = get_json (holder, 1);
  pthread_mutex_unlock (&gdns_thread.mutex);

  if (json == NULL)
    return;

  pthread_mutex_lock (&gwswriter->mutex);
  send_holder_to_client (gwswriter->fd, listener, json, strlen (json));
  pthread_mutex_unlock (&gwswriter->mutex);
  free (json);
}

/* Start reading data coming from the client side through the
 * WebSocket server. */
void
read_client (void *ptr_data) {
  GWSReader *reader = (GWSReader *) ptr_data;

  /* check we have a fifo for reading */
  if (reader->fd == -1)
    return;

  pthread_mutex_lock (&reader->mutex);
  set_self_pipe (reader->self_pipe);
  pthread_mutex_unlock (&reader->mutex);

  while (1) {
    /* poll(2) will block */
    if (read_fifo (reader, fast_forward_client))
      break;
  }
  close (reader->fd);
}

/* Parse tailed lines */
static void
parse_tail_follow (GLog *glog, GFileHandle *fh) {
  GLogItem *logitem = NULL;
#ifdef WITH_GETLINE
  char *buf = NULL;
#else
  char buf[LINE_BUFFER] = { 0 };
#endif

  glog->bytes = 0;

#ifdef WITH_GETLINE
  while ((buf = gfile_getline (fh)) != NULL) {
#else
  while (gfile_gets (buf, LINE_BUFFER, fh) != NULL) {
#endif
    pthread_mutex_lock (&gdns_thread.mutex);
    if ((parse_line (glog, buf, 0, &logitem)) == 0 && logitem != NULL)
      process_log (logitem);
    if (logitem != NULL) {
      free_glog (logitem);
      logitem = NULL;
    }
    pthread_mutex_unlock (&gdns_thread.mutex);

    glog->bytes += strlen (buf);
#ifdef WITH_GETLINE
    free (buf);
#endif

    /* If the ingress rate is greater than MAX_BATCH_LINES,
     * then we break and allow to re-render the UI */
    if (++glog->read % MAX_BATCH_LINES == 0)
      break;
  }
}

static void
verify_inode (GFileHandle *fh, GLog *glog) {
  struct stat fdstat;

  if (stat (glog->props.filename, &fdstat) == -1)
    FATAL ("Unable to stat the specified log file '%s'. %s", glog->props.filename,
           strerror (errno));

  glog->props.size = fdstat.st_size;

  /* Either the log got smaller, probably was truncated so start reading from 0
   * and reset snippet.
   * If the log changed its inode, more likely the log was rotated, so we set
   * the initial snippet for the new log for future iterations */
  if (fdstat.st_ino != glog->props.inode || glog->snippet[0] == '\0' || 0 == glog->props.size) {
    glog->length = glog->bytes = 0;
    set_initial_persisted_data (glog, fh, glog->props.filename);
  }

  glog->props.inode = fdstat.st_ino;
}

/* Check if a file is gzipped by examining magic bytes or extension
 * Returns 1 if gzipped, 0 otherwise */
static int
is_gzipped_file_check (const char *filename) {
  FILE *fp;
  unsigned char magic[2];
  int result = 0;
  size_t len;

  /* Quick check: does it end in .gz? */
  len = strlen (filename);
  if (len > 3 && strcmp (filename + len - 3, ".gz") == 0)
    return 1;

  /* Double-check by reading magic bytes */
  if ((fp = fopen (filename, "rb")) == NULL)
    return 0;

  if (fread (magic, 1, 2, fp) == 2) {
    /* gzip magic number is 0x1f 0x8b */
    if (magic[0] == 0x1f && magic[1] == 0x8b)
      result = 1;
  }

  fclose (fp);
  return result;
}

/* Process appended log data
 *
 * If nothing changed, 0 is returned.
 * If log file changed, 1 is returned. */
static int
perform_tail_follow (GLog *glog) {
  GFileHandle *fh = NULL;
  char buf[READ_BYTES + 1] = { 0 };
  uint16_t len = 0;
  uint64_t length = 0;

  if (glog->props.filename[0] == '-' && glog->props.filename[1] == '\0') {
    /* For stdin pipe, we need to wrap the FILE* into a GFileHandle */
    fh = calloc (1, sizeof (GFileHandle));
    if (!fh)
      return 0;
    fh->fp = glog->pipe;
#ifdef HAVE_ZLIB
    fh->is_gzipped = 0;
    fh->gzfp = NULL;
#endif

    parse_tail_follow (glog, fh);

    /* did we read something from the pipe? */
    if (0 == glog->bytes) {
      free (fh);
      return 0;
    }
    glog->length += glog->bytes;
    free (fh);
    goto out;
  }

  /* Skip tailing gzipped files - they are static archives and should not be monitored
   * for changes in real-time mode. Only regular log files should be tailed. */
  if (is_gzipped_file_check (glog->props.filename)) {
    return 0;
  }

  length = file_size (glog->props.filename);

  /* file hasn't changed */
  /* ###NOTE: This assumes the log file being read can be of smaller size, e.g.,
   * rotated/truncated file or larger when data is appended */
  if (length == glog->length)
    return 0;

  if (!(fh = gfile_open (glog->props.filename, "r")))
    FATAL ("Unable to read the specified log file '%s'. %s", glog->props.filename,
           strerror (errno));

  verify_inode (fh, glog);

  len = MIN (glog->snippetlen, length);
  /* This is not ideal, but maybe the only reliable way to know if the
   * current log looks different than our first read/parse */
  if ((gfile_read (buf, len, 1, fh)) != 1 && gfile_error (fh))
    FATAL ("Unable to read the specified log file '%s'", glog->props.filename);

  /* For the case where the log got larger since the last iteration, we attempt
   * to compare the first READ_BYTES against the READ_BYTES we had since the last
   * parse. If it's different, then it means the file may got truncated but grew
   * faster than the last iteration (odd, but possible), so we read from 0* */
  if (glog->snippet[0] != '\0' && buf[0] != '\0' && memcmp (glog->snippet, buf, len) != 0)
    glog->length = glog->bytes = 0;

  if (!gfile_seek (fh, glog->length, SEEK_SET))
    parse_tail_follow (glog, fh);

  gfile_close (fh);

  glog->length += glog->bytes;

  /* insert the inode of the file parsed and the last line parsed */
  if (glog->props.inode) {
    glog->lp.line = glog->read;
    glog->lp.size = glog->props.size;
    ht_insert_last_parse (glog->props.inode, &glog->lp);
  }

out:
  return 1;
}

/* Loop over and perform a follow for the given logs */
static void
tail_loop_html (Logs *logs) {
  struct timespec refresh = {
    .tv_sec = conf.html_refresh ? conf.html_refresh : HTML_REFRESH,
    .tv_nsec = 0,
  };
  int i = 0, ret = 0;

  while (1) {
    if (conf.stop_processing)
      break;

    for (i = 0, ret = 0; i < logs->size; ++i)
      ret |= perform_tail_follow (&logs->glog[i]); /* 0.2 secs */

    if (1 == ret)
      tail_html ();

    if (nanosleep (&refresh, NULL) == -1 && errno != EINTR)
      FATAL ("nanosleep: %s", strerror (errno));
  }
}

/* Entry point to start processing the HTML output */
static void
process_html (Logs *logs, const char *filename) {
  /* render report */
  pthread_mutex_lock (&gdns_thread.mutex);
  output_html (holder, filename);
  pthread_mutex_unlock (&gdns_thread.mutex);

  /* not real time? */
  if (!conf.real_time_html)
    return;
  /* ignore loading from disk */
  if (logs->load_from_disk_only)
    return;

  pthread_mutex_lock (&gwswriter->mutex);
  gwswriter->fd = open_fifoin ();
  pthread_mutex_unlock (&gwswriter->mutex);

  /* open fifo for write */
  if (gwswriter->fd == -1)
    return;

  set_ready_state ();
  tail_loop_html (logs);
  close (gwswriter->fd);
}

/* Iterate over available panels and advance the panel pointer. */
static int
next_module (void) {
  int next = -1;

  if ((next = get_next_module (gscroll.current)) == -1)
    return 1;

  gscroll.current = next;
  if (!conf.no_tab_scroll)
    gscroll.dash = get_module_index (gscroll.current) * DASH_COLLAPSED;

  return 0;
}

/* Iterate over available panels and rewind the panel pointer. */
static int
previous_module (void) {
  int prev = -1;

  if ((prev = get_prev_module (gscroll.current)) == -1)
    return 1;

  gscroll.current = prev;
  if (!conf.no_tab_scroll)
    gscroll.dash = get_module_index (gscroll.current) * DASH_COLLAPSED;

  return 0;
}

/* Perform several curses operations upon resizing the terminal. */
static void
window_resize (void) {
  endwin ();
  refresh ();
  werase (header_win);
  werase (main_win);
  werase (stdscr);
  term_size (main_win, &main_win_height);
  refresh ();
}

/* Create a new sort dialog window and render it. Upon closing the
 * window, dashboard is refreshed. */
static void
render_sort_dialog (void) {
  load_sort_win (main_win, gscroll.current, &module_sort[gscroll.current]);

  pthread_mutex_lock (&gdns_thread.mutex);
  free_holder (&holder);
  pthread_cond_broadcast (&gdns_thread.not_empty);
  pthread_mutex_unlock (&gdns_thread.mutex);

  free_dashboard (dash);
  allocate_holder ();
  allocate_data ();
}

static void
term_tail_logs (Logs *logs) {
  struct timespec ts = {.tv_sec = 0,.tv_nsec = 200000000 }; /* 0.2 seconds */
  uint32_t offset = 0;
  int i, ret;

  for (i = 0, ret = 0; i < logs->size; ++i)
    ret |= perform_tail_follow (&logs->glog[i]);

  if (1 == ret) {
    tail_term ();
    offset = *logs->processed - logs->offset;
    render_screens (offset);
  }
  if (nanosleep (&ts, NULL) == -1 && errno != EINTR) {
    FATAL ("nanosleep: %s", strerror (errno));
  }
}

static int
cycle_metric (GScroll *scroll, GHolder *holders, GModule mod, int direction) {
  int current_metric, found = 0, attempts = 0;
  int available_metrics[CHART_METRIC_COUNT];
  int num_available = get_available_metrics (mod, available_metrics);

  if (num_available == 0)
    return 0;

  current_metric = scroll->module[mod].current_metric;

  while (attempts < CHART_METRIC_COUNT && !found) {
    if (direction > 0)
      current_metric = (current_metric + 1) % CHART_METRIC_COUNT;
    else
      current_metric = (current_metric - 1 + CHART_METRIC_COUNT) % CHART_METRIC_COUNT;

    for (int i = 0; i < num_available; i++) {
      if (available_metrics[i] == current_metric) {
        if (metric_has_data (&holders[mod], current_metric)) {
          found = 1;
          break;
        }
      }
    }
    attempts++;
  }

  if (found) {
    scroll->module[mod].current_metric = current_metric;
    return 1;
  }

  return 0;
}

/* Interfacing with the keyboard */
static void
get_keys (Logs *logs) {
  int search = 0;
  int c, quit = 1;
  uint32_t offset = 0;

  struct sigaction act, oldact;

  /* Change the action for SIGINT to SIG_IGN and block Ctrl+c
   * before entering the subdialog */
  act.sa_handler = SIG_IGN;
  sigemptyset (&act.sa_mask);
  act.sa_flags = 0;

  while (quit) {
    if (conf.stop_processing)
      break;

    offset = *logs->processed - logs->offset;
    c = wgetch (stdscr);
    switch (c) {
    case 'q':  /* quit */
      if (!gscroll.expanded) {
        quit = 0;
        break;
      }
      if (collapse_current_module () == 0)
        render_screens (offset);
      break;
    case KEY_F (1):
    case '?':
    case 'h':
      sigaction (SIGINT, &act, &oldact);
      load_help_popup (main_win);
      sigaction (SIGINT, &oldact, NULL);
      render_screens (offset);
      break;
    case 49:   /* 1 - jump to first panel */
    case 50:   /* 2 - jump to second panel */
    case 51:   /* 3 */
    case 52:   /* 4 */
    case 53:   /* 5 */
    case 54:   /* 6 */
    case 55:   /* 7 */
    case 56:   /* 8 */
    case 57:   /* 9 */
    case 48:   /* 0 - jump to tenth panel */
      {
        int panel_idx = (c == 48) ? 9 : (c - 49); /* 0 = 10th panel (index 9) */
        int num_modules = get_num_modules ();

        if (panel_idx < num_modules) {
          GModule target = module_list[panel_idx];
          if (set_module_to (&gscroll, target) == 0)
            render_screens (offset);
        }
      }
      break;
    case 9:    /* TAB */
      /* reset expanded module */
      collapse_current_module ();
      if (next_module () == 0)
        render_screens (offset);
      break;
    case 353:  /* Shift TAB */
      /* reset expanded module */
      collapse_current_module ();
      if (previous_module () == 0)
        render_screens (offset);
      break;
    case 'g':  /* g = top */
      scroll_to_first_line ();
      display_content (main_win, dash, &gscroll, holder);
      break;
    case 'G':  /* G = down */
      scroll_to_last_line ();
      display_content (main_win, dash, &gscroll, holder);
      break;
      /* expand dashboard module */
    case KEY_RIGHT:
    case 0x0a:
    case 0x0d:
    case 32:   /* ENTER */
    case 79:   /* o */
    case 111:  /* O */
    case KEY_ENTER:
      expand_current_module ();
      display_content (main_win, dash, &gscroll, holder);
      break;
    case '+':  /* expand selected item's children */
      if (gscroll.expanded) {
        toggle_selected_item_expand (1);
        display_content (main_win, dash, &gscroll, holder);
      }
      break;
    case '-':  /* collapse selected item's children */
      if (gscroll.expanded) {
        toggle_selected_item_expand (0);
        display_content (main_win, dash, &gscroll, holder);
      }
      break;
    case KEY_DOWN: /* scroll main dashboard */
      if ((gscroll.dash + main_win_height) < dash->total_alloc) {
        gscroll.dash++;
        display_content (main_win, dash, &gscroll, holder);
      }
      break;
    case KEY_MOUSE: /* handles mouse events */
      if (expand_on_mouse_click () == 0)
        render_screens (offset);
      break;
    case 106:  /* j - DOWN expanded module */
      scroll_down_expanded_module ();
      display_content (main_win, dash, &gscroll, holder);
      break;
      /* scroll up main_win */
    case KEY_UP:
      if (gscroll.dash > 0) {
        scroll_up_dashboard ();
        display_content (main_win, dash, &gscroll, holder);
      }
      break;
    case 2:    /* ^ b - page up */
    case 339:  /* ^ PG UP */
      page_up_module ();
      display_content (main_win, dash, &gscroll, holder);
      break;
    case 6:    /* ^ f - page down */
    case 338:  /* ^ PG DOWN */
      page_down_module ();
      display_content (main_win, dash, &gscroll, holder);
      break;
    case 107:  /* k - UP expanded module */
      scroll_up_expanded_module ();
      display_content (main_win, dash, &gscroll, holder);
      break;
    case 'n':
      if (search_next_match (search) == 0)
        render_screens (offset);
      break;
    case '/':
      sigaction (SIGINT, &act, &oldact);
      if (render_search_dialog (search) == 0)
        render_screens (offset);
      sigaction (SIGINT, &oldact, NULL);
      break;
    case 'p':  /* reorder panels */
    case 'P':
      sigaction (SIGINT, &act, &oldact);
      load_panels_win (main_win);
      sigaction (SIGINT, &oldact, NULL);

      /* Rebuild dashboard with new panel order */
      pthread_mutex_lock (&gdns_thread.mutex);
      free_holder (&holder);
      pthread_cond_broadcast (&gdns_thread.not_empty);
      pthread_mutex_unlock (&gdns_thread.mutex);

      free_dashboard (dash);
      allocate_holder ();
      allocate_data ();
      render_screens (offset);
      break;
    case 'r':  /* toggle reverse bars */
    case 'R':
      {
        GModule mod = gscroll.current;
        gscroll.module[mod].reverse_bars = !gscroll.module[mod].reverse_bars;
        display_content (main_win, dash, &gscroll, holder);
      }
      break;
    case 'm':  /* cycle metrics forward */
      {
        GModule mod = gscroll.current;
        if (cycle_metric (&gscroll, holder, mod, +1))
          display_content (main_win, dash, &gscroll, holder);
      }
      break;

    case 'M':  /* cycle metrics backward */
      {
        GModule mod = gscroll.current;
        if (cycle_metric (&gscroll, holder, mod, -1))
          display_content (main_win, dash, &gscroll, holder);
      }
      break;
      break;
    case 'l':  /* toggle log scale */
    case 'L':
      {
        GModule mod = gscroll.current;
        gscroll.module[mod].use_log_scale = !gscroll.module[mod].use_log_scale;

        /* Refresh display whether expanded or collapsed */
        display_content (main_win, dash, &gscroll, holder);
      }
      break;

    case 99:   /* c */
      if (conf.no_color)
        break;

      sigaction (SIGINT, &act, &oldact);
      load_schemes_win (main_win);
      sigaction (SIGINT, &oldact, NULL);

      free_dashboard (dash);
      allocate_data ();
      set_wbkgd (main_win, header_win);
      render_screens (offset);
      break;
    case 115:  /* s */
      sigaction (SIGINT, &act, &oldact);
      render_sort_dialog ();
      sigaction (SIGINT, &oldact, NULL);

      render_screens (offset);
      break;
    case 269:
    case KEY_RESIZE:
      window_resize ();
      render_screens (offset);
      break;
    default:
      if (logs->load_from_disk_only)
        break;
      term_tail_logs (logs);
      break;
    }
  }
}

/* Store accumulated processing time
 * Note: As we store with time_t second resolution,
 * if elapsed time == 0, we will bump it to 1.
 */
static void
set_accumulated_time (void) {
  time_t elapsed = end_proc - start_proc;
  elapsed = (!elapsed) ? !elapsed : elapsed;
  ht_inc_cnt_overall ("processing_time", elapsed);
}

/* Execute the following calls right before we start the main
 * processing/parsing loop */
static void
init_processing (void) {
  /* perform some additional checks before parsing panels */
  verify_panels ();

  init_storage ();
  insert_methods_protocols ();
  set_spec_date_format ();

  if ((!conf.skip_term_resolver && !conf.output_stdout) ||
      (conf.enable_html_resolver && conf.real_time_html))
    gdns_thread_create ();
}

/* Determine the type of output, i.e., JSON, CSV, HTML */
static void
standard_output (Logs *logs) {
  char *csv = NULL, *json = NULL, *html = NULL;

  /* CSV */
  if (find_output_type (&csv, "csv", 1) == 0)
    output_csv (holder, csv);
  /* JSON */
  if (find_output_type (&json, "json", 1) == 0)
    output_json (holder, json);
  /* HTML */
  if (find_output_type (&html, "html", 1) == 0 || conf.output_format_idx == 0) {
    if (conf.real_time_html)
      setup_ws_server (gwswriter, gwsreader);
    process_html (logs, html);
  }

  free (csv);
  free (html);
  free (json);
}

/* Output to a terminal */
static void
curses_output (Logs *logs) {
  allocate_data ();

  clean_stdscrn ();
  render_screens (0);
  /* will loop in here */
  get_keys (logs);
}

/* Set locale */
static void
set_locale (void) {
  char *loc_ctype;

  setlocale (LC_ALL, "");
#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
#endif

  loc_ctype = getenv ("LC_CTYPE");
  if (loc_ctype != NULL)
    setlocale (LC_CTYPE, loc_ctype);
  else if ((loc_ctype = getenv ("LC_ALL")))
    setlocale (LC_CTYPE, loc_ctype);
  else
    setlocale (LC_CTYPE, "");
}

/* Attempt to get the current name of a terminal or fallback to /dev/tty
 *
 * On error, -1 is returned
 * On success, the new file descriptor is returned */
static int
open_term (char **buf) {
  const char *term = "/dev/tty";

  if (!isatty (STDERR_FILENO) || (term = ttyname (STDERR_FILENO)) == 0) {
    if (!isatty (STDOUT_FILENO) || (term = ttyname (STDOUT_FILENO)) == 0) {
      if (!isatty (STDIN_FILENO) || (term = ttyname (STDIN_FILENO)) == 0) {
        term = "/dev/tty";
      }
    }
  }
  *buf = xstrdup (term);

  return open (term, O_RDONLY);
}

/* Determine if reading from a pipe, and duplicate file descriptors so
 * it doesn't get in the way of curses' normal reading stdin for
 * wgetch() */
static FILE *
set_pipe_stdin (void) {
  char *term = NULL;
  FILE *pipe = stdin;
  int term_fd = -1;
  int pipe_fd = -1;

  /* If unable to open a terminal, yet data is being piped, then it's
   * probably from the cron, or when running as a user that can't open a
   * terminal. In that case it's still important to set the pipe as
   * non-blocking.
   *
   * Note: If used from the cron, it will require the
   * user to use a single dash to parse piped data such as:
   * cat access.log | goaccess - */
  if ((term_fd = open_term (&term)) == -1)
    goto out1;

  if ((pipe_fd = dup (fileno (stdin))) == -1)
    FATAL ("Unable to dup stdin: %s", strerror (errno));

  pipe = fdopen (pipe_fd, "r");
  if (freopen (term, "r", stdin) == 0)
    FATAL ("Unable to open input from TTY");
  if (fileno (stdin) != 0)
    (void) dup2 (fileno (stdin), 0);

  add_dash_filename ();

out1:

  /* no need to set it as non-blocking since we are simply outputting a
   * static report */
  if (conf.output_stdout && !conf.real_time_html)
    goto out2;

  /* Using select(), poll(), or epoll(), etc may be a better choice... */
  if (pipe_fd == -1)
    pipe_fd = fileno (pipe);
  if (fcntl (pipe_fd, F_SETFL, fcntl (pipe_fd, F_GETFL, 0) | O_NONBLOCK) == -1)
    FATAL ("Unable to set fd as non-blocking: %s.", strerror (errno));

out2:

  free (term);

  return pipe;
}

/* Determine if we are getting data from the stdin, and where are we
 * outputting to. */
static void
set_io (FILE **pipe) {
  /* For backwards compatibility, check if we are not outputting to a
   * terminal or if an output format was supplied */
  if (!isatty (STDOUT_FILENO) || conf.output_format_idx > 0)
    conf.output_stdout = 1;
  /* dup fd if data piped */
  if (!isatty (STDIN_FILENO))
    *pipe = set_pipe_stdin ();
}

/* Process command line options and set some default options. */
static void
parse_cmd_line (int argc, char **argv) {
  read_option_args (argc, argv);
  set_default_static_files ();
}

static void
handle_signal_action (GO_UNUSED int sig_number) {
  if (sig_number == SIGINT)
    fprintf (stderr, "\nSIGINT caught!\n");
  else if (sig_number == SIGTERM)
    fprintf (stderr, "\nSIGTERM caught!\n");
  else if (sig_number == SIGQUIT)
    fprintf (stderr, "\nSIGQUIT caught!\n");
  else
    fprintf (stderr, "\nSignal %d caught!\n", sig_number);
  fprintf (stderr, "Closing GoAccess...\n");

  if (conf.output_stdout && conf.real_time_html)
    stop_ws_server (gwswriter, gwsreader);
  conf.stop_processing = 1;
}

static void
setup_thread_signals (void) {
  struct sigaction act;

  act.sa_handler = handle_signal_action;
  sigemptyset (&act.sa_mask);
  act.sa_flags = 0;

  sigaction (SIGINT, &act, NULL);
  sigaction (SIGTERM, &act, NULL);
  sigaction (SIGQUIT, &act, NULL);
  signal (SIGPIPE, SIG_IGN);

  /* Restore old signal mask for the main thread */
  pthread_sigmask (SIG_SETMASK, &oldset, NULL);
}

static void
block_thread_signals (void) {
  /* Avoid threads catching SIGINT/SIGPIPE/SIGTERM/SIGQUIT and handle them in
   * main thread */
  sigset_t sigset;
  sigemptyset (&sigset);
  sigaddset (&sigset, SIGINT);
  sigaddset (&sigset, SIGPIPE);
  sigaddset (&sigset, SIGTERM);
  sigaddset (&sigset, SIGQUIT);
  pthread_sigmask (SIG_BLOCK, &sigset, &oldset);
}

/* Initialize various types of data. */
static Logs *
initializer (void) {
  int i;
  FILE *pipe = NULL;
  Logs *logs;

  /* drop permissions right away */
  if (conf.username)
    drop_permissions ();

  /* then initialize modules and set */
  gscroll.current = init_modules ();
  /* setup to use the current locale */
  set_locale ();

  parse_browsers_file ();

#ifdef HAVE_GEOLOCATION
  init_geoip ();
#endif

  set_io (&pipe);

  /* init glog */
  if (!(logs = init_logs (conf.filenames_idx)))
    FATAL (ERR_NO_DATA_PASSED);

  set_signal_data (logs);

  for (i = 0; i < logs->size; ++i)
    if (logs->glog[i].props.filename[0] == '-' && logs->glog[i].props.filename[1] == '\0')
      logs->glog[i].pipe = pipe;

  /* init parsing spinner */
  parsing_spinner = new_gspinner ();
  parsing_spinner->processed = &(logs->processed);
  parsing_spinner->filename = &(logs->filename);

  /* init reverse lookup thread */
  gdns_init ();

  /* init random number generator */
  srand (getpid ());
  init_pre_storage (logs);

  return logs;
}

static char *
generate_fifo_name (void) {
  char fname[RAND_FN];
  const char *tmp;
  char *path;
  size_t len;

  if ((tmp = getenv ("TMPDIR")) == NULL)
    tmp = "/tmp";

  memset (fname, 0, sizeof (fname));
  genstr (fname, RAND_FN - 1);

  len = snprintf (NULL, 0, "%s/goaccess_fifo_%s", tmp, fname) + 1;
  path = xmalloc (len);
  snprintf (path, len, "%s/goaccess_fifo_%s", tmp, fname);

  return path;
}

static int
spawn_ws (void) {
  gwswriter = new_gwswriter ();
  gwsreader = new_gwsreader ();

  if (!conf.fifo_in)
    conf.fifo_in = generate_fifo_name ();
  if (!conf.fifo_out)
    conf.fifo_out = generate_fifo_name ();

  /* open fifo for read */
  if ((gwsreader->fd = open_fifoout ()) == -1) {
    LOG (("Unable to open FIFO for read.\n"));
    return 1;
  }

  if (conf.daemonize)
    daemonize ();

  return 0;
}

static void
set_standard_output (void) {
  int html = 0;

  /* HTML */
  if (find_output_type (NULL, "html", 0) == 0 || conf.output_format_idx == 0)
    html = 1;

  /* Spawn WebSocket server threads */
  if (html && conf.real_time_html) {
    if (spawn_ws ())
      return;
  }
  setup_thread_signals ();

  /* Spawn progress spinner thread */
  ui_spinner_create (parsing_spinner);
}

/* Set up curses. */
static void
set_curses (Logs *logs, int *quit) {
  const char *err_log = NULL;

  setup_thread_signals ();
  set_input_opts ();
  if (conf.no_color || has_colors () == FALSE) {
    conf.color_scheme = NO_COLOR;
    conf.no_color = 1;
  } else {
    start_color ();
  }
  init_colors (0);
  init_windows (&header_win, &main_win);
  set_curses_spinner (parsing_spinner);

  /* Display configuration dialog if missing formats and not piping data in */
  if (!conf.read_stdin && (verify_formats () || conf.load_conf_dlg)) {
    refresh ();
    *quit = render_confdlg (logs, parsing_spinner);
    clear ();
  }
  /* Piping data in without log/date/time format */
  else if (conf.read_stdin && (err_log = verify_formats ())) {
    FATAL ("%s", err_log);
  }
  /* straight parsing */
  else {
    ui_spinner_create (parsing_spinner);
  }
}

/* Where all begins... */
int
main (int argc, char **argv) {
  Logs *logs = NULL;
  int quit = 0, ret = 0;

  block_thread_signals ();
  setup_sigsegv_handler ();

  /* command line/config options */
  verify_global_config (argc, argv);
  parse_conf_file (&argc, &argv);
  parse_cmd_line (argc, argv);

  logs = initializer ();

  /* ignore outputting, process only */
  if (conf.process_and_exit) {
  }
  /* set stdout */
  else if (conf.output_stdout) {
    set_standard_output ();
  }
  /* set curses */
  else {
    set_curses (logs, &quit);
  }

  /* no log/date/time format set */
  if (quit)
    goto clean;

  init_processing ();

  /* main processing event */
  time (&start_proc);

  if ((ret = parse_log (logs, 0))) {
    end_spinner ();
    goto clean;
  }

  if (conf.stop_processing)
    goto clean;
  logs->offset = *logs->processed;

  parse_initial_sort ();
  allocate_holder ();

  end_spinner ();
  time (&end_proc);

  set_accumulated_time ();
  if (conf.process_and_exit) {
  }
  /* stdout */
  else if (conf.output_stdout) {
    standard_output (logs);
  }
  /* curses */
  else {
    curses_output (logs);
  }

  /* clean */
clean:
  cleanup (ret);

  return ret ? EXIT_FAILURE : EXIT_SUCCESS;
}
