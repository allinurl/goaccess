#!/bin/bash
#
# Init script
#
###########################################################

# Thanks to http://stackoverflow.com/a/10467453
function sedeasy {
  sed -i "s/$(echo $1 | sed -e 's/\([[\/.*]\|\]\)/\\&/g')/$(echo $2 | sed -e 's/[\/&]/\\&/g')/g" $3
}

# Create empty goaccess configuration if it doesn't exist
if [ ! -f "$DATA_DIR/goaccess.conf" ]; then
  touch $DATA_DIR/goaccess.conf
fi

# Update configuration path inside supervisord.conf file
sedeasy "GOACCESS_CONF" "$DATA_DIR/goaccess.conf" "/etc/supervisord.conf"

# Fix permissions
find $DATA_DIR -type d -exec chmod 775 {} \;
find $DATA_DIR -type f -exec chmod 644 {} \;
chown -R root:root $DATA_DIR

# Start supervisor
supervisord -c /etc/supervisord.conf
