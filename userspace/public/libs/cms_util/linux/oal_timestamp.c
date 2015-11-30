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

#include "../oal.h"
#include <unistd.h>
#include <time.h>


/** OS dependent timestamp functions go in this file.
 */
void oalTms_get(CmsTimestamp *tms)
{
   struct timespec ts;
   SINT32 rc;

   if (tms == NULL)
   {
      return;
   }

   rc = clock_gettime(CLOCK_MONOTONIC, &ts);
   if (rc == 0)
   {
      tms->sec = ts.tv_sec;
      tms->nsec = ts.tv_nsec;
   }
   else
   {
      cmsLog_error("clock_gettime failed, set timestamp to 0");
      tms->sec = 0;
      tms->nsec = 0;
   }
}


CmsRet oalTms_getXSIDateTime(UINT32 t, char *buf, UINT32 bufLen)
{
	int          c;
   time_t       now;
	struct tm   *tmp;

   if (t == 0)
   {
      now = time(NULL);
   }
   else
   {
      now = t;
   }

	tmp = localtime(&now);
   memset(buf, 0, bufLen);
	c = strftime(buf, bufLen, "%Y-%m-%dT%H:%M:%S%z", tmp);
   if ((c == 0) || (c+1 > bufLen))
   {
      /* buf was not long enough */
      return CMSRET_RESOURCE_EXCEEDED;
   }

	/* fix missing : in time-zone offset-- change -500 to -5:00 */
   buf[c+1] = '\0';
   buf[c] = buf[c-1];
   buf[c-1] = buf[c-2];
   buf[c-2]=':';

   return CMSRET_SUCCESS;
}

