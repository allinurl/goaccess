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
    musl-dev \
    tzdata

# GoAccess
COPY . /goaccess
WORKDIR /goaccess
RUN autoreconf -fiv && rm -rf autom4te.cache
# Configure with partial static linking (OpenSSL dynamic)
RUN CC="clang" CFLAGS="-O3" LDFLAGS="-static-libgcc" LIBS="$(pkg-config --libs openssl)" ./configure --prefix=/usr --enable-utf8 --with-openssl --enable-geoip=mmdb
RUN make -j$(nproc) && make DESTDIR=/dist install
# Check dynamic dependencies (expect only libssl.so, libcrypto.so, ld-musl)
RUN ldd /dist/usr/bin/goaccess && echo "Dependencies checked"

# Runtime stage
FROM alpine:3.20
RUN apk add --no-cache \
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
