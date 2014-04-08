#AUTOMAKE_OPTIONS = foreign
bin_PROGRAMS = goaccess
goaccess_SOURCES = \
   browsers.c      \
   browsers.h      \
   commons.c       \
   commons.h       \
   csv.c           \
   csv.h           \
   error.c         \
   error.h         \
   gdashboard.c    \
   gdashboard.h    \
   gdns.c          \
   gdns.h          \
   gmenu.c         \
   gmenu.h         \
   goaccess.c      \
   goaccess.h      \
   json.c          \
   json.h          \
   opesys.c        \
   opesys.h        \
   output.c        \
   output.h        \
   parser.c        \
   parser.h        \
   settings.c      \
   settings.h      \
   ui.c            \
   ui.h            \
   util.c          \
   util.h          \
   xmalloc.c       \
   xmalloc.h

if TCB
goaccess_SOURCES += \
   tcabinet.c       \
   tcabinet.h
else
goaccess_SOURCES += \
   glibht.c         \
   glibht.h
endif

if DEBUG
AM_CFLAGS = -DDEBUG -O0 -g
else
AM_CFLAGS = -O2
endif

AM_CFLAGS += @GLIB2_CFLAGS@
AM_CFLAGS += -Wno-long-long -Wall -W -Wnested-externs -Wformat=2
AM_CFLAGS += -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
AM_CFLAGS += -Wwrite-strings -Wshadow -Wpointer-arith -Wsign-compare
AM_CFLAGS += -Wundef -Wbad-function-cast -Winline -Wcast-align -Wextra
AM_CFLAGS += -Wdeclaration-after-statement -Wno-missing-field-initializers

goaccess_LDADD = -lm
dist_man_MANS = goaccess.1
