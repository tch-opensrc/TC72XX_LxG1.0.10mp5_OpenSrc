#
# build file 
#
#
CFLAGS = -s -Os -fomit-frame-pointer
LDFLAGS = -Wl,-allow-shlib-undefined
ifeq ($(strip $(BUILD_URLFILTER)), static)
CFLAGS += -DBUILD_STATIC 
endif

LIBS=$(BUILD_DIR)/userspace/gpl/apps/iptables/libipq/libipq.a
CFLAGS+=-Wall
CFLAGS += -I$(TOOLCHAIN)/include

OBJS = filter.o

all: urlfilterd

install:
	install -m 755 urlfilterd $(INSTALL_DIR)/bin
	$(STRIP) $(INSTALL_DIR)/bin/urlfilterd

dynamic: all install

static: urlfilterd.a

.c.o:
	$(CC) $(CFLAGS) -c $<

urlfilterd: filter.o
	$(CC) -o urlfilterd filter.o $(LDFLAGS) $(LIBS)

urlfilterd.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

clean:
	-rm -f urlfilterd *.o  urlfilterd.a
