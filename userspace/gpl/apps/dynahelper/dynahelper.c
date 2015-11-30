#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "log.h"
#include "libipq.h"
#include <linux/netlink.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4/ip_queue.h>

/* ipq handle */
static struct ipq_handle *ipq;
extern char **environ;

int bcmDynahelperSystem(char *command)
{
	int pid = 0, status = 0;

	if (command == 0)
		return 1;

	pid = fork();
	if (pid == -1)
		return -1;

	if (pid == 0) {
		char *argv[4];
		argv[0] = "sh";
		argv[1] = "-c";
		argv[2] = command;
		argv[3] = 0;
		printf("app: %s\r\n", command);
		execve("/bin/sh", argv, environ);
		exit(127);
	}

	/* wait for child process return */
	do {
		if (waitpid(pid, &status, 0) == -1) {
			if (errno != EINTR)
				return -1;
		} else
			return status;
	} while (1);

	return status;
}

/****************************************************************************/
static void load_modules(char *proto)
{
	char cmd[128];

	sprintf(cmd, "insmod /lib/modules/2.6.21.5/kernel/net"
		"/netfilter/nf_conntrack_%s.ko", proto);
	bcmDynahelperSystem(cmd);
	sprintf(cmd, "insmod /lib/modules/2.6.21.5/kernel/net/ipv4"
		"/netfilter/nf_nat_%s.ko", proto);
	bcmDynahelperSystem(cmd);

	NOTICE("Helper %s loaded\n", proto);
}

/****************************************************************************/
static void unload_modules(char *proto)
{
	char ct_module[256] = "/sbin/rmmod nf_conntrack_";
	char nat_module[256] = "/sbin/rmmod nf_nat_";

	strcat(ct_module, proto);
	strcat(nat_module, proto);

	bcmDynahelperSystem(nat_module);
	bcmDynahelperSystem(ct_module);

	NOTICE("Helper %s unloaded\n", proto);
}

/****************************************************************************/
static void setup_ipq(void)
{
	ipq = ipq_create_handle(0, PF_INET);
	if (!ipq) {
		ERROR("ipq_create_handle() failed: %s\n", ipq_errstr());
		printf("ipq_create_handle() failed: %s\n", ipq_errstr());
		exit(1);
	}

	if (ipq_set_mode(ipq, IPQ_COPY_META,
			 NLMSG_SPACE(sizeof(ipq_packet_msg_t))) < 0) {
		ERROR("ipq_set_mode() failed: %s\n", ipq_errstr());
		printf("ipq_set_mode() failed: %s\n", ipq_errstr());
		exit(1);
	}
}

/****************************************************************************/
static void process_ipq_msg(char *data)
{
	ipq_packet_msg_t *msg = ipq_get_packet((const unsigned char *) data);

	switch (ipq_message_type((const unsigned char *) data)) {
	case NLMSG_ERROR:
		INFO("Received ipq error message %d\n",
		     ipq_get_msgerr((const unsigned char *) data));
		break;
	case IPQM_DYNAHELPER_LOAD:
		INFO("Received message to load helper %s\n", msg->indev_name);

#if defined(STATIC_KERNEL_NF_NAT_ALG_FTP)
		if (strstr(msg->indev_name, "ftp") != NULL)
			break;
#endif
#if defined(STATIC_KERNEL_NF_NAT_ALG_H323)
		if (strstr(msg->indev_name, "h323") != NULL)
			break;
#endif
#if defined(STATIC_KERNEL_NF_NAT_ALG_IPSEC)
		if (strstr(msg->indev_name, "ipsec") != NULL)
			goto loaded;
#endif
		/* Load ct and nat modules */
		load_modules(msg->indev_name);
		break;
	case IPQM_DYNAHELPER_UNLOAD:
		INFO("Received message to unload helper %s\n",
		     msg->indev_name);

#if defined(STATIC_KERNEL_NF_NAT_ALG_FTP)
		if (strstr(msg->indev_name, "ftp") != NULL)
			break;
#endif
#if defined(STATIC_KERNEL_NF_NAT_ALG_H323)
		if (strstr(msg->indev_name, "h323") != NULL)
			break;
#endif
#if defined(STATIC_KERNEL_NF_NAT_ALG_IPSEC)
		if (strstr(msg->indev_name, "ipsec") != NULL)
			break;
#endif
		unload_modules(msg->indev_name);
		break;
	default:
		INFO("Unknown ipq message type\n");
		break;
	}
}

/****************************************************************************/
int main(int argc, char *argv[])
{
	FILE *fp;
	unsigned char buf[NLMSG_SPACE(sizeof(ipq_packet_msg_t))];

	printf("Starting Dynahelper deamon...\n");

	setlogmask(LOG_UPTO(LOG_DEBUG));

	/* Enter daemon mode */
	daemon(1, 1);

	/* Write pid */
	if (!(fp = fopen("/var/run/dynahelper.pid", "w"))) {
		ERROR("Open pid file error: %m\n");
		return 1;
	}
	fprintf(fp, "%d\n", getpid());
	fclose(fp);
	DEBUG("pid file created\n");

	/* Setup ipq */
	setup_ipq();

	do {
		int ret = ipq_read(ipq, buf, sizeof(buf), 1000000);

		if (ret < 0) {
			INFO("ipq_read() failed: %s\n", ipq_errstr());
			printf("ipq_read() failed: %s\n", ipq_errstr());
		} else if (ret > 0)
			process_ipq_msg((char *) buf);
	} while (1);

	return 0;
}
