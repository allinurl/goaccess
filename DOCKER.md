
# Docker image features

* This command uses the language set for this system. If that does not support it will be output in English. [**Supported Language**](https://github.com/allinurl/goaccess/raw/master/po/LINGUAS)

* This image supports building on the ARM architecture (e.g. Raspberry Pi)

* Do you want to change the timezone? Use the `-e` option to pass the time-zone setting to Docker. (e.g. `-e TZ="America/New_York"`)

## A custom setup for Docker-based log analysis

The following example assumes you will store your GoAccess data below
`/srv/goaccess`, but you can use a different prefix if you like or if you run
as non-root user.

    mkdir -p /srv/goaccess/{data,html,logs}

Before running your own GoAccess Docker container, first create a config file
in `/srv/goaccess/data`. You can start one from scratch or use the one from
[`config/goaccess.conf`](https://raw.githubusercontent.com/allinurl/goaccess/master/config/goaccess.conf)
as a starting point and change it as needed.

A minimal config file for a real-time HTML report requires at least the
following options: `log-format`, `log-file`, `output` and `real-time-html`. For
example, for Apache's *combined* log format:

    log-format COMBINED
    log-file /goaccess/access.log
    output /goaccess/index.html
    real-time-html true

**(Note)**: The docker container doesn't use subfolders `/srv/{data,html,logs}` anymore. 
Goaccess container will now by default look for files inside its `/goaccess` folder.

If you want to expose goaccess on a different port on the host machine, you
*have to* set the `ws-url` option in the config file, e.g.:

    ws-url ws://example.com:8080

or for secured connections (TLS/SSL), please ensure your configuration file
contains the following lines:

    ssl-cert /srv/data/domain.crt
    ssl-key /srv/data/domain.key
    ws-url wss://example.com:8080

Once you have your configuration file all set, clone the repo:

    $ git clone https://github.com/allinurl/goaccess.git goaccess && cd $_

and then build and run the image as follows:

    docker build . -t allinurl/goaccess
    docker run --restart=always -d -p 7890:7890 \
      -v "/srv/goaccess/data:/goaccess" \
      -v "/srv/goaccess/html/index.html:/goaccess/index.html" \
      -v "/var/log/apache2/access.log:/goaccess/access.log" \
      --name=goaccess allinurl/goaccess \
      goaccess --no-global-config --config-file=/goaccess/goaccess.conf
      
If you made changes to the config file after building the image, you don't
have to rebuild from scratch. Simply restart the container:

    docker restart goaccess

And restart the container as follows:

    docker run --restart=always -d -p 8080:7890 \
      -v "/srv/goaccess/data:/srv/data"         \
      -v "/srv/goaccess/html:/srv/report"       \
      -v "/var/log/apache:/srv/logs"            \
      --name=goaccess allinurl/goaccess

If you had already run the container, you may have to stop and remove it first:

    docker stop goaccess
    docker rm goaccess

Note, it is possible to specify a different command and command line options to
run in the container directly on the docker command line, e.g.:

    docker run --restart=always -d -p 8080:7890 \
      -v "/srv/goaccess/data:/goaccess" \
      -v "/srv/goaccess/html/index.html:/goaccess/index.html" \
      -v "/var/log/apache/access.log:/goaccess/access.log" \
      --name=goaccess allinurl/goaccess \
      goaccess --no-global-config --config-file=/goaccess/goaccess.conf  \
               --ws-url=example.org:8080 --output=/goaccess/index.html \
               --log-file=/goaccess/access.log

The container and image can be completely removed as follows:

    docker stop goaccess
    docker rm goaccess
    docker rmi allinurl/goaccess

There is also a prebuilt [**docker
image**](https://hub.docker.com/r/allinurl/goaccess/) that can be run without
cloning the git repository:

    docker run --restart=always -d -p 8080:7890 \
        -v "/srv/goaccess/data:/goaccess" \
        -v "/srv/goaccess/html/index.html:/goaccess/index.html" \
        -v "/var/log/apache/access.log:/goaccess/access.log" \
        --name=goaccess allinurl/goaccess

