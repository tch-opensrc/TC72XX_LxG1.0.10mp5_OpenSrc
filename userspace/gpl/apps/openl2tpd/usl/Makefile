USL_SRCS.c=		usl_fd.c usl_misc.c usl_pid.c usl_signal.c \
			usl_timer.c usl_fsm.c
USL_SRCS.h=		usl_fsm.h usl.h usl_list.h
USL_SRCS_TEST.c=	usl_test.c

USL_SRCS.o=		$(USL_SRCS.c:%.c=%.o)
USL_SRCS_TEST.o=	$(USL_SRCS_TEST.c:%.c=%.o)

LDFLAGS.usl_test=	-L. -lc

ifeq ($(USE_DMALLOC),y)
CPPFLAGS.dmalloc=	-DUSL_DMALLOC
LIBS.dmalloc=		-ldmalloc
endif

CPPFLAGS+=		$(CFLAGS.optimize) -g
CFLAGS=			-I. -MMD -Wall -Werror $(CPPFLAGS) $(CPPFLAGS.dmalloc)

.PHONY:			all test clean

all:			libusl.a

test:			libusl.a usl_test

libusl.a:		$(USL_SRCS.o)
			$(AR) rus $@ $^

usl_test:		$(USL_SRCS_TEST.o) libusl.a
			$(CC) -o $@ $(USL_SRCS_TEST.o) $(LDFLAGS.usl_test)

clean:
			-$(RM) $(USL_SRCS.o) $(USL_SRCS_TEST.o) libusl.a usl_test $(wildcard *.d)

distclean:
			-$(RM) $(wildcard *~)

%.o:	%.c
			$(CC) -c $(CFLAGS) $< -o $@

install:		all
			$(INSTALL) -d $(DESTDIR)$(SYS_LIBDIR)
			$(INSTALL) libusl.a $(DESTDIR)$(SYS_LIBDIR)

checkin:		clean
			ci -u -q Makefile $(USL_SRCS.c) $(USL_SRCS.h) $(USL_SRCS_TEST.c)

TAGS:			$(USL_SRCS.c) $(USL_SRCS.h)
			etags $^

include $(wildcard *.d /dev/null)

