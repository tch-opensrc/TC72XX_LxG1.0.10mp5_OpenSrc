#****************************************************************************
#
# Copyright © 2006-2008 Broadcom Corporation
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
#****************************************************************************

#
# In most cases, you only need to modify this first section.
# Just add libraries to LIB_DIRS
#
LIB_DIRS = cms_boardctl cms_util cms_msg openssl


#
# We need to declare these directory names as phony targets,
# otherwise make will think the directories themselves are
# the targets and will never execute the cmds.
# See GNU Make, 4.6 
#
.PHONY : $(LIB_DIRS)
all    : $(LIB_DIRS)


#
# Cleaning rules
# See example in GNU Make, 4.6
#
clean: 
	for dir in $(LIB_DIRS); do \
		$(MAKE) -C $$dir clean; \
	done


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



##############################################################
# 
# Rules for inidividual libs
# libs can be compiled or not depending on the profile (which
# is modified via make menuconfig)
#
##############################################################


# always build this lib
cms_boardctl:
	$(MAKE) -C $@

# always build this lib
cms_util:
	$(MAKE) -C $@

# always build this lib
cms_msg:
	$(MAKE) -C $@


# DO_BUILD_OPENSSL is set in make.common
ifneq ($(strip $(DO_BUILD_OPENSSL)),)
openssl: sanity_check
	(tar xkfj openssl.tar.bz2 2> /dev/null || true)
	$(MAKE) -C openssl all install
else
openssl: sanity_check
	@echo "skipping openssl (not configured)"
endif


