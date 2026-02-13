#AUTOMAKE_OPTIONS = foreign
bin_PROGRAMS = goaccess
AUTOMAKE_OPTIONS = subdir-objects

dist_noinst_DATA =                 \
  resources/tpls.html              \
  resources/css/app.css            \
  resources/css/bootstrap.min.css  \
  resources/css/fa.min.css         \
  resources/js/app.js              \
  resources/js/charts.js           \
  resources/countries-110m.json    \
  resources/cities-10m.json        \
  resources/js/d3.v7.min.js        \
  resources/js/topojson.v3.min.js  \
  resources/js/hogan.min.js

noinst_PROGRAMS = bin2c
bin2c_SOURCES = src/bin2c.c

BUILT_SOURCES =       \
  src/tpls.h          \
  src/bootstrapcss.h  \
  src/facss.h         \
  src/appcss.h        \
  src/d3js.h          \
  src/topojsonjs.h    \
  src/hoganjs.h       \
  src/countries110m.h \
  src/cities10m.h     \
  src/chartsjs.h      \
  src/appjs.h

CLEANFILES =                          \
  src/tpls.h                          \
  src/bootstrapcss.h                  \
  src/facss.h                         \
  src/appcss.h                        \
  src/d3js.h                          \
  src/topojsonjs.h                    \
  src/hoganjs.h                       \
  src/countries110m.h                 \
  src/cities10m.h                     \
  src/chartsjs.h                      \
  src/appjs.h                         \
  resources/tpls.html.tmp             \
  resources/countries-110m.json.tmp   \
  resources/cities-10m.json.tmp       \
  resources/css/bootstrap.min.css.tmp \
  resources/css/fa.min.css.tmp        \
  resources/css/app.css.tmp           \
  resources/js/d3.v7.min.js.tmp       \
  resources/js/topojson.v3.min.js.tmp \
  resources/js/hogan.min.js.tmp       \
  resources/js/charts.js.tmp          \
  resources/js/app.js.tmp

# Tpls
src/tpls.h: bin2c$(EXEEXT) $(srcdir)/resources/tpls.html
if HAS_SEDTR
	cat $(srcdir)/resources/tpls.html | sed "s/^[[:space:]]*//" | sed "/^$$/d" | tr -d "\r\n" > $(srcdir)/resources/tpls.html.tmp
	./bin2c $(srcdir)/resources/tpls.html.tmp src/tpls.h tpls
else
	./bin2c $(srcdir)/resources/tpls.html src/tpls.h tpls
endif
# countries.json
src/countries110m.h: bin2c$(EXEEXT) $(srcdir)/resources/countries-110m.json
if HAS_SEDTR
	cat $(srcdir)/resources/countries-110m.json | sed "s/^[[:space:]]*//" | sed "/^$$/d" | tr -d "\r\n" > $(srcdir)/resources/countries-110m.json.tmp
	./bin2c $(srcdir)/resources/countries-110m.json.tmp src/countries110m.h countries_json
else
	./bin2c $(srcdir)/resources/countries-110m.json src/countries110m.h countries_json
endif
# cities.json
src/cities10m.h: bin2c$(EXEEXT) $(srcdir)/resources/cities-10m.json
if HAS_SEDTR
	cat $(srcdir)/resources/cities-10m.json | sed "s/^[[:space:]]*//" | sed "/^$$/d" | tr -d "\r\n" > $(srcdir)/resources/cities-10m.json.tmp
	./bin2c $(srcdir)/resources/cities-10m.json.tmp src/cities10m.h cities_json
else
	./bin2c $(srcdir)/resources/cities-10m.json src/cities10m.h cities_json
endif
# Bootstrap
src/bootstrapcss.h: bin2c$(EXEEXT) $(srcdir)/resources/css/bootstrap.min.css
if HAS_SEDTR
	cat $(srcdir)/resources/css/bootstrap.min.css | sed "s/^[[:space:]]*//" | sed "/^$$/d" | tr -d "\r\n" > $(srcdir)/resources/css/bootstrap.min.css.tmp
	./bin2c $(srcdir)/resources/css/bootstrap.min.css.tmp src/bootstrapcss.h bootstrap_css
else
	./bin2c $(srcdir)/resources/css/bootstrap.min.css src/bootstrapcss.h bootstrap_css
endif
# Font Awesome
src/facss.h: bin2c$(EXEEXT) $(srcdir)/resources/css/fa.min.css
if HAS_SEDTR
	cat $(srcdir)/resources/css/fa.min.css | sed "s/^[[:space:]]*//" | sed "/^$$/d" | tr -d "\r\n" > $(srcdir)/resources/css/fa.min.css.tmp
	./bin2c $(srcdir)/resources/css/fa.min.css.tmp src/facss.h fa_css
else
	./bin2c $(srcdir)/resources/css/fa.min.css src/facss.h fa_css
endif
# App.css
src/appcss.h: bin2c$(EXEEXT) $(srcdir)/resources/css/app.css
if HAS_SEDTR
	cat $(srcdir)/resources/css/app.css | sed "s/^[[:space:]]*//" | sed "/^$$/d" | tr -d "\r\n" > $(srcdir)/resources/css/app.css.tmp
	./bin2c $(srcdir)/resources/css/app.css.tmp src/appcss.h app_css
else
	./bin2c $(srcdir)/resources/css/app.css src/appcss.h app_css
endif
# D3.js
src/d3js.h: bin2c$(EXEEXT) $(srcdir)/resources/js/d3.v7.min.js
if HAS_SEDTR
	cat $(srcdir)/resources/js/d3.v7.min.js | sed "s/^[[:space:]]*//" | sed "/^$$/d" | tr -d "\r\n" > $(srcdir)/resources/js/d3.v7.min.js.tmp
	./bin2c $(srcdir)/resources/js/d3.v7.min.js.tmp src/d3js.h d3_js
else
	./bin2c $(srcdir)/resources/js/d3.v7.min.js src/d3js.h d3_js
endif
# topojson.js
src/topojsonjs.h: bin2c$(EXEEXT) $(srcdir)/resources/js/topojson.v3.min.js
if HAS_SEDTR
	cat $(srcdir)/resources/js/topojson.v3.min.js | sed "s/^[[:space:]]*//" | sed "/^$$/d" | tr -d "\r\n" > $(srcdir)/resources/js/topojson.v3.min.js.tmp
	./bin2c $(srcdir)/resources/js/topojson.v3.min.js.tmp src/topojsonjs.h topojson_js
else
	./bin2c $(srcdir)/resources/js/topojson.v3.min.js src/topojsonjs.h topojson_js
endif
# Hogan.js
src/hoganjs.h: bin2c$(EXEEXT) $(srcdir)/resources/js/hogan.min.js
if HAS_SEDTR
	cat $(srcdir)/resources/js/hogan.min.js | sed "s/^[[:space:]]*//" | sed "/^$$/d" | tr -d "\r\n" > $(srcdir)/resources/js/hogan.min.js.tmp
	./bin2c $(srcdir)/resources/js/hogan.min.js.tmp src/hoganjs.h hogan_js
else
	./bin2c $(srcdir)/resources/js/hogan.min.js src/hoganjs.h hogan_js
endif
# Charts.js
src/chartsjs.h: bin2c$(EXEEXT) $(srcdir)/resources/js/charts.js
if HAS_SEDTR
	cat $(srcdir)/resources/js/charts.js | sed -E "s@(,|;)[[:space:]]*//..*@\1@g" | sed -E "s@^[[:space:]]*//..*@@g" | sed "s/^[[:space:]]*//" | sed "/^$$/d" | tr -d "\r\n" > $(srcdir)/resources/js/charts.js.tmp
	./bin2c $(srcdir)/resources/js/charts.js.tmp src/chartsjs.h charts_js
else
	./bin2c $(srcdir)/resources/js/charts.js src/chartsjs.h charts_js
endif
if DEBUG
	./bin2c $(srcdir)/resources/js/charts.js src/chartsjs.h charts_js
endif
# App.js
src/appjs.h: bin2c$(EXEEXT) $(srcdir)/resources/js/app.js
if HAS_SEDTR
	cat $(srcdir)/resources/js/app.js | sed -E "s@(,|;)[[:space:]]*//..*@\1@g" | sed -E "s@^[[:space:]]*//..*@@g" | sed "s/^[[:space:]]*//" | sed "/^$$/d" | tr -d "\r\n" > $(srcdir)/resources/js/app.js.tmp
	./bin2c $(srcdir)/resources/js/app.js.tmp src/appjs.h app_js
else
	./bin2c $(srcdir)/resources/js/app.js src/appjs.h app_js
endif
if DEBUG
	./bin2c $(srcdir)/resources/js/app.js src/appjs.h app_js
endif

confdir = $(sysconfdir)/goaccess
dist_conf_DATA = config/goaccess.conf
dist_conf_DATA += config/browsers.list
dist_conf_DATA += config/podcast.list

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
   src/dialogs.c       \
   src/dialogs.h       \
   src/error.c         \
   src/error.h         \
   src/fileio.c        \
   src/fileio.h        \
   src/gchart.c        \
   src/gchart.h        \
   src/gdashboard.c    \
   src/gdashboard.h    \
   src/gdns.c          \
   src/gdns.h          \
   src/gholder.c       \
   src/gholder.h       \
   src/gkhash.c        \
   src/gkhash.h        \
   src/gkmhash.c       \
   src/gkmhash.h       \
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
   src/khash.h         \
   src/labels.h        \
   src/opesys.c        \
   src/opesys.h        \
   src/options.c       \
   src/options.h       \
   src/output.c        \
   src/output.h        \
   src/parser.c        \
   src/parser.h        \
   src/persistence.c   \
   src/persistence.h   \
   src/pdjson.c        \
   src/pdjson.h        \
   src/settings.c      \
   src/settings.h      \
   src/sort.c          \
   src/sort.h          \
   src/tpl.c           \
   src/tpl.h           \
   src/ui.c            \
   src/ui.h            \
   src/util.c          \
   src/util.h          \
   src/websocket.c     \
   src/websocket.h     \
   src/xmalloc.c       \
   src/xmalloc.h

if WITH_SSL
goaccess_SOURCES +=  \
   src/wsauth.c      \
   src/wsauth.h
endif

if USE_SHA1
goaccess_SOURCES +=  \
   src/sha1.c        \
   src/sha1.h
endif

if USE_MMAP
goaccess_SOURCES +=  \
   src/win/mman.h    \
   src/win/mmap.c
endif

if GEOIP_LEGACY
goaccess_SOURCES +=  \
  src/geoip1.c       \
  src/geoip1.h
endif

if GEOIP_MMDB
goaccess_SOURCES +=  \
   src/geoip2.c      \
   src/geoip1.h
endif

if DEBUG
AM_CFLAGS = -DDEBUG -O0 -DSYSCONFDIR=\"$(sysconfdir)\"
else
AM_CFLAGS = -O2 -DSYSCONFDIR=\"$(sysconfdir)\"
endif

if WITH_RDYNAMIC
AM_LDFLAGS = -rdynamic
endif

if WITH_ZLIB
AM_LDFLAGS = -lz
endif

goaccess_LDADD = -lm

AM_CFLAGS += -Wall -Wextra -Wnested-externs -Wformat=2 -g
AM_CFLAGS += -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
AM_CFLAGS += -Wwrite-strings -Wshadow -Wpointer-arith -Wsign-compare
AM_CFLAGS += -Wbad-function-cast -Wcast-align
AM_CFLAGS += -Wdeclaration-after-statement -Wshadow -Wold-style-definition

if WITH_ASAN
AM_CFLAGS += -fsanitize=address
endif

dist_man_MANS = goaccess.1

SUBDIRS = po

ACLOCAL_AMFLAGS = -I m4

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

EXTRA_DIST = config.rpath
