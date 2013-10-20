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

AM_CFLAGS = -g -Wall @GLIB2_CFLAGS@
goaccess_LDADD = -lm
dist_man_MANS = goaccess.1
