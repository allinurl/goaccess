/**
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

#ifndef GDNS_H_INCLUDED
#define GDNS_H_INCLUDED

#define H_SIZE     1025
#define QUEUE_SIZE 400

typedef struct GDnsThread_
{
  pthread_cond_t not_empty;
  pthread_cond_t not_full;
  pthread_mutex_t mutex;
  pthread_t thread;
} GDnsThread;

typedef struct GDnsQueue_
{
  int head;
  int tail;
  int size;
  int capacity;
  char buffer[QUEUE_SIZE][H_SIZE];
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
