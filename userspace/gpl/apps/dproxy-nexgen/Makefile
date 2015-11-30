#
# In most cases, you only need to modify this first section.
#
EXE=dnsproxy
OBJS=dproxy.o dns_decode.o dns_list.o conf.o dns_construct.o dns_io.o dns_probe.o dns_dyn_cache.o
#LIBS=-lcms_msg $(CMS_COMMON_LIBS)
LIBS=
CUSTOM_CFLAGS=-DDNS_DYN_CACHE

all dynamic install: sanity_check $(EXE) generic_exe_install

clean: generic_clean
	rm -f $(INSTALL_DIR)/bin/$(EXE)



#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
#
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))

include $(BUILD_DIR)/make.common

ifneq ($(strip $(BUILD_VODSL)),)
include $(BUILD_DIR)/make.voice
endif



#
# Private apps and libs are allowed to include header files from the
# private and public directories.
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_INCLUDE_PATHS := -I.\
                         -I$(BUILD_DIR)/userspace/gpl/include  \
                         -I$(BUILD_DIR)/userspace/gpl/include/$(OALDIR) \
                         -I$(BUILD_DIR)/userspace/public/include  \
                         -I$(BUILD_DIR)/userspace/public/include/$(OALDIR)



#
# Private apps and libs are allowed to link with libraries from the
# private and public directories.
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_LIB_DIRS := /lib:/lib/gpl:/lib/public


ifneq ($(strip $(BUILD_DPROXYWITHPROBE)),)
CUSTOM_CFLAGS:=$(CUSTOM_CFLAGS) -DDNS_PROBE
endif

#
# Implicit rule will make the .c into a .o
# Implicit rule is $(CC) -c $(CPPFLAGS) $(CFLAGS)
# See Section 10.2 of Gnu Make manual
# 
$(EXE): $(OBJS)
	$(CC) -o $@ $(OBJS) -Wl,-rpath,$(CMS_LIB_RPATH) $(CMS_LIB_PATH) $(LIBS)



#
# Include the rule for making dependency files.
# The '-' in front of the second include suppresses
# error messages when make cannot find the .d files.
# It will just regenerate them.
# See Section 4.14 of Gnu Make.
#

include ../../../../make.deprules

-include $(OBJS:.o=.d)


