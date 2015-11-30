/*

bftpd Copyright (C) 1999-2003 Max-Wilhelm Bruker

This program is is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2 of the
License as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include <config.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_ASM_SOCKET_H
#include <asm/socket.h>
#endif
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_WAIT_H
# include <wait.h>
#else
# ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
# endif
#endif

#include "main.h"
#include "cwd.h"
#include "mystring.h"
#include "logging.h"
#include "dirlist.h"
#include "bftpdutmp.h"
#include "options.h"
#include "login.h"
#include "list.h"

//brcm
#include "cms_util.h"
#include "cms_msg.h"
void *msgHandle = NULL;

#define FTPD_TIMEOUT    300

// brcm int global_argc;
//brcm char **global_argv;
struct sockaddr_in name;
int isparent = 1;
int sock     = -1;
int listensocket;
// brcm FILE *passwdfile = NULL, *groupfile = NULL, *devnull;
struct sockaddr_in remotename;
char *remotehostname;
int control_timeout, data_timeout;
int alarm_type = 0;

//inetd struct bftpd_list_element *child_list;

/* Command line parameters */
char *configpath = NULL;    
// int daemonmode = 1;         // brcm was 0;
NetworkAccessMode accessMode; // needed by login.c for authentication

/* interface name from socket */
char connIfName[CMS_IFNAME_LENGTH]={0};

#if 0 // brcm bengin
void print_file(int number, char *filename)
{
	FILE *phile;
	char foo[256];
	phile = fopen(filename, "r");
	if (phile) {
		while (fgets(foo, sizeof(foo), phile)) {
			foo[strlen(foo) - 1] = '\0';
			control_printf(SL_SUCCESS, "%i-%s", number, foo);
		}
		fclose(phile);
	}
}
#endif // brcm end

void end_child()
{
#if 0 //brcm	
   if (passwdfile)
		fclose(passwdfile);
	if (groupfile)
		fclose(groupfile);

	config_end();
	bftpd_log("Quitting.\n");
   bftpd_statuslog(1, 0, "quit");
   // brcm bftpdutmp_end();
	log_end();
	login_end();
   bftpd_cwd_end();

   if (daemonmode) {
      close(sock);
      close(0);
		close(1);
		close(2);
	}
#endif //brcm
}

#if 0 //inetd
void handler_sigchld(int sig)
{
	pid_t pid;
	int i;
	struct bftpd_childpid *childpid;

	pid = wait(NULL);					/* Get the child's return code so that the zombie dies */

	for (i = 0; i < bftpd_list_count(child_list); i++) {
		childpid = bftpd_list_get(child_list, i);
		if (childpid->pid == pid) {
			close(childpid->sock);
			bftpd_list_del(&child_list, i);
			free(childpid);
         break;
		}
	}
}
#endif

void handler_sigterm(int signum __attribute__((unused)))
{
	exit(0);					/* Force normal termination so that end_child() is called */
}

void handler_sigalrm(int signum __attribute__((unused)))
{
    if (alarm_type) {
        close(alarm_type);
//brcm        bftpd_log("Kicked from the server due to data connection timeout.\n");
        control_printf(SL_FAILURE, "421 Kicked from the server due to data connection timeout.");
        exit(0);
    } else {
//brcm        bftpd_log("Kicked from the server due to control connection timeout.\n");
        control_printf(SL_FAILURE, "421 Kicked from the server due to control connection timeout.");
        exit(0);
    }
}
#if 0 //brcm
void init_everything()
{

	if (!daemonmode) {
		config_init();
        hidegroups_init();
    }

	log_init();
    bftpdutmp_init();
	login_init();
}
#endif //brcm

int main(int argc, char **argv)
{
	char str[MAXCMD + 1];
	static struct hostent *he;
	UINT32 i = 1;
   int retval;
   int logLevelNum;
   CmsLogLevel logLevel;
   CmsRet ret;
   
#if 0 // brcm begin
	while (((retval = getopt(argc, argv, "c:hdDin"))) > -1) {
		switch (retval) {
			case 'h':
				printf(
					"Usage: %s [-h] [-i|-d|-D] [-c <filename>|-n]\n"
					"-h print this help\n"
					"-i (default) run from inetd\n"
					"-d daemon mode: fork() and run in TCP listen mode\n"
					"-D run in TCP listen mode, but don't pre-fork()\n"
					"-c read the config file named \"filename\" instead of " PATH_BFTPD_CONF "\n"
					"-n no config file, use defaults\n", argv[0]);
				return 0;
			case 'i': daemonmode = 0; break;
			case 'd': daemonmode = 1; break;
			case 'D': daemonmode = 2; break;
			case 'c': configpath = strdup(optarg); break;
			case 'n': configpath = NULL; break;
		}
	}
#endif //brcm end

   cmsLog_init(EID_FTPD);

   while (((retval = getopt(argc, argv, "v:h"))) > -1) {
      switch (retval) {
         case 'h':
            printf(
               "Usage: %s [-v verbosity_level]\n"
               "-h print this help\n"
               "-v verbosity level (2-max, 1-terse, 0-errors only)\n", argv[0]);
            return 0;
         case 'v':
         logLevelNum = atoi(optarg);
         if (logLevelNum == 0)
         {
            logLevel = LOG_LEVEL_ERR;
         }
         else if (logLevelNum == 1)
         {
            logLevel = LOG_LEVEL_NOTICE;
         }
         else
         {
            logLevel = LOG_LEVEL_DEBUG;
         }
         /*
          * By default, CMS logging goes to standard error.  But about 100
          * lines from here, ftpd will do a dup2 of the socket to the client
          * to standard error, so all logging will start going to the client.
          * But the client expects messages in a special ftp format, so
          * our messages will confuse the client.  Bottom line: if you
          * use cmsLog_debug/notice/error, it will only work up to the point
          * ftp does the dup2.  After that, you will need to configure CMS
          * logging to log to syslog to see it.
          */
         cmsLog_setLevel(logLevel);
         break;
      }
   }   

   if ((ret = cmsMsg_init(EID_FTPD, &msgHandle)) != CMSRET_SUCCESS)
   {
      cmsLog_error("Could not initialize msgHandle, ret=%d", ret);
      exit(1);
   }
   
#if 0 //inetd
	if (daemonmode) {
		struct sockaddr_in new;
		struct sockaddr_in myaddr;

		if (daemonmode == 1) {
         if (fork()) {
				exit(0);  /* Exit from parent process */
         }
			setsid();
         int pid = 0;
         if ((pid = fork()) != 0) {
            if (!(pid_fp = fopen("/var/run/ftpd_pid", "w"))) {
               perror("/var/run/ftpd_pid");
               return 0;
            }
            fprintf(pid_fp, "%d\n", pid);
            fclose(pid_fp);
			   return 0;
         }
      }

		signal(SIGCHLD, handler_sigchld);
#if 0 //brcm		
      config_init();
    	chdir("/");
      hidegroups_init();
#endif //brcm

		listensocket = socket(AF_INET, SOCK_STREAM, 0);
#ifdef SO_REUSEADDR
		setsockopt(listensocket, SOL_SOCKET, SO_REUSEADDR, (void *) &i, sizeof(i));
#endif
#ifdef SO_REUSEPORT
		setsockopt(listensocket, SOL_SOCKET, SO_REUSEPORT, (void *) &i, sizeof(i));
#endif
		memset((void *) &myaddr, 0, sizeof(myaddr));
#if 0 //brcm
      if (!((port = strtoul(config_getoption("PORT"), NULL, 10)))) 
         port = 21;
      if (!strcasecmp(config_getoption("BIND_TO_ADDR"), "any")
            || !config_getoption("BIND_TO_ADDR")[0])
			myaddr.sin_addr.s_addr = INADDR_ANY;
		else
			myaddr.sin_addr.s_addr = inet_addr(config_getoption("BIND_TO_ADDR"));
#endif //brcm use default beloww (3 lines)
      port = 21;
		myaddr.sin_port = htons(port);
      myaddr.sin_addr.s_addr = INADDR_ANY;

      if (bind(listensocket, (struct sockaddr *) &myaddr, sizeof(myaddr)) < 0) {
         fprintf(stderr, "Bind failed: %s\n", strerror(errno));
         exit(1);
      }
      if (listen(listensocket, 5)) {
         fprintf(stderr, "Listen failed: %s\n", strerror(errno));
         exit(1);
      }
#if 0
      for (i = 0; i < 3; i++) {
         close(i);		/* Remove fd pointing to the console */
         open("/dev/null", O_RDWR);	/* Create fd pointing nowhere */
      }
#endif
   }
#endif   //inetd

   {
#ifdef DMP_X_BROADCOM_COM_IPV6_1 /* aka SUPPORT_IPV6 */
      struct sockaddr_in6 new;
#else
      struct sockaddr_in new;
#endif
      fd_set          readFds;
      int             selret;

      atexit(end_child);
      signal(SIGTERM, handler_sigterm);
      signal(SIGALRM, handler_sigalrm);
      signal(SIGPIPE, SIG_IGN);

      listensocket = CMS_DYNAMIC_LAUNCH_SERVER_FD;
      
    	FD_ZERO(&readFds);
		FD_SET(listensocket, &readFds);

		/* pend, waiting for one or more fds to become ready */
		selret = select(listensocket+1, &readFds, NULL, NULL, NULL);
      if (selret <= 0)
      {
         exit(0);
      }
      
      i = sizeof(new);
      sock = accept(listensocket, (struct sockaddr *) &new, &i);
      if (sock > 0) {
         char ipAddr[BUFLEN_128] = {0};

#ifdef DMP_X_BROADCOM_COM_IPV6_1 /* aka SUPPORT_IPV6 */
         char ipAddrBuf[BUFLEN_40];

         inet_ntop(AF_INET6, &new.sin6_addr, ipAddrBuf, sizeof(ipAddrBuf));
         cmsLog_debug("client ip=%s", ipAddrBuf);

         /* see if this is a IPv4-Mapped IPv6 address (::ffff:xxx.xxx.xxx.xxx) */
         if (strchr(ipAddrBuf, '.') && strstr(ipAddrBuf, ":ffff:"))
         {
            /* IPv4 client */
            char *v4addr;

            /* convert address to clean ipv4 address */
            v4addr = strrchr(ipAddrBuf, ':') + 1;
               
            strcpy(ipAddr, v4addr);
         }
         else
         {
            /* IPv6 client */
            strcpy(ipAddr, ipAddrBuf);
         }
#else

         strcpy(ipAddr, inet_ntoa(new.sin_addr));

#endif

         cmsLog_debug("checking accessMode for %s", ipAddr);

         accessMode = cmsNet_isAddressOnLanSide(ipAddr) ? 
                          NETWORK_ACCESS_LAN_SIDE : NETWORK_ACCESS_WAN_SIDE;

         cmsLog_debug("accessMode=%d", accessMode);

         /* save the connection interface name for later deciding if
         * it is a WAN or LAN interface in the uploading process
         */
         if (cmsImg_saveIfNameFromSocket(sock, connIfName) != CMSRET_SUCCESS)
         {
            cmsLog_error("Failed to get remote ifc name!");
         }         

         isparent = 0;
         dup2(sock, fileno(stdin));
         dup2(sock, fileno(stderr));
      }
      else
      {
         cmsLog_error("accept of connection failed, exit server.");
         exit(0);
      }
   }



   control_timeout = FTPD_TIMEOUT;
   data_timeout = FTPD_TIMEOUT;
#ifdef SUPPORT_FTPD_STORAGE
   // mwang: our Taiwan intern set this to 512, not clear why.
   xfer_bufsize = 512;
#else
   xfer_bufsize = 4*1024;
#endif

   i = sizeof(remotename);
   if (getpeername(fileno(stderr), (struct sockaddr *) &remotename, &i)) {
      control_printf(SL_FAILURE, "421-Could not get peer IP address.\r\n421 %s.",
		               strerror(errno));
      return 0;
   }

	i = 1;
   setsockopt(fileno(stdin), SOL_SOCKET, SO_OOBINLINE, (void *) &i, sizeof(i));
   setsockopt(fileno(stdin), SOL_SOCKET, SO_KEEPALIVE, (void *) &i, sizeof(i));

   remotehostname = strdup(inet_ntoa(remotename.sin_addr));

#if 0 //brcm
   bftpd_log("Incoming connection from %s.\n", remotehostname);
   bftpd_statuslog(1, 0, "connect %s", remotehostname);
#endif //brcm
   i = sizeof(name);
   getsockname(fileno(stdin), (struct sockaddr *) &name, &i);


//brcm	print_file(220, config_getoption("MOTD_GLOBAL"));

   /* Parse hello message */
   // brcm strcpy(str, (char *) config_getoption("HELLO_STRING"));
   strcpy(str, "Ftp firmware update utility");     //brcm
   //brcm replace(str, "%v", VERSION);
   if (strstr(str, "%h")) {
      if ((he = gethostbyaddr((char *) &name.sin_addr, sizeof(struct in_addr), AF_INET)))
			replace(str, "%h", he->h_name);
		else
			replace(str, "%h", (char *) inet_ntoa(name.sin_addr));
   }

   replace(str, "%i", (char *) inet_ntoa(name.sin_addr));
   control_printf(SL_SUCCESS, "220 %s", str);

   /* Read lines from client and execute appropriate commands */
   while (fgets(str, sizeof(str), stdin)) {
// printf("after while (fgets(str = %s, stdin\n", str);  // brcm
      alarm(control_timeout);
      str[strlen(str) - 2] = 0;
//brcm        bftpd_statuslog(2, 0, "%s", str);
#ifdef DEBUG
//		bftpd_log("Processing command: %s\n", str);
#endif
      parsecmd(str);
   }
   return 0;
}
