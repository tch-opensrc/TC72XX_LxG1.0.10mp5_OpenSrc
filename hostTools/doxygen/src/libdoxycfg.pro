#
# This file was generated from libdoxycfg.pro.in on Sat Jun 10 21:08:33 CEST 2006
#

#
# $Id: libdoxycfg.pro,v 1.3 2015/08/20 02:25:06 dana_tseng Exp $
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
# TMake project file for doxygen

TEMPLATE     =	libdoxycfg.t
CONFIG       =	console warn_on staticlib debug thread
HEADERS      =  config.h	
SOURCES      =	config.cpp 
win32:TMAKE_CXXFLAGS       += -DQT_NODLL
win32-g++:TMAKE_CXXFLAGS   += -fno-exceptions -fno-rtti
INCLUDEPATH                += ../qtools
win32:INCLUDEPATH          += .
DESTDIR                    =  ../lib
TARGET                     =  doxycfg
OBJECTS_DIR                =  ../objects
TMAKE_MOC = /usr/qt/3/bin/moc
