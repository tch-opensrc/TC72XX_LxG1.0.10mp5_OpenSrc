#
# This file was generated from doxytag.pro.in on Sat Jun 10 21:08:33 CEST 2006
#

# 
# $Id: doxytag.pro,v 1.3 2015/08/20 02:25:05 dana_tseng Exp $
#
# Copyright (C) 1997-2006 by Dimitri van Heesch.
# 
# Permission to use, copy, modify, and distribute this software and its
# documentation under the terms of the GNU General Public License is hereby 
# granted. No representations are made about the suitability of this software 
# for any purpose. It is provided "as is" without express or implied warranty.
# See the GNU General Public License for more details.
#
# Documents produced by Doxygen are derivative works derived from the
# input used in their production; they are not affected by this license.
#
# TMake project file for doxytag

TEMPLATE     =  doxytag.t
CONFIG       =	console warn_on debug thread
HEADERS      =  logos.h version.h
SOURCES      =	doxytag.cpp logos.cpp version.cpp 
unix:LIBS                  += -L../lib -lqtools
win32:INCLUDEPATH          += .
win32-mingw:LIBS           += -L../lib -lqtools
win32-msvc:LIBS            += qtools.lib shell32.lib 
win32-msvc:TMAKE_LFLAGS    += /LIBPATH:..\lib
win32-borland:LIBS         += qtools.lib shell32.lib
win32-borland:TMAKE_LFLAGS += -L..\lib -L$(BCB)\lib\psdk
win32:TMAKE_CXXFLAGS       += -DQT_NODLL
INCLUDEPATH                += ../qtools
OBJECTS_DIR                =  ../objects
TARGET                     =  ../bin/doxytag
TMAKE_MOC = /usr/qt/3/bin/moc
