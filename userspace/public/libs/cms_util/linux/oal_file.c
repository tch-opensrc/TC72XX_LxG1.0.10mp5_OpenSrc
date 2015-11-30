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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "cms.h"
#include "cms_util.h"
#include "../oal.h"

UBOOL8 oalFil_isFilePresent(const char *filename)
{
   struct stat statbuf;
   SINT32 rc;

   rc = stat(filename, &statbuf);

   if (rc == 0)
   {
      return TRUE;
   }
   else
   {
      return FALSE;
   }
}


SINT32 oalFil_getSize(const char *filename)
{
   struct stat statbuf;
   SINT32 rc;

   rc = stat(filename, &statbuf);

   if (rc == 0)
   {
      return ((SINT32) statbuf.st_size);
   }
   else
   {
      return -1;
   }
}


CmsRet oalFil_copyToBuffer(const char *filename, UINT8 *buf, UINT32 *bufSize)
{
   SINT32 actualFileSize;
   SINT32 fd, rc;

   actualFileSize = oalFil_getSize(filename);

   if (*bufSize < (UINT32) actualFileSize)
   {
      cmsLog_error("user supplied buffer is %d, filename actual size is %d", *bufSize, actualFileSize);
      return CMSRET_RESOURCE_EXCEEDED;
   }

   *bufSize = 0;
       
   fd = open(filename, 0);
   if (fd < 0)
   {
      cmsLog_error("could not open file %s, errno=%d", filename, errno);
      return CMSRET_INTERNAL_ERROR;
   }

   rc = read(fd, buf, actualFileSize);
   if (rc != actualFileSize)
   {
      cmsLog_error("read error, got %d, expected %d", rc, actualFileSize);
      close(fd);
      return CMSRET_INTERNAL_ERROR;
   }

   close(fd);

   /* let user know how many bytes was actually copied */
   *bufSize = (UINT32) actualFileSize;
   return CMSRET_SUCCESS;
}


