/**
 * gwsocket.c -- An interface to send/recv data from/to Web Socket Server
 *    _______       _______            __        __
 *   / ____/ |     / / ___/____  _____/ /_____  / /_
 *  / / __ | | /| / /\__ \/ __ \/ ___/ //_/ _ \/ __/
 * / /_/ / | |/ |/ /___/ / /_/ / /__/ ,< /  __/ /_
 * \____/  |__/|__//____/\____/\___/_/|_|\___/\__/
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

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gwsocket.h"

#include "commons.h"
#include "error.h"
#include "json.h"
#include "parser.h"
#include "settings.h"
#include "websocket.h"
#include "xmalloc.h"

static pthread_t gws_thread;
static pthread_mutex_t gws_mutex;
static WSServer *server = NULL;

/* Write the JSON data to a pipe.
 *
 * If unable to write bytes, -1 is returned.
 * On success, the number of written bytes is returned . */
static int
write_holder (int fd, const char *buf, int len)
{
  int i, ret = 0;

  for (i = 0; i < len;) {
    ret = write (fd, buf + i, len - i);
    if (ret < 0) {
      if (errno == EINTR || errno == EAGAIN)
        continue;
      return -1;
    } else {
      i += ret;
    }
  }

  return i;
}

/* Pack the JSON data into a network byte order and write it to a
 * pipe.
 *
 * On success, 0 is returned . */
int
broadcast_holder (int fd, const char *buf, int len)
{
  char *p = NULL, *ptr = NULL;

  p = calloc (sizeof (uint32_t) * 3, sizeof (char));

  ptr = p;
  ptr += pack_uint32 (ptr, 0);
  ptr += pack_uint32 (ptr, 0x01);
  ptr += pack_uint32 (ptr, len);

  write_holder (fd, p, sizeof (uint32_t) * 3);
  write_holder (fd, buf, len);
  free (p);

  return 0;
}

/* Open the named pipe where the websocket server reads from.
 *
 * If unable to open, -1 is returned.
 * On success, return the new file descriptor is returned . */
int
open_fifo (void)
{
  const char *fifo = "/tmp/wspipein.fifo";
  int fdfifo;

  if ((fdfifo = open (fifo, O_WRONLY | O_NONBLOCK)) == -1)
    return -1;

  return fdfifo;
}

/* Set the self-pipe trick to handle select(2). */
static void
set_self_pipe (void)
{
  /* Initialize self pipe. */
  if (pipe (server->self_pipe) == -1)
    FATAL ("Unable to create pipe: %s.", strerror (errno));

  /* make the read and write pipe non-blocking */
  set_nonblocking (server->self_pipe[0]);
  set_nonblocking (server->self_pipe[1]);
}

/* Close the WebSocket server and clean up. */
void
stop_ws_server (void)
{
  if (server == NULL)
    return;

  /* if it fails to write, force stop */
  if ((write (server->self_pipe[1], "x", 1)) == -1 && errno != EAGAIN)
    ws_stop (server);

  pthread_cancel (gws_thread);
  pthread_join (gws_thread, NULL);
}

/* Start the WebSocket server and initialize default options. */
static void
start_server (void)
{
  if ((server = ws_init ("0.0.0.0", "7890")) == NULL)
    return;

  ws_set_config_strict (1);
  if (conf.addr)
    ws_set_config_host (conf.addr);
  if (conf.port)
    ws_set_config_port (conf.port);
  if (conf.origin)
    ws_set_config_origin (conf.origin);
  set_self_pipe ();

  /* select(2) will block in here */
  ws_start (server);
  fprintf (stderr, "\nStopping WebSocket server...\n");
  ws_stop (server);
}

/* Setup and start the WebSocket thread. */
int
setup_ws_server (void)
{
  int thread;

  if (pthread_mutex_init (&gws_mutex, NULL))
    FATAL ("Failed init gws_thread mutex");

  thread = pthread_create (&(gws_thread), NULL, (void *) &start_server, NULL);
  if (thread)
    FATAL ("Return code from pthread_create(): %d", thread);

  return 0;
}
