# Copyright 2019 Hiroki Kamino.
# Originally under MIT in Gerardo Orellana. https://goaccess.io/
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

FROM alpine:edge AS builds
RUN apk add --no-cache \
	autoconf \
	automake \
	build-base \
	clang \
	clang-static \
	cmake \
	gettext-dev \
	gettext-static \
	git \
	linux-headers \
	ncurses-dev \
	ncurses-static \
	rsync \
	tzdata \
	libmaxminddb-dev \
	libressl-dev \
	xz

# GoAccess
COPY . /goaccess
WORKDIR /goaccess
RUN autoreconf -fiv
RUN CC="clang" CFLAGS="-O3 -static" LIBS="$(pkg-config --libs openssl)" ./configure --prefix="" --enable-utf8 --with-openssl --enable-geoip=mmdb
RUN make && make DESTDIR=/dist install

# Time Zone
RUN tar Jcf /dist/tzdata.tar.xz -C /usr/share/zoneinfo/right .

# Container
FROM busybox:musl
COPY --from=builds /dist /
VOLUME /var/www/goaccess
EXPOSE 7890
ENTRYPOINT ["/bin/goaccess"]
CMD ["--help"]
