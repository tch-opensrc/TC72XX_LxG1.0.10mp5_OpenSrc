#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifndef DPROXY_H
#define DPROXY_H

#define PORT 53
#define NAME_SIZE 255
#define MAX_PACKET_SIZE 512
#define BUF_SIZE NAME_SIZE

typedef u_int16_t uint16;
typedef u_int32_t uint32;

#include "dns.h"

#ifndef DNS_TIMEOUT 
#define DNS_TIMEOUT 240
#endif
#ifndef NAME_SERVER_DEFAULT
#define NAME_SERVER_DEFAULT "0.0.0.0"
#endif
#ifndef CONFIG_FILE_DEFAULT 
#define CONFIG_FILE_DEFAULT "/etc/dproxy.conf"
#endif
#ifndef DENY_FILE_DEFAULT 
#define DENY_FILE_DEFAULT "/var/cache/dproxy.deny"
#endif
#ifndef CACHE_FILE_DEFAULT 
#define CACHE_FILE_DEFAULT "/var/cache/dproxy.cache"
#endif
#ifndef HOSTS_FILE_DEFAULT 
//BRCM
//#define HOSTS_FILE_DEFAULT "/etc/hosts"
#define HOSTS_FILE_DEFAULT "/var/hosts"
#endif
#ifndef PURGE_TIME_DEFAULT 
#define PURGE_TIME_DEFAULT 48 * 60 * 60
#endif
#ifndef PPP_DEV_DEFAULT 
#define PPP_DEV_DEFAULT "/var/run/ppp0.pid"
#endif
#ifndef DHCP_LEASES_DEFAULT 
//BRCM
//#define DHCP_LEASES_DEFAULT "/var/state/dhcp/dhcpd.leases"
#define DHCP_LEASES_DEFAULT "/var/udhcpd/udhcpd.leases"
#endif
#ifndef PPP_DETECT_DEFAULT 
#define PPP_DETECT_DEFAULT 1
#endif
#ifndef DEBUG_FILE_DEFAULT 
#define DEBUG_FILE_DEFAULT "/var/log/dproxy.debug.log"
#endif

#ifndef DOMAIN_NAME_DEFAULT
#define DOMAIN_NAME_DEFAULT "Home"
#endif

//BRCM
#define STATIC_DNS_FILE_DEFAULT "/var/fyi/sys/dns"

#if 0
#ifdef DPROXY_DEBUG
void debug_perror( char *msg );
void debug(char *fmt, ...);
#else
#define debug_perror(msg) 
#define debug(fmt,arg...)
#endif
#endif
#include "cms.h"
#include "cms_eid.h"
#include "cms_util.h"
#include "cms_msg.h"
void debug_perror( char *msg );
#define debug cmsLog_debug

#define PRIMARY_SERVER      1
#define SECONDARY_SERVER    2
#define PURGE_TIMEOUT       30

#endif
