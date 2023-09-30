#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([goaccess],[1.8],[hello@goaccess.io],[],[https://goaccess.io])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/goaccess.c])
AC_CONFIG_HEADERS([src/config.h])

# Use empty CFLAGS by default so autoconf does not add
# CFLAGS="-O2 -g"
# NOTE: Needs to go after AC_INIT and before AC_PROG_CC to select an
# empty default instead.
: ${CFLAGS=""}

# Prefer host default compiler
AC_PROG_CC([cc gcc clang])
AM_PROG_CC_C_O

# Check for programs
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19])
# Fix `undefined reference to `libintl_gettext'` on docker:
AC_CHECK_LIB([intl], [libintl_dgettext])
# Fix undefined reference to dgettext on NetBSD
AC_CHECK_LIB([intl], [dgettext])

# pthread
AC_CHECK_LIB([pthread], [pthread_create], [], [AC_MSG_ERROR([pthread is missing])])
CFLAGS="$CFLAGS -pthread"

# DEBUG
AC_ARG_ENABLE([debug],[AS_HELP_STRING([--enable-debug],[Create a debug build. Default is disabled])],[debug="$enableval"],[debug=no])

if test "$debug" = "yes"; then
  AC_DEFINE([_DEBUG], 1, [Debug option])
fi
AM_CONDITIONAL([DEBUG], [test "x$debug" = "xyes"])

# Handle rdynamic only on systems using GNU ld
AC_CANONICAL_HOST
AC_MSG_CHECKING([whether to build with rdynamic for GNU ld])
with_rdyanimc=yes
case "$host_os" in
  *darwin*|*cygwin*|*aix*|*mingw*) with_rdyanimc=no
  ;;
esac
AC_MSG_RESULT([$with_rdyanimc])
AM_CONDITIONAL([WITH_RDYNAMIC], [test "x$with_rdyanimc" = "xyes"])

# Check for libc implementation on NetBSD
AC_CHECK_HEADERS([sha.h sha1.h])
AC_CHECK_FUNCS([SHA1Init])
AM_CONDITIONAL([USE_SHA1], [test "x$ac_cv_func_SHA1Init" != "xyes"])

# Build with OpenSSL
AC_ARG_WITH([openssl],[AS_HELP_STRING([--with-openssl],[Build with OpenSSL support. Default is disabled])],[openssl="$withval"],[openssl="no"])

if test "$openssl" = 'yes'; then
  AC_CHECK_LIB([ssl], [SSL_CTX_new],,[AC_MSG_ERROR([ssl library missing])])
  AC_CHECK_LIB([crypto], [CRYPTO_free],,[AC_MSG_ERROR([crypto library missing])])
  AC_CHECK_LIB([ssl], [SSL_CIPHER_standard_name], [AC_DEFINE([HAVE_CIPHER_STD_NAME], 1, [HAVE_CIPHER_STD_NAME])])
fi

# GeoIP
AC_ARG_ENABLE([geoip],[AS_HELP_STRING([--enable-geoip],[Enable GeoIP country lookup. Supported types: mmdb, legacy. Default is disabled])],[geoip="$enableval"],[geoip=no])

geolocation="N/A"
if test "$geoip" = "mmdb"; then
  AC_CHECK_LIB([maxminddb], [MMDB_open], [], [AC_MSG_ERROR([
    *** Missing development files for libmaxminddb library.
  ])])
  geolocation="GeoIP2"
  AC_DEFINE([HAVE_GEOLOCATION], 1, [Build using GeoIP.])
elif test "$geoip" = "legacy"; then
  AC_CHECK_LIB([GeoIP], [GeoIP_new], [], [AC_MSG_ERROR([
    *** Missing development files for the GeoIP library
  ])])
  geolocation="GeoIP Legacy"
  AC_DEFINE([HAVE_GEOLOCATION], 1, [Build using GeoIP.])
elif test "$geoip" != "no"; then
  AC_MSG_ERROR([*** Invalid argument for GeoIP: $geoip])
fi
AM_CONDITIONAL([GEOIP_LEGACY], [test "x$geoip" = "xlegacy"])
AM_CONDITIONAL([GEOIP_MMDB], [test "x$geoip" = "xmmdb"])

# GNU getline / POSIX.1-2008
AC_ARG_WITH([getline],[AS_HELP_STRING([--with-getline],[Build using dynamic line buffer. Default is disabled])],[with_getline=$withval],[with_getline=no])

if test "$with_getline" = "yes"; then
  AC_DEFINE([WITH_GETLINE], 1, [Build using GNU getline.])
fi

# UTF8
AC_ARG_ENABLE([utf8],[AS_HELP_STRING([--enable-utf8],[Enable ncurses library that handles wide characters. Default is disabled])],[utf8="$enableval"],[utf8=no])

if test "$utf8" = "yes"; then
  libncursesw=ncursesw
  # Simply called libncurses on OS X
  case "$host_os" in
    *darwin*) libncursesw=ncurses
    ;;
  esac

  AC_CHECK_LIB([$libncursesw], [mvaddwstr], [],
      [AC_MSG_ERROR([*** Missing development libraries for ncursesw])])
  AC_SEARCH_LIBS([tputs], [tinfow], ,[AC_MSG_ERROR([Cannot find a library providing tputs])])
  AC_DEFINE([HAVE_LIBNCURSESW], [1], ["ncursesw is present."])

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
    [AC_CHECK_LIB([curses], [refresh], [],
    [AC_MSG_ERROR([*** Missing development libraries for ncurses])])])
  AC_SEARCH_LIBS([tputs], [tinfo], ,[AC_MSG_ERROR([Cannot find a library providing tputs])])

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
    AC_MSG_ERROR([Missing ncurses header file])
  fi
fi

# Default Hash
storage="In-Memory with On-Disk Persistent Storage"

HAS_SEDTR=no
AC_CHECK_PROG([SED_CHECK],[sed],[yes],[no])
if test x"$SED_CHECK" = x"yes" ; then
  AC_CHECK_PROG([TR_CHECK],[tr],[yes],[no])
  if test x"$TR_CHECK" = x"yes" ; then
    HAS_SEDTR=yes
  fi
fi
AM_CONDITIONAL([HAS_SEDTR], [test "x$HAS_SEDTR" = xyes])

# detect Cygwin or MinGW and use mmap family replacements
USE_MMAP=no
case $host in
  *-*-mingw32* | *-*-cygwin* | *-*-windows*)
  USE_MMAP=yes
  AC_MSG_NOTICE([using custom mmap for Cygwin/MinGW])
  ;;
esac
AM_CONDITIONAL([USE_MMAP], [test "x$USE_MMAP" = xyes])

# Solaris
AC_CHECK_LIB([nsl], [gethostbyname])
AC_CHECK_LIB([socket], [socket])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h])
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([inttypes.h])
AC_CHECK_HEADERS([limits.h])
AC_CHECK_HEADERS([locale.h])
AC_CHECK_HEADERS([netdb.h])
AC_CHECK_HEADERS([netinet/in.h])
AC_CHECK_HEADERS([stddef.h])
AC_CHECK_HEADERS([stdint.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([strings.h])
AC_CHECK_HEADERS([sys/socket.h])
AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_CHECK_TYPES([ptrdiff_t])
AC_STRUCT_TM
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FSEEKO
AC_FUNC_MEMCMP
AC_FUNC_MKTIME
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_STRTOD
AC_CHECK_FUNCS([floor])
AC_CHECK_FUNCS([gethostbyaddr])
AC_CHECK_FUNCS([gethostbyname])
AC_CHECK_FUNCS([gettimeofday])
AC_CHECK_FUNCS([malloc])
AC_CHECK_FUNCS([memmove])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([mkfifo])
AC_CHECK_FUNCS([poll])
AC_CHECK_FUNCS([realloc])
AC_CHECK_FUNCS([realpath])
AC_CHECK_FUNCS([regcomp])
AC_CHECK_FUNCS([setlocale])
AC_CHECK_FUNCS([socket])
AC_CHECK_FUNCS([strcasecmp])
AC_CHECK_FUNCS([strchr])
AC_CHECK_FUNCS([strcspn])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strerror])
AC_CHECK_FUNCS([strncasecmp])
AC_CHECK_FUNCS([strpbrk])
AC_CHECK_FUNCS([strrchr])
AC_CHECK_FUNCS([strspn])
AC_CHECK_FUNCS([strstr])
AC_CHECK_FUNCS([strtol])
AC_CHECK_FUNCS([strtoull])
AC_CHECK_FUNCS([timegm])

AC_CONFIG_FILES([Makefile po/Makefile.in])
AC_OUTPUT

cat << EOF

Your build configuration:

  Prefix         : $prefix
  Package        : $PACKAGE_NAME
  Version        : $VERSION
  Compiler flags : $CFLAGS
  Linker flags   : $LIBS $LDFLAGS
  UTF-8 support  : $utf8
  Dynamic buffer : $with_getline
  Geolocation    : $geolocation
  Storage method : $storage
  TLS/SSL        : $openssl
  Bugs           : $PACKAGE_BUGREPORT

EOF
