/***********************************************************************
 *
 *  Copyright (c) 2007  Broadcom Corporation
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


#ifndef __CMS_BOARDCMDS_H__
#define __CMS_BOARDCMDS_H__

#include <sys/ioctl.h>
#include "cms.h"

/*!\file cms_boardcmds.h
 * \brief Header file for the Board Control Command API.
 *
 * These functions are the simple board control functions that other apps,
 * including GPL apps, may need.  These functions are mostly just wrappers
 * around devCtl_boardIoctl().  The nice things about this file is that
 * it does not require the program to link against additional bcm kernel
 * driver header files.
 *
 */


/** Get the board's br0 interface mac address.
 * 
 * @param macAddrNum (OUT) The user must pass in an array of UINT8 of at least
 *                         MAC_ADDR_LEN (6) bytes long.
 * 
 * @return CmsRet enum.
 */
CmsRet devCtl_getBaseMacAddress(UINT8 *macAddrNum);


/** Get the number of ethernet MACS on the system.
 * 
 * @return number of ethernet MACS.
 */
UINT32 devCtl_getNumEnetMacs(void);


/** Get the number of ethernet ports on the system.
 * 
 * @return number of ethernet ports.
 */
UINT32 devCtl_getNumEnetPorts(void);


/** Get SDRAM size on the system.
 * 
 * @return SDRAM size in number of bytes.
 */
UINT32 devCtl_getSdramSize(void);


/** Get the chipId.
 *
 * This info is used in various places, including CLI and writing new
 * flash image.  It may be accessed by GPL apps, so it cannot be put
 * exclusively in the data model.
 *  
 * @param chipId (OUT) The chip id returned by the kernel.
 * @return CmsRet enum.
 */
CmsRet devCtl_getChipId(UINT32 *chipId);


/** Read the config file from the specified config area of the flash and put it in the
 *  buffer.
 *
 * @param selector (IN) Which config buffer to read, CMS_CONFIG_PRIMARY or
 *                      CMS_CONFIG_BACKUP.
 * @param buf (OUT)    Caller provided buffer for holding the config, and this
 *                     function will read the config file from the 
 *                     persistent storage into this buffer.
 * @param len (IN/OUT) On entry, len contains the length of the buffer.
 *                     On successful exit, len contains the number of bytes
 *                     actually read.
 *
 * @return CmsRet enum.  If the config area of the flash is empty, this
 *                       function will return CMSRET_SUCCESS with len=0;
 */
CmsRet devCtl_readConfigFlashToBuf(const char *selector, char *buf, UINT32 *len);



#endif /* __CMS_BOARDCMDS_H__ */
