#!/bin/sh
if [ -d /apps ]; then
    if [ -f /apps/version.txt ]; then
	cat /apps/version.txt
    else
	echo "Version file does not exist"
    fi
else
    echo "Apps partition not mounted"
fi
exit 0