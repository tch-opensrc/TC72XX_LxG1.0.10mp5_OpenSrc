#! /bin/sh
#
#
# Dump device S.M.A.R.T. information

/usr/local/sbin/smartctl -H -i -A -f brief -l error -q noserial -d sat $1> /var/log/smartinfo.log

RETVAL=$?

echo info.sh returns $RETVAL

exit $RETVAL
