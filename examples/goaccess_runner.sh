#!/usr/bin/env bash

set -eu

# Discover log files from various webservers.
# Create persistent database, process logs.

GOACCESS=/usr/bin/goaccess

# no trailing slash
DBPATH=/var/lib/goaccess
HTMLPATH=/var/www/html/goaccess

log() {
  if [ -f /usr/bin/systemd-cat ]; then
    echo "goaccess_runner $1" | /usr/bin/systemd-cat
  else
    /usr/bin/logger "goaccess_runner $1"
  fi
}

for wsdir in /var/log/nginx /var/log/apache2 ; do

  if [ ! -d $wsdir ]; then
    continue
  fi

  for logfname in $wsdir/*.log; do
    sitename=$(basename ${logfname%.log})
    [[ $sitename == error ]] && continue
    [[ $sitename == netdata ]] && continue

    dbdir="$DBPATH/$sitename"
    if [ ! -d $dbdir ]; then
      log "initializing $HTMLPATH/$sitename.html"
      mkdir -p $dbdir
      # e.g. /var/log/nginx/foo.org.log.6.gz
      if compgen -G "$logfname.*.gz" > /dev/null; then
        for zfn in $logfname.*.gz; do
          log "parsing $zfn"
          zcat $zfn | $GOACCESS \
            --keep-db-files --load-from-disk \
            --db-path=$dbdir \
            -f - \
            -o $HTMLPATH/$sitename.html \
            --log-format=COMMON || true
        done
      fi
    fi

    log "updating $HTMLPATH/$sitename.html"
    $GOACCESS \
      --keep-db-files --load-from-disk \
      --db-path=$dbdir \
      -f $logfname \
      -o $HTMLPATH/$sitename.html \
      --log-format=COMMON || true

  done  # logfname
done  # wsdir
