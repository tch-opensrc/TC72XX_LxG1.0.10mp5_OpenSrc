#!/bin/sh
# index.html
#
echo "Content-type: text/html"
echo
echo "<html>"
echo "<head>"
echo "<script language=\"javascript\">"
echo "<!-- hide me"
echo "var default_gw=\"http://$(./default-gw.sh )\";"
echo "location.replace(default_gw)"
echo "// show me -->"
echo "</script>"
echo 
echo "</head>"
echo "</html>"
exit 0
