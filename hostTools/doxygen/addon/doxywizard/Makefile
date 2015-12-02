#
# This file was generated from Makefile.in on Sat Jun 10 21:08:33 CEST 2006
#

DOXYGEN   = /home/dimitri/doxygen
TMAKEPATH = /home/dimitri/doxygen/tmake/lib/linux-g++
ENV       = env TMAKEPATH=$(TMAKEPATH)
TMAKE     = /home/dimitri/doxygen/tmake/bin/tmake
MAKE      = /usr/bin/gmake
PERL      = /usr/bin/perl
RM        = rm -f
CP        = cp
VERSION   = 1.4.6
INSTALL   = /usr/local
INSTTOOL  = /usr/bin/install
DOXYDOCS  = ..
DOCDIR    = /usr/local/share/doc/packages/doxygen
QTDIR     = /usr/qt/3
HAVE_DOT  = /usr/local/bin/dot
#
# $Id: Makefile,v 1.3 2015/08/20 02:25:01 dana_tseng Exp $
#
# Copyright (C) 1997-2006 by Dimitri van Heesch.
# 
# Permission to use, copy, modify, and distribute this software and its
# documentation under the terms of the GNU General Public License is hereby 
# granted. No representations are made about the suitability of this software 
# for any purpose. It is provided "as is" without express or implied warranty.
# See the GNU General Public License for more details.
# 

all: config.h config.l version.cpp Makefile.doxywizard  
	$(MAKE) -f Makefile.doxywizard QTDIR=$(QTDIR) $@

config.h: ../../src/config.h
	$(CP) ../../src/config.h config.h

config.l: ../../src/config.l
	$(CP) ../../src/config.l config.l

version.cpp: ../../src/version.cpp
	$(CP) ../../src/version.cpp version.cpp

Makefile.doxywizard: doxywizard.pro 
	$(ENV) $(PERL) $(TMAKE) doxywizard.pro >Makefile.doxywizard

tmake: 
	$(ENV) $(PERL) $(TMAKE) doxywizard.pro >Makefile.doxywizard

clean: Makefile.doxywizard 
	$(MAKE) -f Makefile.doxywizard clean 
	$(RM) config.cpp

distclean: clean
	$(RM) Makefile.doxywizard config.l config.h version.cpp

install:
	$(INSTTOOL) -d $(INSTALL)/bin	
	$(INSTTOOL) -m 755 ../../bin/doxywizard $(INSTALL)/bin	

FORCE:
