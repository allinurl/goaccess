/**
 * wsmessage.c -- WebSocket control message parsing
 *    _______       _______            __        __
 *   / ____/ |     / / ___/____  _____/ /_____  / /_
 *  / / __ | | /| / /\__ \/ __ \/ ___/ //_/ _ \/ __/
 * / /_/ / | |/ |/ /___/ / /_/ / /__/ ,< /  __/ /_
 * \____/  |__/|__//____/\____/\___/_/|_|\___/\__/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2026 Gerardo Orellana <hello @ goaccess.io>
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

#include <stdlib.h>
#include <string.h>

#include "wsmessage.h"

#include "pdjson.h"
#include "xmalloc.h"

/* Token-refresh control message schema. */
#define WS_AUTH_ACTION_KEY "action"
#define WS_AUTH_TOKEN_KEY "token"
#define WS_AUTH_VALIDATE_ACTION "validate_token"

typedef struct WSTokenParseState_ {
  WSTokenMessage *message;
  int action_members;
  int token_members;
  int invalid_member;
} WSTokenParseState;

/* Check whether the current JSON string exactly matches an expected value.
 *
 * On success, non-zero is returned.
 * On failure, including strings with embedded NUL bytes, 0 is returned. */
static int
json_string_equals (json_stream *json, const char *expected) {
  const char *value = NULL;
  size_t expected_len = 0, value_len = 0;

  value = json_get_string (json, &value_len);
  expected_len = strlen (expected);

  if (value_len != expected_len + 1)
    return 0;

  return memcmp (value, expected, expected_len + 1) == 0;
}

/* Copy the current JSON string when it contains no embedded NUL bytes.
 *
 * On success, a newly allocated string is returned.
 * On failure, NULL is returned. */
static char *
copy_json_string (json_stream *json) {
  const char *value = NULL;
  char *copy = NULL;
  size_t value_len = 0;

  value = json_get_string (json, &value_len);
  if (value_len == 0 || value[value_len - 1] != '\0')
    return NULL;

  if (memchr (value, '\0', value_len - 1) != NULL)
    return NULL;

  copy = xmalloc (value_len);
  memcpy (copy, value, value_len);

  return copy;
}

/* Consume a JSON container whose opening token was already read.
 *
 * On success, 0 is returned.
 * On malformed or incomplete input, -1 is returned. */
static int
skip_open_json_container (json_stream *json, enum json_type opening) {
  enum json_type type = JSON_ERROR;
  size_t depth = 1;

  if (opening != JSON_ARRAY && opening != JSON_OBJECT)
    return 0;

  while (depth > 0) {
    type = json_next (json);
    if (type == JSON_ERROR || type == JSON_DONE)
      return -1;

    if (type == JSON_ARRAY || type == JSON_OBJECT)
      depth++;
    else if (type == JSON_ARRAY_END || type == JSON_OBJECT_END)
      depth--;
  }

  return 0;
}

/* Read a known token-message member and record its schema state.
 *
 * On success, 0 is returned.
 * On malformed or incomplete input, -1 is returned. */
static int
parse_known_token_member (json_stream *json, int is_action, WSTokenParseState *state) {
  enum json_type value_type = JSON_ERROR;
  char *token = NULL;

  value_type = json_next (json);
  if (value_type == JSON_ERROR || value_type == JSON_DONE)
    return -1;

  if (is_action) {
    state->action_members++;
    if (state->action_members > 1 || value_type != JSON_STRING)
      state->invalid_member = 1;
    if (value_type == JSON_STRING && json_string_equals (json, WS_AUTH_VALIDATE_ACTION))
      state->message->is_validation = 1;
  } else {
    state->token_members++;
    if (state->token_members > 1 || value_type != JSON_STRING) {
      state->invalid_member = 1;
    } else if ((token = copy_json_string (json)) == NULL) {
      state->invalid_member = 1;
    } else {
      state->message->token = token;
    }
  }

  return skip_open_json_container (json, value_type);
}

/* Extract authentication members from a top-level JSON object.
 *
 * On success, WS_TOKEN_PARSE_OBJECT is returned.
 * For a non-object message, WS_TOKEN_PARSE_OTHER is returned.
 * On malformed or incomplete object input, WS_TOKEN_PARSE_ERROR is returned. */
static WSTokenParseResult
parse_json_token_message (json_stream *json, WSTokenParseState *state) {
  enum json_type type = JSON_ERROR;
  int is_action = 0, is_token = 0;

  if (json_next (json) != JSON_OBJECT)
    return WS_TOKEN_PARSE_OTHER;

  while ((type = json_next (json)) == JSON_STRING) {
    is_action = json_string_equals (json, WS_AUTH_ACTION_KEY);
    is_token = json_string_equals (json, WS_AUTH_TOKEN_KEY);

    if (is_action || is_token) {
      if (parse_known_token_member (json, is_action, state) != 0)
        return WS_TOKEN_PARSE_ERROR;
      continue;
    }

    if (json_skip (json) == JSON_ERROR)
      return WS_TOKEN_PARSE_ERROR;
  }

  if (type != JSON_OBJECT_END)
    return WS_TOKEN_PARSE_ERROR;

  if (json_next (json) != JSON_DONE)
    return WS_TOKEN_PARSE_ERROR;

  return WS_TOKEN_PARSE_OBJECT;
}

/* Release data owned by a parsed token message. */
void
ws_free_token_message (WSTokenMessage *message) {
  if (message == NULL)
    return;

  free (message->token);
  memset (message, 0, sizeof (*message));
}

/* Parse a bounded WebSocket token-control payload.
 *
 * On success, WS_TOKEN_PARSE_OBJECT is returned and message describes the object.
 * For a non-object message, WS_TOKEN_PARSE_OTHER is returned.
 * On malformed or incomplete object input, WS_TOKEN_PARSE_ERROR is returned. */
WSTokenParseResult
ws_parse_token_message (const char *payload, size_t payloadsz, WSTokenMessage *message) {
  json_stream json;
  WSTokenParseState state = { 0 };
  WSTokenParseResult result = WS_TOKEN_PARSE_ERROR;

  if (payload == NULL || message == NULL)
    return WS_TOKEN_PARSE_ERROR;

  memset (message, 0, sizeof (*message));
  state.message = message;

  json_open_buffer (&json, payload, payloadsz);
  json_set_streaming (&json, false);
  result = parse_json_token_message (&json, &state);
  json_close (&json);

  if (result == WS_TOKEN_PARSE_OBJECT && message->is_validation && !state.invalid_member &&
      state.action_members == 1 && state.token_members == 1 && message->token != NULL)
    message->is_valid = 1;

  return result;
}
