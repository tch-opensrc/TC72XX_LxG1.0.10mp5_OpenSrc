# rngd makefile

#CROSS_COMPILE=mips-linux-
LDFLAGS = -Wl,--allow-shlib-undefined

prefix=/usr
SBINDIR=/sbin
USRSBINDIR=${prefix}/sbin
USRBINDIR=${prefix}/bin
USRSHAREDIR=${prefix}/share

# Uncomment this to get a shared binary. Call as rngd for the server,
# and rngtest for the client
COMBINED_BINARY=1


INSTALL = install

VER := 0.9.6


OBJS_SHARED = fips.o
RNGD_OBJS = rngd_entsource.o rngd_linux.o rngd.o
RNGTEST_OBJS = stats.o rngtest.o

EXEC1 = rngd
OBJS1 = $(RNGD_OBJS) $(OBJS_SHARED)

EXEC2 = rngtest
OBJS2 = $(RNGTEST_OBJS) $(OBJS_SHARED)

DAEMONS = rngd
COMMANDS = rngtest

CFLAGS += -s -W -Wall -Wstrict-prototypes -DVERSION='"$(VER)"' -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) 
CFLAGS += -DBRCM_CMS_BUILD -DCMS_LOG3 -I$(BUILD_DIR)/userspace/public/include -I$(BUILD_DIR)/userspace/public/include/linux
CFLAGS += -I$(TOOLCHAIN)/include

LIBS=-Wl,-rpath,/lib:/lib/public:$(INSTALL_DIR)/lib:$(INSTALL_DIR)/lib/public -L$(INSTALL_DIR)/lib/public -lcms_util -lcms_msg

#
# Use between the following 2 lines depending on whether you want verbose debug or not.
#
#CFLAGS += -g -DVERBOSE
CFLAGS += -Os -fomit-frame-pointer -DHAVE_CONFIG_H

# BRCM begin
#all: $(AR1)
all: $(EXEC1) $(EXEC2)
# BRCM end

$(OBJS1) $(OBJS2) : *.h Makefile
$(EXEC1) $(EXEC2) : Makefile

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

install: 
	install -m 755 rngd $(INSTALL_DIR)/bin
	$(STRIP) $(INSTALL_DIR)/bin/rngd
	install -m 755 rngtest $(INSTALL_DIR)/bin
	$(STRIP) $(INSTALL_DIR)/bin/rngtest

dynamic: all install

static: $(AR1)

clean:
	-rm -f rngd rngtest *.o core

