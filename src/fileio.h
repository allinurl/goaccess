/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2025 Gerardo Orellana <hello @ goaccess.io>
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

#ifndef FILEIO_H_INCLUDED
#define FILEIO_H_INCLUDED

#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

/* File handle abstraction */
typedef struct GFileHandle_ {
#ifdef HAVE_ZLIB
  gzFile gzfp;
  int is_gzipped;
#endif
  FILE *fp;
} GFileHandle;

/* Function prototypes */
GFileHandle *gfile_open (const char *filename, const char *mode);
void gfile_close (GFileHandle * fh);
char *gfile_gets (char *buf, int size, GFileHandle * fh);
int gfile_eof (GFileHandle * fh);
int gfile_read (void *buf, size_t size, size_t count, GFileHandle * fh);
int gfile_seek (GFileHandle * fh, long offset, int whence);
long gfile_tell (GFileHandle * fh);
int gfile_error (GFileHandle * fh);

#endif /* FILEIO_H_INCLUDED */
