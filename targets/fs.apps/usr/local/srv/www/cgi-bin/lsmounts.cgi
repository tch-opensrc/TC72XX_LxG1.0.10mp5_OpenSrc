#!/bin/sh
# Generates list of avaliable mountpoints for use with Firefly music directory configuration

# DLNA server creates a "downloads" folder in /mnt which is always empty, sed line removes this from list
ls -d /mnt/hd/* | sed 's_/mnt/hd/download_ _' > /apps/usr/local/share/mt-daapd/admin-root/dir.txt



echo "Content-type: text/html"
echo 

exit 0
