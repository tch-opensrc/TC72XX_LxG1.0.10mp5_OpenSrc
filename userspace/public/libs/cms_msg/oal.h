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


#ifndef __OAL_H__
#define __OAL_H__


#include "cms.h"
#include "cms_eid.h"
#include "cms_msg.h"


/** This is the internal structure of the message handle.
 *
 * It is highly OS dependent.  Management applications should not
 * use any of its fields directly.
 *
 */
typedef struct
{
   CmsEntityId  eid;        /**< Entity id of the owner of this handle. */
   SINT32       commFd;     /**< communications fd */
   UBOOL8       standalone; /**< are we running without smd, for unittests */
   CmsMsgHeader *putBackQueue;  /**< Messages pushed back into the handle. */
} CmsMsgHandle;



/** Initialize messaging system.
 *
 * Same semantics as cmsMsg_init().
 * 
 * @param eid       (IN)  Entity id of the calling process.
 * @param msgHandle (OUT) msgHandle.
 *
 * @return CmsRet enum.
 */
CmsRet oalMsg_init(CmsEntityId eid, void **msgHandle);


/** Clean up messaging system.
 *
 * Same semantics as cmsMsg_cleanup().
 * @param msgHandle (IN) This was the msg_handle that was
 *                       created by cmsMsg_init().
 */
void oalMsg_cleanup(void **msgHandle);



/** Send a message (blocking).
 *
 * Same semantics as cmsMsg_send().
 *
 * @param fd        (IN) The commFd to send the msg out of.
 * @param buf       (IN) This buf contains a CmsMsgHeader and possibly
 *                       more data depending on the message type.
 * @return CmsRet enum.
 */
CmsRet oalMsg_send(SINT32 fd, const CmsMsgHeader *buf);


/** Receive a new message from fd.
 *
 * @param fd         (IN) commFd to receive input from.
 * @param buf       (OUT) Returns a pointer to message buffer.  Caller is responsible
 *                        for freeing the buffer.
 * @param timeout    (IN) Pointer to UINT32, specifying the timeout in milliseconds.
 *                        If pointer is NULL, receive will block until a message is
 *                        received.
 *
 * @return CmsRet enum.
 */
CmsRet oalMsg_receive(SINT32 fd, CmsMsgHeader **buf, UINT32 *timeout);


/** Get operating system dependent handle for receive message notification.
 *
 * Same semantics as cmsMsg_getEventHandle();
 * @param msgHandle    (IN) This was the msgHandle created by cmsMsg_init().
 * @param eventHandle (OUT) This is the OS dependent event handle.  For LINUX,
 *                          eventHandle is the file descriptor number.
 * @return CmsRet enum.
 */
CmsRet oalMsg_getEventHandle(const CmsMsgHandle *msgHandle, void *eventHandle);



#endif /* __OAL_H__ */

