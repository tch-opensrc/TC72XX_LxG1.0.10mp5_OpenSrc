#! /bin/sh
#
#

/usr/local/sbin/smartctl -H -d sat $1 | grep "SMART overall-health self-assessment test result:"
/usr/local/sbin/smartctl -H -d sat $1 | grep "SMART overall-health self-assessment test result:" | grep "FAILED"

RETVAL=$?

# FAILED 
[ $RETVAL -eq 0 ] && exit 1

# PASSED == ! FAILED
exit 0
