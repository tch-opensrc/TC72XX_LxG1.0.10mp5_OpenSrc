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

#include <stdlib.h>
#include "../oal.h"


void *oal_malloc(UINT32 size)
{
   return malloc(size);
}


void oal_free(void *buf)
{
   free(buf);
}


