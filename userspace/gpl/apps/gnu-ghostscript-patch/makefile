all: config
	make -f Makefile

config: config.status

config.status:
	( \
	export CC=mips-linux-uclibc-gcc ;\
	export CFLAGS="-fPIC -Os -g -fomit-frame-pointer -Wall -I$(APPS_STAGING_DIR)/include -Wl,-L$(APPS_STAGING_DIR)/lib" ;\
	export LDFLAGS="-L$(APPS_STAGING_DIR)/lib" ;\
	export LIBS="-lcupsimage -lcups -ldbus-1 -lssl -lcrypto -lz -lpthread -lm -lcrypt -ljpeg -lpng $(AVAHI_LIBS)" ;\
	export CUPSCONFIG="$(APPS_STAGING_DIR)/bin/cups-config" ;\
	./configure \
	--prefix=$(APPS_STAGING_DIR) \
	--exec-prefix=$(APPS_STAGING_DIR) \
	--build=x86 \
	--host=mips-linux-uclibc \
	--with-png=shared \
	--without-x \
	--disable-fontconfig \
	--disable-cairo \
	--disable-gtk \
	)

configlocal:
	( \
	export CC=mips-linux-uclibc-gcc ;\
	export CFLAGS="-fPIC -Os -g -fomit-frame-pointer -Wall -I$(APPS_STAGING_DIR)/include -Wl,-L$(APPS_STAGING_DIR)/lib" ;\
	export LDFLAGS="-L$(APPS_STAGING_DIR)/lib" ;\
	export LIBS="-lcupsimage -lcups -ldbus-1 -lssl -lcrypto -lz -lpthread -lm -lcrypt -ljpeg -lpng $(AVAHI_LIBS)" ;\
	export CUPSCONFIG="$(APPS_STAGING_DIR)/bin/cups-config" ;\
	./configure \
	--prefix=/usr/local \
	--exec-prefix=/usr/local \
	--build=x86 \
	--host=mips-linux-uclibc \
	--with-png=shared \
	--without-x \
	--disable-fontconfig \
	--disable-cairo \
	--disable-gtk \
	)

installocal:
	make -f Makefile prefix=$$PWD/usr/local exec_prefix=$$PWD/usr/local install

clean:
	make -f Makefile $@
distclean:
	make -f Makefile $@
all-am:
	make -f Makefile $@
install:
	make -f Makefile $@
install-exec-am:
	make -f Makefile $@
install-data-am:
	make -f Makefile $@
install-data-hook:
	make -f Makefile $@
