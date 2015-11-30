#include <config.h>
#include <stdio.h>
#include <pwd.h>
#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif
#ifdef WANT_PAM
#include <security/pam_appl.h>
#endif
#ifdef HAVE_UTMP_H
# include <utmp.h>
# ifdef HAVE_PATHS_H
#  include <paths.h>
#  ifndef _PATH_WTMP
#   define _PATH_WTMP "/dev/null"
#   warning "<paths.h> doesn't set _PATH_WTMP. You can not use wtmp logging"
#   warning "with bftpd."
#  endif
# else
#  define _PATH_WTMP "/dev/null"
#  warning "<paths.h> was not found. You can not use wtmp logging with bftpd."
# endif
#endif
#include <errno.h>
#include <grp.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "cwd.h"
#include "dirlist.h"
#include "mystring.h"
#include "options.h"
#include "login.h"
#include "logging.h"
#include "bftpdutmp.h"
#include "main.h"
#include "syslog.h"

// brcm
#include "cms_util.h"
#include "cms_msg.h"
extern NetworkAccessMode accessMode;

#ifdef WANT_PAM
char usepam = 0;
pam_handle_t *pamh = NULL;
#endif

#ifdef HAVE_UTMP_H
FILE *wtmp;
#endif

struct passwd userinfo;
char userinfo_set = 0;
#if 0 //brcm
char *mygetpwuid(int uid, FILE * file, char *name)
{
	int _uid;
	char foo[256];
    int i;
	if (file) {
		rewind(file);
        while (fscanf(file, "%255s%*[^\n]\n", foo) != EOF) {
            if ((foo[0] == '#') || (!strchr(foo, ':')) || (strchr(foo, ':') > foo + USERLEN - 1))
                continue;
            i = strchr(foo, ':') - foo;
            strncpy(name, foo, i);
            name[i] = 0;
			sscanf(strchr(foo + i + 1, ':') + 1, "%i", &_uid);
			if (_uid == uid) {
				if (name[0] == '\n')
					cutto(name, 1);
				return name;
			}
		}
	}
	sprintf(name, "%i", uid);
	return name;
}

int mygetpwnam(char *name, FILE * file)
{
	char _name[USERLEN + 1];
	char foo[256];
	int uid, i;
	if (file) {
		rewind(file);
        while (fscanf(file, "%255s%*[^\n]\n", foo) != EOF) {
            if ((foo[0] == '#') || (!strchr(foo, ':')) || (strchr(foo, ':') > foo + USERLEN - 1))
                continue;
            i = strchr(foo, ':') - foo;
            strncpy(_name, foo, i);
            _name[i] = 0;
			sscanf(strchr(foo + i + 1, ':') + 1, "%i", &uid);
			if (_name[0] == '\n')
				cutto(_name, 1);
			if (!strcmp(name, _name))
				return uid;
		}
	}
	return -1;
}
#endif //brcm

#ifdef HAVE_UTMP_H
void wtmp_init()
{
	if (strcasecmp(config_getoption("LOG_WTMP"), "no")) {
		if (!((wtmp = fopen(_PATH_WTMP, "a"))))
			bftpd_log("Warning: Unable to open %s.\n", _PATH_WTMP);
	}
}

void bftpd_logwtmp(char type)
{
	struct utmp ut;
	if (!wtmp)
		return;
	memset((void *) &ut, 0, sizeof(ut));
#ifdef _HAVE_UT_PID
	ut.ut_pid = getpid();
#endif
	sprintf(ut.ut_line, "ftp%i", (int) getpid());
	if (type) {
#ifdef _HAVE_UT_TYPE
		ut.ut_type = USER_PROCESS;
#endif
		strncpy(ut.ut_name, user, sizeof(ut.ut_name));
#ifdef _HAVE_UT_HOST   
		strncpy(ut.ut_host, remotehostname, sizeof(ut.ut_host));
#endif    
	} else {
#ifdef _HAVE_UT_TYPE
		ut.ut_type = DEAD_PROCESS;
#endif
	}
	time(&(ut.ut_time));
	fseek(wtmp, 0, SEEK_END);
	fwrite((void *) &ut, sizeof(ut), 1, wtmp);
	fflush(wtmp);
}

void wtmp_end()
{
	if (wtmp) {
		if (state >= STATE_AUTHENTICATED)
			bftpd_logwtmp(0);
		fclose(wtmp);
	}
}
#endif
#if 0 //brcm
void login_init()
{
    char *foo = config_getoption("INITIAL_CHROOT");
#ifdef HAVE_UTMP_H
	wtmp_init();
#endif
    if (foo[0]) { /* Initial chroot */
        if (chroot(foo) == -1) {
            control_printf(SL_FAILURE, "421 Initial chroot to '%s' failed.\r\n%s.",
                    foo, strerror(errno));
            exit(1);
        }
    }
}
#endif //brcm
int bftpd_setuid(uid_t uid)
{
#if 0 //brcm
    /* If we must open the data connections from port 20,
     * we have to keep the possibility to regain root privileges */
    if (!strcasecmp(config_getoption("DATAPORT20"), "yes"))
        return seteuid(uid);
    else
#endif //brcm
        return setuid(uid);
}


int bftpd_login(char *password)
{
#if 0	// brcm 
    char str[256];
	char *foo;
	int maxusers;
#endif //brcm
//printf("In bftpd_login password=%s, user=%s\n", password, user); // brcm
	if (!getpwnam(user)) {
        syslog(LOG_WARNING,"104051 FTP Server Login UserName or Password Error\n");
        control_printf(SL_FAILURE, "421 Login incorrect.");
		return 1;
    }
    // brcm add local/remote login check
    if ((accessMode == NETWORK_ACCESS_DISABLED) ||
        (accessMode == NETWORK_ACCESS_LAN_SIDE && (strcmp(user, "user") && strcmp(user, "admin"))) ||
        (accessMode == NETWORK_ACCESS_WAN_SIDE && strcmp(user, "support")))
    {
       syslog(LOG_WARNING,"104051 FTP Server Login UserName or Password Error\n");
       cmsLog_debug("wrong username, accessMode=%d user=%s", accessMode, user);
       control_printf(SL_FAILURE, "421 Login incorrect.");
	    return 1;
    }
#if 0 //brcm
	if (strncasecmp(foo = config_getoption("DENY_LOGIN"), "no", 2)) {
		if (foo[0] != '\0') {
			if (strncasecmp(foo, "yes", 3))
				control_printf(SL_FAILURE, "421-Server disabled.\r\n421 Reason: %s", foo);
			else
				control_printf(SL_FAILURE, "421 Login incorrect.");
			bftpd_log("Login as user '%s' failed: Server disabled.\n", user);
			exit(0);
		}
	}
	maxusers = strtoul(config_getoption("USERLIMIT_GLOBAL"), NULL, 10);
	if ((maxusers) && (maxusers == bftpdutmp_usercount("*"))) {
		control_printf(SL_FAILURE, "421 There are already %i users logged in.", maxusers);
		exit(0);
	}
	maxusers = strtoul(config_getoption("USERLIMIT_SINGLEUSER"), NULL, 10);
	if ((maxusers) && (maxusers == bftpdutmp_usercount(user))) {
		control_printf(SL_FAILURE, "421 User %s is already logged in %i times.", user, maxusers);
		exit(0);
	}
#endif //brcm

    if(checkuser() || checkshell()) {
      syslog(LOG_WARNING,"104051 FTP Server Login UserName or Password Error\n");
      cmsLog_debug("checkuser or checkshell failed");
		control_printf(SL_FAILURE, "421 Login incorrect.");
		exit(0);
	}
	if (checkpass(password))
   {
      cmsLog_debug("checkpass returned non-zero");
		return 1;
   }
#if 0 //brcm
	if (strcasecmp((char *) config_getoption("RATIO"), "none")) {
		sscanf((char *) config_getoption("RATIO"), "%i/%i",
			   &ratio_send, &ratio_recv);
	}
	strcpy(str, config_getoption("ROOTDIR"));
	if (!str[0])
		strcpy(str, "%h");
	replace(str, "%u", userinfo.pw_name);
	replace(str, "%h", userinfo.pw_dir);
	if (!strcasecmp(config_getoption("RESOLVE_UIDS"), "yes")) {
		passwdfile = fopen("/etc/passwd", "r");
		groupfile = fopen("/etc/group", "r");
	}
#endif //brcm
	setgid(userinfo.pw_gid);

	initgroups(userinfo.pw_name, userinfo.pw_gid);
#if 0 //brcm
	if (strcasecmp(config_getoption("DO_CHROOT"), "no")) {
		if (chroot(str)) {
			control_printf(SL_FAILURE, "421 Unable to change root directory.\r\n%s.",
					strerror(errno));
			exit(0);
		}
		if (bftpd_setuid(userinfo.pw_uid)) {
			control_printf(SL_FAILURE, "421 Unable to change uid.");
			exit(0);
		}
		if (chdir("/")) {
			control_printf(SL_FAILURE, "421 Unable to change working directory.\r\n%s.",
					 strerror(errno));
			exit(0);
		}
	} else {
		if (bftpd_setuid(userinfo.pw_uid)) {
			control_printf(SL_FAILURE, "421 Unable to change uid.");
			exit(0);
		}
		if (chdir(str)) {
			control_printf(SL_FAILURE, "230 Couldn't change cwd to '%s': %s.", str,
					 strerror(errno));
			chdir("/");
		}
	}
    new_umask();
	print_file(230, config_getoption("MOTD_USER"));
#endif //brcm
	
    control_printf(SL_SUCCESS, "230 User logged in.");

#ifdef HAVE_UTMP_H
	bftpd_logwtmp(1);
#endif
#if 0 //brcm
    bftpdutmp_log(1);
	bftpd_log("Successfully logged in as user '%s'.\n", user);

    if (config_getoption("AUTO_CHDIR")[0])
        chdir(config_getoption("AUTO_CHDIR"));
#endif //brcm        
	state = STATE_AUTHENTICATED;

#ifdef SUPPORT_FTPD_STORAGE
	bftpd_cwd_init();
#endif

	return 0;
}

int checkpass(char *password)
{
    if (!getpwnam(user))
		return 1;
#if 0 //brcm
    if (!strcasecmp(config_getoption("ANONYMOUS_USER"), "yes"))
		return 0;
#endif //brcm
#ifdef WANT_PAM
	if (!strcasecmp(config_getoption("AUTH"), "pam"))
		return checkpass_pam(password);
	else
#endif
	return checkpass_pwd(password);
}

void login_end()
{
#ifdef WANT_PAM
	if (usepam)
		return end_pam();
#endif
#ifdef HAVE_UTMP_H
	wtmp_end();
#endif
}

int checkpass_pwd(char *password)
{
#ifdef HAVE_SHADOW_H
	struct spwd *shd;
#endif
    if (strncmp(userinfo.pw_passwd, (char *) crypt(password, userinfo.pw_passwd), strlen(userinfo.pw_passwd))) {
#ifdef HAVE_SHADOW_H
		if (!(shd = getspnam(user)))
			return 1;
		if (strcmp(shd->sp_pwdp, (char *) crypt(password, shd->sp_pwdp)))
#endif
         cmsLog_debug("passwd check failed, userinfo.pw_passwd=%s password=%s", userinfo.pw_passwd, password);
         cmsLog_debug("crypt version %s", crypt(password, userinfo.pw_passwd));
			return 1;
	}
	return 0;
}

#ifdef WANT_PAM
int conv_func(int num_msg, const struct pam_message **msgm,
			  struct pam_response **resp, void *appdata_ptr)
{
	struct pam_response *response;
	int i;
	response = (struct pam_response *) malloc(sizeof(struct pam_response)
											  * num_msg);
	for (i = 0; i < num_msg; i++) {
		response[i].resp = (char *) strdup(appdata_ptr);
		response[i].resp_retcode = 0;
	}
	*resp = response;
	return 0;
}

int checkpass_pam(char *password)
{
	struct pam_conv conv = { conv_func, password };
	int retval = pam_start("bftpd", user, (struct pam_conv *) &conv,
						   (pam_handle_t **) & pamh);
	if (retval != PAM_SUCCESS) {
		printf("Error while initializing PAM: %s\n",
			   pam_strerror(pamh, retval));
		return 1;
	}
	pam_fail_delay(pamh, 0);
	retval = pam_authenticate(pamh, 0);
	if (retval == PAM_SUCCESS)
		retval = pam_acct_mgmt(pamh, 0);
	if (retval == PAM_SUCCESS)
		pam_open_session(pamh, 0);
	if (retval != PAM_SUCCESS)
		return 1;
	else
		return 0;
}

void end_pam()
{
	if (pamh) {
		pam_close_session(pamh, 0);
		pam_end(pamh, 0);
	}
}
#endif

int checkuser()
{
#if 0 //brcm 
	FILE *fd;
	char *p;
	char line[256];

	if ((fd = fopen(config_getoption("PATH_FTPUSERS"), "r"))) {
		while (fgets(line, sizeof(line), fd))
			if ((p = strchr(line, '\n'))) {
				*p = '\0';
				if (line[0] == '#')
					continue;
				if (!strcasecmp(line, user)) {
					fclose(fd);
					return 1;
				}
			}
		fclose(fd);
	}
#endif //brcm 
	return 0;
}

int checkshell()
{
#ifdef HAVE_GETUSERSHELL
	char *cp;
	struct passwd *pwd;
#if 0 //brcm
    if (!strcasecmp(config_getoption("AUTH_ETCSHELLS"), "no"))
        return 0;
#endif //brcm    
	pwd = getpwnam(user);

	while ((cp = getusershell()))
		if (!strcmp(cp, pwd->pw_shell))
			break;
	endusershell();

	if (!cp)
		return 1;
	else
		return 0;
#else
    return 0;
#   warning "Your system doesn't have getusershell(). You can not"
#   warning "use /etc/shells authentication with bftpd."
#endif
}

