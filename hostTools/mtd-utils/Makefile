
# -*- sh -*-

PREFIX = ..
ZLIBCPPFLAGS = -I$(PREFIX)/include
LZOCPPFLAGS = -I$(PREFIX)/include
ZLIBLDFLAGS = -L$(PREFIX)/lib
LZOLDFLAGS = -L$(PREFIX)/lib
LDFLAGS += $(ZLIBLDFLAGS) $(LZOLDFLAGS)
CFLAGS = -O2 -g $(ZLIBCPPFLAGS) $(LZOCPPFLAGS)
KERNEL_DIR = ../../linux

CFLAGS += -I$(KERNEL_DIR)/include -I./include $(ZLIBCPPFLAGS) $(LZOCPPFLAGS)

ifeq ($(WITHOUT_XATTR), 1)
  CPPFLAGS += -DWITHOUT_XATTR
endif

SUBDIRS = ubi-utils mkfs.ubifs

TARGETS = 

# TARGETS = flash_erase flash_eraseall nanddump \
# 	flash_lock flash_unlock flash_info \
# 	flash_otp_info flash_otp_dump mtd_debug flashcp nandwrite nandtest 

SYMLINKS =

include common.mk

clean::
	-rm -f $(SYMLINKS)
ifneq ($(BUILDDIR)/.git,)
ifneq ($(BUILDDIR),.)
ifneq ($(BUILDDIR),$(PWD))
	rm -rf $(BUILDDIR)
endif
endif
endif

$(SYMLINKS):
	ln -sf ../fs/jffs2/$@ $@

$(BUILDDIR)/mkfs.jffs2: $(addprefix $(BUILDDIR)/,\
	crc32.o compr_rtime.o mkfs.jffs2.o compr_zlib.o compr_lzo.o \
	compr.o rbtree.o)
LDFLAGS_mkfs.jffs2 = $(ZLIBLDFLAGS) $(LZOLDFLAGS)
LDLIBS_mkfs.jffs2  = -lz -llzo2

$(BUILDDIR)/flash_eraseall: $(BUILDDIR)/crc32.o $(BUILDDIR)/flash_eraseall.o

$(BUILDDIR)/jffs2reader: $(BUILDDIR)/jffs2reader.o
LDFLAGS_jffs2reader = $(ZLIBLDFLAGS) $(LZOLDFLAGS)
LDLIBS_jffs2reader  = -lz -llzo2

$(BUILDDIR)/jffs2dump: $(BUILDDIR)/jffs2dump.o $(BUILDDIR)/crc32.o

$(BUILDDIR)/sumtool: $(BUILDDIR)/sumtool.o $(BUILDDIR)/crc32.o

$(BUILDDIR)/serve_image: $(BUILDDIR)/serve_image.o $(BUILDDIR)/crc32.o $(BUILDDIR)/fec.o

$(BUILDDIR)/recv_image: $(BUILDDIR)/recv_image.o $(BUILDDIR)/crc32.o $(BUILDDIR)/fec.o

$(BUILDDIR)/fectest: $(BUILDDIR)/fectest.o $(BUILDDIR)/crc32.o $(BUILDDIR)/fec.o



ifneq ($(strip $(TARGETS)),)
install:: ${TARGETS}
	install -m 0755 ${TARGETS} ${INSTALL_DIR}/bin 
	mkdir -p ${INSTALL_DIR}/${MANDIR}/man1
	gzip -9c mkfs.jffs2.1 > ${INSTALL_DIR}/${MANDIR}/man1/mkfs.jffs2.1.gz
else
install::

endif

