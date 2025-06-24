# Build stage
FROM alpine:3.20 AS builds
RUN apk add --no-cache \
    autoconf \
    automake \
    build-base \
    clang \
    gettext-dev \
    libmaxminddb-dev \
    openssl-dev \
    linux-headers \
    ncurses-dev \
    pkgconf \
    tzdata

# GoAccess
COPY . /goaccess
WORKDIR /goaccess
RUN autoreconf -fiv && rm -rf autom4te.cache
RUN CC="clang" CFLAGS="-O3" LIBS="$(pkg-config --libs openssl)" ./configure --prefix=/usr --enable-utf8 --with-openssl --enable-geoip=mmdb
RUN make -j$(nproc) && make DESTDIR=/dist install

# Runtime stage
FROM alpine:3.20
RUN apk add --no-cache \
    libmaxminddb \
    ncurses-libs \
    openssl \
    tzdata
# Create non-root user
RUN adduser -D -u 1000 goaccess
USER goaccess
# Copy GoAccess binary and assets
COPY --from=builds /dist/usr/bin/goaccess /usr/bin/goaccess
COPY --from=builds /dist/usr/share /usr/share
COPY --from=builds /usr/share/zoneinfo /usr/share/zoneinfo
# Set up volume and port
VOLUME /var/www/goaccess
EXPOSE 7890
ENTRYPOINT ["/usr/bin/goaccess"]
CMD ["--help"]
