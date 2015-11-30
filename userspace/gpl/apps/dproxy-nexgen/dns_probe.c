#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cms_params.h>

#include "dproxy.h"
#include "conf.h"
#include "dns_probe.h"

extern int dns_wanup;
extern int dns_querysock;
static char primary_ns[CMS_IPADDR_LENGTH];
static char secondary_ns[CMS_IPADDR_LENGTH];
static struct sockaddr_in probe_addr;
#ifdef DNS_PROBE
static time_t probe_next_time;
static int probe_tried;
static time_t probe_timeout;
static uint16_t probe_id;
static char probe_pkt[36] =
	{0x0, 0x0, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	 0x1, 'a', 0xc, 'r', 'o', 'o', 't', '-', 's', 'e', 'r', 'v',
	 'e', 'r', 's', 0x3, 'n', 'e', 't', 0x0, 0x0, 0x1, 0x0, 0x1, };
#else
time_t dns_recover_time;
#endif

/* Load name servers' addresses from /etc/resolv.conf */
static int load_brcm_name_servers(void)
{
	FILE * fp;
	char line[256];
	char addr[CMS_IPADDR_LENGTH];
	char domain[256];

	primary_ns[0] = secondary_ns[0] = 0;

	if((fp = fopen("/etc/resolv.conf", "r")) == NULL) {
		debug("Opend /etc/resolv.conf failed");
		return 0;
	}
	while(fgets(line, sizeof(line), fp)) {
		if (sscanf(line, "nameserver %15s", addr) == 1) {
			if (primary_ns[0] == 0) {
				strncpy(primary_ns, addr, sizeof(primary_ns)-1);
			} else if (secondary_ns[0] == 0) {
				strncpy(secondary_ns, addr, sizeof(secondary_ns)-1);
			} else {
            printf("dnsproxy supports a max of two name servers.  "
                   "Additional name servers are ignored.\n");
			}
		} else if (sscanf(line, "domain %s", domain) == 1) {
			strncpy(config.domain_name, domain, sizeof(config.domain_name)-1);
		}
	}
	fclose(fp);

	debug("dproxy: load nameserver %s %s", primary_ns, secondary_ns);
	debug("dproxy: load domain %s", config.domain_name);

	if ((primary_ns[0] == 0 && secondary_ns[0] == 0) ||
	    (!strcmp(primary_ns, "0.0.0.0") &&
	     !strcmp(secondary_ns, "0.0.0.0")))
		return 0;
	return 1;
}

/* Initialization before probing */
int dns_probe_init(void)
{
	int ret;
	
	debug("dns_probe_init");

	/* Try to read name servers from /var/fyi/sys/dns */
	if ((ret = load_brcm_name_servers())) {
		strcpy(config.name_server, primary_ns);
	} else {
      /*
       * ret == 0 means wan is not up. 
       * BRCM: Use the magic ppp on demand address.
       */
      debug("wan is not up, set name_server to 128.9.0.107");
		strcpy(config.name_server, "128.9.0.107");
   }

	/* Set primary server as the probing address */
	memset(&probe_addr, 0, sizeof(probe_addr));
	probe_addr.sin_family = AF_INET;
	inet_aton(primary_ns, &probe_addr.sin_addr);
	probe_addr.sin_port = ntohs(PORT);

#ifdef DNS_PROBE
	/* Initialize request id */
	srandom(time(NULL));
	probe_id = (uint16_t)random();
	probe_next_time = 0;
	probe_tried = 0;
	probe_timeout = 0;
#endif
	return ret;
}

/* Send, resend probing request, check timeout, and switch name servers */
int dns_probe(void)
{
	int ret = 0;
	time_t now = time(NULL);

	debug("dns_probe: entered, dns_wanup=%d\n", dns_wanup);

	if (!dns_wanup) { /* No need to probe if no WAN interface */
		return ret;
	}

#ifdef DNS_PROBE
	if (probe_tried) { /* Probing */
		if (now >= probe_timeout) { /* Timed out */
			if (probe_tried >= DNS_PROBE_MAX_TRY) {
				/* Probe failed */
				debug("dproxy: probing failed\n");
				if (secondary_ns[0] &&
				    strcmp(config.name_server, secondary_ns)) {
					printf("Primary DNS server Is Down... "
					       "Switching To Secondary DNS "
					       "server \n");
					strcpy(config.name_server,
					       secondary_ns);
				}
				probe_tried = 0;
				probe_next_time = now + DNS_PROBE_INTERVAL;
				ret = DNS_PROBE_INTERVAL;
			} else { /* Retry */
				sendto(dns_querysock, probe_pkt, sizeof(probe_pkt),
				       0, (struct sockaddr*)&probe_addr,
				       sizeof(probe_addr));
				probe_timeout = now + DNS_PROBE_TIMEOUT;
				probe_tried++;
				ret = DNS_PROBE_TIMEOUT;
			}
		} else {
			ret = probe_timeout - now;
		}
	} else if (now >= probe_next_time) { /* Time to probe */
		*((uint16_t*)probe_pkt) = htons(++probe_id);
		sendto(dns_querysock, probe_pkt, sizeof(probe_pkt), 0,
		       (struct sockaddr*)&probe_addr, sizeof(probe_addr));
		probe_tried = 1;
		probe_timeout = now + DNS_PROBE_TIMEOUT;
		ret = DNS_PROBE_TIMEOUT;
	} else {
		ret = probe_next_time - now;
	}
#else
	if (dns_recover_time) { /* Recovering */
		if (now >= dns_recover_time)
			dns_probe_switchback();
		else
			ret = dns_recover_time - now;
	}

#endif

	debug("dns_probe: done, ret=%d\n", ret);

	return ret;
}

#ifndef DNS_PROBE
void dns_probe_set_recover_time(void)
{
   if (dns_recover_time)
      return;

   printf("Primary DNS server (%s) Is Down..."
          "Switching To Secondary DNS server (%s) "
          "for %d seconds.  \n",
          primary_ns, secondary_ns, DNS_RECOVER_TIMEOUT);

   strncpy(config.name_server, secondary_ns, sizeof(config.name_server)-1);

	dns_recover_time = time(NULL) + DNS_RECOVER_TIMEOUT;
}
#endif

/* Switch back to primary server */
void dns_probe_switchback(void)
{
#ifdef DNS_PROBE
	static int first_time = 1;
#else
	dns_recover_time = 0;
#endif

	debug("config.name_server=%s primary_ns=%s", config.name_server, primary_ns);

	if (strcmp(config.name_server, primary_ns) == 0)
		return;

#ifdef DNS_PROBE
	if (first_time)
		first_time = 0;
	else
#endif

   printf("Switching back to primary dns server (%s).\n", primary_ns);

	strncpy(config.name_server, primary_ns, sizeof(config.name_server)-1);
}

/* Activate primary server */
int dns_probe_activate(uint32_t name_server)
{
	debug("name_server=0x%x probe_addr=0x%x\n", name_server, probe_addr.sin_addr.s_addr);

	if (name_server != probe_addr.sin_addr.s_addr)
		return 0;

#ifdef DNS_PROBE
	probe_tried = 0;
	probe_next_time = time(NULL) + DNS_PROBE_INTERVAL;
#endif

	dns_probe_switchback();
	debug("probe_activate: done\n");
	return 1;
}

/* Activate name server if it's the response for probing request */
int dns_probe_response(dns_request_t *m)
{
#ifdef DNS_PROBE
	if (m->message.header.flags.flags & 0x8000 &&
	    m->message.header.id != probe_id)
		return 0;
	return dns_probe_activate(m->src_addr.s_addr);
#else
	return 0;
#endif
}

#ifdef SUPPORT_DEBUG_TOOLS
void dns_probe_print(void)
{
   printf("primary_ns=%s secondary_ns=%s\n", primary_ns, secondary_ns);
#ifndef DNS_PROBE
   printf("dns_recover_time=%d\n", (int) dns_recover_time);
#endif
}
#endif  /* SUPPORT_DEBUG_TOOLS */
