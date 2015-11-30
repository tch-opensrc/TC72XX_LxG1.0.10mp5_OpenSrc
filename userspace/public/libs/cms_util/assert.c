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

#include "cms.h"
#include "cms_ast.h"
#include "cms_log.h"
#include "prctl.h"


void cmsAst_assertFunc(const char *filename, UINT32 lineNumber, const char *exprString, SINT32 expr)
{

   if (expr == 0)
   {
      cmsLog_error("assertion \"%s\" failed at %s:%d", exprString, filename, lineNumber);

#ifndef NDEBUG
      /* Send SIGABRT only if NDEBUG is not defined */
      prctl_signalProcess(getpid(), SIGABRT);
#endif
   }

}




