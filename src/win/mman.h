#ifndef _MMAN_H_
#define _MMAN_H_

/* Protections */
#define PROT_NONE       0x00    /* no permissions */
#define PROT_READ       0x01    /* pages can be read */
#define PROT_WRITE      0x02    /* pages can be written */
#define PROT_EXEC       0x04    /* pages can be executed */

/* Sharing type and options */
#define MAP_SHARED      0x0001          /* share changes */
#define MAP_PRIVATE     0x0002          /* changes are private */
#define MAP_COPY        MAP_PRIVATE     /* Obsolete */
#define MAP_FIXED        0x0010 /* map addr must be exactly as requested */
#define MAP_RENAME       0x0020 /* Sun: rename private pages to file */
#define MAP_NORESERVE    0x0040 /* Sun: don't reserve needed swap area */
#define MAP_INHERIT      0x0080 /* region is retained after exec */
#define MAP_NOEXTEND     0x0100 /* for MAP_FILE, don't change file size */
#define MAP_HASSEMAPHORE 0x0200 /* region may contain semaphores */
#define MAP_STACK        0x0400 /* region grows down, like a stack */

/* Error returned from mmap() */
#define MAP_FAILED      ((void *)-1)

/* Flags to msync */
#define MS_ASYNC        0x01    /* perform asynchronous writes */
#define MS_SYNC         0x02    /* perform synchronous writes */
#define MS_INVALIDATE   0x04    /* invalidate cached data */

/* File modes for 'open' not defined in MinGW32  (not used by mmap) */
#ifndef S_IWGRP
#define S_IWGRP 0
#define S_IRGRP 0
#define S_IROTH 0
#endif

/**
 * Map a file to a memory region
 */
void *mmap(void *addr, unsigned int len, int prot, int flags, int fd, unsigned int offset);

/**
 * Unmap a memory region
 */
int munmap(void *addr, int len);

/**
 * Synchronize a mapped region
 */
int msync(char *addr, int len, int flags);

#endif	/* _MMAN_H_ */
