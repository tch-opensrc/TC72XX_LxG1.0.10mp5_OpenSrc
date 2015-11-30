DESTDIR=
SBINDIR=$(INSTALL_DIR)/bin
CONFDIR=$(INSTALL_DIR)/etc/iproute2
DOCDIR=

# Path to parent kernel include files directory
KERNEL_INCLUDES:=$(KERNEL_DIR)/include $(KERNEL_DIR)/arch/mips/include
LIBC_INCLUDE=$(KERNEL_DIR)/include/linux

# Path to db_185.h include
#DBM_INCLUDE:=/usr/include

#DEFINES= -DRESOLVE_HOSTNAMES

#options if you have a bind>=4.9.4 libresolv (or, maybe, glibc)
LDLIBS=
ADDLIB=

#options for decnet
#ADDLIB+=dnet_ntop.o dnet_pton.o

#options for ipx
#ADDLIB+=ipx_ntop.o ipx_pton.o

ifeq ($(LIBC_INCLUDE)/socketbits.h,$(wildcard $(LIBC_INCLUDE)/socketbits.h))
  ifeq ($(LIBC_INCLUDE)/net/if_packet.h,$(wildcard $(LIBC_INCLUDE)/net/if_packet.h))
    GLIBCFIX=-I../include-glibc -include ../include-glibc/glibc-bugs.h
  endif
endif
ifeq ($(LIBC_INCLUDE)/bits/socket.h,$(wildcard $(LIBC_INCLUDE)/bits/socket.h))
  GLIBCFIX=-I../include-glibc -I/usr/include/db3 -include ../include-glibc/glibc-bugs.h
endif

#CC = gcc
CCOPTS = -D_GNU_SOURCE -s -Os -Wstrict-prototypes -Wall -g

ifeq ($(strip $(DESKTOP_LINUX)),y)
# some apps, such as iptables and iproute2, include MIPS kernel header files
# which need to know what endieness we are.  Fake the settings here.  We will
# not run those apps on the desktop anyways.
CCOPTS += -D__MIPSEL__ -D_MIPS_SZLONG=32
endif

CFLAGS = $(CCOPTS) $(GLIBCFIX) -I../include $(addprefix -I,$(KERNEL_INCLUDES)) $(DEFINES)
CFLAGS += -I$(TOOLCHAIN)/include
YACCFLAGS = -d -t -v

LDLIBS += -L../lib -lnetlink -lipr2util -lm 

#SUBDIRS=lib ip tc misc netem genl
SUBDIRS=lib ip tc misc

LIBNETLINK=../lib/libnetlink.a ../lib/libipr2util.a

all: Config
	@set -e; \
	for i in $(SUBDIRS); \
	do $(MAKE) $(MFLAGS) -C $$i; done

Config:
	./configure $(KERNEL_INCLUDE)

install: all
	install -m 0755 tc/tc $(DESTDIR)$(SBINDIR)
	$(STRIP) $(INSTALL_DIR)/bin/tc
	install -m 0755 ip/ip $(DESTDIR)$(SBINDIR)
	$(STRIP) $(INSTALL_DIR)/bin/ip
#
# These apps in the iproute2 package are not used by us.
#
#	install -m 0755 misc/ss $(DESTDIR)$(SBINDIR)
#	$(STRIP) $(INSTALL_DIR)/bin/ss
#	install -m 0755 misc/nstat $(DESTDIR)$(SBINDIR)
#	$(STRIP) $(INSTALL_DIR)/bin/nstat
#	install -m 0755 misc/ifstat $(DESTDIR)$(SBINDIR)
#	$(STRIP) $(INSTALL_DIR)/bin/ifstat
#	install -m 0755 misc/rtacct $(DESTDIR)$(SBINDIR)
#	$(STRIP) $(INSTALL_DIR)/bin/rtacct
#	install -m 0755 misc/lnstat $(DESTDIR)$(SBINDIR)
#	$(STRIP) $(INSTALL_DIR)/bin/lnstat

dynamic: install

clean:
	@for i in $(SUBDIRS) doc; \
	do if [ -f Config ]; then $(MAKE) $(MFLAGS) -C $$i clean; fi; done

clobber: clean
	rm -f Config


.EXPORT_ALL_VARIABLES:
