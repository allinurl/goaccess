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
#include "goaccess.h"
#include "json.h"
#include "parser.h"
#include "settings.h"
#include "websocket.h"
#include "xmalloc.h"

static WSServer *server = NULL;

/* Allocate memory for a new GWSThread instance.
 *
 * On success, the newly allocated GWSThread is returned . */
GWSThread *
new_gwserver (void)
{
  GWSThread *srv = xmalloc (sizeof (GWSThread));
  memset (srv, 0, sizeof *srv);

  return srv;
}

void
free_gwserver (GWSThread * gwserver)
{
  if (gwserver)
    free (gwserver);
}

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

/* Clear an incoming FIFO packet and header data. */
static void
clear_fifo_packet (GWSThread * gwserver)
{
  memset (gwserver->hdr, 0, sizeof (gwserver->hdr));
  gwserver->hlen = 0;

  if (gwserver->packet == NULL)
    return;

  if (gwserver->packet->data)
    free (gwserver->packet->data);
  free (gwserver->packet);
  gwserver->packet = NULL;
}

/* Pack the JSON data into a network byte order and writes it to a
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

/* Pack the JSON data into a network byte order and write it to a
 * pipe.
 *
 * On success, 0 is returned . */
int
send_holder_to_client (int fd, int listener, const char *buf, int len)
{
  char *p = NULL, *ptr = NULL;

  p = calloc (sizeof (uint32_t) * 3, sizeof (char));

  ptr = p;
  ptr += pack_uint32 (ptr, listener);
  ptr += pack_uint32 (ptr, 0x01);
  ptr += pack_uint32 (ptr, len);

  write_holder (fd, p, sizeof (uint32_t) * 3);
  write_holder (fd, buf, len);
  free (p);

  return 0;
}

/* Attempt to read data from the named pipe on strict mode.
 * Note: For now it only reads on new connections, i.e., onopen.
 *
 * If there's less data than requested, 0 is returned
 * If the thread is done, 1 is returned */
int
read_fifo (GWSThread * gwserver, fd_set rfds, fd_set wfds, void (*f) (int, int))
{
  WSPacket **pa = &gwserver->packet;
  char *ptr;
  int bytes = 0, readh = 0, need = 0, fd = gwserver->pipeout, max = 0;
  uint32_t listener = 0, type = 0, size = 0;

  FD_ZERO (&rfds);
  FD_ZERO (&wfds);
  /* self-pipe trick to stop the event loop */
  FD_SET (gwserver->self_pipe[0], &rfds);
  /* fifo */
  FD_SET (fd, &rfds);
  max = MAX (fd, gwserver->self_pipe[0]);

  if (select (max + 1, &rfds, &wfds, NULL, NULL) == -1) {
    switch (errno) {
    case EINTR:
      break;
    default:
      FATAL ("Unable to select: %s.", strerror (errno));
    }
  }
  /* handle self-pipe trick */
  if (FD_ISSET (gwserver->self_pipe[0], &rfds))
    return 1;
  if (!FD_ISSET (fd, &rfds)) {
    LOG (("No file descriptor set on read_message()\n"));
    return 0;
  }

  readh = gwserver->hlen;       /* read from header so far */
  need = HDR_SIZE - readh;      /* need to read */
  if (need > 0) {
    if ((bytes =
         ws_read_fifo (fd, gwserver->hdr, &gwserver->hlen, readh, need)) < 0)
      return 0;
    if (bytes != need)
      return 0;
  }

  /* unpack size, and type */
  ptr = gwserver->hdr;
  ptr += unpack_uint32 (ptr, &listener);
  ptr += unpack_uint32 (ptr, &type);
  ptr += unpack_uint32 (ptr, &size);

  if ((*pa) == NULL) {
    (*pa) = xcalloc (1, sizeof (WSPacket));
    (*pa)->type = type;
    (*pa)->size = size;
    (*pa)->data = xcalloc (size, sizeof (char));
  }

  readh = (*pa)->len;   /* read from payload so far */
  need = (*pa)->size - readh;   /* need to read */
  if (need > 0) {
    if ((bytes = ws_read_fifo (fd, (*pa)->data, &(*pa)->len, readh, need)) < 0)
      return 0;
    if (bytes != need)
      return 0;
  }
  clear_fifo_packet (gwserver);
  /* fast forward JSON data to the given client */
  (*f) (gwserver->pipein, listener);

  return 0;
}

/* Callback once a new connection is established
 *
 * It writes to a named pipe a header containing the socket, the
 * message type, the payload's length and the actual payload */
static int
onopen (WSPipeOut * pipeout, WSClient * client)
{
  uint32_t hsize = sizeof (uint32_t) * 3;
  char *hdr = calloc (hsize, sizeof (char));
  char *ptr = hdr;

  ptr += pack_uint32 (ptr, client->listener);
  ptr += pack_uint32 (ptr, WS_OPCODE_TEXT);
  ptr += pack_uint32 (ptr, INET6_ADDRSTRLEN);

  ws_write_fifo (pipeout, hdr, hsize);
  ws_write_fifo (pipeout, client->remote_ip, INET6_ADDRSTRLEN);
  free (hdr);

  return 0;
}

/* Open the named pipe where the websocket server writes to.
 *
 * If unable to open, -1 is returned.
 * On success, return the new file descriptor is returned . */
int
open_fifoout (void)
{
  const char *fifo = "/tmp/wspipeout.fifo";
  int fdfifo;

  /* open fifo for reading before writing */
  ws_setfifo (fifo);
  if ((fdfifo = open (fifo, O_RDWR | O_NONBLOCK)) == -1)
    return -1;

  return fdfifo;
}

/* Open the named pipe where the websocket server reads from.
 *
 * If unable to open, -1 is returned.
 * On success, return the new file descriptor is returned . */
int
open_fifoin (void)
{
  const char *fifo = "/tmp/wspipein.fifo";
  int fdfifo;

  if ((fdfifo = open (fifo, O_WRONLY | O_NONBLOCK)) == -1)
    return -1;

  return fdfifo;
}

/* Set the self-pipe trick to handle select(2). */
void
set_self_pipe (int *self_pipe)
{
  /* Initialize self pipe. */
  if (pipe (self_pipe) == -1)
    FATAL ("Unable to create pipe: %s.", strerror (errno));

  /* make the read and write pipe non-blocking */
  set_nonblocking (self_pipe[0]);
  set_nonblocking (self_pipe[1]);
}

/* Close the WebSocket server and clean up. */
void
stop_ws_server (GWSThread * gwserver)
{
  if (server == NULL)
    return;

  if ((write (gwserver->self_pipe[1], "x", 1)) == -1 && errno != EAGAIN)
    LOG (("Unable to write to self pipe on pipeout.\n"));
  /* if it fails to write, force stop */
  if ((write (server->self_pipe[1], "x", 1)) == -1 && errno != EAGAIN)
    ws_stop (server);

  if (pthread_join (gwserver->thout, NULL) != 0)
    LOG (("Unable to join thread: %n %s\n", gwserver->thout, strerror (errno)));
  if (pthread_join (gwserver->thin, NULL) != 0)
    LOG (("Unable to join thread: %n %s\n", gwserver->thout, strerror (errno)));
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
  server->onopen = onopen;
  set_self_pipe (server->self_pipe);

  /* select(2) will block in here */
  ws_start (server);
  fprintf (stderr, "Stopping WebSocket server...\n");
  ws_stop (server);
}

/* Setup and start the WebSocket threads. */
int
setup_ws_server (GWSThread * gwserver)
{
  int tid;

  if (pthread_mutex_init (&gwserver->mtxin, NULL))
    FATAL ("Failed init gwserver mutex fifo in");
  if (pthread_mutex_init (&gwserver->mtxout, NULL))
    FATAL ("Failed init gwserver mutex fifo out");

  /* send WS data thread */
  tid = pthread_create (&(gwserver->thin), NULL, (void *) &start_server, NULL);
  if (tid)
    FATAL ("Return code from pthread_create(): %d", tid);

  /* read WS data thread */
  tid = pthread_create (&(gwserver->thout), NULL, (void *) &read_client, NULL);
  if (tid)
    FATAL ("Return code from pthread_create(): %d", tid);

  return 0;
}
