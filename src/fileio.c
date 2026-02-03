/**
 * fileio.c -- gFile I/O abstraction layer for goaccess
 * This provides a unified interface for reading both regular and gzipped files
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "fileio.h"

#ifdef HAVE_ZLIB
/* Check if a file is gzipped by examining its magic bytes */
static int
is_gzipped_file (const char *filename) {
  FILE *fp;
  unsigned char magic[2];
  int result = 0;

  if ((fp = fopen (filename, "rb")) == NULL)
    return 0;

  if (fread (magic, 1, 2, fp) == 2) {
    /* gzip magic number is 0x1f 0x8b */
    if (magic[0] == 0x1f && magic[1] == 0x8b)
      result = 1;
  }

  fclose (fp);
  return result;
}
#endif

/* Open a file for reading, automatically detecting gzip compression */
GFileHandle *
gfile_open (const char *filename, const char *mode) {
  GFileHandle *fh;

  if ((fh = calloc (1, sizeof (GFileHandle))) == NULL)
    return NULL;

#ifdef HAVE_ZLIB
  /* Check if file is gzipped */
  if (is_gzipped_file (filename)) {
    fh->is_gzipped = 1;
    fh->gzfp = gzopen (filename, mode);
    if (fh->gzfp == NULL) {
      free (fh);
      return NULL;
    }
    fh->fp = NULL;
    return fh;
  }
#endif

  /* Open as regular file */
#ifdef HAVE_ZLIB
  fh->is_gzipped = 0;
  fh->gzfp = NULL;
#endif
  fh->fp = fopen (filename, mode);
  if (fh->fp == NULL) {
    free (fh);
    return NULL;
  }

  return fh;
}

/* Close a file handle */
void
gfile_close (GFileHandle *fh) {
  if (!fh)
    return;

#ifdef HAVE_ZLIB
  if (fh->is_gzipped && fh->gzfp) {
    gzclose (fh->gzfp);
  } else
#endif
  if (fh->fp) {
    fclose (fh->fp);
  }

  free (fh);
}

/* Read a line from the file */
char *
gfile_gets (char *buf, int size, GFileHandle *fh) {
  if (!fh || !buf || size <= 0)
    return NULL;

#ifdef HAVE_ZLIB
  if (fh->is_gzipped && fh->gzfp) {
    return gzgets (fh->gzfp, buf, size);
  }
#endif

  if (fh->fp)
    return fgets (buf, size, fh->fp);

  return NULL;
}

/* Check if end of file is reached */
int
gfile_eof (GFileHandle *fh) {
  if (!fh)
    return 1;

#ifdef HAVE_ZLIB
  if (fh->is_gzipped && fh->gzfp) {
    return gzeof (fh->gzfp);
  }
#endif

  if (fh->fp)
    return feof (fh->fp);

  return 1;
}

/* Read data from file */
int
gfile_read (void *buf, size_t size, size_t count, GFileHandle *fh) {
  if (!fh || !buf)
    return 0;

#ifdef HAVE_ZLIB
  if (fh->is_gzipped && fh->gzfp) {
    int bytes_to_read = size * count;
    int bytes_read = gzread (fh->gzfp, buf, bytes_to_read);
    if (bytes_read < 0)
      return 0;
    return bytes_read / size;
  }
#endif

  if (fh->fp)
    return fread (buf, size, count, fh->fp);

  return 0;
}

/* Seek to a position in the file */
int
gfile_seek (GFileHandle *fh, long offset, int whence) {
  if (!fh)
    return -1;

#ifdef HAVE_ZLIB
  if (fh->is_gzipped && fh->gzfp) {
    /* gzseek returns the current offset, or -1 on error */
    z_off_t result = gzseek (fh->gzfp, offset, whence);
    return (result == -1) ? -1 : 0;
  }
#endif

  if (fh->fp)
    return fseek (fh->fp, offset, whence);

  return -1;
}

/* Get current position in file */
long
gfile_tell (GFileHandle *fh) {
  if (!fh)
    return -1;

#ifdef HAVE_ZLIB
  if (fh->is_gzipped && fh->gzfp) {
    return (long) gztell (fh->gzfp);
  }
#endif

  if (fh->fp)
    return ftell (fh->fp);

  return -1;
}

/* Check for file errors */
int
gfile_error (GFileHandle *fh) {
  if (!fh)
    return 1;

#ifdef HAVE_ZLIB
  if (fh->is_gzipped && fh->gzfp) {
    int errnum;
    gzerror (fh->gzfp, &errnum);
    return errnum != Z_OK && errnum != Z_STREAM_END;
  }
#endif

  if (fh->fp)
    return ferror (fh->fp);

  return 1;
}
