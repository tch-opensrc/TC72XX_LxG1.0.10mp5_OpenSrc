#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <features.h>
#if __GLIBC__ >=2 && __GLIBC_MINOR >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#endif
#include <errno.h>

#include "packet.h"
#include "debug.h"
#include "dhcpd.h"
#include "options.h"

/* read a packet from socket fd */
int get_packet(struct dhcpMessage *packet, int fd)
{
	int bytes;
	int i;
	char broken_vendors[][8] = {
		"MSFT 98",
		""
	};
	char *vendor;

	memset(packet, 0, sizeof(struct dhcpMessage));
	bytes = read(fd, packet, sizeof(struct dhcpMessage));
	if (bytes < 0) {
		DEBUG(LOG_INFO, "couldn't read on listening socket -- ignoring");
		return -1;
	}

	if (ntohl(packet->cookie) != DHCP_MAGIC) {
		LOG(LOG_ERR, "received bogus message -- ignoring");
		return -1;
	}
	DEBUG(LOG_INFO, "oooooh!!! got some!");
	
	if (packet->op == BOOTREQUEST && (vendor = get_option(packet, DHCP_VENDOR))) {
		for (i = 0; broken_vendors[i][0]; i++) {
			if (vendor[OPT_LEN - 2] == (signed) strlen(broken_vendors[i]) &&
			    !strncmp(vendor, broken_vendors[i], vendor[OPT_LEN - 2]) &&
			    !(ntohs(packet->flags) & BROADCAST_FLAG)) {
			    	DEBUG(LOG_INFO, "broken client (%s), forcing broadcast",
			    		broken_vendors[i]);
			    	packet->flags |= htons(BROADCAST_FLAG);
			}
		}
	}
			    	

	return bytes;
}


/* Calculate the length of a packet, and make sure its a multiple of 2 */
static int calc_length(struct dhcpMessage *payload)
{
	int payload_length;
	
	payload_length = sizeof(struct dhcpMessage) - 308;
	payload_length += end_option(payload->options) + 1;
	if (payload_length % 2) {
		payload_length++;
		*((char *) payload + payload_length - 1) = '\0';
	}

	DEBUG(LOG_INFO, "payload length is %d bytes", payload_length);
	
	return payload_length;
}


u_int16_t checksum(void *addr, int count)
{
	/* Compute Internet Checksum for "count" bytes
	 *         beginning at location "addr".
	 */
	register int32_t sum = 0;
	u_int16_t *source = (u_int16_t *) addr;

	while( count > 1 )  {
		/*  This is the inner loop */
		sum += *source++;
		count -= 2;
	}

	/*  Add left-over byte, if any */
	if( count > 0 )
		sum += * (unsigned char *) source;

	/*  Fold 32-bit sum to 16 bits */
	while (sum>>16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}


/* Constuct a ip/udp header for a packet, and specify the source and dest hardware address */
int raw_packet(struct dhcpMessage *payload, u_int32_t source_ip, int source_port,
		   u_int32_t dest_ip, int dest_port, char *dest_arp, int ifindex)
{
	int fd;
	int result, payload_length = calc_length(payload);
	struct sockaddr_ll dest;
	struct udp_dhcp_packet packet;

	if ((fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP))) < 0) {
		DEBUG(LOG_ERR, "socket call failed: errno=%d", errno);
		return -1;
	}
	
	memset(&dest, 0, sizeof(dest));
	memset(&packet, 0, sizeof(packet));
	
	dest.sll_family = AF_PACKET;
	dest.sll_protocol = htons(ETH_P_IP);
	dest.sll_ifindex = ifindex;
	dest.sll_halen = 6;
	memcpy(dest.sll_addr, dest_arp, 6);
	if (bind(fd, (struct sockaddr *)&dest, sizeof(struct sockaddr_ll)) < 0) {
		DEBUG(LOG_ERR, "bind call failed: errno=%d", errno);
		close(fd);
		return -1;
	}

	packet.ip.protocol = IPPROTO_UDP;
	packet.ip.saddr = source_ip;
	packet.ip.daddr = dest_ip;
	packet.ip.tot_len = htons(sizeof(packet.udp) + payload_length); /* cheat on the psuedo-header */
	packet.udp.source = htons(source_port);
	packet.udp.dest = htons(dest_port);
	packet.udp.len = htons(sizeof(packet.udp) + payload_length);
	memcpy(&(packet.data), payload, payload_length);
	packet.udp.check = checksum(&packet, sizeof(packet.ip) + sizeof(packet.udp) + payload_length);
	
	packet.ip.tot_len = htons(sizeof(packet.ip) + sizeof(packet.udp) + payload_length);
	packet.ip.ihl = sizeof(packet.ip) >> 2;
	packet.ip.version = IPVERSION;
	packet.ip.ttl = IPDEFTTL;
	packet.ip.check = checksum(&(packet.ip), sizeof(packet.ip));

	result = sendto(fd, &packet, ntohs(packet.ip.tot_len), 0, (struct sockaddr *) &dest, sizeof(dest));
	if (result <= 0) {
		DEBUG(LOG_ERR, "write on socket failed: errno=%d", errno);
	}
	close(fd);
	return result;
}


/* Let the kernel do all the work for packet generation */
int kernel_packet(struct dhcpMessage *payload, u_int32_t source_ip, int source_port,
		   u_int32_t dest_ip, int dest_port)
{
	int n = 1;
	int fd, result, payload_length = calc_length(payload);
	struct sockaddr_in client;
	
	if ((fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		return -1;
	
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &n, sizeof(n)) == -1)
		return -1;

	memset(&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(source_port);
	client.sin_addr.s_addr = source_ip;

	if (bind(fd, (struct sockaddr *)&client, sizeof(struct sockaddr)) == -1)
		return -1;

	memset(&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(dest_port);
	client.sin_addr.s_addr = dest_ip; 

	if (connect(fd, (struct sockaddr *)&client, sizeof(struct sockaddr)) == -1)
		return -1;

	result = write(fd, payload, payload_length);
	close(fd);
	return result;
}	

