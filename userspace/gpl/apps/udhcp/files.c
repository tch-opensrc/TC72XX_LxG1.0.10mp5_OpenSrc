/* 
 * files.c -- DHCP server file manipulation *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 */
 
#include <stdio.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <ctype.h>

#include "cms_msg.h"
#include "debug.h"
#include "dhcpd.h"
#include "files.h"
#include "options.h"
#include "leases.h"
#include "static_leases.h"

#define BRCM_RETRY_INTERVAL 1
#define BRCM_RETRY_COUNT    3

#ifdef DHCP_RELAY
static void register_message(CmsMsgType msgType);
#endif

typedef struct netiface {
	char nif_name[32];
	unsigned char nif_mac[6];
	unsigned int nif_index;
	in_addr_t nif_ip;
}netiface;

netiface *get_netifaces(int *count)
{
	netiface * netifaces = NULL;
	struct ifconf ifc;
	char buf[1024];
	int skt;
	int i;

	/* Create socket for querying interfaces */
	if ((skt = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return NULL;

	/* Query available interfaces. */
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if(ioctl(skt, SIOCGIFCONF, &ifc) < 0)
		goto err;

	/* Allocate memory for netiface array */
	if (ifc.ifc_len < 1)
		goto err;
	*count = ifc.ifc_len / sizeof(struct ifreq);
	netifaces = calloc(*count, sizeof(netiface));
	if (netifaces == NULL)
		goto err;

	/* Iterate through the list of interfaces to retrieve info */
	for (i = 0; i < *count; i++) {
		struct ifreq ifr;
		ifr.ifr_addr.sa_family = AF_INET;
		strcpy(ifr.ifr_name, ifc.ifc_req[i].ifr_name);

		/* Interface name */
		strcpy(netifaces[i].nif_name, ifc.ifc_req[i].ifr_name);

		/* Interface index */
		if (ioctl(skt, SIOCGIFINDEX, &ifr))
			goto err;
		netifaces[i].nif_index = ifr.ifr_ifindex;

		/* IPv4 address */
		if (ioctl(skt, SIOCGIFADDR, &ifr))
			goto err;
		netifaces[i].nif_ip = ((struct sockaddr_in*)&ifr.ifr_addr)->
				sin_addr.s_addr;

		/* MAC address */
		if (ioctl(skt, SIOCGIFHWADDR, &ifr))
			goto err;
		memcpy(netifaces[i].nif_mac, ifr.ifr_hwaddr.sa_data, 6);

	}
	close(skt);
	return netifaces;
err:
	close(skt);
	if (netifaces)
		free(netifaces);
	return NULL;
}

/* on these functions, make sure you datatype matches */
static int read_ip(char *line, void *arg)
{
	struct in_addr *addr = arg;
	inet_aton(line, addr);
	return 1;
}

//For static IP lease
static int read_mac(const char *line, void *arg)
{
	uint8_t *mac_bytes = arg;
	struct ether_addr *temp_ether_addr;
	int retval = 1;

	temp_ether_addr = ether_aton(line);

	if(temp_ether_addr == NULL)
		retval = 0;
	else
		memcpy(mac_bytes, temp_ether_addr, 6);

	return retval;
}

static int read_str(char *line, void *arg)
{
	char **dest = arg;
	int i;
	
	if (*dest) free(*dest);

	*dest = strdup(line);
	
	/* elimate trailing whitespace */
	for (i = strlen(*dest) - 1; i > 0 && isspace((*dest)[i]); i--);
	(*dest)[i > 0 ? i + 1 : 0] = '\0';
	return 1;
}

static int read_qstr(char *line, char *arg, int max_len)
{
	char * p = line;
	int quoted = 0;
	int len;
	
	if (*p == '\"') {
		quoted = 1;
		line++;
		p++;
	}
	
	while (*p) {
		if (*p == '\"' && quoted)
			break;
		else if (isspace(*p)) {
			if (!isblank(*p) || !quoted)
				break;
		}
		p++;
	}

	len = p - line;
	if (len >= max_len)
		len = max_len - 1;
	memcpy(arg, line, len);
	arg[len] = 0;
	
	return len;
}

static int read_u32(char *line, void *arg)
{
	u_int32_t *dest = arg;
	*dest = strtoul(line, NULL, 0);
	return 1;
}


static int read_yn(char *line, void *arg)
{
	char *dest = arg;
	if (!strcasecmp("yes", line) || !strcmp("1", line) || !strcasecmp("true", line))
		*dest = 1;
	else if (!strcasecmp("no", line) || !strcmp("0", line) || !strcasecmp("false", line))
		*dest = 0;
	else return 0;
	
	return 1;
}


/* read a dhcp option and add it to opt_list */
static int read_opt(char *line, void *arg)
{
	struct option_set **opt_list = arg;
	char *opt, *val;
	char fail;
	struct dhcp_option *option = NULL;
	int length = 0;
	char buffer[255];
	u_int16_t result_u16;
	int16_t result_s16;
	u_int32_t result_u32;
	int32_t result_s32;
	
	int i;
	
	if (!(opt = strtok(line, " \t="))) return 0;
	
	for (i = 0; options[i].code; i++)
		if (!strcmp(options[i].name, opt)) {
			option = &(options[i]);
			break;
		}
		
	if (!option) return 0;
	
	do {
		val = strtok(NULL, ", \t");
		if (val) {
			fail = 0;
			length = 0;
			switch (option->flags & TYPE_MASK) {
			case OPTION_IP:
				read_ip(val, buffer);
				break;
			case OPTION_IP_PAIR:
				read_ip(val, buffer);
				if ((val = strtok(NULL, ", \t/-")))
					read_ip(val, buffer + 4);
				else fail = 1;
				break;
			case OPTION_STRING:
				length = strlen(val);
				if (length > 254) length = 254;
				memcpy(buffer, val, length);
				break;
			case OPTION_BOOLEAN:
				if (!read_yn(val, buffer)) fail = 1;
				break;
			case OPTION_U8:
				buffer[0] = strtoul(val, NULL, 0);
				break;
			case OPTION_U16:
				result_u16 = htons(strtoul(val, NULL, 0));
				memcpy(buffer, &result_u16, 2);
				break;
			case OPTION_S16:
				result_s16 = htons(strtol(val, NULL, 0));
				memcpy(buffer, &result_s16, 2);
				break;
			case OPTION_U32:
				result_u32 = htonl(strtoul(val, NULL, 0));
				memcpy(buffer, &result_u32, 4);
				break;
			case OPTION_S32:
				result_s32 = htonl(strtol(val, NULL, 0));	
				memcpy(buffer, &result_s32, 4);
				break;
			default:
				break;
			}
			length += option_lengths[option->flags & TYPE_MASK];
			if (!fail)
				attach_option(opt_list, option, buffer, length);
		} else fail = 1;
	} while (!fail && option->flags & OPTION_LIST);
	return 1;
}

//For static IP lease
static int read_staticlease(const char *const_line, void *arg)
{

	char *line;
	char *mac_string;
	char *ip_string;
	uint8_t *mac_bytes;
	uint32_t *ip;


	/* Allocate memory for addresses */
	mac_bytes = xmalloc(sizeof(unsigned char) * 8);
	ip = xmalloc(sizeof(uint32_t));

	/* Read mac */
	line = (char *) const_line;
	mac_string = strtok(line, " \t");
	read_mac(mac_string, mac_bytes);

	/* Read ip */
	ip_string = strtok(NULL, " \t");
	read_ip(ip_string, ip);

	addStaticLease(arg, mac_bytes, ip);

#ifdef UDHCP_DEBUG
	printStaticLeases(arg);
#endif

	return 1;

}

static void release_iface_config(struct iface_config_t * iface)
{
	struct option_set *cur, *next;
	struct static_lease *sl_cur, *sl_next;
	vendor_id_t *vid_cur, *vid_next;

	if (iface->skt >= 0) {
		close(iface->skt);
		iface->skt = -1;
	}
	cur = iface->options;
	while(cur) {
		next = cur->next;
		if(cur->data)
			free(cur->data);
		free(cur);
		cur = next;
	}
	if (iface->interface)
		free(iface->interface);
	if (iface->sname)
		free(iface->sname);
	if (iface->boot_file)
		free(iface->boot_file);
	if (iface->leases)
		free(iface->leases);
	sl_cur = iface->static_leases;
	while(sl_cur) {
		sl_next = sl_cur->next;
		if(sl_cur->mac)
			free(sl_cur->mac);
		if(sl_cur->ip)
			free(sl_cur->ip);
		free(sl_cur);
		sl_cur = sl_next;
	}
	vid_cur = iface->vendor_ids;
	while(vid_cur) {
		vid_next = vid_cur->next;
		free(vid_cur);
		vid_cur = vid_next;
	}
	free(iface);
}

static void set_server_config_defaults(void)
{
	server_config.remaining = 1;
	server_config.auto_time = 7200;
	server_config.decline_time = 3600;
	server_config.conflict_time = 3600;
	server_config.offer_time = 3600;
	server_config.min_lease = 60;
	if (server_config.lease_file)
		free(server_config.lease_file);
	server_config.lease_file = strdup("/etc/udhcpd.leases");
	if (server_config.pidfile)
		free(server_config.pidfile);
	server_config.pidfile = strdup("/var/run/udhcpd.pid");
	if (server_config.notify_file)
		free(server_config.notify_file);
	server_config.notify_file = NULL;
	if (server_config.decline_file)
		free(server_config.decline_file);
	server_config.decline_file = strdup("");
}

static int set_iface_config_defaults(void)
{
	int fd;
	struct ifreq ifr;
	struct sockaddr_in *sin;
	struct option_set *option;
	struct iface_config_t *iface;
	int retry_count;
	int local_rc;
   int foundBr = 0;

	/* Create fd to retrieve interface info */
	if((fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
		LOG(LOG_ERR, "socket failed!");
		return 0;
	}

	for(iface = iface_config; iface; iface = iface->next) {

      if (iface->interface == NULL || strstr(iface->interface, "br") == 0)
         continue;

		/* Initialize socket to invalid */
		iface->skt = -1;
		/* Retrieve IP of the interface */
		/*
		 * BRCM begin: mwang: during startup, dhcpd is started by
		 * rcl_lanHostCfgObject, but br0 has not been created yet
		 * because that is done by rcl_lanIpIntfObject, which is
		 * called after rcl_lanHostCfgObject. So retry a few times
		 * to get br0 info before giving up.
		 */
		local_rc = -1;
		for (retry_count = 0; retry_count < BRCM_RETRY_COUNT;
			retry_count++) {
			ifr.ifr_addr.sa_family = AF_INET;
			strcpy(ifr.ifr_name, iface->interface);
			if ((local_rc = ioctl(fd, SIOCGIFADDR, &ifr)) == 0) {
				sin = (struct sockaddr_in *) &ifr.ifr_addr;
				iface->server = sin->sin_addr.s_addr;
				DEBUG(LOG_INFO, "server_ip(%s) = %s",
				ifr.ifr_name, inet_ntoa(sin->sin_addr));
				break;
			}
			sleep(BRCM_RETRY_INTERVAL);
		}
		if (local_rc < 0) {
			LOG(LOG_ERR, "SIOCGIFADDR failed on %s!",
				ifr.ifr_name);
			return 0;
		}

		/* Set default start and end if missing */
		if (iface->start == 0) {
			iface->start = (iface->server & htonl(0xffffff00)) |
				htonl(20);
		}
		if (iface->end == 0) {
			iface->end = (iface->server & htonl(0xffffff00)) |
				htonl(254);
		}

		/* Retrieve ifindex of the interface */
		if (ioctl(fd, SIOCGIFINDEX, &ifr) == 0) {
			DEBUG(LOG_INFO, "ifindex(%s)  = %d", ifr.ifr_name,
				ifr.ifr_ifindex);
			iface->ifindex = ifr.ifr_ifindex;
		} else {
			LOG(LOG_ERR, "SIOCGIFINDEX failed on %s!",
				ifr.ifr_name);
			return 0;
		}
		/* Retrieve MAC of the interface */
		if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0) {
			memcpy(iface->arp, ifr.ifr_hwaddr.sa_data, 6);
			DEBUG(LOG_INFO, "mac(%s) = "
				"%02x:%02x:%02x:%02x:%02x:%02x", ifr.ifr_name,
				iface->arp[0], iface->arp[1], iface->arp[2], 
				iface->arp[3], iface->arp[4], iface->arp[5]);
		} else {
			LOG(LOG_ERR, "SIOCGIFHWADDR failed on %s!",
				ifr.ifr_name);
			return 0;
		}
		/* set lease time from option or default */
		if ((option = find_option(iface->options, DHCP_LEASE_TIME))) {
			memcpy(&iface->lease, option->data + 2, 4);
			iface->lease = ntohl(iface->lease);
		}
		else
			iface->lease = LEASE_TIME;
		/* Set default max_leases */
		if (iface->max_leases == 0)
			iface->max_leases = 254;
		/* Allocate for leases */
		iface->leases = calloc(1, sizeof(struct dhcpOfferedAddr) *
			iface->max_leases);

      foundBr = 1;
	}
	close(fd);
//	return 1;
   return foundBr;
}

#ifdef DHCP_RELAY
#if 0 // For single interface
static u_int32_t relay_remote;
#endif
void set_relays(void)
{
	int skt;
	int socklen;
	struct sockaddr_in addr;
	struct iface_config_t *iface;
	struct relay_config_t *relay;
	struct relay_config_t *new_relay;

	netiface *nifs = NULL;
	int nif_count;
	int i;

	/* Release all relays */
	cur_relay = relay_config;
	while(cur_relay) {
		relay = cur_relay->next;
		if (cur_relay->skt >= 0)
			close(cur_relay->skt);
		free(cur_relay);
		cur_relay = relay;
	}
	relay_config = cur_relay = NULL;

	/* Reset all relay interface names */
	for (iface = iface_config; iface; iface = iface->next) {
#if 0 // For single interface
		iface->relay_remote = relay_remote;
#endif
		iface->relay_interface[0] = 0;
	}

	/* Get network interface array */
	for (i = 0; i < BRCM_RETRY_COUNT; i++) {
		if ((nifs = get_netifaces(&nif_count)))
			break;
		if (i < BRCM_RETRY_COUNT)
			sleep(BRCM_RETRY_INTERVAL);
	}
	if (nifs == NULL) {
		LOG(LOG_ERR, "failed querying interfaces\n");
		return;
	}

	/* Create UDP for looking up routes */
	if ((skt = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		free(nifs);
		return;
	}

	for (iface = iface_config; iface; iface = iface->next) {
		/* Is this a relay interface? */
		if (iface->decline || iface->relay_remote == 0)
			continue;

		/* Connect UDP socket to relay to find out local IP address */
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = SERVER_PORT;
		addr.sin_addr.s_addr = iface->relay_remote;
		if (connect(skt, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			LOG(LOG_WARNING, "no route to relay %u.%u.%u.%u",
			    ((unsigned char *)&addr.sin_addr.s_addr)[0],
			    ((unsigned char *)&addr.sin_addr.s_addr)[1],
			    ((unsigned char *)&addr.sin_addr.s_addr)[2],
			    ((unsigned char *)&addr.sin_addr.s_addr)[3]);
			continue;
		}
		socklen = sizeof(addr);
		if (getsockname(skt, (struct sockaddr *)&addr, &socklen) < 0)
			continue;

		/* Iterate through the list of interfaces to find the one that
		 * has route to remote DHCP server */
		for (i = 0; i < nif_count; i++) {
			if (nifs[i].nif_ip == addr.sin_addr.s_addr) {
				strcpy(iface->relay_interface,
				       nifs[i].nif_name);
				break;
			}
		}
		if (!iface->relay_interface[0])
			continue;

		/* If the same relay (same relay interface) has been created,
		 * don't do it again */
		for (relay = relay_config; relay; relay = relay->next) {
			if (!strcmp(relay->interface, iface->relay_interface))
				break;
		}
		if (relay)
			continue;

		/* Create new relay entry */
		new_relay = malloc(sizeof(*new_relay));
		new_relay->next = NULL;
		strcpy(new_relay->interface, iface->relay_interface);
		new_relay->skt = -1;

		/* Link new relay */
		if (relay_config) {
			for (relay = relay_config; relay->next;
			     relay = relay->next);
			relay->next = new_relay;
		} else
			relay_config = new_relay;
	}
	close(skt);
	free(nifs);
}
#endif

int read_config(char *file)
{
	FILE *in;
	char buffer[80], *token, *line;
	struct iface_config_t * iface;
#ifdef DHCP_RELAY
	int relayEnabled = 0;
#endif
      
	/* Release all interfaces */
	cur_iface = iface_config;
	while(cur_iface) {
		iface = cur_iface->next;
		release_iface_config(cur_iface);
		cur_iface = iface;
	}
	iface_config = cur_iface = NULL;

	/* Reset server config to defaults */
	set_server_config_defaults();

	/* Allocate the first interface config */
	iface_config = cur_iface = calloc(1, sizeof(struct iface_config_t));

	if (!(in = fopen(file, "r"))) {
		LOG(LOG_ERR, "unable to open config file: %s", file);
		return 0;
	}

#ifdef DHCP_RELAY
#if 0 // For single interface
	relay_remote = 0;
#endif
#endif
	/* Read lines */
	while (fgets(buffer, 80, in)) {
		if (strchr(buffer, '\n')) *(strchr(buffer, '\n')) = '\0';
		if (strchr(buffer, '#')) *(strchr(buffer, '#')) = '\0';
		token = buffer + strspn(buffer, " \t");
		if (*token == '\0') continue;
		line = token + strcspn(token, " \t=");
		if (*line == '\0') continue;
		*line = '\0';
		line++;
		line = line + strspn(line, " \t=");
		if (*line == '\0') continue;
		
		if (strcasecmp(token, "interface") == 0) {
			/* Read interface name */
			char * iface_name = NULL;
			read_str(line, &iface_name);
			if (!iface_name)
				continue;
			/* Lookup read interfaces. If this interface already
			 * read, ignore it */
			for (iface = iface_config; iface; iface = iface->next) {
				if (iface->interface &&
				    strcmp(iface->interface, iface_name) == 0) {
				    free(iface_name);
				    iface_name = NULL;
				    break;
				}
			}
			if (iface_name == NULL)
				continue;
			/* Assign the interface name to the first iface */
			if (cur_iface->interface == NULL)
				cur_iface->interface = iface_name;
			/* Finish the current iface, start a new one */
			else {
				iface = calloc(1, sizeof(struct iface_config_t));
				iface->interface = iface_name;
				cur_iface->next = iface;
				cur_iface = iface;
			}
		} else if (strcasecmp(token, "start") == 0)
			read_ip(line, &cur_iface->start);
		else if (strcasecmp(token, "end") == 0)
			read_ip(line, &cur_iface->end);
		else if (strcasecmp(token, "option") == 0 ||
			strcasecmp(token, "opt") == 0)
			read_opt(line, &cur_iface->options);
		else if (strcasecmp(token, "max_leases") == 0)
			read_u32(line, &cur_iface->max_leases);
		else if (strcasecmp(token, "remaining") == 0)
			read_yn(line, &server_config.remaining);
		else if (strcasecmp(token, "auto_time") == 0)
			read_u32(line, &server_config.auto_time);
		else if (strcasecmp(token, "decline_time") == 0)
			read_u32(line, &server_config.decline_time);
		else if (strcasecmp(token, "conflict_time") == 0)
			read_u32(line, &server_config.conflict_time);
		else if (strcasecmp(token, "offer_time") == 0)
			read_u32(line, &server_config.offer_time);
		else if (strcasecmp(token, "min_lease") == 0)
			read_u32(line, &server_config.min_lease);
		else if (strcasecmp(token, "lease_file") == 0)
			read_str(line, &server_config.lease_file);
		else if (strcasecmp(token, "pidfile") == 0)
			read_str(line, &server_config.pidfile);
		else if (strcasecmp(token, "notify_file") == 0)
			read_str(line, &server_config.notify_file);
		else if (strcasecmp(token, "siaddr") == 0)
			read_ip(line, &cur_iface->siaddr);
		else if (strcasecmp(token, "sname") == 0)
			read_str(line, &cur_iface->sname);
		else if (strcasecmp(token, "boot_file") == 0)
			read_str(line, &cur_iface->boot_file);
		else if (strcasecmp(token, "static_lease") == 0)
			read_staticlease(line, &cur_iface->static_leases);
		else if (strcasecmp(token, "vendor_id") == 0) {
			vendor_id_t * new = malloc(sizeof(vendor_id_t));
			new->next = NULL;
			new->len = read_qstr(line, new->id, sizeof(new->id));
			if (new->len > 0) {
				if (cur_iface->vendor_ids == NULL) {
					cur_iface->vendor_ids = new;
				} else {
					vendor_id_t * vid;

					for (vid = cur_iface->vendor_ids;
					     vid->next; vid = vid->next);
					vid->next = new;
				}
			} else
				free(new);
		}
		else if (strcasecmp(token, "decline_file") == 0)
			read_str(line, &server_config.decline_file);
		else if (strcasecmp(token, "decline") == 0)
			cur_iface->decline = 1;
#ifdef DHCP_RELAY
		else if (strcasecmp(token, "relay") == 0) {
			relayEnabled = 1;

#if 0 // For single interface
			read_ip(line, &relay_remote);
#else
			read_ip(line, &cur_iface->relay_remote);
#endif
		}
#endif
		else
			LOG(LOG_WARNING, "unknown keyword '%s'", token);
	}
	fclose(in);

	/* Set default interface name if it's missing */
	if (iface_config->interface == NULL)
		iface_config->interface = strdup("eth0");

	/* Finish interface config automatically */
	if (!set_iface_config_defaults())
		exit_server(1);

#ifdef DHCP_RELAY
	set_relays();
      	if (relayEnabled)
     	{
      		register_message(CMS_MSG_WAN_CONNECTION_UP);
      	}
#endif

	return 1;
}


/* the dummy var is here so this can be a signal handler */
void write_leases(int dummy __attribute__((unused)))
{
	FILE *fp;
	unsigned int i;
	char buf[255];
	time_t curr = time(0);
	unsigned long lease_time;
	struct iface_config_t * iface;
	
	dummy = 0;
	
	if (!(fp = fopen(server_config.lease_file, "w"))) {
		LOG(LOG_ERR, "Unable to open %s for writing", server_config.lease_file);
		return;
	}

	for (iface = iface_config; iface; iface = iface->next) {
		for (i = 0; i < iface->max_leases; i++) {
			if (iface->leases[i].yiaddr != 0) {
				if (server_config.remaining) {
					if (lease_expired(&(iface->leases[i])))
						lease_time = 0;
					else
						lease_time =
							iface->leases[i].expires
							- curr;
				} else
					lease_time = iface->leases[i].expires;
				lease_time = htonl(lease_time);
				fwrite(iface->leases[i].chaddr, 16, 1, fp);
				fwrite(&(iface->leases[i].yiaddr), 4, 1, fp);
				fwrite(&lease_time, 4, 1, fp);
				//BRCM: hostname field is used by dproxy
				fwrite(iface->leases[i].hostname,
				       sizeof(iface->leases[i].hostname),
				       1, fp);
			}
		}
	}
	fclose(fp);
	
	if (server_config.notify_file) {
		sprintf(buf, "%s %s", server_config.notify_file, server_config.lease_file);
		system(buf);
	}
}

struct saved_lease {
	u_int8_t chaddr[16];
	u_int32_t yiaddr;	/* network order */
	u_int32_t expires;	/* host order */
	char hostname[64];
};


void read_leases(char *file)
{
	FILE *fp;
	time_t curr = time(0);
	struct saved_lease lease;
	struct iface_config_t *iface;
	int count = 0;
	
	if (!(fp = fopen(file, "r"))) {
		LOG(LOG_ERR, "Unable to open %s for reading", file);
		return;
	}

	while ((fread(&lease, sizeof lease, 1, fp) == 1)) {
		for (iface = iface_config; iface; iface = iface->next) {
			if (lease.yiaddr >= iface->start &&
				lease.yiaddr <= iface->end &&
				iface->cnt_leases < iface->max_leases) {
				iface->leases[cur_iface->cnt_leases].yiaddr =
					lease.yiaddr;
				iface->leases[cur_iface->cnt_leases].expires =
					ntohl(lease.expires);	
				if (server_config.remaining)
					iface->leases[cur_iface->cnt_leases].
					expires += curr;
				memcpy(iface->leases[cur_iface->cnt_leases].
					chaddr, lease.chaddr,
					sizeof(lease.chaddr));
				memcpy(iface->leases[cur_iface->cnt_leases].
					hostname, lease.hostname,
					sizeof(lease.hostname));
				iface->cnt_leases++;
				count++;
				break;
			}
		}
	}
	
	DEBUG(LOG_INFO, "Read %d leases", count);
	
	fclose(fp);
}
		
// BRCM_begin

void send_lease_info(UBOOL8 isDelete, const struct dhcpOfferedAddr *lease)
{
   char buf[sizeof(CmsMsgHeader) + sizeof(DhcpdHostInfoMsgBody)] = {0};
   CmsMsgHeader *hdr = (CmsMsgHeader *) buf;
   DhcpdHostInfoMsgBody *body = (DhcpdHostInfoMsgBody *) (hdr+1);
   CmsRet ret;
   struct in_addr inaddr;
   UINT32 remaining, now;

   inaddr.s_addr = lease->yiaddr;
   if (lease->expires == 0xffffffff)  /* check if expires == -1 */
   {
      remaining = lease->expires;
   }
   else
   {
      now = time(0);
      if (lease->expires < now)
      {
         remaining = 0;
      }
      else
      {
         remaining = lease->expires - now;
         /*
          * dhcpd is reporting remaining time to ssk, which sticks it into
          * the data model.  The data model expects a SINT32, so make sure
          * our UINT32 remaining does not go above MAX_SINT32.
          */
         if (remaining > MAX_SINT32)
         {
            remaining = MAX_SINT32;
         }
      }
   }

   DEBUG(LOG_INFO, "sending lease info update msg, isDelete=%d, leaseTimeRemaining=%d", isDelete, remaining);
   DEBUG(LOG_INFO, "assigned addr = %s", inet_ntoa(inaddr));

	hdr->type = CMS_MSG_DHCPD_HOST_INFO;
	hdr->src = EID_DHCPD;
	hdr->dst = EID_SSK;
	hdr->flags_event = 1;
   hdr->dataLength = sizeof(DhcpdHostInfoMsgBody);

   body->deleteHost = isDelete;
   body->leaseTimeRemaining = (SINT32) remaining;

   snprintf(body->ifName, sizeof(body->ifName), cur_iface->interface);
   snprintf(body->ipAddr, sizeof(body->ipAddr), inet_ntoa(inaddr));
   snprintf(body->hostName, sizeof(body->hostName), lease->hostname);
   cmsUtl_macNumToStr(lease->chaddr, body->macAddr);

   
   /* does DHCP include the statically assigned addresses?  Or should that be STATIC? */
   snprintf(body->addressSource, sizeof(body->addressSource), MDMVS_DHCP);

   /* is there a way we can tell if we assigned this address to a host on WLAN? */
   snprintf(body->interfaceType, sizeof(body->interfaceType), MDMVS_ETHERNET);

   /* the vendor id is also contained in the lease struct,
    * we could also send that to ssk to put into the host entry. */

   snprintf(body->oui, sizeof(body->oui), lease->oui);
   snprintf(body->serialNum, sizeof(body->serialNum), lease->serialNumber);
   snprintf(body->productClass, sizeof(body->productClass), lease->productClass);

   if ((ret = cmsMsg_send(msgHandle, hdr)) != CMSRET_SUCCESS)
   {
      LOG(LOG_WARNING, "could not send lease info update");
   }
   else
   {
      DEBUG(LOG_INFO, "lease info update sent!");
   }
}


void write_decline(void)
{
	FILE *fp;
	char msg[sizeof(CmsMsgHeader) + sizeof(DHCPDenyVendorID)] = {0};
	CmsMsgHeader *hdr = (CmsMsgHeader*)&msg;
	DHCPDenyVendorID *vid = (DHCPDenyVendorID*)(&msg[sizeof(CmsMsgHeader)]);
	

	/* Write a log to console */
	printf("Denied vendor ID \"%s\", MAC=%02x:%02x:%02x:%02x:%02x:%02x Interface=%s\n",
	       declines->vendorid, declines->chaddr[0], declines->chaddr[1],
	       declines->chaddr[2], declines->chaddr[3], declines->chaddr[4],
	       declines->chaddr[5], cur_iface->interface);
	fflush(stdout);

	if (!(fp = fopen(server_config.decline_file, "w"))) {
		LOG(LOG_ERR, "Unable to open %s for writing", server_config.decline_file);
		return;
	}

	fwrite(declines->chaddr, 16, 1, fp);
	fwrite(declines->vendorid, 64, 1, fp);
	fclose(fp);
	
	/*
	 * Send an event msg to ssk.
	 */
	hdr->type = CMS_MSG_DHCPD_DENY_VENDOR_ID;
	hdr->src = EID_DHCPD;
	hdr->dst = EID_SSK;
	hdr->flags_event = 1;
	hdr->dataLength = sizeof(DHCPDenyVendorID);
	vid->deny_time = time(NULL);
	memcpy(vid->chaddr, declines->chaddr, sizeof(vid->chaddr));
	strncpy(vid->vendor_id, declines->vendorid, sizeof(vid->vendor_id)-1);
	strncpy(vid->ifName, cur_iface->interface, sizeof(vid->ifName)-1);
	cmsMsg_send(msgHandle, hdr);
}

static struct dhcpOfferedAddr *find_expired_lease_by_yiaddr(u_int32_t yiaddr)
{
	struct iface_config_t * iface;

	for (iface = iface_config; iface; iface = iface->next) {
		unsigned int i;
		for (i = 0; i < iface->max_leases; i++) {
			if (iface->leases[i].yiaddr == yiaddr) {
				if (iface->leases[i].expires >
					(unsigned long) time(0))
					return &(iface->leases[i]);
				else
					return NULL;
			}
		}
	}
	return NULL;
}

/* get signal to write viTable to file */
void write_viTable(int dummy)
{
	FILE *fp;
	int count;
	pVI_OPTION_INFO pPtr=NULL;

	if (!(fp = fopen("/var/udhcpd/managable.device", "w+"))) {
		LOG(LOG_ERR, "Unable to open %s for writing", "/var/udhcpd/managable.device");
		return;
	}
	count = viList->count;
	fprintf(fp,"NumberOfDevices %d\n",count);
	if (count > 0) {
	  pPtr = viList->pHead;
	  while (pPtr) {
	    if (find_expired_lease_by_yiaddr(pPtr->ipAddr)) {
	      strcpy(pPtr->oui,"");
	      strcpy(pPtr->serialNumber,"");
	      strcpy(pPtr->productClass,"");
	    }
	    fprintf(fp,"IPaddr %x Enterprise %d OUI %s SerialNumber %s ProductClass %s\n",
		    pPtr->ipAddr,pPtr->enterprise,pPtr->oui,pPtr->serialNumber,
		    pPtr->productClass);
	    pPtr = pPtr->next;
	  }
	}
	fclose(fp);
}

#ifdef DHCP_RELAY
/* Register interested message to smd to receive it later */
static void register_message(CmsMsgType msgType)
{
	CmsMsgHeader msg;
	CmsRet ret;

	memset(&msg, 0, sizeof(msg));
	msg.type = CMS_MSG_REGISTER_EVENT_INTEREST;
	msg.src = EID_DHCPD;
	msg.dst = EID_SMD;
	msg.flags_request = 1;
	msg.wordData = msgType;

	ret = cmsMsg_sendAndGetReply(msgHandle, &msg);
	if (ret != CMSRET_SUCCESS) {
		cmsLog_error("register_message(%d) error (%d)", msgType, ret);
	} else {
		cmsLog_debug("register_message(%d) succeeded", msgType);
	} 

   	return;
}
#endif  /* DHCP_RELAY */

// BRCM_end
