#!/bin/bash

if [ ! -f /var/lock/matt_daemon.lock ]; then
    echo "Lock file does not exist. Exiting."
    exit 1
fi

cat /var/lock/matt_daemon.lock 

sudo pkill -F /var/lock/matt_daemon.lock

sleep 1

sudo rm -rf /var/lock/matt_daemon.lock

if [ $? -ne 0 ]; then
    echo "Failed to remove lock file."
    exit 1
fi
echo "Lock file removed successfully."

exit 0