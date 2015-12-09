#!/bin/sh

if [ -z "$1" ] || [ "$1" != "3384" -a "$1" != "93383LxG" ]; then
	echo "Usage: $0 3384/93383LxG !!!"
	exit 1
fi

TOOLCHAIN_TOP=/opt/toolchains/uclibc-crosstools-gcc-4.2.3-4
TOOLCHAIN_BIN=$TOOLCHAIN_TOP/usr/bin
TOOLCHAIN_BIN_PATH=`echo $PATH | grep "$TOOLCHAIN_BIN"`

if [ -z "$TOOLCHAIN_BIN_PATH" ]; then
	export PATH=$TOOLCHAIN_BIN:$PATH
fi

PROFILE=$1
echo "make PROFILE=$PROFILE $2"
make PROFILE=$PROFILE $2
