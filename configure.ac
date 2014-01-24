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

PKG_CHECK_MODULES(GLIB2, glib-2.0, [], AC_MSG_ERROR([*** glib2 not found!]))
#PKG_CHECK_MODULES([GLIB2], [glib-2.0])
AC_SUBST(GLIB2_CFLAGS)
AC_SUBST(GLIB2_LIBS)

AC_ARG_ENABLE([debug], AC_HELP_STRING([--enable-debug], [create a debug build]))
AM_CONDITIONAL([DEBUG], [test "x$enable_debug" = xyes])

AC_ARG_ENABLE(geoip,--enable-geoip Enable GeoIP country lookup, USE_GEOIP="yes")
AC_ARG_ENABLE(utf8,--enable-utf8 Enable UTF-8 support for wide characters, USE_UTF8="yes")

if test "$USE_GEOIP" = "yes"; then
   AC_CHECK_LIB([GeoIP], [GeoIP_new], [], [AC_MSG_ERROR([libgeoip-dev is missing])])
fi

if test "$USE_UTF8" = "yes"; then
   AC_CHECK_LIB([ncursesw], [mvaddwstr], [], [AC_MSG_ERROR([libncursesw5-dev is missing])])
else
   AC_CHECK_LIB([ncurses], [refresh], [], [AC_MSG_ERROR([libncurses5-dev is missing])])
fi

# pthread
AC_CHECK_LIB([pthread], [pthread_create], [], [AC_MSG_ERROR([pthread is missing])])
CFLAGS="$CFLAGS -pthread"

# Checks for libraries.
AC_CHECK_LIB([glib-2.0], [g_free], [], [AC_MSG_ERROR([glib-2.x is missing])])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([netinet/in.h sys/socket.h arpa/inet.h locale.h netdb.h stdint.h stdlib.h string.h unistd.h stddef.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_STRTOD
AC_FUNC_FSEEKO
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([regcomp strtoull memmove floor gethostbyaddr gethostbyname memset setlocale strchr strdup strerror strrchr strstr strtol realpath])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
