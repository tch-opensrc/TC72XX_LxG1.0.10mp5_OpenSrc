#!/bin/sh

TOOLCHAIN_TOP=/opt/toolchains/uclibc-crosstools-gcc-4.2.3-4
TOOLCHAIN_BIN=$TOOLCHAIN_TOP/usr/bin
TOOLCHAIN_BIN_PATH=`echo $PATH | grep "$TOOLCHAIN_BIN"`

if [ -z "$TOOLCHAIN_BIN_PATH" ]; then
	export PATH=$TOOLCHAIN_BIN:$PATH
fi

PROFILE=3384
echo "make PROFILE=$PROFILE $1"
make PROFILE=$PROFILE $1
