all: config
	make -f Makefile

config: config.status

config.status:
	( \
	export CXX=mips-linux-uclibc-g++ ;\
	export CXXFLAGS="-fPIC -Os -g -fomit-frame-pointer -Wall" ;\
	export CC=mips-linux-uclibc-gcc ;\
	export CFLAGS="-fPIC -Os -g -fomit-frame-pointer -Wall" ;\
	./configure \
	--prefix=$(APPS_STAGING_DIR) \
	--build=x86 \
	--host=mips-linux-uclibc \
	)

clean:
	make -f Makefile $@

distclean:
	make -f Makefile $@

install:
	make -f Makefile $@

all-recursive:
	make -f Makefile $@

all-am:
	make -f Makefile $@

install-am:
	make -f Makefile $@

install-exec-am:
	make -f Makefile $@

install-data-am:
	make -f Makefile $@
