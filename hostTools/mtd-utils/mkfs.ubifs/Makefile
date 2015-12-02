ALL_SOURCES=*.[ch] hashtable/*.[ch]

TARGETS = mkfs.ubifs

LDLIBS_mkfs.ubifs = -lz -llzo2 -lm -luuid

PREFIX = ../../../hostTools
ZLIBCPPFLAGS = -I$(PREFIX)/include
LZOCPPFLAGS = -I$(PREFIX)/include
LUUIDFLAGS = -I$(PREFIX)/include/uuid
ZLIBLDFLAGS = -L$(PREFIX)/lib
LZOLDFLAGS = -L$(PREFIX)/lib
LDFLAGS += $(ZLIBLDFLAGS) $(LZOLDFLAGS)
KERNEL_DIR = ../../../kernel/linux

CFLAGS += -I$(KERNEL_DIR)/include -I./include $(ZLIBCPPFLAGS) $(LZOCPPFLAGS) $(LUUIDFLAGS)

include ../common.mk

$(BUILDDIR)/mkfs.ubifs: $(addprefix $(BUILDDIR)/,\
	crc16.o crc32.o lpt.o compr.o devtable.o \
	hashtable/hashtable.o hashtable/hashtable_itr.o)

clean::
	rm -f $(BUILDDIR)/hashtable/*.o cscope.*

cscope:
	@echo $(ALL_SOURCES) > cscope.files
	@cscope -bR
	@rm cscope.files

install:: ${TARGETS}
	install -m 0755 ${TARGETS} $(TOPDIR)
