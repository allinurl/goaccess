GoAccess [![Build Status](https://travis-ci.org/allinurl/goaccess.svg?branch=master)](http://travis-ci.org/allinurl/goaccess)
========

## What is it? ##
GoAccess is an open source **real-time web log analyzer** and interactive
viewer that **runs in a terminal in *nix systems**. It provides fast and
valuable HTTP statistics for system administrators that require a visual server
report on the fly.
More info at: [http://goaccess.io](http://goaccess.io/?src=gh).

![GoAccess Main Dashboard](http://goaccess.io/images/goaccess_screenshot1M-03L.png?1406550836)

## Features ##
GoAccess parses the specified web log file and outputs the data to the X
terminal. Features include:

* General Statistics, bandwidth, etc.
* Time taken to serve the request (useful to track pages that are slowing down your site)
* Top Visitors
* Requested files
* Requested static files, images, swf, js, etc.
* 404 or Not Found
* Hosts, Reverse DNS, IP Location
* Operating Systems
* Browsers and Spiders
* Referring Sites
* Referrers URLs
* Keyphrases
* Geo Location - Continent/Country/City
* HTTP Status Codes
* Ability to output JSON and CSV
* Different Color Schemes
* Support for large datasets and data persistence
* Support for IPv6
* Output statistics to HTML. See [report](http://goaccess.io/goaccess_html_report.html?src=gh).

### Nearly all web log formats... ###
GoAccess allows any custom log format string. Predefined options include, but
not limited to:

* Common Log Format (CLF) Apache
* Combined Log Format (XLF/ELF) Apache | Nginx
* W3C format (IIS).
* Amazon CloudFront (Download Distribution).
* Apache virtual hosts

## Why GoAccess? ##
The main idea behind GoAccess is being able to quickly analyze and view web
server statistics in real time without having to generate an HTML report.
Although it is possible to generate an `HTML`, `JSON`, `CSV` report, by default
it outputs to a terminal.

You can see it more as a monitor command tool than anything else.

## Installation ##
GoAccess can be compiled and used on Linux, OSX, OpenBSD, NetBSD, FreeBSD.

Download, extract and compile GoAccess with:

    $ wget http://tar.goaccess.io/goaccess-0.8.3.tar.gz
    $ tar -xzvf goaccess-0.8.3.tar.gz
    $ cd goaccess-0.8.3/
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

## Distributions ##

It is easiest to install GoAccess on Linux using the preferred package manager
of your Linux distribution.

Please note that not all distributions will have the lastest version of
GoAccess available

### Debian/Ubuntu ###

    # apt-get install goaccess

**NOTE:** this might not always give you the latest stable version. To make
sure that you're running the latest stable version of GoAccess see alternative
option below.

#### GoAccess' Debian & Ubuntu repository ####

    $ echo "deb http://deb.goaccess.io $(lsb_release -cs) main" | sudo tee -a /etc/apt/sources.list
    $ wget -O - http://deb.goaccess.io/gnugpg.key | sudo apt-key add -
    $ sudo apt-get update
    $ sudo apt-get install goaccess

***Important*** If APT complains about the public key not being available,
"signatures couldn't be verified because the public key is not available",
please re-download the key.

### Fedora ###

    # yum install goaccess

### Arch Linux ###

    # pacman -S goaccess

### Gentoo ###

    # emerge net-analyzer/goaccess

### OS X / Homebrew ###

    # brew install goaccess

### FreeBSD ###

    # cd /usr/ports/sysutils/goaccess/ && make install clean
    $ pkg_add -r goaccess

## Command Line / Config Options ##
The following options can also be supplied to the command or specified in the
configuration file:

| Command Line Option            | Description                                                                       |
| -------------------------------|-----------------------------------------------------------------------------------|
| `-a --agent-list`              | Enable a list of user-agents by host.                                             |
| `-c --config-dialog`           | Prompt log/date configuration window.                                             |
| `-d --with-output-resolver`    | Enable IP resolver on HTML|JSON output.                                           |
| `-e --exclude-ip=<IP>`         | Exclude one or multiple IPv4/v6 including IP ranges.                              |
| `-f --log-file=<filename>`     | Path to input log file.                                                           |
| `-g --std-geoip`               | Standard GeoIP database for less memory usage.                                    |
| `-h --help`                    | This help.                                                                        |
| `-H --http-protocol `          | Include HTTP request protocol if found.                                           |
| `-M --http-method`             | Include HTTP request method if found.                                             |
| `-m --with-mouse `             | Enable mouse support on main dashboard.                                           |
| `-o --output-format=csv,json`  | Output format: `-o csv` for CSV.  `-o json` for JSON.                             |
| `-p --config-file=<filename>`  | Custom configuration file.                                                        |
| `-q --no-query-string`         | Ignore request's query string.                                                    |
| `-r --no-term-resolver`        | Disable IP resolver on terminal output.                                           |
| `-s --storage`                 | Display current storage method. i.e., B+ Tree, Hash.                              |
| `-V --version`                 | Display version information and exit.                                             |
| `--static-file=<extension>`    | Add static file extension. e.g.: .mp3, Case sensitive.                            |
| `--geoip-city-data=<path>`     | Specify path to GeoIP City database file. i.e., GeoLiteCity.dat                   |
| `--db-path=<path>`             | Path of the database file. [/tmp/]                                                |
| `--xmmap=<number>`             | Set the size in bytes of the extra mapped memory. [0]                             |
| `--cache-lcnum=<number>`       | Max number of leaf nodes to be cached. [1024]                                     |
| `--cache-ncnum=<number>`       | Max number of non-leaf nodes to be cached. [512]                                  |
| `--tune-lmemb=<number>`        | Number of members in each leaf page. [128]                                        |
| `--tune-nmemb=<number>`        | Number of members in each non-leaf page. [256]                                    |
| `--tune-bnum=<number>`         | Number of elements of the bucket array. [32749]                                   |
| `--compression=<zlib,bz2>`     | Specifies that each page is compressed with ZLIB|BZ2 encoding.                    |
| `--ignore-crawlers`            | Ignore crawlers.                                                                  |
| `--no-color`                   | Disable colored output.                                                           |
| `--no-global-config`           | Do not load the global configuration file.                                        |
| `--no-progress`                | Disable progress metrics.                                                         |
| `--real-os`                    | Display real OS names. e.g, Windows XP, Snow Leopard.                             |
| `--color-scheme=<1,2>`         | Color schemes: `1 => Default grey scheme`, `2 => Green scheme`                    |
| `--date-format=<dateformat>`   | Specify log date format.                                                          |
| `--log-format="<logformat>"`   | Specify log format. Inner quotes need to be escaped.                              |

## Usage ##

The simplest and fastest usage would be:

    # goaccess -f access.log
That will generate an interactive text-only output.

To generate full statistics we can run GoAccess as:

    # goaccess -f access.log -a
To generate an HTML report:

    # goaccess -f access.log -a > report.html
To generate a JSON file:

    # goaccess -f access.log -a -d -o json > report.json
To generate a CSV file:

    # goaccess -f access.log -o csv > report.csv

The `-a` flag indicates that we want to process an agent-list for every host
parsed.

The `-d` flag indicates that we want to enable the IP resolver on the HTML |
JSON output. (It will take longer time to output since it has to resolve all
queries.)

The `-c` flag will prompt the date and log format configuration window. Only
when curses is initialized.

Now if we want to add more flexibility to GoAccess, we can do a series of
pipes. For instance:

If we would like to process all `access.log.*.gz` we can do:

    # zcat access.log.*.gz | goaccess
    OR
    # zcat -f access.log* | goaccess

Another useful pipe would be filtering dates out of the web log

The following will get all HTTP requests starting on 05/Dec/2010 until the end
of the file.

    # sed -n '/05\/Dec\/2010/,$ p' access.log | goaccess -a

To exclude a list of virtual hosts you can do the following:

    # grep -v "`cat exclude_vhost_list_file`" vhost_access.log | goaccess

For more examples, please check GoAccess' man page:
http://goaccess.io/man

## Contributing ##

Any help on GoAccess is welcome. Feel free to use the Github issue tracker and
pull requests to discuss and submit code changes.

Enjoy!
