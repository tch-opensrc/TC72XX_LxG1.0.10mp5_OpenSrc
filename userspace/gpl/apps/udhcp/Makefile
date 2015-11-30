# udhcp makefile

#CROSS_COMPILE=mips-linux-
LDFLAGS = -Wl,--allow-shlib-undefined

prefix=/usr
SBINDIR=/sbin
USRSBINDIR=${prefix}/sbin
USRBINDIR=${prefix}/bin
USRSHAREDIR=${prefix}/share

# Uncomment this to get a shared binary. Call as udhcpd for the server,
# and udhcpc for the client
COMBINED_BINARY=1


INSTALL = install

VER := 0.9.6


OBJS_SHARED = options.o socket.o packet.o pidfile.o
DHCPD_OBJS = dhcpd.o arpping.o files.o leases.o serverpacket.o static_leases.o msgapi.o
DHCPC_OBJS = dhcpc.o clientpacket.o script.o

ifdef COMBINED_BINARY
# BRCM begin
AR1 = udhcpd.a
# BRCM end
EXEC1 = udhcpd
OBJS1 = $(DHCPD_OBJS) $(DHCPC_OBJS) $(OBJS_SHARED) frontend.o
CFLAGS += -DCOMBINED_BINARY
else
EXEC1 = udhcpd
OBJS1 = $(DHCPD_OBJS) $(OBJS_SHARED)

EXEC2 = udhcpc
OBJS2 = $(DHCPC_OBJS) $(OBJS_SHARED)
endif

EXEC3 = dumpleases
OBJS3 = dumpleases.o

BOOT_PROGRAMS = udhcpc
DAEMONS = udhcpd
COMMANDS = dumpleases

CFLAGS += -s -W -Wall -Wstrict-prototypes -DVERSION='"$(VER)"' -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) 
CFLAGS += -DBRCM_CMS_BUILD -DCMS_LOG3 -I$(BUILD_DIR)/userspace/public/include -I$(BUILD_DIR)/userspace/public/include/linux
CFLAGS += -I$(TOOLCHAIN)/include

LIBS=-Wl,-rpath,/lib:/lib/public:$(INSTALL_DIR)/lib:$(INSTALL_DIR)/lib/public -L$(INSTALL_DIR)/lib/public -lcms_util -lcms_msg

#
# Use between the following 2 lines depending on whether you want verbose debug or not.
#
#CFLAGS += -g -DVERBOSE
CFLAGS += -Os -fomit-frame-pointer


ifeq ($(strip $(BUILD_UDHCP)), static)
CFLAGS += -DBUILD_STATIC
endif

ifeq ($(strip $(BUILD_UDHCP_RELAY)), y)
CFLAGS += -DDHCP_RELAY
DHCPD_OBJS += relay.o
endif

# BRCM begin
#all: $(AR1)
all: $(EXEC1) $(EXEC2) $(EXEC3)
# BRCM end

$(OBJS1) $(OBJS2) $(OBJS3): *.h Makefile
$(EXEC1) $(EXEC2) $(EXEC3): Makefile

.c.o:
	$(CC) -c $(CFLAGS) $<

# BRCM begin
$(AR1): $(OBJS1)
	$(AR) cvr $(AR1) $(OBJS1)
# BRCM end

$(EXEC1): $(OBJS1)
	$(CC) $(LDFLAGS) $(OBJS1) -o $(EXEC1) $(LIBS)

$(EXEC2): $(OBJS2)
	$(CC) $(LDFLAGS) $(OBJS2) -o $(EXEC2)

$(EXEC3): $(OBJS3)
	$(CC) $(LDFLAGS) $(OBJS3) -o $(EXEC3)


install: 
	install -m 755 udhcpd $(INSTALL_DIR)/bin
	$(STRIP) $(INSTALL_DIR)/bin/udhcpd
	ln -sf udhcpd $(INSTALL_DIR)/bin/dhcpc
	ln -sf udhcpd $(INSTALL_DIR)/bin/dhcpd

dynamic: all install

static: $(AR1)

#	$(INSTALL) $(STRIP) $(DAEMONS) $(USRSBINDIR)
#	$(INSTALL) $(STRIP) $(COMMANDS) $(USRBINDIR)
#ifdef COMBINED_BINARY
#	ln -sf $(USRSBINDIR)/$(DAEMONS) $(SBINDIR)/$(BOOT_PROGRAMS)
#else
#	$(INSTALL) $(STRIP) $(BOOT_PROGRAMS) $(SBINDIR)
#endif
#	mkdir -p $(USRSHAREDIR)/udhcpc
#	for name in bound deconfig renew script ; do \
#		$(INSTALL) samples/sample.$$name \
#			$(USRSHAREDIR)/udhcpc/default.$$name ; \
#	done
#	mkdir -p $(USRSHAREDIR)/man/man1
#	$(INSTALL) dumpleases.1 $(USRSHAREDIR)/man/man1
#	mkdir -p $(USRSHAREDIR)/man/man5
#	$(INSTALL) udhcpd.conf.5 $(USRSHAREDIR)/man/man5
#	mkdir -p $(USRSHAREDIR)/man/man8
#	$(INSTALL) udhcpc.8 udhcpd.8 $(USRSHAREDIR)/man/man8

clean:
	-rm -f udhcpd udhcpc dumpleases *.o core

