#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include "main.h"
#include "bftpdutmp.h"
#include "mypaths.h"
#include "logging.h"
#include "options.h"

FILE *bftpdutmp = NULL;
long bftpdutmp_offset = 0xFFFFFFFF;

void bftpdutmp_init()
{
	char *filename = strdup(config_getoption("PATH_BFTPDUTMP"));
	if ((!strcasecmp(filename, "none")) || (!filename[0]))
		return;
    /* First we have to create the file if it doesn't exist */
    bftpdutmp = fopen(filename, "a");
    if (bftpdutmp)
        fclose(bftpdutmp);
    /* Then we can open it for reading and writing */
    if (!(bftpdutmp = fopen(filename, "r+"))) {
        control_printf(SL_FAILURE, "421-Could not open %s\r\n"
                 "421 Server disabled for security reasons.", filename);
        exit(1);
    }
    rewind(bftpdutmp);
}

void bftpdutmp_end()
{
    if (bftpdutmp) {
        if (bftpdutmp_offset != -1)
            bftpdutmp_log(0);
        fclose(bftpdutmp);
        bftpdutmp = NULL;
    }
}

void bftpdutmp_log(char type)
{
    struct bftpdutmp ut, tmp;
    long i;
    if (!bftpdutmp)
        return;
    memset((void *) &ut, 0, sizeof(ut));
    ut.bu_pid = getpid();
    if (type) {
        ut.bu_type = 1;
        strncpy(ut.bu_name, user, sizeof(ut.bu_name));
        strncpy(ut.bu_host, remotehostname, sizeof(ut.bu_host));
       /* Determine offset of first user marked dead */
        rewind(bftpdutmp);
        i = 0;
        while (fread((void *) &tmp, sizeof(tmp), 1, bftpdutmp)) {
            if (!tmp.bu_type)
                break;
            i++;
        }
        bftpdutmp_offset = i * sizeof(tmp);
    } else
        ut.bu_type = 0;
    time(&(ut.bu_time));
    fseek(bftpdutmp, bftpdutmp_offset, SEEK_SET);
    fwrite((void *) &ut, sizeof(ut), 1, bftpdutmp);
    fflush(bftpdutmp);
}

char bftpdutmp_pidexists(pid_t pid)
{
    struct bftpdutmp tmp;
	if (!bftpdutmp)
		return 0;
    rewind(bftpdutmp);
    while (fread((void *) &tmp, sizeof(tmp), 1, bftpdutmp)) {
        if ((tmp.bu_pid == pid) && (tmp.bu_type))
            return 1;
    }
    return 0;
}

int bftpdutmp_usercount(char *username)
{
    struct bftpdutmp tmp;
	int count = 0;
	if (!bftpdutmp)
		return 0;
    rewind(bftpdutmp);
    while (fread((void *) &tmp, sizeof(tmp), 1, bftpdutmp)) {
		bftpd_log("bu_name=%s; username=%s, bu_type=%i\n", tmp.bu_name, username, tmp.bu_type);
        if (tmp.bu_type && (!strcmp(tmp.bu_name, username) || !strcmp(username, "*")))
			count++;
    }
    return count;
}
