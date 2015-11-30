#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#define __USE_GNU
#include <unistd.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_ASM_SOCKET_H
#include <asm/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <fcntl.h>
#include <string.h>
#ifdef HAVE_WAIT_H
# include <wait.h>
#else
# ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
# endif
#endif
#ifdef HAVE_SYS_SENDFILE_H
# include <sys/sendfile.h>
#endif

#include "mystring.h"
#include "login.h"
#include "logging.h"
#include "dirlist.h"
#include "options.h"
#include "main.h"
#include "targzip.h"
#include "cwd.h"
#include <syslog.h>

#ifdef HAVE_ZLIB_H
# include <zlib.h>
#else
# undef WANT_GZIP
#endif

int state = STATE_CONNECTED;
char user[USERLEN + 1];
struct sockaddr_in sa;
char pasv = 0;
int sock;
int pasvsock;
char *philename = NULL;
int offset = 0;
short int xfertype = TYPE_BINARY;
int ratio_send = 1, ratio_recv = 1;
int bytes_sent = 0, bytes_recvd = 0;
int epsvall = 0;
int xfer_bufsize;

//brcm begin
#include "cms_util.h"
static char *imagePtr = NULL;
static UINT32 uploadSize = 0;
static CmsImageFormat imageFormat = CMS_IMAGE_FORMAT_INVALID;
extern void *msgHandle;

/* defined in main.c */
extern char connIfName[CMS_IFNAME_LENGTH];

typedef enum
{
    UPLOAD_OK,
    UPLOAD_FAIL_NO_MEM,
    UPLOAD_FAIL_ILLEGAL_IMAGE,
    UPLOAD_FAIL_FLASH,
    UPLOAD_FAIL_FTP,
} UPLOAD_RESULT;
//brcm end

void control_printf(char success __attribute__((unused)), char *format, ...)
{
    char buffer[256];
    va_list val;
    va_start(val, format);
    vsnprintf(buffer, sizeof(buffer), format, val);
    va_end(val);
    fprintf(stderr, "%s\r\n", buffer);
    replace(buffer, "\r", "");
    // brcm bftpd_statuslog(3, success, "%s", buffer);
}

#ifdef SUPPORT_FTPD_STORAGE
void new_umask()
{
    int um;
    char *foo = config_getoption("UMASK");
    if (!foo[0])
        um = 022;
    else
        um = strtoul(foo, NULL, 8);
    umask(um);
}
#endif

void prepare_sock(int sock)
{
    int on = 1;
#ifdef TCP_NODELAY
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void *) &on, sizeof(on));
#endif
#ifdef TCP_NOPUSH
	setsockopt(sock, IPPROTO_TCP, TCP_NOPUSH, (void *) &on, sizeof(on));
#endif
#ifdef SO_REUSEADDR
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &on, sizeof(on));
#endif
#ifdef SO_REUSEPORT
	setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (void *) &on, sizeof(on));
#endif
#ifdef SO_SNDBUF
	on = 65536;
	setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (void *) &on, sizeof(on));
#endif
}

int dataconn()
{
	struct sockaddr foo;
	struct sockaddr_in local;
	size_t namelen = sizeof(foo);
    // brcm int curuid = geteuid();

	memset(&foo, 0, sizeof(foo));
	memset(&local, 0, sizeof(local));

	if (pasv) {
		sock = accept(pasvsock, (struct sockaddr *) &foo, &namelen);
		if (sock == -1) {
            control_printf(SL_FAILURE, "425-Unable to accept data connection.\r\n425 %s.",
                     strerror(errno));
			return 1;
		}
        close(pasvsock);
        prepare_sock(sock);
	} else {
		sock = socket(AF_INET, SOCK_STREAM, 0);
        prepare_sock(sock);
		local.sin_addr.s_addr = name.sin_addr.s_addr;
		local.sin_family = AF_INET;
#if 0 //brcm
        if (!strcasecmp(config_getoption("DATAPORT20"), "yes")) {
            seteuid(0);
            local.sin_port = htons(20);
        }
#endif //brcm
		if (bind(sock, (struct sockaddr *) &local, sizeof(local)) < 0) {
			control_printf(SL_FAILURE, "425-Unable to bind data socket.\r\n425 %s.",
					strerror(errno));
			return 1;
		}
#if 0 //brcm
        if (!strcasecmp(config_getoption("DATAPORT20"), "yes"))
            seteuid(curuid);
#endif //brcm
		sa.sin_family = AF_INET;
		if (connect(sock, (struct sockaddr *) &sa, sizeof(sa)) == -1) {
			control_printf(SL_FAILURE, "425-Unable to establish data connection.\r\n"
                    "425 %s.", strerror(errno));
			return 1;
		}
	}
	control_printf(SL_SUCCESS, "150 %s data connection established.",
	               xfertype == TYPE_BINARY ? "BINARY" : "ASCII");
	return 0;
}

void init_userinfo()
{
    struct passwd *temp = getpwnam(user);
    if (temp) {
        userinfo.pw_name = strdup(temp->pw_name);
        userinfo.pw_passwd = strdup(temp->pw_passwd);
        userinfo.pw_uid = temp->pw_uid;
        userinfo.pw_gid = temp->pw_gid;
        userinfo.pw_gecos = strdup(temp->pw_gecos);
        userinfo.pw_dir = strdup(temp->pw_dir);
        userinfo.pw_shell = strdup(temp->pw_shell);
        userinfo_set = 1;
    }
}

void command_user(char *username)
{

//printf("In command_user username=%s\n", username); // brcm

	// brcm char *alias;
	if (state) {
		control_printf(SL_FAILURE, "503 Username already given.");
		return;
	}
	mystrncpy(user, username, sizeof(user) - 1);
    userinfo_set = 1; /* Dirty! */
#if 0 // brcm 	
    alias = (char *) config_getoption("ALIAS");
    userinfo_set = 0;
	if (alias[0] != '\0')
		mystrncpy(user, alias, sizeof(user) - 1);
#endif //brcm
    init_userinfo();
#ifdef DEBUG
//	bftpd_log("Trying to log in as %s.\n", user);
#endif
#if 0 //brcm
    expand_groups();
	if (!strcasecmp(config_getoption("ANONYMOUS_USER"), "yes"))
		bftpd_login("");
	else {
		state = STATE_USER;
		control_printf(SL_SUCCESS, "331 Password please.");
	}
#endif //brcm
    state = STATE_USER; //brcm
	control_printf(SL_SUCCESS, "331 Password please."); //brcm

//printf("Done command_user username=%s\n", username); // brcm
}

void command_pass(char *password)
{
//printf("In command_pass password=%s\n", password); // brcm
	if (state > STATE_USER) {
		control_printf(SL_FAILURE, "503 Already logged in.");
		return;
	}
	if (bftpd_login(password)) {
//brcm		bftpd_log("Login as user '%s' failed.\n", user);
		control_printf(SL_FAILURE, "421 Login incorrect.");
      syslog(LOG_WARNING,"104051 FTP Server Login UserName or Password Error\n");
		exit(0);
	}
}

void command_type(char *params)
{
    if ((*params == 'I') || (*params == 'i')) {
      	control_printf(SL_SUCCESS, "200 Transfer type changed to BINARY");
        xfertype = TYPE_BINARY;
    } else {
#ifdef SUPPORT_FTPD_STORAGE
        control_printf(SL_SUCCESS, "200 Transfer type changed to ASCII");
        xfertype = TYPE_ASCII;
#else
        control_printf(SL_FAILURE, "500 Type '%c' not supported. Only support BINARY mode", *params);
#endif
    }
}

#ifdef SUPPORT_FTPD_STORAGE
void command_pwd(char *params)
{
	control_printf(SL_SUCCESS, "257 \"%s\" is the current working directory.",
	               bftpd_cwd_getcwd());

}
#endif

void command_port(char *params) {
  unsigned long a0, a1, a2, a3, p0, p1, addr;
  if (epsvall) {
      control_printf(SL_FAILURE, "500 EPSV ALL has been called.");
      return;
  }
  sscanf(params, "%lu,%lu,%lu,%lu,%lu,%lu", &a0, &a1, &a2, &a3, &p0, &p1);
  addr = htonl((a0 << 24) + (a1 << 16) + (a2 << 8) + a3);
#if 0 //brcm
  if((addr != remotename.sin_addr.s_addr) &&( strncasecmp(config_getoption("ALLOW_FXP"), "yes", 3))) {
      control_printf(SL_FAILURE, "500 The given address is not yours.");
      return;
  }
#endif //brcm
  sa.sin_addr.s_addr = addr;
  sa.sin_port = htons((p0 << 8) + p1);
  if (pasv) {
    close(sock);
    pasv = 0;
  }
  control_printf(SL_SUCCESS, "200 PORT %lu.%lu.%lu.%lu:%lu OK",
           a0, a1, a2, a3, (p0 << 8) + p1);
}



#ifdef SUPPORT_FTPD_STORAGE

void command_pasv(char *foo)
{
	int a1, a2, a3, a4;
   socklen_t namelen;
	struct sockaddr_in localsock;
    if (epsvall) {
        control_printf(SL_FAILURE, "500 EPSV ALL has been called.");
        return;
    }
	pasvsock = socket(AF_INET, SOCK_STREAM, 0);
	sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_family = AF_INET;

    if (!config_getoption("PASSIVE_PORTS") || !strlen(config_getoption("PASSIVE_PORTS"))) {
        /* bind to any port */
        sa.sin_port = 0;
        if (bind(pasvsock, (struct sockaddr *) &sa, sizeof(sa)) == -1) {
            control_printf(SL_FAILURE, "425-Error: Unable to bind data socket.\r\n425 %s", strerror(errno));
            return;
        }
	} else {
        int i = 0, success = 0, port;
        for (;;) {
            port = int_from_list(config_getoption("PASSIVE_PORTS"), i++);
            if (port < 0)
                break;
            sa.sin_port = htons(port);
            if (bind(pasvsock, (struct sockaddr *) &sa, sizeof(sa)) == 0) {
                success = 1;
#ifdef DEBUG
//                bftpd_log("Passive mode: Successfully bound port %d\n", port);
#endif
                break;
            }
        }
        if (!success) {
            control_printf(SL_FAILURE, "425 Error: Unable to bind data socket.");
            return;
        }
        prepare_sock(pasvsock);
    }       

	if (listen(pasvsock, 1)) {
		control_printf(SL_FAILURE, "425-Error: Unable to make socket listen.\r\n425 %s",
				 strerror(errno));
		return;
	}
	namelen = sizeof(localsock);
	getsockname(pasvsock, (struct sockaddr *) &localsock, (int *) &namelen);
	sscanf((char *) inet_ntoa(name.sin_addr), "%i.%i.%i.%i",
		   &a1, &a2, &a3, &a4);
	control_printf(SL_SUCCESS, "227 Entering Passive Mode (%i,%i,%i,%i,%i,%i)", a1, a2, a3, a4,
			 ntohs(localsock.sin_port) >> 8, ntohs(localsock.sin_port) & 0xFF);
	pasv = 1;
}

#endif /* SUPPORT_FTPD_STORAGE */


#if 0
void command_eprt(char *params) {
    char delim;
    int af;
    char addr[51];
    char foo[20];
    int port;
    if (epsvall) {
        control_printf(SL_FAILURE, "500 EPSV ALL has been called.");
        return;
    }
    if (strlen(params) < 5) {
        control_printf(SL_FAILURE, "500 Syntax error.");
        return;
    }
    delim = params[0];
    sprintf(foo, "%c%%i%c%%50[^%c]%c%%i%c", delim, delim, delim, delim, delim);
    if (sscanf(params, foo, &af, addr, &port) < 3) {
        control_printf(SL_FAILURE, "500 Syntax error.");
        return;
    }
    if (af != 1) {
        control_printf(SL_FAILURE, "522 Protocol unsupported, use (1)");
        return;
    }
    sa.sin_addr.s_addr = inet_addr(addr);
    if ((sa.sin_addr.s_addr != remotename.sin_addr.s_addr) && (strncasecmp(config_getoption("ALLOW_FXP"), "yes", 3))) {
        control_printf(SL_FAILURE, "500 The given address is not yours.");
        return;
    }
    sa.sin_port = htons(port);
    if (pasv) {
        close(sock);
        pasv = 0;
    }
    control_printf(SL_FAILURE, "200 EPRT %s:%i OK", addr, port);
}


void command_epsv(char *params)
{
    struct sockaddr_in localsock;
    int namelen;
    int af;
    if (params[0]) {
        if (!strncasecmp(params, "ALL", 3))
            epsvall = 1;
        else {
            if (sscanf(params, "%i", &af) < 1) {
                control_printf(SL_FAILURE, "500 Syntax error.");
                return;
            } else {
                if (af != 1) {
                    control_printf(SL_FAILURE, "522 Protocol unsupported, use (1)");
                    return;
                }
            }
        }
    }
    pasvsock = socket(AF_INET, SOCK_STREAM, 0);
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port = 0;
    sa.sin_family = AF_INET;
    if (bind(pasvsock, (struct sockaddr *) &sa, sizeof(sa)) == -1) {
		control_printf(SL_FAILURE, "500-Error: Unable to bind data socket.\r\n425 %s",
				 strerror(errno));
		return;
	}
	if (listen(pasvsock, 1)) {
		control_printf(SL_FAILURE, "500-Error: Unable to make socket listen.\r\n425 %s",
				 strerror(errno));
		return;
	}
	namelen = sizeof(localsock);
	getsockname(pasvsock, (struct sockaddr *) &localsock, (int *) &namelen);
    control_printf(SL_SUCCESS, "229 Entering extended passive mode (|||%i|)",
             ntohs(localsock.sin_port));
    pasv = 1;
}

void command_allo(char *foo)
{
    command_noop(foo);
}
#endif // brcm commands not used


#ifdef SUPPORT_FTPD_STORAGE
char test_abort(char selectbefore, int file, int sock)
{
    char str[256];
    fd_set rfds;
    struct timeval tv;
    if (selectbefore) {
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        FD_ZERO(&rfds);
        FD_SET(fileno(stdin), &rfds);
        if (!select(fileno(stdin) + 1, &rfds, NULL, NULL, &tv))
            return 0;
    }
	fgets(str, sizeof(str), stdin);
    if (strstr(str, "ABOR")) {
        control_printf(SL_SUCCESS, "426 Transfer aborted.");
    	close(file);
		close(sock);
   		control_printf(SL_SUCCESS, "226 Aborted.");
         //		bftpd_log("Client aborted file transmission.\n");
      printf("Client aborted file transmission. \n");
        alarm(control_timeout);
        return 1;
	}
    return 0;
}
#endif


void displayMessage(UPLOAD_RESULT result)
{

    switch (result)
    {
        case UPLOAD_OK:
 	        control_printf(SL_SUCCESS, "Ftp image done. PLEASE TYPE 'bye' or 'quit' NOW to quit ftp and the Router will start writing the image to flash.");
            break;
        case UPLOAD_FAIL_NO_MEM:
            control_printf(SL_FAILURE, "Not enough memory error.");        
            break;
        case UPLOAD_FAIL_ILLEGAL_IMAGE:
            control_printf(SL_FAILURE, "Image updating failed. The selected file contains an illegal image.  PLEASE TYPE 'bye' or 'quit' NOW to quit ftp");
            break;
        case UPLOAD_FAIL_FLASH:
        case UPLOAD_FAIL_FTP:
            control_printf(SL_FAILURE, "ftp connection failed.");
            break;
    }
}


#ifndef SUPPORT_FTPD_STORAGE

/*
 * We only do image updates when FTPD storage feature is not defined.
 */

void do_fwUpdate(void)
{
    int byteRd = 0;
    char *curPtr = NULL;
    unsigned int totalAllocatedSize = 0;
    char *buffer;
    int max;
    fd_set rfds;
    struct timeval tv;
    UBOOL8 isConfigFile;

   /* reset all of our globals before starting another download */    
    imageFormat = CMS_IMAGE_FORMAT_INVALID;
    if (imagePtr)
       free(imagePtr);
    imagePtr = NULL;
    uploadSize = 0;

	if (dataconn())
	    return;
    alarm(0);

    if ((buffer = malloc(xfer_bufsize)) == NULL)
    {
        displayMessage(UPLOAD_FAIL_NO_MEM);
        return;
    }

    max = (sock > fileno(stdin) ? sock : fileno(stdin)) + 1;
    for (;;) {
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        FD_SET(fileno(stdin), &rfds);
        
        tv.tv_sec = data_timeout;
        tv.tv_usec = 0;
        if (!select(max, &rfds, NULL, NULL, &tv)) {
            close(sock);
            control_printf(SL_FAILURE, "426 Kicked due to data transmission timeout.");
            if (imagePtr)
                free(imagePtr);
            free(buffer);
            displayMessage(UPLOAD_FAIL_FTP);
            return;     // exit ?
        }

		  if (!((byteRd = recv(sock, buffer, xfer_bufsize, 0))))
            break;

        if (curPtr == NULL)
        {
            // Also look in tftpd.c, which does about the same thing

            isConfigFile = cmsImg_isConfigFileLikely(buffer);
            cmsLog_debug("isConfigFile = %d", isConfigFile);
            
            if (isConfigFile)
            {
               totalAllocatedSize = cmsImg_getConfigFlashSize();
            }
            else
            {
               totalAllocatedSize = cmsImg_getImageFlashSize() + cmsImg_getBroadcomImageTagSize();
               // let smd know that we are about to start a big download
               cmsImg_sendLoadStartingMsg(msgHandle, connIfName);
            }

            if ((curPtr = (char *) malloc(totalAllocatedSize)) == NULL)
            {
               cmsLog_error("Not enough memory (%d bytes needed)", totalAllocatedSize);
               free(buffer);
               cmsImg_sendLoadDoneMsg(msgHandle);
               return;
            }
            printf("%d bytes allocated for image\n", totalAllocatedSize);
            imagePtr = curPtr;   
        }

        if (uploadSize + byteRd < totalAllocatedSize)
        {
            memcpy(curPtr, buffer, byteRd);     
            curPtr += byteRd;
            uploadSize += byteRd;
        }
        else
        {
            printf("Image could not fit into %d byte buffer.\n", totalAllocatedSize);
            free(buffer);
            free(imagePtr);
            imagePtr = NULL;
            cmsImg_sendLoadDoneMsg(msgHandle);
            return;
        }
        
	}  // end for loop to read in complete image


   free(buffer);

   
   /*
    * Now we have the entire image.  Validate it.
    */
   if ((imageFormat = cmsImg_validateImage(imagePtr, uploadSize, msgHandle)) == CMS_IMAGE_FORMAT_INVALID)
   {
      displayMessage(UPLOAD_FAIL_ILLEGAL_IMAGE);
      free(imagePtr);
      imagePtr = NULL;
      cmsImg_sendLoadDoneMsg(msgHandle);
   }
   else
   {
      printf("Image validated, size=%u format=%d, waiting for quit before flashing.\n", uploadSize, imageFormat);
      displayMessage(UPLOAD_OK);  // flash image will be done when user types bye or OK
   }

   close(sock);   // this tells the ftp client that the transfer is complete
   alarm(control_timeout);

}



void command_fwupdate(char *param  __attribute__((unused)))
{
    do_fwUpdate();
}

#endif /* not SUPPORT_FTPD_STORAGE */


void command_syst(char *params __attribute__((unused)))
{
	control_printf(SL_SUCCESS, "215 UNIX Type: L8");
}

void command_quit(char *params __attribute__((unused)))
{
   CmsRet ret;
   
   if (imagePtr == NULL)
   {
      /*
       * Either no image download was attempted, or download failed, or
       * got a bad image.
       */
       
      control_printf(SL_SUCCESS, "221 %s", "See you later...");
      exit(0);
   }

        
   /*
    * This is when the user has successfully download an image,
    * and now the user quits the control connection, so we go and
    * write the image.
    */
   control_printf(SL_SUCCESS, "221 %s", "The Router is rebooting...");
   printf("Flashing image now....\n");
   ret = cmsImg_writeValidatedImage(imagePtr, uploadSize, imageFormat, msgHandle);
   if (ret != CMSRET_SUCCESS)
   {
      /* uh, oh, something still went wrong.  Tell the user */
      control_printf(SL_FAILURE, "Image updating failed.");
      cmsImg_sendLoadDoneMsg(msgHandle);
      exit(1);
   }
   else
   {
      /* on the modem, a successful flash of the image or the 
       * config file will result in reboot.  On desktop linux, we
       * simulate this effect by exiting.
       */
      exit(0);
    }
}



#ifdef SUPPORT_FTPD_STORAGE

void do_stor(char* filename, int flags)
{
    int fd, i, max;
    char *buffer;
    fd_set rfds;
    char *mapped = bftpd_cwd_mappath(filename);
    int xfer_delay;
    struct timeval tv;
    char *p, *pp;

    // See if we should delay between data transfers
    xfer_delay = strtoul( config_getoption("XFER_DELAY"), NULL, 0);

    // Check the file exists and over-writable
    fd = open(mapped, O_RDONLY);
    if (fd >= 0) 	// file exists
    {
	// close file
	close(fd);
	// check over-writable
	if ( !strcasecmp( config_getoption("ALLOWCOMMAND_DELE"), "no") )
	{
	    control_printf(SL_FAILURE, "553 Error: Remote file is write protected.");
	    if (mapped)
		free(mapped);
	    close(sock);
	    return;
	}
    }
    fd = open(mapped, flags, 00666);
    if (mapped)
	free(mapped);
    if(fd == -1) 
    {
	control_printf(SL_FAILURE, "553 Error: %s.", strerror(errno));
	return;
    }
    printf("Client is storing file '%s'.\n", filename);
    if (dataconn())
	return;

    alarm(0);
    buffer = malloc(xfer_bufsize);
    // check out of memory
    if ( ! buffer)
    {
	printf("Unable to create buffer to receive file.\n");
	control_printf(SL_FAILURE, "553 Error: An unknown error occured on the server.");
	if (fd >= 0)
	    close(fd);
	close(sock);
	return;
    }

    lseek(fd, offset, SEEK_SET);
    offset = 0;
    // Do not use the whole buffer, because a null byte is needed in ASCII mode
    max = (sock > fileno(stdin) ? sock : fileno(stdin)) + 1;
    for (;;)
    {
	FD_ZERO(&rfds);
	FD_SET(sock, &rfds);
	FD_SET(fileno(stdin), &rfds);
	
	tv.tv_sec = data_timeout;
	tv.tv_usec = 0;
	if ( !select(max, &rfds, NULL, NULL, &tv))
	{
	    close(sock);
	    close(fd);
	    control_printf(SL_FAILURE, "426 Kicked dut to data transmission timeout.");
	    printf("Kicked due to data transmission timeout.\n");
	    exit(0);
	}
	if (FD_ISSET(fileno(stdin), &rfds))
	{
	    test_abort(0, fd, sock);
	    if (buffer)
		free(buffer);
	    return;
	}
	if (!(i = recv(sock, buffer, xfer_bufsize-1, 0)))
	    break;
	bytes_recvd += i;
	if (xfertype == TYPE_ASCII)
	{
	    buffer[i] = '\0';
	    // on ASCII transfer, strip character 13
	    p = pp = buffer;
	    while(*p) {
		if ( (unsigned char) *p == 13)
		    p++;
		else
		    *pp++ = *p++;
	    }
	    *pp++ = 0;
	    i = strlen(buffer);
	}	// end of ASCII type transfer
	
	// write data into fd
	write(fd, buffer, i);

	if ( xfer_delay )
	{
	    struct timeval wait_time;
	    wait_time.tv_sec = 0;
	    wait_time.tv_usec = xfer_delay; 
	    select( 0, NULL, NULL, NULL, &wait_time);
	}
    }	// end of for
    free(buffer);
    close(fd);
    close(sock);
    alarm(control_timeout);
    offset = 0;
    control_printf(SL_SUCCESS, "226 File transmission successful.");
    printf("File transmission successful.\n");
}


void command_stor(char *filename)
{
    do_stor(filename, O_CREAT | O_WRONLY | O_TRUNC);
}

#endif  /* SUPPORT_FTPD_STORAGE */

// brcm command not used
#if 0
void command_appe(char *filename)
{
    // brcm do_stor(filename, O_CREAT | O_WRONLY | O_APPEND);
}
#endif


#ifdef SUPPORT_FTPD_STORAGE
void command_retr(char *filename)
{
	char *mapped;
	char *buffer;
#ifdef WANT_GZIP
    gzFile gzfile;
#endif
	int phile;
	int i, whattodo = DO_NORMAL;
	struct stat statbuf;
#ifdef HAVE_SYS_SENDFILE_H
    off_t sendfile_offset;
#endif
#if (defined(WANT_TAR) && defined(WANT_GZIP))
    int filedes[2];
#endif
#if (defined(WANT_TAR) || defined(WANT_GZIP))
    char *foo;
#endif
#ifdef WANT_TAR
    char *argv[4];
#endif
	mapped = bftpd_cwd_mappath(filename);
	phile = open(mapped, O_RDONLY);
	if (phile == -1) {
#if (defined(WANT_TAR) && defined(WANT_GZIP))
		if ((foo = strstr(filename, ".tar.gz")))
			if (strlen(foo) == 7) {
				whattodo = DO_TARGZ;
				*foo = '\0';
			}
#endif
#ifdef WANT_TAR
		if ((foo = strstr(filename, ".tar")))
			if (strlen(foo) == 4) {
				whattodo = DO_TARONLY;
				*foo = '\0';
			}
#endif
#ifdef WANT_GZIP
		if ((foo = strstr(filename, ".gz")))
			if (strlen(foo) == 3) {
				whattodo = DO_GZONLY;
				*foo = '\0';
			}
#endif
		if (whattodo == DO_NORMAL) {
			//bftpd_log("Error: '%s' while trying to receive file '%s'.\n",
			//		  strerror(errno), filename);
			printf("Error: '%s' while trying to receive file '%s'.\n", strerror(errno), filename);
			control_printf(SL_FAILURE, "553 Error: %s.", strerror(errno));
			if (mapped)
				free(mapped);
			return;
		}
	} /* No else, the file remains open so that it needn't be opened again */
	stat(mapped, (struct stat *) &statbuf);
	if (S_ISDIR(statbuf.st_mode)) {
		control_printf(SL_FAILURE, "550 Error: Is a directory.");
		if (mapped)
			free(mapped);
		return;
	}
   /*
	if ((((statbuf.st_size - offset) * ratio_send) / ratio_recv > bytes_recvd
		 - bytes_sent) && (strcmp((char *) config_getoption("RATIO"), "none"))) {
		// bftpd_log("Error: 'File too big (ratio)' while trying to receive file "
		//		  "'%s'.\n", filename);
		//cbj
		printf("Error: 'File too big (ratio)' while trying to receive file %s.\n", filename);
		control_printf(SL_FAILURE, "553 File too big. Send at least %i bytes first.",
				(int) (((statbuf.st_size - offset) * ratio_send) / ratio_recv)
				- bytes_recvd);
		if (mapped)
			free(mapped);
		return;
      } */
	// bftpd_log("Client is receiving file '%s'.\n", filename);
	printf("Client is receiving file '%s'.\n", filename);
	switch (whattodo) {
#if (defined(WANT_TAR) && defined(WANT_GZIP))
        case DO_TARGZ:
            close(phile);
            if (dataconn()) {
				if (mapped)
					free(mapped);
                return;
			}
            alarm(0);
            pipe(filedes);
            if (fork()) {
                buffer = malloc(xfer_bufsize);
                close(filedes[1]);
                gzfile = gzdopen(sock, "wb");
                while ((i = read(filedes[0], buffer, xfer_bufsize))) {
                    gzwrite(gzfile, buffer, i);
                    test_abort(1, phile, sock);
                }
                free(buffer);
                gzclose(gzfile);
                wait(NULL); /* Kill the zombie */
            } else {
                stderr = devnull;
                close(filedes[0]);
                close(fileno(stdout));
                dup2(filedes[1], fileno(stdout));
                setvbuf(stdout, NULL, _IONBF, 0);
                argv[0] = "tar";
                argv[1] = "cf";
                argv[2] = "-";
                argv[3] = mapped;
                exit(pax_main(4, argv));
            }
            break;
#endif
#ifdef WANT_TAR
		case DO_TARONLY:
            if (dataconn()) {
				if (mapped)
					free(mapped);
                return;
			}
            alarm(0);
            if (fork())
                wait(NULL);
            else {
                stderr = devnull;
                dup2(sock, fileno(stdout));
                argv[0] = "tar";
                argv[1] = "cf";
                argv[2] = "-";
                argv[3] = mapped;
                exit(pax_main(4, argv));
            }
            break;
#endif
#ifdef WANT_GZIP
		case DO_GZONLY:
			if (mapped)
				free(mapped);
            if ((phile = open(mapped, O_RDONLY)) < 0) {
				control_printf(SL_FAILURE, "553 Error: %s.", strerror(errno));
				return;
			}
			if (dataconn()) {
				if (mapped)
					free(mapped);
				return;
			}
            alarm(0);
            buffer = malloc(xfer_bufsize);
            /* Use "wb9" for maximum compression, uses more CPU time... */
            gzfile = gzdopen(sock, "wb");
            while ((i = read(phile, buffer, xfer_bufsize))) {
                gzwrite(gzfile, buffer, i);
                test_abort(1, phile, sock);
            }
            free(buffer);
            close(phile);
            gzclose(gzfile);
            break;
#endif
		case DO_NORMAL:
			if (mapped)
				free(mapped);
			if (dataconn())
				return;
            alarm(0);
#ifdef HAVE_SYS_SENDFILE_H
            sendfile_offset = offset;
            if (xfertype != TYPE_ASCII) {
                alarm_type = phile;
                while (sendfile(sock, phile, &sendfile_offset, xfer_bufsize)) {
                    alarm(data_timeout);
                    if (test_abort(1, phile, sock))
                        return;
                }
                alarm(control_timeout);
                alarm_type = 0;
            } else {
#endif
			lseek(phile, offset, SEEK_SET);
			offset = 0;
			buffer = malloc(xfer_bufsize * 2 + 1);
			while ((i = read(phile, buffer, xfer_bufsize))) {
				if (test_abort(1, phile, sock)) {
					free(buffer);
					return;
				}

#ifndef HAVE_SYS_SENDFILE_H
                if (xfertype == TYPE_ASCII) {
#endif
                    buffer[i] = '\0';
                    i += replace(buffer, "\n", "\r\n");
#ifndef HAVE_SYS_SENDFILE_H
                }
#endif
				send(sock, buffer, i, 0);
				bytes_sent += i;
			}
            free(buffer);
#ifdef HAVE_SYS_SENDFILE_H
            }
#endif
			close(phile);
	}
	close(sock);
    offset = 0;
    alarm(control_timeout);
	control_printf(SL_SUCCESS, "226 File transmission successful.");
   //	bftpd_log("File transmission successful.\n");
	printf("File transmission successful.\n");
}

void do_dirlist(char *dirname, char verbose)
{
	FILE *datastream;
	if (dirname[0] != '\0') {
		/* skip arguments */
		if (dirname[0] == '-') {
			while ((dirname[0] != ' ') && (dirname[0] != '\0'))
				dirname++;
			if (dirname[0] != '\0')
				dirname++;
		}
	}
	if (dataconn())
		return;
    alarm(0);
	datastream = fdopen(sock, "w");
	if (dirname[0] == '\0')
		dirlist("*", datastream, verbose);
	else {
		char *mapped = bftpd_cwd_mappath(dirname);
		dirlist(mapped, datastream, verbose);
		free(mapped);
	}
	fclose(datastream);
    alarm(control_timeout);
	control_printf(SL_SUCCESS, "226 Directory list has been submitted.");
}

void command_list(char *dirname)
{
	do_dirlist(dirname, 1);
}

void command_nlst(char *dirname)
{
	do_dirlist(dirname, 0);
}

#endif /* SUPPORT_FTPD_STORAGE */

// brcm not used commands
#if 0
void command_syst(char *params)
{
	control_printf(SL_SUCCESS, "215 UNIX Type: L8");
}

void command_mdtm(char *filename)
{
	struct stat statbuf;
	struct tm *filetime;
	char *fullfilename = bftpd_cwd_mappath(filename);
	if (!stat(fullfilename, (struct stat *) &statbuf)) {
		filetime = gmtime((time_t *) & statbuf.st_mtime);
		control_printf(SL_SUCCESS, "213 %04i%02i%02i%02i%02i%02i",
				filetime->tm_year + 1900, filetime->tm_mon + 1,
				filetime->tm_mday, filetime->tm_hour, filetime->tm_min,
				filetime->tm_sec);
	} else {
		control_printf(SL_FAILURE, "550 Error while determining the modification time: %s",
				strerror(errno));
	}
	free(fullfilename);
}
#endif


#ifdef SUPPORT_FTPD_STORAGE

void command_cwd(char *dir)
{
    if (bftpd_cwd_chdir(dir)) {
       // bftpd_log("Error: '%s' while changing directory to '%s'.\n",
       //				  strerror(errno), dir);
		control_printf(SL_FAILURE, "451 Error: %s.", strerror(errno));
	} else {
       //bftpd_log("Changed directory to '%s'.\n", dir);
		control_printf(SL_SUCCESS, "250 OK");
	}
}

void command_cdup(char *params)
{
   //	bftpd_log("Changed directory to '..'.\n");
	bftpd_cwd_chdir("..");
	control_printf(SL_SUCCESS, "250 OK");
}

void command_dele(char *filename)
{
	char *mapped = bftpd_cwd_mappath(filename);
	if (unlink(mapped)) {
		//bftpd_log("Error: '%s' while trying to delete file '%s'.\n",
		//		  strerror(errno), filename);
		control_printf(SL_FAILURE, "451 Error: %s.", strerror(errno));
	} else {
		//bftpd_log("Deleted file '%s'.\n", filename);
		control_printf(SL_SUCCESS, "200 OK");
	}
	free(mapped);
}

void command_noop(char *params)
{
        control_printf(SL_SUCCESS, "200 OK");
}


void command_mkd(char *dirname)
{
	char *mapped = bftpd_cwd_mappath(dirname);
	if (mkdir(mapped, 0755)) {
      //bftpd_log("Error: '%s' while trying to create directory '%s'.\n",
		//		  strerror(errno), dirname);
		control_printf(SL_FAILURE, "451 Error: %s.", strerror(errno));
	} else {
		//bftpd_log("Created directory '%s'.\n", dirname);
		control_printf(SL_SUCCESS, "257 \"%s\" has been created.", dirname);
	}
	free(mapped);
}

void command_rmd(char *dirname)
{
	char *mapped = bftpd_cwd_mappath(dirname);
	if (rmdir(mapped)) {
		//bftpd_log("Error: '%s' while trying to remove directory '%s'.\n",
		//		  strerror(errno), dirname);
		control_printf(SL_FAILURE, "451 Error: %s.", strerror(errno));
	} else {
		//bftpd_log("Removed directory '%s'.\n", dirname);
		control_printf(SL_SUCCESS, "250 OK");
	}
	free(mapped);
}

#endif /* SUPPORT_FTPD_STORAGE */


// brcm not needed
#if 0

void command_rnfr(char *oldname)
{
	FILE *file;
	char *mapped = bftpd_cwd_mappath(oldname);
	if ((file = fopen(mapped, "r"))) {
		fclose(file);
		if (philename)
			free(philename);
		philename = mapped;
		state = STATE_RENAME;
		control_printf(SL_SUCCESS, "350 File exists, ready for destination name");
	} else {
		free(mapped);
		control_printf(SL_FAILURE, "451 Error: %s.", strerror(errno));
	}
}

void command_rnto(char *newname)
{
	char *mapped = bftpd_cwd_mappath(newname);
	if (rename(philename, mapped)) {
		bftpd_log("Error: '%s' while trying to rename '%s' to '%s'.\n",
				  strerror(errno), philename, bftpd_cwd_mappath(newname));
		control_printf(SL_FAILURE, "451 Error: %s.", strerror(errno));
	} else {
		bftpd_log("Successfully renamed '%s' to '%s'.\n", philename, bftpd_cwd_mappath(newname));
		control_printf(SL_SUCCESS, "250 OK");
		state = STATE_AUTHENTICATED;
	}
	free(philename);
	free(mapped);
	philename = NULL;
}

void command_rest(char *params)
{
    offset = strtoul(params, NULL, 10);
	control_printf(SL_SUCCESS, "350 Restarting at offset %i.", offset);
}

void command_size(char *filename)
{
	struct stat statbuf;
	char *mapped = bftpd_cwd_mappath(filename);
	if (!stat(mapped, &statbuf)) {
		control_printf(SL_SUCCESS, "213 %i", (int) statbuf.st_size);
	} else {
		control_printf(SL_FAILURE, "550 Error: %s.", strerror(errno));
	}
	free(mapped);
}

void command_quit(char *params)
{
	control_printf(SL_SUCCESS, "221 %s", config_getoption("QUIT_MSG"));
	exit(0);
}

void command_stat(char *filename)
{
	char *mapped = bftpd_cwd_mappath(filename);
    control_printf(SL_SUCCESS, "213-Status of %s:", filename);
    bftpd_stat(mapped, stderr);
    control_printf(SL_SUCCESS, "213 End of Status.");
	free(mapped);
}

/* SITE commands */

void command_chmod(char *params)
{
	int permissions;
	char *mapped;
	if (!strchr(params, ' ')) {
		control_printf(SL_FAILURE, "550 Usage: SITE CHMOD <permissions> <filename>");
		return;
	}
	mapped = bftpd_cwd_mappath(strdup(strchr(params, ' ') + 1));
	*strchr(params, ' ') = '\0';
	sscanf(params, "%o", &permissions);
	if (chmod(mapped, permissions))
		control_printf(SL_FAILURE, "Error: %s.", strerror(errno));
	else {
		bftpd_log("Changed permissions of '%s' to '%o'.\n", mapped,
				  permissions);
		control_printf(SL_SUCCESS, "200 CHMOD successful.");
	}
	free(mapped);
}

void command_chown(char *params)
{
	char foo[MAXCMD + 1], owner[MAXCMD + 1], group[MAXCMD + 1],
		filename[MAXCMD + 1], *mapped;
	int uid, gid;
	if (!strchr(params, ' ')) {
		control_printf(SL_FAILURE, "550 Usage: SITE CHOWN <owner>[.<group>] <filename>");
		return;
	}
	sscanf(params, "%[^ ] %s", foo, filename);
	if (strchr(foo, '.'))
		sscanf(foo, "%[^.].%s", owner, group);
	else {
		strcpy(owner, foo);
		group[0] = '\0';
	}
	if (!sscanf(owner, "%i", &uid))	/* Is it a number? */
		if (((uid = mygetpwnam(owner, passwdfile))) < 0) {
			control_printf(SL_FAILURE, "550 User '%s' not found.", owner);
			return;
		}
	if (!sscanf(group, "%i", &gid))
		if (((gid = mygetpwnam(group, groupfile))) < 0) {
			control_printf(SL_FAILURE, "550 Group '%s' not found.", group);
			return;
		}
	mapped = bftpd_cwd_mappath(filename);
	if (chown(mapped, uid, gid))
		control_printf(SL_FAILURE, "550 Error: %s.", strerror(errno));
	else {
		bftpd_log("Changed owner of '%s' to UID %i GID %i.\n", filename, uid,
				  gid);
		control_printf(SL_SUCCESS, "200 CHOWN successful.");
	}
	free(mapped);
}

void command_site(char *str)
{
	const struct command subcmds[] = {
		{"chmod ", NULL, command_chmod, STATE_AUTHENTICATED},
		{"chown ", NULL, command_chown, STATE_AUTHENTICATED},
		{NULL, NULL, 0},
	};
	int i;
	if (!strcasecmp(config_getoption("ENABLE_SITE"), "no")) {
		control_printf(SL_FAILURE, "550 SITE commands are disabled.");
		return;
	}
	for (i = 0; subcmds[i].name; i++) {
		if (!strncasecmp(str, subcmds[i].name, strlen(subcmds[i].name))) {
			cutto(str, strlen(subcmds[i].name));
			subcmds[i].function(str);
			return;
		}
	}
	control_printf(SL_FAILURE, "550 Unknown command: 'SITE %s'.", str);
}

void command_auth(char *type)
{
    control_printf(SL_FAILURE, "550 Not implemented yet\r\n");
}

#endif //brcm command not used

/* Command parsing */
#if 0 //brcm orig commands - can be put back if needed
const struct command commands[] = {
	{"USER", "<sp> username", command_user, STATE_CONNECTED, 0},
	{"PASS", "<sp> password", command_pass, STATE_USER, 0},
    {"XPWD", "(returns cwd)", command_pwd, STATE_AUTHENTICATED, 1},
	{"PWD", "(returns cwd)", command_pwd, STATE_AUTHENTICATED, 0},
	{"TYPE", "<sp> type-code (A or I)", command_type, STATE_AUTHENTICATED, 0},
	{"PORT", "<sp> h1,h2,h3,h4,p1,p2", command_port, STATE_AUTHENTICATED, 0},
    {"EPRT", "<sp><d><net-prt><d><ip><d><tcp-prt><d>", command_eprt, STATE_AUTHENTICATED, 1},
	{"PASV", "(returns address/port)", command_pasv, STATE_AUTHENTICATED, 0},
    {"EPSV", "(returns address/post)", command_epsv, STATE_AUTHENTICATED, 1},
    {"ALLO", "<sp> size", command_allo, STATE_AUTHENTICATED, 1},
	{"STOR", "<sp> pathname", command_stor, STATE_AUTHENTICATED, 0},
    {"APPE", "<sp> pathname", command_appe, STATE_AUTHENTICATED, 1},
	{"RETR", "<sp> pathname", command_retr, STATE_AUTHENTICATED, 0},
	{"LIST", "[<sp> pathname]", command_list, STATE_AUTHENTICATED, 0},
	{"NLST", "[<sp> pathname]", command_nlst, STATE_AUTHENTICATED, 0},
	{"SYST", "(returns system type)", command_syst, STATE_CONNECTED, 0},
	{"MDTM", "<sp> pathname", command_mdtm, STATE_AUTHENTICATED, 1},
    {"XCWD", "<sp> pathname", command_cwd, STATE_AUTHENTICATED, 1},
	{"CWD", "<sp> pathname", command_cwd, STATE_AUTHENTICATED, 0},
    {"XCUP", "(up one directory)", command_cdup, STATE_AUTHENTICATED, 1},
	{"CDUP", "(up one directory)", command_cdup, STATE_AUTHENTICATED, 0},
	{"DELE", "<sp> pathname", command_dele, STATE_AUTHENTICATED, 0},
    {"XMKD", "<sp> pathname", command_mkd, STATE_AUTHENTICATED, 1},
	{"MKD", "<sp> pathname", command_mkd, STATE_AUTHENTICATED, 0},
    {"XRMD", "<sp> pathname", command_rmd, STATE_AUTHENTICATED, 1},
	{"RMD", "<sp> pathname", command_rmd, STATE_AUTHENTICATED, 0},
	{"NOOP", "(no operation)", command_noop, STATE_AUTHENTICATED, 0},
	{"RNFR", "<sp> pathname", command_rnfr, STATE_AUTHENTICATED, 0},
	{"RNTO", "<sp> pathname", command_rnto, STATE_RENAME, 0},
	{"REST", "<sp> byte-count", command_rest, STATE_AUTHENTICATED, 1},
	{"SIZE", "<sp> pathname", command_size, STATE_AUTHENTICATED, 1},
	{"QUIT", "(close control connection)", command_quit, STATE_CONNECTED, 0},
	{"HELP", "[<sp> command]", command_help, STATE_AUTHENTICATED, 0},
    {"STAT", "<sp> pathname", command_stat, STATE_AUTHENTICATED, 0},
	{"SITE", "<sp> string", command_site, STATE_AUTHENTICATED, 0},
    {"FEAT", "(returns list of extensions)", command_feat, STATE_AUTHENTICATED, 0},
/*    {"AUTH", "<sp> authtype", command_auth, STATE_CONNECTED, 0},
*/    {"ADMIN_LOGIN", "(admin)", command_adminlogin, STATE_CONNECTED, 0},
	{NULL, NULL, NULL, 0, 0}
};
#endif //brcm


#ifdef SUPPORT_FTPD_STORAGE
/*
 * Commands used when we are in ftpd storage mode.
 * I don't know why our Taiwan intern "cbj" removed the
 * syntax part of the command.  Just porting his code over.  --mwang
 */
const struct command commands[] = {
	{"USER",/* "<sp> username",*/ command_user, STATE_CONNECTED, 0},
	{"PASS",/* "<sp> password",*/ command_pass, STATE_USER, 0},
	{"PORT",/* "<sp> h1,h2,h3,h4,p1,p2",*/ command_port, STATE_AUTHENTICATED, 0},
	{"RETR",/* "<sp> pathname",*/ command_retr, STATE_AUTHENTICATED, 0},
	{"MKD",/* "<sp> pathname",*/ command_mkd, STATE_AUTHENTICATED, 0},
	{"RMD",/* "<sp> pathname",*/ command_rmd, STATE_AUTHENTICATED, 0},
	{"PWD",/* "(return cwd)",*/ command_pwd, STATE_AUTHENTICATED, 0},
	{"CWD",/* "<sp> pathname",*/ command_cwd, STATE_AUTHENTICATED, 0},
	{"CDUP",/* "(up one directory)",*/ command_cdup, STATE_AUTHENTICATED, 0},
	{"DELE",/* "<sp> pathname",*/ command_dele, STATE_AUTHENTICATED, 0},
	{"LIST",/* "[<sp> pathname]",*/ command_list, STATE_AUTHENTICATED, 0},
	{"NLST",/* "[<sp> pathname]",*/ command_nlst, STATE_AUTHENTICATED, 0},
	{"NOOP",/* "(no operation)",*/ command_noop, STATE_AUTHENTICATED, 0},
	{"PASV",/* "(returns address/port)",*/ command_pasv, STATE_AUTHENTICATED, 0},
	{"TYPE",/* "<sp> type-code (A or I)",*/ command_type, STATE_AUTHENTICATED, 0},
	{"SYST",/* "(returns system type)",*/ command_syst, STATE_CONNECTED, 0},
	{"STOR",/* "<sp> pathname",*/ command_stor, STATE_AUTHENTICATED, 0},
	{"QUIT",/* "(close control connection)",*/ command_quit, STATE_CONNECTED, 0},
	{NULL, NULL, 0, 0}
};
#else
// brcm - commands in use for firmware update only
const struct command commands[] = {
	{"USER", "<sp> username", command_user, STATE_CONNECTED, 0},
	{"PASS", "<sp> password", command_pass, STATE_USER, 0},
	{"PORT", "<sp> h1,h2,h3,h4,p1,p2", command_port, STATE_AUTHENTICATED, 0},
	{"TYPE", "<sp> type-code (A or I)", command_type, STATE_AUTHENTICATED, 0},
	{"SYST", "(returns system type)", command_syst, STATE_CONNECTED, 0},
	{"STOR", "<sp> pathname", command_fwupdate, STATE_AUTHENTICATED, 0},
	{"QUIT", "(close control connection)", command_quit, STATE_CONNECTED, 0},
	{NULL, NULL, NULL, 0, 0}
};
#endif /* SUPPORT_FTPD_STORAGE */

#if 0 //brcm
void command_feat(char *params)
{
    int i;
    control_printf(SL_SUCCESS, "211-Extensions supported:");
    for (i = 0; commands[i].name; i++)
        if (commands[i].showinfeat)
            control_printf(SL_SUCCESS, " %s", commands[i].name);
    control_printf(SL_SUCCESS, "211 End");
}

void command_help(char *params)
{
	int i;
	if (params[0] == '\0') {
		control_printf(SL_SUCCESS, "214-The following commands are recognized.");
		for (i = 0; commands[i].name; i++)
			control_printf(SL_SUCCESS, "214-%s", commands[i].name);
        control_printf(SL_SUCCESS, "214 End of help");
	} else {
		for (i = 0; commands[i].name; i++)
			if (!strcasecmp(params, commands[i].name))
				control_printf(SL_SUCCESS, "214 Syntax: %s", commands[i].syntax);
	}
}
#endif //brcm, not in use

int parsecmd(char *str)
{
	int i;
	char *p, *pp, confstr[18]; /* strlen("ALLOWCOMMAND_XXXX") + 1 == 18 */
	p = pp = str;			/* Remove garbage in the string */
	while (*p)
		if ((unsigned char) *p < 32)
			p++;
		else
			*pp++ = *p++;
	*pp++ = 0;
	for (i = 0; commands[i].name; i++) {	/* Parse command */

// printf("commands[i].name=%s\n", commands[i].name); // brcm

		if (!strncasecmp(str, commands[i].name, strlen(commands[i].name))) {
            sprintf(confstr, "ALLOWCOMMAND_%s", commands[i].name);
#if 0 //brcm
            if (!strcasecmp(config_getoption(confstr), "no")) {
                control_printf(SL_FAILURE, "550 The command '%s' is disabled.",
                        commands[i].name);
                return 1;
            }
#endif //brcm
			cutto(str, strlen(commands[i].name));
			p = str;
			while ((*p) && ((*p == ' ') || (*p == '\t')))
				p++;
			memmove(str, p, strlen(str) - (p - str) + 1);
			if (state >= commands[i].state_needed) {
				commands[i].function(str);
				return 0;
			} else {
				switch (state) {
					case STATE_CONNECTED: {
						control_printf(SL_FAILURE, "503 USER expected.");
						return 1;
					}
					case STATE_USER: {
						control_printf(SL_FAILURE, "503 PASS expected.");
						return 1;
					}
					case STATE_AUTHENTICATED: {
						control_printf(SL_FAILURE, "503 RNFR before RNTO expected.");
						return 1;
					}
				}
			}
		}
	}
	control_printf(SL_FAILURE, "500 Unknown command: \"%s\"", str);
	return 0;
}

