# Docker-compose configuration

Here are two docker-compose configurations for goaccess which
combine a static site with a real-time report of goaccess.
The directories used are:

- `configs` - for the nginx and goaccess configuration
- `public` - the files served by nginx - put your static site here
- `logs` - nginx logs - there is no log rotation in place!

There are two flavors of the docker-compose files and the goaccess files: 
- `*.vanilla.*` which need you to take care of the TLS certificates
- `*.traefik.*` using traefik for TLS and domain routing

## Vanilla

For the vanilla version, you'll have to do the following:

- put the TLS certificates into `configs/certs`, following the naming scheme
in `goaccess.vanilla.conf`
- route requests for the static webpage to port `8080`

## Traefik

The traefik version is setup to make it easier to do the routing.
You don't need to take care of the TLS certificates, and the
goaccess websocket gets its own subdomain.

To put it all together, the following environment is needed:
- traefik configured according to [Traefik-101](https://ineiti.ch/posts/traefik-101/traefik-101/)
- DNS configuration for two domain names pointing to your server's IP:
  - `yourdomain` for the static pages, e.g., a blog-post using [Hugo](https://gohugo.io/)
  - `goaccess.yourdomain` for the stats with goaccess

