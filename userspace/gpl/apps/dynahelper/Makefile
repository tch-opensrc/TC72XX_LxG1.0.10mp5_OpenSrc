TARGET=dynahelper
SRCS=dynahelper.c libipq.c
DEFS=
INCS=$(KERNEL_DIR)/include
LIBS=
LIB3=

#######################################################################
# DO NOT CHANGE ANYTHING BELOW!
# 
# The available command line arguments include:
#
# 1. (Nothing) 	- To build target without debug info.
# 2. debug	- To build target with debug info for gdb use.
# 3. clean	- To delete target and objects.
# 4. install	- To build if neccesary and install target to installdir
# 5. uninstall	- To remove target from installdir
#######################################################################
GRMS:=$(filter %.y, $(SRCS))
LEXS:=$(filter %.l, $(SRCS))
OBJS:=$(SRCS:.cpp=.o)
OBJS:=$(OBJS:.cxx=.o)
OBJS:=$(OBJS:.c=.o)
OBJS:=$(OBJS:.y=.o)
OBJS:=$(OBJS:.l=.o)
TMPS:=$(OBJS) $(GRMS:.y=.c) $(GRMS:.y=.h) $(LEXS:.l=.c)
DEFS:=$(addprefix -D,$(DEFS))
DEFS:=$(DEFS) $(addprefix -D,$(MAKEOVERRIDES))
INCS:=$(addprefix -I,$(INCS))
LIBS:=$(addprefix -l,$(LIBS))
ifeq ($(MAKECMDGOALS),debug)
	CFLAGS:=-Wall -c -g
	LDFLAGS:=-g
	DEFS:=$(DEFS) -D_DEBUG_
else
	CFLAGS:=-Wall -c -Os
	LDFLAGS:=-s
endif

ifeq ($(strip $(BRCM_KERNEL_NF_NAT_ALG_FTP)),y)
	DEFS:=$(DEFS) -DSTATIC_KERNEL_NF_NAT_ALG_FTP
endif
ifeq ($(strip $(BRCM_KERNEL_NF_NAT_ALG_H323)),y)
	DEFS:=$(DEFS) -DSTATIC_KERNEL_NF_NAT_ALG_H323
endif
ifeq ($(strip $(BRCM_KERNEL_NF_NAT_ALG_IPSEC)),y)
	DEFS:=$(DEFS) -DSTATIC_KERNEL_NF_NAT_ALG_IPSEC
endif

dynamic: all install

all: $(TARGET)

debug: $(TARGET)

clean:
	-rm -f $(TARGET) $(TMPS)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -o $(TARGET) $(OBJS) $(LIB3)

%.o: %.l
	flex $<
	$(CC) $(CFLAGS) $(DEFS) $(INCS) $*.c
	
%.o: %.y
	bison $<
	$(CC) $(CFLAGS) $(DEFS) $(INCS) $*.c
	
%.o: %.c
	$(CC) $(CFLAGS) $(DEFS) $(INCS) $<

%.o: %.cxx
	$(CC) $(CFLAGS) $(DEFS) $(INCS) $<
	
%.o: %.cpp
	$(CC) $(CFLAGS) $(DEFS) $(INCS) $<


#######################################################################
# Install & Remove
#######################################################################

install: $(TARGET)
	install -m 755 $(TARGET) $(INSTALL_DIR)/bin
	$(STRIP) $(INSTALL_DIR)/bin/dynahelper
#	@if [ -f /var/run/$(TARGET).pid ]; then \
#		sh $(TARGET).sh stop; \
#	fi
#	install -m755 -oroot -p $(TARGET) $(INSTALL_DIR)
#	install -m755 -oroot -p $(TARGET).sh /etc/init.d/$(TARGET)
#	update-rc.d $(TARGET) defaults
	@echo $(TARGET) insalled.

uninstall:
	@if [ -f /var/run/$(TARGET).pid ]; then \
		sh $(TARGET).sh stop; \
	fi
	@if [ -f $(INSTALL_DIR)/$(TARGET) ]; then \
		rm -vf $(INSTALL_DIR)/$(TARGET); \
		if [ -f /etc/init.d/$(TARGET) ]; then \
			rm -vf /etc/init.d/$(TARGET); \
			update-rc.d -f $(TARGET) remove; \
		fi; \
		echo $(TARGET) uninstalled. ; \
	else \
		echo $(TARGET) not installed yet. ; \
	fi

