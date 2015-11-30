
DESTDIR=
# brcm begin
KERNEL_HEADERS=-I$(KERNEL_DIR)/include
# brcm end

INSTALL=/usr/bin/install -c

prefix=/usr/local
exec_prefix=${prefix}
bindir=${exec_prefix}/bin
sbindir=${exec_prefix}/sbin
mandir=${prefix}/share/man
distdir = $(PACKAGE)-$(VERSION)

SUBDIRS=libbridge brctl doc

all:
	for x in $(SUBDIRS); do $(MAKE) $(MFLAGS) -C $$x ; done

clean:
	for x in $(SUBDIRS); do $(MAKE) $(MFLAGS) -C $$x clean ; done

distclean:	clean
	rm -f config.log
	rm -f config.cache
	rm -f config.status

maintainer-clean: distclean
	rm -f Makefile
	rm -f brctl/Makefile
	rm -f libbridge/Makefile
	rm -f doc/Makefile

install:
	for x in $(SUBDIRS); do $(MAKE) $(MFLAGS) -C $$x install; done
# brcm begin
dynamic: all
	install -m 755 brctl/brctl $(INSTALL_DIR)/bin
	$(STRIP) $(INSTALL_DIR)/bin/brctl

static: all
	make -C brctl static
# brcm end	
