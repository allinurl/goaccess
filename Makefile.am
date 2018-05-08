#AUTOMAKE_OPTIONS = foreign
bin_PROGRAMS = goaccess
AUTOMAKE_OPTIONS = subdir-objects

dist_noinst_DATA =                \
  resources/tpls.html             \
  resources/css/app.css           \
  resources/css/bootstrap.min.css \
  resources/css/fa.min.css        \
  resources/js/app.js             \
  resources/js/charts.js          \
  resources/js/d3.v3.min.js       \
  resources/js/hogan.min.js

noinst_PROGRAMS = bin2c
bin2c_SOURCES = src/bin2c.c

BUILT_SOURCES =   \
  tpls.h          \
  bootstrapcss.h  \
  facss.h         \
  appcss.h        \
  d3js.h          \
  hoganjs.h       \
  chartsjs.h      \
  appjs.h

CLEANFILES =          					\
  src/tpls.h          					\
  src/bootstrapcss.h  					\
  src/facss.h         					\
  src/appcss.h        					\
  src/d3js.h          					\
  src/hoganjs.h       					\
  src/chartsjs.h      					\
  src/appjs.h   								\
	resources/tpls.html.tmp

# Tpls
tpls.h: bin2c$(EXEEXT) resources/tpls.html
	cat resources/tpls.html | sed "s/^[ \t]*//" | sed "/^$$/d" | tr -d "\r\n" > resources/tpls.html.tmp
	./bin2c resources/tpls.html.tmp src/tpls.h tpls
# Bootstrap
bootstrapcss.h: bin2c$(EXEEXT) resources/css/bootstrap.min.css
	./bin2c resources/css/bootstrap.min.css src/bootstrapcss.h bootstrap_css
# Font Awesome
facss.h: bin2c$(EXEEXT) resources/css/fa.min.css
	./bin2c resources/css/fa.min.css src/facss.h fa_css
# App.css
appcss.h: bin2c$(EXEEXT) resources/css/app.css
	./bin2c resources/css/app.css src/appcss.h app_css
# D3.js
d3js.h: bin2c$(EXEEXT) resources/js/d3.v3.min.js
	./bin2c resources/js/d3.v3.min.js src/d3js.h d3_js
# Hogan.js
hoganjs.h: bin2c$(EXEEXT) resources/js/hogan.min.js
	./bin2c resources/js/hogan.min.js src/hoganjs.h hogan_js
# Charts.js
chartsjs.h: bin2c$(EXEEXT) resources/js/charts.js
	./bin2c resources/js/charts.js src/chartsjs.h charts_js
# App.js
appjs.h: bin2c$(EXEEXT) resources/js/app.js
	./bin2c resources/js/app.js src/appjs.h app_js

confdir = $(sysconfdir)
dist_conf_DATA = config/goaccess.conf

goaccess_SOURCES = \
   src/base64.c        \
   src/base64.h        \
   src/browsers.c      \
   src/browsers.h      \
   src/color.c         \
   src/color.h         \
   src/commons.c       \
   src/commons.h       \
   src/csv.c           \
   src/csv.h           \
   src/error.c         \
   src/error.h         \
   src/gdashboard.c    \
   src/gdashboard.h    \
   src/gdns.c          \
   src/gdns.h          \
   src/gholder.c       \
   src/gholder.h       \
   src/gmenu.c         \
   src/gmenu.h         \
   src/goaccess.c      \
   src/goaccess.h      \
   src/gslist.c        \
   src/gslist.h        \
   src/gstorage.c      \
   src/gstorage.h      \
   src/gwsocket.c      \
   src/gwsocket.h      \
   src/json.c          \
   src/json.h          \
   src/labels.h        \
   src/opesys.c        \
   src/opesys.h        \
   src/options.c       \
   src/options.h       \
   src/output.c        \
   src/output.h        \
   src/parser.c        \
   src/parser.h        \
   src/settings.c      \
   src/settings.h      \
   src/sha1.c          \
   src/sha1.h          \
   src/sort.c          \
   src/sort.h          \
   src/ui.c            \
   src/ui.h            \
   src/util.c          \
   src/util.h          \
   src/websocket.c     \
   src/websocket.h     \
   src/xmalloc.c       \
   src/xmalloc.h

if TCB
goaccess_SOURCES += \
   src/tcabdb.c     \
   src/tcabdb.h     \
   src/tcbtdb.c     \
   src/tcbtdb.h
else
goaccess_SOURCES += \
   src/khash.h      \
   src/gkhash.c     \
   src/gkhash.h
endif

if GEOIP_LEGACY
goaccess_SOURCES +=  \
   src/geoip1.c \
   src/geoip1.h
endif

if GEOIP_MMDB
goaccess_SOURCES +=  \
   src/geoip2.c \
   src/geoip1.h
endif

if DEBUG
AM_CFLAGS = -DDEBUG -O0 -g -DSYSCONFDIR=\"$(sysconfdir)\"
else
AM_CFLAGS = -O2 -DSYSCONFDIR=\"$(sysconfdir)\"
endif

if WITH_RDYNAMIC
AM_LDFLAGS = -rdynamic
endif

AM_CFLAGS += -Wno-long-long -Wall -W -Wnested-externs -Wformat=2
AM_CFLAGS += -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
AM_CFLAGS += -Wwrite-strings -Wshadow -Wpointer-arith -Wsign-compare
AM_CFLAGS += -Wbad-function-cast -Winline -Wcast-align -Wextra
AM_CFLAGS += -Wdeclaration-after-statement -Wno-missing-field-initializers

dist_man_MANS = goaccess.1

SUBDIRS = po

ACLOCAL_AMFLAGS = -I m4

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

EXTRA_DIST = config.rpath
