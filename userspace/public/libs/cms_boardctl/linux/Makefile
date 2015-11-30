#****************************************************************************
#
# Copyright © 2006-2008 Broadcom Corporation
#
# This program is the proprietary software of Broadcom Corporation and/or its
# licensors, and may only be used, duplicated, modified or distributed
# pursuant to the terms and conditions of a separate, written license
# agreement executed between you and Broadcom (an "Authorized License").
# Except as set forth in an Authorized License, Broadcom grants no license
# (express or implied), right to use, or waiver of any kind with respect to
# the Software, and Broadcom expressly reserves all rights in and to the
# Software and all intellectual property rights therein.  IF YOU HAVE NO
# AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY,
# AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE
# SOFTWARE.
#
# Except as expressly set forth in the Authorized License,
#
# 1.     This program, including its structure, sequence and organization,
#        constitutes the valuable trade secrets of Broadcom, and you shall
#        use all reasonable efforts to protect the confidentiality thereof,
#        and to use this information only in connection with your use of
#        Broadcom integrated circuit products.
#
# 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
#        "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
#        REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY,
#        OR OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
#        DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
#        NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
#        ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
#        CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
#        OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
#
# 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM
#        OR ITS LICENSORS BE LIABLE FOR
#        (i)  CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR EXEMPLARY
#             DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO
#             YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM
#             HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR
#        (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE
#             SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
#             LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF
#             ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
#
#****************************************************************************
#
#  $Id:
#
#   Filename: Makefile
#   Author:
#   Creation Date: 2006
#
#
#****************************************************************************

#
# In most cases, you only need to modify this first section.
#
LIB = oal.a

OBJS = board.o 

all: sanity_check $(LIB)

clean: generic_clean



#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
# You do not need to modify this part.
#
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, ,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))

include $(BUILD_DIR)/make.common

ifneq ($(strip $(BUILD_VODSL)),)
include $(BUILD_DIR)/make.voice
endif



#
# Public libs are only allowed to include header files from the
# public directory.
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_INCLUDE_PATHS := -I. -I..\
                         -I$(BUILD_DIR)/userspace/public/include  \
                         -I$(BUILD_DIR)/userspace/public/include/$(OALDIR)


# This libraray needs to communicate with the kernel, so it needs these 
# header files.  Other libraries in the public dir should not copy this line.
ALLOWED_INCLUDE_PATHS += -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD)  \
                         -I$(INC_BRCMDRIVER_PRIV_PATH)/$(BRCM_BOARD) \
                         -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)



#
# Implicit rule will make the .c into a .o
# Implicit rule is $(CC) -c $(CPPFLAGS) $(CFLAGS)
# See Section 10.2 of Gnu Make manual
# 


#
# Generic rule makes the library oal.a
# see make.common
#


#
# Include the rule for making dependency files.
# The '-' in front of the second include suppresses
# error messages when make cannot find the .d files.
# It will just regenerate them.
# See Section 4.14 of Gnu Make.
#

include $(BUILD_DIR)/make.deprules

-include $(OBJS:.o=.d)
