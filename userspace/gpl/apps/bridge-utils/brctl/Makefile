
# brcm begin
KERNEL_HEADERS=-I$(KERNEL_DIR)/include

#CC=gcc
#CFLAGS= -Wall -g -O2
CFLAGS= -Wall -s
ifeq ($(strip $(BUILD_BRCTL)),static)
CFLAGS += -DBUILD_STATIC
endif

CFLAGS += -I$(TOOLCHAIN)/include

# brcm end
LDFLAGS=
INCLUDE=-I../libbridge $(KERNEL_HEADERS) 
LIBS= -L ../libbridge -lbridge 

prefix=/usr/local
exec_prefix=${prefix}
bindir=${exec_prefix}/bin
sbindir=${exec_prefix}/sbin
mandir=${prefix}/share/man

INSTALL=/usr/bin/install -c


common_SOURCES= brctl_cmd.c brctl_disp.c 
brctl_SOURCES=  brctl.c $(common_SOURCES)

common_OBJECTS= $(common_SOURCES:.c=.o)
brctl_OBJECTS= $(brctl_SOURCES:.c=.o)

OBJECTS= $(common_OBJECTS) $(brctl_OBJECTS)

PROGRAMS= brctl 


all:	$(PROGRAMS)
# brcm befing
dynamic: $(PROGRAMS)

static: $(brctl_OBJECTS) ../libbridge/libbridge.a
	$(AR) rcs brctl.a $(brctl_OBJECTS) $(LIBS)
# brcm end

install: $(PROGRAMS)
	mkdir -p $(DESTDIR)$(sbindir)
	$(INSTALL) -m 755 $(PROGRAMS) $(DESTDIR)$(sbindir)

brctl:	$(brctl_OBJECTS) ../libbridge/libbridge.a
	$(CC) $(LDFLAGS) $(brctl_OBJECTS) $(LIBS) -o brctl

%.o: %.c brctl.h
	$(CC) $(CFLAGS) $(INCLUDE) -c $< 

clean:
	rm -f *.o brctl core

