
# Docker image features

* This command uses the language set for this system. If that does not support it will be output in English. [**Supported Language**](https://github.com/allinurl/goaccess/raw/master/po/LINGUAS)

* This image supports building on the ARM architecture (e.g. Raspberry Pi)

* Do you want to change the timezone? Use the `-e` option to pass the time-zone setting to Docker. (e.g. `-e TZ="America/New_York"`)

* The container is built with geo-location support (see [the manual](https://goaccess.io/man#options)). To enable the respective panel, mount the geolocation database using `-v /path/to/GeoLite2-City.mmdb:/GeoLite2-City.mmdb` and specify `--geoip-database /GeoLite2-City.mmdb` when running GoAccess.

* If you made changes to the config file after building the image, you don't have to rebuild from scratch. Simply restart the container:

```
    docker restart goaccess
```

* If you had already run the container, you may have to stop and remove it first:

```
    docker stop goaccess
    docker rm goaccess
```

* The container and image can be completely removed as follows:

```
    docker stop goaccess
    docker rm goaccess
    docker rmi allinurl/goaccess
```
