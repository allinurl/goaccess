#!/usr/bin/env sh
# Build binaries on Debian.
# TODO
# - build in separate folder
# - make volume read only
set -o nounset   ## set -u : exit the script if you try to use an uninitialised variable
set -o errexit   ## set -e : exit the script if any statement returns a non-true return value

autoreconf -fi > /dev/null 2>&1
./configure CFLAGS=-Werror --enable-utf8 --with-openssl --with-getline --enable-geoip=legacy >/dev/null
make -j > /dev/null
cat goaccess
