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
#include "cms_mem.h"
#include "cms_msg.h"
#include "cms_log.h"
#include "oal.h"

#ifdef DESKTOP_LINUX
UINT16 desktopFakePid = 30;
#endif


/* message API functions go here */


CmsRet cmsMsg_init(CmsEntityId eid, void **msgHandle)
{
   return oalMsg_init(eid, msgHandle);
}

void cmsMsg_cleanup(void **msgHandle)
{
   CmsMsgHandle *handle = (CmsMsgHandle *) *msgHandle;
   CmsMsgHeader *msg;

   /* free any queued up messages */
   while ((msg = handle->putBackQueue) != NULL)
   {
      handle->putBackQueue = msg->next;
      CMSMEM_FREE_BUF_AND_NULL_PTR(msg);
   }

   return oalMsg_cleanup(msgHandle);
}


CmsRet cmsMsg_send(void *msgHandle, const CmsMsgHeader *buf)
{
   CmsMsgHandle *handle = (CmsMsgHandle *) msgHandle;

#ifdef DESKTOP_LINUX
   if (handle->standalone)
   {
      /* just pretend to have sent the message */
      return CMSRET_SUCCESS;
   }
#endif   

   return oalMsg_send(handle->commFd, buf);
}


CmsRet cmsMsg_sendReply(void *msgHandle, const CmsMsgHeader *msg, CmsRet retCode)
{
   CmsMsgHandle *handle = (CmsMsgHandle *) msgHandle;
   CmsMsgHeader replyMsg = EMPTY_MSG_HEADER;

   replyMsg.dst = msg->src;
   replyMsg.src = msg->dst;
   replyMsg.type = msg->type;

   replyMsg.flags_request = 0;
   replyMsg.flags_response = 1;
   replyMsg.flags_bounceIfNotRunning = msg->flags_bounceIfNotRunning;
   /* do we want to copy any other flags? */

   replyMsg.wordData = retCode;

   return oalMsg_send(handle->commFd, &replyMsg);
}



static CmsRet sendAndGetReply(void *msgHandle, const CmsMsgHeader *buf, UINT32 *timeout)
{
   CmsMsgHandle *handle = (CmsMsgHandle *) msgHandle;
   CmsMsgType sentType;
   CmsMsgHeader *replyMsg=NULL;
   UBOOL8 doReceive=TRUE;
   CmsRet ret;

#ifdef DESKTOP_LINUX
   if (handle->standalone)
   {
      CmsMsgHeader *msg = (CmsMsgHeader *) buf;

      /*
       * Standalone mode occurs during unittests.
       * Pretend to send out the message and get a successful reply.
       */
      if ((msg->type == CMS_MSG_START_APP) || (msg->type == CMS_MSG_RESTART_APP))
      {
         /* For the START_APP and RESTART_APP messages, the expected return value is the pid. */
         return desktopFakePid++;
      }
      else
      {
         return CMSRET_SUCCESS;
      }
   }
#endif   

   /* remember what msg type we sent out. */
   sentType = buf->type;

   ret = oalMsg_send(handle->commFd, buf);
   if (ret != CMSRET_SUCCESS)
   {
      return ret;
   }

   while (doReceive)
   {
      ret = oalMsg_receive(handle->commFd, &replyMsg, timeout);
      if (ret != CMSRET_SUCCESS)
      {
         if ((timeout == NULL) ||
             ((timeout != NULL) && (ret != CMSRET_TIMED_OUT)))
         {
            cmsLog_error("error during get of reply, ret=%d", ret);
         }
         cmsMem_free(replyMsg);
         doReceive = FALSE;
      }
      else
      {
         if (replyMsg->type == sentType)
         {
            ret = replyMsg->wordData;
            doReceive = FALSE;
            CMSMEM_FREE_BUF_AND_NULL_PTR(replyMsg);
         }
         else
         {
            /* we got a mesage, but it was not the reply we were expecting.
             * Could be an event msg.  Push it back on the put-back queue and
             * keep trying to get the message we really want.
             */
            cmsMsg_putBack(msgHandle, &replyMsg);
            replyMsg = NULL;

            /*
             * I don't know how much time was used to get this response message
             * we weren't expecting, but approximate some elapsed time by 
             * reducing the timeout by 1/3.  This ensures that we will
             * eventually timeout if we don't get the response msg we are expecting.
             */
            if (timeout != NULL)
            {
               (*timeout) = (*timeout) / 3;
            }
         }
      }
   }

   cmsMsg_requeuePutBacks(msgHandle);

   return ret;
}

static CmsRet sendAndGetReplyBuf(void *msgHandle, const CmsMsgHeader *buf, CmsMsgHeader **replyBuf, UINT32 *timeout)
{
   CmsMsgHandle *handle = (CmsMsgHandle *) msgHandle;
   CmsMsgType sentType;
   CmsMsgHeader *replyMsg=NULL;
   UBOOL8 doReceive=TRUE;
   CmsRet ret;
   int i;

   /* remember what msg type we sent out. */
   sentType = buf->type;

   ret = oalMsg_send(handle->commFd, buf);
   if (ret != CMSRET_SUCCESS)
   {
      return ret;
   }

   while (doReceive)
   {
      ret = oalMsg_receive(handle->commFd, &replyMsg, timeout);
      if (ret != CMSRET_SUCCESS)
      {
         if ((timeout == NULL) ||
             ((timeout != NULL) && (ret != CMSRET_TIMED_OUT)))
         {
            cmsLog_error("error during get of reply, ret=%d", ret);
         }
         cmsMem_free(replyMsg);
         doReceive = FALSE;
      }
      else
      {
         if (replyMsg->type == sentType)
         {  
            memcpy((*replyBuf), replyMsg, (sizeof(CmsMsgHeader) + replyMsg->dataLength));
            doReceive = FALSE;
            CMSMEM_FREE_BUF_AND_NULL_PTR(replyMsg);
         }
         else
         {
            /* we got a mesage, but it was not the reply we were expecting.
             * Could be an event msg.  Push it back on the put-back queue and
             * keep trying to get the message we really want.
             */
            cmsMsg_putBack(msgHandle, &replyMsg);
            replyMsg = NULL;

            /*
             * I don't know how much time was used to get this response message
             * we weren't expecting, but approximate some elapsed time by 
             * reducing the timeout by 1/3.  This ensures that we will
             * eventually timeout if we don't get the response msg we are expecting.
             */
            if (timeout != NULL)
            {
               (*timeout) = (*timeout) / 3;
            }
         }
      }
   }

   cmsMsg_requeuePutBacks(msgHandle);

   return ret;
}


CmsRet cmsMsg_sendAndGetReply(void *msgHandle, const CmsMsgHeader *buf)
{
   return (sendAndGetReply(msgHandle, buf, NULL));
}


CmsRet cmsMsg_sendAndGetReplyWithTimeout(void *msgHandle,
                                         const CmsMsgHeader *buf,
                                         UINT32 timeoutMilliSeconds)
{
   UINT32 timeout = timeoutMilliSeconds;

   return (sendAndGetReply(msgHandle, buf, &timeout));
}

CmsRet cmsMsg_sendAndGetReplyBuf(void *msgHandle, const CmsMsgHeader *buf, CmsMsgHeader **replyBuf)
{
   return (sendAndGetReplyBuf(msgHandle, buf, replyBuf, NULL)); 
}

CmsRet cmsMsg_sendAndGetReplyBufWithTimeout(void *msgHandle, const CmsMsgHeader *buf, CmsMsgHeader **replyBuf, UINT32 timeoutMilliSeconds)
{
   UINT32 timeout = timeoutMilliSeconds;

   return (sendAndGetReplyBuf(msgHandle, buf, replyBuf, &timeout)); 
}

CmsRet cmsMsg_receive(void *msgHandle, CmsMsgHeader **buf)
{
   CmsMsgHandle *handle = (CmsMsgHandle *) msgHandle;

#ifdef DESKTOP_LINUX
   if (handle->standalone)
   {
      /*
       * Hmm, this is a tricky situation.  Caller has told us to block until
       * we get a message, but since smd is not running, we will never get
       * a message.  Return INTERNAL_ERROR and let caller handle it?
       */
      cmsLog_error("cannot receive msg while in standalone (unittest) mode");
      *buf = NULL;
      return CMSRET_INTERNAL_ERROR;
   }
#endif   

   return oalMsg_receive(handle->commFd, buf, NULL);
}


CmsRet cmsMsg_receiveWithTimeout(void *msgHandle, CmsMsgHeader **buf, UINT32 timeoutMilliSeconds)
{
   CmsMsgHandle *handle = (CmsMsgHandle *) msgHandle;
   UINT32 timeout = timeoutMilliSeconds;

#ifdef DESKTOP_LINUX
   if (handle->standalone)
   {
      *buf = NULL;
      return CMSRET_TIMED_OUT;
   }
#endif   

   return oalMsg_receive(handle->commFd, buf, &timeout);
}


CmsRet cmsMsg_getEventHandle(const void *msgHandle, void *eventHandle)
{
   return (oalMsg_getEventHandle((CmsMsgHandle *) msgHandle, eventHandle));
}


CmsEntityId cmsMsg_getHandleEid(const void *msgHandle)
{
   const CmsMsgHandle *handle = (const CmsMsgHandle *) msgHandle;

   return (handle == NULL ? 0 : handle->eid);
}


CmsMsgHeader *cmsMsg_duplicate(const CmsMsgHeader *msg)
{
   UINT32 totalLen;
   void *newMsg;

   totalLen = sizeof(CmsMsgHeader) + msg->dataLength;
   newMsg = cmsMem_alloc(totalLen, 0);
   if (newMsg != NULL)
   {
      memcpy(newMsg, msg, totalLen);
   }

   return newMsg;
}


void cmsMsg_putBack(void *msgHandle, CmsMsgHeader **buf)
{
   CmsMsgHandle *handle = (CmsMsgHandle *) msgHandle;
   CmsMsgHeader *prevMsg;

   (*buf)->next = NULL;

   /* put the new message at the end of the putBackQueue */
   if (handle->putBackQueue == NULL)
   {
      handle->putBackQueue = (*buf);
   }
   else
   {
      prevMsg = handle->putBackQueue;
      while (prevMsg->next != NULL)
      {
         prevMsg = prevMsg->next;
      }

      prevMsg->next = (*buf);
   }

   /* we've taken ownership of this msg, so null out caller's pointer */
   *buf = NULL;

   return;
}


void cmsMsg_requeuePutBacks(void *msgHandle)
{
   CmsMsgHandle *handle = (CmsMsgHandle *) msgHandle;
   CmsMsgHeader *msg;

   while ((msg = handle->putBackQueue) != NULL)
   {
      handle->putBackQueue = msg->next;
      msg->next = NULL;

      msg->flags_requeue = 1;

      oalMsg_send(handle->commFd, msg);
   }

   return;
}

