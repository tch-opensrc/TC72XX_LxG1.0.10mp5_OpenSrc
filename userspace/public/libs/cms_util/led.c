/***********************************************************************
 *
 *  Copyright (c) 2006-2007  Broadcom Corporation
 *  All Rights Reserved
 *
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
 *
 ************************************************************************/

#include "cms.h"
#include "cms_util.h"
#include "cms_boardioctl.h"

/*
 * See:
 * bcmdrivers/opensource/include/bcm963xx/board.h
 * bcmdrivers/opensource/char/board/bcm963xx/impl1/board.c and bcm63xx_led.c
 */

void cmsLed_setWanConnected(void)
{
   devCtl_boardIoctl(BOARD_IOCTL_LED_CTRL, 0, NULL, kLedWanData, kLedStateOn, NULL);
}


void cmsLed_setWanDisconnected(void)
{
   devCtl_boardIoctl(BOARD_IOCTL_LED_CTRL, 0, NULL, kLedWanData, kLedStateOff, NULL);
}


void cmsLed_setWanFailed(void)
{
   devCtl_boardIoctl(BOARD_IOCTL_LED_CTRL, 0, NULL, kLedWanData, kLedStateFail, NULL);
}


