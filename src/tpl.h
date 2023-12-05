/*
Copyright (c) 2005-2013, Troy D. Hanson     http://troydhanson.github.com/tpl/
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef TPL_H
#define TPL_H

#include <stddef.h>     /* size_t */

#include <stdarg.h>     /* va_list */

#ifdef __INTEL_COMPILER
#include <tbb/tbbmalloc_proxy.h>
#endif /* Intel Compiler efficient memcpy etc */

#ifdef _MSC_VER
typedef unsigned int uint32_t;
#else
#include <inttypes.h>   /* uint32_t */
#endif

#if defined __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef TPL_EXPORTS
#define TPL_API __declspec(dllexport)
#else                           /*  */
#ifdef TPL_NOLIB
#define TPL_API
#else
#define TPL_API __declspec(dllimport)
#endif                          /* TPL_NOLIB */
#endif                          /* TPL_EXPORTS */
#else
#define TPL_API
#endif

/* bit flags (external) */
#define TPL_FILE      (1 << 0)
#define TPL_MEM       (1 << 1)
#define TPL_PREALLOCD (1 << 2)
#define TPL_EXCESS_OK (1 << 3)
#define TPL_FD        (1 << 4)
#define TPL_UFREE     (1 << 5)
#define TPL_DATAPEEK  (1 << 6)
#define TPL_FXLENS    (1 << 7)
#define TPL_GETSIZE   (1 << 8)
/* do not add flags here without renumbering the internal flags! */

/* flags for tpl_gather mode */
#define TPL_GATHER_BLOCKING    1
#define TPL_GATHER_NONBLOCKING 2
#define TPL_GATHER_MEM         3

/* Hooks for error logging, memory allocation functions and fatal */
  typedef int (tpl_print_fcn) (const char *fmt, ...);
  typedef void *(tpl_malloc_fcn) (size_t sz);
  typedef void *(tpl_realloc_fcn) (void *ptr, size_t sz);
  typedef void (tpl_free_fcn) (void *ptr);
  typedef void (tpl_fatal_fcn) (const char *fmt, ...);

  typedef struct tpl_hook_t {
    tpl_print_fcn *oops __attribute__((__format__ (printf, 1, 2)));
    tpl_malloc_fcn *malloc;
    tpl_realloc_fcn *realloc;
    tpl_free_fcn *free;
    tpl_fatal_fcn *fatal __attribute__((__format__ (printf, 1, 2)))
      __attribute__((__noreturn__));
    size_t gather_max;
  } tpl_hook_t;

  typedef struct tpl_node {
    int type;
    void *addr;
    void *data;                 /* r:tpl_root_data*. A:tpl_atyp*. ow:szof type */
    int num;                    /* length of type if it's a C array */
    size_t ser_osz;             /* serialization output size for subtree */
    struct tpl_node *children;  /* my children; linked-list */
    struct tpl_node *next, *prev;       /* my siblings (next child of my parent) */
    struct tpl_node *parent;    /* my parent */
  } tpl_node;

/* used when un/packing 'B' type (binary buffers) */
  typedef struct tpl_bin {
    void *addr;
    uint32_t sz;
  } tpl_bin;

/* for async/piecemeal reading of tpl images */
  typedef struct tpl_gather_t {
    char *img;
    int len;
  } tpl_gather_t;

/* Callback used when tpl_gather has read a full tpl image */
  typedef int (tpl_gather_cb) (void *img, size_t sz, void *data);

/* Prototypes */
  TPL_API tpl_node *tpl_map (char *fmt, ...);   /* define tpl using format */
  TPL_API void tpl_free (tpl_node * r); /* free a tpl map */
  TPL_API int tpl_pack (tpl_node * r, int i);   /* pack the n'th packable */
  TPL_API int tpl_unpack (tpl_node * r, int i); /* unpack the n'th packable */
  TPL_API int tpl_dump (tpl_node * r, int mode, ...);   /* serialize to mem/file */
  TPL_API int tpl_load (tpl_node * r, int mode, ...);   /* set mem/file to unpack */
  TPL_API int tpl_Alen (tpl_node * r, int i);   /* array len of packable i */
  TPL_API char *tpl_peek (int mode, ...);       /* sneak peek at format string */
  TPL_API int tpl_gather (int mode, ...);       /* non-blocking image gather */
  TPL_API int tpl_jot (int mode, ...);  /* quick write a simple tpl */

  TPL_API tpl_node *tpl_map_va (char *fmt, va_list ap);

#if defined __cplusplus
}
#endif
#endif                          /* TPL_H */
