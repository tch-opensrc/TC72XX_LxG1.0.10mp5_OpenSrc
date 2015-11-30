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
#include <unistd.h>


/** OS dependent pid functions go in this file.
 */
SINT32 oal_getPid(void)
{
   return (getpid());
}
