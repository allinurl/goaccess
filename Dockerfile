FROM alpine
LABEL maintainer "Julian Xhokaxhiu <info@julianxhokaxhiu.com>"

# Environment variables
#######################

ENV DATA_DIR /srv/data

# Configurable environment variables
####################################

# Copy required files and fix permissions
#########################################

COPY Dockerfile_src/* /opt/

# Create missing directories
############################

RUN mkdir -p $DATA_DIR \
    && mkdir -p /opt

# Set the work directory
########################

WORKDIR /opt

# Fix permissions
#################

RUN chmod 0644 * \
    && chmod 0755 *.sh

# Install required packages
##############################

RUN apk update \
    && apk add -u supervisor goaccess

# Cleanup
#########

RUN rm -rf /var/cache/apk/*

# Replace default configurations
################################

RUN rm /etc/supervisord.conf \
    && mv /opt/supervisord.conf /etc

# Allow redirection of stdout to docker logs
############################################

RUN ln -sf /proc/1/fd/1 /var/log/docker.log

# Expose required ports
#######################

EXPOSE 8080

# Set the entry point to init.sh
###########################################

ENTRYPOINT /opt/init.sh
