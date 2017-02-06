GoAccess [![Build Status](https://travis-ci.org/allinurl/goaccess.svg?branch=master)](http://travis-ci.org/allinurl/goaccess) [![GoAccess](https://goaccess.io/badge?v1.0)](http://goaccess.io)
========

## What is it? ##
GoAccess is an open source **real-time web log analyzer** and interactive
viewer that **runs in a terminal in *nix systems** or through your browser. It
provides fast and valuable HTTP statistics for system administrators that
require a visual server report on the fly.
More info at: [http://goaccess.io](http://goaccess.io/?src=gh).

[![GoAccess Terminal Dashboard](http://goaccess.io/images/goaccess-real-time-term-gh.png?20160617000000)](https://goaccess.io/)
[![GoAccess HTML Dashboard](http://goaccess.io/images/goaccess-real-time-html-gh.png?20161123000000)](http://rt.goaccess.io/?src=gh)

## Features ##
GoAccess parses the specified web log file and outputs the data to the X
terminal. Features include:

* **Completely Real Time**
  All panels and metrics are timed to be updated every 200 ms on the terminal
  output and every second on the HTML output.

* **No configuration needed**
  You can just run it against your access log file, pick the log format and 
  let GoAccess parse the access log and show you the stats.

* **Track Application Response Time**
  Track the time taken to serve the request. Extremely useful if you want to
  track pages that are slowing down your site.

* **Nearly All Web Log Formats**
  GoAccess allows any custom log format string. Predefined options include,
  Apache, Nginx, Amazon S3, Elastic Load Balancing, CloudFront, etc 

* **Incremental Log Processing**
  Need data persistence? GoAccess has the ability to process logs incrementally
  through the on-disk B+Tree database.

* **Only one dependency**
  GoAccess is written in C. To run it, you only need ncurses as a dependency.
  That's it. It even features its own Web Socket server -  http://gwsocket.io/.

* **Visitors**
  Determine the amount of hits, visitors, bandwidth, and metrics for slowest
  running requests by the hour, or date.

* **Metrics per Virtual Host**
  Have multiple Virtual Hosts (Server Blocks)? A panel that displays which
  virtual host is consuming most of the web server resources.

* **Color Scheme Customizable**
  Tailor GoAccess to suit your own color taste/schemes. Either through the 
  terminal, or by simply applying the stylesheet on the HTML output.

* **Support for large datasets**
  GoAccess features an on-disk B+Tree storage for large datasets where it is not 
  possible to fit everything in memory.

* **Docker support**
  GoAccess comes with a default Docker that will listen for HTTP connections on port 8080.
  Although, you can still fully configure it, by using Volume mapping and editing `goaccess.conf`.

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
generate a complete real-time [**`HTML`**](http://rt.goaccess.io/?src=gh)
report, as well as a [**`JSON`**](http://goaccess.io/json?src=gh), and
[**`CSV`**](http://goaccess.io/goaccess_csv_report.csv?src=gh) report.

You can see it more of a monitor command tool than anything else.

## Installation ##
GoAccess can be compiled and used on *nix systems.

Download, extract and compile GoAccess with:

    $ wget http://tar.goaccess.io/goaccess-1.1.1.tar.gz
    $ tar -xzvf goaccess-1.1.1.tar.gz
    $ cd goaccess-1.1.1/
    $ ./configure --enable-geoip --enable-utf8
    $ make
    # make install

### Build from GitHub (Development) ###

    $ git clone https://github.com/allinurl/goaccess.git
    $ cd goaccess
    $ autoreconf -fiv
    $ ./configure --enable-geoip --enable-utf8
    $ make
    # make install

### Docker ###

```
docker run \
    --restart=always \
    -d \
    -p 80:8080 \
    -v "/home/user/data:/srv/data" \
    allinurl/goaccess
```

Inside your folder `/home/user/data` you can place your `goaccess.conf` file, which will be used by the Docker to configure goaccess.
If no configuration will be present, a new empty file will be created.

## Distributions ##

It is easiest to install GoAccess on Linux using the preferred package manager
of your Linux distribution.

Please note that not all distributions will have the lastest version of
GoAccess available

#### Debian/Ubuntu ####

    # apt-get install goaccess

**NOTE:** It is likely this will install an outdated version of GoAccess. To
make sure that you're running the latest stable version of GoAccess see
alternative option below.

#### Official GoAccess Debian & Ubuntu repository ####

    $ echo "deb http://deb.goaccess.io/ $(lsb_release -cs) main" | sudo tee -a /etc/apt/sources.list.d/goaccess.list
    $ wget -O - http://deb.goaccess.io/gnugpg.key | sudo apt-key add -
    $ sudo apt-get update
    $ sudo apt-get install goaccess
    
**Note**:
* For *on-disk* support (Trusty+ or Wheezy+), run: `sudo apt-get install goaccess-tcb`
* `.deb` packages in the official repo are available through https as well. You may need to install `apt-transport-https`.

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

#### OpenIndiana ####

    # pkg install goaccess

#### pkgsrc (NetBSD, Solaris, SmartOS, ...) ####

    # pkgin install goaccess

#### Windows ####

GoAccess can be used in Windows through Cygwin.

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
The following options can be supplied to the command or specified in the
configuration file. If specified in the configuration file, long options need
to be used without prepending `--`.

| Command Line Option                | Description                                                   |
| -----------------------------------|---------------------------------------------------------------|
| `-a --agent-list`                  | Enable a list of user-agents by host.                         |
| `-c --config-dialog`               | Prompt log/date configuration window.                         |
| `-d --with-output-resolver`        | Enable IP resolver on HTML|JSON output.                       |
| `-e --exclude-ip=<IP>`             | Exclude one or multiple IPv4/v6 including IP ranges.          |
| `-f --log-file=<filename>`         | Path to input log file.                                       |
| `-g --std-geoip`                   | Standard GeoIP database for less memory usage.                |
| `-h --help`                        | This help.                                                    |
| `-H --http-protocol=<yes|no>`      | Set/unset HTTP request protocol if found.                     |
| `-i --hl-header`                   | Color highlight active panel.                                 |
| `-M --http-method=<yes|no>`        | Set/unset HTTP request method if found.                       |
| `-m --with-mouse `                 | Enable mouse support on main dashboard.                       |
| `-o --output=<file.[html|csv|json>`| Output either an HTML, JSON or a CSV file.                    |
| `-p --config-file=<filename>`      | Custom configuration file.                                    |
| `-q --no-query-string`             | Remove request's query string. Can reduce mem usage.          |
| `-r --no-term-resolver`            | Disable IP resolver on terminal output.                       |
| `-s --storage`                     | Display current storage method. i.e., B+ Tree, Hash.          |
| `-V --version`                     | Display version information and exit.                         |
| `--444-as-404`                     | Treat non-standard status code 444 as 404.                    |
| `--4xx-to-unique-count`            | Add 4xx client errors to the unique visitors count.           |
| `--addr=<addr>`                    | Specify IP address to bind server to.                         |
| `--all-static-files`               | Include static files that contain a query string.             |
| `--cache-lcnum=<number>`           | Max number of leaf nodes to be cached. [1024]                 |
| `--cache-ncnum=<number>`           | Max number of non-leaf nodes to be cached. [512]              |
| `--color=<fg:bg[attrs, PANEL]>`    | Specify custom colors.                                        |
| `--color-scheme=<1|2|3>`           | Schemes: 1 => Grey, 2 => Green, 3 => Monokai.                 |
| `--compression=<zlib,bz2>`         | Each page is compressed with ZLIB|BZ2 encoding.               |
| `--crawlers-only`                  | Parse and display crawlers/bots only.                         |
| `--date-format=<dateformat>`       | Specify log date format.                                      |
| `--date-spec=<date|hr>`            | Date specificity (date default).                              |
| `--db-path=<path>`                 | Path of the database file. [/tmp/]                            |
| `--dcf`                            | Display the path of the default config file.                  |
| `--debug-file=<path>`              | Send all debug messages to the specified file.                |
| `--double-decode`                  | Decode double-encoded values.                                 |
| `--enable-panel=<PANEL>`           | Enable parsing and displaying the given panel.                |
| `--fifo-in=<path/file>`            | Path to read named pipe (FIFO).                               |
| `--fifo-out=<path/file>`           | Path to write named pipe (FIFO).                              |
| `--geoip-city-data=<path>`         | Same as using `--geoip-database`.                             |
| `--geoip-database=<path>`          | Path to GeoIP database v4/v6. i.e., GeoLiteCity.dat           |
| `--hour-spec=<hr|min>`             | Hour specificity (hr default).                                |
| `--html-custom-css=<path.css>`     | Specify a custom CSS file in the HTML report.                 |
| `--html-custom-js=<path.js>`       | Specify a custom JS file in the HTML report.                  |
| `--html-prefs=<JSON>`              | Set HTML report default preferences (JSON object).            |
| `--html-report-title`              | Set HTML report page title and header.                        |
| `--ignore-crawlers`                | Ignore crawlers.                                              |
| `--ignore-panel=<PANEL>`           | Ignore parsing and displaying the given panel.                |
| `--ignore-referer=<referer>`       | Ignore referers from being counted. Wildcards allowed.        |
| `--ignore-status=<CODE>`           | Ignore parsing the given status code(s).                      |
| `--invalid-requests=<filename>`    | Log invalid requests to the specified file.                   |
| `--json-pretty-print`              | Format JSON output using tabs and newlines.                   |
| `--keep-db-files`                  | Persist parsed data into disk.                                |
| `--load-from-disk`                 | Load previously stored data from disk.                        |
| `--log-format="<logformat>"`       | Specify log format. Inner quotes need to be escaped.          |
| `--max-items`                      | Maximum number of items to show per panel.                    |
| `--no-color`                       | Disable colored output.                                       |
| `--no-column-names`                | Don't write column names in term output.                      |
| `--no-csv-summary`                 | Disable summary metrics on the CSV output.                    |
| `--no-global-config`               | Do not load the global configuration file.                    |
| `--no-html-last-updated`           | Do not show the last updated field in the HTML report.        |
| `--no-progress`                    | Disable progress metrics.                                     |
| `--no-tab-scroll`                  | Disable scrolling through panels on TAB.                      |
| `--num-tests=<number>`             | Number of lines to test against the given log format.         |
| `--origin=<url>`                   | Ensure clients send stated origin header on WS handshake.     |
| `--port=<port>`                    | Specify the port to use.                                      |
| `--process-and-exit`               | Parse log and exit without outputting data.                   |
| `--real-os`                        | Display real OS names. e.g, Windows XP, Snow Leopard.         |
| `--real-time-html`                 | Enable real-time HTML output.                                 |
| `--sort-panel=PANEL,METRIC,ORDER`  | Sort panel on initial load. See manpage for metrics.          |
| `--ssl-cert=<path/cert.crt>`       | Path to TLS/SSL certificate.                                  |
| `--ssl-key=<path/priv.key>`        | Path to TLS/SSL private key.                                  |
| `--static-file=<extension>`        | Add static file extension. e.g.: .mp3, Case sensitive.        |
| `--time-format=<timeformat>`       | Specify log time format.                                      |
| `--tune-bnum=<number>`             | Number of elements of the bucket array. [32749]               |
| `--tune-lmemb=<number>`            | Number of members in each leaf page. [128]                    |
| `--tune-nmemb=<number>`            | Number of members in each non-leaf page. [256]                |
| `--ws-url=<[scheme://]url[:port]>` | URL to which the WebSocket server responds.                   |
| `--xmmap=<number>`                 | Set the size in bytes of the extra mapped memory. [0]         |

## Usage ##
##### Different Outputs #####

To output to a terminal and generate an interactive report:

    # goaccess -f access.log

To generate an HTML report:

    # goaccess -f access.log -a > report.html
    
To generate a JSON report:

    # goaccess -f access.log -a -d -o json > report.json
    
To generate a CSV file:

    # goaccess -f access.log --no-csv-summary -o csv > report.csv

The `-a` flag indicates that we want to process an agent-list for every host
parsed.

The `-d` flag indicates that we want to enable the IP resolver on the HTML |
JSON output. (It will take longer time to output since it has to resolve all
queries.)

The `-c` flag will prompt the date and log format configuration window. Only
when curses is initialized.

##### Multiple Log Files #####

Filtering can be done through the use of pipes. For instance, using grep to
filter specific data and then pipe the output into GoAccess. This adds a great
amount of flexibility to what GoAccess can display. For example:

If we would like to process all `access.log.*.gz` we can do one of the following:

    # zcat -f access.log* | goaccess

    # zcat access.log.*.gz | goaccess

Note: On Mac OS X, use `gunzip -c` instead of `zcat`.

##### Real Time HTML Output #####

GoAccess has the ability the output real-time data in the HTML report. You can
even email the HTML file since it is composed of a single file with no external
file dependencies, how neat is that!

The process of generating a real-time HTML report is very similar to the
process of creating a static report. Only `--real-time-html` is needed to make
it real-time.

**Note** that `--ws-url` is also required *IF* GoAccess is running on a
different machine than the one used to open the html report. See
[FAQ](https://goaccess.io/faq) for more details.

    # goaccess -f access.log -o /usr/share/nginx/html/your_site/report.html --real-time-html --ws-url=host

By default, GoAccess listens on port 7890, to use a different port other than
7890, you can specify it as (make sure the port is opened):

    # goaccess -f access.log -o report.html --real-time-html --ws-url=goaccess.io --port=9870

And to bind the WebSocket server to a different address other than 0.0.0.0, you
can specify it as:

    # goaccess -f access.log -o report.html --real-time-html --ws-url=goaccess.io --addr=127.0.0.1

**Note**: To output real time data over a TLS/SSL connection, you need to use
`--ssl-cert=<cert.crt>` and `--ssl-key=<priv.key>`.

##### Working with Dates #####

Another useful pipe would be filtering dates out of the web log

The following will get all HTTP requests starting on `05/Dec/2010` until the
end of the file.

    # sed -n '/05\/Dec\/2010/,$ p' access.log | goaccess -a

or using relative dates such as yesterdays or tomorrows day:

    # sed -n '/'$(date '+%d\/%b\/%Y' -d '1 week ago')'/,$ p' access.log | goaccess -a

If we want to parse only a certain time-frame from DATE a to DATE b, we can do:

    # sed -n '/5\/Nov\/2010/,/5\/Dec\/2010/ p' access.log | goaccess -a

##### Virtual Hosts #####

Assuming your log contains the virtual host field. For instance:

    vhost.io:80 8.8.4.4 - - [02/Mar/2016:08:14:04 -0600] "GET /shop HTTP/1.1" 200 615 "-" "Googlebot-Image/1.0"

And you would like to append the virtual host to the request in order to see
which virtual host the top urls belong to

    awk '$8=$1$8' access.log | goaccess -a

To exclude a list of virtual hosts you can do the following:

    # grep -v "`cat exclude_vhost_list_file`" vhost_access.log | goaccess

##### Files & Status Codes #####

To parse specific pages, e.g., page views, `html`, `htm`, `php`, etc. within a
request:

    # awk '$7~/\.html|\.htm|\.php/' access.log | goaccess

Note, `$7` is the request field for the common and combined log format,
(without Virtual Host), if your log includes Virtual Host, then you probably
want to use `$8` instead. It's best to check which field you are shooting for,
e.g.:

    # tail -10 access.log | awk '{print $8}'

Or to parse a specific status code, e.g., 500 (Internal Server Error):

    # awk '$9~/500/' access.log | goaccess

##### Server #####

Also, it is worth pointing out that if we want to run GoAccess at lower
priority, we can run it as:

    # nice -n 19 goaccess -f access.log -a

and if you don't want to install it on your server, you can still run it from
your local machine:

    # ssh root@server 'cat /var/log/apache2/access.log' | goaccess -a

##### Incremental Log Processing #####

GoAccess has the ability to process logs incrementally through the on-disk
B+Tree database. It works in the following way:

1. A data set must be persisted first with `--keep-db-files`, then the same
data set can be loaded with `--load-from-disk`.
2. If new data is passed (piped or through a log file), it will append it to
the original data set.
3. To preserve the data at all times, `--keep-db-files` must be used.
4. If `--load-from-disk` is used without `--keep-db-files`, database files will
be deleted upon closing the program.

###### Examples ######

    // last month access log
    # goaccess -f access.log.1 --keep-db-files

then, load it with

    // append this month access log, and preserve new data
    # goaccess -f access.log --load-from-disk --keep-db-files

To read persisted data only (without parsing new data)

    # goaccess --load-from-disk --keep-db-files

## Contributing ##

Any help on GoAccess is welcome. The most helpful way is to try it out and give
feedback. Feel free to use the Github issue tracker and pull requests to
discuss and submit code changes.

Enjoy!
