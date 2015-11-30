#! /bin/sh
#
#
#

/usr/local/sbin/smartctl -i -s on -d sat $1 | grep "SMART support is: Available"

RETVAL=$?

exit $RETVAL
