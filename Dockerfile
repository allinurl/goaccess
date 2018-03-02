# Builds a goaccess image from the current working directory:
FROM alpine:edge

COPY . /goaccess
WORKDIR /goaccess

ARG build_deps="build-base ncurses-dev autoconf automake git gettext-dev"
ARG runtime_deps="tini ncurses libintl gettext openssl-dev"

RUN apk update && \
    apk add -u $runtime_deps $build_deps && \
    autoreconf -fiv && \
    ./configure --enable-utf8 --with-openssl && \
    make && \
    make install && \
    apk del $build_deps && \
    rm -rf /var/cache/apk/* /tmp/goaccess/* /goaccess

VOLUME /srv/data
VOLUME /srv/logs
VOLUME /srv/report
EXPOSE 7890

ENTRYPOINT ["/sbin/tini", "--"]
CMD ["goaccess", "--no-global-config", "--config-file=/srv/data/goaccess.conf"]
