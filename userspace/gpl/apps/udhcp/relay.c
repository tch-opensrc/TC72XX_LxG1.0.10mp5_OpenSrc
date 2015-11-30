#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <assert.h>
#include <sys/select.h>
#include <sys/param.h>
#include <errno.h>
#include <alloca.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <syslog.h>
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <netinet/udp.h>

#include <linux/if_packet.h>
#include <linux/if_ether.h>

#include "debug.h"
#include "dhcpd.h"

#define BUFFER_SIZE   2048

struct dhcp {
	u_int8_t op;
	u_int8_t htype;
	u_int8_t hlen;
	u_int8_t hops;
	u_int32_t xid;
	u_int16_t secs;
	u_int16_t flags;
	u_int32_t ciaddr;
	u_int32_t yiaddr;
	u_int32_t siaddr;
	u_int32_t giaddr;
	u_int8_t chaddr[16];
	u_int8_t srvname[64];
	u_int8_t file[128];
	u_int32_t cookie;
	u_int8_t options[308];
};

static void toSock(int sock, char *buf, int size, struct sockaddr_in *to)
{
	if (sendto(sock, buf, size, 0, (struct sockaddr *) to, sizeof(*to)) ==
	    -1){
		LOG(LOG_ERR, "dhcp relay: sendto failed. \n");
	}
}

static void pktHandler(char *buffer, unsigned int ip)
{
	((struct dhcp *) buffer)->hops += 1;

	if (!((struct dhcp *) buffer)->giaddr)
		((struct dhcp *) buffer)->giaddr = ip;
}

void process_relay_request(char *pkt, int len)
{
	struct relay_config_t *relay;

	if ((((struct dhcp *) pkt)->hops & 0xff) > 6)
		return;
	pktHandler(pkt, cur_iface->server);

	for (relay = relay_config; relay; relay = relay->next) {
		if (!strcmp(relay->interface, cur_iface->relay_interface)) {
			struct sockaddr_in remote_addr;
			memset(&remote_addr, 0, sizeof(remote_addr));
			remote_addr.sin_family = AF_INET;
			remote_addr.sin_port = htons(SERVER_PORT);
			remote_addr.sin_addr.s_addr = cur_iface->relay_remote;
			toSock(relay->skt, pkt, len, &remote_addr);
			break;
		}
	}
}

void process_relay_response(void)
{
	int len;
	struct sockaddr_in from;
	unsigned int fromSize = sizeof(from);
	char *buf = malloc(BUFFER_SIZE);
	struct iface_config_t *iface;

	len = recvfrom(cur_relay->skt, buf, BUFFER_SIZE, 0,
		       (struct sockaddr *) &from, &fromSize);
	if (len == -1) {
		LOG(LOG_NOTICE, "dhcp relay request: recvfrom failed. \n");
		goto end;
	}

	for (iface = iface_config; iface; iface = iface->next) {
		if (from.sin_addr.s_addr == iface->relay_remote &&
		    ((struct dhcp *) buf)->giaddr == iface->server) {
			struct sockaddr_in addr;
			memset(&addr, 0, sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_port = htons(CLIENT_PORT);
			addr.sin_addr.s_addr = INADDR_BROADCAST;
			toSock(iface->skt, buf, len, &addr);
			break;
		}
	}

      end:
	free(buf);
}

