#AUTOMAKE_OPTIONS = foreign
bin_PROGRAMS = goaccess
goaccess_SOURCES = error.c error.h alloc.h  commons.c  commons.h  goaccess.c  parser.c  parser.h  ui.c  ui.h  util.c  util.h 

AM_CFLAGS = -g @GLIB2_CFLAGS@
AM_LDFLAGS = -lmenu -lcurses -lGeoIP 
LIBS = @GLIB2_LIBS@ -lrt
dist_man_MANS = goaccess.1

#AM_LDFLAGS = `pkg-config --libs glib-2.0`
