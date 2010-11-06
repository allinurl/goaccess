#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([goaccess], [0.4], [goaccess@prosoftcorp.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([goaccess.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES(GLIB2, glib-2.0, [], AC_MSG_ERROR([*** glib2 not found!]))
#PKG_CHECK_MODULES([GLIB2], [glib-2.0])
AC_SUBST(GLIB2_CFLAGS)
AC_SUBST(GLIB2_LIBS)

# Checks for libraries.
AC_CHECK_LIB([ncurses], [refresh], [], [AC_MSG_ERROR([libncurses5-dev is missing])])
AC_CHECK_LIB([menu], [new_menu], [], [AC_MSG_ERROR([libncurses5-dev is missing])])
AC_CHECK_LIB([glib-2.0], [g_free], [], [AC_MSG_ERROR([glib-2.x is missing])])
AC_CHECK_LIB([GeoIP], [GeoIP_new], [], [AC_MSG_ERROR([libgeoip-dev is missing])])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([locale.h arpa/inet.h netdb.h stdint.h stdlib.h string.h unistd.h])
AC_CHECK_FUNCS([setlocale])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_FSEEKO
AC_CHECK_FUNCS([floor gethostbyaddr gethostbyname strchr strdup strerror strrchr strstr strtol memset])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
