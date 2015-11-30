all: config
	make -f Makefile MANTOHTML=$$PWD/../cups-native/man/mantohtml GENSTRINGS=$$PWD/../cups-native/ppdc/genstrings

config: config.status

config.status:
	( \
	export CC=mips-linux-uclibc-gcc ;\
	export CFLAGS="-fPIC -Os -g -fomit-frame-pointer -Wall -I$(APPS_STAGING_DIR)/include" ;\
	export CXXFLAGS="-I/opt/toolchains/uclibc-crosstools-gcc-4.2.3-4/usr/include" ;\
	export LDFLAGS="-L$(APPS_STAGING_DIR)/lib" ;\
	export LIBS="-lpng -lz -ldbus-1" ;\
	export PKG_CONFIG_PATH=$(APPS_STAGING_DIR)/lib/pkgconfig; \
	./configure \
	--prefix=/usr/local \
	--exec-prefix=/usr/local \
	--libdir=/usr/local/lib \
	--disable-cdsassl \
	--disable-gnutls \
	--enable-openssl \
	--with-openssl-libs=$(APPS_STAGING_DIR)/lib \
	--with-openssl-includes=$(APPS_STAGING_DIR)/include \
	--build=x86 \
	--host=mips-linux-uclibc \
	--enable-avahi=$(patsubst ,no,$(patsubst n,no,$(patsubst y,yes,$(strip $(BUILD_AVAHI))))) \
	--disable-gssapi \
	--disable-libusb \
	--with-languages=en \
	--with-pdftops=/usr/local/bin/gs \
	)
	( \
	cat Makedefs | sed 's/^OPTIM.*/OPTIM\t\t=\t-D_GNU_SOURCE/' > foo ;\
	mv foo Makedefs \
	)

native:
	( \
	export -n AR ;\
	export -n AS ;\
	export CC=/usr/bin/gcc ;\
	export -n CPP ;\
	export -n CROSS_COMPILE ;\
	export CXX=/usr/bin/g++ ;\
	export -n EXTRALIBDIR ;\
	export -n LD ;\
	export -n LIBCDIR ;\
	export -n LIBDIR ;\
	export -n LIBOPT ;\
	export -n LIB_PATH ;\
	export -n MAKEFLAGS ;\
	export -n MAKELEVEL ;\
	export -n MAKEOVERRIDES ;\
	export -n MFLAGS ;\
	export -n NM ;\
	export -n OBJCOPY ;\
	export -n OBJDUMP ;\
	export -n RANLIB ;\
	export -n STRIP ;\
	export -n USRLIBDIR ;\
	./configure \
	--disable-gssapi \
	--disable-gnutls \
	)
	make -f Makefile
clean:
	make -f Makefile clean

distclean:
	make -f Makefile distclean

install:
	make -f Makefile MANTOHTML=$$PWD/../cups-native/man/mantohtml GENSTRINGS=$$PWD/../cups-native/ppdc/genstrings DSTROOT=$$PWD STRIPPROG=mips-linux-uclibc-strip install
