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

#define TPL_VERSION 1.6

/*static const char id[]="$Id: tpl.c 192 2009-04-24 10:35:30Z thanson $";*/


#include <stdlib.h>     /* malloc */
#include <stdarg.h>     /* va_list */
#include <string.h>     /* memcpy, memset, strchr */
#include <stdio.h>      /* printf (tpl_hook.oops default function) */

#ifndef _WIN32
#include <unistd.h>     /* for ftruncate */
#else
#include <io.h>
#define ftruncate(x,y) _chsize(x,y)
#endif
#include <sys/types.h>  /* for 'open' */
#include <sys/stat.h>   /* for 'open' */
#include <fcntl.h>      /* for 'open' */
#include <errno.h>
#ifndef _WIN32
#include <inttypes.h>   /* uint32_t, uint64_t, etc */
#else
typedef unsigned short ushort;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#endif

#ifndef S_ISREG
#define S_ISREG(mode)  (((mode) & S_IFMT) == S_IFREG)
#endif

#if ( defined __CYGWIN__ || defined __MINGW32__ || defined _WIN32 )
#include "win/mman.h"   /* mmap */
#else
#include <sys/mman.h>   /* mmap */
#endif

#include "tpl.h"

#define TPL_GATHER_BUFLEN 8192
#define TPL_MAGIC "tpl"

/* macro to add a structure to a doubly-linked list */
#define DL_ADD(head,add)                                        \
    do {                                                        \
        if (head) {                                             \
            (add)->prev = (head)->prev;                         \
            (head)->prev->next = (add);                         \
            (head)->prev = (add);                               \
            (add)->next = NULL;                                 \
        } else {                                                \
            (head)=(add);                                       \
            (head)->prev = (head);                              \
            (head)->next = NULL;                                \
        }                                                       \
    } while (0);

#define fatal_oom() tpl_hook.fatal("out of memory\n")

/* bit flags (internal). preceded by the external flags in tpl.h */
#define TPL_WRONLY         (1 << 9)     /* app has initiated tpl packing  */
#define TPL_RDONLY         (1 << 10)    /* tpl was loaded (for unpacking) */
#define TPL_XENDIAN        (1 << 11)    /* swap endianness when unpacking */
#define TPL_OLD_STRING_FMT (1 << 12)    /* tpl has strings in 1.2 format */

/* values for the flags byte that appears after the magic prefix */
#define TPL_SUPPORTED_BITFLAGS 3
#define TPL_FL_BIGENDIAN   (1 << 0)
#define TPL_FL_NULLSTRINGS (1 << 1)

/* char values for node type */
#define TPL_TYPE_ROOT   0
#define TPL_TYPE_INT32  1
#define TPL_TYPE_UINT32 2
#define TPL_TYPE_BYTE   3
#define TPL_TYPE_STR    4
#define TPL_TYPE_ARY    5
#define TPL_TYPE_BIN    6
#define TPL_TYPE_DOUBLE 7
#define TPL_TYPE_INT64  8
#define TPL_TYPE_UINT64 9
#define TPL_TYPE_INT16  10
#define TPL_TYPE_UINT16 11
#define TPL_TYPE_POUND  12

/* error codes */
#define ERR_NOT_MINSIZE        (-1)
#define ERR_MAGIC_MISMATCH     (-2)
#define ERR_INCONSISTENT_SZ    (-3)
#define ERR_FMT_INVALID        (-4)
#define ERR_FMT_MISSING_NUL    (-5)
#define ERR_FMT_MISMATCH       (-6)
#define ERR_FLEN_MISMATCH      (-7)
#define ERR_INCONSISTENT_SZ2   (-8)
#define ERR_INCONSISTENT_SZ3   (-9)
#define ERR_INCONSISTENT_SZ4   (-10)
#define ERR_UNSUPPORTED_FLAGS  (-11)

/* access to A(...) nodes by index */
typedef struct tpl_pidx {
  struct tpl_node *node;
  struct tpl_pidx *next, *prev;
} tpl_pidx;

/* A(...) node datum */
typedef struct tpl_atyp {
  uint32_t num;                 /* num elements */
  size_t sz;                    /* size of each backbone's datum */
  struct tpl_backbone *bb, *bbtail;
  void *cur;
} tpl_atyp;

/* backbone to extend A(...) lists dynamically */
typedef struct tpl_backbone {
  struct tpl_backbone *next;
  /* when this structure is malloc'd, extra space is alloc'd at the
   * end to store the backbone "datum", and data points to it. */
#if __STDC_VERSION__ < 199901
  char *data;
#else
  char data[];
#endif
} tpl_backbone;

/* mmap record */
typedef struct tpl_mmap_rec {
  int fd;
  void *text;
  size_t text_sz;
} tpl_mmap_rec;

/* root node datum */
typedef struct tpl_root_data {
  int flags;
  tpl_pidx *pidx;
  tpl_mmap_rec mmap;
  char *fmt;
  int *fxlens, num_fxlens;
} tpl_root_data;

/* node type to size mapping */
struct tpl_type_t {
  char c;
  int sz;
};


/* Internal prototypes */
static tpl_node *tpl_node_new (tpl_node * parent);
static tpl_node *tpl_find_i (tpl_node * n, int i);
static void *tpl_cpv (void *datav, const void *data, size_t sz);
static void *tpl_extend_backbone (tpl_node * n);
static char *tpl_fmt (tpl_node * r);
static void *tpl_dump_atyp (tpl_node * n, tpl_atyp * at, void *dv);
static size_t tpl_ser_osz (tpl_node * n);
static void tpl_free_atyp (tpl_node * n, tpl_atyp * atyp);
static int tpl_dump_to_mem (tpl_node * r, void *addr, size_t sz);
static int tpl_mmap_file (char *filename, tpl_mmap_rec * map_rec);
static int tpl_mmap_output_file (char *filename, size_t sz, void **text_out);
static int tpl_cpu_bigendian (void);
static int tpl_needs_endian_swap (void *);
static void tpl_byteswap (void *word, int len);
static void tpl_fatal (const char *fmt, ...)
  __attribute__((__format__ (printf, 1, 2))) __attribute__((__noreturn__));
static int tpl_serlen (tpl_node * r, tpl_node * n, void *dv, size_t *serlen);
static int tpl_unpackA0 (tpl_node * r);
static int tpl_oops (const char *fmt, ...)
  __attribute__((__format__ (printf, 1, 2)));
static int tpl_gather_mem (char *buf, size_t len, tpl_gather_t ** gs,
                           tpl_gather_cb * cb, void *data);
static int tpl_gather_nonblocking (int fd, tpl_gather_t ** gs, tpl_gather_cb * cb, void *data);
static int tpl_gather_blocking (int fd, void **img, size_t *sz);

/* This is used internally to help calculate padding when a 'double'
 * follows a smaller datatype in a structure. Normally under gcc
 * on x86, d will be aligned at +4, however use of -malign-double
 * causes d to be aligned at +8 (this is actually faster on x86).
 * Also SPARC and x86_64 seem to align always on +8.
 */
struct tpl_double_alignment_detector {
  char a;
  double d;                     /* some platforms align this on +4, others on +8 */
};

/* this is another case where alignment varies. mac os x/gcc was observed
 * to align the int64_t at +4 under -m32 and at +8 under -m64 */
struct tpl_int64_alignment_detector {
  int i;
  int64_t j;                    /* some platforms align this on +4, others on +8 */
};

typedef struct {
  size_t inter_elt_len;         /* padded inter-element len; i.e. &a[1].field - &a[0].field */
  tpl_node *iter_start_node;    /* node to jump back to, as we start each new iteration */
  size_t iternum;               /* current iteration number (total req'd. iter's in n->num) */
} tpl_pound_data;

/* Hooks for customizing tpl mem alloc, error handling, etc. Set defaults. */
tpl_hook_t tpl_hook = {
  /* .oops =       */ tpl_oops,
  /* .malloc =     */ malloc,
  /* .realloc =    */ realloc,
  /* .free =       */ free,
  /* .fatal =      */ tpl_fatal,
  /* .gather_max = */ 0
    /* max tpl size (bytes) for tpl_gather */
};

static const char tpl_fmt_chars[] = "AS($)BiucsfIUjv#"; /* valid format chars */
/* valid within S(...) */
/* static const char tpl_S_fmt_chars[] = "iucsfIUjv#$()"; */
static const char tpl_datapeek_ok_chars[] = "iucsfIUjv";        /* valid in datapeek */
static const struct tpl_type_t tpl_types[] = {
  /* [TPL_TYPE_ROOT] =   */ {'r', 0},
  /* [TPL_TYPE_INT32] =  */ {'i', sizeof (int32_t)},
  /* [TPL_TYPE_UINT32] = */ {'u', sizeof (uint32_t)},
  /* [TPL_TYPE_BYTE] =   */ {'c', sizeof (char)},
  /* [TPL_TYPE_STR] =    */ {'s', sizeof (char *)},
  /* [TPL_TYPE_ARY] =    */ {'A', 0},
  /* [TPL_TYPE_BIN] =    */ {'B', 0},
  /* [TPL_TYPE_DOUBLE] = */ {'f', 8},
  /* not sizeof(double) as that varies */
  /* [TPL_TYPE_INT64] =  */ {'I', sizeof (int64_t)},
  /* [TPL_TYPE_UINT64] = */ {'U', sizeof (uint64_t)},
  /* [TPL_TYPE_INT16] =  */ {'j', sizeof (int16_t)},
  /* [TPL_TYPE_UINT16] = */ {'v', sizeof (uint16_t)},
  /* [TPL_TYPE_POUND] =  */ {'#', 0},
};

/* default error-reporting function. Just writes to stderr. */
static int
tpl_oops (const char *fmt, ...) {
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  return 0;
}


static tpl_node *
tpl_node_new (tpl_node * parent) {
  tpl_node *n;
  if ((n = tpl_hook.malloc (sizeof (tpl_node))) == NULL) {
    fatal_oom ();
  }
  n->addr = NULL;
  n->data = NULL;
  n->num = 1;
  n->ser_osz = 0;
  n->children = NULL;
  n->next = NULL;
  n->parent = parent;
  return n;
}

/* Used in S(..) formats to pack several fields from a structure based on
 * only the structure address. We need to calculate field addresses
 * manually taking into account the size of the fields and intervening padding.
 * The wrinkle is that double is not normally aligned on x86-32 but the
 * -malign-double compiler option causes it to be. Double are aligned
 * on Sparc, and apparently on 64 bit x86. We use a helper structure
 * to detect whether double is aligned in this compilation environment.
 */
static char *
calc_field_addr (tpl_node * parent, int type, char *struct_addr, int ordinal) {
  tpl_node *prev;
  int offset;
  int align_sz;

  if (ordinal == 1)
    return struct_addr; /* first field starts on structure address */

  /* generate enough padding so field addr is divisible by it's align_sz. 4, 8, etc */
  prev = parent->children->prev;
  switch (type) {
  case TPL_TYPE_DOUBLE:
    align_sz = sizeof (struct tpl_double_alignment_detector) > 12 ? 8 : 4;
    break;
  case TPL_TYPE_INT64:
  case TPL_TYPE_UINT64:
    align_sz = sizeof (struct tpl_int64_alignment_detector) > 12 ? 8 : 4;
    break;
  default:
    align_sz = tpl_types[type].sz;
    break;
  }
  offset = ((uintptr_t) prev->addr - (uintptr_t) struct_addr)
    + (tpl_types[prev->type].sz * prev->num);
  offset = (offset + align_sz - 1) / align_sz * align_sz;
  return struct_addr + offset;
}

TPL_API tpl_node *
tpl_map (char *fmt, ...) {
  va_list ap;
  tpl_node *tn;

  va_start (ap, fmt);
  tn = tpl_map_va (fmt, ap);
  va_end (ap);
  return tn;
}

TPL_API tpl_node *
tpl_map_va (char *fmt, va_list ap) {
  int lparen_level = 0, expect_lparen = 0, t = 0, in_structure = 0, ordinal = 0;
  int in_nested_structure = 0;
  char *c, *peek, *struct_addr = NULL, *struct_next;
  tpl_node *root, *parent, *n = NULL, *preceding, *iter_start_node = NULL,
    *struct_widest_node = NULL, *np;
  tpl_pidx *pidx;
  tpl_pound_data *pd;
  int *fxlens, num_fxlens, pound_num, pound_prod, applies_to_struct;
  int contig_fxlens[10];        /* temp space for contiguous fxlens */
  uint32_t num_contig_fxlens;
  uint32_t i;
  int j;
  ptrdiff_t inter_elt_len = 0;  /* padded element length of contiguous structs in array */


  root = tpl_node_new (NULL);
  root->type = TPL_TYPE_ROOT;
  root->data = (tpl_root_data *) tpl_hook.malloc (sizeof (tpl_root_data));
  if (!root->data)
    fatal_oom ();
  memset ((tpl_root_data *) root->data, 0, sizeof (tpl_root_data));

  /* set up root nodes special ser_osz to reflect overhead of preamble */
  root->ser_osz = sizeof (uint32_t);    /* tpl leading length */
  root->ser_osz += strlen (fmt) + 1;    /* fmt + NUL-terminator */
  root->ser_osz += 4;   /* 'tpl' magic prefix + flags byte */

  parent = root;

  c = fmt;
  while (*c != '\0') {
    switch (*c) {
    case 'c':
    case 'i':
    case 'u':
    case 'j':
    case 'v':
    case 'I':
    case 'U':
    case 'f':
      if (*c == 'c')
        t = TPL_TYPE_BYTE;
      else if (*c == 'i')
        t = TPL_TYPE_INT32;
      else if (*c == 'u')
        t = TPL_TYPE_UINT32;
      else if (*c == 'j')
        t = TPL_TYPE_INT16;
      else if (*c == 'v')
        t = TPL_TYPE_UINT16;
      else if (*c == 'I')
        t = TPL_TYPE_INT64;
      else if (*c == 'U')
        t = TPL_TYPE_UINT64;
      else if (*c == 'f')
        t = TPL_TYPE_DOUBLE;

      if (expect_lparen)
        goto fail;
      n = tpl_node_new (parent);
      n->type = t;
      if (in_structure) {
        if (ordinal == 1) {
          /* for S(...)# iteration. Apply any changes to case 's' too!!! */
          iter_start_node = n;
          struct_widest_node = n;
        }
        if (tpl_types[n->type].sz > tpl_types[struct_widest_node->type].sz) {
          struct_widest_node = n;
        }
        n->addr = calc_field_addr (parent, n->type, struct_addr, ordinal++);
      } else
        n->addr = (void *) va_arg (ap, void *);
      n->data = tpl_hook.malloc (tpl_types[t].sz);
      if (!n->data)
        fatal_oom ();
      if (n->parent->type == TPL_TYPE_ARY)
        ((tpl_atyp *) (n->parent->data))->sz += tpl_types[t].sz;
      DL_ADD (parent->children, n);
      break;
    case 's':
      if (expect_lparen)
        goto fail;
      n = tpl_node_new (parent);
      n->type = TPL_TYPE_STR;
      if (in_structure) {
        if (ordinal == 1) {
          iter_start_node = n;  /* for S(...)# iteration */
          struct_widest_node = n;
        }
        if (tpl_types[n->type].sz > tpl_types[struct_widest_node->type].sz) {
          struct_widest_node = n;
        }
        n->addr = calc_field_addr (parent, n->type, struct_addr, ordinal++);
      } else
        n->addr = (void *) va_arg (ap, void *);
      n->data = tpl_hook.malloc (sizeof (char *));
      if (!n->data)
        fatal_oom ();
      *(char **) (n->data) = NULL;
      if (n->parent->type == TPL_TYPE_ARY)
        ((tpl_atyp *) (n->parent->data))->sz += sizeof (void *);
      DL_ADD (parent->children, n);
      break;
    case '#':
      /* apply a 'num' to preceding atom */
      if (!parent->children)
        goto fail;
      preceding = parent->children->prev;       /* first child's prev is 'last child' */
      t = preceding->type;
      applies_to_struct = (*(c - 1) == ')') ? 1 : 0;
      if (!applies_to_struct) {
        if (!(t == TPL_TYPE_BYTE || t == TPL_TYPE_INT32 ||
              t == TPL_TYPE_UINT32 || t == TPL_TYPE_DOUBLE ||
              t == TPL_TYPE_UINT64 || t == TPL_TYPE_INT64 ||
              t == TPL_TYPE_UINT16 || t == TPL_TYPE_INT16 || t == TPL_TYPE_STR))
          goto fail;
      }
      /* count up how many contiguous # and form their product */
      pound_prod = 1;
      num_contig_fxlens = 0;
      for (peek = c; *peek == '#'; peek++) {
        pound_num = va_arg (ap, int);
        if (pound_num < 1) {
          tpl_hook.fatal ("non-positive iteration count %d\n", pound_num);
        }
        if (num_contig_fxlens >= (sizeof (contig_fxlens) / sizeof (contig_fxlens[0]))) {
          tpl_hook.fatal ("contiguous # exceeds hardcoded limit\n");
        }
        contig_fxlens[num_contig_fxlens++] = pound_num;
        pound_prod *= pound_num;
      }
      /* increment c to skip contiguous # so its points to last one */
      c = peek - 1;
      /* differentiate atom-# from struct-# by noting preceding rparen */
      if (applies_to_struct) {  /* insert # node to induce looping */
        n = tpl_node_new (parent);
        n->type = TPL_TYPE_POUND;
        n->num = pound_prod;
        n->data = tpl_hook.malloc (sizeof (tpl_pound_data));
        if (!n->data)
          fatal_oom ();
        pd = (tpl_pound_data *) n->data;
        pd->inter_elt_len = inter_elt_len;
        pd->iter_start_node = iter_start_node;
        pd->iternum = 0;
        DL_ADD (parent->children, n);
        /* multiply the 'num' and data space on each atom in the structure */
        for (np = iter_start_node; np != n; np = np->next) {
          if (n->parent->type == TPL_TYPE_ARY) {
            ((tpl_atyp *) (n->parent->data))->sz +=
              tpl_types[np->type].sz * (np->num * (n->num - 1));
          }
          np->data = tpl_hook.realloc (np->data, tpl_types[np->type].sz * np->num * n->num);
          if (!np->data)
            fatal_oom ();
          memset (np->data, 0, tpl_types[np->type].sz * np->num * n->num);
        }
      } else {  /* simple atom-# form does not require a loop */
        preceding->num = pound_prod;
        preceding->data = tpl_hook.realloc (preceding->data, tpl_types[t].sz * preceding->num);
        if (!preceding->data)
          fatal_oom ();
        memset (preceding->data, 0, tpl_types[t].sz * preceding->num);
        if (n->parent->type == TPL_TYPE_ARY) {
          ((tpl_atyp *) (n->parent->data))->sz += tpl_types[t].sz * (preceding->num - 1);
        }
      }
      root->ser_osz += (sizeof (uint32_t) * num_contig_fxlens);

      j = ((tpl_root_data *) root->data)->num_fxlens;   /* before incrementing */
      (((tpl_root_data *) root->data)->num_fxlens) += num_contig_fxlens;
      num_fxlens = ((tpl_root_data *) root->data)->num_fxlens;  /* new value */
      fxlens = ((tpl_root_data *) root->data)->fxlens;
      fxlens = tpl_hook.realloc (fxlens, sizeof (int) * num_fxlens);
      if (!fxlens)
        fatal_oom ();
      ((tpl_root_data *) root->data)->fxlens = fxlens;
      for (i = 0; i < num_contig_fxlens; i++)
        fxlens[j++] = contig_fxlens[i];

      break;
    case 'B':
      if (expect_lparen)
        goto fail;
      if (in_structure)
        goto fail;
      n = tpl_node_new (parent);
      n->type = TPL_TYPE_BIN;
      n->addr = (tpl_bin *) va_arg (ap, void *);
      n->data = tpl_hook.malloc (sizeof (tpl_bin *));
      if (!n->data)
        fatal_oom ();
      *((tpl_bin **) n->data) = NULL;
      if (n->parent->type == TPL_TYPE_ARY)
        ((tpl_atyp *) (n->parent->data))->sz += sizeof (tpl_bin);
      DL_ADD (parent->children, n);
      break;
    case 'A':
      if (in_structure)
        goto fail;
      n = tpl_node_new (parent);
      n->type = TPL_TYPE_ARY;
      DL_ADD (parent->children, n);
      parent = n;
      expect_lparen = 1;
      pidx = (tpl_pidx *) tpl_hook.malloc (sizeof (tpl_pidx));
      if (!pidx)
        fatal_oom ();
      pidx->node = n;
      pidx->next = NULL;
      DL_ADD (((tpl_root_data *) (root->data))->pidx, pidx);
      /* set up the A's tpl_atyp */
      n->data = (tpl_atyp *) tpl_hook.malloc (sizeof (tpl_atyp));
      if (!n->data)
        fatal_oom ();
      ((tpl_atyp *) (n->data))->num = 0;
      ((tpl_atyp *) (n->data))->sz = 0;
      ((tpl_atyp *) (n->data))->bb = NULL;
      ((tpl_atyp *) (n->data))->bbtail = NULL;
      ((tpl_atyp *) (n->data))->cur = NULL;
      if (n->parent->type == TPL_TYPE_ARY)
        ((tpl_atyp *) (n->parent->data))->sz += sizeof (void *);
      break;
    case 'S':
      if (in_structure)
        goto fail;
      expect_lparen = 1;
      ordinal = 1;      /* index upcoming atoms in S(..) */
      in_structure = 1 + lparen_level;  /* so we can tell where S fmt ends */
      struct_addr = (char *) va_arg (ap, void *);
      break;
    case '$':  /* nested structure */
      if (!in_structure)
        goto fail;
      expect_lparen = 1;
      in_nested_structure++;
      break;
    case ')':
      lparen_level--;
      if (lparen_level < 0)
        goto fail;
      if (*(c - 1) == '(')
        goto fail;
      if (in_nested_structure)
        in_nested_structure--;
      else if (in_structure && (in_structure - 1 == lparen_level)) {
        /* calculate delta between contiguous structures in array */
        struct_next = calc_field_addr (parent, struct_widest_node->type,
                                       struct_addr, ordinal++);
        inter_elt_len = struct_next - struct_addr;
        in_structure = 0;
      } else
        parent = parent->parent;        /* rparen ends A() type, not S() type */
      break;
    case '(':
      if (!expect_lparen)
        goto fail;
      expect_lparen = 0;
      lparen_level++;
      break;
    default:
      tpl_hook.oops ("unsupported option %c\n", *c);
      goto fail;
    }
    c++;
  }
  if (lparen_level != 0)
    goto fail;

  /* copy the format string, save for convenience */
  ((tpl_root_data *) (root->data))->fmt = tpl_hook.malloc (strlen (fmt) + 1);
  if (((tpl_root_data *) (root->data))->fmt == NULL)
    fatal_oom ();
  memcpy (((tpl_root_data *) (root->data))->fmt, fmt, strlen (fmt) + 1);

  return root;

fail:
  tpl_hook.oops ("failed to parse %s\n", fmt);
  tpl_free (root);
  return NULL;
}

static int
tpl_unmap_file (tpl_mmap_rec * mr) {

  if (munmap (mr->text, mr->text_sz) == -1) {
    tpl_hook.oops ("Failed to munmap: %s\n", strerror (errno));
  }
  close (mr->fd);
  mr->text = NULL;
  mr->text_sz = 0;
  return 0;
}

static void
tpl_free_keep_map (tpl_node * r) {
  int mmap_bits = (TPL_RDONLY | TPL_FILE);
  int ufree_bits = (TPL_MEM | TPL_UFREE);
  tpl_node *nxtc, *c;
  int find_next_node = 0, looking, i;
  size_t sz;

  /* For mmap'd files, or for 'ufree' memory images , do appropriate release */
  if ((((tpl_root_data *) (r->data))->flags & mmap_bits) == mmap_bits) {
    tpl_unmap_file (&((tpl_root_data *) (r->data))->mmap);
  } else if ((((tpl_root_data *) (r->data))->flags & ufree_bits) == ufree_bits) {
    tpl_hook.free (((tpl_root_data *) (r->data))->mmap.text);
  }

  c = r->children;
  if (c) {
    while (c->type != TPL_TYPE_ROOT) {  /* loop until we come back to root node */
      switch (c->type) {
      case TPL_TYPE_BIN:
        /* free any binary buffer hanging from tpl_bin */
        if (*((tpl_bin **) (c->data))) {
          if ((*((tpl_bin **) (c->data)))->addr) {
            tpl_hook.free ((*((tpl_bin **) (c->data)))->addr);
          }
          *((tpl_bin **) c->data) = NULL;       /* reset tpl_bin */
        }
        find_next_node = 1;
        break;
      case TPL_TYPE_STR:
        /* free any packed (copied) string */
        for (i = 0; i < c->num; i++) {
          char *str = ((char **) c->data)[i];
          if (str) {
            tpl_hook.free (str);
            ((char **) c->data)[i] = NULL;
          }
        }
        find_next_node = 1;
        break;
      case TPL_TYPE_INT32:
      case TPL_TYPE_UINT32:
      case TPL_TYPE_INT64:
      case TPL_TYPE_UINT64:
      case TPL_TYPE_BYTE:
      case TPL_TYPE_DOUBLE:
      case TPL_TYPE_INT16:
      case TPL_TYPE_UINT16:
      case TPL_TYPE_POUND:
        find_next_node = 1;
        break;
      case TPL_TYPE_ARY:
        c->ser_osz = 0; /* zero out the serialization output size */

        sz = ((tpl_atyp *) (c->data))->sz;      /* save sz to use below */
        tpl_free_atyp (c, c->data);

        /* make new atyp */
        c->data = (tpl_atyp *) tpl_hook.malloc (sizeof (tpl_atyp));
        if (!c->data)
          fatal_oom ();
        ((tpl_atyp *) (c->data))->num = 0;
        ((tpl_atyp *) (c->data))->sz = sz;      /* restore bb datum sz */
        ((tpl_atyp *) (c->data))->bb = NULL;
        ((tpl_atyp *) (c->data))->bbtail = NULL;
        ((tpl_atyp *) (c->data))->cur = NULL;

        c = c->children;
        break;
      default:
        tpl_hook.fatal ("unsupported format character\n");
        break;
      }

      if (find_next_node) {
        find_next_node = 0;
        looking = 1;
        while (looking) {
          if (c->next) {
            nxtc = c->next;
            c = nxtc;
            looking = 0;
          } else {
            if (c->type == TPL_TYPE_ROOT)
              break;    /* root node */
            else {
              nxtc = c->parent;
              c = nxtc;
            }
          }
        }
      }
    }
  }

  ((tpl_root_data *) (r->data))->flags = 0;     /* reset flags */
}

TPL_API void
tpl_free (tpl_node * r) {
  int mmap_bits = (TPL_RDONLY | TPL_FILE);
  int ufree_bits = (TPL_MEM | TPL_UFREE);
  tpl_node *nxtc, *c;
  int find_next_node = 0, looking, num, i;
  tpl_pidx *pidx, *pidx_nxt;

  /* For mmap'd files, or for 'ufree' memory images , do appropriate release */
  if ((((tpl_root_data *) (r->data))->flags & mmap_bits) == mmap_bits) {
    tpl_unmap_file (&((tpl_root_data *) (r->data))->mmap);
  } else if ((((tpl_root_data *) (r->data))->flags & ufree_bits) == ufree_bits) {
    tpl_hook.free (((tpl_root_data *) (r->data))->mmap.text);
  }

  c = r->children;
  if (c) {
    while (c->type != TPL_TYPE_ROOT) {  /* loop until we come back to root node */
      switch (c->type) {
      case TPL_TYPE_BIN:
        /* free any binary buffer hanging from tpl_bin */
        if (*((tpl_bin **) (c->data))) {
          if ((*((tpl_bin **) (c->data)))->sz != 0) {
            tpl_hook.free ((*((tpl_bin **) (c->data)))->addr);
          }
          tpl_hook.free (*((tpl_bin **) c->data));      /* free tpl_bin */
        }
        tpl_hook.free (c->data);        /* free tpl_bin* */
        find_next_node = 1;
        break;
      case TPL_TYPE_STR:
        /* free any packed (copied) string */
        num = 1;
        nxtc = c->next;
        while (nxtc) {
          if (nxtc->type == TPL_TYPE_POUND) {
            num = nxtc->num;
          }
          nxtc = nxtc->next;
        }
        for (i = 0; i < c->num * num; i++) {
          char *str = ((char **) c->data)[i];
          if (str) {
            tpl_hook.free (str);
            ((char **) c->data)[i] = NULL;
          }
        }
        tpl_hook.free (c->data);
        find_next_node = 1;
        break;
      case TPL_TYPE_INT32:
      case TPL_TYPE_UINT32:
      case TPL_TYPE_INT64:
      case TPL_TYPE_UINT64:
      case TPL_TYPE_BYTE:
      case TPL_TYPE_DOUBLE:
      case TPL_TYPE_INT16:
      case TPL_TYPE_UINT16:
      case TPL_TYPE_POUND:
        tpl_hook.free (c->data);
        find_next_node = 1;
        break;
      case TPL_TYPE_ARY:
        tpl_free_atyp (c, c->data);
        if (c->children)
          c = c->children;      /* normal case */
        else
          find_next_node = 1;   /* edge case, handle bad format A() */
        break;
      default:
        tpl_hook.fatal ("unsupported format character\n");
        break;
      }

      if (find_next_node) {
        find_next_node = 0;
        looking = 1;
        while (looking) {
          if (c->next) {
            nxtc = c->next;
            tpl_hook.free (c);
            c = nxtc;
            looking = 0;
          } else {
            if (c->type == TPL_TYPE_ROOT)
              break;    /* root node */
            else {
              nxtc = c->parent;
              tpl_hook.free (c);
              c = nxtc;
            }
          }
        }
      }
    }
  }

  /* free root */
  for (pidx = ((tpl_root_data *) (r->data))->pidx; pidx; pidx = pidx_nxt) {
    pidx_nxt = pidx->next;
    tpl_hook.free (pidx);
  }
  tpl_hook.free (((tpl_root_data *) (r->data))->fmt);
  if (((tpl_root_data *) (r->data))->num_fxlens > 0) {
    tpl_hook.free (((tpl_root_data *) (r->data))->fxlens);
  }
  tpl_hook.free (r->data);      /* tpl_root_data */
  tpl_hook.free (r);
}


/* Find the i'th packable ('A' node) */
static tpl_node *
tpl_find_i (tpl_node * n, int i) {
  int j = 0;
  tpl_pidx *pidx;
  if (n->type != TPL_TYPE_ROOT)
    return NULL;
  if (i == 0)
    return n;   /* packable 0 is root */
  for (pidx = ((tpl_root_data *) (n->data))->pidx; pidx; pidx = pidx->next) {
    if (++j == i)
      return pidx->node;
  }
  return NULL;
}

static void *
tpl_cpv (void *datav, const void *data, size_t sz) {
  if (sz > 0)
    memcpy (datav, data, sz);
  return (void *) ((uintptr_t) datav + sz);
}

static void *
tpl_extend_backbone (tpl_node * n) {
  tpl_backbone *bb;
  bb = (tpl_backbone *) tpl_hook.malloc (sizeof (tpl_backbone) + ((tpl_atyp *) (n->data))->sz); /* datum hangs on coattails of bb */
  if (!bb)
    fatal_oom ();
#if __STDC_VERSION__ < 199901
  bb->data = (char *) ((uintptr_t) bb + sizeof (tpl_backbone));
#endif
  memset (bb->data, 0, ((tpl_atyp *) (n->data))->sz);
  bb->next = NULL;
  /* Add the new backbone to the tail, also setting head if necessary  */
  if (((tpl_atyp *) (n->data))->bb == NULL) {
    ((tpl_atyp *) (n->data))->bb = bb;
    ((tpl_atyp *) (n->data))->bbtail = bb;
  } else {
    ((tpl_atyp *) (n->data))->bbtail->next = bb;
    ((tpl_atyp *) (n->data))->bbtail = bb;
  }

  ((tpl_atyp *) (n->data))->num++;
  return bb->data;
}

/* Get the format string corresponding to a given tpl (root node) */
static char *
tpl_fmt (tpl_node * r) {
  return ((tpl_root_data *) (r->data))->fmt;
}

/* Get the fmt # lengths as a contiguous buffer of ints (length num_fxlens) */
static int *
tpl_fxlens (tpl_node * r, int *num_fxlens) {
  *num_fxlens = ((tpl_root_data *) (r->data))->num_fxlens;
  return ((tpl_root_data *) (r->data))->fxlens;
}

/* called when serializing an 'A' type node into a buffer which has
 * already been set up with the proper space. The backbone is walked
 * which was obtained from the tpl_atyp header passed in.
 */
static void *
tpl_dump_atyp (tpl_node * n, tpl_atyp * at, void *dv) {
  tpl_backbone *bb;
  tpl_node *c;
  void *datav;
  uint32_t slen;
  tpl_bin *binp;
  char *strp;
  tpl_atyp *atypp;
  tpl_pound_data *pd;
  int i;
  size_t itermax;

  /* handle 'A' nodes */
  dv = tpl_cpv (dv, &at->num, sizeof (uint32_t));       /* array len */
  for (bb = at->bb; bb; bb = bb->next) {
    datav = bb->data;
    c = n->children;
    while (c) {
      switch (c->type) {
      case TPL_TYPE_BYTE:
      case TPL_TYPE_DOUBLE:
      case TPL_TYPE_INT32:
      case TPL_TYPE_UINT32:
      case TPL_TYPE_INT64:
      case TPL_TYPE_UINT64:
      case TPL_TYPE_INT16:
      case TPL_TYPE_UINT16:
        dv = tpl_cpv (dv, datav, tpl_types[c->type].sz * c->num);
        datav = (void *) ((uintptr_t) datav + tpl_types[c->type].sz * c->num);
        break;
      case TPL_TYPE_BIN:
        /* dump the buffer length followed by the buffer */
        memcpy (&binp, datav, sizeof (tpl_bin *));      /* cp to aligned */
        slen = binp->sz;
        dv = tpl_cpv (dv, &slen, sizeof (uint32_t));
        dv = tpl_cpv (dv, binp->addr, slen);
        datav = (void *) ((uintptr_t) datav + sizeof (tpl_bin *));
        break;
      case TPL_TYPE_STR:
        /* dump the string length followed by the string */
        for (i = 0; i < c->num; i++) {
          memcpy (&strp, datav, sizeof (char *));       /* cp to aligned */
          slen = strp ? (strlen (strp) + 1) : 0;
          dv = tpl_cpv (dv, &slen, sizeof (uint32_t));
          if (slen > 1)
            dv = tpl_cpv (dv, strp, slen - 1);
          datav = (void *) ((uintptr_t) datav + sizeof (char *));
        }
        break;
      case TPL_TYPE_ARY:
        memcpy (&atypp, datav, sizeof (tpl_atyp *));    /* cp to aligned */
        dv = tpl_dump_atyp (c, atypp, dv);
        datav = (void *) ((uintptr_t) datav + sizeof (void *));
        break;
      case TPL_TYPE_POUND:
        /* iterate over the preceding nodes */
        pd = (tpl_pound_data *) c->data;
        itermax = c->num;
        if (++(pd->iternum) < itermax) {
          c = pd->iter_start_node;
          continue;
        } else {        /* loop complete. */
          pd->iternum = 0;
        }
        break;
      default:
        tpl_hook.fatal ("unsupported format character\n");
        break;
      }
      c = c->next;
    }
  }
  return dv;
}

/* figure the serialization output size needed for tpl whose root is n*/
static size_t
tpl_ser_osz (tpl_node * n) {
  tpl_node *c, *np;
  size_t sz, itermax;
  tpl_bin *binp;
  char *strp;
  tpl_pound_data *pd;
  int i;

  /* handle the root node ONLY (subtree's ser_osz have been bubbled-up) */
  if (n->type != TPL_TYPE_ROOT) {
    tpl_hook.fatal ("internal error: tpl_ser_osz on non-root node\n");
  }

  sz = n->ser_osz;      /* start with fixed overhead, already stored */
  c = n->children;
  while (c) {
    switch (c->type) {
    case TPL_TYPE_BYTE:
    case TPL_TYPE_DOUBLE:
    case TPL_TYPE_INT32:
    case TPL_TYPE_UINT32:
    case TPL_TYPE_INT64:
    case TPL_TYPE_UINT64:
    case TPL_TYPE_INT16:
    case TPL_TYPE_UINT16:
      sz += tpl_types[c->type].sz * c->num;
      break;
    case TPL_TYPE_BIN:
      sz += sizeof (uint32_t);  /* binary buf len */
      memcpy (&binp, c->data, sizeof (tpl_bin *));      /* cp to aligned */
      sz += binp->sz;
      break;
    case TPL_TYPE_STR:
      for (i = 0; i < c->num; i++) {
        sz += sizeof (uint32_t);        /* string len */
        memcpy (&strp, &((char **) c->data)[i], sizeof (char *));       /* cp to aligned */
        sz += strp ? strlen (strp) : 0;
      }
      break;
    case TPL_TYPE_ARY:
      sz += sizeof (uint32_t);  /* array len */
      sz += c->ser_osz; /* bubbled-up child array ser_osz */
      break;
    case TPL_TYPE_POUND:
      /* iterate over the preceding nodes */
      itermax = c->num;
      pd = (tpl_pound_data *) c->data;
      if (++(pd->iternum) < itermax) {
        for (np = pd->iter_start_node; np != c; np = np->next) {
          np->data = (char *) (np->data) + (tpl_types[np->type].sz * np->num);
        }
        c = pd->iter_start_node;
        continue;
      } else {  /* loop complete. */
        pd->iternum = 0;
        for (np = pd->iter_start_node; np != c; np = np->next) {
          np->data = (char *) (np->data) - ((itermax - 1) * tpl_types[np->type].sz * np->num);
        }
      }
      break;
    default:
      tpl_hook.fatal ("unsupported format character\n");
      break;
    }
    c = c->next;
  }
  return sz;
}


TPL_API int
tpl_dump (tpl_node * r, int mode, ...) {
  va_list ap;
  char *filename, *bufv;
  void **addr_out, *buf, *pa_addr;
  int fd, rc = 0;
  size_t sz, *sz_out, pa_sz;
  struct stat sbuf;

  if (((tpl_root_data *) (r->data))->flags & TPL_RDONLY) {      /* unusual */
    tpl_hook.oops ("error: tpl_dump called for a loaded tpl\n");
    return -1;
  }

  sz = tpl_ser_osz (r); /* compute the size needed to serialize  */

  va_start (ap, mode);
  if (mode & TPL_FILE) {
    filename = va_arg (ap, char *);
    fd = tpl_mmap_output_file (filename, sz, &buf);
    if (fd == -1)
      rc = -1;
    else {
      rc = tpl_dump_to_mem (r, buf, sz);
      if (msync (buf, sz, MS_SYNC) == -1) {
        tpl_hook.oops ("msync failed on fd %d: %s\n", fd, strerror (errno));
      }
      if (munmap (buf, sz) == -1) {
        tpl_hook.oops ("munmap failed on fd %d: %s\n", fd, strerror (errno));
      }
      close (fd);
    }
  } else if (mode & TPL_FD) {
    fd = va_arg (ap, int);
    if ((buf = tpl_hook.malloc (sz)) == NULL)
      fatal_oom ();
    tpl_dump_to_mem (r, buf, sz);
    bufv = buf;
    do {
      rc = write (fd, bufv, sz);
      if (rc > 0) {
        sz -= rc;
        bufv += rc;
      } else if (rc == -1) {
        if (errno == EINTR || errno == EAGAIN)
          continue;
        tpl_hook.oops ("error writing to fd %d: %s\n", fd, strerror (errno));
        /* attempt to rewind partial write to a regular file */
        if (fstat (fd, &sbuf) == 0 && S_ISREG (sbuf.st_mode)) {
          if (ftruncate (fd, sbuf.st_size - (bufv - (char *) buf)) == -1) {
            tpl_hook.oops ("can't rewind: %s\n", strerror (errno));
          }
        }
        free (buf);
        return -1;
      }
    } while (sz > 0);
    free (buf);
    rc = 0;
  } else if (mode & TPL_MEM) {
    if (mode & TPL_PREALLOCD) { /* caller allocated */
      pa_addr = (void *) va_arg (ap, void *);
      pa_sz = va_arg (ap, size_t);
      if (pa_sz < sz) {
        tpl_hook.oops ("tpl_dump: buffer too small, need %zu bytes\n", sz);
        return -1;
      }
      rc = tpl_dump_to_mem (r, pa_addr, sz);
    } else {    /* we allocate */
      addr_out = (void **) va_arg (ap, void *);
      sz_out = va_arg (ap, size_t *);
      if ((buf = tpl_hook.malloc (sz)) == NULL)
        fatal_oom ();
      *sz_out = sz;
      *addr_out = buf;
      rc = tpl_dump_to_mem (r, buf, sz);
    }
  } else if (mode & TPL_GETSIZE) {
    sz_out = va_arg (ap, size_t *);
    *sz_out = sz;
  } else {
    tpl_hook.oops ("unsupported tpl_dump mode %d\n", mode);
    rc = -1;
  }
  va_end (ap);
  return rc;
}

/* This function expects the caller to have set up a memory buffer of
 * adequate size to hold the serialized tpl. The sz parameter must be
 * the result of tpl_ser_osz(r).
 */
static int
tpl_dump_to_mem (tpl_node * r, void *addr, size_t sz) {
  uint32_t slen, sz32;
  int *fxlens, num_fxlens, i;
  void *dv;
  char *fmt, flags;
  tpl_node *c, *np;
  tpl_pound_data *pd;
  size_t itermax;

  fmt = tpl_fmt (r);
  flags = 0;
  if (tpl_cpu_bigendian ())
    flags |= TPL_FL_BIGENDIAN;
  if (strchr (fmt, 's'))
    flags |= TPL_FL_NULLSTRINGS;
  sz32 = sz;

  dv = addr;
  dv = tpl_cpv (dv, TPL_MAGIC, 3);      /* copy tpl magic prefix */
  dv = tpl_cpv (dv, &flags, 1); /* copy flags byte */
  dv = tpl_cpv (dv, &sz32, sizeof (uint32_t));  /* overall length (inclusive) */
  dv = tpl_cpv (dv, fmt, strlen (fmt) + 1);     /* copy format with NUL-term */
  fxlens = tpl_fxlens (r, &num_fxlens);
  dv = tpl_cpv (dv, fxlens, num_fxlens * sizeof (uint32_t));    /* fmt # lengths */

  /* serialize the tpl content, iterating over direct children of root */
  c = r->children;
  while (c) {
    switch (c->type) {
    case TPL_TYPE_BYTE:
    case TPL_TYPE_DOUBLE:
    case TPL_TYPE_INT32:
    case TPL_TYPE_UINT32:
    case TPL_TYPE_INT64:
    case TPL_TYPE_UINT64:
    case TPL_TYPE_INT16:
    case TPL_TYPE_UINT16:
      dv = tpl_cpv (dv, c->data, tpl_types[c->type].sz * c->num);
      break;
    case TPL_TYPE_BIN:
      slen = (*(tpl_bin **) (c->data))->sz;
      dv = tpl_cpv (dv, &slen, sizeof (uint32_t));      /* buffer len */
      dv = tpl_cpv (dv, (*(tpl_bin **) (c->data))->addr, slen); /* buf */
      break;
    case TPL_TYPE_STR:
      for (i = 0; i < c->num; i++) {
        char *str = ((char **) c->data)[i];
        slen = str ? strlen (str) + 1 : 0;
        dv = tpl_cpv (dv, &slen, sizeof (uint32_t));    /* string len */
        if (slen > 1)
          dv = tpl_cpv (dv, str, slen - 1);     /*string */
      }
      break;
    case TPL_TYPE_ARY:
      dv = tpl_dump_atyp (c, (tpl_atyp *) c->data, dv);
      break;
    case TPL_TYPE_POUND:
      pd = (tpl_pound_data *) c->data;
      itermax = c->num;
      if (++(pd->iternum) < itermax) {

        /* in start or midst of loop. advance data pointers. */
        for (np = pd->iter_start_node; np != c; np = np->next) {
          np->data = (char *) (np->data) + (tpl_types[np->type].sz * np->num);
        }
        /* do next iteration */
        c = pd->iter_start_node;
        continue;

      } else {  /* loop complete. */

        /* reset iteration index and addr/data pointers. */
        pd->iternum = 0;
        for (np = pd->iter_start_node; np != c; np = np->next) {
          np->data = (char *) (np->data) - ((itermax - 1) * tpl_types[np->type].sz * np->num);
        }

      }
      break;
    default:
      tpl_hook.fatal ("unsupported format character\n");
      break;
    }
    c = c->next;
  }

  return 0;
}

static int
tpl_cpu_bigendian (void) {
  unsigned i = 1;
  char *c;
  c = (char *) &i;
  return (c[0] == 1 ? 0 : 1);
}


/*
 * algorithm for sanity-checking a tpl image:
 * scan the tpl whilst not exceeding the buffer size (bufsz) ,
 * formulating a calculated (expected) size of the tpl based
 * on walking its data. When calcsize has been calculated it
 * should exactly match the buffer size (bufsz) and the internal
 * recorded size (intlsz)
 */
static int
tpl_sanity (tpl_node * r, int excess_ok) {
  uint32_t intlsz;
  int found_nul = 0, rc, octothorpes = 0, num_fxlens, *fxlens, flen;
  void *d, *dv;
  char intlflags, *fmt, c, *mapfmt;
  size_t bufsz, serlen;

  d = ((tpl_root_data *) (r->data))->mmap.text;
  bufsz = ((tpl_root_data *) (r->data))->mmap.text_sz;

  dv = d;
  if (bufsz < (4 + sizeof (uint32_t) + 1))
    return ERR_NOT_MINSIZE;     /* min sz: magic+flags+len+nul */
  if (memcmp (dv, TPL_MAGIC, 3) != 0)
    return ERR_MAGIC_MISMATCH;  /* missing tpl magic prefix */
  if (tpl_needs_endian_swap (dv))
    ((tpl_root_data *) (r->data))->flags |= TPL_XENDIAN;
  dv = (void *) ((uintptr_t) dv + 3);
  memcpy (&intlflags, dv, sizeof (char));       /* extract flags */
  if (intlflags & ~TPL_SUPPORTED_BITFLAGS)
    return ERR_UNSUPPORTED_FLAGS;
  /* TPL1.3 stores strings with a "length+1" prefix to discern NULL strings from
     empty strings from non-empty strings; TPL1.2 only handled the latter two.
     So we need to be mindful of which string format we're reading from. */
  if (!(intlflags & TPL_FL_NULLSTRINGS)) {
    ((tpl_root_data *) (r->data))->flags |= TPL_OLD_STRING_FMT;
  }
  dv = (void *) ((uintptr_t) dv + 1);
  memcpy (&intlsz, dv, sizeof (uint32_t));      /* extract internal size */
  if (((tpl_root_data *) (r->data))->flags & TPL_XENDIAN)
    tpl_byteswap (&intlsz, sizeof (uint32_t));
  if (!excess_ok && (intlsz != bufsz))
    return ERR_INCONSISTENT_SZ; /* inconsisent buffer/internal size */
  dv = (void *) ((uintptr_t) dv + sizeof (uint32_t));

  /* dv points to the start of the format string. Look for nul w/in buf sz */
  fmt = (char *) dv;
  while ((uintptr_t) dv - (uintptr_t) d < bufsz && !found_nul) {
    if ((c = *(char *) dv) != '\0') {
      if (strchr (tpl_fmt_chars, c) == NULL)
        return ERR_FMT_INVALID; /* invalid char in format string */
      if ((c = *(char *) dv) == '#')
        octothorpes++;
      dv = (void *) ((uintptr_t) dv + 1);
    } else
      found_nul = 1;
  }
  if (!found_nul)
    return ERR_FMT_MISSING_NUL; /* runaway format string */
  dv = (void *) ((uintptr_t) dv + 1);   /* advance to octothorpe lengths buffer */

  /* compare the map format to the format of this tpl image */
  mapfmt = tpl_fmt (r);
  rc = strcmp (mapfmt, fmt);
  if (rc != 0)
    return ERR_FMT_MISMATCH;

  /* compare octothorpe lengths in image to the mapped values */
  if ((((uintptr_t) dv + (octothorpes * 4)) - (uintptr_t) d) > bufsz)
    return ERR_INCONSISTENT_SZ4;
  fxlens = tpl_fxlens (r, &num_fxlens); /* mapped fxlens */
  while (num_fxlens--) {
    memcpy (&flen, dv, sizeof (uint32_t));      /* stored flen */
    if (((tpl_root_data *) (r->data))->flags & TPL_XENDIAN)
      tpl_byteswap (&flen, sizeof (uint32_t));
    if (flen != *fxlens)
      return ERR_FLEN_MISMATCH;
    dv = (void *) ((uintptr_t) dv + sizeof (uint32_t));
    fxlens++;
  }

  /* dv now points to beginning of data */
  rc = tpl_serlen (r, r, dv, &serlen);  /* get computed serlen of data part */
  if (rc == -1)
    return ERR_INCONSISTENT_SZ2;        /* internal inconsistency in tpl image */
  serlen += ((uintptr_t) dv - (uintptr_t) d);   /* add back serlen of preamble part */
  if (excess_ok && (bufsz < serlen))
    return ERR_INCONSISTENT_SZ3;
  if (!excess_ok && (serlen != bufsz))
    return ERR_INCONSISTENT_SZ3;        /* buffer/internal sz exceeds serlen */
  return 0;
}

static void *
tpl_find_data_start (void *d) {
  int octothorpes = 0;
  d = (void *) ((uintptr_t) d + 4);     /* skip TPL_MAGIC and flags byte */
  d = (void *) ((uintptr_t) d + 4);     /* skip int32 overall len */
  while (*(char *) d != '\0') {
    if (*(char *) d == '#')
      octothorpes++;
    d = (void *) ((uintptr_t) d + 1);
  }
  d = (void *) ((uintptr_t) d + 1);     /* skip NUL */
  d = (void *) ((uintptr_t) d + (octothorpes * sizeof (uint32_t)));     /* skip # array lens */
  return d;
}

static int
tpl_needs_endian_swap (void *d) {
  char *c;
  int cpu_is_bigendian;
  c = (char *) d;
  cpu_is_bigendian = tpl_cpu_bigendian ();
  return ((c[3] & TPL_FL_BIGENDIAN) == cpu_is_bigendian) ? 0 : 1;
}

static size_t
tpl_size_for (char c) {
  uint32_t i;
  for (i = 0; i < sizeof (tpl_types) / sizeof (tpl_types[0]); i++) {
    if (tpl_types[i].c == c)
      return tpl_types[i].sz;
  }
  return 0;
}

TPL_API char *
tpl_peek (int mode, ...) {
  va_list ap;
  int xendian = 0, found_nul = 0, old_string_format = 0;
  char *filename = NULL, *datapeek_f = NULL, *datapeek_c, *datapeek_s;
  void *addr = NULL, *dv, *datapeek_p = NULL;
  size_t sz = 0, fmt_len, first_atom, num_fxlens = 0;
  uint32_t datapeek_ssz, datapeek_csz, datapeek_flen;
  tpl_mmap_rec mr = { 0, NULL, 0 };
  char *fmt, *fmt_cpy = NULL, c;
  uint32_t intlsz, **fxlens = NULL, *num_fxlens_out = NULL, *fxlensv;

  va_start (ap, mode);
  if ((mode & TPL_FXLENS) && (mode & TPL_DATAPEEK)) {
    tpl_hook.oops ("TPL_FXLENS and TPL_DATAPEEK mutually exclusive\n");
    goto fail;
  }
  if (mode & TPL_FILE)
    filename = va_arg (ap, char *);
  else if (mode & TPL_MEM) {
    addr = va_arg (ap, void *);
    sz = va_arg (ap, size_t);
  } else {
    tpl_hook.oops ("unsupported tpl_peek mode %d\n", mode);
    goto fail;
  }
  if (mode & TPL_DATAPEEK) {
    datapeek_f = va_arg (ap, char *);
  }
  if (mode & TPL_FXLENS) {
    num_fxlens_out = va_arg (ap, uint32_t *);
    fxlens = va_arg (ap, uint32_t **);
    *num_fxlens_out = 0;
    *fxlens = NULL;
  }

  if (mode & TPL_FILE) {
    if (tpl_mmap_file (filename, &mr) != 0) {
      tpl_hook.oops ("tpl_peek failed for file %s\n", filename);
      goto fail;
    }
    addr = mr.text;
    sz = mr.text_sz;
  }

  dv = addr;
  if (sz < (4 + sizeof (uint32_t) + 1))
    goto fail;  /* min sz */
  if (memcmp (dv, TPL_MAGIC, 3) != 0)
    goto fail;  /* missing tpl magic prefix */
  if (tpl_needs_endian_swap (dv))
    xendian = 1;
  if ((((char *) dv)[3] & TPL_FL_NULLSTRINGS) == 0)
    old_string_format = 1;
  dv = (void *) ((uintptr_t) dv + 4);
  memcpy (&intlsz, dv, sizeof (uint32_t));      /* extract internal size */
  if (xendian)
    tpl_byteswap (&intlsz, sizeof (uint32_t));
  if (intlsz != sz)
    goto fail;  /* inconsisent buffer/internal size */
  dv = (void *) ((uintptr_t) dv + sizeof (uint32_t));

  /* dv points to the start of the format string. Look for nul w/in buf sz */
  fmt = (char *) dv;
  while ((uintptr_t) dv - (uintptr_t) addr < sz && !found_nul) {
    if ((c = *(char *) dv) == '\0') {
      found_nul = 1;
    } else if (c == '#') {
      num_fxlens++;
    }
    dv = (void *) ((uintptr_t) dv + 1);
  }
  if (!found_nul)
    goto fail;  /* runaway format string */
  fmt_len = (char *) dv - fmt;  /* include space for \0 */
  fmt_cpy = tpl_hook.malloc (fmt_len);
  if (fmt_cpy == NULL) {
    fatal_oom ();
  }
  memcpy (fmt_cpy, fmt, fmt_len);

  /* retrieve the octothorpic lengths if requested */
  if (num_fxlens > 0) {
    if (sz < ((uintptr_t) dv + (num_fxlens * sizeof (uint32_t)) - (uintptr_t) addr)) {
      goto fail;
    }
  }
  if ((mode & TPL_FXLENS) && (num_fxlens > 0)) {
    *fxlens = tpl_hook.malloc (num_fxlens * sizeof (uint32_t));
    if (*fxlens == NULL)
      tpl_hook.fatal ("out of memory");
    *num_fxlens_out = num_fxlens;
    fxlensv = *fxlens;
    while (num_fxlens--) {
      memcpy (fxlensv, dv, sizeof (uint32_t));
      if (xendian)
        tpl_byteswap (fxlensv, sizeof (uint32_t));
      dv = (void *) ((uintptr_t) dv + sizeof (uint32_t));
      fxlensv++;
    }
  }
  /* if caller requested, peek into the specified data elements */
  if (mode & TPL_DATAPEEK) {

    first_atom = strspn (fmt, "S()");   /* skip any leading S() */

    datapeek_flen = strlen (datapeek_f);
    if (strspn (datapeek_f, tpl_datapeek_ok_chars) < datapeek_flen) {
      tpl_hook.oops ("invalid TPL_DATAPEEK format: %s\n", datapeek_f);
      tpl_hook.free (fmt_cpy);
      fmt_cpy = NULL;   /* fail */
      goto fail;
    }

    if (strncmp (&fmt[first_atom], datapeek_f, datapeek_flen) != 0) {
      tpl_hook.oops ("TPL_DATAPEEK format mismatches tpl iamge\n");
      tpl_hook.free (fmt_cpy);
      fmt_cpy = NULL;   /* fail */
      goto fail;
    }

    /* advance to data start, then copy out requested elements */
    dv = (void *) ((uintptr_t) dv + (num_fxlens * sizeof (uint32_t)));
    for (datapeek_c = datapeek_f; *datapeek_c != '\0'; datapeek_c++) {
      datapeek_p = va_arg (ap, void *);
      if (*datapeek_c == 's') { /* special handling for strings */
        if ((uintptr_t) dv - (uintptr_t) addr + sizeof (uint32_t) > sz) {
          tpl_hook.oops ("tpl_peek: tpl has insufficient length\n");
          tpl_hook.free (fmt_cpy);
          fmt_cpy = NULL;       /* fail */
          goto fail;
        }
        memcpy (&datapeek_ssz, dv, sizeof (uint32_t));  /* get slen */
        if (xendian)
          tpl_byteswap (&datapeek_ssz, sizeof (uint32_t));
        if (old_string_format)
          datapeek_ssz++;
        dv = (void *) ((uintptr_t) dv + sizeof (uint32_t));     /* adv. to str */
        if (datapeek_ssz == 0)
          datapeek_s = NULL;
        else {
          if ((uintptr_t) dv - (uintptr_t) addr + datapeek_ssz - 1 > sz) {
            tpl_hook.oops ("tpl_peek: tpl has insufficient length\n");
            tpl_hook.free (fmt_cpy);
            fmt_cpy = NULL;     /* fail */
            goto fail;
          }
          datapeek_s = tpl_hook.malloc (datapeek_ssz);
          if (datapeek_s == NULL)
            fatal_oom ();
          memcpy (datapeek_s, dv, datapeek_ssz - 1);
          datapeek_s[datapeek_ssz - 1] = '\0';
          dv = (void *) ((uintptr_t) dv + datapeek_ssz - 1);
        }
        *(char **) datapeek_p = datapeek_s;
      } else {
        datapeek_csz = tpl_size_for (*datapeek_c);
        if ((uintptr_t) dv - (uintptr_t) addr + datapeek_csz > sz) {
          tpl_hook.oops ("tpl_peek: tpl has insufficient length\n");
          tpl_hook.free (fmt_cpy);
          fmt_cpy = NULL;       /* fail */
          goto fail;
        }
        memcpy (datapeek_p, dv, datapeek_csz);
        if (xendian)
          tpl_byteswap (datapeek_p, datapeek_csz);
        dv = (void *) ((uintptr_t) dv + datapeek_csz);
      }
    }
  }

fail:
  va_end (ap);
  if ((mode & TPL_FILE) && mr.text != NULL)
    tpl_unmap_file (&mr);
  return fmt_cpy;
}

/* tpl_jot(TPL_FILE, "file.tpl", "si", &s, &i); */
/* tpl_jot(TPL_MEM, &buf, &sz, "si", &s, &i); */
/* tpl_jot(TPL_FD, fd, "si", &s, &i); */
TPL_API int
tpl_jot (int mode, ...) {
  va_list ap;
  char *filename, *fmt;
  size_t *sz;
  int fd, rc = 0;
  void **buf;
  tpl_node *tn;

  va_start (ap, mode);
  if (mode & TPL_FILE) {
    filename = va_arg (ap, char *);
    fmt = va_arg (ap, char *);
    tn = tpl_map_va (fmt, ap);
    if (tn == NULL) {
      rc = -1;
      goto fail;
    }
    tpl_pack (tn, 0);
    rc = tpl_dump (tn, TPL_FILE, filename);
    tpl_free (tn);
  } else if (mode & TPL_MEM) {
    buf = va_arg (ap, void *);
    sz = va_arg (ap, size_t *);
    fmt = va_arg (ap, char *);
    tn = tpl_map_va (fmt, ap);
    if (tn == NULL) {
      rc = -1;
      goto fail;
    }
    tpl_pack (tn, 0);
    rc = tpl_dump (tn, TPL_MEM, buf, sz);
    tpl_free (tn);
  } else if (mode & TPL_FD) {
    fd = va_arg (ap, int);
    fmt = va_arg (ap, char *);
    tn = tpl_map_va (fmt, ap);
    if (tn == NULL) {
      rc = -1;
      goto fail;
    }
    tpl_pack (tn, 0);
    rc = tpl_dump (tn, TPL_FD, fd);
    tpl_free (tn);
  } else {
    tpl_hook.fatal ("invalid tpl_jot mode\n");
  }

fail:
  va_end (ap);
  return rc;
}

TPL_API int
tpl_load (tpl_node * r, int mode, ...) {
  va_list ap;
  int rc = 0, fd = 0;
  char *filename = NULL;
  void *addr;
  size_t sz;

  va_start (ap, mode);
  if (mode & TPL_FILE)
    filename = va_arg (ap, char *);
  else if (mode & TPL_MEM) {
    addr = va_arg (ap, void *);
    sz = va_arg (ap, size_t);
  } else if (mode & TPL_FD) {
    fd = va_arg (ap, int);
  } else {
    tpl_hook.oops ("unsupported tpl_load mode %d\n", mode);
    va_end (ap);
    return -1;
  }
  va_end (ap);

  if (r->type != TPL_TYPE_ROOT) {
    tpl_hook.oops ("error: tpl_load to non-root node\n");
    return -1;
  }
  if (((tpl_root_data *) (r->data))->flags & (TPL_WRONLY | TPL_RDONLY)) {
    /* already packed or loaded, so reset it as if newly mapped */
    tpl_free_keep_map (r);
  }
  if (mode & TPL_FILE) {
    if (tpl_mmap_file (filename, &((tpl_root_data *) (r->data))->mmap) != 0) {
      tpl_hook.oops ("tpl_load failed for file %s\n", filename);
      return -1;
    }
    if ((rc = tpl_sanity (r, (mode & TPL_EXCESS_OK))) != 0) {
      if (rc == ERR_FMT_MISMATCH) {
        tpl_hook.oops ("%s: format signature mismatch\n", filename);
      } else if (rc == ERR_FLEN_MISMATCH) {
        tpl_hook.oops ("%s: array lengths mismatch\n", filename);
      } else {
        tpl_hook.oops ("%s: not a valid tpl file\n", filename);
      }
      tpl_unmap_file (&((tpl_root_data *) (r->data))->mmap);
      return -1;
    }
    ((tpl_root_data *) (r->data))->flags = (TPL_FILE | TPL_RDONLY);
  } else if (mode & TPL_MEM) {
    ((tpl_root_data *) (r->data))->mmap.text = addr;
    ((tpl_root_data *) (r->data))->mmap.text_sz = sz;
    if ((rc = tpl_sanity (r, (mode & TPL_EXCESS_OK))) != 0) {
      if (rc == ERR_FMT_MISMATCH) {
        tpl_hook.oops ("format signature mismatch\n");
      } else {
        tpl_hook.oops ("not a valid tpl file\n");
      }
      return -1;
    }
    ((tpl_root_data *) (r->data))->flags = (TPL_MEM | TPL_RDONLY);
    if (mode & TPL_UFREE)
      ((tpl_root_data *) (r->data))->flags |= TPL_UFREE;
  } else if (mode & TPL_FD) {
    /* if fd read succeeds, resulting mem img is used for load */
    if (tpl_gather (TPL_GATHER_BLOCKING, fd, &addr, &sz) > 0) {
      return tpl_load (r, TPL_MEM | TPL_UFREE, addr, sz);
    } else
      return -1;
  } else {
    tpl_hook.oops ("invalid tpl_load mode %d\n", mode);
    return -1;
  }
  /* this applies to TPL_MEM or TPL_FILE */
  if (tpl_needs_endian_swap (((tpl_root_data *) (r->data))->mmap.text))
    ((tpl_root_data *) (r->data))->flags |= TPL_XENDIAN;
  tpl_unpackA0 (r);     /* prepare root A nodes for use */
  return 0;
}

TPL_API int
tpl_Alen (tpl_node * r, int i) {
  tpl_node *n;

  n = tpl_find_i (r, i);
  if (n == NULL) {
    tpl_hook.oops ("invalid index %d to tpl_unpack\n", i);
    return -1;
  }
  if (n->type != TPL_TYPE_ARY)
    return -1;
  return ((tpl_atyp *) (n->data))->num;
}

static void
tpl_free_atyp (tpl_node * n, tpl_atyp * atyp) {
  tpl_backbone *bb, *bbnxt;
  tpl_node *c;
  void *dv;
  tpl_bin *binp;
  tpl_atyp *atypp;
  char *strp;
  size_t itermax;
  tpl_pound_data *pd;
  int i;

  bb = atyp->bb;
  while (bb) {
    bbnxt = bb->next;
    dv = bb->data;
    c = n->children;
    while (c) {
      switch (c->type) {
      case TPL_TYPE_BYTE:
      case TPL_TYPE_DOUBLE:
      case TPL_TYPE_INT32:
      case TPL_TYPE_UINT32:
      case TPL_TYPE_INT64:
      case TPL_TYPE_UINT64:
      case TPL_TYPE_INT16:
      case TPL_TYPE_UINT16:
        dv = (void *) ((uintptr_t) dv + tpl_types[c->type].sz * c->num);
        break;
      case TPL_TYPE_BIN:
        memcpy (&binp, dv, sizeof (tpl_bin *)); /* cp to aligned */
        if (binp->addr)
          tpl_hook.free (binp->addr);   /* free buf */
        tpl_hook.free (binp);   /* free tpl_bin */
        dv = (void *) ((uintptr_t) dv + sizeof (tpl_bin *));
        break;
      case TPL_TYPE_STR:
        for (i = 0; i < c->num; i++) {
          memcpy (&strp, dv, sizeof (char *));  /* cp to aligned */
          if (strp)
            tpl_hook.free (strp);       /* free string */
          dv = (void *) ((uintptr_t) dv + sizeof (char *));
        }
        break;
      case TPL_TYPE_POUND:
        /* iterate over the preceding nodes */
        itermax = c->num;
        pd = (tpl_pound_data *) c->data;
        if (++(pd->iternum) < itermax) {
          c = pd->iter_start_node;
          continue;
        } else {        /* loop complete. */
          pd->iternum = 0;
        }
        break;
      case TPL_TYPE_ARY:
        memcpy (&atypp, dv, sizeof (tpl_atyp *));       /* cp to aligned */
        tpl_free_atyp (c, atypp);       /* free atyp */
        dv = (void *) ((uintptr_t) dv + sizeof (void *));
        break;
      default:
        tpl_hook.fatal ("unsupported format character\n");
        break;
      }
      c = c->next;
    }
    tpl_hook.free (bb);
    bb = bbnxt;
  }
  tpl_hook.free (atyp);
}

/* determine (by walking) byte length of serialized r/A node at address dv
 * returns 0 on success, or -1 if the tpl isn't trustworthy (fails consistency)
 */
static int
tpl_serlen (tpl_node * r, tpl_node * n, void *dv, size_t *serlen) {
  uint32_t slen;
  int num = 0, fidx;
  tpl_node *c;
  size_t len = 0, alen, buf_past, itermax;
  tpl_pound_data *pd;

  buf_past = ((uintptr_t) ((tpl_root_data *) (r->data))->mmap.text +
              ((tpl_root_data *) (r->data))->mmap.text_sz);

  if (n->type == TPL_TYPE_ROOT)
    num = 1;
  else if (n->type == TPL_TYPE_ARY) {
    if ((uintptr_t) dv + sizeof (uint32_t) > buf_past)
      return -1;
    memcpy (&num, dv, sizeof (uint32_t));
    if (((tpl_root_data *) (r->data))->flags & TPL_XENDIAN)
      tpl_byteswap (&num, sizeof (uint32_t));
    dv = (void *) ((uintptr_t) dv + sizeof (uint32_t));
    len += sizeof (uint32_t);
  } else
    tpl_hook.fatal ("internal error in tpl_serlen\n");

  while (num-- > 0) {
    c = n->children;
    while (c) {
      switch (c->type) {
      case TPL_TYPE_BYTE:
      case TPL_TYPE_DOUBLE:
      case TPL_TYPE_INT32:
      case TPL_TYPE_UINT32:
      case TPL_TYPE_INT64:
      case TPL_TYPE_UINT64:
      case TPL_TYPE_INT16:
      case TPL_TYPE_UINT16:
        for (fidx = 0; fidx < c->num; fidx++) { /* octothorpe support */
          if ((uintptr_t) dv + tpl_types[c->type].sz > buf_past)
            return -1;
          dv = (void *) ((uintptr_t) dv + tpl_types[c->type].sz);
          len += tpl_types[c->type].sz;
        }
        break;
      case TPL_TYPE_BIN:
        len += sizeof (uint32_t);
        if ((uintptr_t) dv + sizeof (uint32_t) > buf_past)
          return -1;
        memcpy (&slen, dv, sizeof (uint32_t));
        if (((tpl_root_data *) (r->data))->flags & TPL_XENDIAN)
          tpl_byteswap (&slen, sizeof (uint32_t));
        len += slen;
        dv = (void *) ((uintptr_t) dv + sizeof (uint32_t));
        if ((uintptr_t) dv + slen > buf_past)
          return -1;
        dv = (void *) ((uintptr_t) dv + slen);
        break;
      case TPL_TYPE_STR:
        for (fidx = 0; fidx < c->num; fidx++) { /* octothorpe support */
          len += sizeof (uint32_t);
          if ((uintptr_t) dv + sizeof (uint32_t) > buf_past)
            return -1;
          memcpy (&slen, dv, sizeof (uint32_t));
          if (((tpl_root_data *) (r->data))->flags & TPL_XENDIAN)
            tpl_byteswap (&slen, sizeof (uint32_t));
          if (!(((tpl_root_data *) (r->data))->flags & TPL_OLD_STRING_FMT))
            slen = (slen > 1) ? (slen - 1) : 0;
          len += slen;
          dv = (void *) ((uintptr_t) dv + sizeof (uint32_t));
          if ((uintptr_t) dv + slen > buf_past)
            return -1;
          dv = (void *) ((uintptr_t) dv + slen);
        }
        break;
      case TPL_TYPE_ARY:
        if (tpl_serlen (r, c, dv, &alen) == -1)
          return -1;
        dv = (void *) ((uintptr_t) dv + alen);
        len += alen;
        break;
      case TPL_TYPE_POUND:
        /* iterate over the preceding nodes */
        itermax = c->num;
        pd = (tpl_pound_data *) c->data;
        if (++(pd->iternum) < itermax) {
          c = pd->iter_start_node;
          continue;
        } else {        /* loop complete. */
          pd->iternum = 0;
        }
        break;
      default:
        tpl_hook.fatal ("unsupported format character\n");
        break;
      }
      c = c->next;
    }
  }
  *serlen = len;
  return 0;
}

static int
tpl_mmap_output_file (char *filename, size_t sz, void **text_out) {
  void *text;
  int fd, perms;

#ifndef _WIN32
  perms = S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP | S_IROTH;      /* ug+w o+r */
  fd = open (filename, O_CREAT | O_TRUNC | O_RDWR, perms);
#else
  perms = _S_IWRITE;
  fd = _open (filename, _O_CREAT | _O_TRUNC | _O_RDWR, perms);
#endif

  if (fd == -1) {
    tpl_hook.oops ("Couldn't open file %s: %s\n", filename, strerror (errno));
    return -1;
  }

  text = mmap (0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (text == MAP_FAILED) {
    tpl_hook.oops ("Failed to mmap %s: %s\n", filename, strerror (errno));
    close (fd);
    return -1;
  }
  if (ftruncate (fd, sz) == -1) {
    tpl_hook.oops ("ftruncate failed: %s\n", strerror (errno));
    munmap (text, sz);
    close (fd);
    return -1;
  }
  *text_out = text;
  return fd;
}

static int
tpl_mmap_file (char *filename, tpl_mmap_rec * mr) {
  struct stat stat_buf;

  if ((mr->fd = open (filename, O_RDONLY)) == -1) {
    tpl_hook.oops ("Couldn't open file %s: %s\n", filename, strerror (errno));
    return -1;
  }

  if (fstat (mr->fd, &stat_buf) == -1) {
    close (mr->fd);
    tpl_hook.oops ("Couldn't stat file %s: %s\n", filename, strerror (errno));
    return -1;
  }

  mr->text_sz = (size_t) stat_buf.st_size;
  mr->text = mmap (0, stat_buf.st_size, PROT_READ, MAP_PRIVATE, mr->fd, 0);
  if (mr->text == MAP_FAILED) {
    close (mr->fd);
    tpl_hook.oops ("Failed to mmap %s: %s\n", filename, strerror (errno));
    return -1;
  }

  return 0;
}

TPL_API int
tpl_pack (tpl_node * r, int i) {
  tpl_node *n, *child, *np;
  void *datav = NULL;
  size_t sz, itermax;
  uint32_t slen;
  char *str;
  tpl_bin *bin;
  tpl_pound_data *pd;
  int fidx;

  n = tpl_find_i (r, i);
  if (n == NULL) {
    tpl_hook.oops ("invalid index %d to tpl_pack\n", i);
    return -1;
  }

  if (((tpl_root_data *) (r->data))->flags & TPL_RDONLY) {
    /* convert to an writeable tpl, initially empty */
    tpl_free_keep_map (r);
  }

  ((tpl_root_data *) (r->data))->flags |= TPL_WRONLY;

  if (n->type == TPL_TYPE_ARY)
    datav = tpl_extend_backbone (n);
  child = n->children;
  while (child) {
    switch (child->type) {
    case TPL_TYPE_BYTE:
    case TPL_TYPE_DOUBLE:
    case TPL_TYPE_INT32:
    case TPL_TYPE_UINT32:
    case TPL_TYPE_INT64:
    case TPL_TYPE_UINT64:
    case TPL_TYPE_INT16:
    case TPL_TYPE_UINT16:
      /* no need to use fidx iteration here; we can copy multiple values in one memcpy */
      memcpy (child->data, child->addr, tpl_types[child->type].sz * child->num);
      if (datav)
        datav = tpl_cpv (datav, child->data, tpl_types[child->type].sz * child->num);
      if (n->type == TPL_TYPE_ARY)
        n->ser_osz += tpl_types[child->type].sz * child->num;
      break;
    case TPL_TYPE_BIN:
      /* copy the buffer to be packed */
      slen = ((tpl_bin *) child->addr)->sz;
      if (slen > 0) {
        str = tpl_hook.malloc (slen);
        if (!str)
          fatal_oom ();
        memcpy (str, ((tpl_bin *) child->addr)->addr, slen);
      } else
        str = NULL;
      /* and make a tpl_bin to point to it */
      bin = tpl_hook.malloc (sizeof (tpl_bin));
      if (!bin)
        fatal_oom ();
      bin->addr = str;
      bin->sz = slen;
      /* now pack its pointer, first deep freeing any pre-existing bin */
      if (*(tpl_bin **) (child->data) != NULL) {
        if ((*(tpl_bin **) (child->data))->sz != 0) {
          tpl_hook.free ((*(tpl_bin **) (child->data))->addr);
        }
        tpl_hook.free (*(tpl_bin **) (child->data));
      }
      memcpy (child->data, &bin, sizeof (tpl_bin *));
      if (datav) {
        datav = tpl_cpv (datav, &bin, sizeof (tpl_bin *));
        *(tpl_bin **) (child->data) = NULL;
      }
      if (n->type == TPL_TYPE_ARY) {
        n->ser_osz += sizeof (uint32_t);        /* binary buf len word */
        n->ser_osz += bin->sz;  /* binary buf */
      }
      break;
    case TPL_TYPE_STR:
      for (fidx = 0; fidx < child->num; fidx++) {
        /* copy the string to be packed. slen includes \0. this
           block also works if the string pointer is NULL. */
        char *caddr = ((char **) child->addr)[fidx];
        char **cdata = &((char **) child->data)[fidx];
        slen = caddr ? (strlen (caddr) + 1) : 0;
        if (slen) {
          str = tpl_hook.malloc (slen);
          if (!str)
            fatal_oom ();
          memcpy (str, caddr, slen);    /* include \0 */
        } else {
          str = NULL;
        }
        /* now pack its pointer, first freeing any pre-existing string */
        if (*cdata != NULL) {
          tpl_hook.free (*cdata);
        }
        memcpy (cdata, &str, sizeof (char *));
        if (datav) {
          datav = tpl_cpv (datav, &str, sizeof (char *));
          *cdata = NULL;
        }
        if (n->type == TPL_TYPE_ARY) {
          n->ser_osz += sizeof (uint32_t);      /* string len word */
          if (slen > 1)
            n->ser_osz += slen - 1;     /* string (without nul) */
        }
      }
      break;
    case TPL_TYPE_ARY:
      /* copy the child's tpl_atype* and reset it to empty */
      if (datav) {
        sz = ((tpl_atyp *) (child->data))->sz;
        datav = tpl_cpv (datav, &child->data, sizeof (void *));
        child->data = tpl_hook.malloc (sizeof (tpl_atyp));
        if (!child->data)
          fatal_oom ();
        ((tpl_atyp *) (child->data))->num = 0;
        ((tpl_atyp *) (child->data))->sz = sz;
        ((tpl_atyp *) (child->data))->bb = NULL;
        ((tpl_atyp *) (child->data))->bbtail = NULL;
      }
      /* parent is array? then bubble up child array's ser_osz */
      if (n->type == TPL_TYPE_ARY) {
        n->ser_osz += sizeof (uint32_t);        /* array len word */
        n->ser_osz += child->ser_osz;   /* child array ser_osz */
        child->ser_osz = 0;     /* reset child array ser_osz */
      }
      break;

    case TPL_TYPE_POUND:
      /* we need to iterate n times over preceding nodes in S(...).
       * we may be in the midst of an iteration each time or starting. */
      pd = (tpl_pound_data *) child->data;
      itermax = child->num;

      /* itermax is total num of iterations needed  */
      /* pd->iternum is current iteration index  */
      /* pd->inter_elt_len is element-to-element len of contiguous structs */
      /* pd->iter_start_node is where we jump to at each iteration. */

      if (++(pd->iternum) < itermax) {

        /* in start or midst of loop. advance addr/data pointers. */
        for (np = pd->iter_start_node; np != child; np = np->next) {
          np->data = (char *) (np->data) + (tpl_types[np->type].sz * np->num);
          np->addr = (char *) (np->addr) + pd->inter_elt_len;
        }
        /* do next iteration */
        child = pd->iter_start_node;
        continue;

      } else {  /* loop complete. */

        /* reset iteration index and addr/data pointers. */
        pd->iternum = 0;
        for (np = pd->iter_start_node; np != child; np = np->next) {
          np->data = (char *) (np->data) - ((itermax - 1) * tpl_types[np->type].sz * np->num);
          np->addr = (char *) (np->addr) - ((itermax - 1) * pd->inter_elt_len);
        }

      }
      break;
    default:
      tpl_hook.fatal ("unsupported format character\n");
      break;
    }
    child = child->next;
  }
  return 0;
}

TPL_API int
tpl_unpack (tpl_node * r, int i) {
  tpl_node *n, *c, *np;
  uint32_t slen;
  int rc = 1, fidx;
  char *str;
  void *dv = NULL, *caddr;
  size_t A_bytes, itermax;
  tpl_pound_data *pd;
  void *img;
  size_t sz;


  /* handle unusual case of tpl_pack,tpl_unpack without an
   * intervening tpl_dump. do a dump/load implicitly. */
  if (((tpl_root_data *) (r->data))->flags & TPL_WRONLY) {
    if (tpl_dump (r, TPL_MEM, &img, &sz) != 0)
      return -1;
    if (tpl_load (r, TPL_MEM | TPL_UFREE, img, sz) != 0) {
      tpl_hook.free (img);
      return -1;
    };
  }

  n = tpl_find_i (r, i);
  if (n == NULL) {
    tpl_hook.oops ("invalid index %d to tpl_unpack\n", i);
    return -1;
  }

  /* either root node or an A node */
  if (n->type == TPL_TYPE_ROOT) {
    dv = tpl_find_data_start (((tpl_root_data *) (n->data))->mmap.text);
  } else if (n->type == TPL_TYPE_ARY) {
    if (((tpl_atyp *) (n->data))->num <= 0)
      return 0; /* array consumed */
    else
      rc = ((tpl_atyp *) (n->data))->num--;
    dv = ((tpl_atyp *) (n->data))->cur;
    if (!dv)
      tpl_hook.fatal ("must unpack parent of node before node itself\n");
  }

  c = n->children;
  while (c) {
    switch (c->type) {
    case TPL_TYPE_BYTE:
    case TPL_TYPE_DOUBLE:
    case TPL_TYPE_INT32:
    case TPL_TYPE_UINT32:
    case TPL_TYPE_INT64:
    case TPL_TYPE_UINT64:
    case TPL_TYPE_INT16:
    case TPL_TYPE_UINT16:
      /* unpack elements of cross-endian octothorpic array individually */
      if (((tpl_root_data *) (r->data))->flags & TPL_XENDIAN) {
        for (fidx = 0; fidx < c->num; fidx++) {
          caddr = (void *) ((uintptr_t) c->addr + (fidx * tpl_types[c->type].sz));
          memcpy (caddr, dv, tpl_types[c->type].sz);
          tpl_byteswap (caddr, tpl_types[c->type].sz);
          dv = (void *) ((uintptr_t) dv + tpl_types[c->type].sz);
        }
      } else {
        /* bulk unpack ok if not cross-endian */
        memcpy (c->addr, dv, tpl_types[c->type].sz * c->num);
        dv = (void *) ((uintptr_t) dv + tpl_types[c->type].sz * c->num);
      }
      break;
    case TPL_TYPE_BIN:
      memcpy (&slen, dv, sizeof (uint32_t));
      if (((tpl_root_data *) (r->data))->flags & TPL_XENDIAN)
        tpl_byteswap (&slen, sizeof (uint32_t));
      if (slen > 0) {
        str = (char *) tpl_hook.malloc (slen);
        if (!str)
          fatal_oom ();
      } else
        str = NULL;
      dv = (void *) ((uintptr_t) dv + sizeof (uint32_t));
      if (slen > 0)
        memcpy (str, dv, slen);
      memcpy (&(((tpl_bin *) c->addr)->addr), &str, sizeof (void *));
      memcpy (&(((tpl_bin *) c->addr)->sz), &slen, sizeof (uint32_t));
      dv = (void *) ((uintptr_t) dv + slen);
      break;
    case TPL_TYPE_STR:
      for (fidx = 0; fidx < c->num; fidx++) {
        memcpy (&slen, dv, sizeof (uint32_t));
        if (((tpl_root_data *) (r->data))->flags & TPL_XENDIAN)
          tpl_byteswap (&slen, sizeof (uint32_t));
        if (((tpl_root_data *) (r->data))->flags & TPL_OLD_STRING_FMT)
          slen += 1;
        dv = (void *) ((uintptr_t) dv + sizeof (uint32_t));
        if (slen) {     /* slen includes \0 */
          str = (char *) tpl_hook.malloc (slen);
          if (!str)
            fatal_oom ();
          if (slen > 1)
            memcpy (str, dv, slen - 1);
          str[slen - 1] = '\0'; /* nul terminate */
          dv = (void *) ((uintptr_t) dv + slen - 1);
        } else
          str = NULL;
        memcpy (&((char **) c->addr)[fidx], &str, sizeof (char *));
      }
      break;
    case TPL_TYPE_POUND:
      /* iterate over preceding nodes */
      pd = (tpl_pound_data *) c->data;
      itermax = c->num;
      if (++(pd->iternum) < itermax) {
        /* in start or midst of loop. advance addr/data pointers. */
        for (np = pd->iter_start_node; np != c; np = np->next) {
          np->addr = (char *) (np->addr) + pd->inter_elt_len;
        }
        /* do next iteration */
        c = pd->iter_start_node;
        continue;

      } else {  /* loop complete. */

        /* reset iteration index and addr/data pointers. */
        pd->iternum = 0;
        for (np = pd->iter_start_node; np != c; np = np->next) {
          np->addr = (char *) (np->addr) - ((itermax - 1) * pd->inter_elt_len);
        }

      }
      break;
    case TPL_TYPE_ARY:
      if (tpl_serlen (r, c, dv, &A_bytes) == -1)
        tpl_hook.fatal ("internal error in unpack\n");
      memcpy (&((tpl_atyp *) (c->data))->num, dv, sizeof (uint32_t));
      if (((tpl_root_data *) (r->data))->flags & TPL_XENDIAN)
        tpl_byteswap (&((tpl_atyp *) (c->data))->num, sizeof (uint32_t));
      ((tpl_atyp *) (c->data))->cur = (void *) ((uintptr_t) dv + sizeof (uint32_t));
      dv = (void *) ((uintptr_t) dv + A_bytes);
      break;
    default:
      tpl_hook.fatal ("unsupported format character\n");
      break;
    }

    c = c->next;
  }
  if (n->type == TPL_TYPE_ARY)
    ((tpl_atyp *) (n->data))->cur = dv; /* next element */
  return rc;
}

/* Specialized function that unpacks only the root's A nodes, after tpl_load  */
static int
tpl_unpackA0 (tpl_node * r) {
  tpl_node *n, *c;
  uint32_t slen;
  int rc = 1, fidx, i;
  void *dv;
  size_t A_bytes, itermax;
  tpl_pound_data *pd;

  n = r;
  dv = tpl_find_data_start (((tpl_root_data *) (r->data))->mmap.text);

  c = n->children;
  while (c) {
    switch (c->type) {
    case TPL_TYPE_BYTE:
    case TPL_TYPE_DOUBLE:
    case TPL_TYPE_INT32:
    case TPL_TYPE_UINT32:
    case TPL_TYPE_INT64:
    case TPL_TYPE_UINT64:
    case TPL_TYPE_INT16:
    case TPL_TYPE_UINT16:
      for (fidx = 0; fidx < c->num; fidx++) {
        dv = (void *) ((uintptr_t) dv + tpl_types[c->type].sz);
      }
      break;
    case TPL_TYPE_BIN:
      memcpy (&slen, dv, sizeof (uint32_t));
      if (((tpl_root_data *) (r->data))->flags & TPL_XENDIAN)
        tpl_byteswap (&slen, sizeof (uint32_t));
      dv = (void *) ((uintptr_t) dv + sizeof (uint32_t));
      dv = (void *) ((uintptr_t) dv + slen);
      break;
    case TPL_TYPE_STR:
      for (i = 0; i < c->num; i++) {
        memcpy (&slen, dv, sizeof (uint32_t));
        if (((tpl_root_data *) (r->data))->flags & TPL_XENDIAN)
          tpl_byteswap (&slen, sizeof (uint32_t));
        if (((tpl_root_data *) (r->data))->flags & TPL_OLD_STRING_FMT)
          slen += 1;
        dv = (void *) ((uintptr_t) dv + sizeof (uint32_t));
        if (slen > 1)
          dv = (void *) ((uintptr_t) dv + slen - 1);
      }
      break;
    case TPL_TYPE_POUND:
      /* iterate over the preceding nodes */
      itermax = c->num;
      pd = (tpl_pound_data *) c->data;
      if (++(pd->iternum) < itermax) {
        c = pd->iter_start_node;
        continue;
      } else {  /* loop complete. */
        pd->iternum = 0;
      }
      break;
    case TPL_TYPE_ARY:
      if (tpl_serlen (r, c, dv, &A_bytes) == -1)
        tpl_hook.fatal ("internal error in unpackA0\n");
      memcpy (&((tpl_atyp *) (c->data))->num, dv, sizeof (uint32_t));
      if (((tpl_root_data *) (r->data))->flags & TPL_XENDIAN)
        tpl_byteswap (&((tpl_atyp *) (c->data))->num, sizeof (uint32_t));
      ((tpl_atyp *) (c->data))->cur = (void *) ((uintptr_t) dv + sizeof (uint32_t));
      dv = (void *) ((uintptr_t) dv + A_bytes);
      break;
    default:
      tpl_hook.fatal ("unsupported format character\n");
      break;
    }
    c = c->next;
  }
  return rc;
}

/* In-place byte order swapping of a word of length "len" bytes */
static void
tpl_byteswap (void *word, int len) {
  int i;
  char c, *w;
  w = (char *) word;
  for (i = 0; i < len / 2; i++) {
    c = w[i];
    w[i] = w[len - 1 - i];
    w[len - 1 - i] = c;
  }
}

static void
tpl_fatal (const char *fmt, ...) {
  va_list ap;
  char exit_msg[100];

  va_start (ap, fmt);
  vsnprintf (exit_msg, 100, fmt, ap);
  va_end (ap);

  tpl_hook.oops ("%s", exit_msg);
  exit (-1);
}

TPL_API int
tpl_gather (int mode, ...) {
  va_list ap;
  int fd, rc = 0;
  size_t *szp, sz;
  void **img, *addr, *data;
  tpl_gather_t **gs;
  tpl_gather_cb *cb;

  va_start (ap, mode);
  switch (mode) {
  case TPL_GATHER_BLOCKING:
    fd = va_arg (ap, int);
    img = va_arg (ap, void *);
    szp = va_arg (ap, size_t *);
    rc = tpl_gather_blocking (fd, img, szp);
    break;
  case TPL_GATHER_NONBLOCKING:
    fd = va_arg (ap, int);
    gs = (tpl_gather_t **) va_arg (ap, void *);
    cb = (tpl_gather_cb *) va_arg (ap, tpl_gather_cb *);
    data = va_arg (ap, void *);
    rc = tpl_gather_nonblocking (fd, gs, cb, data);
    break;
  case TPL_GATHER_MEM:
    addr = va_arg (ap, void *);
    sz = va_arg (ap, size_t);
    gs = (tpl_gather_t **) va_arg (ap, void *);
    cb = (tpl_gather_cb *) va_arg (ap, tpl_gather_cb *);
    data = va_arg (ap, void *);
    rc = tpl_gather_mem (addr, sz, gs, cb, data);
    break;
  default:
    tpl_hook.fatal ("unsupported tpl_gather mode %d\n", mode);
    break;
  }
  va_end (ap);
  return rc;
}

/* dequeue a tpl by reading until one full tpl image is obtained.
 * We take care not to read past the end of the tpl.
 * This is intended as a blocking call i.e. for use with a blocking fd.
 * It can be given a non-blocking fd, but the read spins if we have to wait.
 */
static int
tpl_gather_blocking (int fd, void **img, size_t *sz) {
  char preamble[8];
  int rc;
  uint32_t i = 0, tpllen;

  do {
    rc = read (fd, &preamble[i], 8 - i);
    i += (rc > 0) ? rc : 0;
  } while ((rc == -1 && (errno == EINTR || errno == EAGAIN)) || (rc > 0 && i < 8));

  if (rc < 0) {
    tpl_hook.oops ("tpl_gather_fd_blocking failed: %s\n", strerror (errno));
    return -1;
  } else if (rc == 0) {
    /* tpl_hook.oops("tpl_gather_fd_blocking: eof\n"); */
    return 0;
  } else if (i != 8) {
    tpl_hook.oops ("internal error\n");
    return -1;
  }

  if (preamble[0] == 't' && preamble[1] == 'p' && preamble[2] == 'l') {
    memcpy (&tpllen, &preamble[4], 4);
    if (tpl_needs_endian_swap (preamble))
      tpl_byteswap (&tpllen, 4);
  } else {
    tpl_hook.oops ("tpl_gather_fd_blocking: non-tpl input\n");
    return -1;
  }

  /* malloc space for remainder of tpl image (overall length tpllen)
   * and read it in
   */
  if (tpl_hook.gather_max > 0 && tpllen > tpl_hook.gather_max) {
    tpl_hook.oops ("tpl exceeds max length %zu\n", tpl_hook.gather_max);
    return -2;
  }
  *sz = tpllen;
  if ((*img = tpl_hook.malloc (tpllen)) == NULL) {
    fatal_oom ();
  }

  memcpy (*img, preamble, 8);   /* copy preamble to output buffer */
  i = 8;
  do {
    rc = read (fd, &((*(char **) img)[i]), tpllen - i);
    i += (rc > 0) ? rc : 0;
  } while ((rc == -1 && (errno == EINTR || errno == EAGAIN)) || (rc > 0 && i < tpllen));

  if (rc < 0) {
    tpl_hook.oops ("tpl_gather_fd_blocking failed: %s\n", strerror (errno));
    tpl_hook.free (*img);
    return -1;
  } else if (rc == 0) {
    /* tpl_hook.oops("tpl_gather_fd_blocking: eof\n"); */
    tpl_hook.free (*img);
    return 0;
  } else if (i != tpllen) {
    tpl_hook.oops ("internal error\n");
    tpl_hook.free (*img);
    return -1;
  }

  return 1;
}

/* Used by select()-driven apps which want to gather tpl images piecemeal */
/* the file descriptor must be non-blocking for this functino to work. */
static int
tpl_gather_nonblocking (int fd, tpl_gather_t ** gs, tpl_gather_cb * cb, void *data) {
  char buf[TPL_GATHER_BUFLEN], *img, *tpl;
  int rc, keep_looping, cbrc = 0;
  size_t catlen;
  uint32_t tpllen;

  while (1) {
    rc = read (fd, buf, TPL_GATHER_BUFLEN);
    if (rc == -1) {
      if (errno == EINTR)
        continue;       /* got signal during read, ignore */
      if (errno == EAGAIN)
        return 1;       /* nothing to read right now */
      else {
        tpl_hook.oops ("tpl_gather failed: %s\n", strerror (errno));
        if (*gs) {
          tpl_hook.free ((*gs)->img);
          tpl_hook.free (*gs);
          *gs = NULL;
        }
        return -1;      /* error, caller should close fd  */
      }
    } else if (rc == 0) {
      if (*gs) {
        tpl_hook.oops ("tpl_gather: partial tpl image precedes EOF\n");
        tpl_hook.free ((*gs)->img);
        tpl_hook.free (*gs);
        *gs = NULL;
      }
      return 0; /* EOF, caller should close fd */
    } else {
      /* concatenate any partial tpl from last read with new buffer */
      if (*gs) {
        catlen = (*gs)->len + rc;
        if (tpl_hook.gather_max > 0 && catlen > tpl_hook.gather_max) {
          tpl_hook.free ((*gs)->img);
          tpl_hook.free ((*gs));
          *gs = NULL;
          tpl_hook.oops ("tpl exceeds max length %zu\n", tpl_hook.gather_max);
          return -2;    /* error, caller should close fd */
        }
        if ((img = tpl_hook.realloc ((*gs)->img, catlen)) == NULL) {
          fatal_oom ();
        }
        memcpy (img + (*gs)->len, buf, rc);
        tpl_hook.free (*gs);
        *gs = NULL;
      } else {
        img = buf;
        catlen = rc;
      }
      /* isolate any full tpl(s) in img and invoke cb for each */
      tpl = img;
      keep_looping = (tpl + 8 < img + catlen) ? 1 : 0;
      while (keep_looping) {
        if (strncmp ("tpl", tpl, 3) != 0) {
          tpl_hook.oops ("tpl prefix invalid\n");
          if (img != buf)
            tpl_hook.free (img);
          tpl_hook.free (*gs);
          *gs = NULL;
          return -3;    /* error, caller should close fd */
        }
        memcpy (&tpllen, &tpl[4], 4);
        if (tpl_needs_endian_swap (tpl))
          tpl_byteswap (&tpllen, 4);
        if (tpl + tpllen <= img + catlen) {
          cbrc = (cb) (tpl, tpllen, data);      /* invoke cb for tpl image */
          tpl += tpllen;        /* point to next tpl image */
          if (cbrc < 0)
            keep_looping = 0;
          else
            keep_looping = (tpl + 8 < img + catlen) ? 1 : 0;
        } else
          keep_looping = 0;
      }
      /* check if app callback requested closure of tpl source */
      if (cbrc < 0) {
        tpl_hook.oops ("tpl_fd_gather aborted by app callback\n");
        if (img != buf)
          tpl_hook.free (img);
        if (*gs)
          tpl_hook.free (*gs);
        *gs = NULL;
        return -4;
      }
      /* store any leftover, partial tpl fragment for next read */
      if (tpl == img && img != buf) {
        /* consumed nothing from img!=buf */
        if ((*gs = tpl_hook.malloc (sizeof (tpl_gather_t))) == NULL) {
          fatal_oom ();
        }
        (*gs)->img = tpl;
        (*gs)->len = catlen;
      } else if (tpl < img + catlen) {
        /* consumed 1+ tpl(s) from img!=buf or 0 from img==buf */
        if ((*gs = tpl_hook.malloc (sizeof (tpl_gather_t))) == NULL) {
          fatal_oom ();
        }
        if (((*gs)->img = tpl_hook.malloc (img + catlen - tpl)) == NULL) {
          fatal_oom ();
        }
        (*gs)->len = img + catlen - tpl;
        memcpy ((*gs)->img, tpl, img + catlen - tpl);
        /* free partially consumed concat buffer if used */
        if (img != buf)
          tpl_hook.free (img);
      } else {  /* tpl(s) fully consumed */
        /* free consumed concat buffer if used */
        if (img != buf)
          tpl_hook.free (img);
      }
    }
  }
}

/* gather tpl piecemeal from memory buffer (not fd) e.g., from a lower-level api */
static int
tpl_gather_mem (char *buf, size_t len, tpl_gather_t ** gs, tpl_gather_cb * cb, void *data) {
  char *img, *tpl;
  int keep_looping, cbrc = 0;
  size_t catlen;
  uint32_t tpllen;

  /* concatenate any partial tpl from last read with new buffer */
  if (*gs) {
    catlen = (*gs)->len + len;
    if (tpl_hook.gather_max > 0 && catlen > tpl_hook.gather_max) {
      tpl_hook.free ((*gs)->img);
      tpl_hook.free ((*gs));
      *gs = NULL;
      tpl_hook.oops ("tpl exceeds max length %zu\n", tpl_hook.gather_max);
      return -2;        /* error, caller should stop accepting input from source */
    }
    if ((img = tpl_hook.realloc ((*gs)->img, catlen)) == NULL) {
      fatal_oom ();
    }
    memcpy (img + (*gs)->len, buf, len);
    tpl_hook.free (*gs);
    *gs = NULL;
  } else {
    img = buf;
    catlen = len;
  }
  /* isolate any full tpl(s) in img and invoke cb for each */
  tpl = img;
  keep_looping = (tpl + 8 < img + catlen) ? 1 : 0;
  while (keep_looping) {
    if (strncmp ("tpl", tpl, 3) != 0) {
      tpl_hook.oops ("tpl prefix invalid\n");
      if (img != buf)
        tpl_hook.free (img);
      tpl_hook.free (*gs);
      *gs = NULL;
      return -3;        /* error, caller should stop accepting input from source */
    }
    memcpy (&tpllen, &tpl[4], 4);
    if (tpl_needs_endian_swap (tpl))
      tpl_byteswap (&tpllen, 4);
    if (tpl + tpllen <= img + catlen) {
      cbrc = (cb) (tpl, tpllen, data);  /* invoke cb for tpl image */
      tpl += tpllen;    /* point to next tpl image */
      if (cbrc < 0)
        keep_looping = 0;
      else
        keep_looping = (tpl + 8 < img + catlen) ? 1 : 0;
    } else
      keep_looping = 0;
  }
  /* check if app callback requested closure of tpl source */
  if (cbrc < 0) {
    tpl_hook.oops ("tpl_mem_gather aborted by app callback\n");
    if (img != buf)
      tpl_hook.free (img);
    if (*gs)
      tpl_hook.free (*gs);
    *gs = NULL;
    return -4;
  }
  /* store any leftover, partial tpl fragment for next read */
  if (tpl == img && img != buf) {
    /* consumed nothing from img!=buf */
    if ((*gs = tpl_hook.malloc (sizeof (tpl_gather_t))) == NULL) {
      fatal_oom ();
    }
    (*gs)->img = tpl;
    (*gs)->len = catlen;
  } else if (tpl < img + catlen) {
    /* consumed 1+ tpl(s) from img!=buf or 0 from img==buf */
    if ((*gs = tpl_hook.malloc (sizeof (tpl_gather_t))) == NULL) {
      fatal_oom ();
    }
    if (((*gs)->img = tpl_hook.malloc (img + catlen - tpl)) == NULL) {
      fatal_oom ();
    }
    (*gs)->len = img + catlen - tpl;
    memcpy ((*gs)->img, tpl, img + catlen - tpl);
    /* free partially consumed concat buffer if used */
    if (img != buf)
      tpl_hook.free (img);
  } else {      /* tpl(s) fully consumed */
    /* free consumed concat buffer if used */
    if (img != buf)
      tpl_hook.free (img);
  }
  return 1;
}
