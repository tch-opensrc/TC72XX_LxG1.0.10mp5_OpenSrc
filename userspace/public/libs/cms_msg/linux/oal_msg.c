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


/* OS dependent messaging functions go here */

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "../oal.h"
#include "cms_util.h"


CmsRet oalMsg_init(CmsEntityId eid, void **msgHandle)
{
   CmsMsgHandle *handle;
   const CmsEntityInfo *eInfo;
   struct sockaddr_un serverAddr;
   SINT32 rc;

   if ((eInfo = cmsEid_getEntityInfo(eid)) == NULL)
   {
      cmsLog_error("Invalid eid %d", eid);
      return CMSRET_INVALID_ARGUMENTS;
   }
   
   if ((handle = (CmsMsgHandle *) cmsMem_alloc(sizeof(CmsMsgHandle), ALLOC_ZEROIZE)) == NULL)
   {
      cmsLog_error("could not allocate storage for msg handle");
      return CMSRET_RESOURCE_EXCEEDED;
   }

   /* store caller's eid */
   handle->eid = eid;

#ifdef DESKTOP_LINUX
   /*
    * Applications may be run without smd on desktop linux, so if we
    * don't see a socket for smd, don't bother connecting to it.
    */
   {
      struct stat statbuf;

      if ((rc = stat(SMD_MESSAGE_ADDR, &statbuf)) < 0)
      {
         handle->commFd = CMS_INVALID_FD;
         handle->standalone = TRUE;
         *msgHandle = (void *) handle;
         cmsLog_notice("no smd server socket detected, running in standalone mode.");
         return CMSRET_SUCCESS;
      }
   }
#endif /* DESKTOP_LINUX */


      /*
       * Create a unix domain socket.
       */
      handle->commFd = socket(AF_LOCAL, SOCK_STREAM, 0);
      if (handle->commFd < 0)
      {
         cmsLog_error("Could not create socket");
         cmsMem_free(handle);
         return CMSRET_INTERNAL_ERROR;
      }


      /*
       * Set close-on-exec, even though all apps should close their
       * fd's before fork and exec.
       */
      if ((rc = fcntl(handle->commFd, F_SETFD, FD_CLOEXEC)) != 0)
      {
         cmsLog_error("set close-on-exec failed, rc=%d errno=%d", rc, errno);
         close(handle->commFd);
         cmsMem_free(handle);
         return CMSRET_INTERNAL_ERROR;
      }


      /*
       * Connect to smd.
       */
      memset(&serverAddr, 0, sizeof(serverAddr));
      serverAddr.sun_family = AF_LOCAL;
      strncpy(serverAddr.sun_path, SMD_MESSAGE_ADDR, sizeof(serverAddr.sun_path));

      rc = connect(handle->commFd, (struct sockaddr *) &serverAddr, sizeof(serverAddr));
      if (rc != 0)
      {
         cmsLog_error("connect to %s failed, rc=%d errno=%d", SMD_MESSAGE_ADDR, rc, errno);
         close(handle->commFd);
         cmsMem_free(handle);
         return CMSRET_INTERNAL_ERROR;
      }
      else
      {
         cmsLog_debug("commFd=%d connected to smd", handle->commFd);
      }

      /* send a launched message to smd */
      {
         CmsRet ret;
         CmsMsgHeader launchMsg = EMPTY_MSG_HEADER;

         launchMsg.type = CMS_MSG_APP_LAUNCHED;
         launchMsg.src = (eInfo->flags & EIF_MULTIPLE_INSTANCES) ? MAKE_SPECIFIC_EID(getpid(), eid) : eid;
         launchMsg.dst = EID_SMD;
         launchMsg.flags_event = 1;

         if ((ret = oalMsg_send(handle->commFd, &launchMsg)) != CMSRET_SUCCESS)
         {
            close(handle->commFd);
            cmsMem_free(handle);
            return CMSRET_INTERNAL_ERROR;
         }
         else
         {
            cmsLog_debug("sent LAUNCHED message to smd");
         }
      }

   /* successful, set handle pointer */
   *msgHandle = (void *) handle;

   return CMSRET_SUCCESS;
}


void oalMsg_cleanup(void **msgHandle)
{
   CmsMsgHandle *handle = (CmsMsgHandle *) *msgHandle;

   if (handle->commFd != CMS_INVALID_FD)
   {
      close(handle->commFd);
   }

   CMSMEM_FREE_BUF_AND_NULL_PTR((*msgHandle));

   return;
}


CmsRet oalMsg_getEventHandle(const CmsMsgHandle *msgHandle, void *eventHandle)
{
   SINT32 *fdPtr = (SINT32 *) eventHandle;

   *fdPtr = msgHandle->commFd;

   return CMSRET_SUCCESS;
}


CmsRet oalMsg_send(SINT32 fd, const CmsMsgHeader *buf)
{
   UINT32 totalLen;
   SINT32 rc;
   CmsRet ret=CMSRET_SUCCESS;

   totalLen = sizeof(CmsMsgHeader) + buf->dataLength;

   rc = write(fd, buf, totalLen);
   if (rc < 0)
   {
      if (errno == EPIPE)
      {
         /*
          * This could happen when smd tries to write to an app that
          * has exited.  Don't print out a scary error message.
          * Just return an error code and let upper layer app handle it.
          */
         cmsLog_debug("got EPIPE, dest app is dead");
         return CMSRET_DISCONNECTED;
      }
      else
      {
         cmsLog_error("write failed, errno=%d", errno);
         ret = CMSRET_INTERNAL_ERROR;
      }
   }
   else if (rc != (SINT32) totalLen)
   {
      cmsLog_error("unexpected rc %d, expected %u", rc, totalLen);
      ret = CMSRET_INTERNAL_ERROR;
   }

   return ret;
}


static CmsRet waitForDataAvailable(SINT32 fd, UINT32 timeout)
{
   struct timeval tv;
   fd_set readFds;
   SINT32 rc;

   FD_ZERO(&readFds);
   FD_SET(fd, &readFds);

   tv.tv_sec = timeout / MSECS_IN_SEC;
   tv.tv_usec = (timeout % MSECS_IN_SEC) * USECS_IN_MSEC;

   rc = select(fd+1, &readFds, NULL, NULL, &tv);
   if ((rc == 1) && (FD_ISSET(fd, &readFds)))
   {
      return CMSRET_SUCCESS;
   }
   else
   {
      return CMSRET_TIMED_OUT;
   }
}


CmsRet oalMsg_receive(SINT32 fd, CmsMsgHeader **buf, UINT32 *timeout)
{
   CmsMsgHeader *msg;
   SINT32 rc;
   CmsRet ret;

   if (buf == NULL)
   {
      cmsLog_error("buf is NULL!");
      return CMSRET_INVALID_ARGUMENTS;
   }
   else
   {
      *buf = NULL;
   }

   if (timeout)
   {
      if ((ret = waitForDataAvailable(fd, *timeout)) != CMSRET_SUCCESS)
      {
         return ret;
      }
   }

   /*
    * Read just the header in the first read.
    * Do not try to read more because we might get part of 
    * another message in the TCP socket.
    */
   msg = (CmsMsgHeader *) cmsMem_alloc(sizeof(CmsMsgHeader), ALLOC_ZEROIZE);
   if (msg == NULL)
   {
      cmsLog_error("alloc of msg header failed");
      return CMSRET_RESOURCE_EXCEEDED;
   }

   rc = read(fd, msg, sizeof(CmsMsgHeader));
   if ((rc == 0) ||
       ((rc == -1) && (errno == 131)))  /* new 2.6.21 kernel seems to give us this before rc==0 */
   {
      /* broken connection */
      cmsMem_free(msg);
      return CMSRET_DISCONNECTED;
   }
   else if (rc < 0 || rc != sizeof(CmsMsgHeader))
   {
      cmsLog_error("bad read, rc=%d errno=%d", rc, errno);
      cmsMem_free(msg);
      return CMSRET_INTERNAL_ERROR;
   }

   if (msg->dataLength > 0)
   {
      SINT32 totalReadSoFar=0;
      SINT32 totalRemaining=msg->dataLength;
      char *inBuf;

      /* there is additional data in the message */
      msg = (CmsMsgHeader *) cmsMem_realloc(msg, sizeof(CmsMsgHeader) + msg->dataLength);
      if (msg == NULL)
      {
         cmsLog_error("realloc to %d bytes failed", sizeof(CmsMsgHeader) + msg->dataLength);
         cmsMem_free(msg);
         return CMSRET_RESOURCE_EXCEEDED;
      }

      inBuf = (char *) (msg + 1);
      while (totalReadSoFar < msg->dataLength)
      {
         cmsLog_debug("reading segment: soFar=%d total=%d", totalReadSoFar, totalRemaining);
         if (timeout)
         {
            if ((ret = waitForDataAvailable(fd, *timeout)) != CMSRET_SUCCESS)
            {
               cmsMem_free(msg);
               return ret;
            }
         }

         rc = read(fd, inBuf, totalRemaining);
         if (rc <= 0)
         {
            cmsLog_error("bad data read, rc=%d errno=%d readSoFar=%d remaining=%d", rc, errno, totalReadSoFar, totalRemaining);
            cmsMem_free(msg);
            return CMSRET_INTERNAL_ERROR;
         }
         else
         {
            inBuf += rc;
            totalReadSoFar += rc;
            totalRemaining -= rc;
         }
      }
   }

   *buf = msg;

   return CMSRET_SUCCESS;
}

