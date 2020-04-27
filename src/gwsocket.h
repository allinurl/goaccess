/**
 *    _______       _______            __        __
 *   / ____/ |     / / ___/____  _____/ /_____  / /_
 *  / / __ | | /| / /\__ \/ __ \/ ___/ //_/ _ \/ __/
 * / /_/ / | |/ |/ /___/ / /_/ / /__/ ,< /  __/ /_
 * \____/  |__/|__//____/\____/\___/_/|_|\___/\__/
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

#ifndef GWSOCKET_H_INCLUDED
#define GWSOCKET_H_INCLUDED

#define GW_VERSION "0.1"

#include <pthread.h>
#include "websocket.h"

typedef struct GWSReader_ {
  int fd;
  int self_pipe[2];             /* self-pipe */

  pthread_mutex_t mutex;        /* Mutex fifo out */
  pthread_t thread;             /* Thread fifo in */

  WSPacket *packet;             /* FIFO data's buffer */
  char hdr[HDR_SIZE];           /* FIFO header's buffer */
  int hlen;                     /* header length */
} GWSReader;

typedef struct GWSWriter_ {
  int fd;

  pthread_mutex_t mutex;        /* Mutex fifo in */
  pthread_t thread;             /* Thread fifo out */

  WSServer *server;             /* WebSocket server */
} GWSWriter;

GWSReader *new_gwsreader (void);
GWSWriter *new_gwswriter (void);
int broadcast_holder (int fd, const char *buf, int len);
int open_fifoin (void);
int open_fifoout (void);
int read_fifo (GWSReader * gwsreader, fd_set rfds, fd_set wfds, void (*f) (int));
int send_holder_to_client (int fd, int listener, const char *buf, int len);
int setup_ws_server (GWSWriter * gwswriter, GWSReader * gwsreader);
void set_ready_state (void);
void set_self_pipe (int *self_pipe);
void stop_ws_server (GWSWriter * gwswriter, GWSReader * gwsreader);

#endif // for #ifndef GWSOCKET_H
