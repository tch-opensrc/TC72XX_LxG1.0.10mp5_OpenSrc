CFLAGS	:= $(CFLAGS.optimize) -MMD -Wall \
	-isystem $(KERNEL_BLDDIR)/include -isystem $(KERNEL_SRCDIR)/include \
	-I. -I.. -I../usl -fPIC $(EXTRA_CFLAGS)
LDFLAGS	:= -shared

PLUGINS.c:=	ppp_unix.c ppp_null.c ipsec.c event_sock.c

ifneq (,$(findstring -DL2TP_TEST,$(CFLAGS)))
PLUGINS.c+=	ppp_test.c
endif

PLUGINS.so=	$(PLUGINS.c:%.c=%.so)

SRC.generated=

.PHONY:		all clean install TAGS generated-files

all:		generated-files $(PLUGINS.so)

clean:
		$(RM) $(PLUGINS.so) $(wildcard *.o) $(wildcard *.d) $(SRC.generated)

%.so: %.c
		$(CC) -o $@ $(LDFLAGS) $(CFLAGS) $<

install:	all
		$(INSTALL) -d $(DESTDIR)$(SYS_LIBDIR)/openl2tp
		$(INSTALL) $(PLUGINS.so) $(DESTDIR)$(SYS_LIBDIR)/openl2tp
		$(INSTALL) -m 0644 event_sock.h $(DESTDIR)$(SYS_LIBDIR)/openl2tp


generated-files:	$(SRC.generated)

include $(wildcard *.d) /dev/null
