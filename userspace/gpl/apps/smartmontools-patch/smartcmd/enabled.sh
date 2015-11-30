#! /bin/sh
#
#
#

/usr/local/sbin/smartctl -s on -d sat $1 | grep "SMART Enabled"

RETVAL=$?

exit $RETVAL
