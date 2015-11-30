/*
<:copyright-gpl 
 Copyright 2009 Broadcom Corp. All Rights Reserved. 
 
 This program is free software; you can distribute it and/or modify it 
 under the terms of the GNU General Public License (Version 2) as 
 published by the Free Software Foundation. 
 
 This program is distributed in the hope it will be useful, but WITHOUT 
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License 
 for more details. 
 
 You should have received a copy of the GNU General Public License along 
 with this program; if not, write to the Free Software Foundation, Inc., 
 59 Temple Place - Suite 330, Boston MA 02111-1307, USA. 
:>
*/

/**************************************************************************
 * File Name  : bcmnetlink.h
 *
 * Description: This file defines broadcom specific netlink message types
 ***************************************************************************/
#ifndef _BCMNETLINK_H
#define _BCMNETLINK_H

#include<linux/netlink.h>

#ifndef NETLINK_BRCM_MONITOR
#define NETLINK_BRCM_MONITOR 25
#endif

/* message types exchanged using NETLINK_BRCM_MONITOR */
#define MSG_NETLINK_BRCM_WAKEUP_MONITOR_TASK 0X1000

#define MSG_NETLINK_BRCM_LINK_STATUS_CHANGED 0X2000


extern void kerSysSendtoMonitorTask(int msgType, char *msgData, int msgDataLen);

#endif /*_BCMNETLINK_H */
