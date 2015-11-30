all: config
	make -f Makefile

config:
	(\
	echo "*******************************"; \
	echo "* Configuring PNG library *"; \
	echo "*******************************"; \
	export AS=mips-linux-uclibc-as; \
	export LD=mips-linux-uclibc-ld; \
	export CC=mips-linux-uclibc-gcc; \
	export CXX=mips-linux-uclibc-g++; \
	export AR=mips-linux-uclibc-ar; \
	export NM=mips-linux-uclibc-nm; \
	export STRIP=mips-linux-uclibc-strip; \
	export OBJCOPY=mips-linux-uclibc-objcopy; \
	export OBJDUMP=mips-linux-uclibc-objdump; \
	export RANLIB=mips-linux-uclibc-ranlib; \
	export CFLAGS="-fPIC -DPIC -O2 -I$(APPS_STAGING_DIR)/include"; \
	export LDFLAGS="-L$(APPS_STAGING_DIR)/lib" ;\
	export LIBS="-lz" ;\
	./configure --enable-shared --host=mips-linux-uclibc --prefix=$(APPS_STAGING_DIR) \
	)

clean:
	make -f Makefile clean
	rm -rf usr

distclean:
	make -f Makefile distclean
	rm -rf usr

install:
	make -f Makefile install

all-am:
	make -f Makefile all-am

install-exec-am:
	make -f Makefile install-exec-am

install-data-am:
	make -f Makefile install-data-am
install-exec-hook:
	make -f Makefile $@
install-data-hook:
	make -f Makefile $@

