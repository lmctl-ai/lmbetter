#!/bin/sh
# TODO: harden this script
TARGET=$1
cd /srv/app
rm -rf $TARGET
eval "$USER_CMD"
FILES=`ls /tmp`
chmod 777 /srv/app/data
