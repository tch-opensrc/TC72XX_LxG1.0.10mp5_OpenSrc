/***********************************************************************
//
//  Copyright (c) 2006  Broadcom Corporation
//  All Rights Reserved
//  No portions of this material may be reproduced in any form without the
//  written permission of:
//          Broadcom Corporation
//          16215 Alton Parkway
//          Irvine, California 92619
//  All information contained in this document is Broadcom Corporation
//  company private, proprietary, and trade secret.
//
************************************************************************/

#include "../oal.h"

/** OS dependent logging functions go in this file.
 */
void oalLog_init(void)
{
   openlog(NULL, 0, LOG_DAEMON);
   return;
}

void oalLog_syslog(CmsLogLevel level, const char *buf)
{
   syslog(level, buf);
   return;
}

void oalLog_cleanup(void)
{
   closelog();
   return;
}
