all: config
	make -f Makefile

config: config.status

config.status:
	( \
	export CXX=mips-linux-uclibc-g++ ;\
	export CXXFLAGS="-fPIC -Os -g -fomit-frame-pointer -Wall -I/opt/toolchains/uclibc-crosstools-gcc-4.2.3-4/usr/include -I$(APPS_STAGING_DIR)/include" ;\
	export CC=mips-linux-uclibc-gcc ;\
	export CFLAGS="-fPIC -Os -g -fomit-frame-pointer -Wall -I$(APPS_STAGING_DIR)/include" ;\
	export LDFLAGS="-L$(APPS_STAGING_DIR)/lib" ;\
	export LIBS="-lcupsimage -lcups -ldbus-1 -lssl -lcrypto -lz -lpthread -lm -lcrypt -ljpeg -lpng $(AVAHI_LIBS)" ;\
	./configure \
	--prefix=$(APPS_STAGING_DIR) \
	--exec-prefix=$(APPS_STAGING_DIR) \
	--with-cupsbackenddir=$(APPS_STAGING_DIR)/lib/cups/backend \
	--with-cupsfilterdir=$(APPS_STAGING_DIR)/lib/cups/filter \
	--build=x86 \
	--host=mips-linux-uclibc \
	--enable-hpcups-only-build \
	--disable-network-build \
	)


	
	
clean:
	make -f Makefile $@
distclean:
	make -f Makefile $@
install:
	make -f Makefile $@
install-exec-am:
	make -f Makefile $@
install-data-am:
	make -f Makefile $@
install-data-hook:
	make -f Makefile $@
