/**
 * websocket.c -- An rfc6455-complaint Web Socket Server
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "websocket.h"

#include "base64.h"
#include "error.h"
#include "gslist.h"
#include "sha1.h"
#include "xmalloc.h"

/* *INDENT-OFF* */

/* UTF-8 Decoder */
/* Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
 * See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details. */
#define UTF8_VALID 0
#define UTF8_INVAL 1
static const uint8_t utf8d[] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 00..1f */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 20..3f */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 40..5f */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 60..7f */
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, /* 80..9f */
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, /* a0..bf */
  8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, /* c0..df */
  0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, /* e0..ef */
  0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, /* f0..ff */
  0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, /* s0..s0 */
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, /* s1..s2 */
  1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, /* s3..s4 */
  1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, /* s5..s6 */
  1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* s7..s8 */
};
/* *INDENT-ON* */

static int max_file_fd = 0;
static WSEState fdstate;
static WSConfig wsconfig = { 0 };

static void handle_read_close (int conn, WSClient * client, WSServer * server);
static void handle_reads (int conn, WSServer * server);
static void handle_writes (int conn, WSServer * server);
#ifdef HAVE_LIBSSL
static int shutdown_ssl (WSClient * client);
#endif

/* Determine if the given string is valid UTF-8.
 *
 * The state after the by has been processed is returned. */
static uint32_t
verify_utf8 (uint32_t * state, const char *str, int len) {
  int i;
  uint32_t type;

  for (i = 0; i < len; ++i) {
    type = utf8d[(uint8_t) str[i]];
    *state = utf8d[256 + (*state) * 16 + type];

    if (*state == UTF8_INVAL)
      break;
  }

  return *state;
}

/* Decode a character maintaining state and a byte, and returns the
 * state achieved after processing the byte.
 *
 * The state after the by has been processed is returned. */
static uint32_t
utf8_decode (uint32_t * state, uint32_t * p, uint32_t b) {
  uint32_t type = utf8d[(uint8_t) b];

  *p = (*state != UTF8_VALID) ? (b & 0x3fu) | (*p << 6) : (0xff >> type) & (b);
  *state = utf8d[256 + *state * 16 + type];

  return *state;
}

/* Replace malformed sequences with a substitute character.
 *
 * On success, it replaces the whole sequence and return a malloc'd buffer. */
static char *
sanitize_utf8 (const char *str, int len) {
  char *buf = NULL;
  uint32_t state = UTF8_VALID, prev = UTF8_VALID, cp = 0;
  int i = 0, j = 0, k = 0, l = 0;

  buf = xcalloc (len + 1, sizeof (char));
  for (; i < len; prev = state, ++i) {
    switch (utf8_decode (&state, &cp, (unsigned char) str[i])) {
    case UTF8_INVAL:
      /* replace the whole sequence */
      if (k) {
        for (l = i - k; l < i; ++l)
          buf[j++] = '?';
      } else {
        buf[j++] = '?';
      }
      state = UTF8_VALID;
      if (prev != UTF8_VALID)
        i--;
      k = 0;
      break;
    case UTF8_VALID:
      /* fill i - k valid continuation bytes */
      if (k)
        for (l = i - k; l < i; ++l)
          buf[j++] = str[l];
      buf[j++] = str[i];
      k = 0;
      break;
    default:
      /* UTF8_VALID + continuation bytes */
      k++;
      break;
    }
  }

  return buf;
}

/* Allocate memory for a websocket server */
static WSServer *
new_wsserver (void) {
  WSServer *server = xcalloc (1, sizeof (WSServer));

  return server;
}

/* Allocate memory for a websocket client */
static WSClient *
new_wsclient (void) {
  WSClient *client = xcalloc (1, sizeof (WSClient));
  client->status = WS_OK;

  return client;
}

/* Allocate memory for a websocket header */
static WSHeaders *
new_wsheader (void) {
  WSHeaders *headers = xcalloc (1, sizeof (WSHeaders));
  memset (headers->buf, 0, sizeof (headers->buf));
  headers->reading = 1;

  return headers;
}

/* Allocate memory for a websocket frame */
static WSFrame *
new_wsframe (void) {
  WSFrame *frame = xcalloc (1, sizeof (WSFrame));
  memset (frame->buf, 0, sizeof (frame->buf));
  frame->reading = 1;

  return frame;
}

/* Allocate memory for a websocket message */
static WSMessage *
new_wsmessage (void) {
  WSMessage *msg = xcalloc (1, sizeof (WSMessage));

  return msg;
}

/* Allocate memory for a websocket pipeout */
static WSPipeOut *
new_wspipeout (void) {
  WSPipeOut *pipeout = xcalloc (1, sizeof (WSPipeOut));
  pipeout->fd = -1;

  return pipeout;
}

/* Allocate memory for a websocket pipein */
static WSPipeIn *
new_wspipein (void) {
  WSPipeIn *pipein = xcalloc (1, sizeof (WSPipeIn));
  pipein->fd = -1;

  return pipein;
}

/* Escapes the special characters, e.g., '\n', '\r', '\t', '\'
 * in the string source by inserting a '\' before them.
 *
 * On error NULL is returned.
 * On success the escaped string is returned */
static char *
escape_http_request (const char *src) {
  char *dest, *q;
  const unsigned char *p;

  if (src == NULL || *src == '\0')
    return NULL;

  p = (const unsigned char *) src;
  q = dest = xmalloc (strlen (src) * 4 + 1);

  while (*p) {
    switch (*p) {
    case '\\':
      *q++ = '\\';
      *q++ = '\\';
      break;
    case '\n':
      *q++ = '\\';
      *q++ = 'n';
      break;
    case '\r':
      *q++ = '\\';
      *q++ = 'r';
      break;
    case '\t':
      *q++ = '\\';
      *q++ = 't';
      break;
    case '"':
      *q++ = '\\';
      *q++ = '"';
      break;
    default:
      if ((*p < ' ') || (*p >= 0177)) {
        /* not ASCII */
      } else {
        *q++ = *p;
      }
      break;
    }
    p++;
  }
  *q = 0;
  return dest;
}

/* Make a string uppercase.
 *
 * On error the original string is returned.
 * On success, the uppercased string is returned. */
static char *
strtoupper (char *str) {
  char *p = str;
  if (str == NULL || *str == '\0')
    return str;

  while (*p != '\0') {
    *p = toupper (*p);
    p++;
  }

  return str;
}

/* Chop n characters from the beginning of the supplied buffer.
 *
 * The new length of the string is returned. */
static size_t
chop_nchars (char *str, size_t n, size_t len) {
  if (n == 0 || str == 0)
    return 0;

  if (n > len)
    n = len;
  memmove (str, str + n, len - n);

  return (len - n);
}

/* Match a client given a socket id and an item from the list.
 *
 * On match, 1 is returned, else 0. */
static int
ws_find_client_sock_in_list (void *data, void *needle) {
  WSClient *client = data;

  return client->listener == (*(int *) needle);
}

/* Find a client given a socket id.
 *
 * On success, an instance of a GSLList node is returned, else NULL. */
static GSLList *
ws_get_list_node_from_list (int listener, GSLList ** colist) {
  GSLList *match = NULL;

  /* Find the client data for the socket in use */
  if (!(match = list_find (*colist, ws_find_client_sock_in_list, &listener)))
    return NULL;
  return match;
}

/* Find a client given a socket id.
 *
 * On success, an instance of a WSClient is returned, else NULL. */
static WSClient *
ws_get_client_from_list (int listener, GSLList ** colist) {
  GSLList *match = NULL;

  /* Find the client data for the socket in use */
  if (!(match = list_find (*colist, ws_find_client_sock_in_list, &listener)))
    return NULL;
  return (WSClient *) match->data;
}

/* Free a frame structure and its data for the given client. */
static void
ws_free_frame (WSClient * client) {
  if (client->frame)
    free (client->frame);
  client->frame = NULL;
}

/* Free a message structure and its data for the given client. */
static void
ws_free_message (WSClient * client) {
  if (client->message && client->message->payload)
    free (client->message->payload);
  if (client->message)
    free (client->message);
  client->message = NULL;
}

/* Free all HTTP handshake headers data for the given client. */
static void
ws_free_header_fields (WSHeaders * headers) {
  if (headers->connection)
    free (headers->connection);
  if (headers->host)
    free (headers->host);
  if (headers->agent)
    free (headers->agent);
  if (headers->method)
    free (headers->method);
  if (headers->origin)
    free (headers->origin);
  if (headers->path)
    free (headers->path);
  if (headers->protocol)
    free (headers->protocol);
  if (headers->upgrade)
    free (headers->upgrade);
  if (headers->ws_accept)
    free (headers->ws_accept);
  if (headers->ws_key)
    free (headers->ws_key);
  if (headers->ws_protocol)
    free (headers->ws_protocol);
  if (headers->ws_resp)
    free (headers->ws_resp);
  if (headers->ws_sock_ver)
    free (headers->ws_sock_ver);
  if (headers->referer)
    free (headers->referer);
}

/* Clear the client's sent queue and its data. */
static void
ws_clear_queue (WSClient * client) {
  WSQueue **queue = &client->sockqueue;
  if (!(*queue))
    return;

  if ((*queue)->queued)
    free ((*queue)->queued);
  (*queue)->queued = NULL;
  (*queue)->qlen = 0;

  free ((*queue));
  (*queue) = NULL;

  /* done sending the whole queue, stop throttling */
  client->status &= ~WS_THROTTLING;
  /* done sending, close connection if set to close */
  if ((client->status & WS_CLOSE) && (client->status & WS_SENDING))
    client->status = WS_CLOSE;
}

/* Free all HTTP handshake headers and structure. */
static void
ws_clear_handshake_headers (WSHeaders * headers) {
  ws_free_header_fields (headers);
  free (headers);
}

/* Remove the given client from the list. */
static void
ws_remove_client_from_list (WSClient * client, WSServer * server) {
  GSLList *node = NULL;

  if (!(node = ws_get_list_node_from_list (client->listener, &server->colist)))
    return;

  if (client->headers)
    ws_clear_handshake_headers (client->headers);
  list_remove_node (&server->colist, node);
}

#if HAVE_LIBSSL
/* Attempt to send the TLS/SSL "close notify" shutdown and and removes
 * the SSL structure pointed to by ssl and frees up the allocated
 * memory. */
static void
ws_shutdown_dangling_clients (WSClient * client) {
  shutdown_ssl (client);
  SSL_free (client->ssl);
  client->ssl = NULL;
}

/* Attempt to remove the SSL_CTX object pointed to by ctx and frees up
 * the allocated memory and cleans some more generally used TLS/SSL
 * memory.  */
static void
ws_ssl_cleanup (WSServer * server) {
  if (!wsconfig.use_ssl)
    return;

  if (server->ctx)
    SSL_CTX_free (server->ctx);

  CRYPTO_cleanup_all_ex_data ();
  CRYPTO_set_id_callback (NULL);
  CRYPTO_set_locking_callback (NULL);
  ERR_free_strings ();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ERR_remove_state (0);
#endif
  EVP_cleanup ();
}
#endif

/* Remove all clients that are still hanging out. */
static int
ws_remove_dangling_clients (void *value, void *user_data) {
  WSClient *client = value;
  (void) (user_data);

  if (client == NULL)
    return 1;

  if (client->headers)
    ws_clear_handshake_headers (client->headers);
  if (client->sockqueue)
    ws_clear_queue (client);
#ifdef HAVE_LIBSSL
  if (client->ssl)
    ws_shutdown_dangling_clients (client);
#endif

  return 0;
}

/* Do some housekeeping on the named pipe data packet. */
static void
ws_clear_fifo_packet (WSPacket * packet) {
  if (!packet)
    return;

  if (packet->data)
    free (packet->data);
  free (packet);
}

/* Do some housekeeping on the named pipe. */
static void
ws_clear_pipein (WSPipeIn * pipein) {
  WSPacket **packet = &pipein->packet;
  if (!pipein)
    return;

  if (pipein->fd != -1)
    close (pipein->fd);

  ws_clear_fifo_packet (*packet);
  free (pipein);

  if (wsconfig.pipein && access (wsconfig.pipein, F_OK) != -1)
    unlink (wsconfig.pipein);
}

/* Do some housekeeping on the named pipe. */
static void
ws_clear_pipeout (WSPipeOut * pipeout) {
  if (!pipeout)
    return;

  if (pipeout->fd != -1)
    close (pipeout->fd);

  free (pipeout);

  if (wsconfig.pipeout && access (wsconfig.pipeout, F_OK) != -1)
    unlink (wsconfig.pipeout);
}

/* Stop the server and do some cleaning. */
void
ws_stop (WSServer * server) {
  WSPipeIn **pipein = &server->pipein;
  WSPipeOut **pipeout = &server->pipeout;

  ws_clear_pipein (*pipein);
  ws_clear_pipeout (*pipeout);

  /* close access log (if any) */
  if (wsconfig.accesslog)
    access_log_close ();

  /* remove dangling clients */
  if (list_count (server->colist) > 0)
    list_foreach (server->colist, ws_remove_dangling_clients, NULL);

  if (server->colist)
    list_remove_nodes (server->colist);

#ifdef HAVE_LIBSSL
  ws_ssl_cleanup (server);
#endif

  free (server);
}

/* A wrapper to close a socket. */
static void
ws_close (int listener) {
  close (listener);
}

/* Set the connection status for the given client and return the given
 * bytes.
 *
 * The given number of bytes are returned. */
static int
ws_set_status (WSClient * client, WSStatus status, int bytes) {
  client->status = status;
  return bytes;
}

/* Append the source string to destination and reallocates and
 * updating the destination buffer appropriately. */
static void
ws_append_str (char **dest, const char *src) {
  size_t curlen = strlen (*dest);
  size_t srclen = strlen (src);
  size_t newlen = curlen + srclen;

  char *str = xrealloc (*dest, newlen + 1);
  memcpy (str + curlen, src, srclen + 1);
  *dest = str;
}

#if HAVE_LIBSSL
/* Create a new SSL_CTX object as framework to establish TLS/SSL
 * enabled connections.
 *
 * On error 1 is returned.
 * On success, SSL_CTX object is malloc'd and 0 is returned.
 */
static int
initialize_ssl_ctx (WSServer * server) {
  int ret = 1;
  SSL_CTX *ctx = NULL;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  SSL_library_init ();
  SSL_load_error_strings ();
#endif

  /* Ciphers and message digests */
  OpenSSL_add_ssl_algorithms ();

  /* ssl context */
  if (!(ctx = SSL_CTX_new (SSLv23_server_method ())))
    goto out;
  /* set certificate */
  if (!SSL_CTX_use_certificate_file (ctx, wsconfig.sslcert, SSL_FILETYPE_PEM))
    goto out;
  /* ssl private key */
  if (!SSL_CTX_use_PrivateKey_file (ctx, wsconfig.sslkey, SSL_FILETYPE_PEM))
    goto out;
  if (!SSL_CTX_check_private_key (ctx))
    goto out;

  /* since we queued up the send data, a retry won't be the same buffer,
   * thus we need the following flags */
  SSL_CTX_set_mode (ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_ENABLE_PARTIAL_WRITE);

  server->ctx = ctx;
  ret = 0;
out:
  if (ret) {
    SSL_CTX_free (ctx);
    LOG (("Error: %s\n", ERR_error_string (ERR_get_error (), NULL)));
  }

  return ret;
}

/* Log result code for TLS/SSL I/O operation */
static void
log_return_message (int ret, int err, const char *fn) {
  unsigned long e;

  switch (err) {
  case SSL_ERROR_NONE:
    LOG (("SSL: %s -> SSL_ERROR_NONE\n", fn));
    LOG (("SSL: TLS/SSL I/O operation completed\n"));
    break;
  case SSL_ERROR_WANT_READ:
    LOG (("SSL: %s -> SSL_ERROR_WANT_READ\n", fn));
    LOG (("SSL: incomplete, data available for reading\n"));
    break;
  case SSL_ERROR_WANT_WRITE:
    LOG (("SSL: %s -> SSL_ERROR_WANT_WRITE\n", fn));
    LOG (("SSL: incomplete, data available for writing\n"));
    break;
  case SSL_ERROR_ZERO_RETURN:
    LOG (("SSL: %s -> SSL_ERROR_ZERO_RETURN\n", fn));
    LOG (("SSL: TLS/SSL connection has been closed\n"));
    break;
  case SSL_ERROR_WANT_X509_LOOKUP:
    LOG (("SSL: %s -> SSL_ERROR_WANT_X509_LOOKUP\n", fn));
    break;
  case SSL_ERROR_SYSCALL:
    LOG (("SSL: %s -> SSL_ERROR_SYSCALL\n", fn));

    e = ERR_get_error ();
    if (e > 0)
      LOG (("SSL: %s -> %s\n", fn, ERR_error_string (e, NULL)));

    /* call was not successful because a fatal error occurred either at the
     * protocol level or a connection failure occurred. */
    if (ret != 0) {
      LOG (("SSL bogus handshake interrupt: %s\n", strerror (errno)));
      break;
    }
    /* call not yet finished. */
    LOG (("SSL: handshake interrupted, got EOF\n"));
    if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)
      LOG (("SSL: %s -> not yet finished %s\n", fn, strerror (errno)));
    break;
  default:
    LOG (("SSL: %s -> failed fatal error code: %d\n", fn, err));
    LOG (("SSL: %s\n", ERR_error_string (ERR_get_error (), NULL)));
    break;
  }
}

/* Shut down the client's TLS/SSL connection
 *
 * On fatal error, 1 is returned.
 * If data still needs to be read/written, -1 is returned.
 * On success, the TLS/SSL connection is closed and 0 is returned */
static int
shutdown_ssl (WSClient * client) {
  int ret = -1, err = 0;

  /* all good */
  if ((ret = SSL_shutdown (client->ssl)) > 0)
    return ws_set_status (client, WS_CLOSE, 0);

  err = SSL_get_error (client->ssl, ret);
  log_return_message (ret, err, "SSL_shutdown");

  switch (err) {
  case SSL_ERROR_WANT_READ:
  case SSL_ERROR_WANT_WRITE:
    client->sslstatus = WS_TLS_SHUTTING;
    break;
  case SSL_ERROR_SYSCALL:
    if (ret == 0) {
      LOG (("SSL: SSL_shutdown, connection unexpectedly closed by peer.\n"));
      /* The shutdown is not yet finished. */
      if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)
        client->sslstatus = WS_TLS_SHUTTING;
      break;
    }
    LOG (("SSL: SSL_shutdown, probably unrecoverable, forcing close.\n"));
    /* FALLTHRU */
  case SSL_ERROR_ZERO_RETURN:
  case SSL_ERROR_WANT_X509_LOOKUP:
  default:
    return ws_set_status (client, WS_ERR | WS_CLOSE, 1);
  }

  return ret;
}

/* Wait for a TLS/SSL client to initiate a TLS/SSL handshake
 *
 * On fatal error, the connection is shut down.
 * If data still needs to be read/written, -1 is returned.
 * On success, the TLS/SSL connection is completed and 0 is returned */
static int
accept_ssl (WSClient * client) {
  int ret = -1, err = 0;

  /* all good on TLS handshake */
  if ((ret = SSL_accept (client->ssl)) > 0) {
    client->sslstatus &= ~WS_TLS_ACCEPTING;
    return 0;
  }

  err = SSL_get_error (client->ssl, ret);
  log_return_message (ret, err, "SSL_accept");

  switch (err) {
  case SSL_ERROR_WANT_READ:
  case SSL_ERROR_WANT_WRITE:
    client->sslstatus = WS_TLS_ACCEPTING;
    break;
  case SSL_ERROR_SYSCALL:
    /* Wait for more activity else bail out, for instance if the socket is closed
     * during the handshake. */
    if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
      client->sslstatus = WS_TLS_ACCEPTING;
      break;
    }
    /* The peer notified that it is shutting down through a SSL "close_notify" so
     * we shutdown too */
    /* FALLTHRU */
  case SSL_ERROR_ZERO_RETURN:
  case SSL_ERROR_WANT_X509_LOOKUP:
  default:
    client->sslstatus &= ~WS_TLS_ACCEPTING;
    return ws_set_status (client, WS_ERR | WS_CLOSE, 1);
  }

  return ret;
}

/* Create a new SSL structure for a connection and perform handshake */
static void
handle_accept_ssl (WSClient * client, WSServer * server) {
  /* attempt to create SSL connection if we don't have one yet */
  if (!client->ssl) {
    if (!(client->ssl = SSL_new (server->ctx))) {
      LOG (("SSL: SSL_new, new SSL structure failed.\n"));
      return;
    }
    if (!SSL_set_fd (client->ssl, client->listener)) {
      LOG (("SSL: unable to set file descriptor\n"));
      return;
    }
  }

  /* attempt to initiate the TLS/SSL handshake */
  if (accept_ssl (client) == 0) {
    LOG (("SSL Accepted: %d %s\n", client->listener, client->remote_ip));
  }
}

/* Given the current status of the SSL buffer, perform that action.
 *
 * On error or if no SSL pending status, 1 is returned.
 * On success, the TLS/SSL pending action is called and 0 is returned */
static int
handle_ssl_pending_rw (int conn, WSServer * server, WSClient * client) {
  if (!wsconfig.use_ssl)
    return 1;

  /* trying to write but still waiting for a successful SSL_accept */
  if (client->sslstatus & WS_TLS_ACCEPTING) {
    handle_accept_ssl (client, server);
    return 0;
  }
  /* trying to read but still waiting for a successful SSL_read */
  if (client->sslstatus & WS_TLS_READING) {
    handle_reads (conn, server);
    return 0;
  }
  /* trying to write but still waiting for a successful SSL_write */
  if (client->sslstatus & WS_TLS_WRITING) {
    handle_writes (conn, server);
    return 0;
  }
  /* trying to write but still waiting for a successful SSL_shutdown */
  if (client->sslstatus & WS_TLS_SHUTTING) {
    if (shutdown_ssl (client) == 0)
      handle_read_close (conn, client, server);
    return 0;
  }

  return 1;
}

/* Write bytes to a TLS/SSL connection for a given client.
 *
 * On error or if no write is performed <=0 is returned.
 * On success, the number of bytes actually written to the TLS/SSL
 * connection are returned */
static int
send_ssl_buffer (WSClient * client, const char *buffer, int len) {
  int bytes = 0, err = 0;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ERR_clear_error ();
#endif
  if ((bytes = SSL_write (client->ssl, buffer, len)) > 0)
    return bytes;

  err = SSL_get_error (client->ssl, bytes);
  log_return_message (bytes, err, "SSL_write");

  switch (err) {
  case SSL_ERROR_WANT_WRITE:
    break;
  case SSL_ERROR_WANT_READ:
    client->sslstatus = WS_TLS_WRITING;
    break;
  case SSL_ERROR_SYSCALL:
    if ((bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)))
      break;
    /* The connection was shut down cleanly */
    /* FALLTHRU */
  case SSL_ERROR_ZERO_RETURN:
  case SSL_ERROR_WANT_X509_LOOKUP:
  default:
    return ws_set_status (client, WS_ERR | WS_CLOSE, -1);
  }

  return bytes;
}

/* Read data from the given client's socket and set a connection
 * status given the output of recv().
 *
 * On error, -1 is returned and the connection status is set.
 * On success, the number of bytes read is returned. */
static int
read_ssl_socket (WSClient * client, char *buffer, int size) {
  int bytes = 0, done = 0, err = 0;
  do {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    ERR_clear_error ();
#endif

    done = 0;
    if ((bytes = SSL_read (client->ssl, buffer, size)) > 0)
      break;

    err = SSL_get_error (client->ssl, bytes);
    log_return_message (bytes, err, "SSL_read");

    switch (err) {
    case SSL_ERROR_WANT_WRITE:
      client->sslstatus = WS_TLS_READING;
      done = 1;
      break;
    case SSL_ERROR_WANT_READ:
      done = 1;
      break;
    case SSL_ERROR_SYSCALL:
      if ((bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)))
        break;
      /* FALLTHRU */
    case SSL_ERROR_ZERO_RETURN:
    case SSL_ERROR_WANT_X509_LOOKUP:
    default:
      return ws_set_status (client, WS_ERR | WS_CLOSE, -1);
    }
  } while (SSL_pending (client->ssl) && !done);

  return bytes;
}
#endif

/* Get sockaddr, either IPv4 or IPv6 */
static void *
ws_get_raddr (struct sockaddr *sa) {
  if (sa->sa_family == AF_INET)
    return &(((struct sockaddr_in *) (void *) sa)->sin_addr);

  return &(((struct sockaddr_in6 *) (void *) sa)->sin6_addr);
}

/* Set the given file descriptor as NON BLOCKING. */
void
set_nonblocking (int sock) {
  if (fcntl (sock, F_SETFL, fcntl (sock, F_GETFL, 0) | O_NONBLOCK) == -1)
    FATAL ("Unable to set socket as non-blocking: %s.", strerror (errno));
}

/* Accept a new connection on a socket and add it to the list of
 * current connected clients.
 *
 * The newly assigned socket is returned. */
static int
accept_client (int listener, GSLList ** colist) {
  WSClient *client;
  struct sockaddr_storage raddr;
  int newfd;
  const void *src = NULL;
  socklen_t alen;

  alen = sizeof (raddr);
  if ((newfd = accept (listener, (struct sockaddr *) &raddr, &alen)) == -1)
    FATAL ("Unable to set accept: %s.", strerror (errno));

  if (newfd == -1) {
    LOG (("Unable to accept: %s.", strerror (errno)));
    return newfd;
  }
  src = ws_get_raddr ((struct sockaddr *) &raddr);

  /* malloc a new client */
  client = new_wsclient ();
  client->listener = newfd;
  inet_ntop (raddr.ss_family, src, client->remote_ip, INET6_ADDRSTRLEN);

  /* add up our new client to keep track of */
  if (*colist == NULL)
    *colist = list_create (client);
  else
    *colist = list_insert_prepend (*colist, client);

  /* make the socket non-blocking */
  set_nonblocking (client->listener);

  return newfd;
}

/* Extract the HTTP method.
 *
 * On error, or if not found, NULL is returned.
 * On success, the HTTP method is returned. */
static const char *
ws_get_method (const char *token) {
  const char *lookfor = NULL;

  if ((lookfor = "GET", !memcmp (token, "GET ", 4)) ||
      (lookfor = "get", !memcmp (token, "get ", 4)))
    return lookfor;
  return NULL;
}

/* Parse a request containing the method and protocol.
 *
 * On error, or unable to parse, NULL is returned.
 * On success, the HTTP request is returned and the method and
 * protocol are assigned to the corresponding buffers. */
static char *
ws_parse_request (char *line, char **method, char **protocol) {
  const char *meth;
  char *req = NULL, *request = NULL, *proto = NULL;
  ptrdiff_t rlen;

  if ((meth = ws_get_method (line)) == NULL) {
    return NULL;
  } else {
    req = line + strlen (meth);
    if ((proto = strstr (line, " HTTP/1.0")) == NULL &&
        (proto = strstr (line, " HTTP/1.1")) == NULL)
      return NULL;

    req++;
    if ((rlen = proto - req) <= 0)
      return NULL;

    request = xmalloc (rlen + 1);
    strncpy (request, req, rlen);
    request[rlen] = 0;

    (*method) = strtoupper (xstrdup (meth));
    (*protocol) = strtoupper (xstrdup (++proto));
  }

  return request;
}

/* Given a pair of key/values, assign it to our HTTP headers
 * structure. */
static void
ws_set_header_key_value (WSHeaders * headers, char *key, char *value) {
  if (strcasecmp ("Host", key) == 0)
    headers->host = xstrdup (value);
  else if (strcasecmp ("Origin", key) == 0)
    headers->origin = xstrdup (value);
  else if (strcasecmp ("Upgrade", key) == 0)
    headers->upgrade = xstrdup (value);
  else if (strcasecmp ("Connection", key) == 0)
    headers->connection = xstrdup (value);
  else if (strcasecmp ("Sec-WebSocket-Protocol", key) == 0)
    headers->ws_protocol = xstrdup (value);
  else if (strcasecmp ("Sec-WebSocket-Key", key) == 0)
    headers->ws_key = xstrdup (value);
  else if (strcasecmp ("Sec-WebSocket-Version", key) == 0)
    headers->ws_sock_ver = xstrdup (value);
  else if (strcasecmp ("User-Agent", key) == 0)
    headers->agent = xstrdup (value);
  else if (strcasecmp ("Referer", key) == 0)
    headers->referer = xstrdup (value);
}

/* Verify that the given HTTP headers were passed upon doing the
 * websocket handshake.
 *
 * On error, or header missing, 1 is returned.
 * On success, 0 is returned. */
static int
ws_verify_req_headers (WSHeaders * headers) {
  if (!headers->host)
    return 1;
  if (!headers->method)
    return 1;
  if (!headers->protocol)
    return 1;
  if (!headers->path)
    return 1;
  if (wsconfig.origin && !headers->origin)
    return 1;
  if (wsconfig.origin && strcasecmp (wsconfig.origin, headers->origin) != 0)
    return 1;
  if (!headers->connection)
    return 1;
  if (!headers->ws_key)
    return 1;
  if (!headers->ws_sock_ver)
    return 1;
  return 0;
}

/* From RFC2616, each header field consists of a name followed by a
 * colon (":") and the field value. Field names are case-insensitive.
 * The field value MAY be preceded by any amount of LWS, though a
 * single SP is preferred */
static int
ws_set_header_fields (char *line, WSHeaders * headers) {
  char *path = NULL, *method = NULL, *proto = NULL, *p, *value;

  if (line[0] == '\n' || line[0] == '\r')
    return 1;

  if ((strstr (line, "GET ")) || (strstr (line, "get "))) {
    if ((path = ws_parse_request (line, &method, &proto)) == NULL)
      return 1;
    headers->path = path;
    headers->method = method;
    headers->protocol = proto;

    return 0;
  }

  if ((p = strchr (line, ':')) == NULL)
    return 1;

  value = p + 1;
  while (p != line && isspace ((unsigned char) *(p - 1)))
    p--;

  if (p == line)
    return 1;

  *p = '\0';
  if (strpbrk (line, " \t") != NULL) {
    *p = ' ';
    return 1;
  }
  while (isspace ((unsigned char) *value))
    value++;

  ws_set_header_key_value (headers, line, value);

  return 0;
}

/* Parse the given HTTP headers and set the expected websocket
 * handshake.
 *
 * On error, or 1 is returned.
 * On success, 0 is returned. */
static int
parse_headers (WSHeaders * headers) {
  char *tmp = NULL;
  const char *buffer = headers->buf;
  const char *line = buffer, *next = NULL;
  int len = 0;

  while (line) {
    if ((next = strstr (line, "\r\n")) != NULL)
      len = (next - line);
    else
      len = strlen (line);

    if (len <= 0)
      return 1;

    tmp = xmalloc (len + 1);
    memcpy (tmp, line, len);
    tmp[len] = '\0';

    if (ws_set_header_fields (tmp, headers) == 1) {
      free (tmp);
      return 1;
    }

    free (tmp);
    line = next ? (next + 2) : NULL;

    if (next && strcmp (next, "\r\n\r\n") == 0)
      break;
  }

  return 0;
}

/* Set into a queue the data that couldn't be sent. */
static void
ws_queue_sockbuf (WSClient * client, const char *buffer, int len, int bytes) {
  WSQueue *queue = xcalloc (1, sizeof (WSQueue));

  if (bytes < 1)
    bytes = 0;

  queue->queued = xcalloc (len - bytes, sizeof (char));
  memcpy (queue->queued, buffer + bytes, len - bytes);
  queue->qlen = len - bytes;
  client->sockqueue = queue;

  client->status |= WS_SENDING;
}

/* Read data from the given client's socket and set a connection
 * status given the output of recv().
 *
 * On error, -1 is returned and the connection status is set.
 * On success, the number of bytes read is returned. */
static int
read_plain_socket (WSClient * client, char *buffer, int size) {
  int bytes = 0;

  bytes = recv (client->listener, buffer, size, 0);

  if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    return ws_set_status (client, WS_READING, bytes);
  else if (bytes == -1 || bytes == 0)
    return ws_set_status (client, WS_ERR | WS_CLOSE, bytes);

  return bytes;
}

/* Read data from the given client's socket and set a connection
 * status given the output of recv().
 *
 * On error, -1 is returned and the connection status is set.
 * On success, the number of bytes read is returned. */
static int
read_socket (WSClient * client, char *buffer, int size) {
#ifdef HAVE_LIBSSL
  if (wsconfig.use_ssl)
    return read_ssl_socket (client, buffer, size);
  else
    return read_plain_socket (client, buffer, size);
#else
  return read_plain_socket (client, buffer, size);
#endif
}

static int
send_plain_buffer (WSClient * client, const char *buffer, int len) {
  return send (client->listener, buffer, len, 0);
}

static int
send_buffer (WSClient * client, const char *buffer, int len) {
#ifdef HAVE_LIBSSL
  if (wsconfig.use_ssl)
    return send_ssl_buffer (client, buffer, len);
  else
    return send_plain_buffer (client, buffer, len);
#else
  return send_plain_buffer (client, buffer, len);
#endif
}

/* Attmpt to send the given buffer to the given socket.
 *
 * On error, -1 is returned and the connection status is set.
 * On success, the number of bytes sent is returned. */
static int
ws_respond_data (WSClient * client, const char *buffer, int len) {
  int bytes = 0;

  bytes = send_buffer (client, buffer, len);
  if (bytes == -1 && errno == EPIPE)
    return ws_set_status (client, WS_ERR | WS_CLOSE, bytes);

  /* did not send all of it... buffer it for a later attempt */
  if (bytes < len || (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)))
    ws_queue_sockbuf (client, buffer, len, bytes);

  return bytes;
}

/* Attempt to send the queued up client's data to the given socket.
 *
 * On error, -1 is returned and the connection status is set.
 * On success, the number of bytes sent is returned. */
static int
ws_respond_cache (WSClient * client) {
  WSQueue *queue = client->sockqueue;
  int bytes = 0;

  bytes = send_buffer (client, queue->queued, queue->qlen);
  if (bytes == -1 && errno == EPIPE)
    return ws_set_status (client, WS_ERR | WS_CLOSE, bytes);

  if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    return bytes;

  if (chop_nchars (queue->queued, bytes, queue->qlen) == 0)
    ws_clear_queue (client);
  else
    queue->qlen -= bytes;

  return bytes;
}

/* Attempt to realloc the current sent queue.
 *
 * On error, 1 is returned and the connection status is set.
 * On success, 0 is returned. */
static int
ws_realloc_send_buf (WSClient * client, const char *buf, int len) {
  WSQueue *queue = client->sockqueue;
  char *tmp = NULL;
  int newlen = 0;

  newlen = queue->qlen + len;
  tmp = realloc (queue->queued, newlen);
  if (tmp == NULL && newlen > 0) {
    ws_clear_queue (client);
    return ws_set_status (client, WS_ERR | WS_CLOSE, 1);
  }
  queue->queued = tmp;
  memcpy (queue->queued + queue->qlen, buf, len);
  queue->qlen += len;

  /* client probably  too slow, so stop queueing until everything is
   * sent */
  if (queue->qlen >= WS_THROTTLE_THLD)
    client->status |= WS_THROTTLING;

  return 0;
}

/* An entry point to attempt to send the client's data.
 *
 * On error, 1 is returned and the connection status is set.
 * On success, the number of bytes sent is returned. */
static int
ws_respond (WSClient * client, const char *buffer, int len) {
  int bytes = 0;

  /* attempt to send the whole buffer buffer */
  if (client->sockqueue == NULL)
    bytes = ws_respond_data (client, buffer, len);
  /* buffer not empty, just append new data iff we're not throttling the
   * client */
  else if (client->sockqueue != NULL && buffer != NULL && !(client->status & WS_THROTTLING)) {
    if (ws_realloc_send_buf (client, buffer, len) == 1)
      return bytes;
  }
  /* send from cache buffer */
  else {
    bytes = ws_respond_cache (client);
  }

  return bytes;
}

/* Encode a websocket frame (header/message) and attempt to send it
 * through the client's socket.
 *
 * On success, 0 is returned. */
static int
ws_send_frame (WSClient * client, WSOpcode opcode, const char *p, int sz) {
  unsigned char buf[32] = { 0 };
  char *frm = NULL;
  uint64_t payloadlen = 0, u64;
  int hsize = 2;

  if (sz < 126) {
    payloadlen = sz;
  } else if (sz < (1 << 16)) {
    payloadlen = WS_PAYLOAD_EXT16;
    hsize += 2;
  } else {
    payloadlen = WS_PAYLOAD_EXT64;
    hsize += 8;
  }

  buf[0] = 0x80 | ((uint8_t) opcode);
  switch (payloadlen) {
  case WS_PAYLOAD_EXT16:
    buf[1] = WS_PAYLOAD_EXT16;
    buf[2] = (sz & 0xff00) >> 8;
    buf[3] = (sz & 0x00ff) >> 0;
    break;
  case WS_PAYLOAD_EXT64:
    buf[1] = WS_PAYLOAD_EXT64;
    u64 = htobe64 (sz);
    memcpy (buf + 2, &u64, sizeof (uint64_t));
    break;
  default:
    buf[1] = (sz & 0xff);
  }
  frm = xcalloc (hsize + sz, sizeof (unsigned char));
  memcpy (frm, buf, hsize);
  if (p != NULL && sz > 0)
    memcpy (frm + hsize, p, sz);

  ws_respond (client, frm, hsize + sz);
  free (frm);

  return 0;
}

/* Send an error message to the given client.
 *
 * On success, the number of sent bytes is returned. */
static int
ws_error (WSClient * client, unsigned short code, const char *err) {
  unsigned int len;
  unsigned short code_be;
  char buf[128] = { 0 };

  len = 2;
  code_be = htobe16 (code);
  memcpy (buf, &code_be, 2);
  if (err)
    len += snprintf (buf + 2, sizeof buf - 4, "%s", err);

  return ws_send_frame (client, WS_OPCODE_CLOSE, buf, len);
}

/* Log hit to the access log.
 *
 * On success, the hit/entry is logged. */
static void
access_log (WSClient * client, int status_code) {
  WSHeaders *hdrs = client->headers;
  char buf[64] = { 0 };
  uint32_t elapsed = 0;
  struct timeval tv;
  struct tm time;
  char *req = NULL, *ref = NULL, *ua = NULL;

  gettimeofday (&tv, NULL);
  strftime (buf, sizeof (buf) - 1, "[%d/%b/%Y:%H:%M:%S %z]", localtime_r (&tv.tv_sec, &time));

  elapsed = (client->end_proc.tv_sec - client->start_proc.tv_sec) * 1000.0;
  elapsed += (client->end_proc.tv_usec - client->start_proc.tv_usec) / 1000.0;

  req = escape_http_request (hdrs->path);
  ref = escape_http_request (hdrs->referer);
  ua = escape_http_request (hdrs->agent);

  ACCESS_LOG (("%s ", client->remote_ip));
  ACCESS_LOG (("- - "));
  ACCESS_LOG (("%s ", buf));
  ACCESS_LOG (("\"%s ", hdrs->method));
  ACCESS_LOG (("%s ", req ? req : "-"));
  ACCESS_LOG (("%s\" ", hdrs->protocol));
  ACCESS_LOG (("%d ", status_code));
  ACCESS_LOG (("%d ", hdrs->buflen));
  ACCESS_LOG (("\"%s\" ", ref ? ref : "-"));
  ACCESS_LOG (("\"%s\" ", ua ? ua : "-"));
  ACCESS_LOG (("%u\n", elapsed));

  if (req)
    free (req);
  if (ref)
    free (ref);
  if (ua)
    free (ua);
}

/* Send an HTTP error status to the given client.
 *
 * On success, the number of sent bytes is returned. */
static int
http_error (WSClient * client, const char *buffer) {
  /* do access logging */
  gettimeofday (&client->end_proc, NULL);
  if (wsconfig.accesslog)
    access_log (client, 400);

  return ws_respond (client, buffer, strlen (buffer));
}

/* Compute the SHA1 for the handshake. */
static void
ws_sha1_digest (char *s, int len, unsigned char *digest) {
  SHA1_CTX sha;

  SHA1Init (&sha);
  SHA1Update (&sha, (uint8_t *) s, len);
  SHA1Final (digest, &sha);
}

/* Set the parsed websocket handshake headers. */
static void
ws_set_handshake_headers (WSHeaders * headers) {
  size_t klen = strlen (headers->ws_key);
  size_t mlen = strlen (WS_MAGIC_STR);
  size_t len = klen + mlen;
  char *s = xmalloc (klen + mlen + 1);
  uint8_t digest[SHA_DIGEST_LENGTH];

  memset (digest, 0, sizeof *digest);

  memcpy (s, headers->ws_key, klen);
  memcpy (s + klen, WS_MAGIC_STR, mlen + 1);

  ws_sha1_digest (s, len, digest);

  /* set response headers */
  headers->ws_accept = base64_encode ((unsigned char *) digest, sizeof (digest));
  headers->ws_resp = xstrdup (WS_SWITCH_PROTO_STR);

  if (!headers->upgrade)
    headers->upgrade = xstrdup ("websocket");
  if (!headers->connection)
    headers->upgrade = xstrdup ("Upgrade");

  free (s);
}

/* Send the websocket handshake headers to the given client.
 *
 * On success, the number of sent bytes is returned. */
static int
ws_send_handshake_headers (WSClient * client, WSHeaders * headers) {
  int bytes = 0;
  char *str = xstrdup ("");

  ws_append_str (&str, headers->ws_resp);
  ws_append_str (&str, CRLF);

  ws_append_str (&str, "Upgrade: ");
  ws_append_str (&str, headers->upgrade);
  ws_append_str (&str, CRLF);

  ws_append_str (&str, "Connection: ");
  ws_append_str (&str, headers->connection);
  ws_append_str (&str, CRLF);

  ws_append_str (&str, "Sec-WebSocket-Accept: ");
  ws_append_str (&str, headers->ws_accept);
  ws_append_str (&str, CRLF CRLF);

  bytes = ws_respond (client, str, strlen (str));
  free (str);

  return bytes;
}

/* Given the HTTP connection headers, attempt to parse the web socket
 * handshake headers.
 *
 * On success, the number of sent bytes is returned. */
static int
ws_get_handshake (WSClient * client, WSServer * server) {
  int bytes = 0, readh = 0;
  char *buf = NULL;

  if (client->headers == NULL)
    client->headers = new_wsheader ();

  buf = client->headers->buf;
  readh = client->headers->buflen;
  /* Probably the connection was closed before finishing handshake */
  if ((bytes = read_socket (client, buf + readh, WS_MAX_HEAD_SZ - readh)) < 1) {
    if (client->status & WS_CLOSE)
      http_error (client, WS_BAD_REQUEST_STR);
    return bytes;
  }
  client->headers->buflen += bytes;

  buf[client->headers->buflen] = '\0';  /* null-terminate */

  /* Must have a \r\n\r\n */
  if (strstr (buf, "\r\n\r\n") == NULL) {
    if (strlen (buf) < WS_MAX_HEAD_SZ)
      return ws_set_status (client, WS_READING, bytes);

    http_error (client, WS_BAD_REQUEST_STR);
    return ws_set_status (client, WS_CLOSE, bytes);
  }

  /* Ensure we have valid HTTP headers for the handshake */
  if (parse_headers (client->headers) != 0) {
    http_error (client, WS_BAD_REQUEST_STR);
    return ws_set_status (client, WS_CLOSE, bytes);
  }

  /* Ensure we have the required headers */
  if (ws_verify_req_headers (client->headers) != 0) {
    http_error (client, WS_BAD_REQUEST_STR);
    return ws_set_status (client, WS_CLOSE, bytes);
  }

  ws_set_handshake_headers (client->headers);

  /* handshake response */
  ws_send_handshake_headers (client, client->headers);

  /* upon success, call onopen() callback */
  if (server->onopen && wsconfig.strict && !wsconfig.echomode)
    server->onopen (server->pipeout, client);
  client->headers->reading = 0;

  /* do access logging */
  gettimeofday (&client->end_proc, NULL);
  if (wsconfig.accesslog)
    access_log (client, 101);
  LOG (("Active: %d\n", list_count (server->colist)));

  return ws_set_status (client, WS_OK, bytes);
}

/* Send a data message to the given client.
 *
 * On success, 0 is returned. */
int
ws_send_data (WSClient * client, WSOpcode opcode, const char *p, int sz) {
  char *buf = NULL;

  buf = sanitize_utf8 (p, sz);
  ws_send_frame (client, opcode, buf, sz);
  free (buf);

  return 0;
}

/* Read a websocket frame's header.
 *
 * On success, the number of bytesr read is returned. */
static int
ws_read_header (WSClient * client, WSFrame * frm, int pos, int need) {
  char *buf = frm->buf;
  int bytes = 0;

  /* read the first 2 bytes for basic frame info */
  if ((bytes = read_socket (client, buf + pos, need)) < 1) {
    if (client->status & WS_CLOSE)
      ws_error (client, WS_CLOSE_UNEXPECTED, "Unable to read header");
    return bytes;
  }
  frm->buflen += bytes;
  frm->buf[frm->buflen] = '\0'; /* null-terminate */

  return bytes;
}

/* Read a websocket frame's payload.
 *
 * On success, the number of bytesr read is returned. */
static int
ws_read_payload (WSClient * client, WSMessage * msg, int pos, int need) {
  char *buf = msg->payload;
  int bytes = 0;

  /* read the first 2 bytes for basic frame info */
  if ((bytes = read_socket (client, buf + pos, need)) < 1) {
    if (client->status & WS_CLOSE)
      ws_error (client, WS_CLOSE_UNEXPECTED, "Unable to read payload");
    return bytes;
  }
  msg->buflen += bytes;
  msg->payloadsz += bytes;

  return bytes;
}

/* Set the basic frame headers on a frame structure.
 *
 * On success, 0 is returned. */
static int
ws_set_front_header_fields (WSClient * client) {
  WSFrame **frm = &client->frame;
  char *buf = (*frm)->buf;

  (*frm)->fin = WS_FRM_FIN (*(buf));
  (*frm)->masking = WS_FRM_MASK (*(buf + 1));
  (*frm)->opcode = WS_FRM_OPCODE (*(buf));
  (*frm)->res = WS_FRM_R1 (*(buf)) || WS_FRM_R2 (*(buf)) || WS_FRM_R3 (*(buf));

  /* should be masked and can't be using RESVd  bits */
  if (!(*frm)->masking || (*frm)->res)
    return ws_set_status (client, WS_ERR | WS_CLOSE, 1);

  return 0;
}

/* Unmask the payload given the current frame's masking key. */
static void
ws_unmask_payload (char *buf, int len, int offset, unsigned char mask[]) {
  int i, j = 0;

  /* unmask data */
  for (i = offset; i < len; ++i, ++j) {
    buf[i] ^= mask[j % 4];
  }
}

/* Close a websocket connection. */
static int
ws_handle_close (WSClient * client) {
  client->status = WS_ERR | WS_CLOSE;
  return ws_send_frame (client, WS_OPCODE_CLOSE, NULL, 0);
}

/* Handle a websocket error.
 *
 * On success, the number of bytes sent is returned. */
static int
ws_handle_err (WSClient * client, unsigned short code, WSStatus status, const char *m) {
  client->status = status;
  return ws_error (client, code, m);
}

/* Handle a websocket pong. */
static void
ws_handle_pong (WSClient * client) {
  WSFrame **frm = &client->frame;

  if (!(*frm)->fin) {
    ws_handle_err (client, WS_CLOSE_PROTO_ERR, WS_ERR | WS_CLOSE, NULL);
    return;
  }
  ws_free_message (client);
}

/* Handle a websocket ping from the client and it attempts to send
 * back a pong as soon as possible. */
static void
ws_handle_ping (WSClient * client) {
  WSFrame **frm = &client->frame;
  WSMessage **msg = &client->message;
  char *buf = NULL, *tmp = NULL;
  int pos = 0, len = (*frm)->payloadlen, newlen = 0;

  /* RFC states that Control frames themselves MUST NOT be
   * fragmented. */
  if (!(*frm)->fin) {
    ws_handle_err (client, WS_CLOSE_PROTO_ERR, WS_ERR | WS_CLOSE, NULL);
    return;
  }

  /* Control frames are only allowed to have payload up to and
   * including 125 octets */
  if ((*frm)->payloadlen > 125) {
    ws_handle_err (client, WS_CLOSE_PROTO_ERR, WS_ERR | WS_CLOSE, NULL);
    return;
  }

  /* No payload from ping */
  if (len == 0) {
    ws_send_frame (client, WS_OPCODE_PONG, NULL, 0);
    return;
  }

  /* Copy the ping payload */
  pos = (*msg)->payloadsz - len;
  buf = xcalloc (len, sizeof (char));
  memcpy (buf, (*msg)->payload + pos, len);

  /* Unmask it */
  ws_unmask_payload (buf, len, 0, (*frm)->mask);

  /* Resize the current payload (keep an eye on this realloc) */
  newlen = (*msg)->payloadsz - len;
  tmp = realloc ((*msg)->payload, newlen);
  if (tmp == NULL && newlen > 0) {
    free ((*msg)->payload);
    free (buf);

    (*msg)->payload = NULL;
    client->status = WS_ERR | WS_CLOSE;
    return;
  }

  (*msg)->payload = tmp;
  (*msg)->payloadsz -= len;

  ws_send_frame (client, WS_OPCODE_PONG, buf, len);

  (*msg)->buflen = 0;   /* done with the current frame's payload */
  /* Control frame injected in the middle of a fragmented message. */
  if (!(*msg)->fragmented) {
    ws_free_message (client);
  }
  free (buf);
}

/* Ensure we have valid UTF-8 text payload.
 *
 * On error, or if the message is invalid, 1 is returned.
 * On success, or if the message is valid, 0 is returned. */
int
ws_validate_string (const char *str, int len) {
  uint32_t state = UTF8_VALID;

  if (verify_utf8 (&state, str, len) == UTF8_INVAL) {
    LOG (("Invalid UTF8 data!\n"));
    return 1;
  }
  if (state != UTF8_VALID) {
    LOG (("Invalid UTF8 data!\n"));
    return 1;
  }

  return 0;
}

/* It handles a text or binary message frame from the client. */
static void
ws_handle_text_bin (WSClient * client, WSServer * server) {
  WSFrame **frm = &client->frame;
  WSMessage **msg = &client->message;
  int offset = (*msg)->mask_offset;

  /* All data frames after the initial data frame must have opcode 0 */
  if ((*msg)->fragmented && (*frm)->opcode != WS_OPCODE_CONTINUATION) {
    client->status = WS_ERR | WS_CLOSE;
    return;
  }

  /* RFC states that there is a new masking key per frame, therefore,
   * time to unmask... */
  ws_unmask_payload ((*msg)->payload, (*msg)->payloadsz, offset, (*frm)->mask);
  /* Done with the current frame's payload */
  (*msg)->buflen = 0;
  /* Reading a fragmented frame */
  (*msg)->fragmented = 1;

  if (!(*frm)->fin)
    return;

  /* validate text data encoded as UTF-8 */
  if ((*msg)->opcode == WS_OPCODE_TEXT) {
    if (ws_validate_string ((*msg)->payload, (*msg)->payloadsz) != 0) {
      ws_handle_err (client, WS_CLOSE_INVALID_UTF8, WS_ERR | WS_CLOSE, NULL);
      return;
    }
  }

  if ((*msg)->opcode != WS_OPCODE_CONTINUATION && server->onmessage) {
    /* just echo the message to the client */
    if (wsconfig.echomode)
      ws_send_data (client, (*msg)->opcode, (*msg)->payload, (*msg)->payloadsz);
    /* just pipe out the message */
    else if (!wsconfig.strict)
      ws_write_fifo (server->pipeout, (*msg)->payload, (*msg)->payloadsz);
    else
      server->onmessage (server->pipeout, client);
  }
  ws_free_message (client);
}

/* Depending on the frame opcode, then we take certain decisions. */
static void
ws_manage_payload_opcode (WSClient * client, WSServer * server) {
  WSFrame **frm = &client->frame;
  WSMessage **msg = &client->message;

  switch ((*frm)->opcode) {
  case WS_OPCODE_CONTINUATION:
    LOG (("CONTINUATION\n"));
    /* first frame can't be a continuation frame */
    if (!(*msg)->fragmented) {
      client->status = WS_ERR | WS_CLOSE;
      break;
    }
    ws_handle_text_bin (client, server);
    break;
  case WS_OPCODE_TEXT:
  case WS_OPCODE_BIN:
    LOG (("TEXT\n"));
    client->message->opcode = (*frm)->opcode;
    ws_handle_text_bin (client, server);
    break;
  case WS_OPCODE_PONG:
    LOG (("PONG\n"));
    ws_handle_pong (client);
    break;
  case WS_OPCODE_PING:
    LOG (("PING\n"));
    ws_handle_ping (client);
    break;
  default:
    LOG (("CLOSE\n"));
    ws_handle_close (client);
  }
}

/* Set the extended payload length into the given pointer. */
static void
ws_set_extended_header_size (const char *buf, int *extended) {
  uint64_t payloadlen = 0;
  /* determine the payload length, else read more data */
  payloadlen = WS_FRM_PAYLOAD (*(buf + 1));
  switch (payloadlen) {
  case WS_PAYLOAD_EXT16:
    *extended = 2;
    break;
  case WS_PAYLOAD_EXT64:
    *extended = 8;
    break;
  }
}

/* Set the extended payload length into our frame structure. */
static void
ws_set_payloadlen (WSFrame * frm, const char *buf) {
  uint64_t payloadlen = 0, len64;
  uint16_t len16;

  /* determine the payload length, else read more data */
  payloadlen = WS_FRM_PAYLOAD (*(buf + 1));
  switch (payloadlen) {
  case WS_PAYLOAD_EXT16:
    memcpy (&len16, (buf + 2), sizeof (uint16_t));
    frm->payloadlen = ntohs (len16);
    break;
  case WS_PAYLOAD_EXT64:
    memcpy (&len64, (buf + 2), sizeof (uint64_t));
    frm->payloadlen = be64toh (len64);
    break;
  default:
    frm->payloadlen = payloadlen;
  }
}

/* Set the masking key into our frame structure. */
static void
ws_set_masking_key (WSFrame * frm, const char *buf) {
  uint64_t payloadlen = 0;

  /* determine the payload length, else read more data */
  payloadlen = WS_FRM_PAYLOAD (*(buf + 1));
  switch (payloadlen) {
  case WS_PAYLOAD_EXT16:
    memcpy (&frm->mask, buf + 4, sizeof (frm->mask));
    break;
  case WS_PAYLOAD_EXT64:
    memcpy (&frm->mask, buf + 10, sizeof (frm->mask));
    break;
  default:
    memcpy (&frm->mask, buf + 2, sizeof (frm->mask));
  }
}

/* Attempt to read the frame's header and set the relavant data into
 * our frame structure.
 *
 * On error, or if no data available to read, the number of bytes is
 * returned and the appropriate connection status is set.
 * On success, the number of bytes is returned. */
static int
ws_get_frm_header (WSClient * client) {
  WSFrame **frm = NULL;
  int bytes = 0, readh = 0, need = 0, offset = 0, extended = 0;

  if (client->frame == NULL)
    client->frame = new_wsframe ();

  frm = &client->frame;

  /* Read the first 2 bytes for basic frame info */
  readh = (*frm)->buflen;       /* read from header so far */
  need = 2 - readh;     /* need to read */
  if (need > 0) {
    if ((bytes = ws_read_header (client, (*frm), readh, need)) < 1)
      return bytes;
    if (bytes != need)
      return ws_set_status (client, WS_READING, bytes);
  }
  offset += 2;

  if (ws_set_front_header_fields (client) != 0)
    return bytes;

  ws_set_extended_header_size ((*frm)->buf, &extended);
  /* read the extended header */
  readh = (*frm)->buflen;       /* read from header so far */
  need = (extended + offset) - readh;   /* read from header field so far */
  if (need > 0) {
    if ((bytes = ws_read_header (client, (*frm), readh, need)) < 1)
      return bytes;
    if (bytes != need)
      return ws_set_status (client, WS_READING, bytes);
  }
  offset += extended;

  /* read the masking key */
  readh = (*frm)->buflen;       /* read from header so far */
  need = (4 + offset) - readh;
  if (need > 0) {
    if ((bytes = ws_read_header (client, (*frm), readh, need)) < 1)
      return bytes;
    if (bytes != need)
      return ws_set_status (client, WS_READING, bytes);
  }
  offset += 4;

  ws_set_payloadlen ((*frm), (*frm)->buf);
  ws_set_masking_key ((*frm), (*frm)->buf);

  if ((*frm)->payloadlen > wsconfig.max_frm_size) {
    ws_error (client, WS_CLOSE_TOO_LARGE, "Frame is too big");
    return ws_set_status (client, WS_ERR | WS_CLOSE, bytes);
  }

  (*frm)->buflen = 0;
  (*frm)->reading = 0;
  (*frm)->payload_offset = offset;

  return ws_set_status (client, WS_OK, bytes);
}

/* Attempt to realloc the message payload.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
static int
ws_realloc_frm_payload (WSFrame * frm, WSMessage * msg) {
  char *tmp = NULL;
  uint64_t newlen = 0;

  newlen = msg->payloadsz + frm->payloadlen;
  tmp = realloc (msg->payload, newlen);
  if (tmp == NULL && newlen > 0) {
    free (msg->payload);
    msg->payload = NULL;
    return 1;
  }
  msg->payload = tmp;

  return 0;
}

/* Attempt to read the frame's payload and set the relavant data into
 * our message structure.
 *
 * On error, or if no data available to read, the number of bytes is
 * returned and the appropriate connection status is set.
 * On success, the number of bytes is returned. */
static int
ws_get_frm_payload (WSClient * client, WSServer * server) {
  WSFrame **frm = NULL;
  WSMessage **msg = NULL;
  int bytes = 0, readh = 0, need = 0;

  if (client->message == NULL)
    client->message = new_wsmessage ();

  frm = &client->frame;
  msg = &client->message;

  /* message within the same frame */
  if ((*msg)->payload == NULL && (*frm)->payloadlen)
    (*msg)->payload = xcalloc ((*frm)->payloadlen, sizeof (char));
  /* handle a new frame */
  else if ((*msg)->buflen == 0 && (*frm)->payloadlen) {
    if (ws_realloc_frm_payload ((*frm), (*msg)) == 1)
      return ws_set_status (client, WS_ERR | WS_CLOSE, 0);
  }

  readh = (*msg)->buflen;       /* read from so far */
  need = (*frm)->payloadlen - readh;    /* need to read */
  if (need > 0) {
    if ((bytes = ws_read_payload (client, (*msg), (*msg)->payloadsz, need)) < 0)
      return bytes;
    if (bytes != need)
      return ws_set_status (client, WS_READING, bytes);
  }

  (*msg)->mask_offset = (*msg)->payloadsz - (*msg)->buflen;

  ws_manage_payload_opcode (client, server);
  ws_free_frame (client);

  return bytes;
}

/* Determine if we need to read a frame's header or its payload.
 *
 * On success, the number of bytes is returned. */
static int
ws_get_message (WSClient * client, WSServer * server) {
  int bytes = 0;
  if ((client->frame == NULL) || (client->frame->reading))
    if ((bytes = ws_get_frm_header (client)) < 1 || client->frame->reading)
      return bytes;
  return ws_get_frm_payload (client, server);
}

/* Determine if we need to read an HTTP request or a websocket frame.
 *
 * On success, the number of bytes is returned. */
static int
read_client_data (WSClient * client, WSServer * server) {
  int bytes = 0;

  /* Handshake */
  if ((!(client->headers) || (client->headers->reading)))
    bytes = ws_get_handshake (client, server);
  /* Message */
  else
    bytes = ws_get_message (client, server);

  return bytes;
}

/* Handle a tcp close connection. */
static void
handle_tcp_close (int conn, WSClient * client, WSServer * server) {
#ifdef HAVE_LIBSSL
  if (client->ssl)
    shutdown_ssl (client);
#endif

  shutdown (conn, SHUT_RDWR);
  /* upon close, call onclose() callback */
  if (server->onclose && wsconfig.strict && !wsconfig.echomode)
    (*server->onclose) (server->pipeout, client);

  /* do access logging */
  gettimeofday (&client->end_proc, NULL);
  if (wsconfig.accesslog)
    access_log (client, 200);

  /* errored out while parsing a frame or a message */
  if (client->status & WS_ERR) {
    ws_clear_queue (client);
    ws_free_frame (client);
    ws_free_message (client);
  }

  server->closing = 0;
  ws_close (conn);

#ifdef HAVE_LIBSSL
  if (client->ssl)
    SSL_free (client->ssl);
  client->ssl = NULL;
#endif

  /* remove client from our list */
  ws_remove_client_from_list (client, server);
  LOG (("Active: %d\n", list_count (server->colist)));
}

/* Handle a tcp read close connection. */
static void
handle_read_close (int conn, WSClient * client, WSServer * server) {
  if (client->status & WS_SENDING) {
    server->closing = 1;
    return;
  }
  handle_tcp_close (conn, client, server);
}

/* Handle a new socket connection. */
static void
handle_accept (int listener, WSServer * server) {
  WSClient *client = NULL;
  int newfd;

  newfd = accept_client (listener, &server->colist);
  if (newfd == -1)
    return;

  client = ws_get_client_from_list (newfd, &server->colist);
  if (newfd > FD_SETSIZE - 1) {
    LOG (("Too busy: %d %s.\n", newfd, client->remote_ip));

    http_error (client, WS_TOO_BUSY_STR);
    handle_read_close (newfd, client, server);
    return;
  }
#ifdef HAVE_LIBSSL
  /* set flag to do TLS handshake */
  if (wsconfig.use_ssl)
    client->sslstatus |= WS_TLS_ACCEPTING;
#endif

  LOG (("Accepted: %d %s\n", newfd, client->remote_ip));
}

/* Handle a tcp read. */
static void
handle_reads (int conn, WSServer * server) {
  WSClient *client = NULL;

  if (!(client = ws_get_client_from_list (conn, &server->colist)))
    return;

#ifdef HAVE_LIBSSL
  if (handle_ssl_pending_rw (conn, server, client) == 0)
    return;
#endif

  /* *INDENT-OFF* */
  client->start_proc = client->end_proc = (struct timeval) {0};
  /* *INDENT-ON* */
  gettimeofday (&client->start_proc, NULL);
  read_client_data (client, server);
  /* An error ocurred while reading data or connection closed */
  if ((client->status & WS_CLOSE)) {
    handle_read_close (conn, client, server);
  }
}

/* Handle a tcp write close connection. */
static void
handle_write_close (int conn, WSClient * client, WSServer * server) {
  handle_tcp_close (conn, client, server);
}

/* Handle a tcp write. */
static void
handle_writes (int conn, WSServer * server) {
  WSClient *client = NULL;

  if (!(client = ws_get_client_from_list (conn, &server->colist)))
    return;

#ifdef HAVE_LIBSSL
  if (handle_ssl_pending_rw (conn, server, client) == 0)
    return;
#endif

  ws_respond (client, NULL, 0); /* buffered data */
  /* done sending data */
  if (client->sockqueue == NULL)
    client->status &= ~WS_SENDING;

  /* An error ocurred while sending data or while reading data but still
   * waiting from the last send() from the server to the client.  e.g.,
   * sending status code */
  if ((client->status & WS_CLOSE) && !(client->status & WS_SENDING))
    handle_write_close (conn, client, server);
}

/* Handle reads/writes on a TCP connection. */
static void
ws_listen (int listener, int conn, WSServer * server) {
  /* handle new connections */
  if (FD_ISSET (conn, &fdstate.rfds) && conn == listener)
    handle_accept (listener, server);
  /* handle data from a client */
  else if (FD_ISSET (conn, &fdstate.rfds) && conn != listener)
    handle_reads (conn, server);
  /* handle sending data to a client */
  else if (FD_ISSET (conn, &fdstate.wfds) && conn != listener)
    handle_writes (conn, server);
}

/* Create named pipe (FIFO) with the given pipe name.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
int
ws_setfifo (const char *pipename) {
  struct stat fistat;
  const char *f = pipename;

  if (access (f, F_OK) == 0)
    return 0;

  if (mkfifo (f, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) < 0)
    FATAL ("Unable to set fifo: %s.", strerror (errno));
  if (stat (f, &fistat) < 0)
    FATAL ("Unable to stat fifo: %s.", strerror (errno));
  if (!S_ISFIFO (fistat.st_mode))
    FATAL ("pipe is not a fifo: %s.", strerror (errno));

  return 0;
}

/* Open a named pipe (FIFO) for input to the server (reader). */
static void
ws_openfifo_in (WSPipeIn * pipein) {
  ws_setfifo (wsconfig.pipein);
  /* we should be able to open it at as reader */
  if ((pipein->fd = open (wsconfig.pipein, O_RDWR | O_NONBLOCK)) < 0)
    FATAL ("Unable to open fifo in: %s.", strerror (errno));
}


/* Open a named pipe (FIFO) for output from the server (writer). */
static int
ws_openfifo_out (WSPipeOut * pipeout) {
  int status = 0;

  ws_setfifo (wsconfig.pipeout);
  status = open (wsconfig.pipeout, O_WRONLY | O_NONBLOCK);
  /* will attempt on the next write */
  if (status == -1 && errno == ENXIO)
    LOG (("Unable to open fifo out: %s.\n", strerror (errno)));
  else if (status < 0)
    FATAL ("Unable to open fifo out: %s.", strerror (errno));
  pipeout->fd = status;

  if (status != -1 && status > max_file_fd)
    max_file_fd = status;

  return status;
}

/* Set a new named pipe for incoming messages and one for outgoing
 * messages from the client. */
static void
ws_fifo (WSServer * server) {
  ws_openfifo_in (server->pipein);
  ws_openfifo_out (server->pipeout);
}

/* Clear the queue for an outgoing named pipe. */
static void
clear_fifo_queue (WSPipeOut * pipeout) {
  WSQueue **queue = &pipeout->fifoqueue;
  if (!(*queue))
    return;

  if ((*queue)->queued)
    free ((*queue)->queued);
  (*queue)->queued = NULL;
  (*queue)->qlen = 0;

  free ((*queue));
  (*queue) = NULL;
}

/* Attempt to realloc the current sent queue for an outgoing named pip
 * (FIFO).
 *
 * On error, 1 is returned and the connection status is closed and
 * reopened.
 * On success, 0 is returned. */
static int
ws_realloc_fifobuf (WSPipeOut * pipeout, const char *buf, int len) {
  WSQueue *queue = pipeout->fifoqueue;
  char *tmp = NULL;
  int newlen = 0;

  newlen = queue->qlen + len;
  tmp = realloc (queue->queued, newlen);
  if (tmp == NULL && newlen > 0) {
    close (pipeout->fd);
    clear_fifo_queue (pipeout);
    ws_openfifo_out (pipeout);
    return 1;
  }

  queue->queued = tmp;
  memcpy (queue->queued + queue->qlen, buf, len);
  queue->qlen += len;

  return 0;
}

/* Set into a queue the data that couldn't be sent in the outgoing
 * FIFO. */
static void
ws_queue_fifobuf (WSPipeOut * pipeout, const char *buffer, int len, int bytes) {
  WSQueue **queue = &pipeout->fifoqueue;

  if (bytes < 1)
    bytes = 0;

  (*queue) = xcalloc (1, sizeof (WSQueue));
  (*queue)->queued = xcalloc (len - bytes, sizeof (char));
  memcpy ((*queue)->queued, buffer + bytes, len - bytes);
  (*queue)->qlen = len - bytes;

  pipeout->status |= WS_SENDING;
}

/* Attmpt to send the given buffer to the given outgoing FIFO.
 *
 * On error, the data is queued up.
 * On success, the number of bytes sent is returned. */
static int
ws_write_fifo_data (WSPipeOut * pipeout, char *buffer, int len) {
  int bytes = 0;

  bytes = write (pipeout->fd, buffer, len);

  /* At this point, the reader probably closed the pipe, so a cheap *hack* for
   * this is to close the pipe on our end and attempt to reopen it. If unable to
   * do so, then let it be -1 and try on the next attempt to write. */
  if (bytes == -1 && errno == EPIPE) {
    close (pipeout->fd);
    ws_openfifo_out (pipeout);
    return bytes;
  }
  if (bytes < len || (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)))
    ws_queue_fifobuf (pipeout, buffer, len, bytes);

  return bytes;
}

/* Attempt to send the queued up client's data through the outgoing
 * named pipe (FIFO) .
 *
 * On error, 1 is returned and the connection status is set.
 * On success, the number of bytes sent is returned. */
static int
ws_write_fifo_cache (WSPipeOut * pipeout) {
  WSQueue *queue = pipeout->fifoqueue;
  int bytes = 0;

  bytes = write (pipeout->fd, queue->queued, queue->qlen);

  /* At this point, the reader probably closed the pipe, so a cheap *hack* for
   * this is to close the pipe on our end and attempt to reopen it. If unable to
   * do so, then let it be -1 and try on the next attempt to write. */
  if (bytes == -1 && errno == EPIPE) {
    close (pipeout->fd);
    ws_openfifo_out (pipeout);
    return bytes;
  }

  if (chop_nchars (queue->queued, bytes, queue->qlen) == 0)
    clear_fifo_queue (pipeout);
  else
    queue->qlen -= bytes;

  return bytes;
}

/* An entry point to attempt to send the client's data into an
 * outgoing named pipe (FIFO).
 *
 * On success, the number of bytes sent is returned. */
int
ws_write_fifo (WSPipeOut * pipeout, char *buffer, int len) {
  int bytes = 0;

  if (pipeout->fd == -1 && ws_openfifo_out (pipeout) == -1)
    return bytes;

  /* attempt to send the whole buffer buffer */
  if (pipeout->fifoqueue == NULL)
    bytes = ws_write_fifo_data (pipeout, buffer, len);
  /* buffer not empty, just append new data */
  else if (pipeout->fifoqueue != NULL && buffer != NULL) {
    if (ws_realloc_fifobuf (pipeout, buffer, len) == 1)
      return bytes;
  }
  /* send from cache buffer */
  else {
    bytes = ws_write_fifo_cache (pipeout);
  }

  if (pipeout->fifoqueue == NULL)
    pipeout->status &= ~WS_SENDING;

  return bytes;
}

/* Clear an incoming FIFO packet and header data. */
static void
clear_fifo_packet (WSPipeIn * pipein) {
  memset (pipein->hdr, 0, sizeof (pipein->hdr));
  pipein->hlen = 0;

  if (pipein->packet == NULL)
    return;

  if (pipein->packet->data)
    free (pipein->packet->data);
  free (pipein->packet);
  pipein->packet = NULL;
}

/* Broadcast to all connected clients the given message. */
static int
ws_broadcast_fifo (void *value, void *user_data) {
  WSClient *client = value;
  WSPacket *packet = user_data;

  if (client == NULL || user_data == NULL)
    return 1;
  /* no handshake for this client */
  if (client->headers == NULL || client->headers->ws_accept == NULL)
    return 1;

  ws_send_data (client, packet->type, packet->data, packet->size);

  return 0;
}

/* Send a message from the incoming named pipe to specific client
 * given the socket id. */
static void
ws_send_strict_fifo_to_client (WSServer * server, int listener, WSPacket * pa) {
  WSClient *client = NULL;

  if (!(client = ws_get_client_from_list (listener, &server->colist)))
    return;
  /* no handshake for this client */
  if (client->headers == NULL || client->headers->ws_accept == NULL)
    return;
  ws_send_data (client, pa->type, pa->data, pa->len);
}

/* Attempt to read message from a named pipe (FIFO).
 *
 * On error, -1 is returned.
 * On success, the number of bytes read is returned. */
int
ws_read_fifo (int fd, char *buf, int *buflen, int pos, int need) {
  int bytes = 0;

  bytes = read (fd, buf + pos, need);
  if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    return bytes;
  else if (bytes == -1)
    return bytes;
  *buflen += bytes;

  return bytes;
}

/* Pack the given value into a network byte order.
 *
 * On success, the number size of uint32_t is returned. */
size_t
pack_uint32 (void *buf, uint32_t val) {
  uint32_t v32 = htonl (val);
  memcpy (buf, &v32, sizeof (uint32_t));

  return sizeof (uint32_t);
}

/* Unpack the given value into a host byte order.
 *
 * On success, the number size of uint32_t is returned. */
size_t
unpack_uint32 (const void *buf, uint32_t * val) {
  uint32_t v32 = 0;
  memcpy (&v32, buf, sizeof (uint32_t));
  *val = ntohl (v32);

  return sizeof (uint32_t);
}

/* Ensure the fields coming from the named pipe are valid.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
static int
validate_fifo_packet (uint32_t listener, uint32_t type, int size) {
  if (listener > FD_SETSIZE) {
    LOG (("Invalid listener\n"));
    return 1;
  }

  if (type != WS_OPCODE_TEXT && type != WS_OPCODE_BIN) {
    LOG (("Invalid fifo packet type\n"));
    return 1;
  }

  if (size > wsconfig.max_frm_size) {
    LOG (("Invalid fifo packet size\n"));
    return 1;
  }

  return 0;
}

/* Handle reading and sending the incoming data from the named pipe on
 * strict mode. */
static void
handle_strict_fifo (WSServer * server) {
  WSPipeIn *pi = server->pipein;
  WSPacket **pa = &pi->packet;
  int bytes = 0, readh = 0, need = 0;

  char *ptr = NULL;
  uint32_t listener = 0, type = 0, size = 0;

  readh = pi->hlen;     /* read from header so far */
  need = HDR_SIZE - readh;      /* need to read */
  if (need > 0) {
    if ((bytes = ws_read_fifo (pi->fd, pi->hdr, &pi->hlen, readh, need)) < 0)
      return;
    if (bytes != need)
      return;
  }

  /* unpack size, and type */
  ptr = pi->hdr;
  ptr += unpack_uint32 (ptr, &listener);
  ptr += unpack_uint32 (ptr, &type);
  ptr += unpack_uint32 (ptr, &size);

  if (validate_fifo_packet (listener, type, size) == 1) {
    close (pi->fd);
    clear_fifo_packet (pi);
    ws_openfifo_in (pi);
    return;
  }

  if ((*pa) == NULL) {
    (*pa) = xcalloc (1, sizeof (WSPacket));
    (*pa)->type = type;
    (*pa)->size = size;
    (*pa)->data = xcalloc (size, sizeof (char));
  }

  readh = (*pa)->len;   /* read from payload so far */
  need = (*pa)->size - readh;   /* need to read */
  if (need > 0) {
    if ((bytes = ws_read_fifo (pi->fd, (*pa)->data, &(*pa)->len, readh, need)) < 0)
      return;
    if (bytes != need)
      return;
  }

  /* no clients to send data to */
  if (list_count (server->colist) == 0) {
    clear_fifo_packet (pi);
    return;
  }

  /* Either send it to a specific client or brodcast message to all
   * clients */
  if (listener != 0)
    ws_send_strict_fifo_to_client (server, listener, *pa);
  else
    list_foreach (server->colist, ws_broadcast_fifo, *pa);
  clear_fifo_packet (pi);
}

/* Handle reading and sending the incoming data from the named pipe on
 * a fixed buffer mode. */
static void
handle_fixed_fifo (WSServer * server) {
  WSPipeIn *pi = server->pipein;
  WSPacket **pa = &pi->packet;

  int bytes = 0;
  char buf[PIPE_BUF] = { 0 };

  if ((bytes = read (pi->fd, buf, PIPE_BUF)) < 0)
    return;

  buf[bytes] = '\0';    /* null-terminate */
  if (ws_validate_string (buf, bytes) != 0)
    return;

  (*pa) = xcalloc (1, sizeof (WSPacket));
  (*pa)->type = WS_OPCODE_TEXT;
  (*pa)->size = bytes;
  (*pa)->data = xstrdup (buf);

  /* no clients to send data to */
  if (list_count (server->colist) == 0) {
    clear_fifo_packet (pi);
    return;
  }

  /* brodcast message to all clients */
  list_foreach (server->colist, ws_broadcast_fifo, *pa);
  clear_fifo_packet (pi);
}

/* Determine which mode should use the incoming message from the FIFO. */
static void
handle_fifo (WSServer * server) {
  if (wsconfig.strict)
    handle_strict_fifo (server);
  else
    handle_fixed_fifo (server);
}

/* Creates an endpoint for communication and start listening for
 * connections on a socket */
static void
ws_socket (int *listener) {
  int ov = 1;
  struct addrinfo hints, *ai;

  /* get a socket and bind it */
  memset (&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  /*hints.ai_flags = AI_PASSIVE; */
  if (getaddrinfo (wsconfig.host, wsconfig.port, &hints, &ai) != 0)
    FATAL ("Unable to set server: %s.", gai_strerror (errno));

  /* Create a TCP socket.  */
  *listener = socket (ai->ai_family, ai->ai_socktype, ai->ai_protocol);

  /* Options */
  if (setsockopt (*listener, SOL_SOCKET, SO_REUSEADDR, &ov, sizeof (ov)) == -1)
    FATAL ("Unable to set setsockopt: %s.", strerror (errno));

  /* Bind the socket to the address. */
  if (bind (*listener, ai->ai_addr, ai->ai_addrlen) != 0)
    FATAL ("Unable to set bind: %s.", strerror (errno));
  freeaddrinfo (ai);

  /* Tell the socket to accept connections. */
  if (listen (*listener, SOMAXCONN) == -1)
    FATAL ("Unable to listen: %s.", strerror (errno));
}

/* Handle incoming messages through a pipe (let gwsocket be the
 * reader) and outgoing messages through the pipe (writer). */
static void
ws_fifos (WSServer * server, WSPipeIn * pi, WSPipeOut * po) {
  /* handle data via fifo */
  if (pi->fd != -1 && FD_ISSET (pi->fd, &fdstate.rfds))
    handle_fifo (server);
  /* handle data via fifo */
  if (po->fd != -1 && FD_ISSET (po->fd, &fdstate.wfds))
    ws_write_fifo (po, NULL, 0);
}

/* Check each client to determine if:
 * 1. We want to see if it has data for reading
 * 2. We want to write data to it.
 * If so, set the client's socket descriptor in the descriptor set. */
static void
set_rfds_wfds (int listener, WSServer * server, WSPipeIn * pi, WSPipeOut * po) {
  WSClient *client = NULL;
  int conn;

  /* pipe out */
  if (po->fd != -1) {
    if (po->status & WS_SENDING)
      FD_SET (po->fd, &fdstate.wfds);
  }
  /* pipe in */
  if (pi->fd != -1)
    FD_SET (pi->fd, &fdstate.rfds);

  /* self-pipe trick to stop the event loop */
  FD_SET (server->self_pipe[0], &fdstate.rfds);
  /* server socket, ready for accept() */
  FD_SET (listener, &fdstate.rfds);

  for (conn = 0; conn < FD_SETSIZE; ++conn) {
    if (conn == pi->fd || conn == po->fd)
      continue;
    if (!(client = ws_get_client_from_list (conn, &server->colist)))
      continue;

    /* As long as we are not closing a connection, we assume we always
     * check a client for reading */
    if (!server->closing) {
      FD_SET (conn, &fdstate.rfds);
      if (conn > max_file_fd)
        max_file_fd = conn;
    }
    /* Only if we have data to send the client */
    if (client->status & WS_SENDING) {
      FD_SET (conn, &fdstate.wfds);
      if (conn > max_file_fd)
        max_file_fd = conn;
    }
  }
}

/* Start the websocket server and start to monitor multiple file
 * descriptors until we have something to read or write. */
void
ws_start (WSServer * server) {
  WSPipeIn *pipein = server->pipein;
  WSPipeOut *pipeout = server->pipeout;
  int listener = 0, conn = 0;

#ifdef HAVE_LIBSSL
  if (wsconfig.sslcert && wsconfig.sslkey) {
    LOG (("==Using TLS/SSL==\n"));
    wsconfig.use_ssl = 1;
    if (initialize_ssl_ctx (server)) {
      LOG (("Unable to initialize_ssl_ctx\n"));
      return;
    }
  }
#endif

  memset (&fdstate, 0, sizeof fdstate);
  ws_socket (&listener);

  while (1) {
    /* If the pipeout file descriptor was opened after the server socket
     * was opened, then it's possible the max file descriptor would be the
     * pipeout fd, in any case we check this here */
    max_file_fd = MAX (listener, pipeout->fd);
    /* Clear out the fd sets for this iteration. */
    FD_ZERO (&fdstate.rfds);
    FD_ZERO (&fdstate.wfds);

    set_rfds_wfds (listener, server, pipein, pipeout);
    max_file_fd += 1;

    /* yep, wait patiently */
    /* should it be using epoll/kqueue? will see... */
    if (select (max_file_fd, &fdstate.rfds, &fdstate.wfds, NULL, NULL) == -1) {
      switch (errno) {
      case EINTR:
        LOG (("A signal was caught on select(2)\n"));
        break;
      default:
        FATAL ("Unable to select: %s.", strerror (errno));
      }
    }
    /* handle self-pipe trick */
    if (FD_ISSET (server->self_pipe[0], &fdstate.rfds)) {
      LOG (("Handled self-pipe to close event loop.\n"));
      break;
    }

    /* iterate over existing connections */
    for (conn = 0; conn < max_file_fd; ++conn) {
      if (conn != pipein->fd && conn != pipeout->fd) {
        ws_listen (listener, conn, server);
      }
    }
    /* handle FIFOs */
    ws_fifos (server, pipein, pipeout);
  }
}

/* Set the origin so the server can force connections to have the
 * given HTTP origin. */
void
ws_set_config_origin (const char *origin) {
  wsconfig.origin = origin;
}

/* Set the the maximum websocket frame size. */
void
ws_set_config_frame_size (int max_frm_size) {
  wsconfig.max_frm_size = max_frm_size;
}

/* Set specific name for the reader named pipe. */
void
ws_set_config_pipein (const char *pipein) {
  wsconfig.pipein = pipein;
}

/* Set specific name for the writer named pipe. */
void
ws_set_config_pipeout (const char *pipeout) {
  wsconfig.pipeout = pipeout;
}

/* Set a path and a file for the access log. */
void
ws_set_config_accesslog (const char *accesslog) {
  wsconfig.accesslog = accesslog;

  if (access_log_open (wsconfig.accesslog) == 1)
    FATAL ("Unable to open access log: %s.", strerror (errno));
}

/* Set if the server should handle strict named pipe handling. */
void
ws_set_config_strict (int strict) {
  wsconfig.strict = strict;
}

/* Set the server into echo mode. */
void
ws_set_config_echomode (int echomode) {
  wsconfig.echomode = echomode;
}

/* Set the server host bind address. */
void
ws_set_config_host (const char *host) {
  wsconfig.host = host;
}

/* Set the server port bind address. */
void
ws_set_config_port (const char *port) {
  wsconfig.port = port;
}

/* Set specific name for the SSL certificate. */
void
ws_set_config_sslcert (const char *sslcert) {
  wsconfig.sslcert = sslcert;
}

/* Set specific name for the SSL key. */
void
ws_set_config_sslkey (const char *sslkey) {
  wsconfig.sslkey = sslkey;
}

/* Create a new websocket server context. */
WSServer *
ws_init (const char *host, const char *port, void (*initopts) (void)) {
  WSServer *server = new_wsserver ();
  server->pipein = new_wspipein ();
  server->pipeout = new_wspipeout ();
  memset (server->self_pipe, 0, sizeof (server->self_pipe));

  wsconfig.accesslog = NULL;
  wsconfig.host = host;
  wsconfig.max_frm_size = WS_MAX_FRM_SZ;
  wsconfig.origin = NULL;
  wsconfig.pipein = NULL;
  wsconfig.pipeout = NULL;
  wsconfig.sslcert = NULL;
  wsconfig.sslkey = NULL;
  wsconfig.port = port;
  wsconfig.strict = 0;
  wsconfig.use_ssl = 0;

  initopts ();
  ws_fifo (server);

  return server;
}
