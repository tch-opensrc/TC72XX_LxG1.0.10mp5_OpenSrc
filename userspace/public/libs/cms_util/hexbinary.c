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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cms_util.h"


CmsRet cmsUtl_binaryBufToHexString(const UINT8 *binaryBuf, UINT32 binaryBufLen, char **hexStr)
{
   UINT32 i, j;

   if (hexStr == NULL)
   {
      cmsLog_error("hexStr buffer is NULL");
      return CMSRET_INVALID_ARGUMENTS;
   }

   *hexStr = cmsMem_alloc((binaryBufLen*2)+1, ALLOC_ZEROIZE);
   if (*hexStr == NULL)
   {
      return CMSRET_RESOURCE_EXCEEDED;
   }

   for (i=0, j=0; i < binaryBufLen; i++, j+=2)
   {
      sprintf(&((*hexStr)[j]), "%02x", binaryBuf[i]);
   }

   return CMSRET_SUCCESS;
}


CmsRet cmsUtl_hexStringToBinaryBuf(const char *hexStr, UINT8 **binaryBuf, UINT32 *binaryBufLen)
{
   UINT32 len;
   UINT32 val;
   UINT32 i, j;
   char tmpbuf[3];
   CmsRet ret;

   len = strlen(hexStr);
   if (len % 2 != 0)
   {
      cmsLog_error("hexStr must be an even number of characters");
      return CMSRET_INVALID_ARGUMENTS;
   }

   *binaryBuf = cmsMem_alloc(len/2, 0);
   if (*binaryBuf == NULL)
   {
      return CMSRET_RESOURCE_EXCEEDED;
   }

   for (i=0, j=0; j < len; i++, j+=2)
   {
      tmpbuf[0] = hexStr[j];
      tmpbuf[1] = hexStr[j+1];
      tmpbuf[2] = 0;

      ret = cmsUtl_strtoul(tmpbuf, NULL, 16, &val);
      if (ret != CMSRET_SUCCESS)
      {
         cmsMem_free(*binaryBuf);
         *binaryBuf = NULL;
         return ret;
      }
      else
      {
         (*binaryBuf)[i] = (UINT8) val;
      }
   }

   /* if we get here, we were successful, set length */
   *binaryBufLen = len / 2;

   return ret;
}



