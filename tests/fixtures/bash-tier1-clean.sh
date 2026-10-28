#!/bin/sh
# Hardened deploy script.
TARGET="${1:-/tmp/default}"
if [ -d /srv/app ]; then
  printf '%s\n' "entering /srv/app"
fi
rm -r "$TARGET/old"
FILES="$(ls /tmp)"
chmod 750 /srv/app/data
exit 0
