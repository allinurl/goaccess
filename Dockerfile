FROM pureos/amber AS builds
RUN apk add --no-cache \
    autoconf \
    automake \
    build-base \
    clang \
    clang-static \
    gettext-dev \
    gettext-static \
    git \
    libmaxminddb-dev \
    libmaxminddb-static \
    libressl-dev \
    linux-headers \
    ncurses-dev \
    ncurses-static \
    tzdata

# GoAccess
COPY . /goaccess
WORKDIR /goaccess
RUN autoreconf -fiv
RUN CC="clang" CFLAGS="-O3 -static" LIBS="$(pkg-config --libs openssl)" ./configure --prefix="" --enable-utf8 --with-openssl --enable-geoip=mmdb
RUN make && make DESTDIR=/dist install

# Container
FROM busybox:musl
COPY --from=builds /dist /
COPY --from=builds /usr/share/zoneinfo /usr/share/zoneinfo
VOLUME /var/www/goaccess
EXPOSE 7890
ENTRYPOINT ["/bin/goaccess"]
CMD ["--help"]
