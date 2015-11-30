# ebtables Makefile

PROGNAME:=ebtables
PROGVERSION:=2.0.6
PROGDATE:=November\ 2003

MANDIR?=/usr/local/man
#BRCM begin
#CFLAGS:=-Wall -Wunused
CFLAGS:=-Wall -Wunused -Os -s
ifeq ($(strip $(BUILD_EBTABLES)), static)
CFLAGS += -DBUILD_STATIC
endif
#CC:=gcc
CFLAGS += $(BRCM_APP_CFLAGS)

#BRCM end

ifeq ($(shell uname -m),sparc64)
CFLAGS+=-DEBT_MIN_ALIGN=8 -DKERNEL_64_USERSPACE_32
endif

include extensions/Makefile

OBJECTS:=getethertype.o ebtables.o communication.o $(EXT_OBJS)

#KERNEL_INCLUDES?=include/
KERNEL_INCLUDES?=$(KERNEL_DIR)/include -I$(KERNEL_DIR)/arch/mips/include
KERNEL_INCLUDES += -I$(TOOLCHAIN)/include
#BRCM begin
KERNEL_INCLUDES += -I$(KERNEL_DIR)/arch/mips/include
#ETHERTYPESPATH?=/etc
ETHERTYPESPATH?=$(INSTALL_DIR)/etc
ETHERTYPESFILE:=$(ETHERTYPESPATH)/ethertypes

#BINPATH?=/sbin/
BINPATH?=$(INSTALL_DIR)/bin/
BINFILE:=$(BINPATH)ebtables

#BRCM end

PROGSPECS:=-DPROGVERSION=\"$(PROGVERSION)\" \
	-DPROGNAME=\"$(PROGNAME)\" \
	-DPROGDATE=\"$(PROGDATE)\" 
#BRCM begin
#	-D_PATH_ETHERTYPES=\"$(ETHERTYPESFILE)\"
#BRCM end


all: ebtables

communication.o: communication.c include/ebtables_u.h
	$(CC) $(CFLAGS) $(PROGSPECS) -c -o $@ $< -I$(KERNEL_INCLUDES)

getethertype.o: getethertype.c include/ethernetdb.h
	$(CC) $(CFLAGS) $(PROGSPECS) -c -o $@ $< -Iinclude/ -I$(TOOLCHAIN)/include

ebtables.o: ebtables.c include/ebtables_u.h
	$(CC) $(CFLAGS) $(PROGSPECS) -c -o $@ $< -I$(KERNEL_INCLUDES)
  
#BRCM begin
ether_addr.o: ether_addr.c
	$(CC) $(CFLAGS) $(PROGSPECS) -c -o $@ $< -I$(KERNEL_INCLUDES)
#BRCM end

ebtables: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ -I$(KERNEL_INCLUDES)

$(MANDIR)/man8/ebtables.8: ebtables.8
	mkdir -p $(@D)
#BRCM begin
	#install -m 0644 -o root -g root $< $@
	install -m 0644 $< $@
#BRCM end

$(ETHERTYPESFILE): ethertypes
	mkdir -p $(@D)
#BRCM begin
	#install -m 0644 -o root -g root $< $@
	install -m 0644 $< $@
#BRCM end


#BRCM begin
dynamic: install

static: ebtables.a

ebtables.a: $(OBJECTS)
	$(AR) rcs ebtables.a $(OBJECTS)
#BRCM end

.PHONY: exec
exec: ebtables
#BRCM begin
	#install -m 0755 -o root -g root $< $(BINFILE)
	install -m 0755 $< $(BINFILE)
#BRCM end

.PHONY: install
#BRCM begin
#install: $(MANDIR)/man8/ebtables.8 $(ETHERTYPESFILE) exec
install: $(ETHERTYPESFILE) exec
#BRCM end

.PHONY: clean
clean:
	rm -f ebtables
	rm -f *.o *.c~
	rm -f extensions/*.o extensions/*.c~

DIR:=$(PROGNAME)-v$(PROGVERSION)
# This is used to make a new userspace release
.PHONY: release
release:
	mkdir -p include/linux/netfilter_bridge
	install -m 0644 -o root -g root \
		$(KERNEL_INCLUDES)/linux/netfilter_bridge.h include/linux/
# To keep possible compile error complaints about undefined ETH_P_8021Q
# off my back
	install -m 0644 -o root -g root \
		$(KERNEL_INCLUDES)/linux/if_ether.h include/linux/
	install -m 0644 -o root -g root \
		$(KERNEL_INCLUDES)/linux/netfilter_bridge/*.h \
		include/linux/netfilter_bridge/
	install -m 0644 -o root -g root \
		include/ebtables.h include/linux/netfilter_bridge/
	make clean
	touch *
	touch extensions/*
	touch include/*
	touch include/linux/*
	touch include/linux/netfilter_bridge/*
	cd ..;tar -c $(DIR) | gzip >$(DIR).tar.gz
