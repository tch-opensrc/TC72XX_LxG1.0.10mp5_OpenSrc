#***********************************************************************
#
#  Copyright (c) 2006  Broadcom Corporation
#  All Rights Reserved
#
# 
# 
# Unless you and Broadcom execute a separate written software license 
# agreement governing use of this software, this software is licensed 
# to you under the terms of the GNU General Public License version 2 
# (the "GPL"), available at http://www.broadcom.com/licenses/GPLv2.php, 
# with the following added to such license:
# 
#    As a special exception, the copyright holders of this software give 
#    you permission to link this software with independent modules, and 
#    to copy and distribute the resulting executable under terms of your 
#    choice, provided that you also meet, for each linked independent 
#    module, the terms and conditions of the license of that module. 
#    An independent module is a module which is not derived from this
#    software.  The special exception does not apply to any modifications 
#    of the software.  
# 
# Not withstanding the above, under no circumstances may you combine 
# this software in any way with any other Broadcom software provided 
# under a license other than the GPL, without Broadcom's express prior 
# written consent. 
#
#
#***********************************************************************/

#
# In most cases, you only need to modify this first section.
#
EXE = sshd

COMMONOBJS=dbutil.o buffer.o \
		dss.o bignum.o \
		signkey.o rsa.o random.o \
		queue.o \
		atomicio.o compat.o  fake-rfc2553.o

SVROBJS=svr-kex.o svr-algo.o svr-auth.o sshpty.o \
		svr-authpasswd.o svr-authpubkey.o svr-session.o svr-service.o \
		svr-chansession.o svr-runopts.o svr-agentfwd.o svr-main.o svr-x11fwd.o\
		svr-tcpfwd.o svr-authpam.o \

CLISVROBJS=common-session.o packet.o common-algo.o common-kex.o \
			common-channel.o common-chansession.o termcodes.o loginrec.o \
			tcp-accept.o listener.o process-packet.o \
			common-runopts.o circbuffer.o


OBJS = $(COMMONOBJS) $(SVROBJS) $(CLISVROBJS)

LIBS = -lcms_cli -lcms_dal -lcms_msg $(CMS_CORE_LIBS) $(CMS_COMMON_LIBS) \
       libtomcrypt/libtomcrypt.a libtommath/libtommath.a

CUSTOM_CFLAGS = -DDROPBEAR_SERVER -DDEBUG_TRACE -DBRCM_CMS_BUILD

all dynamic install: README libtomcrypt libtommath $(EXE) generic_exe_install

SSHLIBS = libtomcrypt libtommath genkey
.PHONY : $(SSHLIBS)

$(SSHLIBS):
	make -C $@

# If we have not untar'd the source, this file will be missing.
# Go untar it now.
README:
	@echo untaring source files
	cd ..; (tar xkfj sshd.tar.bz2 2> /dev/null || true)

clean: generic_clean
	rm -f $(INSTALL_DIR)/bin/$(EXE)
	for dir in $(SSHLIBS); do \
	        $(MAKE) -C $$dir clean; \
	done

distclean: clean
	rm -f buffer.* dss.c bignum.c \
              signkey.c rsa.c random.c queue.c \
              atomicio.c compat.c  fake-rfc2553.c \
              common-session.c packet.c common-algo.c common-kex.c \
              common-channel.c common-chansession.c termcodes.c loginrec.c \
              svr-kex.c svr-algo.c \
              svr-authpubkey.c svr-session.c svr-service.c \
              svr-runopts.c svr-agentfwd.c svr-x11fwd.c \
              svr-tcpfwd.o svr-authpam.o \
              tcp-accept.c listener.c process-packet.c \
              common-runopts.c circbuffer.* \
              agentfwd.h algo.h atomicio.h auth.h bignum.h \
              channel.h chansession.h cli-algo.c \
              cli-auth.c cli-authpasswd.c cli-authpubkey.c cli-channel.c \
              cli-kex.c cli-main.c cli-runopts.c cli-service.c cli-session.c \
              cli-tcpfwd.c compat.h config.guess config.h.in config.sub configure \
              cli-chansession.c configure.in dbclient.c dbmulti.c dbutil.h \
              debug.h dropbear.8 dropbearconvert.c dropbearkey.8 dropbearkey.c \
              dss.h fake-rfc2553.h filelist.txt gendss.c gendss.h genrsa.* \
              install-sh kex.h keyimport.* listener.h loginrec.h Makefile.in \
              packet.h progressmeter* queue.h random.h rsa.h runopts.h scp.c \
              dbclient.1 scpmisc.* service.h session.h signkey.h ssh.h sshpty.h \
              svr-authpam.c svr-tcpfwd.c tcpfwd.h termcodes.h x11fwd.h
	rm -rf debian MT .*.flags .mt-attrs
	rm -f CHANGES INSTALL LICENSE MULTI README SMALL TAGS TODO
	cd libtommath; (find . -name Makefile -o -exec rm -rf {} \; 2>/dev/null || true) 
	cd libtomcrypt; (find . -name Makefile -o -exec rm -rf {} \; 2>/dev/null || true) 
	cd genkey; (find . -name Makefile -o -exec rm -rf {} \; 2>/dev/null || true) 


#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
# You do not need to modify this part.
#
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))

include $(BUILD_DIR)/make.common


#
# Public apps are only allowed to include header files from the
# private and public directories.
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_INCLUDE_PATHS := -I.\
                         -I$(BUILD_DIR)/userspace/public/include  \
                         -I$(BUILD_DIR)/userspace/public/include/$(OALDIR) \
                         -I$(BUILD_DIR)/userspace/private/include  \
                         -I$(BUILD_DIR)/userspace/private/include/$(OALDIR)


# sshd also needs some header files from the libtomcrypt
ALLOWED_INCLUDE_PATHS += -Ilibtomcrypt/src/headers



#
# Public apps are only allowed to link with libraries from the
# private and public directories.
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_LIB_DIRS := /lib:/lib/private:/lib/public



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

include $(BUILD_DIR)/make.deprules

-include $(OBJS:.o=.d)



