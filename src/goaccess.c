/**
 * goaccess.c -- main log analyzer
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2020 Gerardo Orellana <hello @ goaccess.io>
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

#include "gkhash.h"

#ifdef HAVE_GEOLOCATION
#include "geoip1.h"
#endif

#include "browsers.h"
#include "csv.h"
#include "error.h"
#include "gdashboard.h"
#include "gdns.h"
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
  .hl_header = 1,
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
/* Log line properties structure */
static Logs *logs;
/* Old signal mask */
static sigset_t oldset;
/* Curses windows */
static WINDOW *header_win, *main_win;

static int main_win_height = 0;

/* *INDENT-OFF* */
static GScroll gscroll = {
  {
    {0, 0}, /* visitors    {scroll, offset} */
    {0, 0}, /* requests    {scroll, offset} */
    {0, 0}, /* req static  {scroll, offset} */
    {0, 0}, /* not found   {scroll, offset} */
    {0, 0}, /* hosts       {scroll, offset} */
    {0, 0}, /* os          {scroll, offset} */
    {0, 0}, /* browsers    {scroll, offset} */
    {0, 0}, /* visit times {scroll, offset} */
    {0, 0}, /* referrers   {scroll, offset} */
    {0, 0}, /* ref sites   {scroll, offset} */
    {0, 0}, /* keywords    {scroll, offset} */
#ifdef HAVE_GEOLOCATION
    {0, 0}, /* geolocation {scroll, offset} */
#endif
    {0, 0}, /* status      {scroll, offset} */
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

/* Free malloc'd data across the whole program */
static void
house_keeping (void) {
  house_keeping_holder ();

  /* DASHBOARD */
  if (dash && !conf.output_stdout) {
    free_dashboard (dash);
    reset_find ();
  }

  /* GEOLOCATION */
#ifdef HAVE_GEOLOCATION
  geoip_free ();
#endif

  /* LOGGER */
  free_logs (logs);

  /* INVALID REQUESTS */
  if (conf.invalid_requests_log) {
    LOG_DEBUG (("Closing invalid requests log.\n"));
    invalid_log_close ();
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

  /* unable to process valid data */
  if (ret)
    output_logerrors (logs);

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

  /* extract data from the corresponding hash table */
  raw_data = parse_raw_data (module);
  if (!raw_data) {
    LOG_DEBUG (("raw data is NULL for module: %d.\n", module));
    return;
  }

  load_holder_data (raw_data, holder + module, module, module_sort[module]);
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
allocate_data_by_module (GModule module, int col_data) {
  int size = 0, max_choices = get_max_choices ();

  dash->module[module].head = module_to_head (module);
  dash->module[module].desc = module_to_desc (module);

  size = holder[module].idx;
  if (gscroll.expanded && module == gscroll.current) {
    size = size > max_choices ? max_choices : holder[module].idx;
  } else {
    size = holder[module].idx > col_data ? col_data : holder[module].idx;
  }

  dash->module[module].alloc_data = size;       /* data allocated  */
  dash->module[module].ht_size = holder[module].ht_size;        /* hash table size */
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
  int col_data = get_num_collapsed_data_rows ();
  size_t idx = 0;

  dash = new_gdash ();
  FOREACH_MODULE (idx, module_list) {
    module = module_list[idx];
    allocate_data_by_module (module, col_data);
  }
}

/* A wrapper to render all windows within the dashboard. */
static void
render_screens (void) {
  GColors *color = get_color (COLOR_DEFAULT);
  int row, col, chg = 0;
  char time_str_buf[32];

  getmaxyx (stdscr, row, col);
  term_size (main_win, &main_win_height);

  generate_time ();
  strftime (time_str_buf, sizeof (time_str_buf), "%a %b %e %T %Y", &now_tm);
  chg = *logs->processed - logs->offset;

  draw_header (stdscr, "", "%s", row - 1, 0, col, color_default);

  wattron (stdscr, color->attr | COLOR_PAIR (color->pair->idx));
  mvaddstr (row - 1, 1, T_HELP_ENTER);
  mvprintw (row - 1, col / 2 - 10, "%d - %s", chg, time_str_buf);
  mvaddstr (row - 1, col - 6 - strlen (T_QUIT), T_QUIT);
  mvprintw (row - 1, col - 5, "%s", GO_VERSION);
  wattroff (stdscr, color->attr | COLOR_PAIR (color->pair->idx));

  refresh ();

  /* call general stats header */
  display_general (header_win, holder);
  wrefresh (header_win);

  /* display active label based on current module */
  update_active_module (header_win, gscroll.current);

  display_content (main_win, dash, &gscroll);
}

/* Collapse the current expanded module */
static void
collapse_current_module (void) {
  if (!gscroll.expanded)
    return;

  gscroll.expanded = 0;
  reset_scroll_offsets (&gscroll);
  free_dashboard (dash);
  allocate_data ();
  render_screens ();
}

/* Display message a the bottom of the terminal dashboard that panel
 * is disabled */
static void
disabled_panel_msg (GModule module) {
  const char *lbl = module_to_label (module);
  int row, col;

  getmaxyx (stdscr, row, col);
  draw_header (stdscr, lbl, ERR_PANEL_DISABLED, row - 1, 0, col, color_error);
}

/* Set the current module/panel */
static void
set_module_to (GScroll * scrll, GModule module) {
  if (get_module_index (module) == -1) {
    disabled_panel_msg (module);
    return;
  }

  /* scroll to panel */
  if (!conf.no_tab_scroll)
    gscroll.dash = get_module_index (module) * DASH_COLLAPSED;

  /* reset expanded module */
  collapse_current_module ();
  scrll->current = module;
  render_screens ();
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
  GDashData item = dash->module[HOSTS].data[sel];

  if (!invalid_ipaddr (item.metrics->data, &type_ip))
    load_agent_list (main_win, item.metrics->data);
}

/* Expand the selected module */
static void
expand_current_module (void) {
  if (gscroll.expanded && gscroll.current == HOSTS) {
    load_ip_agent_list ();
    return;
  }

  /* expanded, nothing to do... */
  if (gscroll.expanded)
    return;

  reset_scroll_offsets (&gscroll);
  gscroll.expanded = 1;

  free_holder_by_module (&holder, gscroll.current);
  free_dashboard (dash);
  allocate_holder_by_module (gscroll.current);
  allocate_data ();
}

/* Expand the clicked module/panel given the Y event coordinate. */
static void
expand_module_from_ypos (int y) {
  /* ignore header/footer clicks */
  if (y < MAX_HEIGHT_HEADER || y == LINES - 1)
    return;

  if (set_module_from_mouse_event (&gscroll, dash, y))
    return;

  reset_scroll_offsets (&gscroll);
  gscroll.expanded = 1;

  free_holder_by_module (&holder, gscroll.current);
  free_dashboard (dash);
  allocate_holder_by_module (gscroll.current);
  allocate_data ();

  render_screens ();
}

/* Expand the clicked module/panel */
static void
expand_on_mouse_click (void) {
  int ok_mouse;
  MEVENT event;

  ok_mouse = getmouse (&event);
  if (!conf.mouse_support || ok_mouse != OK)
    return;

  if (event.bstate & BUTTON1_CLICKED)
    expand_module_from_ypos (event.y);
}

/* Scroll dowm expanded module to the last row */
static void
scroll_down_expanded_module (void) {
  int exp_size = get_num_expanded_data_rows ();
  int *scroll_ptr, *offset_ptr;

  scroll_ptr = &gscroll.module[gscroll.current].scroll;
  offset_ptr = &gscroll.module[gscroll.current].offset;

  if (!gscroll.expanded)
    return;
  if (*scroll_ptr >= dash->module[gscroll.current].idx_data - 1)
    return;
  ++(*scroll_ptr);
  if (*scroll_ptr >= exp_size && *scroll_ptr >= *offset_ptr + exp_size)
    ++(*offset_ptr);
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
  if (*scroll_ptr < *offset_ptr)
    --(*offset_ptr);
}

/* Scroll up terminal dashboard */
static void
scroll_up_dashboard (void) {
  gscroll.dash--;
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
  /* decrease scroll and offset by exp_size */
  *scroll_ptr -= exp_size;
  if (*scroll_ptr < 0)
    *scroll_ptr = 0;

  if (*scroll_ptr < *offset_ptr)
    *offset_ptr -= exp_size;
  if (*offset_ptr <= 0)
    *offset_ptr = 0;
}

/* Page down expanded module */
static void
page_down_module (void) {
  int exp_size = get_num_expanded_data_rows ();
  int *scroll_ptr, *offset_ptr;

  scroll_ptr = &gscroll.module[gscroll.current].scroll;
  offset_ptr = &gscroll.module[gscroll.current].offset;

  if (!gscroll.expanded)
    return;

  *scroll_ptr += exp_size;
  if (*scroll_ptr >= dash->module[gscroll.current].idx_data - 1)
    *scroll_ptr = dash->module[gscroll.current].idx_data - 1;
  if (*scroll_ptr >= exp_size && *scroll_ptr >= *offset_ptr + exp_size)
    *offset_ptr += exp_size;
  if (*offset_ptr + exp_size >= dash->module[gscroll.current].idx_data - 1)
    *offset_ptr = dash->module[gscroll.current].idx_data - exp_size;
  if (*scroll_ptr < exp_size - 1)
    *offset_ptr = 0;
}

/* Create a new find dialog window and render it. Upon closing the
 * window, dashboard is refreshed. */
static void
render_search_dialog (int search) {
  if (render_find_dialog (main_win, &gscroll))
    return;

  pthread_mutex_lock (&gdns_thread.mutex);
  search = perform_next_find (holder, &gscroll);
  pthread_mutex_unlock (&gdns_thread.mutex);
  if (search != 0)
    return;

  free_dashboard (dash);
  allocate_data ();
  render_screens ();
}

/* Search for the next occurrence within the dashboard structure */
static void
search_next_match (int search) {
  pthread_mutex_lock (&gdns_thread.mutex);
  search = perform_next_find (holder, &gscroll);
  pthread_mutex_unlock (&gdns_thread.mutex);
  if (search != 0)
    return;

  free_dashboard (dash);
  allocate_data ();
  render_screens ();
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
  render_screens ();
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
  json = get_json (holder, 0);
  pthread_mutex_unlock (&gdns_thread.mutex);

  if (json == NULL)
    return;

  broadcast_holder (gwswriter->fd, json, strlen (json));
  free (json);
}

/* Fast-forward latest JSON data when client connection is opened. */
static void
fast_forward_client (int listener) {
  char *json = NULL;

  pthread_mutex_lock (&gdns_thread.mutex);
  json = get_json (holder, 0);
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
  fd_set rfds, wfds;

  FD_ZERO (&rfds);
  FD_ZERO (&wfds);

  /* check we have a fifo for reading */
  if (reader->fd == -1)
    return;

  pthread_mutex_lock (&reader->mutex);
  set_self_pipe (reader->self_pipe);
  pthread_mutex_unlock (&reader->mutex);

  while (1) {
    /* select(2) will block */
    if (read_fifo (reader, rfds, wfds, fast_forward_client))
      break;
  }
  close (reader->fd);
}

/* Parse tailed lines */
static void
parse_tail_follow (GLog * glog, FILE * fp) {
#ifdef WITH_GETLINE
  char *buf = NULL;
#else
  char buf[LINE_BUFFER] = { 0 };
#endif

  glog->bytes = 0;
#ifdef WITH_GETLINE
  while ((buf = fgetline (fp)) != NULL) {
#else
  while (fgets (buf, LINE_BUFFER, fp) != NULL) {
#endif
    pthread_mutex_lock (&gdns_thread.mutex);
    pre_process_log (glog, buf, 0);
    pthread_mutex_unlock (&gdns_thread.mutex);
    glog->bytes += strlen (buf);
#ifdef WITH_GETLINE
    free (buf);
#endif
    glog->read++;
  }
}

/* Process appended log data */
static void
perform_tail_follow (GLog * glog) {
  FILE *fp = NULL;
  struct stat fdstat;
  char buf[READ_BYTES + 1] = { 0 };
  uint16_t len = 0;
  uint64_t length = 0;

  if (glog->filename[0] == '-' && glog->filename[1] == '\0') {
    parse_tail_follow (glog, glog->pipe);
    glog->length += glog->bytes;
    goto out;
  }
  if (logs->load_from_disk_only)
    return;

  length = file_size (glog->filename);

  /* file hasn't changed */
  /* ###NOTE: This assumes the log file being read can be of smaller size, e.g.,
   * rotated/truncated file or larger when data is appended */
  if (length == glog->length)
    return;

  if (!(fp = fopen (glog->filename, "r")))
    FATAL ("Unable to read the specified log file '%s'. %s", glog->filename, strerror (errno));

  /* insert the inode of the file parsed and the last line parsed */
  if (stat (glog->filename, &fdstat) == 0) {
    glog->inode = fdstat.st_ino;
    glog->size = fdstat.st_size;
  }

  len = MIN (glog->snippetlen, length);
  /* This is not ideal, but maybe the only way reliable way to know if the
   * current log looks different than our first read/parse */
  if ((fread (buf, len, 1, fp)) != 1 && ferror (fp))
    FATAL ("Unable to fread the specified log file '%s'", glog->filename);

  /* Either the log got smaller, probably was truncated so start reading from 0.
   * For the case where the log got larger since the last iteration, we attempt
   * to compare the first READ_BYTES against the READ_BYTES we had since the last
   * parse. If it's different, then it means the file may got truncated but grew
   * faster than the last iteration (odd, but possible), so we read from 0* */
  if (glog->snippet[0] != '\0' && buf[0] != '\0' && memcmp (glog->snippet, buf, len) != 0)
    glog->length = glog->bytes = 0;

  if (!fseeko (fp, glog->length, SEEK_SET))
    parse_tail_follow (glog, fp);
  fclose (fp);

  glog->length += glog->bytes;

  /* insert the inode of the file parsed and the last line parsed */
  if (glog->inode) {
    glog->lp.line = glog->read;
    glog->lp.size = fdstat.st_size;
    ht_insert_last_parse (glog->inode, glog->lp);
  }

out:

  if (!conf.output_stdout)
    tail_term ();
  else
    tail_html ();

  usleep (200000);      /* 0.2 seconds */
}

/* Entry point to start processing the HTML output */
static void
process_html (const char *filename) {
  int i = 0;

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
  while (1) {
    if (conf.stop_processing)
      break;

    for (i = 0; i < logs->size; ++i)
      perform_tail_follow (&logs->glog[i]);     /* 0.2 secs */
    usleep (800000);    /* 0.8 secs */
  }
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
  render_screens ();
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
  render_screens ();
}

/* Interfacing with the keyboard */
static void
get_keys (void) {
  int search = 0;
  int c, quit = 1, i;

  while (quit) {
    if (conf.stop_processing)
      break;
    c = wgetch (stdscr);
    switch (c) {
    case 'q':  /* quit */
      if (!gscroll.expanded) {
        quit = 0;
        break;
      }
      collapse_current_module ();
      break;
    case KEY_F (1):
    case '?':
    case 'h':
      load_help_popup (main_win);
      render_screens ();
      break;
    case 49:   /* 1 */
      /* reset expanded module */
      set_module_to (&gscroll, VISITORS);
      break;
    case 50:   /* 2 */
      /* reset expanded module */
      set_module_to (&gscroll, REQUESTS);
      break;
    case 51:   /* 3 */
      /* reset expanded module */
      set_module_to (&gscroll, REQUESTS_STATIC);
      break;
    case 52:   /* 4 */
      /* reset expanded module */
      set_module_to (&gscroll, NOT_FOUND);
      break;
    case 53:   /* 5 */
      /* reset expanded module */
      set_module_to (&gscroll, HOSTS);
      break;
    case 54:   /* 6 */
      /* reset expanded module */
      set_module_to (&gscroll, OS);
      break;
    case 55:   /* 7 */
      /* reset expanded module */
      set_module_to (&gscroll, BROWSERS);
      break;
    case 56:   /* 8 */
      /* reset expanded module */
      set_module_to (&gscroll, VISIT_TIMES);
      break;
    case 57:   /* 9 */
      /* reset expanded module */
      set_module_to (&gscroll, VIRTUAL_HOSTS);
      break;
    case 48:   /* 0 */
      /* reset expanded module */
      set_module_to (&gscroll, REFERRERS);
      break;
    case 33:   /* shift + 1 */
      /* reset expanded module */
      set_module_to (&gscroll, REFERRING_SITES);
      break;
    case 34:   /* shift + 2 */
      /* reset expanded module */
      set_module_to (&gscroll, KEYPHRASES);
      break;
    case 35:   /* Shift + 3 */
      /* reset expanded module */
      set_module_to (&gscroll, STATUS_CODES);
      break;
    case 36:   /* Shift + 3 */
      /* reset expanded module */
      set_module_to (&gscroll, REMOTE_USER);
      break;
#ifdef HAVE_GEOLOCATION
    case 37:   /* Shift + 4 */
      /* reset expanded module */
      set_module_to (&gscroll, GEO_LOCATION);
      break;
#endif
    case 38:   /* Shift + 5 */
      /* reset expanded module */
      set_module_to (&gscroll, CACHE_STATUS);
      break;
    case 9:    /* TAB */
      /* reset expanded module */
      collapse_current_module ();
      if (next_module () == 0)
        render_screens ();
      break;
    case 353:  /* Shift TAB */
      /* reset expanded module */
      collapse_current_module ();
      if (previous_module () == 0)
        render_screens ();
      break;
    case 'g':  /* g = top */
      scroll_to_first_line ();
      display_content (main_win, dash, &gscroll);
      break;
    case 'G':  /* G = down */
      scroll_to_last_line ();
      display_content (main_win, dash, &gscroll);
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
      display_content (main_win, dash, &gscroll);
      break;
    case KEY_DOWN:     /* scroll main dashboard */
      if ((gscroll.dash + main_win_height) < dash->total_alloc) {
        gscroll.dash++;
        display_content (main_win, dash, &gscroll);
      }
      break;
    case KEY_MOUSE:    /* handles mouse events */
      expand_on_mouse_click ();
      break;
    case 106:  /* j - DOWN expanded module */
      scroll_down_expanded_module ();
      display_content (main_win, dash, &gscroll);
      break;
      /* scroll up main_win */
    case KEY_UP:
      if (gscroll.dash > 0) {
        scroll_up_dashboard ();
        display_content (main_win, dash, &gscroll);
      }
      break;
    case 2:    /* ^ b - page up */
    case 339:  /* ^ PG UP */
      page_up_module ();
      display_content (main_win, dash, &gscroll);
      break;
    case 6:    /* ^ f - page down */
    case 338:  /* ^ PG DOWN */
      page_down_module ();
      display_content (main_win, dash, &gscroll);
      break;
    case 107:  /* k - UP expanded module */
      scroll_up_expanded_module ();
      display_content (main_win, dash, &gscroll);
      break;
    case 'n':
      search_next_match (search);
      break;
    case '/':
      render_search_dialog (search);
      break;
    case 99:   /* c */
      if (conf.no_color)
        break;
      load_schemes_win (main_win);
      free_dashboard (dash);
      allocate_data ();
      set_wbkgd (main_win, header_win);
      render_screens ();
      break;
    case 115:  /* s */
      render_sort_dialog ();
      break;
    case 269:
    case KEY_RESIZE:
      window_resize ();
      break;
    default:
      for (i = 0; i < logs->size; ++i)
        perform_tail_follow (&logs->glog[i]);
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
  /* initialize storage */
  parsing_spinner->label = "SETTING UP STORAGE";
  init_storage ();
  set_spec_date_format ();
}

/* Determine the type of output, i.e., JSON, CSV, HTML */
static void
standard_output (void) {
  char *csv = NULL, *json = NULL, *html = NULL;

  /* CSV */
  if (find_output_type (&csv, "csv", 1) == 0)
    output_csv (holder, csv);
  /* JSON */
  if (find_output_type (&json, "json", 1) == 0)
    output_json (holder, json);
  /* HTML */
  if (find_output_type (&html, "html", 1) == 0 || conf.output_format_idx == 0)
    process_html (html);

  free (csv);
  free (html);
  free (json);
}

/* Output to a terminal */
static void
curses_output (void) {
  allocate_data ();
  if (!conf.skip_term_resolver)
    gdns_thread_create ();

  render_screens ();
  /* will loop in here */
  get_keys ();
}

/* Set locale */
static void
set_locale (void) {
  char *loc_ctype;

  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

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
static void
set_pipe_stdin (void) {
  char *term = NULL;
  FILE *pipe = stdin;
  int fd1, fd2, i;

  /* If unable to open a terminal, yet data is being piped, then it's
   * probably from the cron.
   *
   * Note: If used from the cron, it will require the
   * user to use a single dash to parse piped data such as:
   * cat access.log | goaccess - */
  if ((fd1 = open_term (&term)) == -1)
    goto out;

  if ((fd2 = dup (fileno (stdin))) == -1)
    FATAL ("Unable to dup stdin: %s", strerror (errno));

  pipe = fdopen (fd2, "r");
  if (freopen (term, "r", stdin) == 0)
    FATAL ("Unable to open input from TTY");
  if (fileno (stdin) != 0)
    (void) dup2 (fileno (stdin), 0);

  /* no need to set it as non-blocking since we are simply outputting a
   * static report */
  if (conf.output_stdout && !conf.real_time_html)
    goto out;

  /* Using select(), poll(), or epoll(), etc may be a better choice... */
  if (fcntl (fd2, F_SETFL, fcntl (fd2, F_GETFL, 0) | O_NONBLOCK) == -1)
    FATAL ("Unable to set fd as non-blocking: %s.", strerror (errno));
out:

  for (i = 0; i < logs->size; ++i)
    if (logs->glog[i].filename[0] == '-' && logs->glog[i].filename[1] == '\0')
      logs->glog[i].pipe = pipe;

  free (term);
}

/* Determine if we are getting data from the stdin, and where are we
 * outputting to. */
static void
set_io (void) {
  /* For backwards compatibility, check if we are not outputting to a
   * terminal or if an output format was supplied */
  if (!isatty (STDOUT_FILENO) || conf.output_format_idx > 0)
    conf.output_stdout = 1;
  /* dup fd if data piped */
  if (!isatty (STDIN_FILENO))
    set_pipe_stdin ();
  /* No data piped, no file was used and not loading from disk */
  //if (!conf.filenames_idx && !conf.read_stdin && !conf.load_from_disk)
  //  cmd_help ();
}

/* Process command line options and set some default options. */
static void
parse_cmd_line (int argc, char **argv) {
  read_option_args (argc, argv);
  set_default_static_files ();
}

static void
handle_signal_action (GO_UNUSED int sig_number) {
  fprintf (stderr, "\nSIGINT caught!\n");
  fprintf (stderr, "Closing GoAccess...\n");

  stop_ws_server (gwswriter, gwsreader);
  conf.stop_processing = 1;

  if (!conf.output_stdout) {
    cleanup (EXIT_SUCCESS);
    exit (EXIT_SUCCESS);
  }
}

static void
setup_thread_signals (void) {
  struct sigaction act;

  act.sa_handler = handle_signal_action;
  sigemptyset (&act.sa_mask);
  act.sa_flags = 0;

  sigaction (SIGINT, &act, NULL);
  sigaction (SIGTERM, &act, NULL);
  signal (SIGPIPE, SIG_IGN);

  /* Restore old signal mask for the main thread */
  pthread_sigmask (SIG_SETMASK, &oldset, NULL);
}

static void
block_thread_signals (void) {
  /* Avoid threads catching SIGINT/SIGPIPE/SIGTERM and handle them in
   * main thread */
  sigset_t sigset;
  sigemptyset (&sigset);
  sigaddset (&sigset, SIGINT);
  sigaddset (&sigset, SIGPIPE);
  sigaddset (&sigset, SIGTERM);
  pthread_sigmask (SIG_BLOCK, &sigset, &oldset);
}

/* Initialize various types of data. */
static void
initializer (void) {
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

  if (!isatty (STDIN_FILENO))
    add_dash_filename ();

  /* init glog */
  if (!(logs = init_logs (conf.filenames_idx)))
    FATAL ("No input data was provided to parse.");

  set_io ();
  set_signal_data (logs);

  /* init parsing spinner */
  parsing_spinner = new_gspinner ();
  parsing_spinner->processed = &(logs->processed);
  parsing_spinner->filename = &(logs->filename);

  /* init random number generator */
  srand (getpid ());
}

static char *
generate_fifo_name (void) {
  char fname[RAND_FN];
  const char *tmp = NULL;
  char *path = NULL;

  if ((tmp = getenv ("TMPDIR")) == NULL)
    tmp = "/tmp";

  memset (fname, 0, sizeof (fname));
  genstr (fname, RAND_FN - 1);

  path = xmalloc (snprintf (NULL, 0, "%s/goaccess_fifo_%s", tmp, fname) + 1);
  sprintf (path, "%s/goaccess_fifo_%s", tmp, fname);

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
  setup_ws_server (gwswriter, gwsreader);

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
set_curses (int *quit) {
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
  int quit = 0, ret = 0;

  block_thread_signals ();
  setup_sigsegv_handler ();

  /* command line/config options */
  verify_global_config (argc, argv);
  parse_conf_file (&argc, &argv);
  parse_cmd_line (argc, argv);

  initializer ();

  /* ignore outputting, process only */
  if (conf.process_and_exit) {
  }
  /* set stdout */
  else if (conf.output_stdout) {
    set_standard_output ();
  }
  /* set curses */
  else {
    set_curses (&quit);
  }

  /* no log/date/time format set */
  if (quit)
    goto clean;

  init_processing ();

  /* main processing event */
  time (&start_proc);
  parsing_spinner->label = "PARSING";

  if ((ret = parse_log (logs, 0))) {
    end_spinner ();
    goto clean;
  }

  if (conf.stop_processing)
    goto clean;
  logs->offset = *logs->processed;

  /* init reverse lookup thread */
  gdns_init ();
  parse_initial_sort ();
  allocate_holder ();

  end_spinner ();
  time (&end_proc);

  set_accumulated_time ();
  if (conf.process_and_exit) {
  }
  /* stdout */
  else if (conf.output_stdout) {
    standard_output ();
  }
  /* curses */
  else {
    curses_output ();
  }

  /* clean */
clean:
  cleanup (ret);

  return ret ? EXIT_FAILURE : EXIT_SUCCESS;
}
