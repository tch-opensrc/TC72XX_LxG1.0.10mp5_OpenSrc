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
all clean depend: Makefile.metrics 
	$(MAKE) -f Makefile.metrics $@

distclean: clean
	$(RM) -rf Makefile.metrics metrics.pro Makefile obj

tmake:
	$(ENV) $(PERL) $(TMAKE) metrics.pro >Makefile.metrics

Makefile.metrics: metrics.pro
	$(ENV) $(PERL) $(TMAKE) metrics.pro >Makefile.metrics

install:
