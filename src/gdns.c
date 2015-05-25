/**
 * gdns.c -- hosts resolver
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

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabdb.h"
#else
#include "glibht.h"
#endif

#include "error.h"
#include "goaccess.h"
#include "util.h"
#include "xmalloc.h"

GDnsThread gdns_thread;
static GDnsQueue *gdns_queue;

/* initialize queue */
void
gqueue_init (GDnsQueue * q, int capacity)
{
  q->head = 0;
  q->tail = -1;
  q->size = 0;
  q->capacity = capacity;
}

/* get current size of queue */
int
gqueue_size (GDnsQueue * q)
{
  return q->size;
}

/* is the queue empty */
int
gqueue_empty (GDnsQueue * q)
{
  return q->size == 0;
}

/* is the queue full */
int
gqueue_full (GDnsQueue * q)
{
  return q->size == q->capacity;
}

/* destroy queue */
void
gqueue_destroy (GDnsQueue * q)
{
  free (q);
}

/* add item to the queue */
int
gqueue_enqueue (GDnsQueue * q, char *item)
{
  if (gqueue_full (q))
    return -1;

  q->tail = (q->tail + 1) % q->capacity;
  strcpy (q->buffer[q->tail], item);
  q->size++;
  return 0;
}

int
gqueue_find (GDnsQueue * q, const char *item)
{
  int i;
  if (gqueue_empty (q))
    return 0;

  for (i = 0; i < q->size; i++) {
    if (strcmp (item, q->buffer[i]) == 0)
      return 1;
  }
  return 0;
}

/* remove an item from the queue */
char *
gqueue_dequeue (GDnsQueue * q)
{
  char *item;
  if (gqueue_empty (q))
    return NULL;

  item = q->buffer[q->head];
  q->head = (q->head + 1) % q->capacity;
  q->size--;
  return item;
}

/* get the corresponding host given an address */
static char *
reverse_host (const struct sockaddr *a, socklen_t length)
{
  char h[H_SIZE];
  int flags, st;

  flags = NI_NAMEREQD;
  st = getnameinfo (a, length, h, H_SIZE, NULL, 0, flags);
  if (!st)
    return alloc_string (h);
  return alloc_string (gai_strerror (st));
}

/* determine if IPv4 or IPv6 and resolve */
char *
reverse_ip (char *str)
{
  union
  {
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

/* producer */
void
dns_resolver (char *addr)
{
  pthread_mutex_lock (&gdns_thread.mutex);
  if (!gqueue_full (gdns_queue) && !gqueue_find (gdns_queue, addr)) {
#ifndef HAVE_LIBTOKYOCABINET
    g_hash_table_replace (ht_hostnames, g_strdup (addr), NULL);
#endif
    gqueue_enqueue (gdns_queue, addr);
    pthread_cond_broadcast (&gdns_thread.not_empty);
  }
  pthread_mutex_unlock (&gdns_thread.mutex);
}

/* consumer */
static void
dns_worker (void GO_UNUSED (*ptr_data))
{
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
      if (host)
        free (host);
      break;
    }
#ifdef HAVE_LIBTOKYOCABINET
    tcadbput2 (ht_hostnames, ip, host);
    free (host);
#else
    if (host != NULL && active_gdns)
      g_hash_table_replace (ht_hostnames, g_strdup (ip), host);
#endif

    pthread_cond_signal (&gdns_thread.not_full);
    pthread_mutex_unlock (&gdns_thread.mutex);
  }
}

/* init queue and dns thread */
void
gdns_init (void)
{
  gdns_queue = xmalloc (sizeof (GDnsQueue));
  gqueue_init (gdns_queue, QUEUE_SIZE);

  if (pthread_cond_init (&(gdns_thread.not_empty), NULL))
    FATAL ("Failed init thread condition");

  if (pthread_cond_init (&(gdns_thread.not_full), NULL))
    FATAL ("Failed init thread condition");

  if (pthread_mutex_init (&(gdns_thread.mutex), NULL))
    FATAL ("Failed init thread mutex");
}

/* destroy queue */
void
gdns_free_queue (void)
{
  gqueue_destroy (gdns_queue);
}

/* create a dns thread */
void
gdns_thread_create (void)
{
  int thread;

  active_gdns = 1;
  thread =
    pthread_create (&(gdns_thread.thread), NULL, (void *) &dns_worker, NULL);
  if (thread)
    FATAL ("Return code from pthread_create(): %d", thread);
  pthread_detach (gdns_thread.thread);
}
