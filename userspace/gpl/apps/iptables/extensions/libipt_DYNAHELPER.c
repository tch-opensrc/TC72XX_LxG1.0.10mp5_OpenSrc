#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <getopt.h>
#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ip_queue.h>

#define IPT_DYNAHELPER_OPT_PROTO 0x01
#define IPT_DYNAHELPER_OPT_TIMEOUT 0x02

/****************************************************************************/
/* Function which prints out usage message. */
static void help(void)
{
	printf(
"DYNAHELPER v%s options:\n"
" --proto protocol		Protocol name, e.g. ftp, h323, etc.\n"
" --timeout seconds		Helper's idle timeout.\n\n",
IPTABLES_VERSION);
}

/****************************************************************************/
static struct option opts[] = {
	{.name = "proto",.has_arg = 1,.flag = 0,.val = '!'},
	{.name = "timeout",.has_arg = 1,.flag = 0,.val = '#'},
	{.name = 0}
};

/****************************************************************************/
/* Initialize the target. */
static void init(struct ipt_entry_target *t, unsigned int *nfcache)
{
	struct xt_dynahelper_info *info =
	    (struct xt_dynahelper_info *) t->data;

	info->tracker = NULL;
	info->timeout = DYNAHELPER_DEFAULT_TIMEOUT;
}

/****************************************************************************/
/* Function which parses command options; returns true if it
   ate an option */
static int parse(int c, char **argv, int invert, unsigned int *flags,
		 const struct ipt_entry *entry,
		 struct ipt_entry_target **target)
{
	struct xt_dynahelper_info *info =
	    (struct xt_dynahelper_info *) (*target)->data;
	char *p;

	switch (c) {
	case '!':
		if (*flags & IPT_DYNAHELPER_OPT_PROTO)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --proto twice");

		if (check_inverse(optarg, &invert, NULL, 0))
			exit_error(PARAMETER_PROBLEM,
				   "Unexpected `!' after --proto");

		if (strlen(optarg) > DYNAHELPER_MAXPROTONAMELEN)
			exit_error(PARAMETER_PROBLEM,
				   "Maximum protocol name length %u for "
				   "--proto", DYNAHELPER_MAXPROTONAMELEN);

		if (strlen(optarg) != strlen(strtok(optarg, "\n")))
			exit_error(PARAMETER_PROBLEM,
				   "Newlines not allowed in --proto");

		strcpy(info->proto, optarg);
		for (p = info->proto; *p; p++) {
			if (isupper(*p))
				*p = tolower(*p);
		}
		*flags |= IPT_DYNAHELPER_OPT_PROTO;
		break;
	case '#':
		if (*flags & IPT_DYNAHELPER_OPT_TIMEOUT)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --timeout twice");

		if (check_inverse(optarg, &invert, NULL, 0))
			exit_error(PARAMETER_PROBLEM,
				   "Unexpected `!' after --timeout");

		info->timeout = strtoul(optarg, NULL, 0);
		*flags |= IPT_DYNAHELPER_OPT_TIMEOUT;
		break;
	default:
		return 0;
	}

	return 1;
}

/****************************************************************************/
/* Final check; nothing. */
static void final_check(unsigned int flags)
{
	if (!(flags & IPT_DYNAHELPER_OPT_PROTO))
		exit_error(PARAMETER_PROBLEM,
			   "DYNAHELPER: must specify --proto");
}

/****************************************************************************/
/* Prints out the targinfo. */
static void print(const struct ipt_ip *ip,
		  const struct ipt_entry_target *target, int numeric)
{
	const struct xt_dynahelper_info *info
	    = (const struct xt_dynahelper_info *) target->data;

	printf("DYNAHELPER ");
	printf("proto %s timeout %lu", info->proto, info->timeout);
}

/****************************************************************************/
/* Saves the union ipt_targinfo in parsable form to stdout. */
static void save(const struct ipt_ip *ip,
		 const struct ipt_entry_target *target)
{
	const struct xt_dynahelper_info *info
	    = (const struct xt_dynahelper_info *) target->data;

	printf("--proto %s --timeout %lu", info->proto, info->timeout);
}

/****************************************************************************/
static struct iptables_target dynahelper = {
	.name = "DYNAHELPER",
	.version = IPTABLES_VERSION,
	.size = IPT_ALIGN(sizeof(struct xt_dynahelper_info)),
	.userspacesize = IPT_ALIGN(sizeof(struct xt_dynahelper_info)),
	.help = &help,
	.init = &init,
	.parse = &parse,
	.final_check = &final_check,
	.print = &print,
	.save = &save,
	.extra_opts = opts
};

/****************************************************************************/
void _init(void)
{
	register_target(&dynahelper);
}
