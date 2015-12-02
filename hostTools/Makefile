ifndef KERNEL_DIR
KERNEL_DIR = ../kernel/linux
endif
ifndef BRCM_BOARD
BRCM_BOARD = bcm963xx
endif
ifndef TARGETS_DIR
TARGETS_DIR = ../targets
endif
ifndef SHARED_DIR
SHARED_DIR = ../shared
endif
ifndef INC_BRCMBOARDPARMS_PATH
INC_BRCMBOARDPARMS_PATH = $(SHARED_DIR)/opensource/boardparms
endif

# host tools should not use cross compiler
unexport AR AS LD CC CPP CXX NM STRIP SSTRIP OBJCOPY OBJDUMP RANLIB

INCLUDEDIR = -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I.

CONFIG_SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	else if [ -x /bin/bash ]; then echo /bin/bash; \
	else echo sh; fi ; fi)
TOPDIR	:= $(shell /bin/pwd)
SSHD_DIR = $(BUILD_DIR)/userspace/public/apps/sshd
GENKEY_DIR = $(SSHD_DIR)/genkey
HOST_GENKEY = $(TOPDIR)/host_genkey
CC = gcc
STRIP = strip
RSA_KEY = $(TARGETS_DIR)/fs.src/etc/rsa_host_key
###DSS_KEY = $(TARGETS_DIR)/fs.src/etc/dss_host_key
export SSHD_DIR HOST_GENKEY CC STRIP

#WARNINGS= -Wall

#export DEFS		= -DDEBUG
CFLAGS_NOARCH	= $(DEFS) $(WARNINGS) -O2 -DGNU $(INCLUDEDIR)
CFLAGS		= $(CFLAGS_NOARCH)
ifeq ($(strip $(BUILD_SQUASH_HIGH)),y)
	CFLAGS += -DBUILD_SQUASH_HIGH
endif
ifeq ($(strip $(BUILD_SQUASH_NORMAL)),y)
	CFLAGS += -DBUILD_SQUASH_NORMAL
endif
ifeq ($(strip $(BUILD_SQUASH_LOW)),y)
	CFLAGS += -DBUILD_SQUASH_LOW
endif
ifeq ($(strip $(BRCM_IKOS)),y)
	CFLAGS += -DCONFIG_BRCM_IKOS
endif
export CFLAGS
#On RH 6+ there are new linking rules that do not automatically link dependant libraries
# so we need to check for that here to solve problems on newer build machines
ifeq ($(word 1, $(shell lsb_release)), LSB)
	DIST_VER:=$(shell lsb_release -sr)
	DIST_MAJOR_V:=$(word 1, $(subst ., ,$(DIST_VER)))
	DIST_MINOR_V:=$(word 2, $(subst ., ,$(DIST_VER))) 
endif
ifeq (($ shell expr $(DIST_MAJOR_V) ">=" 6), 1)
	export LDFLAGS = -L/lib64/ -ltinfo
endif


ifeq ($(strip $(BRCM_KERNEL_ROOTFS)),nfs)
BUILD_SET =
else
BUILD_SET = build_mkfs build_cmplzma build_hostgenkey build_imageutil fakeroot
endif


.PHONY: mklzma_sqfs mklzma_cmd $(BUILD_SET)
all: $(BUILD_SET)


#build_imageutil: bcmImageBuilder addvtoken createimg
build_imageutil:

bcmImageBuilder: bcmImageBuilder.c
	gcc $(CFLAGS) -DCONFIG_BCM9$(BRCM_CHIP) -o bcmImageBuilder bcmImageBuilder.c
addvtoken: addvtoken.c
	gcc $(CFLAGS) -o addvtoken addvtoken.c 
createimg: createimg.c $(INC_BRCMBOARDPARMS_PATH)/$(BRCM_BOARD)/boardparms.c
#	gcc $(CFLAGS) -DCONFIG_BCM9$(BRCM_CHIP) -o createimg createimg.c $(INC_BRCMBOARDPARMS_PATH)/$(BRCM_BOARD)/boardparms.c
	gcc $(CFLAGS) -DCONFIG_BCM9$(BRCM_CHIP) -o createimg createimg.c

ifeq ($(strip $(BRCM_KERNEL_ROOTFS)),squashfs)
ifneq ($(strip $(BRCM_ROOTFS_SQUASHFS_4_0)),)
build_mkfs: untar_lzma457 mksquashfs
else
build_mkfs: mksquashfs
endif
else
ifeq ($(strip $(BRCM_KERNEL_ROOTFS)),cramfs)
build_mkfs: mkcramfs $(LZMAOBJS)
else
ifeq ($(strip $(BRCM_KERNEL_ROOTFS)),jffs2)
build_mkfs: mkjffs2
else
ifeq ($(strip $(BRCM_KERNEL_ROOTFS)),ubifs)
build_mkfs: ubi-runtime mkfs.ubifs
endif
endif
endif
endif

ifneq ($(strip $(BRCM_ROOTFS_SQUASHFS_4_0)),)
SQUASHFS_VER="4.0"
SQUASHFS_DIR=$(TOPDIR)/squashfs_4.0
export Sqlzma=$(SQUASHFS_DIR)
export LzmaAlone=$(TOPDIR)/lzma457/CPP/7zip/Compress/LZMA_Alone
export LzmaC=$(TOPDIR)/lzma457/C/Compress/Lzma
else
SQUASHFS_VER="3.0"
SQUASHFS_DIR = $(TOPDIR)/squashfs
export Sqlzma= $(SQUASHFS_DIR)
export LzmaAlone= $(TOPDIR)/lzma/7zip/Compress/LZMA_Alone
export LzmaC= $(TOPDIR)/lzma/7zip/Compress/LZMA_C
endif
JFFS2_DIR = $(TOPDIR)/jffs2

untar_lzma457:
	if [ ! -e lzma457/lzma.txt ]; then \
	echo Untarring lzma 4.57 source...; \
	(tar xkfj lzma457.tar.bz2 2> /dev/null || true); \
	fi

mklzma_cmd:
	@echo building lzma host tool ...
	make -C $(LzmaAlone) -f makefile.gcc
	cp -f $(LzmaAlone)/lzma lzmacmd

mklzma_sqfs:
	@echo building lzma library and host tool for squashfs $(SQUASHFS_VER) ...
	make -C $(LzmaAlone) -f sqlzma.mk
	make -C $(LzmaC) -f sqlzma.mk

mksquashfs: mklzma_sqfs
	@echo building mksqushfs host tool ...
	make -C $(SQUASHFS_DIR)
	mv $(SQUASHFS_DIR)/mksquashfs mksquashfs

mkcramfs:  cramfs/mkcramfs.cpp $(LZMAOBJS)
	@if [ ! -e /usr/include/zlib.h ]; then \
          echo *****You need to install zlib-devel package to build mkcramfs! ; \
          echo *****You may find it in your Linux distribution CDs. ; \
	  exit ;\
        fi
	g++ $(CFLAGS) -I$(KERNEL_DIR)/include -c cramfs/mkcramfs.cpp
	g++ -o mkcramfs mkcramfs.o $(LZMAOBJS) -lz -lm

mkjffs2:
	@echo building mkjffs2 host tool ...
	make -C $(JFFS2_DIR)
	mv jffs2/mkfs.jffs2 .

export LIBUUID_DIR = $(TOPDIR)/util-linux-2.21
export ZLIB_DIR = $(TOPDIR)/zlib-1.2.5
export LZO_DIR = $(TOPDIR)/lzo-2.06

$(LIBUUID_DIR)/.configured:
	(tar -jkxf util-linux-2.21.tar.bz2 2> /dev/null || true)
	cd $(LIBUUID_DIR); ./configure --prefix=$(TOPDIR) --disable-libblkid --disable-libmount --disable-mount --disable-fsck \
		--disable-partx --disable-uuidd --disable-mountpoint --disable-fallocate --disable-unshare --disable-agetty --disable-cramfs \
		--disable-switch_root --disable-pivot_root --disable-kill --disable-rename --disable-schedutils --disable-wall --without-ncurses
	touch $@

libuuid: $(LIBUUID_DIR)/.configured
	@echo building libuuid ...
	make -C util-linux-2.21 install

$(ZLIB_DIR)/.configured:
	(tar -jkxf zlib-1.2.5.tar.bz2 2> /dev/null || true)
	cd $(ZLIB_DIR); ./configure --prefix=$(TOPDIR)
	touch $@

zlib: $(ZLIB_DIR)/.configured
	@echo building zlib ...
	make -C zlib-1.2.5 install

$(LZO_DIR)/.configured:
	(tar -jkxf lzo-2.06.tar.bz2 2> /dev/null || true)
	cd $(LZO_DIR); ./configure --prefix=$(TOPDIR)
	touch $@

lzo: $(LZO_DIR)/.configured
	@echo build lzo ...
	make -C lzo-2.06 install

mkfs.ubifs: libuuid zlib lzo
	@echo building mkfs.ubifs host tool ...
	cd mtd-utils && make clean
	cd mtd-utils && make 
	mv mtd-utils/mkfs.ubifs/mkfs.ubifs .

ubi-runtime:
	@echo building ubi runtime tools ...
	cd mtd-utils && make clean
	cd mtd-utils/ubi-utils && make CC=mips-linux-gcc
	cp mtd-utils/ubi-utils/ubimkvol $(INSTALL_DIR)/bin
	cp mtd-utils/ubi-utils/ubirmvol $(INSTALL_DIR)/bin
	cp mtd-utils/ubi-utils/ubinfo $(INSTALL_DIR)/bin

#build_cmplzma: mklzma_cmd cmplzma.cpp
#	g++ $(CFLAGS) -c cmplzma.cpp
#	g++  -o cmplzma cmplzma.o -lm
build_cmplzma: mklzma_cmd

ifeq ($(strip $(BUILD_SSHD)),y)
ifeq ($(strip $(BUILD_SSHD_MIPS_GENKEY)),y)
build_hostgenkey:
	@if [ -e $(RSA_KEY) ]; then \
		rm -fr $(RSA_KEY); \
	  exit ;\
        fi
###	@if [ -e $(DSS_KEY) ]; then \
###		rm -fr $(DSS_KEY); \
###	  exit ;\
###        fi
else
build_hostgenkey:	
	@if [ ! -e $(RSA_KEY) ]; then \
		cd $(GENKEY_DIR) && $(MAKE); \
		$(HOST_GENKEY) -t rsa -f $(RSA_KEY); \
	  exit ;\
        fi
###	@if [ ! -e $(DSS_KEY) ]; then \
###		$(HOST_GENKEY) -t dss -f $(DSS_KEY); \
###	  exit ;\
###     fi
endif
else
build_hostgenkey:
	@if [ -e $(RSA_KEY) ]; then \
		rm -fr $(RSA_KEY); \
	  exit ;\
        fi
###	@if [ -e $(DSS_KEY) ]; then \
###		rm -fr $(DSS_KEY); \
###	  exit ;\
###     fi
endif

$(LZMAOBJS): %.o: %.cpp
	g++ -c $(CFLAGS) $^ -o $@


export FAKEROOT_DIR = $(TOPDIR)/fakeroot

$(FAKEROOT_DIR)/.configured:
	(tar -jkxf fakeroot.tar.bz2 2> /dev/null || true)
	cd $(FAKEROOT_DIR); CFLAGS="$(CFLAGS_NOARCH)" ./configure --prefix=/usr --disable-nls
	touch $@

fakeroot: $(FAKEROOT_DIR)/.configured
	make -C $(FAKEROOT_DIR)
	sed -e 's,^PREFIX=.*,PREFIX=$(FAKEROOT_DIR),g' -e 's,^BINDIR=.*,BINDIR=$(FAKEROOT_DIR),g' -e 's,^PATHS=.*,PATHS=$(FAKEROOT_DIR)/.libs:/lib:/usr/lib,g' $(FAKEROOT_DIR)/scripts/fakeroot > $(FAKEROOT_DIR)/fakeroot
	chmod u+x $(FAKEROOT_DIR)/fakeroot


######################################################################
#
# This section deals with doxygen
#
# Note that if you want to change the settings in the config file,
# (Doxyfile), you should do it in this makefile.
# Any modifications to the config file itself will be
# wiped out next time you make the build_doxygen target.
#
######################################################################

doxygen:
	tar -jxf doxygen.tar.bz2
	cd doxygen; ./configure


build_doxygen: doxygen
	cd doxygen; make; \
	cd bin; rm -f Doxyfile; \
	./doxygen -g > /dev/null; \
	sed -i 's,\(PROJECT_NAME[ \t]*=\),\1 CMS,' Doxyfile; \
	sed -i 's,\(OUTPUT_DIRECTORY[ \t]*=\),\1 $(BUILD_DIR)/docs/doxygen,' Doxyfile; \
	sed -i 's,\(JAVADOC_AUTOBRIEF[ \t]*=\)[ \tA-Z]*,\1 YES,' Doxyfile; \
	sed -i 's,\(OPTIMIZE_OUTPUT_FOR_C[ \t]*=\)[ \tA-Z]*,\1 YES,' Doxyfile; \
	sed -i 's,\(WARN_NO_PARAMDOC[ \t]*=\)[ \tA-Z]*,\1 YES,' Doxyfile; \
	sed -i 's,\(ENUM_VALUES_PER_LINE[ \t]*=\)[ \t0-9]*,\1 1,' Doxyfile; \
	sed -i 's,\(INPUT[ \t]*=\),\1 $(BUILD_DIR)/userspace/public/include,' Doxyfile; \
	sed -i 's,\(INPUT[ \t]*=\),\1 $(BUILD_DIR)/userspace/private/include,' Doxyfile; \
	sed -i 's,\(INPUT[ \t]*=\),\1 $(BUILD_DIR)/userspace/private/libs/cms_core,' Doxyfile; \
	sed -i 's,\(RECURSIVE[ \t]*=\)[ \tA-Z]*,\1 YES,' Doxyfile; \
	sed -i 's,\(GENERATE_LATEX[ \t]*=\)[ \tA-Z]*,\1 NO,' Doxyfile; \
	sed -i 's,\(PREDEFINED[ \t]*=\)[ \tA-Z]*,\1 MDM_SHARED_MEM CMS_MEM_DEBUG,' Doxyfile;

# if we want to generate documentation for the unittests 
#sed -i 's,\(INPUT[ \t]*=\),\1 $(BUILD_DIR)/unittests,' Doxyfile; 

######################################################################
#
# Cleaning rules
#
######################################################################


clean:
	#rm -f bcmImageBuilder addvtoken createimg mksquashfs mkcramfs lzmacmd cmplzma $(HOST_GENKEY)
	rm -f mksquashfs mkcramfs lzmacmd $(HOST_GENKEY)
	rm -fr *.o core
	rm -fr $(GENKEY_DIR)/*.o
	make -C $(SQUASHFS_DIR) clean
	make -C $(LzmaAlone) -f sqlzma.mk clean
	make -C $(LzmaC) -f sqlzma.mk clean
	if [ -e $(FAKEROOT_DIR)/Makefile ];  then make -C $(FAKEROOT_DIR) distclean; fi
	rm -f $(FAKEROOT_DIR)/fakeroot
	rm -f $(FAKEROOT_DIR)/.configured
	make -C libcreduction clean
	rm -rf $(LIBUUID_DIR) $(ZLIB_DIR) $(LZO_DIR) bin sbin lib mkfs.ubifs

clean_doxygen:
	rm -fr doxygen
