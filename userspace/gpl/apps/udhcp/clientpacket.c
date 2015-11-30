/* clientpacket.c
 *
 * Packet generation and dispatching functions for the DHCP client.
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 
#include <string.h>
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
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "dhcpd.h"
#include "packet.h"
#include "options.h"
#include "dhcpc.h"
#include "debug.h"


/* Create a random xid */
unsigned long random_xid(void)
{
	static int initialized;
	if (!initialized) {
		srand(time(0));
		initialized++;
	}
	return rand();
}


/* initialize a packet with the proper defaults */
static void init_packet(struct dhcpMessage *packet, char type)
{
	//brcm
    char VSI[256];
    char client_id[256];	
	struct vendor  {
		char vendor, length;
		char str[sizeof("uDHCP "VERSION)];
	} vendor_id = { DHCP_VENDOR,  sizeof("uDHCP "VERSION) - 1, "uDHCP "VERSION};
	
	memset(packet, 0, sizeof(struct dhcpMessage));
	
	packet->op = BOOTREQUEST;
	packet->htype = ETH_10MB;
	packet->hlen = ETH_10MB_LEN;
	packet->cookie = htonl(DHCP_MAGIC);
	packet->options[0] = DHCP_END;
	memcpy(packet->chaddr, client_config.arp, 6);
	add_simple_option(packet->options, DHCP_MESSAGE_TYPE, type);
	if (client_config.clientid) add_option_string(packet->options, client_config.clientid);
	if (client_config.hostname) add_option_string(packet->options, client_config.hostname);
	// brcm
	if (en_vendor_class_id) { 
	   if (strlen(vendor_class_id)) {
	       vendor_id.length = strlen(vendor_class_id);
	       sprintf(vendor_id.str, "%s", vendor_class_id);
	       add_option_string(packet->options, (char *) &vendor_id);
	   } else {
	       add_option_string(packet->options, (char *) &vendor_id);
	   }
	}
	if (en_125){
       if ( CreateOption125(VENDOR_IDENTIFYING_FOR_DEVICE, oui_125, sn_125, prod_125, VSI) != -1 ) {
           add_option_string(packet->options,VSI);
	   }
	}
    if ( en_client_id == 1 ){
	   if (strlen(iaid) && strlen(duid)) {
	       if (CreateClntId(iaid, duid, client_id) != -1)
	          add_option_string(packet->options, client_id);
	   }
	}
}


/* Add a paramater request list for stubborn DHCP servers */
static void add_requests(struct dhcpMessage *packet)
{
	char request_list[] = {DHCP_PARAM_REQ, 0, PARM_REQUESTS};
	
	request_list[OPT_LEN] = sizeof(request_list) - 2;
	add_option_string(packet->options, request_list);
}


/* Broadcast a DHCP discover packet to the network, with an optionally requested IP */
int send_discover(unsigned long xid, unsigned long requested)
{
	struct dhcpMessage packet;

	init_packet(&packet, DHCPDISCOVER);
	packet.xid = xid;
	if (requested)
		add_simple_option(packet.options, DHCP_REQUESTED_IP, ntohl(requested));

	add_requests(&packet);
	// brcm
	// LOG(LOG_DEBUG, "Sending discover...");
	return raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST, 
				SERVER_PORT, MAC_BCAST_ADDR, client_config.ifindex);
}


/* Broadcasts a DHCP request message */
int send_selecting(unsigned long xid, unsigned long server, unsigned long requested)
{
	struct dhcpMessage packet;
	struct in_addr addr;

	init_packet(&packet, DHCPREQUEST);
	packet.xid = xid;

	/* expects host order */
	add_simple_option(packet.options, DHCP_REQUESTED_IP, ntohl(requested));

	/* expects host order */
	add_simple_option(packet.options, DHCP_SERVER_ID, ntohl(server));
	
	add_requests(&packet);
	addr.s_addr = requested;
	// brcm
	//LOG(LOG_DEBUG, "Sending select for %s...", inet_ntoa(addr));
	return raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST, 
				SERVER_PORT, MAC_BCAST_ADDR, client_config.ifindex);
}


/* Unicasts or broadcasts a DHCP renew message */
int send_renew(unsigned long xid, unsigned long server, unsigned long ciaddr)
{
	struct dhcpMessage packet;
	int ret = 0;

	init_packet(&packet, DHCPREQUEST);
	packet.xid = xid;
	packet.ciaddr = ciaddr;

	LOG(LOG_DEBUG, "Sending renew...");
	if (server) 
		ret = kernel_packet(&packet, ciaddr, CLIENT_PORT, server, SERVER_PORT);
	else ret = raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
				SERVER_PORT, MAC_BCAST_ADDR, client_config.ifindex);
	return ret;
}	


/* Unicasts a DHCP release message */
int send_release(unsigned long server, unsigned long ciaddr)
{
	struct dhcpMessage packet;

	init_packet(&packet, DHCPRELEASE);
	packet.xid = random_xid();
	packet.ciaddr = ciaddr;
	
	/* expects host order */
	add_simple_option(packet.options, DHCP_REQUESTED_IP, ntohl(ciaddr));
	add_simple_option(packet.options, DHCP_SERVER_ID, ntohl(server));

	LOG(LOG_DEBUG, "Sending release...");
	return kernel_packet(&packet, ciaddr, CLIENT_PORT, server, SERVER_PORT);
}


int get_raw_packet(struct dhcpMessage *payload, int fd)
{
	int bytes;
	struct udp_dhcp_packet packet;
	u_int32_t source, dest;
	u_int16_t check;

	memset(&packet, 0, sizeof(struct udp_dhcp_packet));
	bytes = read(fd, &packet, sizeof(struct udp_dhcp_packet));
	if (bytes < 0) {
		DEBUG(LOG_INFO, "couldn't read on raw listening socket -- ignoring");
		usleep(500000); /* possible down interface, looping condition */
		return -1;
	}
	
	if (bytes < (int) (sizeof(struct iphdr) + sizeof(struct udphdr))) {
		DEBUG(LOG_INFO, "message too short, ignoring");
		return -1;
	}

   /*
    * The minimum ethernet frame for 1000Base-X is 416 bytes,
    * and the miminum ethernet frame for 1000Base-T is 520 bytes,
    * so these small DHCP packets may get padded.
    * Change the size checking below to just ensure byte count
    * is at least as long as the length fields in the ip and
    * udp headers.  But don't expect exact length match anymore.  --mwang
    */
    
	/* Make sure its the right packet for us, and that it passes sanity checks */
	if (packet.ip.protocol != IPPROTO_UDP || packet.ip.version != IPVERSION ||
	    packet.ip.ihl != sizeof(packet.ip) >> 2 || packet.udp.dest != htons(CLIENT_PORT) ||
	    ntohs(packet.ip.tot_len) > bytes ||
	    ntohs(packet.udp.len) > (short) (bytes - sizeof(packet.ip)) ||
       (sizeof(struct udp_dhcp_packet) - 308) > (unsigned int) bytes) {  

	    	DEBUG(LOG_INFO, "unrelated/bogus packet");
	    	return -1;
	}

	/* check IP checksum */
	check = packet.ip.check;
	packet.ip.check = 0;
	if (check != checksum(&(packet.ip), sizeof(packet.ip))) {
		DEBUG(LOG_INFO, "bad IP header checksum, ignoring");
		return -1;
	}
	
	/* verify the UDP checksum by replacing the header with a psuedo header */
	source = packet.ip.saddr;
	dest = packet.ip.daddr;
	check = packet.udp.check;
	packet.udp.check = 0;
	memset(&packet.ip, 0, sizeof(packet.ip));

	packet.ip.protocol = IPPROTO_UDP;
	packet.ip.saddr = source;
	packet.ip.daddr = dest;
	packet.ip.tot_len = packet.udp.len; /* cheat on the psuedo-header */
	// brcm
	/*
	if (check != checksum(&packet, bytes)) {
		DEBUG(LOG_ERR, "packet with bad UDP checksum received, ignoring");
		// cwu
		//return -1;
	}
	*/
	
	memcpy(payload, &(packet.data), bytes - (sizeof(packet.ip) + sizeof(packet.udp)));
	
	if (ntohl(payload->cookie) != DHCP_MAGIC) {
		LOG(LOG_ERR, "received bogus message (bad magic) -- ignoring");
		return -1;
	}
	DEBUG(LOG_INFO, "oooooh!!! got some!");
	return bytes - (sizeof(packet.ip) + sizeof(packet.udp));
}
