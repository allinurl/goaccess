#!/usr/bin/env sh
# Build dynmic linked binaries on Debian.
set -o nounset   ## set -u : exit the script if you try to use an uninitialised variable
set -o errexit   ## set -e : exit the script if any statement returns a non-true return value

# should be either of "mmdb" or "legacy"
geoip=${1:-"mmdb"}

autoreconf -fi > /dev/null 2>&1
./configure CFLAGS=-Werror --enable-utf8 --with-openssl --with-getline --enable-geoip="$geoip" >/dev/null
make -j > /dev/null
cat goaccess
