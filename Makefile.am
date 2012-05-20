#AUTOMAKE_OPTIONS = foreign
bin_PROGRAMS = goaccess
goaccess_SOURCES = gmenu.c gmenu.h settings.c settings.h error.c error.h alloc.h  commons.c  commons.h  goaccess.c  parser.c  parser.h  ui.c  ui.h  util.c  util.h goaccess.h

AM_CFLAGS = -g -Wall @GLIB2_CFLAGS@
goaccess_LDADD = -lm
dist_man_MANS = goaccess.1
