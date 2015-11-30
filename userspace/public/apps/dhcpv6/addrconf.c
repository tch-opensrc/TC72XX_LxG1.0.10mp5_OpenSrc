/*	$KAME: addrconf.c,v 1.8 2005/09/16 11:30:13 suz Exp $	*/

/*
 * Copyright (C) 2002 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/ioctl.h>

#include <net/if.h>
#ifdef __FreeBSD__
#include <net/if_var.h>
#endif

#include <netinet/in.h>

#ifdef __KAME__
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#endif

#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dhcp6.h"
#include "config.h"
#include "common.h"
#include "timer.h"
#include "dhcp6c_ia.h"
#include "prefixconf.h"
#if 1 //brcm
#include "cms_msg.h"
extern void *msgHandle;
extern Dhcp6cStateChangedMsgBody dhcp6cMsgBody;
#endif

TAILQ_HEAD(statefuladdr_list, statefuladdr);
struct iactl_na {
	struct iactl common;
	struct statefuladdr_list statefuladdr_head;
};
#define iacna_ia common.iactl_ia
#define iacna_callback common.callback
#define iacna_isvalid common.isvalid
#define iacna_duration common.duration
#define iacna_renew_data common.renew_data
#define iacna_rebind_data common.rebind_data
#define iacna_reestablish_data common.reestablish_data
#define iacna_release_data common.release_data
#define iacna_cleanup common.cleanup

struct statefuladdr {
	TAILQ_ENTRY (statefuladdr) link;

	struct dhcp6_statefuladdr addr;
	time_t updatetime;
	struct dhcp6_timer *timer;
	struct iactl_na *ctl;
	struct dhcp6_if *dhcpif;
};

static struct statefuladdr *find_addr __P((struct statefuladdr_list *,
    struct dhcp6_statefuladdr *));
static void remove_addr __P((struct statefuladdr *));
static int isvalid_addr __P((struct iactl *));
static u_int32_t duration_addr __P((struct iactl *));
static void cleanup_addr __P((struct iactl *));
static int renew_addr __P((struct iactl *, struct dhcp6_ia *,
    struct dhcp6_eventdata **, struct dhcp6_eventdata *));
static void na_renew_data_free __P((struct dhcp6_eventdata *));

static struct dhcp6_timer *addr_timo __P((void *));

static int na_ifaddrconf __P((ifaddrconf_cmd_t, struct statefuladdr *));

extern struct dhcp6_timer *client6_timo __P((void *));

#if 1 //brcm
static int updateIp6AddrFile __P((ifaddrconf_cmd_t, char *, struct sockaddr_in6 *));
static void sendAddrEventMessage __P((ifaddrconf_cmd_t, const char *, const char *));
#endif

int
update_address(ia, addr, dhcpifp, ctlp, callback)
	struct ia *ia;
	struct dhcp6_statefuladdr *addr;
	struct dhcp6_if *dhcpifp;
	struct iactl **ctlp;
	void (*callback)__P((struct ia *));
{
	struct iactl_na *iac_na = (struct iactl_na *)*ctlp;
	struct statefuladdr *sa;
	int sacreate = 0;
	struct timeval timo;

	/*
	 * A client discards any addresses for which the preferred
         * lifetime is greater than the valid lifetime.
	 * [RFC3315 22.6] 
	 */
	if (addr->vltime != DHCP6_DURATION_INFINITE &&
	    (addr->pltime == DHCP6_DURATION_INFINITE ||
	    addr->pltime > addr->vltime)) {
		dprintf(LOG_INFO, FNAME, "invalid address %s: "
		    "pltime (%lu) is larger than vltime (%lu)",
		    in6addr2str(&addr->addr, 0),
		    addr->pltime, addr->vltime);
		return (-1);
	}

	if (iac_na == NULL) {
		if ((iac_na = malloc(sizeof(*iac_na))) == NULL) {
			dprintf(LOG_NOTICE, FNAME, "memory allocation failed");
			return (-1);
		}
		memset(iac_na, 0, sizeof(*iac_na));
		iac_na->iacna_ia = ia;
		iac_na->iacna_callback = callback;
		iac_na->iacna_isvalid = isvalid_addr;
		iac_na->iacna_duration = duration_addr;
		iac_na->iacna_cleanup = cleanup_addr;
		iac_na->iacna_renew_data =
		    iac_na->iacna_rebind_data =
		    iac_na->iacna_release_data =
		    iac_na->iacna_reestablish_data = renew_addr;

		TAILQ_INIT(&iac_na->statefuladdr_head);
		*ctlp = (struct iactl *)iac_na;
	}

	/* search for the given address, and make a new one if it fails */
	if ((sa = find_addr(&iac_na->statefuladdr_head, addr)) == NULL) {
		if ((sa = malloc(sizeof(*sa))) == NULL) {
			dprintf(LOG_NOTICE, FNAME, "memory allocation failed");
			return (-1);
		}
		memset(sa, 0, sizeof(*sa));
		sa->addr.addr = addr->addr;
		sa->ctl = iac_na;
		TAILQ_INSERT_TAIL(&iac_na->statefuladdr_head, sa, link);
		sacreate = 1;
	}

	/* update the timestamp of update */
	sa->updatetime = time(NULL);

	/* update the prefix according to addr */
	sa->addr.pltime = addr->pltime;
	sa->addr.vltime = addr->vltime;
	sa->dhcpif = dhcpifp;
	dprintf(LOG_DEBUG, FNAME, "%s an address %s pltime=%lu, vltime=%lu",
	    sacreate ? "create" : "update",
	    in6addr2str(&addr->addr, 0), addr->pltime, addr->vltime);

	if (sa->addr.vltime != 0)
		na_ifaddrconf(IFADDRCONF_ADD, sa);

	/*
	 * If the new vltime is 0, this address immediately expires.
	 * Otherwise, set up or update the associated timer.
	 */
	switch (sa->addr.vltime) {
	case 0:
		remove_addr(sa);
		break;
	case DHCP6_DURATION_INFINITE:
		if (sa->timer)
			dhcp6_remove_timer(&sa->timer);
		break;
	default:
		if (sa->timer == NULL) {
			sa->timer = dhcp6_add_timer(addr_timo, sa);
			if (sa->timer == NULL) {
				dprintf(LOG_NOTICE, FNAME,
				    "failed to add stateful addr timer");
				remove_addr(sa); /* XXX */
				return (-1);
			}
		}
		/* update the timer */
		timo.tv_sec = sa->addr.vltime;
		timo.tv_usec = 0;

		dhcp6_set_timer(&timo, sa->timer);
		break;
	}

	return (0);
}

static struct statefuladdr *
find_addr(head, addr)
	struct statefuladdr_list *head;
	struct dhcp6_statefuladdr *addr;
{
	struct statefuladdr *sa;

	for (sa = TAILQ_FIRST(head); sa; sa = TAILQ_NEXT(sa, link)) {
		if (!IN6_ARE_ADDR_EQUAL(&sa->addr.addr, &addr->addr))
			continue;
		return (sa);
	}

	return (NULL);
}

static void
remove_addr(sa)
	struct statefuladdr *sa;
{
	dprintf(LOG_DEBUG, FNAME, "remove an address %s",
	    in6addr2str(&sa->addr.addr, 0));

	if (sa->timer)
		dhcp6_remove_timer(&sa->timer);

	TAILQ_REMOVE(&sa->ctl->statefuladdr_head, sa, link);
	na_ifaddrconf(IFADDRCONF_REMOVE, sa);
	free(sa);
}

static int
isvalid_addr(iac)
	struct iactl *iac;
{
	struct iactl_na *iac_na = (struct iactl_na *)iac;

	if (TAILQ_EMPTY(&iac_na->statefuladdr_head))
		return (0);	/* this IA is invalid */
	return (1);
}

static u_int32_t
duration_addr(iac)
	struct iactl *iac;
{
	struct iactl_na *iac_na = (struct iactl_na *)iac;
	struct statefuladdr *sa;
	u_int32_t base = DHCP6_DURATION_INFINITE, pltime, passed;
	time_t now;

	/* Determine the smallest period until pltime expires. */
	now = time(NULL);
	for (sa = TAILQ_FIRST(&iac_na->statefuladdr_head); sa;
	    sa = TAILQ_NEXT(sa, link)) {
		passed = now > sa->updatetime ?
		    (u_int32_t)(now - sa->updatetime) : 0;
		pltime = sa->addr.pltime > passed ?
		    sa->addr.pltime - passed : 0;

		if (base == DHCP6_DURATION_INFINITE || pltime < base)
			base = pltime;
	}

	return (base);
}

static void
cleanup_addr(iac)
	struct iactl *iac;
{
	struct iactl_na *iac_na = (struct iactl_na *)iac;
	struct statefuladdr *sa;

	while ((sa = TAILQ_FIRST(&iac_na->statefuladdr_head)) != NULL) {
		TAILQ_REMOVE(&iac_na->statefuladdr_head, sa, link);
		remove_addr(sa);
	}

	free(iac);
}

static int
renew_addr(iac, iaparam, evdp, evd)
	struct iactl *iac;
	struct dhcp6_ia *iaparam;
	struct dhcp6_eventdata **evdp, *evd;
{
	struct iactl_na *iac_na = (struct iactl_na *)iac;
	struct statefuladdr *sa;
	struct dhcp6_list *ial = NULL, pl;

	TAILQ_INIT(&pl);
	for (sa = TAILQ_FIRST(&iac_na->statefuladdr_head); sa;
	    sa = TAILQ_NEXT(sa, link)) {
		if (dhcp6_add_listval(&pl, DHCP6_LISTVAL_STATEFULADDR6,
		    &sa->addr, NULL) == NULL)
			goto fail;
	}

	if ((ial = malloc(sizeof(*ial))) == NULL)
		goto fail;
	TAILQ_INIT(ial);
	if (dhcp6_add_listval(ial, DHCP6_LISTVAL_IANA, iaparam, &pl) == NULL)
		goto fail;
	dhcp6_clear_list(&pl);

	evd->type = DHCP6_EVDATA_IANA;
	evd->data = (void *)ial;
	evd->privdata = (void *)evdp;
	evd->destructor = na_renew_data_free;

	return (0);

  fail:
	dhcp6_clear_list(&pl);
	if (ial)
		free(ial);
	return (-1);
}

static void
na_renew_data_free(evd)
	struct dhcp6_eventdata *evd;
{
	struct dhcp6_list *ial;

	if (evd->type != DHCP6_EVDATA_IANA) {
		dprintf(LOG_ERR, FNAME, "assumption failure");
		exit(1);
	}

	if (evd->privdata)
		*(struct dhcp6_eventdata **)evd->privdata = NULL;
	ial = (struct dhcp6_list *)evd->data;
	dhcp6_clear_list(ial);
	free(ial);
}

static struct dhcp6_timer *
addr_timo(arg)
	void *arg;
{
	struct statefuladdr *sa = (struct statefuladdr *)arg;
	struct ia *ia;
	void (*callback)__P((struct ia *));

	dprintf(LOG_DEBUG, FNAME, "address timeout for %s",
	    in6addr2str(&sa->addr.addr, 0));

	ia = sa->ctl->iacna_ia;
	callback = sa->ctl->iacna_callback;

	if (sa->timer)
		dhcp6_remove_timer(&sa->timer);

	remove_addr(sa);

	(*callback)(ia);

	return (NULL);
}

static int
na_ifaddrconf(cmd, sa)
	ifaddrconf_cmd_t cmd;
	struct statefuladdr *sa;
{
	struct dhcp6_statefuladdr *addr;
	struct sockaddr_in6 sin6;

	addr = &sa->addr;
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
#ifndef __linux__
	sin6.sin6_len = sizeof(sin6);
#endif
	sin6.sin6_addr = addr->addr;

#if 1 //brcm
	return (ifaddrconf(cmd, sa->dhcpif->ifname, &sin6, 64,
	    addr->pltime, addr->vltime));
#else
	return (ifaddrconf(cmd, sa->dhcpif->ifname, &sin6, 128,
	    addr->pltime, addr->vltime));
#endif
}

#if 1 //brcm: moved from common.c */
int
ifaddrconf(cmd, ifname, addr, plen, pltime, vltime)
	ifaddrconf_cmd_t cmd;
	char *ifname;
	struct sockaddr_in6 *addr;
	int plen;
	int pltime;
	int vltime;
{
#ifdef __KAME__
	struct in6_aliasreq req;
#endif
#ifdef __linux__
	struct in6_ifreq req;
	struct ifreq ifr;
#endif
	unsigned long ioctl_cmd;
	char *cmdstr;
	int s;			/* XXX overhead */

	switch(cmd) {
	case IFADDRCONF_ADD:
		cmdstr = "add";
#ifdef __KAME__
		ioctl_cmd = SIOCAIFADDR_IN6;
#endif
#ifdef __linux__
		ioctl_cmd = SIOCSIFADDR;
#endif
		break;
	case IFADDRCONF_REMOVE:
		cmdstr = "remove";
#ifdef __KAME__
		ioctl_cmd = SIOCDIFADDR_IN6;
#endif
#ifdef __linux__
		ioctl_cmd = SIOCDIFADDR;
#endif
		break;
	default:
		return (-1);
	}

	if ((s = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		dprintf(LOG_ERR, FNAME, "can't open a temporary socket: %s",
		    strerror(errno));
		return (-1);
	}

	memset(&req, 0, sizeof(req));
#ifdef __KAME__
	req.ifra_addr = *addr;
	memcpy(req.ifra_name, ifname, sizeof(req.ifra_name));
	(void)sa6_plen2mask(&req.ifra_prefixmask, plen);
	/* XXX: should lifetimes be calculated based on the lease duration? */
	req.ifra_lifetime.ia6t_vltime = vltime;
	req.ifra_lifetime.ia6t_pltime = pltime;
#endif
#ifdef __linux__
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
	if (ioctl(s, SIOGIFINDEX, &ifr) < 0) {
		dprintf(LOG_NOTICE, FNAME, "failed to get the index of %s: %s",
		    ifname, strerror(errno));
		close(s); 
		return (-1); 
	}
	memcpy(&req.ifr6_addr, &addr->sin6_addr, sizeof(struct in6_addr));
	req.ifr6_prefixlen = plen;
	req.ifr6_ifindex = ifr.ifr_ifindex;
#endif

	if (ioctl(s, ioctl_cmd, &req)) {
		dprintf(LOG_NOTICE, FNAME, "failed to %s an address on %s: %s",
		    cmdstr, ifname, strerror(errno));
		close(s);
		return (-1);
	}

	dprintf(LOG_DEBUG, FNAME, "%s an address %s/%d on %s", cmdstr,
	    addr2str((struct sockaddr *)addr), plen, ifname);

	close(s);

#if 1 //brcm
   if (updateIp6AddrFile(cmd, ifname, addr) == 0)
   {
      /* send SIGUR1 to httpd process only when non-br interface address changes. */
//      if (strncmp(ifname, "br", 2) != 0)
//      {
         char extAddr[BUFLEN_48];
#if 0
         const char *f_httpdPid = "/var/run/httpd_pid";
         FILE  *fp;

         /* send SIGUR1 to httpd process */
         if ((fp = fopen(f_httpdPid, "r")) != NULL)
         {
            char pid[16];

            fgets(pid, sizeof(pid), fp);
            fclose(fp);
            dprintf(LOG_INFO, FNAME, "send SIGUR1 to httpd process %s", pid);
            kill(atoi(pid), SIGUSR1);
         }
#endif
         snprintf(extAddr, sizeof(extAddr), "%s/%d", addr2str((struct sockaddr *)addr), plen);
         sendAddrEventMessage(cmd, ifname, extAddr);
//      }
   }
#endif

	return (0);
}
#endif

#if 1 //brcm
int updateIp6AddrFile(ifaddrconf_cmd_t cmd, char *ifname, struct sockaddr_in6 *addr)
{
   const char *f_ip6Addr = "/var/ip6addr";
   FILE *fp;
   long curLine;
   long ifaddrLen;
   char ifaddr[84];
   char line[84];
   
   /* see if the f_ip6Addr file already exists */
   if (access(f_ip6Addr, F_OK) == 0)
   {
      /* open the existing file for update */
      if ((fp = fopen(f_ip6Addr, "r+")) == NULL)
      {
         /* error */
         dprintf(LOG_ERR, FNAME, "failed to open %s", f_ip6Addr);
         return -1;
      }
   }
   else
   {
      /* open a new file */
      if ((fp = fopen(f_ip6Addr, "w+")) == NULL)
      {
         /* error */
         dprintf(LOG_ERR, FNAME, "failed to open %s", f_ip6Addr);
         return -1;
      }
   }
   fseek(fp, 0L, SEEK_SET);

   sprintf(ifaddr, "%s %s", ifname, addr2str((struct sockaddr *)addr));
   ifaddrLen = strlen(ifaddr);

   if (cmd == IFADDRCONF_ADD)
   {
      /* write interface name and address to a blank line or to EOF */
      curLine = ftell(fp);
      while (fgets(line, sizeof(line), fp) != NULL)
      {
         if (strchr(line, ':') == NULL)
         {
            /* write over the blank line */
            fseek(fp, curLine, SEEK_SET);
            break;
         }
         curLine = ftell(fp);
      }
      fprintf(fp, "%-80s\n", ifaddr); /* Note: each line must be 80 chars long */
      fclose(fp);
      return 0;
   }
   else if (cmd == IFADDRCONF_REMOVE)
   {
      curLine = ftell(fp);
      while (fgets(line, sizeof(line), fp) != NULL)
      {
         if (strncmp(line, ifaddr, ifaddrLen) == 0)
         {
            /* erase the line */
            fseek(fp, curLine, SEEK_SET);
            fprintf(fp, "%-80s\n", " ");  /* Note: each line must be 80 chars long */
            fclose(fp);
            return 0;
         }
         curLine = ftell(fp);
      }
   }

   fclose(fp);
   return -1;

}  /* End of updateIp6AddrFile() */

void sendAddrEventMessage(ifaddrconf_cmd_t cmd, const char *ifname, const char *addr)
{
   /* TODO: Currently, we always assume br0 as the  PD interface */
   if ( strncmp(ifname, "br0", sizeof(ifname)) == 0 )  /* address of the PD interface */
   {
      snprintf(dhcp6cMsgBody.pdIfAddress, sizeof(dhcp6cMsgBody.pdIfAddress), "%s", addr);
   }
   else  /* address of the WAN interface */
   {
      dhcp6cMsgBody.addrAssigned = TRUE;
      dhcp6cMsgBody.addrCmd      = cmd;
      snprintf(dhcp6cMsgBody.ifname, sizeof(dhcp6cMsgBody.ifname), "%s", ifname);
      snprintf(dhcp6cMsgBody.address, sizeof(dhcp6cMsgBody.address), "%s", addr);
   }

   dprintf(LOG_NOTICE, FNAME, "DHCP6C_ADDR_CHANGED");

   return;

}  /* End of sendAddrEventMessage() */

#endif
