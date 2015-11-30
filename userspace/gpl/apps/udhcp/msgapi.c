#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "msgapi.h"
#include <unistd.h>
#include <errno.h>
#include "dhcpd.h"
extern char **environ;
extern struct optioncmd *optioncmdHead;
void send_message(int qid, struct optionmsgbuf *qbuf, long type, char *text)
{
        /* Send a message to the queue */
        //printf("Sending a message ...\n");
        qbuf->mtype = type;
        strcpy(qbuf->mtext, text);

        if((msgsnd(qid, (struct msgbuf *)qbuf,
                strlen(qbuf->mtext)+1, 0)) ==-1)
        {
                perror("msgsnd");
                exit(1);
        }
}

ssize_t read_message(int qid, struct optionmsgbuf *qbuf, long type)
{
        /* Read a message from the queue */
        //printf("Reading a message ...\n");
        qbuf->mtype = type;
        //msgrcv(qid, (struct msgbuf *)qbuf, MAX_SEND_SIZE, type, 0);
        return(msgrcv(qid, (struct msgbuf *)qbuf, MAX_SEND_SIZE, type, IPC_NOWAIT));
        //printf("Type: %ld Text: %s\n", qbuf->mtype, qbuf->mtext);
}
void remove_queue(int qid)
{
        /* Remove the queue */
        msgctl(qid, IPC_RMID, 0);
}

void change_queue_mode(int qid, char *mode)
{
        struct msqid_ds myqueue_ds;

        /* Get current info */
        msgctl(qid, IPC_STAT, &myqueue_ds);

        /* Convert and load the mode */
        sscanf(mode, "%ho", &myqueue_ds.msg_perm.mode);

        /* Update the mode */
        msgctl(qid, IPC_SET, &myqueue_ds);
}



void bcmHidePassword(char *command) {
   char *ptr = NULL;
   char * begin, *end;
   int len = 0;

   /* pppd -i .....  -p password */
   if ((ptr = strstr(command,"pppd")) != NULL) {
     if (!strstr(ptr, "-p")) 
        return;
     begin = strstr(ptr,"-p") + 3;
     end = strchr(begin,' ');
     if (end == NULL) 
       len = strlen(begin);
     else 
       len = end - begin;
   }

   while (len > 0) {
      *begin = '*';
      begin++; len--;
   }
}


/***************************************************************************
// Function Name: bcmSystem().
// Description  : launch shell command in the child process.
// Parameters   : command - shell command to launch.
// Returns      : status 0 - OK, -1 - ERROR.
****************************************************************************/
int bcmSystemEx (char *command, int printFlag) {
   int pid = 0, status = 0;
   char *newCommand = NULL;

   if ( command == 0 )
      return 1;
   pid = fork();
   if ( pid == -1 )
      return -1;

   if ( pid == 0 ) {
      char *argv[4];
      argv[0] = "sh";
      argv[1] = "-c";
      argv[2] = command;
      argv[3] = 0;
//#ifdef BRCM_DEBUG
      if (printFlag)
         printf("dhcp: %s\r\n", command);
//#endif
      if (printFlag) {
        if ((newCommand = strdup(command)) != NULL) {
           bcmHidePassword(newCommand);
           free(newCommand);
        }
      }
      execve("/bin/sh", argv, environ);
      exit(127);
   }

   /* wait for child process return */
   do {
      if ( waitpid(pid, &status, 0) == -1 ) {
         if ( errno != EINTR )
            return -1;
      } else
         return status;
   } while ( 1 );

   return status;
}


char bcmParseCmdAction(char *cmd)
{
    char *ptoken, table[16], action[2];
    sscanf(cmd, "ebtables -t %s %s", table, action);
    return action[1];
}
	
struct optioncmd * bcmFindRelateOptCmd(char *classname, char *cmd)
{
    struct optioncmd *p = optioncmdHead;
    char *psrc, *pdst;
	
    for(;;p = p->pnext){
      if( p == NULL )
	  	return NULL;
      	  
      if( !strcmp(p->clsname, classname) )
	  	break;
    }

    	
    return NULL;
}


struct optioncmd * bcmInsertOptCmd(char *classname, char *cmd)
{
    struct optioncmd *p, *pnode;
    char *ptokenstart, *ptokenend;
    char op[16];
    for( pnode = optioncmdHead; pnode != NULL; pnode = pnode->pnext){
        if( !strcmp(pnode->clsname, classname) && !strcmp(pnode->command, cmd) )
		return NULL;
    }	
    ptokenstart = strstr(cmd, "sipop");
    if( NULL == ptokenstart )
		return NULL;
    pnode = (struct optioncmd *)malloc(sizeof(struct optioncmd));
    if ( pnode == NULL ) return NULL;
    memset(pnode, 0, sizeof(struct optioncmd));	
    strcpy(pnode->clsname, classname);
    strcpy(pnode->command, cmd);
    pnode->action = bcmParseCmdAction(cmd);
    sscanf(ptokenstart, "%s", op);
#if 0 //JJC
    switch(atoi(op+5)){
		case 1:
			pnode->optionnum = 60; 
			break;
		case 2: 
			pnode->optionnum = 77; 
			break;
		case 3:
			pnode->optionnum = 61; 
			break;
		default:
			pnode->optionnum = 0; 
			break;			
    }
#endif
    pnode->optionnum = atoi(op+5);
    ptokenstart = strstr(cmd, "[");
    ptokenend = strstr(cmd, "]");
    strncpy(pnode->optionval, ptokenstart + 1, (size_t)(ptokenend - ptokenstart - 1));
    pnode->optionval[ptokenend - ptokenstart - 1] = '\0';
    p = optioncmdHead;	
    optioncmdHead = pnode;
    optioncmdHead->pnext = p;
    return pnode;
}

int bcmAddQosIP(struct optioncmd * optcmd, u_int32_t leaseip, int index)
{  
    if( optcmd == NULL)
		return -1;
    if(optcmd->execip[index].ipaddr != leaseip){         
	  if(optcmd->execip[index].execflag == 1)
	  	bcmQoscmdFinishIP(optcmd, optcmd->execip[index].ipaddr);
         optcmd->execip[index].ipaddr = leaseip;
	  optcmd->execip[index].execflag = 1;
	  bcmQoscmdExecuteIP(optcmd, leaseip);
	  return 1;
    }
    return 0;
}


int bcmQoscmdExecuteIP(struct optioncmd *optcmd, u_int32_t leaseip)
{

  char *ptokenstart;
  char *pInsert, *pAppend;
  char cmdseg[256];
  char *command = optcmd->command;  
  strcpy(cmdseg, command);
  ptokenstart = strstr(cmdseg, "sipop");
  strcpy(ptokenstart, inet_ntoa(leaseip));
  //strcat(ptokenstart, " ");
  strcat(ptokenstart, strstr(command, "]") + 1);
  bcmSystemEx(cmdseg, 1); 
}


int bcmQoscmdFinishIP(struct optioncmd *optcmd, u_int32_t leaseip)
{

  char *ptokenstart;
  char *pInsert, *pAppend;
  char cmdseg[256];
  char *command = optcmd->command;
  strcpy(cmdseg, command);
  ptokenstart = strstr(cmdseg, "sipop");
  strcpy(ptokenstart, inet_ntoa(leaseip));
  //strcat(ptokenstart, " ");
  strcat(ptokenstart, strstr(command, "]") + 1);
  pInsert = strstr(cmdseg, "-A");
  pAppend = strstr(cmdseg, "-I");
  if(pInsert != NULL)
  	*(pInsert + 1) = 'D';
  else if(pAppend != NULL)
  	*(pAppend + 1) = 'D';
  else 
  	return 0;
  bcmSystemEx(cmdseg, 1); 
}

int bcmExecOptCmd(struct optioncmd *optcmdnode, int option)
{
	struct iface_config_t *iface;
	unsigned int i;

	for (iface = iface_config; iface; iface = iface->next) {
		for (i = 0; i < iface->max_leases; i++){
			switch(option){
				case 60:
					if( !strcmp(iface->leases[i].vendorid,
						optcmdnode->optionval) ){
						bcmAddQosIP(optcmdnode,
							iface->leases[i].yiaddr,
							i);
					}
					break;
				case 61:
					//printf("op61 not implement, please use the MAC filter\r\n");
					break;
				case 77:
					if( !strcmp(iface->leases[i].classid,
						optcmdnode->optionval) ){
						bcmAddQosIP(optcmdnode,
							iface->leases[i].yiaddr,
							i);
					}
					break;
				default:
					break;
			}	
		}
	}
}

int bcmHandleDeleteRule(char *classname, char *cmd)
{
    struct optioncmd *pnode, *pprevnode, *pnext;
    int i;
    pnode  = pprevnode = optioncmdHead;
    for( ;pnode != NULL;){
	  pnext = pnode->pnext; 		
         if( !strcmp(pnode->clsname, classname) ){
             for(i = 0; i <254; i++){
                  if(pnode->execip[i].execflag)
			bcmQoscmdFinishIP(pnode, pnode->execip[i].ipaddr);	  	
             	}
	       
		if(pprevnode == pnode){
                   optioncmdHead = pnode->pnext;
		     pprevnode = optioncmdHead; 
		     free(pnode);
		     pnode = pnext;
		     continue;
		}else{
      		     pprevnode->pnext = pnode->pnext;
      		     free(pnode);
      		     pnode = pprevnode->pnext;
		}
         }else{
              pnode = pnext;
         }
		 
    }
    //printf("sucess delte rule %s\r\n", classname);	
    return 1;
}

void bcmOptcmdloop(void)
{
   struct optioncmd *pnode = optioncmdHead;

   for ( ; pnode != NULL; pnode = pnode->pnext) {
      if (pnode->action == 'A' || pnode->action == 'I') {
         if (pnode->optionnum != 0) {
            //printf("rule name: %s\r\n", pnode->clsname);
            //printf("execute command for command %s\r\n", pnode->command);
            bcmExecOptCmd(pnode, pnode->optionnum);
         }
      }
   }
}

