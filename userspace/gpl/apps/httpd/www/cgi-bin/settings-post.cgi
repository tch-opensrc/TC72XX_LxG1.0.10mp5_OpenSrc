#!/bin/sh
echo "Content-type: text/html"
echo 
echo "<html>"
echo "<head>"
echo "<link rel="stylesheet" type="text/css" href="main.css" />"
echo "<title>"
echo "Result - Universal Services Gateway"
echo "</title>"
#echo "QUERY_STRING=$QUERY_STRING<br/>"
echo 
RETVAL=0
export PATH=/usr/local/sbin:/usr/local/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

if [ "$(echo "$QUERY_STRING" | sed -n 's/^.*Zeroconf=\([^&]*\).*$/\1/p'   )" = "yes" ]; then
    /usr/local/etc/init.d/avahi status > /dev/null 
    RETVAL=$?
    if [ $RETVAL -ne 0 ]; then
        sed -i s/AVAHI=no/AVAHI=yes/ /usr/local/etc/sysconfig/services
        /usr/local/etc/init.d/avahi start 2> /dev/null  
        RETVAL=$?
        if [ $RETVAL -ne 0 ]; then
            echo "<p>"
            echo "Zeroconf enable: Failed<br/>"
            echo "</p>"
        else
            echo "<p>"
            echo "Zeroconf enable: Sucessfull<br/>"
            echo "</p>"
        fi
    fi
else
    sed -i s/AVAHI=yes/AVAHI=no/ /usr/local/etc/sysconfig/services
    /usr/local/etc/init.d/avahi status > /dev/null 
    RETVAL=$?
    if [ $RETVAL -eq 0 ]; then
        /usr/local/etc/init.d/avahi stop 2> /dev/null
        RETVAL=$?
        if [ $RETVAL -ne 0 ]; then
            echo "<p>"
            echo "Zeroconf disable: Failed<br/>"
            echo "</p>"
        else
            echo "<p>"
            echo "Zeroconf disable: Sucessfull<br/>"
            echo "</p>"
        fi
    fi
fi

if [ "$(echo "$QUERY_STRING" | sed -n 's/^.*Firefly=\([^&]*\).*$/\1/p'    )" = "yes" ]; then 
    /usr/local/etc/init.d/mt-daap status > /dev/null 
    RETVAL=$?
    if [ $RETVAL -ne 0 ]; then
        sed -i s/FIREFLY=no/FIREFLY=yes/ /usr/local/etc/sysconfig/services
        /usr/local/etc/init.d/mt-daap start 2> /dev/null  
        RETVAL=$?
        if [ $RETVAL -ne 0 ]; then
            echo "<p>"
            echo "Firefly enable: Failed<br/>"
            echo "</p>"
        else
            echo "<p>"
            echo "Firefly enable: Sucessfull<br/>"
            echo "</p>"
        fi
    fi
else
    sed -i s/FIREFLY=yes/FIREFLY=no/ /usr/local/etc/sysconfig/services
    /usr/local/etc/init.d/mt-daap status > /dev/null 
    RETVAL=$?
    if [ $RETVAL -eq 0 ]; then
        /usr/local/etc/init.d/mt-daap stop 2> /dev/null
        RETVAL=$?
        if [ $RETVAL -ne 0 ]; then
            echo "<p>"
            echo "Firefly disable: Failed<br/>"
            echo "</p>"
        else
            echo "<p>"
            echo "Firefly disable: Sucessfull<br/>"
            echo "</p>"
        fi
    fi
fi
if [ "$(echo "$QUERY_STRING" | sed -n 's/^.*PrintServer=\([^&]*\).*$/\1/p')" = "yes" ]; then 
    /usr/local/etc/init.d/cups status > /dev/null  
    RETVAL=$?
    if [ $RETVAL -ne 0 ]; then
        sed -i s/CUPS=no/CUPS=yes/ /usr/local/etc/sysconfig/services
        /usr/local/etc/init.d/cups start 2> /dev/null  
        RETVAL=$?
        if [ $RETVAL -ne 0 ]; then
            echo "<p>"
            echo "Print Server enable: Failed<br/>"
            echo "</p>"
        else
            echo "<p>"
            echo "Print Server enable: Sucessfull<br/>"
            echo "</p>"
        fi
    fi
else
    sed -i s/CUPS=yes/CUPS=no/ /usr/local/etc/sysconfig/services
    /usr/local/etc/init.d/cups status > /dev/null  
    RETVAL=$?
    if [ $RETVAL -eq 0 ]; then
        /usr/local/etc/init.d/cups stop 2> /dev/null
        RETVAL=$?
        if [ $RETVAL -ne 0 ]; then
            echo "<p>"
            echo "Print Server disable: Failed<br/>"
            echo "</p>"
        else
            echo "<p>"
            echo "Print Server disable: Sucessfull<br/>"
            echo "</p>"
        fi
    fi
fi

echo "</html>"
exit 0

