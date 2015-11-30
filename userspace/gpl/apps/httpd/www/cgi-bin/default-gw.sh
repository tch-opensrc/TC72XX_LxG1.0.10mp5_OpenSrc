#!/bin/sh
#
#

ip route | awk '/default/ { print $3 }'
#echo 192.168.0.1 
