GoAccess [![Build Status](https://travis-ci.org/allinurl/goaccess.svg?branch=master)](https://travis-ci.org/allinurl/goaccess) [![GoAccess](https://goaccess.io/badge?v1.0)](https://goaccess.io)
========

## What is it? ##
GoAccess is an open source **real-time web log analyzer** and interactive
viewer that runs in a **terminal** on &ast;nix systems or through your
**browser**. It provides **fast** and valuable HTTP statistics for system
administrators that require a visual server report on the fly.
More info at: [https://goaccess.io](https://goaccess.io/?src=gh).

[![GoAccess Terminal Dashboard](https://goaccess.io/images/goaccess-real-time-term-gh.png?20170307000000)](https://goaccess.io/)
[![GoAccess HTML Dashboard](https://goaccess.io/images/goaccess-real-time-html-gh.png?20181123000000)](https://rt.goaccess.io/?src=gh)

## Features ##
GoAccess parses the specified web log file and outputs the data to the X
terminal. Features include:

* **Completely Real Time**<br>
  All panels and metrics are timed to be updated every 200 ms on the terminal
  output and every second on the HTML output.

* **Minimal Configuration needed**<br>
  You can just run it against your access log file, pick the log format and let
  GoAccess parse the access log and show you the stats.

* **Track Application Response Time**<br>
  Track the time taken to serve the request. Extremely useful if you want to
  track pages that are slowing down your site.

* **Nearly All Web Log Formats**<br>
  GoAccess allows any custom log format string.  Predefined options include,
  Apache, Nginx, Amazon S3, Elastic Load Balancing, CloudFront, etc.

* **Incremental Log Processing**<br>
  Need data persistence? GoAccess has the ability to process logs incrementally
  through the on-disk B+Tree database. 

* **Only one dependency**<br>
  GoAccess is written in C. To run it, you only need ncurses as a dependency.
  That's it. It even features its own Web Socket server — http://gwsocket.io/.

* **Visitors**<br>
  Determine the amount of hits, visitors, bandwidth, and metrics for slowest
  running requests by the hour, or date.

* **Metrics per Virtual Host**<br>
  Have multiple Virtual Hosts (Server Blocks)? It features a panel that
  displays which virtual host is consuming most of the web server resources.

* **Color Scheme Customizable**<br>
  Tailor GoAccess to suit your own color taste/schemes. Either through the
  terminal, or by simply applying the stylesheet on the HTML output.

* **Support for Large Datasets**<br>
  GoAccess features an on-disk B+Tree storage for large datasets where it is not
  possible to fit everything in memory.

* **Docker Support**<br>
  Ability to build GoAccess' Docker image from upstream. You can still fully
  configure it, by using Volume mapping and editing `goaccess.conf`.  See
  [Docker](https://github.com/allinurl/goaccess#docker) section below.

### Nearly all web log formats... ###
GoAccess allows any custom log format string. Predefined options include, but
not limited to:

* Amazon CloudFront (Download Distribution).
* Amazon Simple Storage Service (S3)
* AWS Elastic Load Balancing
* Combined Log Format (XLF/ELF) Apache | Nginx
* Common Log Format (CLF) Apache
* Google Cloud Storage.
* Apache virtual hosts
* Squid Native Format.
* W3C format (IIS).

## Why GoAccess? ##
GoAccess was designed to be a fast, terminal-based log analyzer. Its core idea
is to quickly analyze and view web server statistics in real time without
needing to use your browser (_great if you want to do a quick analysis of your
access log via SSH, or if you simply love working in the terminal_).

While the terminal output is the default output, it has the capability to
generate a complete, self-contained, real-time [**`HTML`**](https://rt.goaccess.io/?src=gh)
report, as well as a [**`JSON`**](https://goaccess.io/json?src=gh), and
[**`CSV`**](https://goaccess.io/goaccess_csv_report.csv?src=gh) report.

You can see it more of a monitor command tool than anything else.

## Installation ##

### Build from release

GoAccess can be compiled and used on *nix systems.

Download, extract and compile GoAccess with:

    $ wget https://tar.goaccess.io/goaccess-1.3.tar.gz
    $ tar -xzvf goaccess-1.3.tar.gz
    $ cd goaccess-1.3/
    $ ./configure --enable-utf8 --enable-geoip=legacy
    $ make
    # make install

### Build from GitHub (Development) ###

    $ git clone https://github.com/allinurl/goaccess.git
    $ cd goaccess
    $ autoreconf -fiv
    $ ./configure --enable-utf8 --enable-geoip=legacy
    $ make
    # make install

### Distributions ###

It is easiest to install GoAccess on Linux using the preferred package manager
of your Linux distribution. Please note that not all distributions will have
the lastest version of GoAccess available.

#### Debian/Ubuntu ####

    # apt-get install goaccess

**Note:** It is likely this will install an outdated version of GoAccess. To
make sure that you're running the latest stable version of GoAccess see
alternative option below.

#### Official GoAccess Debian & Ubuntu repository ####

    $ echo "deb https://deb.goaccess.io/ $(lsb_release -cs) main" | sudo tee -a /etc/apt/sources.list.d/goaccess.list
    $ wget -O - https://deb.goaccess.io/gnugpg.key | sudo apt-key add -
    $ sudo apt-get update
    $ sudo apt-get install goaccess

**Note**:
* For *on-disk* support (Trusty+ or Wheezy+), run: `sudo apt-get install goaccess-tcb`
* `.deb` packages in the official repo are available through HTTPS as well. You may need to install `apt-transport-https`.

#### Fedora ####

    # yum install goaccess

#### Arch Linux ####

    # pacman -S goaccess

#### Gentoo ####

    # emerge net-analyzer/goaccess

#### OS X / Homebrew ####

    # brew install goaccess

#### FreeBSD ####

    # cd /usr/ports/sysutils/goaccess/ && make install clean
    # pkg install sysutils/goaccess

#### OpenBSD ####

    # cd /usr/ports/www/goaccess && make install clean
    # pkg_add goaccess

#### openSUSE  ####

    # zypper ar -f obs://server:http
    # zypper ref && zypper in goaccess

#### OpenIndiana ####

    # pkg install goaccess

#### pkgsrc (NetBSD, Solaris, SmartOS, ...) ####

    # pkgin install goaccess

#### Windows ####

GoAccess can be used in Windows through Cygwin.
See Cygwin's <a href="https://goaccess.io/faq#installation">packages</a>. Or
through the Linux Subsystem on Windows 10.

#### Distribution Packages ####

GoAccess has minimal requirements, it's written in C and requires only ncurses.
However, below is a table of some optional dependencies in some distros to
build GoAccess from source.

Distro                 | NCurses          | GeoIP (opt)      | Tokyo Cabinet (opt)      |  OpenSSL (opt)     | gettext
---------------------- | -----------------|------------------| -------------------------| -------------------| -------------------
**Ubuntu/Debian**      | libncursesw5-dev | libgeoip-dev     | libtokyocabinet-dev      |  libssl-dev        | gettext
**RHEL/CentOS** | ncurses-devel    | geoip-devel      | tokyocabinet-devel       |  openssl-devel     | gettext-devel
**Arch Linux**         | ncurses          | geoip            | [compile from source](https://goaccess.io/faq#installation)       |  openssl    | gettext
**Gentoo**             | sys-libs/ncurses | dev-libs/geoip   | dev-db/tokyocabinet      |  dev-libs/openssl  |
**Slackware**          | ncurses          | GeoIP            | tokyocabinet             |  openssl           |

#### Docker ####

A Docker image has been updated, capable of directing output from an access log. If you only want to output a report, you can pipe a log from the external environment to a Docker-based process:

    cat access.log | docker run --rm -i -e LANG=$LANG allinurl/goaccess -a -o html --log-format COMBINED - > report.html

OR real-time

    cat access.log | docker run -p 7890:7890 --rm -i -e LANG=$LANG allinurl/goaccess -a -o html --log-format COMBINED --real-time-html - > report.html

You can read more about using the docker image in [DOCKER.md](https://github.com/allinurl/goaccess/blob/master/DOCKER.md).

## Storage ##

There are three storage options that can be used with GoAccess. Choosing one
will depend on your environment and needs.

#### Default Hash Tables ####

In-memory storage provides better performance at the cost of limiting the
dataset size to the amount of available physical memory. By default GoAccess
uses in-memory hash tables. If your dataset can fit in memory, then this will
perform fine. It has very good memory usage and pretty good performance.

#### Tokyo Cabinet On-Disk B+ Tree ####

Use this storage method for large datasets where it is not possible to fit
everything in memory. The B+ tree database is slower than any of the hash
databases since data has to be committed to disk. However, using an SSD greatly
increases the performance. You may also use this storage method if you need
data persistence to quickly load statistics at a later date.

#### Tokyo Cabinet On-Memory Hash Database ####

An alternative to the default hash tables. It uses generic typing and thus it's
performance in terms of memory and speed is average.

## Command Line / Config Options ##
See [**options**](https://goaccess.io/man#options) that can be supplied to the command or
specified in the configuration file. If specified in the configuration file, long
options need to be used without prepending `--`.

## Usage / Examples ##
**Note**: Piping data into GoAccess won't prompt a log/date/time
configuration dialog, you will need to previously define it in your
configuration file or in the command line.

### Getting Started ###

To output to a terminal and generate an interactive report:

    # goaccess access.log

To generate an HTML report:

    # goaccess access.log -a > report.html
    
To generate a JSON report:

    # goaccess access.log -a -d -o json > report.json
    
To generate a CSV file:

    # goaccess access.log --no-csv-summary -o csv > report.csv

GoAccess also allows great flexibility for real-time filtering and parsing. For
instance, to quickly diagnose issues by monitoring logs since goaccess was
started:

    # tail -f access.log | goaccess -

And even better, to filter while maintaining opened a pipe to preserve
real-time analysis, we can make use of `tail -f` and a matching pattern tool
such as `grep`, `awk`, `sed`, etc:

    # tail -f access.log | grep -i --line-buffered 'firefox' | goaccess --log-format=COMBINED -

or to parse from the beginning of the file while maintaining the pipe opened
and applying a filter

    # tail -f -n +0 access.log | grep -i --line-buffered 'firefox' | goaccess -o report.html --real-time-html -


### Multiple Log files ###

There are several ways to parse multiple logs with GoAccess. The simplest is to
pass multiple log files to the command line:

    # goaccess access.log access.log.1

It's even possible to parse files from a pipe while reading regular files:

    # cat access.log.2 | goaccess access.log access.log.1 -

**Note**: the single dash is appended to the command line to let GoAccess
know that it should read from the pipe.

Now if we want to add more flexibility to GoAccess, we can do a series of
pipes. For instance, if we would like to process all compressed log files
access.log.*.gz in addition to the current log file, we can do:

    # zcat access.log.*.gz | goaccess access.log -

_Note_: On Mac OS X, use `gunzip -c` instead of `zcat`.

### Real-time HTML outputs ###

GoAccess has the ability the output real-time data in the HTML report. You can
even email the HTML file since it is composed of a single file with no external
file dependencies, how neat is that!

The process of generating a real-time HTML report is very similar to the
process of creating a static report. Only `--real-time-html` is needed to make
it real-time.

    # goaccess access.log -o /usr/share/nginx/html/your_site/report.html --real-time-html

To view the report you can navigate to `http://your_site/report.html`.

By default, GoAccess will use the host name of the generated report.
Optionally, you can specify the URL to which the client's browser will connect
to. See [FAQ](https://goaccess.io/faq) for a more detailed example.

    # goaccess access.log -o report.html --real-time-html --ws-url=goaccess.io

By default, GoAccess listens on port 7890, to use a different port other than
7890, you can specify it as (make sure the port is opened):

    # goaccess access.log -o report.html --real-time-html --port=9870

And to bind the WebSocket server to a different address other than 0.0.0.0, you
can specify it as:

    # goaccess access.log -o report.html --real-time-html --addr=127.0.0.1

**Note**: To output real time data over a TLS/SSL connection, you need to use
`--ssl-cert=<cert.crt>` and `--ssl-key=<priv.key>`.

### Filtering ###

#### Working with dates ####

Another useful pipe would be filtering dates out of the web log

The following will get all HTTP requests starting on `05/Dec/2010` until the
end of the file.

    # sed -n '/05\/Dec\/2010/,$ p' access.log | goaccess -a -

or using relative dates such as yesterdays or tomorrows day:

    # sed -n '/'$(date '+%d\/%b\/%Y' -d '1 week ago')'/,$ p' access.log | goaccess -a -

If we want to parse only a certain time-frame from DATE a to DATE b, we can do:

    # sed -n '/5\/Nov\/2010/,/5\/Dec\/2010/ p' access.log | goaccess -a -

#### Virtual hosts ####

Assuming your log contains the virtual host field. For instance:

    vhost.io:80 8.8.4.4 - - [02/Mar/2016:08:14:04 -0600] "GET /shop HTTP/1.1" 200 615 "-" "Googlebot-Image/1.0"

And you would like to append the virtual host to the request in order to see
which virtual host the top urls belong to:

    awk '$8=$1$8' access.log | goaccess -a -
    
To do the same, but also use real-time filtering and parsing:

    tail -f  access.log | unbuffer -p awk '$8=$1$8' | goaccess -a -

To exclude a list of virtual hosts you can do the following:

    # grep -v "`cat exclude_vhost_list_file`" vhost_access.log | goaccess -

#### Files, status codes and bots ####

To parse specific pages, e.g., page views, `html`, `htm`, `php`, etc. within a
request:

    # awk '$7~/\.html|\.htm|\.php/' access.log | goaccess -

Note, `$7` is the request field for the common and combined log format,
(without Virtual Host), if your log includes Virtual Host, then you probably
want to use `$8` instead. It's best to check which field you are shooting for,
e.g.:

    # tail -10 access.log | awk '{print $8}'

Or to parse a specific status code, e.g., 500 (Internal Server Error):

    # awk '$9~/500/' access.log | goaccess -

Or multiple status codes, e.g., all 3xx and 5xx:

    # tail -f -n +0 access.log | awk '$9~/3[0-9]{2}|5[0-9]{2}/' | goaccess -o out.html -

And to get an estimated overview of how many bots (crawlers) are hitting your server:

    # tail -F -n +0 access.log | grep -i --line-buffered 'bot' | goaccess -

### Tips ###

Also, it is worth pointing out that if we want to run GoAccess at lower
priority, we can run it as:

    # nice -n 19 goaccess -f access.log -a

and if you don't want to install it on your server, you can still run it from
your local machine!

    # ssh root@server 'cat /var/log/apache2/access.log' | goaccess -a -


#### Troubleshooting ####

We receive many questions and issues that have been answered previously.

* Date/time matching problems? Check that your log format and the system locale in which you run GoAccess match. See [#1571](https://github.com/allinurl/goaccess/issues/1571#issuecomment-543186858)
* Problems with pattern matching? Spaces are often a problem, see for instance [#136](https://github.com/allinurl/goaccess/issues/136), [#1579](https://github.com/allinurl/goaccess/issues/1579)
* Other issues matching log entries: See [>200 closed issues regarding log/date/time formats](https://github.com/allinurl/goaccess/issues?q=is%3Aissue+is%3Aclosed+label%3A%22log%2Fdate%2Ftime+format%22)
* Problems with log processing? See [>111 issues regarding log processing](https://github.com/allinurl/goaccess/issues?q=is%3Aissue+is%3Aclosed+label%3Alog-processing)


#### Incremental log processing ####

GoAccess has the ability to process logs incrementally through the on-disk
[B+Tree](https://github.com/allinurl/goaccess#storage) database. It works in
the following way:

1. A data set must be persisted first with `--keep-db-files`, then the same
data set can be loaded with `--load-from-disk`.
2. If new data is passed (piped or through a log file), it will append it to
the original data set.
3. To preserve the data at all times, `--keep-db-files` must be used.
4. If `--load-from-disk` is used without `--keep-db-files`, database files will
be deleted upon closing the program.

##### Examples #####

    // last month access log
    # goaccess access.log.1 --keep-db-files

then, load it with

    // append this month access log, and preserve new data
    # goaccess access.log --load-from-disk --keep-db-files

To read persisted data only (without parsing new data)

    # goaccess --load-from-disk --keep-db-files

## Contributing ##

Any help on GoAccess is welcome. The most helpful way is to try it out and give
feedback. Feel free to use the Github issue tracker and pull requests to
discuss and submit code changes.

Enjoy!
