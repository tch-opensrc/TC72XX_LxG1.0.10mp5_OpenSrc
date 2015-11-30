#include <sys/socket.h>
#include <netinet/in.h>

#define DNS_DYN_CACHE_MAX_ENTRIES 100

typedef struct dns_dyn_list_struct
{
  struct dns_dyn_list_struct *prev;
  struct dns_dyn_list_struct *next;
  time_t expiry;
  struct in_addr addr;
  char cname[0];
} dns_dyn_list_t;

/* Entries from /var/hosts */
extern dns_dyn_list_t *dns_dyn_hosts;
/* Entries from dhcpd leases */
extern dns_dyn_list_t *dns_dyn_dhcp_leases;
/* Cahced responses */
extern dns_dyn_list_t *dns_dyn_cache;
extern int dns_dyn_cache_count;

/* Look up a list entry by CNAME and/or IP address in specified list */
dns_dyn_list_t *dns_dyn_list_find(dns_dyn_list_t *list, char *cname,
				  struct in_addr *addr);

/* Look up a list entry by CNAME and/or IP address in all lists. The search
 * order is hosts, DHCP leases, and cache */
dns_dyn_list_t *dns_dyn_find(dns_request_t *m);

/* Add an entry to specified list. -1 of expiry means infinite (for hosts) */
int dns_dyn_list_add(dns_dyn_list_t **list, char *cname, char *ip, int expiry);

/* Add a response to cache list */
int dns_dyn_cache_add(dns_request_t *m);

/* Add entries in /var/hosts to hosts list */
int dns_dyn_hosts_add(void);

/* Add entries in /var/udhcpd/udhcpd.leases to dhcp leases list */ 
//int dns_dyn_dhcp_leases_add(void);

/* Remove an entry from a specified list */
int dns_dyn_list_remove(dns_dyn_list_t **list, dns_dyn_list_t *entry);

/* Purge a list */
void dns_dyn_list_purge(dns_dyn_list_t **list);

/* Remove timeouted entries in the cache list */
int dns_dyn_cache_timeout(void);

/* Print out a list */
void dns_dyn_list_print(dns_dyn_list_t *list);

/* Print out all lists */
void dns_dyn_print(void);

