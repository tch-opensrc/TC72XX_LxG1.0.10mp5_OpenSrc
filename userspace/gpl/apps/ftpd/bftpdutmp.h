#ifndef __BFTPD_BFTPDUTMP_H
#define __BFTPD_BFTPDUTMP_H

#include "commands.h"
#include <sys/types.h>

extern FILE *bftpdutmp;

struct bftpdutmp {
    char bu_type;
    pid_t bu_pid;
    char bu_name[USERLEN + 1];
    char bu_host[256];
    time_t bu_time;
};

void bftpdutmp_init();
void bftpdutmp_end();
void bftpdutmp_log(char type);
char bftpdutmp_pidexists(pid_t pid);
int bftpdutmp_usercount(char *username);

#endif
