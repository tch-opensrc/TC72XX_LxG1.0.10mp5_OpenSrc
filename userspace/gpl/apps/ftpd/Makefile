# Makefile file for bftpd
#
# License:  GNU General Public License, Version 2.
#
EXE = bftpd


OBJS=commands.o list.o login.o main.o mystring.o

LIBS= -lcms_msg $(CMS_COMMON_LIBS)

all dynamic: sanity_check $(EXE) generic_exe_install

# Remove symlinks so customers do not import them into their source control systems
clean: generic_clean
	rm -f $(INSTALL_DIR)/bin/$(EXE)
	rm -f INSTALL README

# this removes all non-source controlled files (this list is not complete)
distclean: clean
	rm -rf doc CHANGELOG INSTALL install-sh .project .settings \
	README Makefile.in debian .cdtproject configure configure.in \
	targzip.h



#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
#
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))

include $(BUILD_DIR)/make.common



#
# GPL apps and libs are only allowed to include header files from the
# gpl and public directories
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
# GPL apps and libs are only allowed to link with libraries from the
# gpl and public directories.
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_LIB_DIRS := /lib:/lib/gpl:/lib/public



#
# Some additional tests for different ftp modes
#
ifeq ($(strip $(BUILD_FTPD_STORAGE)), y)
	OBJS += cwd.o options.o dirlist.o
 	CFLAGS += -DSUPPORT_FTPD_STORAGE
endif

#
# In CMS, we want a standalone ftpd app, so build dynamic
# makemenuconfig should be changed so that static is not even an option
#
ifeq ($(strip $(BUILD_FTPD)), static)
	CFLAGS += -DBUILD_STATIC
endif

#
# Implicit rule will make the .c into a .o
# Implicit rule is $(CC) -c $(CPPFLAGS) $(CFLAGS)
# See Section 10.2 of Gnu Make manual
# 
$(EXE): CHANGELOG $(OBJS)
	$(CC) -o $@ $(OBJS) -Wl,-rpath,$(CMS_LIB_RPATH) $(CMS_LIB_PATH) $(LIBS)


#
# If we have not untar'd the source, this file will be missing.
# Go untar it now.
#
CHANGELOG:
	@echo untaring source files
	cd ..; (tar xkfj ftpd.tar.bz2 2> /dev/null || true)


#
# Some legacy targets.  Do we even support static mode anymore?
#
static: bftpd.a

bftpd.a: $(OBJS)
	$(CC) $(CFLAGS) -c -o $(OBJS)
	$(AR) rcs bftpd.a $(OBJS) $(LIBS)



#
# Include the rule for making dependency files.
# The '-' in front of the second include suppresses
# error messages when make cannot find the .d files.
# It will just regenerate them.
# See Section 4.14 of Gnu Make.
#

include $(BUILD_DIR)/make.deprules

-include $(OBJS:.o=.d)



