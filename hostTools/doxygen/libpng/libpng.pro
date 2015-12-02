#
# This file was generated from libpng.pro.in on Sat Jun 10 21:08:33 CEST 2006
#

TEMPLATE	= lib
CONFIG		= warn_on staticlib debug thread
HEADERS		= deflate.h \
		  infblock.h \
		  infcodes.h \
		  inffast.h \
		  inffixed.h \
		  inftrees.h \
		  infutil.h \
		  png.h \
		  pngasmrd.h \
		  pngconf.h \
		  trees.h \
		  zconf.h \
		  zlib.h \
		  zutil.h
SOURCES		= adler32.c \
		  compress.c \
		  crc32.c \
		  deflate.c \
		  gzio.c \
		  infblock.c \
		  infcodes.c \
		  inffast.c \
		  inflate.c \
		  inftrees.c \
		  infutil.c \
		  png.c \
		  pngerror.c \
		  pnggccrd.c \
		  pngget.c \
		  pngmem.c \
		  pngpread.c \
		  pngread.c \
		  pngrio.c \
		  pngrtran.c \
		  pngrutil.c \
		  pngset.c \
		  pngtrans.c \
		  pngvcrd.c \
		  pngwio.c \
		  pngwrite.c \
		  pngwtran.c \
		  pngwutil.c \
		  trees.c \
		  uncompr.c \
		  zutil.c
win32:INCLUDEPATH          += .
win32-g++:TMAKE_CFLAGS     += -D__CYGWIN__ -DALL_STATIC
DESTDIR                    =  ../lib
TARGET                     =  png
OBJECTS_DIR                =  ../objects

TMAKE_MOC = /usr/qt/3/bin/moc
