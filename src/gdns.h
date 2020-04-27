/**
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

#ifndef GDNS_H_INCLUDED
#define GDNS_H_INCLUDED

#define H_SIZE     1025
#define QUEUE_SIZE 400

typedef struct GDnsThread_ {
  pthread_cond_t not_empty;     /* not empty queue condition */
  pthread_cond_t not_full;      /* not full queue condition */
  pthread_mutex_t mutex;
  pthread_t thread;
} GDnsThread;

typedef struct GDnsQueue_ {
  int head;                     /* index to head of queue */
  int tail;                     /* index to tail of queue */
  int size;                     /* queue size */
  int capacity;                 /* length at most */
  char buffer[QUEUE_SIZE][H_SIZE];      /* data item */
} GDnsQueue;

extern GDnsThread gdns_thread;

char *gqueue_dequeue (GDnsQueue * q);
char *reverse_ip (char *str);
int gqueue_empty (GDnsQueue * q);
int gqueue_enqueue (GDnsQueue * q, char *item);
int gqueue_find (GDnsQueue * q, const char *item);
int gqueue_full (GDnsQueue * q);
int gqueue_size (GDnsQueue * q);
void dns_resolver (char *addr);
void gdns_free_queue (void);
void gdns_init (void);
void gdns_queue_free (void);
void gdns_thread_create (void);
void gqueue_destroy (GDnsQueue * q);
void gqueue_init (GDnsQueue * q, int capacity);

#endif
