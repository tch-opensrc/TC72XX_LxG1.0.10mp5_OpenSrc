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

CmsRet cmsPsp_set(const char *key, const void *buf, UINT32 bufLen)
{
   char *currBuf;
   SINT32 count;

   if ((currBuf = cmsMem_alloc(bufLen, 0)) == NULL)
   {
      return CMSRET_RESOURCE_EXCEEDED;
   }

   /*
    * Writing to the scratch pad is a non-preemptive time consuming
    * operation that should be avoided.
    * Check if the new data is the same as the old data.
    */
   count = devCtl_boardIoctl(BOARD_IOCTL_FLASH_READ, SCRATCH_PAD,
                           (char *) key, 0, (SINT32) bufLen, currBuf);
   if (count == (SINT32) bufLen)
   {
      if (memcmp(currBuf, buf, bufLen) == 0)
      {
         cmsMem_free(currBuf);
         /* set is exactly the same as the orig data, no set needed */
         return CMSRET_SUCCESS;
      }

      cmsMem_free(currBuf);
   }
      

   return (devCtl_boardIoctl(BOARD_IOCTL_FLASH_WRITE, SCRATCH_PAD,
                             (char *) key, 0, (SINT32) bufLen, (void *) buf));
}


SINT32 cmsPsp_get(const char *key, void *buf, UINT32 bufLen)
{

   return ((SINT32) devCtl_boardIoctl(BOARD_IOCTL_FLASH_READ, SCRATCH_PAD,
                                      (char *) key, 0, (SINT32) bufLen, buf));
}


SINT32 cmsPsp_list(char *buf, UINT32 bufLen)
{

   return ((SINT32) devCtl_boardIoctl(BOARD_IOCTL_FLASH_LIST, SCRATCH_PAD,
                                      NULL, 0, (SINT32) bufLen, buf));
}


CmsRet cmsPsp_clearAll(void)
{

   return (devCtl_boardIoctl(BOARD_IOCTL_FLASH_WRITE, SCRATCH_PAD,
                             "", -1, -1, ""));

}



