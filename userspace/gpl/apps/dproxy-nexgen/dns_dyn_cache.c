#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "dproxy.h"
#include "conf.h"
#include "dns_decode.h"
#include "dns_construct.h"
#include "dns_io.h"
#include "dns_dyn_cache.h"

#ifdef DNS_DYN_CACHE
dns_dyn_list_t *dns_dyn_hosts;
//dns_dyn_list_t *dns_dyn_leases;
dns_dyn_list_t *dns_dyn_cache;
int dns_dyn_cache_count;

/*****************************************************************************/
dns_dyn_list_t *dns_dyn_list_find(dns_dyn_list_t *list, char *cname,
				  struct in_addr *addr)
{
  dns_dyn_list_t *ent;

  for(ent = list; ent; ent = ent->next) {
    if (cname && strcasecmp(ent->cname, cname))
      continue;
    if (addr && addr->s_addr != ent->addr.s_addr)
      continue;
    return ent;
  }

  return NULL;
}

/*****************************************************************************/
dns_dyn_list_t *dns_dyn_find(dns_request_t *m)
{
  char *cname = NULL;
  struct in_addr addr, *paddr = NULL;
  dns_dyn_list_t *ret = NULL;

  if(m->message.question[0].type == A) {
    cname = m->cname;
  } else if(m->message.question[0].type == PTR) {
    inet_aton(m->ip, &addr);
    paddr = &addr;
  }

  /* Search order: hosts, cache */

  while((ret = dns_dyn_list_find(dns_dyn_hosts, cname, paddr))) {
    if (ret->expiry > time(NULL))
      goto found;
    dns_dyn_list_remove(&dns_dyn_hosts, ret);
  }

  while((ret = dns_dyn_list_find(dns_dyn_cache, cname, paddr))) {
    if (ret->expiry > time(NULL))
      goto found;
    dns_dyn_list_remove(&dns_dyn_cache, ret);
  }

found:
  return ret;
}

/*****************************************************************************/
int dns_dyn_list_add(dns_dyn_list_t **list, char *cname, char *ip, int expiry)
{
  dns_dyn_list_t *new;
  struct in_addr addr;

  debug("dyn_list_add: entered for cname=%s\n", cname);

  /* Convert IP address */
  if(!inet_aton(ip, &addr)) {
    debug("invalid IP address %s", ip);
    return 0;
  }
  
  /* Already exist? */
  if (dns_dyn_list_find(*list, cname, &addr)) {
    debug("%s already in list\n", cname);
    return 0;
  }

  if ((new = malloc(sizeof(dns_dyn_list_t) + strlen(cname) + 1)) == NULL) {
    debug("malloc() failed");
    return 0;
  }

  new->prev = NULL;
  new->next = *list;
  new->expiry = expiry;
  new->addr = addr;
  strcpy(new->cname, cname);
  if (*list)
    (*list)->prev = new;
  *list = new;

  debug("dyn_list_add: new entry at %p, prev=%p next=%p\n", new, new->prev, new->next);

  return 1;
}

/*****************************************************************************/
int dns_dyn_cache_add(dns_request_t *m)
{
  int ret;

  debug("dyn_cache_add: entered with cache_count=%d dns_dyn_cache=%p",
        dns_dyn_cache_count, dns_dyn_cache);

  /* remove some entries if max limit on entries reached */
  if(dns_dyn_cache_count >= DNS_DYN_CACHE_MAX_ENTRIES) {
    if ((ret = dns_dyn_cache_timeout()) == 0) {
      /* Force to remove one */
      ret = dns_dyn_list_remove(&dns_dyn_cache, dns_dyn_cache);
    }
    dns_dyn_cache_count -= ret;
  }

  ret = dns_dyn_list_add(&dns_dyn_cache, m->cname, m->ip, m->ttl + time(NULL));
  dns_dyn_cache_count += ret;

  debug("dyn_cache_add: returning %d (cache_count=%d dns_dyn_cache=%p)\n",
        ret, dns_dyn_cache_count, dns_dyn_cache);
  return ret;
}

/*****************************************************************************/
int dns_dyn_hosts_add(void)
{
  FILE *fp;
  char line[BUF_SIZE];
  char *ip, *name;
  int count = 0;

  debug("dns_dyn_hosts_add()");

  /* Eliminate all hosts entries */
  dns_dyn_list_purge(&dns_dyn_hosts);

  if (!(fp = fopen(config.hosts_file, "r"))) {
    debug("can not open file %s", config.hosts_file);
    return 0;
  }

  while(fgets(line, BUF_SIZE, fp)){
    line[strlen(line) - 1] = 0; /* get rid of '\n' */
    ip = strtok( line, " \t");
    if(ip == NULL) /* ignore blank lines */
      continue;
    if(ip[0] == '#') /* ignore comments */
      continue;
    while((name = strtok( NULL, " \t" ))){
      if(name[0] == '#')
	break;
      count += dns_dyn_list_add(&dns_dyn_hosts, name, ip, -1);
    }
  }
  fclose(fp);

  debug("added %d entries from hosts file", count);
  return count;
}

#if 0
struct lease_t {
   unsigned char chaddr[16];
   u_int32_t yiaddr;
   u_int32_t expires;
   char hostname[64];
};

#define ETHER_ISNULLADDR(ea) ((((char *)(ea))[0] |     \
                ((char *)(ea))[1] |        \
                ((char *)(ea))[2] |        \
                ((char *)(ea))[3] |        \
                ((char *)(ea))[4] |        \
                ((char *)(ea))[5]) == 0)

/*****************************************************************************/
int dns_dyn_dhcp_leases_add(void)
{
  FILE *fp;
  struct lease_t lease;
  struct in_addr addr;
  char cname[BUF_SIZE];
  char ip[16];
  int count = 0;

  debug("cache_add_dhcp_entreies()\n");

  /* Eliminate all dhcp leases entries */
  dns_dyn_list_purge(&dns_dyn_dhcp_leases);

  /* If dhcpd is not running, we won't read it's leases file */
  ret = system("killall -0 dhcpd 2>/dev/null");
  if (ret != -1) 
    ret = WEXITSTATUS(ret);
  if (ret != 0) { /* dhcpd is not running */
      debug("dhcpd is not running");
      return;
  }
  
  /* Signal dhcpd to flush leases to file */
  system("killall -SIGUSR1 dhcpd");

  if (!(fp = fopen(config.dhcp_lease_file , "r"))) {
    debug_perror("can not open dhcp leases file");
    return;
  }

  while(fread(&lease, sizeof(lease), 1, fp)){
    /* Do not display reserved leases */
    if(ETHER_ISNULLADDR(lease.chaddr))
      continue;
    addr.s_addr = lease.yiaddr;
    strcpy(ip, inet_ntoa(addr));
    strcpy(cname, lease.hostname);
    /* Add entry of hostname */
    count += dns_dyn_list_add(&dns_dyn_dhcp_leases, cname, ip, -1);
    if (config.domain_name[0]) {
      strcat(cname, ".");
      strcat(cname, config.domain_name);
      /* Add another entry of hostname.domain */
      count += dns_dyn_list_add(&dns_dyn_dhcp_leases, cname, ip, -1);
    }
  }
  fclose(fp);

  debug("added %d dhcp leases entries", count);
  return count;
}
#endif

/*****************************************************************************/
int dns_dyn_list_remove(dns_dyn_list_t **list, dns_dyn_list_t *ent)
{
  if (list == NULL || *list == NULL || ent == NULL) {
     debug_perror("list or ent is NULL\n");
     return 0;
  }

  debug("dyn_list_remove: removing ent=%p from list=%p\n", ent, *list);

  if (*list == ent)
    *list = ent->next;
  else
    ent->prev->next = ent->next;
  if (ent->next)
    ent->next->prev = ent->prev;
  free(ent);

  return 1;
}

/*****************************************************************************/
void dns_dyn_list_purge(dns_dyn_list_t **list)
{
  dns_dyn_list_t *ent = *list;
  dns_dyn_list_t *next;

  while(ent) {
    next = ent->next;
    free(ent);
    ent = next;
  }
  *list = NULL;
}

/*****************************************************************************/
int dns_dyn_cache_timeout(void)
{
  time_t now = time(NULL);
  dns_dyn_list_t *ent = dns_dyn_cache;
  dns_dyn_list_t *tmp;
  int count = 0;

  debug("dyn_cache_timeout: time now=%d head=%p dns_dyn_cache_count=%d", now, dns_dyn_cache, dns_dyn_cache_count);

  while(ent) {
    debug("dyn_cache_timeout: ent=%p ent->prev=%p ent->next=%p expiry=%d\n",
          ent, ent->prev, ent->next, ent->expiry);

    if (ent->expiry <= now) {
      debug("dyn_cache_timeout: ent=%p is expired====\n", ent);
      if (ent == dns_dyn_cache) /* The head */
        dns_dyn_cache = ent->next;
      else
        ent->prev->next = ent->next;
      if (ent->next)
        ent->next->prev = ent->prev;
      tmp = ent;
      ent = ent->next;
      free(tmp);
      count++;
      continue;
    }
    ent = ent->next;
  }

  debug("dyn_cache_timeout: returning %d (dns_dyn_cache=%p)\n", count, dns_dyn_cache);
  return count;
}

/*****************************************************************************/
void dns_dyn_list_print(dns_dyn_list_t *list)
{
  time_t now = time(NULL);
  dns_dyn_list_t *ent;

  for(ent = list; ent; ent = ent->next) {
    debug("    ID: %p Name: %s IP: %s TTL %d\n", ent,
	  ent->cname, inet_ntoa(ent->addr), ent->expiry - now);
  }
}

/*****************************************************************************/
void dns_dyn_print(void)
{
  debug("    ----------");
  debug("    /var/hosts");
  debug("    ----------");
  dns_dyn_list_print(dns_dyn_hosts);

#if 0
  debug("    ----------------");
  debug("    /var/dhcp leases");
  debug("    ----------------");
  dns_dyn_list_print(dns_dyn_dhcp_leases);
#endif

  debug("    -----");
  debug("    cache");
  debug("    -----");
  dns_dyn_list_print(dns_dyn_cache);
}

#endif
