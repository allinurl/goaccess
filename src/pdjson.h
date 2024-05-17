#ifndef PDJSON_H
#define PDJSON_H

#ifndef PDJSON_SYMEXPORT
#   define PDJSON_SYMEXPORT
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#include <stdbool.h>
#else
#ifndef bool
#define bool int
#define true 1
#define false 0
#endif /* bool */
#endif /* __STDC_VERSION__ */

#include <stdio.h>

enum json_type {
  JSON_ERROR = 1, JSON_DONE,
  JSON_OBJECT, JSON_OBJECT_END, JSON_ARRAY, JSON_ARRAY_END,
  JSON_STRING, JSON_NUMBER, JSON_TRUE, JSON_FALSE, JSON_NULL
};

struct json_allocator {
  void *(*malloc) (size_t);
  void *(*realloc) (void *, size_t);
  void (*free) (void *);
};

typedef int (*json_user_io) (void *user);

typedef struct json_stream json_stream;
typedef struct json_allocator json_allocator;

PDJSON_SYMEXPORT void json_open_buffer (json_stream * json, const void *buffer, size_t size);
PDJSON_SYMEXPORT void json_open_string (json_stream * json, const char *string);
PDJSON_SYMEXPORT void json_open_stream (json_stream * json, FILE * stream);
PDJSON_SYMEXPORT void json_open_user (json_stream * json, json_user_io get, json_user_io peek,
                                      void *user);
PDJSON_SYMEXPORT void json_close (json_stream * json);

PDJSON_SYMEXPORT void json_set_allocator (json_stream * json, json_allocator * a);
PDJSON_SYMEXPORT void json_set_streaming (json_stream * json, bool mode);

PDJSON_SYMEXPORT enum json_type json_next (json_stream * json);
PDJSON_SYMEXPORT enum json_type json_peek (json_stream * json);
PDJSON_SYMEXPORT void json_reset (json_stream * json);
PDJSON_SYMEXPORT const char *json_get_string (json_stream * json, size_t *length);
PDJSON_SYMEXPORT double json_get_number (json_stream * json);

PDJSON_SYMEXPORT enum json_type json_skip (json_stream * json);
PDJSON_SYMEXPORT enum json_type json_skip_until (json_stream * json, enum json_type type);

PDJSON_SYMEXPORT size_t json_get_lineno (json_stream * json);
PDJSON_SYMEXPORT size_t json_get_position (json_stream * json);
PDJSON_SYMEXPORT size_t json_get_depth (json_stream * json);
PDJSON_SYMEXPORT enum json_type json_get_context (json_stream * json, size_t *count);
PDJSON_SYMEXPORT const char *json_get_error (json_stream * json);

PDJSON_SYMEXPORT int json_source_get (json_stream * json);
PDJSON_SYMEXPORT int json_source_peek (json_stream * json);
PDJSON_SYMEXPORT bool json_isspace (int c);

/* internal */

struct json_source {
  int (*get) (struct json_source *);
  int (*peek) (struct json_source *);
  size_t position;
  union {
    struct {
      FILE *stream;
    } stream;
    struct {
      const char *buffer;
      size_t length;
    } buffer;
    struct {
      void *ptr;
      json_user_io get;
      json_user_io peek;
    } user;
  } source;
};

struct json_stream {
  size_t lineno;

  struct json_stack *stack;
  size_t stack_top;
  size_t stack_size;
  enum json_type next;
  unsigned flags;

  struct {
    char *string;
    size_t string_fill;
    size_t string_size;
  } data;

  size_t ntokens;

  struct json_source source;
  struct json_allocator alloc;
  char errmsg[128];
};

#endif
