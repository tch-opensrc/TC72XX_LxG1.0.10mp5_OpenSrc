/*
 * arpping.c
 *
 * Mostly stolen from: dhcpcd - DHCP client daemon
 * by Yoichi Hariguchi <yoichi@fore.com>
 */

#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/if_ether.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "dhcpd.h"
#include "debug.h"
#include "arpping.h"


/* local prototypes */
int arpCheck(u_long inaddr, struct ifinfo *ifbuf, long timeout);
void mkArpMsg(int opcode, u_long tInaddr, u_char *tHaddr, u_long sInaddr, u_char *sHaddr, struct arpMsg *msg);
int openRawSocket (int *s, u_short type);


/* args:	yiaddr - what IP to ping (eg. on the NETtel cb189701)
 * retn: 	1 addr free
 *		0 addr used
 *		-1 error 
 */  
int arpping(u_int32_t yiaddr, u_int32_t ip, char *arp, char *interface) {
	struct ifinfo ifbuf;

	strcpy(ifbuf.ifname, interface);
	ifbuf.addr = ip;
	ifbuf.mask = 0x0;
	ifbuf.bcast = 0x0;
	
	memcpy(ifbuf.haddr, arp, 6);
	ifbuf.flags = 0;
	
	return arpCheck(yiaddr, &ifbuf, 2);
}


int arpCheck(u_long inaddr, struct ifinfo *ifbuf, long timeout)  {
	int				s;			/* socket */
	int				rv;			/* return value */
	struct sockaddr addr;		/* for interface name */
	struct arpMsg	arp;
	fd_set			fdset;
	struct timeval	tm;
	time_t			prevTime;

	rv = 1;
	openRawSocket(&s, ETH_P_ARP);

	/* send arp request */
	mkArpMsg(ARPOP_REQUEST, inaddr, NULL, ifbuf->addr, ifbuf->haddr, &arp);
	bzero(&addr, sizeof(addr));
	strcpy(addr.sa_data, ifbuf->ifname);
	if ( sendto(s, &arp, sizeof(arp), 0, &addr, sizeof(addr)) < 0 ) rv = 0;
	
	/* wait arp reply, and check it */
	tm.tv_usec = 0;
	time(&prevTime);
	while ( timeout > 0 ) {
		FD_ZERO(&fdset);
		FD_SET(s, &fdset);
		tm.tv_sec  = timeout;
		if ( select(s+1, &fdset, (fd_set *)NULL, (fd_set *)NULL, &tm) < 0 ) {
			DEBUG(LOG_ERR, "Error on ARPING request: errno=%d", errno);
			if (errno != EINTR) rv = 0;
		} else if ( FD_ISSET(s, &fdset) ) {
			if (recv(s, &arp, sizeof(arp), 0) < 0 ) rv = 0;
			if(arp.operation == htons(ARPOP_REPLY) && 
			   bcmp(arp.tHaddr, ifbuf->haddr, 6) == 0 && 
			   *((u_int *)arp.sInaddr) == inaddr ) {
				DEBUG(LOG_INFO, "Valid arp reply receved for this address");
				rv = 0;
				break;
			}
		}
		timeout -= time(NULL) - prevTime;
		time(&prevTime);
	}
	close(s);
	DEBUG(LOG_INFO, "%salid arp replies for this address", rv ? "No v" : "V");	 
	return rv;
}

void mkArpMsg(int opcode, u_long tInaddr, u_char *tHaddr,
		 u_long sInaddr, u_char *sHaddr, struct arpMsg *msg) {
	bzero(msg, sizeof(*msg));
	bcopy(MAC_BCAST_ADDR, msg->ethhdr.h_dest, 6); /* MAC DA */
	bcopy(sHaddr, msg->ethhdr.h_source, 6);	/* MAC SA */
	msg->ethhdr.h_proto = htons(ETH_P_ARP);	/* protocol type (Ethernet) */
	msg->htype = htons(ARPHRD_ETHER);		/* hardware type */
	msg->ptype = htons(ETH_P_IP);			/* protocol type (ARP message) */
	msg->hlen = 6;							/* hardware address length */
	msg->plen = 4;							/* protocol address length */
	msg->operation = htons(opcode);			/* ARP op code */
	*((u_int *)msg->sInaddr) = sInaddr;		/* source IP address */
	bcopy(sHaddr, msg->sHaddr, 6);			/* source hardware address */
	*((u_int *)msg->tInaddr) = tInaddr;		/* target IP address */
	if ( opcode == ARPOP_REPLY )
		bcopy(tHaddr, msg->tHaddr, 6);		/* target hardware address */
}


int openRawSocket (int *s, u_short type) {
	int optval = 1;

	if((*s = socket (AF_INET, SOCK_PACKET, htons (type))) == -1) {
		LOG(LOG_ERR, "Could not open raw socket");
		return -1;
	}
	
	if(setsockopt (*s, SOL_SOCKET, SO_BROADCAST, &optval, sizeof (optval)) == -1) {
		LOG(LOG_ERR, "Could not setsocketopt on raw socket");
		return -1;
	}
	return 0;
}

