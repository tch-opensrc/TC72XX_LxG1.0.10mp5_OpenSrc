#***********************************************************************
#
#  Copyright (c) 2007  Broadcom Corporation
#  All Rights Reserved
#  No portions of this material may be reproduced in any form without the
#  written permission of:
#          Broadcom Corporation
#          5300 California Avenue
#          Irvine, California 92617
#  All information contained in this document is Broadcom Corporation
#  company private, proprietary, and trade secret.
#
#***********************************************************************/

#
# You probably don't need to modify anything in this Makefile.
#



#
# This is the list of directories.  But we cannot do a simple
# traversal into each directory because we need to build all libraries
# before building any of the apps.  See the "all" target.
#
USERSPACE_DIRS = gpl



#
# We need to declare these directory names as phony targets,
# otherwise make will think the directories themselves are
# the targets and will never execute the cmds.
# See GNU Make, 4.6 
#
.PHONY : $(USERSPACE_DIRS)

#all    : public-include private-libs public-libs gpl-apps private-apps public-apps 
all    :  gpl-apps

public-include:
	$(MAKE) -C public/include

private-libs:
	$(MAKE) -C private/libs

public-libs:
	$(MAKE) -C public/libs

gpl-apps:
	$(MAKE) -C gpl/apps

public-apps:
	$(MAKE) -C public/apps

private-apps:
	$(MAKE) -C private/apps


#
# Generic rule for making the dirs
# You can still make an entire directory at once, but be careful
# of the dependencies, you need to make all the libs before you
# can make any of the apps.
#
$(USERSPACE_DIRS):
	$(MAKE) -C $@



#
# Cleaning rules
# See example in GNU Make, 4.6
#
clean: 
	for dir in gpl ; do \
		$(MAKE) -C $$dir clean; \
	done
