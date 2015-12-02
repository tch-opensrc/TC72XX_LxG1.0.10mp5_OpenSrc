#
# This file was generated from libmd5.pro.in on Sat Jun 10 21:08:33 CEST 2006
#

TEMPLATE	= lib
CONFIG		= warn_on staticlib debug thread
HEADERS		= md5.h md5_loc.h
SOURCES		= md5.c
win32:INCLUDEPATH          += .
win32-g++:TMAKE_CFLAGS     += -D__CYGWIN__ -DALL_STATIC
DESTDIR                    =  ../lib
TARGET                     =  md5
OBJECTS_DIR                =  ../objects

TMAKE_MOC = /usr/qt/3/bin/moc
