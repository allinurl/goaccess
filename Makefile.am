#AUTOMAKE_OPTIONS = foreign
bin_PROGRAMS = goaccess
goaccess_SOURCES = settings.c settings.h error.c error.h alloc.h  commons.c  commons.h  goaccess.c  parser.c  parser.h  ui.c  ui.h  util.c  util.h goaccess.h

AM_CFLAGS = -g -Wall @GLIB2_CFLAGS@
AM_LDFLAGS = -lmenu -lcurses -lm
LIBS = @GLIB2_LIBS@ -lrt
dist_man_MANS = goaccess.1

#AM_LDFLAGS = `pkg-config --libs glib-2.0`
