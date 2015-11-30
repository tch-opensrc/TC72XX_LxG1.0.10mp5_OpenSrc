#!/bin/sh
#
#
echo "Content-type: text/html"
echo ""
echo "<html><head><title>System Logs"
echo "</title></head><body>"
echo "<h1>System Logs</h1>"
echo ""
echo "<h1>Memory Info</h1>"
echo "<pre>$(cat /proc/meminfo)</pre>"
echo "<h1>Processes</h1>"
echo "<pre>$(ps)</pre>"
echo "<h1>Syslogs</h1>"
echo "<pre>$(cat /var/log/messages)</pre>"
echo "<h1>Kernel Logs:</h1>"
echo "<pre>$(dmesg)</pre>"
echo "<center>Information generated on $(date)</center>"
echo "</body></html>"
exit 0
