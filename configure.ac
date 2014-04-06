#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([goaccess], [0.7.1], [goaccess@prosoftcorp.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([goaccess.c])
AC_CONFIG_HEADERS([config.h])

# Use empty CFLAGS by default so autoconf does not add
# CFLAGS="-O2 -g"
: ${CFLAGS=""}

# Checks for programs.
AC_PROG_CC
PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES(GLIB2, glib-2.0, [],
  AC_MSG_ERROR([*** Missing development files for the GLib library!]))
AC_SUBST(GLIB2_CFLAGS)
AC_SUBST(GLIB2_LIBS)

# DEBUG
AC_ARG_ENABLE(debug, [  --enable-debug   Create a debug build. Default is disabled],
  [debug="$enableval"], debug=no)

if test "$debug" = "yes"; then
  AC_DEFINE([_DEBUG], 1, [Debug option])
fi
AM_CONDITIONAL([DEBUG], [test "x$debug" = "xyes"])

# GeoIP
AC_ARG_ENABLE(geoip, [  --enable-geoip   Enable GeoIP country lookup. Default is disabled],
  [geoip="$enableval"], geoip=no)

if test "$geoip" = "yes"; then
  AC_CHECK_LIB([GeoIP], [GeoIP_new], [], 
    [AC_MSG_ERROR([*** Missing development files for the GeoIP library])])
fi

# UTF8
AC_ARG_ENABLE(utf8, [  --enable-utf8   Enable ncurses library that handles wide characters],
  [utf8="$enableval"], utf8=no)

if test "$utf8" = "yes"; then
  AC_CHECK_LIB([ncursesw], [mvaddwstr], [],
    [AC_MSG_ERROR([*** Missing development libraries for ncursesw])])
else
  AC_CHECK_LIB([ncurses], [refresh], [],
    [AC_MSG_ERROR([*** Missing development libraries for ncurses])])
fi

# pthread
AC_CHECK_LIB([pthread], [pthread_create], [], [AC_MSG_ERROR([pthread is missing])])
CFLAGS="$CFLAGS -pthread"

# Checks for libraries.
AC_CHECK_LIB([glib-2.0], [g_free], [], [AC_MSG_ERROR([glib-2.x is missing])])

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
  prefix: $prefix
  version: $VERSION

EOF
