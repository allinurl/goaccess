#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([goaccess], [1.2.1], [goaccess@prosoftcorp.com], [], [http://goaccess.io])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/goaccess.c])
AC_CONFIG_HEADERS([src/config.h])

# Use empty CFLAGS by default so autoconf does not add
# CFLAGS="-O2 -g"
# NOTE: Needs to go after AC_INIT and before AC_PROG_CC to select an
# empty default instead.
: ${CFLAGS=""}

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.18])
# Fix `undefined reference to `libintl_gettext'` on docker:
AC_CHECK_LIB([intl], [libintl_dgettext])

# pthread
AC_CHECK_LIB([pthread], [pthread_create], [], [AC_MSG_ERROR([pthread is missing])])
CFLAGS="$CFLAGS -pthread"

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
with_rdyanimc=yes
case "$host_os" in
  *darwin*|*cygwin*|*aix*|*mingw*) with_rdyanimc=no
  ;;
esac
AC_MSG_RESULT([$with_rdyanimc])
AM_CONDITIONAL([WITH_RDYNAMIC], [test "x$with_rdyanimc" = "xyes"])

# Build with OpenSSL
AC_ARG_WITH([openssl],AC_HELP_STRING([--with-openssl], [build with OpenSSL support]),
   [openssl="$withval"],[openssl="no"])

if test "$openssl" = 'yes'; then
  AC_CHECK_LIB([ssl], [SSL_CTX_new],,[AC_MSG_ERROR([ssl library missing])])
  AC_CHECK_LIB([crypto], [CRYPTO_free],,[AC_MSG_ERROR([crypto library missing])])
fi

# GeoIP
AC_ARG_ENABLE(geoip, [  --enable-geoip   Enable GeoIP country lookup. Default is disabled],
  [geoip="$enableval"], geoip=no)

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
fi
AM_CONDITIONAL([GEOIP_LEGACY], [test "x$geoip" = "xlegacy"])
AM_CONDITIONAL([GEOIP_MMDB], [test "x$geoip" = "xmmdb"])

# GNU getline / POSIX.1-2008
AC_ARG_WITH(getline, [  --with-getline Build using dynamic line buffer.],
  [with_getline=$withval], [with_getline=no])

if test "$with_getline" = "yes"; then
  AC_DEFINE([WITH_GETLINE], 1, [Build using GNU getline.])
fi

# UTF8
AC_ARG_ENABLE(utf8, [  --enable-utf8   Enable ncurses library that handles wide characters],
  [utf8="$enableval"], utf8=no)

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
    [AC_MSG_ERROR([*** Missing development libraries for ncurses])])
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
    LDFLAGS="$LDFLAGS $Z_FLAG"
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
    LDFLAGS="$LDFLAGS $BZ2_FLAG"
  fi

  case "$host_os" in
    *darwin*|*bsd*)
       LDFLAGS="$LDFLAGS -ltokyocabinet -lc"
    ;;
    *)
       LDFLAGS="$LDFLAGS -ltokyocabinet -lrt -lc"
    ;;
  esac
fi
AM_CONDITIONAL([TCB], [test "$WITH_TC" = "yes"])

if test "$tcb" = "memhash"; then
  storage="In-memory Hash Database (Tokyo Cabinet)"
elif test "$tcb" = "btree"; then
  storage="On-disk B+ Tree Database (Tokyo Cabinet)"
else
  storage="In-memory Hash Database (Default)"
fi

HAS_SEDTR=no
AC_CHECK_PROG([SED_CHECK],[sed],[yes],[no])
if test x"$SED_CHECK" == x"yes" ; then
  AC_CHECK_PROG([TR_CHECK],[tr],[yes],[no])
  if test x"$TR_CHECK" == x"yes" ; then
    HAS_SEDTR=yes
  fi
fi
AM_CONDITIONAL([HAS_SEDTR], [test "x$HAS_SEDTR" = xyes])

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
AC_CHECK_FUNCS([realloc])
AC_CHECK_FUNCS([realpath])
AC_CHECK_FUNCS([regcomp])
AC_CHECK_FUNCS([select])
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

AC_CONFIG_FILES([Makefile po/Makefile.in])
AC_OUTPUT

cat << EOF

Your build configuration:

  Prefix         : $prefix
  Package        : $PACKAGE_NAME
  Version        : $VERSION
  Compiler flags : $CFLAGS
  Linker flags   : $LIBS $LDFLAGS
  Dynamic buffer : $with_getline
  Geolocation    : $geolocation
  Storage method : $storage
  TLS/SSL        : $openssl
  Bugs           : $PACKAGE_BUGREPORT

EOF
