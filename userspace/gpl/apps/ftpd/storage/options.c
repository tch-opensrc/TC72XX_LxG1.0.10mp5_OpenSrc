#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <grp.h>
#include <unistd.h>

#include "options.h"
#include "mystring.h"
#include "main.h"
#include "login.h"
#include <logging.h>

struct global config_global;
struct group_of_users *config_groups;
struct user *config_users;

char *config_read_line(FILE *configfile)
{
	static char str[256];
	char *s = str;
	if (!fgets(str, sizeof(str), configfile))
		return NULL;
	while ((strchr(s, '#') > strchr(s, '"')) && strchr(s, '"')) {
		s = strchr(strchr(s, '"') + 1, '"');
		if (!s) { // This means there is only one " in the string, which is a syntax error.
			str[0] = 0; // So we empty the string in order not to confuse the parser.
			return str;
		}
	}
	if (strchr(s, '#'))
		*strchr(s, '#') = 0;
	s = str;
	while ((s[0] == ' ') || (s[0] == '\t'))
		s++;
	return s;
}

void create_options(FILE *configfile, struct bftpd_option **options, struct directory **directories)
{
    char *str;
    struct bftpd_option *opt = NULL;
    struct directory *dir = NULL;
	str = config_read_line(configfile);
	while (!strchr(str, '}')) {
  		if (str[0] != '\n') {
            if ((strstr(str, "directory")) && (strchr(str, '{')) && (directories)) {
                char *tmp;
                if (dir) {
                    dir = dir->next = malloc(sizeof(struct directory));
                } else {
                    *directories = dir = malloc(sizeof(struct directory));
                }
                tmp = strchr(str, '"') + 1;
                *strchr(tmp, '"') = 0;
                dir->path = strdup(tmp);
                create_options(configfile, &(dir->options), NULL);
            } else {
       			if (opt) {
       				opt = opt->next = malloc(sizeof(struct bftpd_option));
       			} else {
       				*options = opt = malloc(sizeof(struct bftpd_option));
       			}
       			opt->name = (char *) malloc(strlen(str));
       			opt->value = (char *) malloc(strlen(str));
       			sscanf(str, "%[^=]=\"%[^\n\"]", opt->name, opt->value);
            }
   		}
		str = config_read_line(configfile);
	}
}

void expand_groups()
{
    char foo[USERLEN + 1];
    struct passwd *temp;
    struct group_of_users *grp;
    struct group *grpinfo;
    struct list_of_struct_passwd *endp = NULL;
    struct list_of_struct_group *endg = NULL;
    uid_t uid;
    int i;
    if ((grp = config_groups)) {
        do {
            strcat(grp->temp_members, ",");
            while (strchr(grp->temp_members, ',')) {
                sscanf(grp->temp_members, "%[^,]", foo);
                cutto(grp->temp_members, strlen(foo) + 1);
                if (foo[0] == '@') {
                    if (sscanf(foo + 1, "%i", &uid)) {
                        if (!((grpinfo = getgrgid(uid))))
                            continue;
                    } else
                        if (!((grpinfo = getgrnam(foo + 1))))
                            continue;
                    if (grp->groups)
                        endg = endg->next = malloc(sizeof(struct list_of_struct_group));
                    else
                        grp->groups = endg = malloc(sizeof(struct list_of_struct_group));
                    endg->grp.gr_name = strdup(grpinfo->gr_name);
                    endg->grp.gr_passwd = strdup(grpinfo->gr_passwd);
                    endg->grp.gr_gid = grpinfo->gr_gid;
                    for (i = 0; grpinfo->gr_mem[i]; i++);
                    endg->grp.gr_mem = malloc((i + 1) * sizeof(char *));
                    for (i = 0; grpinfo->gr_mem[i]; i++)
                        endg->grp.gr_mem[i] = strdup(grpinfo->gr_mem[i]);
                    endg->grp.gr_mem[i] = NULL;
                } 
                if (sscanf(foo, "%i", &uid)) {
                    if (!((temp = getpwuid(uid))))
                        continue;
                } else
                    if (!((temp = getpwnam(foo))))
                        continue;
                if (grp->users)
                    endp = endp->next = malloc(sizeof(struct list_of_struct_passwd));
                else
                    grp->users = endp = malloc(sizeof(struct list_of_struct_passwd));
                /* This is ugly, but you can't just use memcpy()! */
                endp->pwd.pw_name = strdup(temp->pw_name);
                endp->pwd.pw_passwd = strdup(temp->pw_passwd);
                endp->pwd.pw_uid = temp->pw_uid;
                endp->pwd.pw_gid = temp->pw_gid;
                endp->pwd.pw_gecos = strdup(temp->pw_gecos);
                endp->pwd.pw_dir = strdup(temp->pw_dir);
                endp->pwd.pw_shell = strdup(temp->pw_shell);
            }
            free(grp->temp_members);
        } while ((grp = grp->next));
    }
}

void config_init()
{
	FILE *configfile;
	char *str;
    struct group_of_users *grp = NULL;
    struct user *usr = NULL;
    config_global.options = NULL;
    config_global.directories = NULL;
	if (!configpath)
		return;
	configfile = fopen(configpath, "r");
	if (!configfile) {
		control_printf(SL_FAILURE, "421 Unable to open configuration file.");
		exit(1);
	}
	while ((str = config_read_line(configfile))) {
		if (strchr(str, '{')) {
            replace(str, " {", "{");
            replace(str, "{ ", "{");
            replace(str, " }", "}");
            replace(str, "} ", "}");
            if (!strcasecmp(str, "global{\n")) {
                create_options(configfile, &(config_global.options), &(config_global.directories));
            } else if (strstr(str, "user ") == str) {
                if (usr) {
                    usr = usr->next = malloc(sizeof(struct user));
                } else {
                    config_users = usr = malloc(sizeof(struct user));
                }
                usr->name = strdup(str + 5);
                *strchr(usr->name, '{') = 0;
                create_options(configfile, &(usr->options), &(usr->directories));
            } else if (strstr(str, "group ") == str) {
                if (grp) {
                    grp = grp->next = malloc(sizeof(struct group_of_users));
                } else {
                    config_groups = grp = malloc(sizeof(struct group_of_users));
                }
                cutto(str, 6);
                *strchr(str, '{') = 0;
                grp->users = NULL;
                grp->next = NULL;
                grp->temp_members = strdup(str);
                create_options(configfile, &(grp->options), &(grp->directories));
            }
		}
	}
	fclose(configfile);
}

char *getoption(struct bftpd_option *opt, char *name)
{
	if (!opt)
		return NULL;
	do {
		if (!strcasecmp(opt->name, name))
			return opt->value;
	} while ((opt = opt->next));
    return NULL;
}

char *getoption_directories(struct directory *dir, char *name) {
    char curpath[256], *bar;
    struct directory *longest = NULL;
    if(!dir)
        return NULL;
    getcwd(curpath, sizeof(curpath) - 1);
    strcat(curpath, "/");
    do {
        bar = malloc(strlen(dir->path) + 2);
        strcpy(bar, dir->path);
        strcat(bar, "/");
        if (!strncmp(curpath, bar, strlen(bar))) {
            if (longest) {
                if ((strlen(bar) > strlen(longest->path) + 1)
                    && (getoption(dir->options, name)))
                    longest = dir;
            } else {
                if (getoption(dir->options, name))
                    longest = dir;
            }
        }
        free(bar);
    } while ((dir = dir->next));
    if (longest)
        return getoption(longest->options, name);
    return NULL;
}

char user_is_in_group(struct group_of_users *grp) {
    struct list_of_struct_group *grplist = grp->groups;
    struct list_of_struct_passwd *pwdlist = grp->users;
    int i;
    if (pwdlist) {
        do {
            if (!strcmp(user, pwdlist->pwd.pw_name))
                return 1;
        } while ((pwdlist = pwdlist->next));
    }
    if (grplist) {
        do {
            if (userinfo.pw_gid == grplist->grp.gr_gid)
                return 1;
            for (i = 0; grplist->grp.gr_mem[i]; i++)
                if (!strcmp(grplist->grp.gr_mem[i], user))
                    return 1;
        } while ((grplist = grplist->next));
    }
    return 0;
}

char *getoption_group(char *name)
{
    char *result;
    struct group_of_users *grp;
    if ((grp = config_groups)) {
        do {
            if (user_is_in_group(grp) && grp->options) {
                if ((result = getoption_directories(grp->directories, name)))
                    return result;
                if ((result = getoption(grp->options, name)))
                    return result;
            }
        } while ((grp = grp->next));
    }
    return NULL;
}

char *getoption_user(char *name)
{
    char *result;
    struct user *usr;
    if ((usr = config_users)) {
        do {
            if (!strcmp(user, usr->name)) {
                if ((result = getoption_directories(usr->directories, name)))
                    return result;
                if ((result = getoption(usr->options, name)))
                    return result;
            }
        } while ((usr = usr->next));
    }
    return NULL;
}

char *getoption_global(char *name)
{
    char *result;
    if ((result = getoption_directories(config_global.directories, name)))
        return result;
    if (config_global.options) {
        if ((result = getoption(config_global.options, name)))
            return result;
    }
    return NULL;
}

char *config_getoption(char *name)
{
    static char empty = 0;
    char *result;
	char *foo;
    if (userinfo_set) {
        if ((foo = getoption_user(name)))
            return foo;
        if ((foo = getoption_group(name)))
            return foo;
    }
    if ((result = getoption_global(name)))
        return result;
    else
        return &empty;
}

void config_end()
{
    /* Needn't do anything ATM */
}
