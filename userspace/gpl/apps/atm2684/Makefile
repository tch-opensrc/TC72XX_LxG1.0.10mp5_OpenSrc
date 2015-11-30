#all:
#	cd atm; $(MAKE) $(MFLAGS) all
#	cp ./atm/lib/libatm.a ./br2684ctl
#	cd br2684ctl; $(MAKE) $(MFLAGS) 
libatm:
	cd atm; $(MAKE) $(MFLAGS) all
#	cp ./atm/lib/libatm.a ./br2684ctl

#install:
#	cd br2684ctl; $(MAKE) $(MFLAGS) dynamic
#	install -m 755 br2684ctl/br2684ctl $(INSTALL_DIR)/bin
#	$(STRIP) $(INSTALL_DIR)/bin/br2684ctl

dynamic: libatm install

#static: libatm 
#	cd br2684ctl; $(MAKE) $(MFLAGS) static

clean:
	rm -f `find . -name '*.[oas]' -print`
	rm -f `find . -name 'core' -print`
	rm -f `find . -name '*~' -print`
#	cd atm; $(MAKE) clean
#	cd br2684ctl; $(MAKE) clean

dist-clean:	clean
	rm -f Makefile `find . -name Makefile -print`
