#!/bin/sh
#
#

. /usr/local/etc/sysconfig/services

RETVAL=0
case $1 in 
    Zeroconf)
	/usr/local/etc/init.d/avahi status > /dev/null 
	RETVAL=$?
	;;
    Firefly)
	/usr/local/etc/init.d/mt-daap status > /dev/null 
	RETVAL=$?
	;;
    PrintServer)
	/usr/local/etc/init.d/cups status > /dev/null 
	RETVAL=$?
	;;
    *)
	RETVAL=1
	;;
esac

if [ $RETVAL -ne 0 ]; then
    echo $1=false 
else
    echo $1=true
fi
exit 0

