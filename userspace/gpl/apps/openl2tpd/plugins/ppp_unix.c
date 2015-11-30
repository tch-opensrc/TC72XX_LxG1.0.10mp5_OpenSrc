/*****************************************************************************
 * Copyright (C) 2004,2005,2006,2007,2008 Katalix Systems Ltd
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 *
 *****************************************************************************/

/*
 * Plugin to use the standard UNIX pppd
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef aligned_u64
/* should be defined in sys/types.h */
#define aligned_u64 unsigned long long __attribute__((aligned(8)))
#endif
#include <sys/ioctl.h>
#include <linux/if_ether.h>
#include <linux/if_pppox.h>
#include <linux/ppp_defs.h>
#include <linux/if_ppp.h>
// todo: support in toolchains 
//#include <linux/if_pppol2tp.h>

#include "usl.h"
#include "l2tp_private.h"

#ifndef offsetof
#define offsetof(type, field) ((int) ((char *) &((type *) 0)->field))
#endif

#define PPP_UNIX_MAX_PPPD_ARGS				80

/* should be in system's socket.h */
#ifndef SOL_PPPOL2TP
#define SOL_PPPOL2TP	273
#endif

/* One ppp context is used per session. We bump reference counts on
 * the tunnel/session while we hold references to those data
 * structures.
 */
struct ppp_context {
	struct usl_list_head				list;
	uint16_t					tunnel_id;
	uint16_t					session_id;
	uint16_t					peer_tunnel_id;
	uint16_t					peer_session_id;
	char						*interface_name;
	struct l2tp_api_ppp_profile_msg_data		*ppp_profile_data;
	uint16_t					mtu;
	uint16_t					mru;
	int						closed:1;
	int						creating:1;
	int						running:1;
	int						deleting:1;
	pid_t						pid;
	int						trace_flags;
	int						tunnel_fd;
	int						kernel_fd;
	struct l2tp_tunnel const			*tunnel;
	struct l2tp_session const			*session;
	struct usl_list_head				deferred_list;
	struct usl_timer				*setup_timer;
};

static USL_LIST_HEAD(ppp_unix_list);
static USL_LIST_HEAD(ppp_unix_deferred_setup_list);

static int ppp_unix_pppd_spawn(struct ppp_context *ppp);
static int ppp_unix_delete(struct ppp_context *ppp);
static int ppp_unix_pppd_terminate(struct ppp_context *ppp, uint16_t tunnel_id, uint16_t session_id);

/* Used to record previous value of hook so that we can chain them */
static int (*ppp_unix_old_session_created_hook)(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id) = NULL;
static int (*ppp_unix_old_session_deleted_hook)(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id) = NULL;
static int (*ppp_unix_old_session_modified_hook)(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id) = NULL;
static int (*ppp_unix_old_session_up_hook)(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id, uint16_t peer_tunnel_id, uint16_t peer_session_id) = NULL;
static int (*ppp_unix_old_session_down_hook)(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id) = NULL;
static int (*ppp_unix_old_session_get_stats_hook)(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id, struct pppol2tp_ioc_stats *stats) = NULL;
static void (*ppp_unix_old_session_ppp_created_hook)(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id, int unit) = NULL;
static void (*ppp_unix_old_session_ppp_deleted_hook)(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id) = NULL;

/* For testing, we use a dummy pppd which dumps its args to a file for analysis and sleeps. */
#ifdef L2TP_TEST
#define PPP_UNIX_PPPD_DUMMY_FILE "./pppd_dummy"
static int ppp_unix_dummy_pppd = 0;
#endif

static int ppp_unix_active_pppd_count = 0;


static int ppp_unix_use_dummy_pppd(void)
{
#ifdef L2TP_TEST
	return ppp_unix_dummy_pppd;
#else
	return FALSE;
#endif
}

/*****************************************************************************
 * Unix pppd command line builder
 *****************************************************************************/

struct ppp_unix_pppd_arg {
	const char              *string;
	int                     offset;
	int                     data_size;
	uint32_t		flags1;
	uint32_t		flags2;
};

#define L2TP_API_PPP_PROFILE_FLAG_NONE 0

#define PPP_UNIX_PARAM(_string, _field, _flags1, _flags2) \
	{ _string, offsetof(struct l2tp_api_ppp_profile_msg_data, _field), \
	  sizeof(((struct l2tp_api_ppp_profile_msg_data *)0)->_field), \
	  L2TP_API_PPP_PROFILE_FLAG_##_flags1, L2TP_API_PPP_PROFILE_FLAG_##_flags2 }

static const struct ppp_unix_pppd_arg ppp_unix_pppd_args[] = {
	PPP_UNIX_PARAM("asyncmap",		asyncmap,			ASYNCMAP,			NONE),
	PPP_UNIX_PARAM("chap-interval",		chap_interval,			CHAP_INTERVAL,			NONE),
	PPP_UNIX_PARAM("chap-max-challenge", 	chap_max_challenge,		CHAP_MAX_CHALLENGE,		NONE),
	PPP_UNIX_PARAM("chap-restart",		chap_restart,			CHAP_RESTART,			NONE),
	PPP_UNIX_PARAM("pap-max-authreq",	pap_max_auth_requests,		PAP_MAX_AUTH_REQUESTS,		NONE),
	PPP_UNIX_PARAM("pap-restart",		pap_restart_interval,		PAP_RESTART_INTERVAL,		NONE),
	PPP_UNIX_PARAM("pap-timeout",		pap_timeout,			PAP_TIMEOUT,			NONE),
	PPP_UNIX_PARAM("idle",			idle_timeout,			IDLE_TIMEOUT,			NONE),
	PPP_UNIX_PARAM("ipcp-max-configure",	ipcp_max_config_requests,	IPCP_MAX_CONFIG_REQUESTS,	NONE),
	PPP_UNIX_PARAM("ipcp-max-failure",	ipcp_max_config_naks,		IPCP_MAX_CONFIG_NAKS, 		NONE),
	PPP_UNIX_PARAM("ipcp-max-terminate",	ipcp_max_terminate_requests,	IPCP_MAX_TERMINATE_REQUESTS,	NONE),
	PPP_UNIX_PARAM("ipcp-restart",		ipcp_retransmit_interval,	IPCP_RETRANSMIT_INTERVAL,	NONE),
	PPP_UNIX_PARAM("lcp-echo-failure",	lcp_echo_failure_count,		LCP_ECHO_FAILURE_COUNT,		NONE),
	PPP_UNIX_PARAM("lcp-echo-interval",	lcp_echo_interval,		LCP_ECHO_INTERVAL,		NONE),
	PPP_UNIX_PARAM("lcp-max-configure",	lcp_max_config_requests,	LCP_MAX_CONFIG_REQUESTS,	NONE),
	PPP_UNIX_PARAM("lcp-max-failure",	lcp_max_config_naks,		LCP_MAX_CONFIG_NAKS,		NONE),
	PPP_UNIX_PARAM("lcp-max-terminate",	lcp_max_terminate_requests,	LCP_MAX_TERMINATE_REQUESTS,	NONE),
	PPP_UNIX_PARAM("lcp-restart",		lcp_retransmit_interval,	LCP_RETRANSMIT_INTERVAL,	NONE),
	PPP_UNIX_PARAM("maxconnect",		max_connect_time,		MAX_CONNECT_TIME,		NONE),
	PPP_UNIX_PARAM("maxfail",		max_failure_count,		MAX_FAILURE_COUNT,		NONE),
	{ NULL, -1, 0, 0, 0 }
};

/* Convert param structure to argv[]. Return argc.
 */
static int ppp_unix_params_to_argv(struct ppp_context *ppp, struct l2tp_api_ppp_profile_msg_data *params, int argc, char *argv[])
{
	int int_val;
	const struct ppp_unix_pppd_arg *ppp_arg;
	int arg = 0;
	char buf[PPP_UNIX_MAX_PPPD_ARGS];
	struct in_addr ip;
	uint32_t auth_flags;

	argv[arg++] = "pppd";

	/* Do the difficult cases which aren't in the translation table */
	buf[0] = '\0';
	if (params->flags2 & (L2TP_API_PPP_PROFILE_FLAG_LOCAL_IP_ADDR | L2TP_API_PPP_PROFILE_FLAG_PEER_IP_ADDR)) {
		if (params->local_ip_addr.s_addr != 0) {
			ip.s_addr = params->local_ip_addr.s_addr;
			strcat(&buf[0], inet_ntoa(ip));
		}
		strcat(&buf[0], ":");
		if (params->peer_ip_addr.s_addr != 0) {
			ip.s_addr = params->peer_ip_addr.s_addr;
			strcat(&buf[0], inet_ntoa(ip));
		}
		if (strlen(buf) > 0) {
			argv[arg++] = strdup(buf);
		}
	}

	if (params->flags2 & L2TP_API_PPP_PROFILE_FLAG_DNS_ADDR_1) {
		ip.s_addr = params->dns_addr_1.s_addr;
		strcpy(&buf[0], inet_ntoa(ip));
		argv[arg++] = "ms-dns";
		argv[arg++] = strdup(buf);
	}
	if (params->flags2 & L2TP_API_PPP_PROFILE_FLAG_DNS_ADDR_2) {
		ip.s_addr = params->dns_addr_2.s_addr;
		strcpy(&buf[0], inet_ntoa(ip));
		argv[arg++] = "ms-dns";
		argv[arg++] = strdup(buf);
	}
	if (params->flags2 & L2TP_API_PPP_PROFILE_FLAG_WINS_ADDR_1) {
		ip.s_addr = params->wins_addr_1.s_addr;
		strcpy(&buf[0], inet_ntoa(ip));
		argv[arg++] = "ms-wins";
		argv[arg++] = strdup(buf);
	}
	if (params->flags2 & L2TP_API_PPP_PROFILE_FLAG_WINS_ADDR_2) {
		ip.s_addr = params->wins_addr_2.s_addr;
		strcpy(&buf[0], inet_ntoa(ip));
		argv[arg++] = "ms-wins";
		argv[arg++] = strdup(buf);
	}
	if ((params->trace_flags & L2TP_DEBUG_PPP) != 0) {
		argv[arg++] = "debug";
		argv[arg++] = "kdebug";
		sprintf(&buf[0], "%d", 7);
		argv[arg++] = strdup(buf);
	}

	if (params->flags & L2TP_API_PPP_PROFILE_FLAG_MTU) {
		argv[arg++] = "mtu";
		sprintf(&buf[0], "%hu", ppp->mtu);
		argv[arg++] = strdup(buf);
	}
	if (params->flags & L2TP_API_PPP_PROFILE_FLAG_MRU) {
		argv[arg++] = "mru";
		sprintf(&buf[0], "%hu", ppp->mru);
		argv[arg++] = strdup(buf);
	}
	if ((params->flags & L2TP_API_PPP_PROFILE_FLAG_LOCAL_NAME) && (OPTSTRING_PTR(params->local_name) != NULL)) {
		argv[arg++] = "name";
		argv[arg++] = strdup(OPTSTRING(params->local_name));
	}
	if ((params->flags & L2TP_API_PPP_PROFILE_FLAG_REMOTE_NAME) && (OPTSTRING_PTR(params->remote_name) != NULL)) {
		argv[arg++] = "remotename";
		argv[arg++] = strdup(OPTSTRING(params->remote_name));
	}
	if ((params->flags & L2TP_API_PPP_PROFILE_FLAG_USE_RADIUS) && (params->use_radius)) {
		/* ppp-2.4.2 now has a radius client plugin. If a radius_hint was given, 
		 * assume it's a radius config filename 
		 */
		argv[arg++] = "plugin";
		argv[arg++] = "radius.so";
		argv[arg++] = "plugin";
		argv[arg++] = "radattr.so";
		if ((params->flags & L2TP_API_PPP_PROFILE_FLAG_RADIUS_HINT) && (OPTSTRING_PTR(params->radius_hint) != NULL)) {
			argv[arg++] = "radius-config-file";
			argv[arg++] = strdup(OPTSTRING(params->radius_hint));
		}
	}
	if (params->flags2 & L2TP_API_PPP_PROFILE_FLAG_USE_AS_DEFAULT_ROUTE) {
		if (params->use_as_default_route) {
			argv[arg++] = "defaultroute";
		} else {
			argv[arg++] = "nodefaultroute";
		}
	}
	
	/* never use a default IP local address (derived from gethostbyname()) */
	argv[arg++] = "noipdefault";

	/* ippool plugin support */
	if ((params->flags2 & L2TP_API_PPP_PROFILE_FLAG_IP_POOL_NAME) && (OPTSTRING_PTR(params->ip_pool_name) != NULL)) {
		argv[arg++] = "plugin";
		argv[arg++] = "ippool.so";
		argv[arg++] = "ippool_name";
		argv[arg++] = strdup(OPTSTRING(params->ip_pool_name));
		if ((params->trace_flags & L2TP_DEBUG_PPP) != 0) {
			argv[arg++] = "ippool_debug";
			argv[arg++] = "1";
		}
	}

	/* By default, use sync mode */
	if (!((params->flags & L2TP_API_PPP_PROFILE_FLAG_SYNC_MODE) &&
	      (params->sync_mode == L2TP_API_PPP_SYNCMODE_ASYNC))) {
		argv[arg++] = "sync";
	}

	/* ppp auth options */
	if (params->flags2 & L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_EAP) {
		argv[arg++] = "refuse-eap";
	}
	if (params->flags2 & L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_MSCHAPV2) {
		argv[arg++] = "refuse-mschap-v2";
	}
	if (params->flags2 & L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_MSCHAP) {
		argv[arg++] = "refuse-mschap";
	}
	if (params->flags2 & L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_CHAP) {
		argv[arg++] = "refuse-chap";
	}
	if (params->flags2 & L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_PAP) {
		argv[arg++] = "refuse-pap";
	}

	/* Special case ppp auth options. If all but one possible auth
	 * option is disabled, set the pppd require-xxx option for the
	 * auth option that is left enabled. This seems to be needed
	 * to interoperate with some ppp implementations that expect
	 * to do only one auth round, i.e. if their auth type is
	 * rejected by the peer, it doesn't try another.
	 */
	auth_flags = params->flags2 & (L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_EAP |
				       L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_MSCHAPV2 |
				       L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_MSCHAP |
				       L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_CHAP |
				       L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_PAP);
	if (auth_flags == (L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_EAP |
			   L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_MSCHAPV2 |
			   L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_MSCHAP |
			   L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_CHAP)) {
		argv[arg++] = "require-pap";
	} else if (auth_flags == (L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_EAP |
				  L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_MSCHAPV2 |
				  L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_MSCHAP |
				  L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_PAP)) {
		argv[arg++] = "require-chap";
	} else if (auth_flags == (L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_EAP |
				  L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_MSCHAPV2 |
				  L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_CHAP |
				  L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_PAP)) {
		argv[arg++] = "require-mschap";
	} else if (auth_flags == (L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_EAP |
				  L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_MSCHAP |
				  L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_CHAP |
				  L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_PAP)) {
		argv[arg++] = "require-mschap-v2";
	}

	/* Now handle each parameter from our translation table. Only
	 * add the arg if the ppp profile indicates that it's been
	 * set.
	 */
	ppp_arg = &ppp_unix_pppd_args[0];
	while (ppp_arg->string) {
		if ((ppp_arg->flags1 & params->flags) || (ppp_arg->flags2 & params->flags2)) {
			int_val = (*(int *) (((char *) params) + ppp_arg->offset));
			if (int_val) {
				argv[arg++] = (char *) ppp_arg->string;
				sprintf(&buf[0], "%u", int_val);
				argv[arg++] = strdup(buf);
			}
		}
		ppp_arg++;
	}

	if (arg > argc) {
		syslog(LOG_ERR, "Too many pppd arguments to convert");
		return -E2BIG;
	}

	return arg;
}

/*****************************************************************************
 * L2TP socket API
 *****************************************************************************/

static int ppp_unix_socket_connect(int session_fd, int tunnel_fd, struct sockaddr_in const *addr, 
				   uint16_t tunnel_id, uint16_t session_id, uint16_t peer_tunnel_id, uint16_t peer_session_id)
{
	struct sockaddr_pppol2tp sax = { 0, };
	int fd;

	L2TP_DEBUG(L2TP_FUNC, "%s: session_fd=%d tunnel_fd=%d tid=%hu sid=%hu peer=%hu/%hu addr=%s port=%hu",
		   __func__, session_fd, tunnel_fd, tunnel_id, session_id,
		   peer_tunnel_id, peer_session_id, inet_ntoa(addr->sin_addr), htons(addr->sin_port));

	/* Note, the target socket must be bound already, else it will not be ready */  
	sax.sa_family = AF_PPPOX;
	sax.sa_protocol = PX_PROTO_OL2TP;
	sax.pppol2tp.pid = 0;
	sax.pppol2tp.fd = tunnel_fd;
	sax.pppol2tp.addr.sin_addr.s_addr = addr->sin_addr.s_addr;
	sax.pppol2tp.addr.sin_port = addr->sin_port;
	sax.pppol2tp.addr.sin_family = AF_INET;
	sax.pppol2tp.s_tunnel  = tunnel_id;
	sax.pppol2tp.s_session = session_id;
	sax.pppol2tp.d_tunnel  = peer_tunnel_id;
	sax.pppol2tp.d_session = peer_session_id;
  
	fd = connect(session_fd, (struct sockaddr *)&sax, sizeof(sax));
	if (fd < 0 )	{
		return -errno;
	}
	return 0;
}

/*****************************************************************************
 * Internal implementation 
 *****************************************************************************/

/* Called just before trying to start pppd to keep track of active
 * pppd processes. If the number of active pppd processes is larger
 * than a configured value, we return -ECONNREFUSED to have the caller
 * defer starting another pppd until other pppd's complete their
 * connection with their peer or die.
 */
static int ppp_unix_inc_pppd_active_count(struct ppp_context *ppp)
{
	if ((l2tp_opt_throttle_ppp_setup_rate > 0) && (ppp_unix_active_pppd_count >= l2tp_opt_throttle_ppp_setup_rate)) {
		l2tp_session_log(ppp->session, L2TP_DATA, LOG_INFO, "%s: deferring UNIX pppd", l2tp_session_get_name(ppp->session));
		return -ECONNREFUSED;
	}

	ppp_unix_active_pppd_count++;
	L2TP_DEBUG(L2TP_FUNC, "%s: ppp setup count now %d", __func__, ppp_unix_active_pppd_count);

	return 0;
}

/* Called when a pppd successfully connects with its peer (when it
 * indicates it's up) or if a pppd fails. We process the deferred
 * setup list if the number of active pppd processes is now below our
 * configured limit.
 */
static void ppp_unix_dec_pppd_active_count(void)
{
	struct ppp_context *ppp;
	int result;
	int count;

	count = ppp_unix_active_pppd_count - 1;
	if (count < 0) {
		l2tp_log(LOG_ERR, "active pppd count gone negative. Fixing up.");
		count = 0;
	}
	L2TP_DEBUG(L2TP_FUNC, "%s: ppp setup count now %d", __func__, count);
	
	if (count < l2tp_opt_throttle_ppp_setup_rate) {
		if (!usl_list_empty(&ppp_unix_deferred_setup_list)) {
			ppp = usl_list_entry(ppp_unix_deferred_setup_list.next, 
					     struct ppp_context, deferred_list);
			l2tp_session_log(ppp->session, L2TP_DATA, LOG_INFO, "%s: starting deferred UNIX pppd",
					 l2tp_session_get_name(ppp->session));

			usl_list_del_init(&ppp->deferred_list);
			ppp->pid = 0;
			ppp->running = 0;
			ppp->deleting = 0;
			ppp->creating = 1;
			result = ppp_unix_pppd_spawn(ppp);
			if (result < 0) {
				usl_list_add(&ppp->deferred_list, &ppp_unix_deferred_setup_list);
			}
			goto out;
		}
	}
	ppp_unix_active_pppd_count = count;
out:
	return;
}

static struct ppp_context *ppp_unix_find_by_id(uint16_t tunnel_id, uint16_t session_id)
{
	struct usl_list_head *walk;
	struct usl_list_head *tmp;
	struct ppp_context *ppp;

	L2TP_DEBUG(L2TP_FUNC, "%s: look for %hu/%hu", __func__, tunnel_id, session_id);

	usl_list_for_each(walk, tmp, &ppp_unix_list) {
		ppp = usl_list_entry(walk, struct ppp_context, list);
		L2TP_DEBUG(L2TP_FUNC, "%s: at %hu/%hu %p", __func__, ppp->tunnel_id, ppp->session_id, ppp);
		if ((ppp->tunnel_id == tunnel_id) && (ppp->session_id == session_id)) {
			return ppp;
		}
	}

	return NULL;
}

/* We come here when one of our pppds terminates. Called from USL.
 */
static void ppp_unix_pppd_terminated(void *arg, pid_t pid, int signal)
{
	struct ppp_context *ppp = arg;

	L2TP_DEBUG(L2TP_FUNC, "%s: %hu/%hu pid %u terminated on signal %d", __func__, ppp->tunnel_id, ppp->session_id, pid, signal);

	if (ppp->deleting) {
#ifdef DEBUG
		memset(ppp, 0xe9, sizeof(*ppp));
#endif
		free(ppp);
        } else {
		ppp->running = 0;
		if (ppp->creating) {
			ppp->creating = 0;
			ppp_unix_dec_pppd_active_count();
		}
	  
		if (!ppp->closed) {
			ppp->closed = 1;
			l2tp_session_close_event(ppp->tunnel_id, ppp->session_id);
		}
	}
}

/* Called on SESSION_DOWN. We find our ppp context and kill its pppd.
 */
static int ppp_unix_pppd_terminate(struct ppp_context *ppp, uint16_t tunnel_id, uint16_t session_id)
{
	pid_t pid;

	if (ppp == NULL) {
		ppp = ppp_unix_find_by_id(tunnel_id, session_id);
		if (ppp == NULL) {
			return 0;
		}
	}

	L2TP_DEBUG(L2TP_FUNC, "%s: pid=%d", __func__, ppp->pid);

	/* If pppd is still alive, kill it. The pid handler will tell
	 * the session, which in turn lead to a SESSION_DELETED event
	 * where we will clean up.
	 */
	pid = ppp->pid;
	if (pid != 0) {
		if (ppp->session != NULL) {
			l2tp_session_log(ppp->session, L2TP_DATA, LOG_INFO, "%s: stopping unix pppd pid %d",
					 l2tp_session_get_name(ppp->session), pid);
			ppp->pid = 0;

			l2tp_session_dec_use_count((void *) ppp->session);
			ppp->session = NULL;
			kill(pid, SIGTERM);
		} else {
			l2tp_session_log(ppp->session, L2TP_DATA, LOG_INFO, "%s: collecting orphaned unix pppd pid %d",
					 l2tp_session_get_name(ppp->session), pid);
			ppp_unix_pppd_terminated(ppp, pid, SIGTERM);
		}
	}

	return 0;
}

#if 0
/* Allow a debugger to attach */
int ppp_unix_debugger_wait = 1;
#endif

/* Spawn pppd. This is horrible because we must build command line
 * arguments for pppd using our binary data structures. PPP parameters
 * come from a ppp profile. Some additional L2TP-specific stuff comes
 * from the session config.
 */
static int ppp_unix_pppd_spawn(struct ppp_context *ppp)
{
	pid_t pid;
	int result = 0;
#ifdef BRCM_L2TP_SUPPORT
	char str[64];
#else
	char str[10];
#endif
	struct l2tp_session_config const *scfg;

#ifdef BRCM_L2TP_SUPPORT
	//printf("l2tp start\n");
#endif

	pid = usl_pid_safe_fork();
	if (pid < 0) {
		l2tp_session_log(ppp->session, L2TP_DATA, LOG_ERR, "unable to fork pppd instance for session %hu/%hu: %m", 
				 ppp->tunnel_id, ppp->session_id);
		result = pid;
		goto out;
	}

	if (pid == 0) { 
		/* child */
		char *argv[PPP_UNIX_MAX_PPPD_ARGS];
		int session_fd;
		int arg;
#if 0
		while (ppp_unix_debugger_wait) {
			sleep(1);
		}
#endif		
		scfg = l2tp_session_get_config(ppp->session);

		/* PPP proxy is not supported, but fake that it is supported if testing with a dummy pppd */
		if (scfg->use_ppp_proxy && !ppp_unix_use_dummy_pppd()) {
			l2tp_session_log(ppp->session, L2TP_DATA, LOG_ERR, "sess %hu/%hu: ppp proxy requested but not supported by UNIX pppd", 
					 ppp->tunnel_id, ppp->session_id);
			result = -EOPNOTSUPP;
			goto abort_child;
		}

		if (scfg->do_pmtu_discovery) {
			ppp->ppp_profile_data->flags |= (L2TP_API_PPP_PROFILE_FLAG_MTU | L2TP_API_PPP_PROFILE_FLAG_MRU);
			ppp->mtu = ppp->mru = scfg->mtu;
		}

		memset(&argv[0], 0, sizeof(argv));
		arg = ppp_unix_params_to_argv(ppp, ppp->ppp_profile_data, PPP_UNIX_MAX_PPPD_ARGS, &argv[0]);
		argv[arg++] = "nodetach";
		if (scfg->user_name != NULL) {
			argv[arg++] = "user";
			argv[arg++] = strdup(scfg->user_name);
		}
		if (scfg->user_password != NULL) {
			argv[arg++] = "password";
			argv[arg++] = strdup(scfg->user_password);
		}

		argv[arg++] = "local";

		/* Handle "auth" and "noauth" options. Specifying both
		 * is silly - "auth" overrides. If neither is specified,
		 * choose default depending on whether session is created
		 * by admin.
		 */
		if (ppp->ppp_profile_data->auth_peer) {
			argv[arg++] = "auth";
		} else if (ppp->ppp_profile_data->auth_none) {
			argv[arg++] = "noauth";
		} else {
			if (l2tp_session_is_created_by_admin(ppp->session)) {
				argv[arg++] = "noauth";
			} else {
				argv[arg++] = "auth";
			}
		}

		/* We never do software data or header compression when using L2TP */
		argv[arg++] = "noaccomp";
		argv[arg++] = "nopcomp";
		argv[arg++] = "nobsdcomp";
		argv[arg++] = "nodeflate";
		argv[arg++] = "nopredictor1";
		argv[arg++] = "novj";
		argv[arg++] = "novjccomp";

		if (ppp->ppp_profile_data->multilink) {
			argv[arg++] = "multilink";
		} else {
			argv[arg++] = "noendpoint";
			argv[arg++] = "nomp";
		}

		if (ppp->ppp_profile_data->proxy_arp) {
			argv[arg++] = "proxyarp";
		}

		argv[arg++] = "plugin";
		argv[arg++] = "pppol2tp.so";
#ifdef L2TP_FEATURE_RPC_MANAGEMENT
		/* This plugin requires pppol2tp.so to be loaded first */
		argv[arg++] = "plugin";
		argv[arg++] = "openl2tp.so";
#endif

		if (!ppp_unix_use_dummy_pppd()) {
			int flags;
			session_fd = socket(AF_PPPOX, SOCK_DGRAM, PX_PROTO_OL2TP);
			if (session_fd < 0) {
				l2tp_session_log(ppp->session, L2TP_DATA, LOG_ERR, "sess %hu/%hu: unable to open pppox socket: %m", 
						 ppp->tunnel_id, ppp->session_id);
				result = -errno;
				goto abort_child;
			}
			flags = fcntl(session_fd, F_GETFL);
			if (flags == -1 || fcntl(session_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
				l2tp_session_log(ppp->session, L2TP_DATA, LOG_ERR, "sess %hu/%hu: unable to set socket nonblock: %m");
				result = -errno;
				goto abort_child;
			}

			result = ppp_unix_socket_connect(session_fd, ppp->tunnel_fd, l2tp_tunnel_get_peer_addr(ppp->tunnel),
							 ppp->tunnel_id, ppp->session_id, 
							 ppp->peer_tunnel_id, ppp->peer_session_id);
			if (result < 0) {
				l2tp_session_log(ppp->session, L2TP_DATA, LOG_ERR, "sess %hu/%hu: unable to connect pppox socket: %m", 
						 ppp->tunnel_id, ppp->session_id);
				result = -errno;
				close(session_fd);
				goto abort_child;
			}
		} else {
			l2tp_session_log(ppp->session, L2TP_DATA, LOG_DEBUG, "sess %hu/%hu: using dummy pppd", 
					 ppp->tunnel_id, ppp->session_id);
			session_fd = 0;
		}

		/* Add pppol2tp arguments.
		 * NOTE: since there is no userspace interface to pppd once it is running,
		 * we cannot modify any PPP parameters once pppd has been started. These
		 * parameters are therefore unmodifiable when using UNIX pppd.
		 */

		argv[arg++] = "pppol2tp";
		sprintf(&str[0], "%d", session_fd);
		argv[arg++] = strdup(str);

		if (scfg->use_sequence_numbers) {
			argv[arg++] = "pppol2tp_send_seq";
		}
		if (scfg->sequencing_required) {
			argv[arg++] = "pppol2tp_recv_seq";
		}
		if (scfg->reorder_timeout) {
			argv[arg++] = "pppol2tp_reorderto";
			sprintf(&str[0], "%u", scfg->reorder_timeout);
			argv[arg++] = strdup(str);
		}
		if (l2tp_session_is_lns(ppp->session)) {
			argv[arg++] = "pppol2tp_lns_mode";
		}
		argv[arg++] = "pppol2tp_tunnel_id";
		sprintf(&str[0], "%hu", ppp->tunnel_id);
		argv[arg++] = strdup(str);
		argv[arg++] = "pppol2tp_session_id";
		sprintf(&str[0], "%hu", ppp->session_id);
		argv[arg++] = strdup(str);

		if (scfg->trace_flags != 0) {
			int flags = 0;
			if (scfg->trace_flags & L2TP_DEBUG_API) {
				flags |= PPPOL2TP_MSG_CONTROL;
			}
			if (scfg->trace_flags & L2TP_DEBUG_XPRT) {
				flags |= PPPOL2TP_MSG_SEQ;
			}
			if (scfg->trace_flags & L2TP_DEBUG_DATA) {
				flags |= PPPOL2TP_MSG_DATA;
			}
			if (scfg->trace_flags & L2TP_DEBUG_FUNC) {
				flags |= PPPOL2TP_MSG_DEBUG;
			}
			if (flags != 0) {
				argv[arg++] = "pppol2tp_debug_mask";
				sprintf(&str[0], "%u", flags);
				argv[arg++] = strdup(str);
			}
		}

		/* Set interface name only if not using multilink */
		if (!ppp->ppp_profile_data->multilink) {
			if ((scfg->interface_name != NULL) && (scfg->flags & L2TP_API_SESSION_FLAG_INTERFACE_NAME)) {
				ppp->interface_name = strdup(scfg->interface_name);

				if (ppp->interface_name == NULL) {
					result = -ENOMEM;
					close(session_fd);
					goto abort_child;
				}
				argv[arg++] = "pppol2tp_ifname";
				argv[arg++] = ppp->interface_name;
			}
		}

		argv[arg++] = NULL;

#if 0
#ifdef BRCM_L2TP_SUPPORT
		printf("==== pppd with sess %hu/%hu:"
#else
		l2tp_session_log(ppp->session, L2TP_DATA, LOG_DEBUG, 
				 "sess %hu/%hu:"
#endif
				 /* 0  1  2  3  4  5  6  7  8  9 */
				 " %s %s %s %s %s %s %s %s %s %s"
				 /*10 11 12 13 14 15 16 17 18 19 */
				 " %s %s %s %s %s %s %s %s %s %s"
				 /*20 21 22 23 24 25 26 27 28 29 */
				 " %s %s %s %s %s %s %s %s %s %s"
				 /*30 31 32 33 34 35 36 37 38 39 */
				 " %s %s %s %s %s %s %s %s %s %s"
				 /*40 41 42 43 44 45 46 47 48 49 */
				 " %s %s %s %s %s %s %s %s %s %s"
				 /*50 51 52 53 54 55 56 57 58 59 */
				 " %s %s %s %s %s %s %s %s %s %s"
				 /*60 */
				 " %s",
				 ppp->tunnel_id, ppp->session_id,
				 argv[0]  ? argv[0]  : "", argv[1]  ? argv[1]  : "", argv[2]  ? argv[2]  : "",
				 argv[3]  ? argv[3]  : "", argv[4]  ? argv[4]  : "", argv[5]  ? argv[5]  : "",
				 argv[6]  ? argv[6]  : "", argv[7]  ? argv[7]  : "", argv[8]  ? argv[8]  : "",
				 argv[9]  ? argv[9]  : "", argv[10] ? argv[10] : "", argv[11] ? argv[11] : "",
				 argv[12] ? argv[12] : "", argv[13] ? argv[13] : "", argv[14] ? argv[14] : "",
				 argv[15] ? argv[15] : "", argv[16] ? argv[16] : "", argv[17] ? argv[17] : "",
				 argv[18] ? argv[18] : "", argv[19] ? argv[19] : "", argv[20] ? argv[20] : "",
				 argv[21] ? argv[21] : "", argv[22] ? argv[22] : "", argv[23] ? argv[23] : "",
				 argv[24] ? argv[24] : "", argv[25] ? argv[25] : "", argv[26] ? argv[26] : "",
				 argv[27] ? argv[27] : "", argv[28] ? argv[28] : "", argv[29] ? argv[29] : "",
				 argv[30] ? argv[30] : "", argv[31] ? argv[31] : "", argv[32] ? argv[32] : "",
				 argv[33] ? argv[33] : "", argv[34] ? argv[34] : "", argv[35] ? argv[35] : "",
				 argv[36] ? argv[36] : "", argv[37] ? argv[37] : "", argv[38] ? argv[38] : "",
				 argv[39] ? argv[39] : "", argv[40] ? argv[40] : "", argv[41] ? argv[41] : "",
				 argv[42] ? argv[42] : "", argv[43] ? argv[43] : "", argv[44] ? argv[44] : "",
				 argv[45] ? argv[45] : "", argv[46] ? argv[46] : "", argv[47] ? argv[47] : "",
				 argv[48] ? argv[48] : "", argv[49] ? argv[49] : "", argv[50] ? argv[50] : "",
				 argv[51] ? argv[51] : "", argv[52] ? argv[52] : "", argv[53] ? argv[53] : "",
				 argv[54] ? argv[54] : "", argv[55] ? argv[55] : "", argv[56] ? argv[56] : "",
				 argv[57] ? argv[57] : "", argv[58] ? argv[58] : "", argv[59] ? argv[59] : "",
				 argv[60] ? "..." : "");
#endif

		fcntl(session_fd,  F_SETFD, 0);   /* Remove close on exec flag */

#ifdef BRCM_L2TP_SUPPORT
//		printf("  l2tp start ppp ...\n");
#else
		memset(&argv[0], 0, sizeof(argv));
		arg = ppp_unix_params_to_argv(ppp, ppp->ppp_profile_data, PPP_UNIX_MAX_PPPD_ARGS, &argv[0]);
#endif
		
		arg=0;
		argv[arg++] = "pppd";
		
		argv[arg++] = "-c";

#ifdef BRCM_L2TP_SUPPORT
		if (strlen(devname) >0 )
		   argv[arg++] = devname;
		else
#endif
		argv[arg++] = "ppp0";
		
		argv[arg++] = "-d";
		
		if (scfg->user_name != NULL) {
			argv[arg++] = "-u";
			argv[arg++] = strdup(scfg->user_name);
		}
		if (scfg->user_password != NULL) {
			argv[arg++] = "-p";
			argv[arg++] = strdup(scfg->user_password);
		}
		argv[arg++] = "-t";
		sprintf(&str[0], "%d.%hu.%hu", session_fd, ppp->tunnel_id, ppp->session_id);
		argv[arg++] = strdup(str);
		argv[arg++] = NULL;
#ifdef L2TP_TEST
		/* For testing, if a pppd_dummy executable exists in the current directory, run it
		 * instead of the real pppd. 
		 */
		result = execv(PPP_UNIX_PPPD_DUMMY_FILE, argv);
#endif
    
		result = execv(L2TP_PPP_UNIX_PPPD_PATH, argv);

		/* If we get here, exec() failed. Log that fact and exit */
		l2tp_log(LOG_ERR, "sess %hu/%hu: pppd could not be started: %s", 
			 ppp->tunnel_id, ppp->session_id, strerror(errno));
		exit(-errno);
	} else {
		/* parent */
		l2tp_session_log(ppp->session, L2TP_DATA, LOG_DEBUG, "sess %hu/%hu: spawned pppd pid=%d", 
				 ppp->tunnel_id, ppp->session_id, pid);
		ppp->pid = pid;

		result = usl_pid_record_child(pid, ppp_unix_pppd_terminated, ppp);
		ppp->running = 1; /* set flag to indicate we are expecting callback */

		if (result < 0) {
			l2tp_session_log(ppp->session, L2TP_DATA, LOG_ERR, "sess %hu/%hu: failed to track pppd child pid %d. Aborting.", 
					 ppp->tunnel_id, ppp->session_id, pid);
			ppp_unix_pppd_terminate(ppp, 0, 0);
			goto out;
		}
	}

out:
	L2TP_DEBUG(L2TP_FUNC, "%s: result=%d", __func__, result);
	return result;

abort_child:
	L2TP_DEBUG(L2TP_FUNC, "%s: child aborting, result=%d", __func__, result);
	exit(result);
}

/* Get PPP parameter data from the profile.
 */
static int ppp_unix_param_defaults(struct ppp_context *ppp, char *ppp_profile_name)
{
	l2tp_api_ppp_profile_msg_data *profile;
	int result = 0;

	profile = calloc(1, sizeof(*profile));
	if (profile == NULL) {
		result = -ENOMEM;
		goto out;
	}

	if (ppp_profile_name == NULL) {
		ppp_profile_name = L2TP_API_PPP_PROFILE_DEFAULT_PROFILE_NAME;
	}

	result = l2tp_ppp_profile_get(ppp_profile_name, profile);
	if (result < 0) {
		L2TP_DEBUG(L2TP_API, "Ppp profile '%s' not found", ppp_profile_name);
		free(profile);
		return result;
	}

	l2tp_session_log(ppp->session, L2TP_API, LOG_INFO, "%s: using ppp profile '%s'", 
			 l2tp_session_get_name(ppp->session), ppp_profile_name);
	
	ppp->ppp_profile_data = profile;

	ppp->trace_flags = profile->trace_flags;

out:
	return result;
}

/*****************************************************************************
 * Context management
 *****************************************************************************/

static void ppp_unix_setup_timeout(void *arg)
{	
	struct ppp_context *ppp = arg;

	l2tp_session_log(ppp->session, L2TP_DATA, LOG_ERR, "%s: ppp setup timeout", l2tp_session_get_name(ppp->session));
	
#ifdef BRCM_L2TP_SUPPORT
	// todo: use l2tp_session_ppp_updown_ind().
	// ppp_unix_pppd_terminate(ppp, 0, 0);
#endif
}

static int ppp_unix_delete(struct ppp_context *ppp)
{
	L2TP_DEBUG(L2TP_FUNC, "%s: tid=%hu sid=%hu", __func__, ppp->tunnel_id, ppp->session_id);

	if (ppp->setup_timer != NULL) {
		usl_timer_delete(ppp->setup_timer);
	}

	usl_list_del(&ppp->deferred_list);
	if (ppp->creating) {
		ppp->creating = 0;
		ppp_unix_dec_pppd_active_count();
	}

	if (ppp->ppp_profile_data != NULL) {
		l2tp_ppp_profile_msg_free(ppp->ppp_profile_data);
	}

	if (ppp->tunnel != NULL) {
		l2tp_tunnel_dec_use_count((void *) ppp->tunnel);
	}
	if (ppp->session != NULL) {
		l2tp_session_dec_use_count((void *) ppp->session);
	}

	if (ppp->interface_name != NULL) {
		free(ppp->interface_name);
	}

	usl_list_del(&ppp->list);

	if (ppp->running) {
		ppp->deleting = 1; /* wait for usl callback before freeing */
	} else {
#ifdef DEBUG
		memset(ppp, 0xe9, sizeof(*ppp));
#endif
		free(ppp);
	}

	return 0;
}

static int ppp_unix_create(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id)
{
	struct ppp_context *ppp = NULL;
	int result = 0;
	struct l2tp_tunnel const *tunnel;
	struct l2tp_session_config const *scfg;
	int establish_timeout;

	L2TP_DEBUG(L2TP_FUNC, "%s: tid=%hu sid=%hu", __func__, tunnel_id, session_id);

	ppp = ppp_unix_find_by_id(tunnel_id, session_id);
	if (ppp != NULL) {
		/* Nothing to do if already exists - it just means we got multiple create events */
		result = -EEXIST;
		L2TP_DEBUG(L2TP_FUNC, "%s: tid=%hu sid=%hu: already exists", __func__, tunnel_id, session_id);
		goto out;
	}

	/* Allocate a new ppp context */
	ppp = calloc(1, sizeof(struct ppp_context));
	if (ppp == NULL) {
		result = -ENOMEM;
		l2tp_stats.no_ppp_resources++;
		goto out;
	}

	USL_LIST_HEAD_INIT(&ppp->list);
	USL_LIST_HEAD_INIT(&ppp->deferred_list);

	tunnel = l2tp_session_get_tunnel(session);
	ppp->tunnel_id = tunnel_id;
	ppp->session_id = session_id;
	l2tp_tunnel_inc_use_count((void *) tunnel);
	ppp->tunnel = tunnel;
	ppp->tunnel_fd = l2tp_tunnel_get_fd(tunnel);
	ppp->kernel_fd = l2tp_xprt_get_kernel_fd(tunnel);
	l2tp_session_inc_use_count((void *) session);
	ppp->session = session;

	/* Fill with values from the specified PPP profile. Use default profile if none is specified. */
	scfg = l2tp_session_get_config(session);
	result = ppp_unix_param_defaults(ppp, scfg->ppp_profile_name);
	if (result < 0) {
		L2TP_DEBUG(L2TP_FUNC, "%s: tid=%hu sid=%hu: param error", __func__, tunnel_id, session_id);
		goto err;
	}

	establish_timeout = l2tp_session_get_establish_timeout();
	if (establish_timeout > 0) {
		ppp->setup_timer = usl_timer_create(USL_TIMER_TICKS(establish_timeout), 0, 
						    ppp_unix_setup_timeout, ppp, NULL);
		if (ppp->setup_timer == NULL) {
			L2TP_DEBUG(L2TP_FUNC, "%s: tid=%hu sid=%hu: timer create error", __func__, tunnel_id, session_id);
			goto err;
		}
	}

	/* The PPP session's MTU is derived from the PPP profile and tunnel's MTU */
	ppp->mtu = ppp->mru = l2tp_tunnel_get_mtu(tunnel);
	if (ppp->ppp_profile_data->mtu < ppp->mtu) {
		ppp->mtu = ppp->ppp_profile_data->mtu;
	}
	if (ppp->ppp_profile_data->mru < ppp->mru) {
		ppp->mru = ppp->ppp_profile_data->mru;
	}

	/* Link it to our list of ppps */
	usl_list_add(&ppp->list, &ppp_unix_list);

	return 0;

err:
	l2tp_session_dec_use_count((void *) session);
	l2tp_tunnel_dec_use_count((void *) tunnel);
	if (ppp != NULL) {
		ppp_unix_delete(ppp);
	}

out:
	return result;
}

/*****************************************************************************
 * Hooks
 * These functions are called by OpenL2TP when certain events occur.
 * We use the events to start/stop pppd.
 * SESSION_CREATED	- create a new context to handle ppp on the session
 * SESSION_UP		- spawn a pppd
 * SESSION_DOWN		- stop (kill) pppd and tell session in case it doesn't
 *			  yet know that ppp has died.
 * SESSION_DELETED	- destroy our ppp context
 *****************************************************************************/

static int ppp_unix_session_created(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id)
{
	int result = 0;
	const struct l2tp_session_config *cfg;

	if (ppp_unix_old_session_created_hook != NULL) {
		result = (*ppp_unix_old_session_created_hook)(session, tunnel_id, session_id);
	}

	L2TP_DEBUG(L2TP_DATA, "%s: %s: tid=%hu sid=%hu", __func__, l2tp_session_get_name(session),
		   tunnel_id, session_id);

	/* Is PPP really required on this session? */
	cfg = l2tp_session_get_config(session);
	if (cfg->no_ppp) {
		goto out;
	}

	if (result >= 0) {
		l2tp_session_log(session, L2TP_DATA, LOG_INFO, "%s: creating UNIX pppd context", l2tp_session_get_name(session));

		result = ppp_unix_create(session, tunnel_id, session_id);
	}

out:
	return result;
}

static int ppp_unix_session_deleted(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id)
{
	int result = 0;
	const struct l2tp_session_config *cfg;

	if (ppp_unix_old_session_deleted_hook != NULL) {
		result = (*ppp_unix_old_session_deleted_hook)(session, tunnel_id, session_id);
	}

	L2TP_DEBUG(L2TP_DATA, "%s: %s: tid=%hu sid=%hu", __func__, l2tp_session_get_name(session),
		   tunnel_id, session_id);

	/* Is PPP enabled on this session? */
	cfg = l2tp_session_get_config(session);
	if (cfg->no_ppp) {
		goto out;
	}

	if (result >= 0) {
		struct ppp_context *ppp = ppp_unix_find_by_id(tunnel_id, session_id);
		if (ppp != NULL) {
			l2tp_session_log(session, L2TP_DATA, LOG_INFO, "%s: cleaning UNIX pppd context",
					 l2tp_session_get_name(session));
			ppp_unix_delete(ppp);
		}
	}

out:
	return result;
}

static int ppp_unix_session_modified(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id)
{
	int result = 0;
	struct ppp_context *ppp;
	int mtu;
	struct ifreq ifr;
	int tmp_fd;

	if (ppp_unix_old_session_modified_hook != NULL) {
		result = (*ppp_unix_old_session_modified_hook)(session, tunnel_id, session_id);
	}

	L2TP_DEBUG(L2TP_API, "%s: tid=%hu sid=%hu", __func__, tunnel_id, session_id);

	ppp = ppp_unix_find_by_id(tunnel_id, session_id);
	if (ppp == NULL) {
		goto out;
	}

	/* MTU changed? */
	mtu = l2tp_tunnel_get_mtu(ppp->tunnel);
	if ((mtu != ppp->mtu) || (mtu != ppp->mru)) {
		l2tp_session_log(session, L2TP_PPP, LOG_INFO, "sess %hu/%hu: changed mtu", 
				 tunnel_id, session_id);

		tmp_fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (tmp_fd >= 0) {
			memset (&ifr, '\0', sizeof (ifr));
			strncpy(ifr.ifr_name, ppp->interface_name, sizeof (ifr.ifr_name));
			ifr.ifr_mtu = mtu;

			result = ioctl(tmp_fd, SIOCSIFMTU, (caddr_t) &ifr);
			close(tmp_fd);
		}     

		/* Unfortunately there is no way to tell pppd to change the MTU of its
		 * file descriptor. ioctl(fd, PPPIOCSMRU/PPPIOCSMTU). pppd really needs
		 * an API to allow these changes...
		 */
		ppp->mtu = mtu;
		ppp->mru = mtu;
	}

		
	/* Parameters of a pppd that is already running cannot be
	 * modified - pppd has no interface to do so.
	 */
out:
	return result;
}

static int ppp_unix_session_up(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id, uint16_t peer_tunnel_id, uint16_t peer_session_id)
{
	struct ppp_context *ppp;
	int result = 0;

	if (ppp_unix_old_session_up_hook != NULL) {
		result = (*ppp_unix_old_session_up_hook)(session, tunnel_id, session_id, peer_tunnel_id, peer_session_id);
	}

	L2TP_DEBUG(L2TP_DATA, "%s: %s: tid=%hu sid=%hu ptid=%hu psid=%hu", __func__, l2tp_session_get_name(session),
		   tunnel_id, session_id, peer_tunnel_id, peer_session_id);

	if (result >= 0) {
		ppp = ppp_unix_find_by_id(tunnel_id, session_id);
		if (ppp != NULL) {
			if (ppp->session != session) {
				l2tp_session_log(session, L2TP_DATA, LOG_ERR, "%s: UNIX pppd context list corruption detected!",
						 l2tp_session_get_name(session));
				result = -ENOENT;
				goto out;
			}
			ppp->peer_tunnel_id = peer_tunnel_id;
			ppp->peer_session_id = peer_session_id;
			if (!ppp->creating) {
				if (ppp_unix_inc_pppd_active_count(ppp) < 0) {
					usl_list_add_tail(&ppp->deferred_list, &ppp_unix_deferred_setup_list);
					result = 0;
					goto out;
				}

				ppp->creating = 1;

				l2tp_session_log(session, L2TP_DATA, LOG_INFO, "%s: starting UNIX pppd", l2tp_session_get_name(session));
				result = ppp_unix_pppd_spawn(ppp);
			}
		} else {
			const struct l2tp_session_config *cfg;

			/* Is PPP enabled on this session? */
			cfg = l2tp_session_get_config(session);
			if (cfg->no_ppp) {
				goto out;
			}

			l2tp_session_log(session, L2TP_DATA, LOG_ERR, "%s: UNIX pppd context not found", l2tp_session_get_name(session));
			result = -ENOENT;
		}
	}

out:
	return result;
}

static int ppp_unix_session_down(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id)
{
	int result = 0;

	if (ppp_unix_old_session_down_hook != NULL) {
		result = (*ppp_unix_old_session_down_hook)(session, tunnel_id, session_id);
	}

	L2TP_DEBUG(L2TP_DATA, "%s: %s: tid=%hu sid=%hu", __func__, l2tp_session_get_name(session),
		   tunnel_id, session_id);

	if (result >= 0) {
		result = ppp_unix_pppd_terminate(NULL, tunnel_id, session_id);
	}

	return result;
}

static int ppp_unix_session_get_stats(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id, struct pppol2tp_ioc_stats *stats)
{
	struct ppp_context *ppp;
	int result = 0;

	if (ppp_unix_old_session_get_stats_hook != NULL) {
		(*ppp_unix_old_session_get_stats_hook)(session, tunnel_id, session_id, stats);
	}

	L2TP_DEBUG(L2TP_DATA, "%s: %s: tid=%hu sid=%hu", __func__, l2tp_session_get_name(session),
		   tunnel_id, session_id);

	ppp = ppp_unix_find_by_id(tunnel_id, session_id);
	if ((ppp != NULL) && (ppp->kernel_fd >= 0)) {
		stats->tunnel_id = tunnel_id;
		stats->session_id = session_id;
		L2TP_DEBUG(L2TP_API, "PPP: sess %s: using fd=%d", l2tp_session_get_name(session), ppp->kernel_fd);
		result = ioctl(ppp->kernel_fd, PPPIOCGL2TPSTATS, stats);
		if (result < 0) {
			result = -errno;
			l2tp_session_log(session, L2TP_API, LOG_ERR, "PPP: sess %s: ioctl(PPPIOCGL2TPSTATS) failed: %m", l2tp_session_get_name(session));
			goto out;
		}
	} else {
		L2TP_DEBUG(L2TP_DATA, "%s: %s: no ppp context for tid=%hu sid=%hu fd=%d", __func__, l2tp_session_get_name(session),
			   tunnel_id, session_id, ppp ? ppp->tunnel_fd : -1);
	}
out:
	return result;
}

static void ppp_unix_session_ppp_created_hook(struct l2tp_session const *session,
					      uint16_t tunnel_id, uint16_t session_id,
					      int unit)
{
	struct ppp_context *ppp;

	if (ppp_unix_old_session_ppp_created_hook != NULL) {
		(*ppp_unix_old_session_ppp_created_hook)(session, tunnel_id, session_id, unit);
	}

	L2TP_DEBUG(L2TP_DATA, "%s: %s: tid=%hu sid=%hu", __func__, l2tp_session_get_name(session),
		   tunnel_id, session_id);

	ppp = ppp_unix_find_by_id(tunnel_id, session_id);
	if (ppp != NULL) {
		if (ppp->creating) {
			ppp->creating = 0;
			ppp_unix_dec_pppd_active_count();
		}

		if (ppp->setup_timer != NULL) {
			usl_timer_stop(ppp->setup_timer);
		}
	}
}

static void ppp_unix_session_ppp_deleted_hook(struct l2tp_session const *session,
					      uint16_t tunnel_id, uint16_t session_id)
{
	struct ppp_context *ppp;

	if (ppp_unix_old_session_ppp_deleted_hook != NULL) {
		(*ppp_unix_old_session_ppp_deleted_hook)(session, tunnel_id, session_id);
	}

	L2TP_DEBUG(L2TP_DATA, "%s: %s: tid=%hu sid=%hu", __func__, l2tp_session_get_name(session),
		   tunnel_id, session_id);

	ppp = ppp_unix_find_by_id(tunnel_id, session_id);
	if ((ppp != NULL) && (ppp->setup_timer != NULL)) {
		usl_timer_stop(ppp->setup_timer);
	}
}

/*****************************************************************************
 * L2TP plugin interface
 *****************************************************************************/

const char openl2tp_plugin_version[] = "V1.5";

int openl2tp_plugin_init(void)
{	
#ifdef L2TP_TEST
	struct stat statbuf;
	if (stat(PPP_UNIX_PPPD_DUMMY_FILE, &statbuf) == 0) {
		L2TP_DEBUG(L2TP_API, "Using dummy pppd for testing.");
		ppp_unix_dummy_pppd = 1;
	}
#endif

	/* Don't load again */
	l2tp_inhibit_default_plugin = 1;

	L2TP_DEBUG(L2TP_FUNC, "%s: enter", __func__);

	ppp_unix_old_session_created_hook = l2tp_session_created_hook;
	ppp_unix_old_session_deleted_hook = l2tp_session_deleted_hook;
	ppp_unix_old_session_modified_hook = l2tp_session_modified_hook;
	ppp_unix_old_session_up_hook = l2tp_session_up_hook;
	ppp_unix_old_session_down_hook = l2tp_session_down_hook;
	ppp_unix_old_session_get_stats_hook = l2tp_session_get_stats_hook;
	ppp_unix_old_session_ppp_created_hook = l2tp_session_ppp_created_hook;
	ppp_unix_old_session_ppp_deleted_hook = l2tp_session_ppp_deleted_hook;

	l2tp_session_created_hook = ppp_unix_session_created;
	l2tp_session_deleted_hook = ppp_unix_session_deleted;
	l2tp_session_modified_hook = ppp_unix_session_modified;
	l2tp_session_up_hook = ppp_unix_session_up;
	l2tp_session_down_hook = ppp_unix_session_down;
	l2tp_session_get_stats_hook = ppp_unix_session_get_stats;
	l2tp_session_ppp_created_hook = ppp_unix_session_ppp_created_hook;
	l2tp_session_ppp_deleted_hook = ppp_unix_session_ppp_deleted_hook;

	return 0;
}

void openl2tp_plugin_cleanup(void)
{
	struct usl_list_head *walk;
	struct usl_list_head *tmp;
	struct ppp_context *ppp;

	L2TP_DEBUG(L2TP_FUNC, "%s: enter", __func__);

	l2tp_session_created_hook = ppp_unix_old_session_created_hook;
	l2tp_session_deleted_hook = ppp_unix_old_session_deleted_hook;
	l2tp_session_modified_hook = ppp_unix_old_session_modified_hook;
	l2tp_session_up_hook = ppp_unix_old_session_up_hook;
	l2tp_session_down_hook = ppp_unix_old_session_down_hook;
	l2tp_session_get_stats_hook = ppp_unix_old_session_get_stats_hook;
	l2tp_session_ppp_created_hook = ppp_unix_old_session_ppp_created_hook;
	l2tp_session_ppp_deleted_hook = ppp_unix_old_session_ppp_deleted_hook;

	usl_list_for_each(walk, tmp, &ppp_unix_list) {
		ppp = usl_list_entry(walk, struct ppp_context, list);
		ppp_unix_delete(ppp);
	}
}

