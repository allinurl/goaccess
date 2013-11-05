#AUTOMAKE_OPTIONS = foreign
bin_PROGRAMS = goaccess
goaccess_SOURCES = \
   output.c        \
   output.h        \
   settings.c      \
   settings.h      \
   error.c         \
   error.h         \
   xmalloc.c       \
   xmalloc.h       \
   commons.c       \
   commons.h       \
   goaccess.c      \
   parser.c        \
   parser.h        \
   ui.c            \
   ui.h            \
   util.c          \
   util.h          \
   gdashboard.c    \
   gdashboard.h    \
   gdns.c          \
   gdns.h          \
   gmenu.c         \
   gmenu.h         \
   goaccess.h

AM_CFLAGS = -g @GLIB2_CFLAGS@
AM_CFLAGS += -Wno-long-long -Wall -W -Wnested-externs -Wformat=2
AM_CFLAGS += -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
AM_CFLAGS += -Wwrite-strings -Wshadow -Wpointer-arith -Wsign-compare
AM_CFLAGS += -Wundef -Wbad-function-cast -Winline -Wcast-align -Wextra
AM_CFLAGS += -Wdeclaration-after-statement -Wno-missing-field-initializers

goaccess_LDADD = -lm
dist_man_MANS = goaccess.1
