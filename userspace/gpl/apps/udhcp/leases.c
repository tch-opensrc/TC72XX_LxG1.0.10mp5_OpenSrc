/* 
 * leases.c -- tools to manage DHCP leases 
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 */

#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "debug.h"
#include "dhcpd.h"
#include "files.h"
#include "options.h"
#include "leases.h"
#include "arpping.h"

//For static IP lease
#include "static_leases.h"

/* clear every lease out that chaddr OR yiaddr matches and is nonzero */
void clear_lease(u_int8_t *chaddr, u_int32_t yiaddr)
{
	unsigned int i, blank_chaddr = 0, blank_yiaddr = 0;
	
	for (i = 0; i < 16 && !chaddr[i]; i++);
	if (i == 16) blank_chaddr = 1;
	blank_yiaddr = (yiaddr == 0);
	
	for (i = 0; i < cur_iface->max_leases; i++)
		if ((!blank_chaddr && !memcmp(cur_iface->leases[i].chaddr,
			chaddr, 16)) ||
		    (!blank_yiaddr && cur_iface->leases[i].yiaddr == yiaddr)) {
			memset(&(cur_iface->leases[i]), 0,
				sizeof(struct dhcpOfferedAddr));
		}
}


/* add a lease into the table, clearing out any old ones */
struct dhcpOfferedAddr *add_lease(u_int8_t *chaddr, u_int32_t yiaddr, unsigned long lease)
{
	struct dhcpOfferedAddr *oldest;
	
	/* clean out any old ones */
	clear_lease(chaddr, yiaddr);
		
	oldest = oldest_expired_lease();
	
	if (oldest) {
		memcpy(oldest->chaddr, chaddr, 16);
		oldest->yiaddr = yiaddr;
		oldest->expires = time(0) + lease;
	}
	
	return oldest;
}


/* true if a lease has expired */
int lease_expired(struct dhcpOfferedAddr *lease)
{
	return (lease->expires < (unsigned long) time(0));
}	


/* return the number of seconds left in the lease */
int lease_time_remaining(const struct dhcpOfferedAddr *lease)
{
   unsigned long now = (unsigned long) time(0);

   if (lease->expires > now) {
      return (lease->expires - now);
   }
   else {
      return 0;
   }
}

/* Find the oldest expired lease, NULL if there are no expired leases */
struct dhcpOfferedAddr *oldest_expired_lease(void)
{
	struct dhcpOfferedAddr *oldest = NULL;
	unsigned long oldest_lease = time(0);
	unsigned int i;

	
	for (i = 0; i < cur_iface->max_leases; i++)
		if (oldest_lease > cur_iface->leases[i].expires) {
			oldest_lease = cur_iface->leases[i].expires;
			oldest = &(cur_iface->leases[i]);
		}
	return oldest;
		
}


/* Find the first lease that matches chaddr, NULL if no match */
struct dhcpOfferedAddr *find_lease_by_chaddr(u_int8_t *chaddr)
{
	unsigned int i;

	for (i = 0; i < cur_iface->max_leases; i++)
		if (!memcmp(cur_iface->leases[i].chaddr, chaddr, 16))
			return &(cur_iface->leases[i]);
	
	return NULL;
}


/* Find the first lease that matches yiaddr, NULL is no match */
struct dhcpOfferedAddr *find_lease_by_yiaddr(u_int32_t yiaddr)
{
	unsigned int i;

	for (i = 0; i < cur_iface->max_leases; i++)
		if (cur_iface->leases[i].yiaddr == yiaddr)
			return &(cur_iface->leases[i]);
	
	return NULL;
}


/* find an assignable address, it check_expired is true, we check all the expired leases as well.
 * Maybe this should try expired leases by age... */
u_int32_t find_address(int check_expired) 
{
	u_int32_t addr, ret = 0;
	struct dhcpOfferedAddr *lease = NULL;		

	addr = cur_iface->start;
	// brcm
	for (;ntohl(addr) <= ntohl(cur_iface->end);
		addr = htonl(ntohl(addr) + 1)) {

		/* ie, 192.168.55.0 */
		if (!(ntohl(addr) & 0xFF)) continue;

		/* ie, 192.168.55.255 */
		if ((ntohl(addr) & 0xFF) == 0xFF) continue;

		//For static IP lease
		/* Only do if it isn't an assigned as a static lease */
		if(!reservedIp(cur_iface->static_leases, htonl(addr))) 
		{
		/* lease is not taken */
		   ret = htonl(addr);
		   if ((!(lease = find_lease_by_yiaddr(ret)) ||
		     	/* or it expired and we are checking for expired leases */
		    	 (check_expired  && lease_expired(lease))) &&
		   		/* and it isn't on the network */
	    	     !check_ip(ret)) 
		   {
		      return ret;
		   }
		}
	}
	return 0;
}


/* return a pointer to the iface struct that has the specified interface name, e.g. br0 */
struct iface_config_t *find_iface_by_ifname(const char *name)
{
   struct iface_config_t *iface;

   for (iface = iface_config; iface; iface = iface->next) {
      if (!cmsUtl_strcmp(iface->interface, name))
      {
         return iface;
      }
   }

   return NULL;
}


/* check is an IP is taken, if it is, add it to the lease table */
int check_ip(u_int32_t addr)
{
	char blank_chaddr[] = {[0 ... 15] = 0};
	struct in_addr temp;
	
	if (!arpping(addr, cur_iface->server, cur_iface->arp,
		cur_iface->interface)) {
		temp.s_addr = addr;
	 	LOG(LOG_INFO, "%s belongs to someone, reserving it for %ld seconds", 
	 		inet_ntoa(temp), server_config.conflict_time);
		add_lease(blank_chaddr, addr, server_config.conflict_time);
		return 1;
	} else return 0;
}

