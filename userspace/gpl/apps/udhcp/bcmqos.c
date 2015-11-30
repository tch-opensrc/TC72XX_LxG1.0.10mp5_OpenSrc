/***********************************************************************
 *
 *  Copyright (c) 2010  Broadcom Corporation
 *  All Rights Reserved
 *
<:license-private
 *
************************************************************************/
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include "dhcpd.h"
#include "cms_log.h"

/* Externs */
extern struct iface_config_t *iface_config;

typedef struct ExecIP {
   u_int32_t ipaddr;
   unsigned char execflag;
   u_int32_t index;
}EXECIP, PEXECIP;

typedef struct optioncmd {
   char command[1024];
   char action;
   int optionnum;
   char optionval[16];
   struct ExecIP execip[254];
   struct optioncmd *pnext;
}OPTIONCMD, POPTIONCMD;

struct optioncmd *optioncmdHead = NULL;

void bcmDelObsoleteRules(void);
void bcmExecOptCmd(void);
void bcmQosDhcp(int optionnum, char *cmd);

static char bcmParseCmdAction(char *cmd);
static void bcmSetQosRule(char action, char *command, u_int32_t leaseip);
static void bcmAddOptCmdIP(struct optioncmd * optcmd, u_int32_t leaseip, int index);
static struct optioncmd * bcmAddOptCmd(int optionnum, char action, char *cmd);
static void bcmDelOptCmd(char *cmd);


char bcmParseCmdAction(char *cmd)
{
   char *token;
   char action = '\0';

   if ((token = strstr(cmd, "-A ")) == NULL)
   {
      if ((token = strstr(cmd, "-I ")) == NULL)
      {
         token = strstr(cmd, "-D ");
      }
   }
   if (token != NULL)
   {
      action = token[1];

      /* replace the command token with %s */
      token[0] = '%';
      token[1] = 's';
   }

   return action;

}  /* End of bcmParseCmdAction() */

void bcmSetQosRule(char action, char *command, u_int32_t leaseip)
{
   char *ptokenstart;
   char cmdseg[1024];
   char actionStr[3];   /* -A or -I or -D */
     
   strcpy(cmdseg, command);
   ptokenstart = strstr(cmdseg, "[");
   strcpy(ptokenstart, inet_ntoa(leaseip));
   strcat(cmdseg, strstr(command, "]") + 1);
   sprintf(actionStr, "-%c", action);
   sprintf(cmdseg, cmdseg, actionStr);
   system(cmdseg);
    
}  /* End of bcmSetQosRule() */

void bcmAddOptCmdIP(struct optioncmd * optcmd, u_int32_t leaseip, int index)
{
   /* if lease ip address is the same and the QoS rule has been executed, do nothing */  
   if (optcmd->execip[index].ipaddr != leaseip || !optcmd->execip[index].execflag)
   {
      if (optcmd->execip[index].execflag)
      {
         /* delete the QoS rule with the old lease ip */
         bcmSetQosRule('D', optcmd->command, optcmd->execip[index].ipaddr);
         optcmd->execip[index].execflag = 0;
      }
      optcmd->execip[index].ipaddr = leaseip;
      optcmd->execip[index].execflag = 1;

      /* add QoS rule with the new lease ip */
      bcmSetQosRule(optcmd->action, optcmd->command, leaseip);
   }
}  /* End of bcmAddOptCmdIP() */

void bcmExecOptCmd(void)
{
   struct optioncmd *pnode;
	struct iface_config_t *iface;
	uint32_t i;

   /* execute all the commands in the option command list */
   for (pnode = optioncmdHead; pnode != NULL; pnode = pnode->pnext)
   {
	   for (iface = iface_config; iface; iface = iface->next)
	   {
		   for (i = 0; i < iface->max_leases; i++)
		   {
            /* skip if lease expires */
            if (lease_expired(&(iface->leases[i])))
               continue;

			   switch (pnode->optionnum)
			   {
				   case DHCP_VENDOR:
					   if (!strcmp(iface->leases[i].vendorid,	pnode->optionval))
					   {
						   bcmAddOptCmdIP(pnode, iface->leases[i].yiaddr, i);
					   }
					   break;
				   case DHCP_CLIENT_ID:
					   //printf("op61 not implement, please use the MAC filter\r\n");
					   break;
				   case DHCP_USER_CLASS_ID:
					   if (!strcmp(iface->leases[i].classid, pnode->optionval))
					   {
						   bcmAddOptCmdIP(pnode, iface->leases[i].yiaddr, i);
					   }
					   break;
				   default:
					   break;
			   }	
		   }
	   }
   }
}  /* End of bcmExecOptCmd() */

struct optioncmd * bcmAddOptCmd(int optionnum, char action, char *cmd)
{
   struct optioncmd *p, *pnode;
   char *ptokenstart, *ptokenend;

   for (pnode = optioncmdHead; pnode != NULL; pnode = pnode->pnext)
   {
      if (!strcmp(pnode->command, cmd))
         return NULL;
   }	

   pnode = (struct optioncmd *)malloc(sizeof(struct optioncmd));
   if ( pnode == NULL )
   {
      cmsLog_error("malloc failed");
      return NULL;
   }

   memset(pnode, 0, sizeof(struct optioncmd));	
   strcpy(pnode->command, cmd);
   pnode->action = action;
   pnode->optionnum = optionnum;
   ptokenstart = strstr(cmd, "[");
   ptokenend = strstr(cmd, "]");
   strncpy(pnode->optionval, ptokenstart + 1, (size_t)(ptokenend - ptokenstart - 1));
   pnode->optionval[ptokenend - ptokenstart - 1] = '\0';
   p = optioncmdHead;	
   optioncmdHead = pnode;
   optioncmdHead->pnext = p;
   return pnode;

}  /* End of bcmAddOptCmd() */

void bcmDelOptCmd(char *cmd)
{
   struct optioncmd *pnode, *pprevnode;
   int i;

   pnode = pprevnode = optioncmdHead;
   for ( ; pnode != NULL;)
   {
      if (!strcmp(pnode->command, cmd))
      {
         /* delete all the ebtables or iptables rules that had been executed */
         for (i = 0; i < 254; i++)
         {
            if (pnode->execip[i].execflag)
            {
               bcmSetQosRule('D', pnode->command, pnode->execip[i].ipaddr);
               pnode->execip[i].execflag = 0;
            }
         }

         /* delete the option command node from the list */	       
         if (optioncmdHead == pnode)
            optioncmdHead = pnode->pnext;
         else
            pprevnode->pnext = pnode->pnext;
         free(pnode);
         break;
      }
      else
      {
         pprevnode = pnode;
         pnode = pnode->pnext;
      }
   }
}  /* End of bcmDelOptCmd() */

void bcmQosDhcp(int optionnum, char *cmd)
{
   char action;

	action = bcmParseCmdAction(cmd);

	switch (action)
	{
		case 'A':
		case 'I':
			if (bcmAddOptCmd(optionnum, action, cmd) != NULL)
            bcmExecOptCmd();
         else
            cmsLog_error("bcmAddOptCmd returns error");
			break;
		case 'D':
			bcmDelOptCmd(cmd);
			break;
		default:
			cmsLog_error("incorrect command action");
			break;
	}
}  /* End of bcmQosDhcp() */

void bcmDelObsoleteRules(void)
{
   struct optioncmd *pnode;
	struct iface_config_t *iface;
   uint32_t delete;
   uint32_t i;

   for (pnode = optioncmdHead; pnode != NULL; pnode = pnode->pnext)
   {
      delete = 1;      
	   for (iface = iface_config; iface && delete; iface = iface->next)
	   {
		   for (i = 0; (i < iface->max_leases) && delete; i++)
		   {
            if (lease_expired(&(iface->leases[i])))
               continue;

			   switch (pnode->optionnum)
			   {
				   case DHCP_VENDOR:
					   if (!strcmp(iface->leases[i].vendorid,	pnode->optionval))
                     delete = 0;
					   break;
				   case DHCP_CLIENT_ID:
					   //printf("op61 not implement, please use the MAC filter\r\n");
					   break;
				   case DHCP_USER_CLASS_ID:
					   if (!strcmp(iface->leases[i].classid, pnode->optionval))
                     delete = 0;
					   break;
				   default:
					   break;
			   }	
		   }
	   }

      if (delete)
      {
         /* delete all the ebtables or iptables rules that had been executed */
         for (i = 0; i < 254; i++)
         {
            if (pnode->execip[i].execflag)
            {
               bcmSetQosRule('D', pnode->command, pnode->execip[i].ipaddr);
               pnode->execip[i].execflag = 0;
            }
         }
      }
   }
}  /* End of bcmDelObsoleteRules() */

