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
all: Makefile.libmd5 
	$(MAKE) -f Makefile.libmd5

clean: Makefile.libmd5 
	$(MAKE) -f Makefile.libmd5 clean

distclean: clean
	$(RM) -f Makefile.libmd5 libmd5.pro Makefile 

tmake:
	$(ENV) $(PERL) $(TMAKE) libmd5.pro >Makefile.libmd5

Makefile.libmd5: libmd5.pro
	$(ENV) $(PERL) $(TMAKE) libmd5.pro >Makefile.libmd5

install:
