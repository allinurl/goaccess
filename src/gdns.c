/**
 * gdns.c -- hosts resolver
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2022 Gerardo Orellana <hello @ goaccess.io>
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

#define _MULTI_THREADED
#ifdef __FreeBSD__
#include <sys/socket.h>
#endif

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "gdns.h"

#include "error.h"
#include "gkhash.h"
#include "goaccess.h"
#include "util.h"
#include "xmalloc.h"

GDnsThread gdns_thread;
static GDnsQueue *gdns_queue;

/* Initialize the queue. */
void
gqueue_init (GDnsQueue * q, int capacity) {
  q->head = 0;
  q->tail = -1;
  q->size = 0;
  q->capacity = capacity;
}

/* Get the current size of queue.
 *
 * Returns the size of the queue. */
int
gqueue_size (GDnsQueue * q) {
  return q->size;
}

/* Determine if the queue is empty.
 *
 * Returns true if empty, otherwise false. */
int
gqueue_empty (GDnsQueue * q) {
  return q->size == 0;
}

/* Determine if the queue is full.
 *
 * Returns true if full, otherwise false. */
int
gqueue_full (GDnsQueue * q) {
  return q->size == q->capacity;
}

/* Free the queue. */
void
gqueue_destroy (GDnsQueue * q) {
  free (q);
}

/* Add at the end of the queue a string item.
 *
 * If the queue is full, -1 is returned.
 * If added to the queue, 0 is returned. */
int
gqueue_enqueue (GDnsQueue * q, const char *item) {
  if (gqueue_full (q))
    return -1;

  q->tail = (q->tail + 1) % q->capacity;
  strncpy (q->buffer[q->tail], item, sizeof (q->buffer[q->tail]));
  q->buffer[q->tail][sizeof (q->buffer[q->tail]) - 1] = '\0';
  q->size++;
  return 0;
}

/* Find a string item in the queue.
 *
 * If the queue is empty, or the item is not in the queue, 0 is returned.
 * If found, 1 is returned. */
int
gqueue_find (GDnsQueue * q, const char *item) {
  int i;
  if (gqueue_empty (q))
    return 0;

  for (i = 0; i < q->size; i++) {
    if (strcmp (item, q->buffer[i]) == 0)
      return 1;
  }
  return 0;
}

/* Remove a string item from the head of the queue.
 *
 * If the queue is empty, NULL is returned.
 * If removed, the string item is returned. */
char *
gqueue_dequeue (GDnsQueue * q) {
  char *item;
  if (gqueue_empty (q))
    return NULL;

  item = q->buffer[q->head];
  q->head = (q->head + 1) % q->capacity;
  q->size--;
  return item;
}

/* Get the corresponding hostname given an IP address.
 *
 * On error, a string error message is returned.
 * On success, a malloc'd hostname is returned. */
static char *
reverse_host (const struct sockaddr *a, socklen_t length) {
  char h[H_SIZE] = { 0 };
  int flags, st;

  flags = NI_NAMEREQD;
  st = getnameinfo (a, length, h, H_SIZE, NULL, 0, flags);
  if (!st) {
    /* BSD returns \0 while Linux . on solve lookups */
    if (*h == '\0')
      return alloc_string (".");
    return alloc_string (h);
  }
  return alloc_string (gai_strerror (st));
}

/* Determine if IPv4 or IPv6 and resolve.
 *
 * On error, NULL is returned.
 * On success, a malloc'd hostname is returned. */
char *
reverse_ip (char *str) {
  union {
    struct sockaddr addr;
    struct sockaddr_in6 addr6;
    struct sockaddr_in addr4;
  } a;

  if (str == NULL || *str == '\0')
    return NULL;

  memset (&a, 0, sizeof (a));
  if (1 == inet_pton (AF_INET, str, &a.addr4.sin_addr)) {
    a.addr4.sin_family = AF_INET;
    return reverse_host (&a.addr, sizeof (a.addr4));
  } else if (1 == inet_pton (AF_INET6, str, &a.addr6.sin6_addr)) {
    a.addr6.sin6_family = AF_INET6;
    return reverse_host (&a.addr, sizeof (a.addr6));
  }
  return NULL;
}

/* Producer - Resolve an IP address and add it to the queue. */
void
dns_resolver (char *addr) {
  pthread_mutex_lock (&gdns_thread.mutex);
  /* queue is not full and the IP address is not in the queue */
  if (!gqueue_full (gdns_queue) && !gqueue_find (gdns_queue, addr)) {
    /* add the IP to the queue */
    gqueue_enqueue (gdns_queue, addr);
    pthread_cond_broadcast (&gdns_thread.not_empty);
  }
  pthread_mutex_unlock (&gdns_thread.mutex);
}

/* Consumer - Once an IP has been resolved, add it to dwithe hostnames
 * hash structure. */
static void
dns_worker (void GO_UNUSED (*ptr_data)) {
  char *ip = NULL, *host = NULL;

  while (1) {
    pthread_mutex_lock (&gdns_thread.mutex);
    /* wait until an item has been added to the queue */
    while (gqueue_empty (gdns_queue))
      pthread_cond_wait (&gdns_thread.not_empty, &gdns_thread.mutex);

    ip = gqueue_dequeue (gdns_queue);

    pthread_mutex_unlock (&gdns_thread.mutex);
    host = reverse_ip (ip);
    pthread_mutex_lock (&gdns_thread.mutex);

    if (!active_gdns) {
      pthread_mutex_unlock (&gdns_thread.mutex);
      free (host);
      return;
    }

    /* insert the corresponding IP -> hostname map */
    if (host != NULL && active_gdns) {
      ht_insert_hostname (ip, host);
      free (host);
    }

    pthread_cond_signal (&gdns_thread.not_full);
    pthread_mutex_unlock (&gdns_thread.mutex);
  }
}

/* Initialize queue and dns thread */
void
gdns_init (void) {
  gdns_queue = xmalloc (sizeof (GDnsQueue));
  gqueue_init (gdns_queue, QUEUE_SIZE);

  if (pthread_cond_init (&(gdns_thread.not_empty), NULL))
    FATAL ("Failed init thread condition");

  if (pthread_cond_init (&(gdns_thread.not_full), NULL))
    FATAL ("Failed init thread condition");

  if (pthread_mutex_init (&(gdns_thread.mutex), NULL))
    FATAL ("Failed init thread mutex");
}

/* Destroy (free) queue */
void
gdns_free_queue (void) {
  gqueue_destroy (gdns_queue);
}

/* Create a DNS thread and make it active */
void
gdns_thread_create (void) {
  int th;

  active_gdns = 1;
  th = pthread_create (&(gdns_thread.thread), NULL, (void *) &dns_worker, NULL);
  if (th)
    FATAL ("Return code from pthread_create(): %d", th);
  pthread_detach (gdns_thread.thread);
}
