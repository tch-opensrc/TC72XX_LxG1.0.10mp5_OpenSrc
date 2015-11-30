#ifndef _MSGAPI_H
#define _MSGAPI_H

#include <ctype.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MAX_SEND_SIZE 256

struct optionmsgbuf {
        long mtype;
        char mtext[MAX_SEND_SIZE];
};

typedef struct ExecIP{
       u_int32_t ipaddr;
	unsigned char execflag;
	u_int32_t index;
}EXECIP, PEXECIP;

typedef struct optioncmd {
	char clsname[16];
	char command[256];
	char action;
	int optionnum;
	char optionval[16];
	struct ExecIP execip[254];
	struct optioncmd *pnext;
}OPTIONCMD, POPTIONCMD;

void send_message(int qid, struct optionmsgbuf *qbuf, long type, char *text);
ssize_t read_message(int qid, struct optionmsgbuf *qbuf, long type);
void remove_queue(int qid);
void change_queue_mode(int qid, char *mode);
int bcmSystemEx (char *command, int printFlag);
char bcmParseCmdAction(char *cmd);
struct optioncmd * bcmFindRelateOptCmd(char *classname, char *cmd);
struct optioncmd * bcmInsertOptCmd(char *classname, char *cmd);
int bcmHandleDeleteRule(classname, cmd);	
int bcmExecOptCmd(struct optioncmd *optcmdnode, int option);
void bcmOptcmdloop(void);
int bcmAddQosIP(struct optioncmd * optcmd, u_int32_t leaseip, int index);
#endif