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

#include "oal.h"
#include "cms_mem.h"

CmsRet prctl_spawnProcess(const SpawnProcessInfo *spawnInfo, SpawnedProcessInfo *procInfo)
{
   return(oal_spawnProcess(spawnInfo, procInfo));
}


CmsRet prctl_collectProcess(const CollectProcessInfo *collectInfo, SpawnedProcessInfo *procInfo)
{
   return (oal_collectProcess(collectInfo, procInfo));
}

CmsRet prctl_terminateProcessGracefully(SINT32 pid)
{
   cmsLog_error("Not implemented yet. (pid=%d)", pid);

   /* mwang_todo: just send a terminate message to the process ? */
   return CMSRET_INTERNAL_ERROR;
}

CmsRet prctl_terminateProcessForcefully(SINT32 pid)
{
   return (oal_signalProcess(pid, SIGTERM));
}

CmsRet prctl_signalProcess(SINT32 pid, SINT32 sig)
{
   return (oal_signalProcess(pid, sig));
}


/** The original cfm bcmSystem() functions forks a command within a shell.
 *
 * The prctl_spawnProcess cannot handle that, so we have a small hack for
 * supporting that here.
 */
static int runCommandInShell(char *command)
{
   int pid;

   pid = fork();
   if (pid == -1)
   {
      cmsLog_error("fork failed!");
      return -1;
   }

   if (pid == 0)
   {
      /* this is the child */
      int i;
      char *argv[4];

      /* close all of the child's other fd's */
      for (i=3; i <= 50; i++)
      {
         close(i);
      }

      argv[0] = "sh";
      argv[1] = "-c";
      argv[2] = command;
      argv[3] = 0;
      execv("/bin/sh", argv);
      cmsLog_error("Should not have reached here!");
      exit(127);
   }

   /* parent returns the pid */
   return pid;
}


int prctl_runCommandInShellBlocking(char *command)
{
   SpawnedProcessInfo procInfo = {0, 0, 0, 0};
   CollectProcessInfo collectInfo;
   CmsRet ret;

   if ( command == 0 )
      return 1;

   cmsLog_debug("executing %s", command);

   if ((procInfo.pid = runCommandInShell(command)) < 0) {
      cmsLog_error("Could not execute %s", command);
      return 1;
   }

   /*
    * Now fill in info for the collect.
    */
   collectInfo.collectMode = COLLECT_PID; /* block until we collect it */
   collectInfo.pid = procInfo.pid;
   collectInfo.timeout = 0;               /* not applicable since we are COLLECT_PID */
   ret = prctl_collectProcess(&collectInfo, &procInfo);
   if (ret != CMSRET_SUCCESS)
   {
      cmsLog_error("prctl_collect failed, ret=%d", ret);
      /* mwang_todo: should we signal/kill the process? */
      return -1;
   }
   else 
   {
      cmsLog_debug("collected pid %d, sigNum=%d exitcode=%d",
                   procInfo.pid, procInfo.signalNumber, procInfo.exitCode);
      return (procInfo.signalNumber != 0) ? procInfo.signalNumber : procInfo.exitCode;
   }
}


/** Start the command and allow it to run for a limited amount of time.
 *
 * This function was called bcmSystemNoHang.
 */
int prctl_runCommandInShellWithTimeout(char *command)
{
   SpawnedProcessInfo procInfo = {0, 0, 0, 0};
   CollectProcessInfo collectInfo;
   CmsRet ret;

   if ( command == 0 )
      return 1;

   cmsLog_debug("executing %s", command);

   if ((procInfo.pid = runCommandInShell(command)) < 0) {
      cmsLog_error("Could not execute %s", command);
      return 1;
   }

   /*
    * Now fill in info for the collect.
    */
   collectInfo.collectMode = COLLECT_PID_TIMEOUT; /* block for up to specified timeout waiting for pid */
   collectInfo.pid = procInfo.pid;
   collectInfo.timeout = 120 * MSECS_IN_SEC;  /* orig code did usleep(20) for 20000 times. */
   ret = prctl_collectProcess(&collectInfo, &procInfo);
   if (ret != CMSRET_SUCCESS)
   {
      cmsLog_error("prctl_collect failed, ret=%d", ret);
      /* mwang_todo: should we signal/kill the process? */
      return -1;
   }
   else 
   {
      cmsLog_debug("collected pid %d, sigNum=%d exitcode=%d",
                   procInfo.pid, procInfo.signalNumber, procInfo.exitCode);
      return (procInfo.signalNumber != 0) ? procInfo.signalNumber : procInfo.exitCode;
   }
}


