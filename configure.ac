#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([goaccess], [0.9.2], [goaccess@prosoftcorp.com], [], [http://goaccess.io])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/goaccess.c])
AC_CONFIG_HEADERS([src/config.h])

# Use empty CFLAGS by default so autoconf does not add
# CFLAGS="-O2 -g"
: ${CFLAGS=""}

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

# pthread
AC_CHECK_LIB([pthread], [pthread_create], [], [AC_MSG_ERROR([pthread is missing])])
CFLAGS="-pthread"

# DEBUG
AC_ARG_ENABLE(debug, [  --enable-debug   Create a debug build. Default is disabled],
  [debug="$enableval"], debug=no)

if test "$debug" = "yes"; then
  AC_DEFINE([_DEBUG], 1, [Debug option])
fi
AM_CONDITIONAL([DEBUG], [test "x$debug" = "xyes"])

# Handle rdynamic only on systems using GNU ld
AC_CANONICAL_HOST
AC_MSG_CHECKING([whether to build with rdynamic for GNU ld])
AS_CASE([$host_os],
        [*darwin*|*cygwin*|*aix*|*mingw*], [with_rdyanimc=no],
        [with_rdyanimc=yes])
AC_MSG_RESULT([$with_rdyanimc])
AM_CONDITIONAL([WITH_RDYNAMIC], [test "x$with_rdyanimc" = "xyes"])

# GeoIP
AC_ARG_ENABLE(geoip, [  --enable-geoip   Enable GeoIP country lookup. Default is disabled],
  [geoip="$enableval"], geoip=no)

if test "$geoip" = "yes"; then
  AC_CHECK_LIB([GeoIP], [GeoIP_new], [],
    [AC_MSG_ERROR([*** Missing development files for the GeoIP library])])
fi
AM_CONDITIONAL([GEOLOCATION], [test "x$geoip" = "xyes"])

# UTF8
AC_ARG_ENABLE(utf8, [  --enable-utf8   Enable ncurses library that handles wide characters],
  [utf8="$enableval"], utf8=no)

if test "$utf8" = "yes"; then
  AC_CHECK_LIB([ncursesw], [mvaddwstr], [],
    [AC_MSG_ERROR([*** Missing development libraries for ncursesw])])

  have_ncurses="yes"
  AC_CHECK_HEADERS([ncursesw/ncurses.h],[have_ncurses=yes], [], [
  #ifdef HAVE_NCURSESW_NCURSES_H
  #include <ncursesw/ncurses.h>
  #endif
  ])

  AC_CHECK_HEADERS([ncurses.h],[have_ncurses=yes], [], [
  #ifdef HAVE_NCURSES_H
  #include <ncurses.h>
  #endif
  ])

  if test "$have_ncurses" != "yes"; then
    AC_MSG_ERROR([Missing ncursesw header file])
  fi
else
  AC_CHECK_LIB([ncurses], [refresh], [],
    [AC_MSG_ERROR([*** Missing development libraries for ncurses])])

  have_ncurses="yes"
  AC_CHECK_HEADERS([ncurses/ncurses.h],[have_ncurses=yes], [], [
  #ifdef HAVE_NCURSES_NCURSES_H
  #include <ncurses/ncurses.h>
  #endif
  ])

  AC_CHECK_HEADERS([ncurses.h],[have_ncurses=yes], [], [
  #ifdef HAVE_NCURSES_H
  #include <ncurses.h>
  #endif
  ])

  AC_CHECK_HEADERS([curses.h],[have_ncurses=yes], [], [
  #ifdef HAVE_CURSES_H
  #include <curses.h>
  #endif
  ])

  if test "$have_ncurses" != "yes"; then
    AC_MSG_ERROR([Missing ncursesw header file])
  fi
fi

# Tokyo Cabinet
AC_ARG_ENABLE(tcb, [  --enable-tcb   Enable TokyoCabinet database. Default is disabled],
  [tcb="$enableval"], tcb=no)

WITH_TC=no
if test "$tcb" = "memhash"; then
  WITH_TC=yes
  AC_DEFINE([TCB_MEMHASH], [1], ["Build using on-memory hash database"])
elif test "$tcb" = "btree"; then
  AC_DEFINE([TCB_BTREE], [1], ["Build using on-disk B+ Tree database"])
  WITH_TC=yes
fi

if test "$WITH_TC" = "yes"; then
  AC_CHECK_LIB([tokyocabinet], [tchdbnew], [],
    [AC_MSG_ERROR([*** Missing development libraries for Tokyo Cabinet Database])])

  AC_ARG_ENABLE([zlib], [  --disable-zlib   Build without ZLIB compression],
    [zlib="$enableval"], zlib=yes)

  if test "$zlib" = "yes"; then
    AC_CHECK_LIB(z, gzread, [Z_FLAG=-lz], AC_MSG_ERROR([
      *** zlib is required. If zlib compression is not needed
      *** you can use --disable-zlib.
      *** Debian based distributions zlib1g-dev
      *** Red Hat based distributions zlib-devel
    ]))
    AC_DEFINE([HAVE_ZLIB], [1], ["Build using ZLIB"])
    CFLAGS="$CFLAGS $Z_FLAG"
  fi

  AC_ARG_ENABLE([bzip], [  --disable-bzip   Build without BZIP2 compression],
    [bz2="$enableval"], bz2=yes)

  if test "$bz2" = "yes"; then
    AC_CHECK_LIB(bz2, BZ2_bzopen, [BZ2_FLAG=-lbz2], AC_MSG_ERROR([
      *** BZIP2 is required. If BZIP2 compression is not needed
      *** you can use --disable-bzip.
      *** Debian based distributions libbz2-dev
      *** Red Hat based distributions bzip2-devel
    ]))
    AC_DEFINE([HAVE_BZ2], [1], ["Build using BZ2"])
    CFLAGS="$CFLAGS $BZ2_FLAG"
  fi

  CFLAGS="$CFLAGS -ltokyocabinet -lrt -lc"

# GLib otherwise
else
  # Check for pkg-config program, used for configuring some libraries.
  m4_define_default([PKG_PROG_PKG_CONFIG],
    [AC_MSG_CHECKING([pkg-config])
    AC_MSG_RESULT([no])])

  PKG_PROG_PKG_CONFIG

  # If pkg-config autoconf support isn't installed, define its autoconf macro.
  m4_define_default([PKG_CHECK_MODULES],
    [AC_MSG_CHECKING([$1])
    AC_MSG_RESULT([no])
    $4])

  AC_PATH_PROG([PKG_CONFIG], [pkg-config], [no])

  AS_IF([test "x$PKG_CONFIG" = "xno"],[
     AC_MSG_ERROR([
     *** pkg-config script could not be found. Make sure it is
     *** in your path, or set the PKG_CONFIG environment variable
     *** to the full path to pkg-config. Otherwise, reinstall glib2
     *** development files (libglib2.0-dev)])
  ])

  PKG_CHECK_MODULES(GLIB2, glib-2.0, [], AC_MSG_ERROR([*** Missing development libraries for GLib]))
  AC_SUBST(GLIB2_CFLAGS)
  AC_SUBST(GLIB2_LIBS)
  AC_CHECK_LIB([glib-2.0], [g_list_append], [], [AC_MSG_ERROR([*** Missing development libraries for GLib])])
fi
AM_CONDITIONAL([TCB], [test "$WITH_TC" = "yes"])

if test "$tcb" = "memhash"; then
  storage="On-memory Hash Database (Tokyo Cabinet)"
elif test "$tcb" = "btree"; then
  storage="On-disk B+ Tree Database (Tokyo Cabinet)"
else
  storage="On-memory Hash Database (GLib)"
fi

# Solaris
AC_CHECK_LIB([socket], [socket])
AC_CHECK_LIB([nsl], [gethostbyname])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([netinet/in.h])
AC_CHECK_HEADERS([sys/socket.h])
AC_CHECK_HEADERS([arpa/inet.h])
AC_CHECK_HEADERS([locale.h])
AC_CHECK_HEADERS([netdb.h])
AC_CHECK_HEADERS([stdint.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([stddef.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_STRTOD
AC_FUNC_FSEEKO
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([regcomp])
AC_CHECK_FUNCS([strtoull])
AC_CHECK_FUNCS([memmove])
AC_CHECK_FUNCS([floor])
AC_CHECK_FUNCS([gethostbyaddr])
AC_CHECK_FUNCS([gethostbyname])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([setlocale])
AC_CHECK_FUNCS([strchr])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strerror])
AC_CHECK_FUNCS([strrchr])
AC_CHECK_FUNCS([strstr])
AC_CHECK_FUNCS([strtol])
AC_CHECK_FUNCS([realpath])
AC_CHECK_FUNCS([malloc])
AC_CHECK_FUNCS([realloc])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

cat << EOF

Your build configuration:

  CFLAGS = $CFLAGS
  storage: $storage
  prefix: $prefix
  package: $PACKAGE_NAME
  version: $VERSION
  bugs: $PACKAGE_BUGREPORT

EOF
