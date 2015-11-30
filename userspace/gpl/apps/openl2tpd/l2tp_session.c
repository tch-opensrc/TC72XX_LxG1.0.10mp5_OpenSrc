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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307	 USA 
 *
 *****************************************************************************/

/* L2TP session implementation.
 *
 * Sessions are scoped by tunnel and are identified by a tunnel_id /
 * session_id pair.  For management convenience, sessions can also be
 * given an administrative name.
 *
 * Sessions are created either by management request or by an L2TP
 * control protocol request from a remote peer. There are 4 types, all
 * of which are implemented here using four different state machines.
 */

#define _GNU_SOURCE
#include <string.h>		/* for strndup() */

#include "usl.h"
#include "l2tp_private.h"
#include "l2tp_rpc.h"

#ifndef aligned_u64
/* should be defined in sys/types.h */
#define aligned_u64 unsigned long long __attribute__((aligned(8)))
#endif
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_pppox.h>
#include <linux/ppp_defs.h>
#include <linux/if_ppp.h>

// todo: support in toolchains 
//#include <linux/if_pppol2tp.h>

#ifdef DEBUG
#define L2TP_SESSION_CHECK(_sess, _tunnel)						\
do {											\
	if ((_tunnel != NULL) && (_tunnel != _sess->my_tunnel)) {			\
		l2tp_log(LOG_ERR, "session check: bad tunnel (%p %p)",			\
			 _tunnel, _sess->my_tunnel);					\
		_tunnel = _sess->my_tunnel;						\
	}										\
} while(0)
#else
#define L2TP_SESSION_CHECK(_sess, _tunnel) do { } while(0)
#endif /* DEBUG */

/* Session context. One per session.
 */
struct l2tp_session {
	struct usl_list_head				list;		/* our session list (all tunnels) */
	struct usl_list_head				session_list;	/* tunnel's session list */
	struct usl_hlist_node				session_id_hlist; /* tunnel's hashed session id list */
	struct usl_list_head				detached_list;	/* detached session list (persisting) */
	struct usl_fsm_instance				fsmi;
	struct l2tp_tunnel				*my_tunnel;
	enum l2tp_api_session_type			type;
	int						open_event;
	int						close_event;
	int						use_count;
	struct usl_timer				*timer;
	struct usl_timer				*retry_timer;
	struct l2tp_session_config			config;
	struct {
		uint16_t				session_id;
		uint16_t				peer_session_id;
		uint16_t				created_by_admin:1;
		uint16_t				persist:1;
		uint16_t				detached:1;
		uint16_t				tunnel_down:1;
		uint16_t				pad1;
		uint32_t				call_serial_number;
		uint32_t				physical_channel_id;
		char					*create_time;
	} status;
	struct {
		struct l2tp_avp_minimum_bps		minimum_bps;
		struct l2tp_avp_maximum_bps		maximum_bps;
		struct l2tp_avp_connect_speed		connect_speed;
		struct l2tp_avp_rx_connect_speed	rx_connect_speed;
		struct l2tp_avp_proxy_auth_type		proxy_auth_type;
		int					sequencing_required:1;
		struct l2tp_avp_proxy_auth_id		proxy_auth_id;
		struct l2tp_avp_proxy_auth_name		*proxy_auth_name;
		struct l2tp_avp_proxy_auth_challenge	*proxy_auth_challenge;
		int					proxy_auth_challenge_len;
		struct l2tp_avp_proxy_auth_response	*proxy_auth_response;
		int					proxy_auth_response_len;
		struct l2tp_avp_priv_group_id		*priv_group_id;
		struct l2tp_avp_framing_type		framing_type;
		struct l2tp_avp_bearer_type		bearer_type;
		struct l2tp_avp_call_serial_number	call_serial_number;
		struct l2tp_avp_physical_channel_id	physical_channel_id;
		struct l2tp_avp_initial_rcvd_lcp_confreq *initial_rcvd_lcp_confreq;
		int					initial_rcvd_lcp_confreq_len;
		struct l2tp_avp_last_sent_lcp_confreq	*last_sent_lcp_confreq;
		int					last_sent_lcp_confreq_len;
		struct l2tp_avp_last_rcvd_lcp_confreq	*last_rcvd_lcp_confreq;
		int					last_rcvd_lcp_confreq_len;
		struct l2tp_avp_called_number		*called_number;
		struct l2tp_avp_calling_number		*calling_number;
		struct l2tp_avp_sub_address		*sub_address;
		struct l2tp_avp_call_errors		*call_errors;
		struct l2tp_avp_accm			*accm;
		struct l2tp_avp_q931_cause_code		*q931_cause_code;
		int					q931_cause_code_len;
	} peer;
	/* Temporary state, filled in by FSM actions */
	struct l2tp_avp_result_code			*result_code;
	int						result_code_len;
	struct l2tp_avp_q931_cause_code			*q931_cause_code;
	int						q931_cause_code_len;
	uint32_t					send_accm;
	uint32_t					recv_accm;
	/* event hook debounce */
	int						sent_created_event:1;
	int						sent_up_event:1;
	int						sent_down_event:1;
	int						sent_deleted_event:1;
	int						cleaned_up:1;
};

/* Session profile context.
 */
struct l2tp_session_profile {
	struct usl_list_head				list;
	char						*profile_name;
	uint32_t					flags;
	int						sequencing_required:1;
	int						use_sequence_numbers:1;
	int						no_ppp:1;
	int						reorder_timeout;
	int						trace_flags;
	char						*ppp_profile_name;
	char						*priv_group_id;
	uint32_t					minimum_bps;
	uint32_t					maximum_bps;
	uint32_t					connect_speed;
	uint32_t					rx_connect_speed;
	uint16_t					framing_type_sync:1;
	uint16_t					framing_type_async:1;
	uint16_t					bearer_type_digital:1;
	uint16_t					bearer_type_analog:1;
	uint16_t					use_ppp_proxy:1;
	int						use_count;
};

/* Hooks, overridable by plugins */
int (*l2tp_session_created_hook)(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id) = NULL;
int (*l2tp_session_deleted_hook)(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id) = NULL;
int (*l2tp_session_modified_hook)(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id) = NULL;
int (*l2tp_session_up_hook)(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id, uint16_t peer_tunnel_id, uint16_t peer_session_id) = NULL;
int (*l2tp_session_down_hook)(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id) = NULL;
int (*l2tp_session_get_stats_hook)(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id, struct pppol2tp_ioc_stats *stats) = NULL;
int (*l2tp_session_open_bearer_hook)(struct l2tp_session const *session, const char *called_number) = NULL;
int (*l2tp_session_close_bearer_hook)(struct l2tp_session const *session, const char *called_number) = NULL;

void (*l2tp_session_ppp_created_hook)(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id, int unit) = NULL;
void (*l2tp_session_ppp_deleted_hook)(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id) = NULL;

static int l2tp_session_queue_event(uint16_t tunnel_id, uint16_t session_id, int event);
static struct l2tp_session *l2tp_session_alloc(struct l2tp_tunnel *tunnel, enum l2tp_api_session_type session_type, uint16_t session_id, 
					       char *session_profile_name, int created_by_admin, int *result);
static void l2tp_session_free(struct l2tp_session *session);
static int l2tp_session_link(struct l2tp_session *session);
static void l2tp_session_unlink(struct l2tp_session *session, int force);
static struct l2tp_session_profile *l2tp_session_profile_find(const char *name);
static void l2tp_session_persist(struct l2tp_session *session);
static void l2tp_session_tunnel_detach(struct l2tp_tunnel *tunnel, struct l2tp_session *session);

enum l2tp_session_msg_action { MSG_ACCEPT, MSG_DENY };

static int l2tp_session_map_event(enum l2tp_api_session_type session_type, int msg_type, enum l2tp_session_msg_action accept);

/* Local data */

static struct l2tp_session_profile *l2tp_session_defaults;

static USL_LIST_HEAD(l2tp_session_list);
static USL_LIST_HEAD(l2tp_session_detached_list);
static USL_LIST_HEAD(l2tp_session_profile_list);
static uint32_t l2tp_session_call_serial_number = 0;

static int l2tp_session_persist_pend_timeout;
static int l2tp_session_establish_timeout;
static int l2tp_session_count;
static int l2tp_session_max_count;

static int l2tp_session_event_pipe[2] = { -1, -1 };

/* If we need to set a result code but we run out of memory, use
 * this preallocated one.
 */
#define L2TP_SES_EMERG_RESULT_CODE_SIZE		128
static struct l2tp_avp_result_code *l2tp_session_emergency_result_code;

#undef RESC
#undef ERRC
#define RESC(x)	L2TP_AVP_RESULT_CDN_##x
#define ERRC(x)	L2TP_AVP_ERROR_##x

/* This table is used to derive English phrases for well-known L2TP session errors.
 * See RFC2661.
 */
static const struct l2tp_result_codes l2tp_session_cdn_result_codes[] = {
	{ RESC(RESERVED),	ERRC(NO_ERROR),		"Reserved" },
	{ RESC(LOST_CARRIER),	ERRC(NO_ERROR),		"Call disconnected due to loss of carrier" },
	{ RESC(GENERAL_ERROR),	ERRC(NO_ERROR),		"Call disconnected for the reason indicated in error code" },
	{ RESC(GENERAL_ERROR),	ERRC(NO_TUNNEL_YET),	"No control connection exists yet for this LAC-LNS pair" },
	{ RESC(GENERAL_ERROR),	ERRC(BAD_LENGTH),	"Length is wrong" },
	{ RESC(GENERAL_ERROR),	ERRC(BAD_VALUE),	"One of the field values was out of range or reserved field was non-zero" },
	{ RESC(GENERAL_ERROR),	ERRC(NO_RESOURCE),	"Insufficient resources to handle this operation now" },
	{ RESC(GENERAL_ERROR),	ERRC(BAD_SESSION_ID),	"The Session ID is invalid in this context" },
	{ RESC(GENERAL_ERROR),	ERRC(VENDOR_ERROR),	"A generic vendor-specific error occurred" },
	{ RESC(GENERAL_ERROR),	ERRC(TRY_ANOTHER),	"Try another" },
	{ RESC(GENERAL_ERROR),	ERRC(MBIT_SHUTDOWN),	"Unknown mandatory AVP received" },
	{ RESC(ADMIN),		ERRC(NO_ERROR),		"Call disconnected for administrative reasons" },
	{ RESC(NO_RESOURCES),	ERRC(NO_ERROR),		"Call failed due to lack of resources" },
	{ RESC(NOT_AVAILABLE),	ERRC(NO_ERROR),		"Call failed due to no configuration or support" },
	{ RESC(INVALID_DEST),	ERRC(NO_ERROR),		"Invalid destination" },
	{ RESC(NO_CARRIER),	ERRC(NO_ERROR),		"Call failed due to no carrier detected" },
	{ RESC(BUSY_SIGNAL),	ERRC(NO_ERROR),		"Call failed due to detection of a busy signal" },
	{ RESC(NO_DIAL_TONE),	ERRC(NO_ERROR),		"Call failed due to lack of a dial tone" },
	{ RESC(NO_ANSWER),	ERRC(NO_ERROR),		"Call was not established within time allotted" },
	{ RESC(INVALID_XPRT),	ERRC(NO_ERROR),		"Call was connected but no appropriate framing was detected" },
	{ -1, -1, NULL },
};

/*****************************************************************************
 * Public interface
 *****************************************************************************/

/* Log a message if the session's trace flags are enabled for the
 * message category. 
 */
void l2tp_session_log(struct l2tp_session const *session, int category, int level, const char *fmt, ...)
{
	if ((session != NULL) && (category & session->config.trace_flags)) {
		va_list ap;

		va_start(ap, fmt);
		l2tp_vlog(level, fmt, ap);
		va_end(ap);
	}
}

/* Give external modules access to the tunnel via the session.
 */
struct l2tp_tunnel *l2tp_session_get_tunnel(struct l2tp_session const *session)
{
	return session->my_tunnel;
}

/* Give external modules visibility of the session name.
 */
const char *l2tp_session_get_name(struct l2tp_session const *session)
{
	return session->fsmi.name;
}

/* For use by plugins.
 */
void l2tp_session_get_call_info(struct l2tp_session const *session, uint16_t *session_id, uint16_t *peer_session_id, 
				uint32_t *call_serial_number, uint32_t *physical_channel_id)
{
	if (session_id != NULL) {
		*session_id = session->status.session_id;
	}
	if (peer_session_id != NULL) {
		*peer_session_id = session->status.peer_session_id;
	}
	if (call_serial_number != NULL) {
		*call_serial_number = session->status.call_serial_number;
	}
	if (physical_channel_id != NULL) {
		*physical_channel_id = session->status.physical_channel_id;
	}
}

/* For access to a session's config. Used by plugins.
 */
struct l2tp_session_config const *l2tp_session_get_config(struct l2tp_session const *session)
{
	return &session->config;
}

/* For access to the configured session establish timeout. Used by plugins..
 */
int l2tp_session_get_establish_timeout(void)
{
	return l2tp_session_establish_timeout;
}

/* Allow plugins to tell if a session is created by local admin.
 */
int l2tp_session_is_created_by_admin(struct l2tp_session const *session)
{
	return session->status.created_by_admin ? 1 : 0;
}

/* Easy way to tell if the session is at the LNS.
 */
int l2tp_session_is_lns(struct l2tp_session const *session)
{
	if ((session->type == L2TP_API_SESSION_TYPE_LNIC) ||
	    (session->type == L2TP_API_SESSION_TYPE_LNOC)) {
		return TRUE;
	}
	return FALSE;
}

/*****************************************************************************
 * Internal implementation
 *****************************************************************************/

static void l2tp_session_fsm_log(struct usl_fsm_instance const *fsmi, int level, const char *fmt, ...)
{
	struct l2tp_session *session = ((void *) fsmi) - offsetof(struct l2tp_session, fsmi);
	
	if (session->config.trace_flags & L2TP_FSM) {
		va_list ap;

		va_start(ap, fmt);
		l2tp_vlog(level, fmt, ap);
		va_end(ap);
	}
}

static void l2tp_session_log_error(struct l2tp_session *session,
				   struct l2tp_avp_result_code *result_code,
				   int result_code_len)
{
	const struct l2tp_result_codes *entry = &l2tp_session_cdn_result_codes[0];

	if ((result_code == NULL) || (result_code_len < 4)) {
		return;
	}

	while (entry->result_code >= 0) {
		if (entry->result_code == result_code->result_code) {
			if (entry->error_code == result_code->error_code) {
				l2tp_session_log(session, L2TP_PROTOCOL, LOG_INFO, "PROTO: session %hu/%hu, CDN error %d/%d: %s%s%s",
						 session->my_tunnel ? l2tp_tunnel_id(session->my_tunnel) : 0, 
						 session->status.session_id, 
						 result_code->result_code,
						 result_code->error_code,
						 entry->error_string ? entry->error_string : "",
						 (result_code_len > sizeof(*result_code)) && 
						 result_code->error_message ? " - " : "",
						 (result_code_len > sizeof(*result_code)) && 
						 result_code->error_message ? result_code->error_message : "");
				break;
			}
		} 
		entry++;
	}
}

/* Called when something goes wrong in the session. Sets result code
 * info in the session which is sent to the peer in certain L2TP
 * protocol messages. As is typical with L2TP protocol messages, the
 * data may be different length depending on the result code.
 */
static void l2tp_session_set_result(struct l2tp_session *session, uint16_t result_code,
				    uint16_t error_code, char *error_string)
{
	int len = 2;

	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s: code=%hu error=%hu msg=%s", __func__,
		   session->fsmi.name, result_code, error_code, error_string ? error_string : "");

	/* Don't overwrite a result that is already present */
	if (session->result_code != NULL) {
		L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s: preserving current data: code=%hu error=%hu msg=%s", __func__,
			   session->fsmi.name, session->result_code->result_code, 
			   session->result_code->error_code, 
			   session->result_code_len > sizeof(*session->result_code) ? session->result_code->error_message : "");
		goto out;
	}

	/* Build result_code data structure */

	if ((error_code != 0) || (error_string != NULL)) {
		len += 2;
		if (error_string != NULL) {
			len += strlen(error_string) + 1;
		}
	}

	session->result_code = malloc(len <= 5 ? 5 : len);
	if (session->result_code == NULL) {
		/* Use emergency result code */
		session->result_code = l2tp_session_emergency_result_code;
	}
	session->result_code->result_code = result_code;
	if (len > 2) {
		session->result_code->error_code = error_code;
	} else {
		session->result_code->error_code = 0;
	}
	if (len > 4) {
		strncpy(&session->result_code->error_message[0], error_string, len - sizeof(struct l2tp_avp_result_code));
	} else {
		session->result_code->error_message[0] = '\0';
	}
	session->result_code_len = len;

out:
	return;
}

/* Convert Unix errno into a reasonable result code.
 */
static void l2tp_session_set_result_from_error_code(struct l2tp_session *session, int error_code)
{
	char buf[80];

	switch (error_code) {
	case -ENOMEM:
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_NO_RESOURCE, NULL);
		break;
	case -EBADMSG:
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_BAD_VALUE, "unable to parse message");
		break;
	case -EINVAL:
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_BAD_VALUE, "bad message");
		break;
	case -EAGAIN:
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_TRY_ANOTHER, NULL);
		break;
	case -EOPNOTSUPP:
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_NOT_AVAILABLE, 0, NULL);
		break;
	default:
		sprintf(&buf[0], "error %d", error_code);
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_NO_ERROR, buf);
	}
}

/* Come here to pass an event to the session's state machine. The
 * event is handled in-line.
 */
static void l2tp_session_handle_event(struct l2tp_session *session, int event)
{
	if (event < 0) {
		l2tp_session_set_result_from_error_code(session, event);

		/* If the event can't be mapped, we must close the session */
		event = session->close_event;
	}

	/* Bump the use count on the session while the event is handled in case the event
	 * cause the session to be deleted. The context must not be deleted until after 
	 * the FSM event handler has returned.
	 */
	if (session->my_tunnel != NULL) {
		l2tp_session_inc_use_count(session);
		usl_fsm_handle_event(&session->fsmi, event, session->my_tunnel, session, NULL);
		l2tp_session_dec_use_count(session);
	}
}

static int l2tp_session_param_defaults(struct l2tp_session *session, char *profile_name)
{
	struct l2tp_session_profile *profile;
	char *session_profile_name = NULL;
	char *ppp_profile_name = NULL;

	l2tp_tunnel_get_profile_names(session->my_tunnel, NULL,
				      &session_profile_name, &ppp_profile_name);

	L2TP_DEBUG(L2TP_API, "tunl %s: Default session/ppp profiles from tunnel: '%s/%s'",
		   session->fsmi.name,
		   session_profile_name ? session_profile_name : "NULL",
		   ppp_profile_name ? ppp_profile_name : "NULL");


	if ((profile_name != NULL) && (profile_name[0] != '\0')) {
		L2TP_DEBUG(L2TP_API, "tunl %s: Deriving defaults using session profile '%s'", session->fsmi.name, profile_name);
	} else {
		profile_name = session_profile_name;
	}

	profile = l2tp_session_profile_find(profile_name);
	if (profile == NULL) {
		L2TP_DEBUG(L2TP_API, "Session profile '%s' not found", profile_name);
		return -L2TP_ERR_SESSION_PROFILE_NOT_FOUND;
	}

	/* Override the ppp profile name set in the tunnel with the one set in
	 * the session profile.
	 */
	if ((profile->ppp_profile_name != NULL) &&
	    (strcmp(profile->ppp_profile_name, L2TP_API_PPP_PROFILE_DEFAULT_PROFILE_NAME) != 0)) {
		ppp_profile_name = profile->ppp_profile_name;
	}
	
	l2tp_session_log(session, L2TP_FUNC, LOG_INFO, "FUNC: tunl %s: using session/ppp profiles '%s/%s' for defaults",
			 session->fsmi.name, profile->profile_name, ppp_profile_name);

	if (strcmp(profile->profile_name, L2TP_API_SESSION_PROFILE_DEFAULT_PROFILE_NAME) != 0) {
		session->config.profile_name = strdup(profile->profile_name);
		if (l2tp_test_is_show_profile_usage()) {
			session->config.flags |= L2TP_API_SESSION_FLAG_PROFILE_NAME;
		}
	}
	if (strcmp(ppp_profile_name, L2TP_API_PPP_PROFILE_DEFAULT_PROFILE_NAME) != 0) {
		session->config.ppp_profile_name = strdup(ppp_profile_name);
		if (l2tp_test_is_show_profile_usage()) {
			session->config.flags |= L2TP_API_SESSION_FLAG_PPP_PROFILE_NAME;
		}
	}
	session->config.trace_flags = profile->trace_flags & l2tp_tunnel_get_trace_flags(session->my_tunnel);
	session->config.sequencing_required = profile->sequencing_required;
	session->config.use_sequence_numbers = profile->use_sequence_numbers;
	session->config.no_ppp = profile->no_ppp;
	session->config.reorder_timeout = profile->reorder_timeout;
	session->config.framing_type_sync = profile->framing_type_sync ? -1 : 0;
	session->config.framing_type_async = profile->framing_type_async ? -1 : 0;
	session->config.bearer_type_digital = profile->bearer_type_digital ? -1 : 0;
	session->config.bearer_type_analog = profile->bearer_type_analog ? -1 : 0;
	session->config.minimum_bps = profile->minimum_bps;
	session->config.maximum_bps = profile->maximum_bps;
	session->config.connect_speed = profile->connect_speed;
	session->config.rx_connect_speed = profile->rx_connect_speed;
	session->config.use_ppp_proxy = profile->use_ppp_proxy;
	session->config.mtu = l2tp_tunnel_get_mtu(session->my_tunnel);
	session->config.do_pmtu_discovery = l2tp_tunnel_get_mtu_discovery(session->my_tunnel);
	if (profile->priv_group_id != NULL) {
		session->config.priv_group_id = strdup(profile->priv_group_id);
	}

	return 0;
}

/*****************************************************************************
 * State change hook helpers.

 * These functions drive the plugins, telling them of sessions created,
 * deleted and up/down events. We guarantee that the session lifecycle
 * will see created, up, down and deleted events (in that order).
 *****************************************************************************/

static int l2tp_session_created_ind(struct l2tp_tunnel *tunnel, struct l2tp_session *session)
{
	int result = 0;

	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s", __func__, session->fsmi.name);
	if ((l2tp_session_created_hook != NULL) && (!session->sent_created_event)) {
		session->sent_created_event = 1;
		session->sent_deleted_event = 0;
		l2tp_session_inc_use_count(session);
		result = (*l2tp_session_created_hook)(session, l2tp_tunnel_id(tunnel), session->status.session_id);
		l2tp_session_dec_use_count(session);
	}

	return result;
}

static int l2tp_session_deleted_ind(struct l2tp_tunnel *tunnel, struct l2tp_session *session)
{
	int result = 0;

	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s", __func__, session->fsmi.name);
	if ((l2tp_session_deleted_hook != NULL) && (!session->sent_deleted_event)) {
		session->sent_deleted_event = 1;
		session->sent_created_event = 0;
		l2tp_session_inc_use_count(session);
		result = (*l2tp_session_deleted_hook)(session, l2tp_tunnel_id(tunnel), session->status.session_id);
		l2tp_session_dec_use_count(session);
	}

	return result;
}

static int l2tp_session_down_ind(struct l2tp_tunnel *tunnel, struct l2tp_session *session)
{
	int result = 0;

	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s", __func__, session->fsmi.name);
	if ((l2tp_session_down_hook != NULL) && (!session->sent_down_event)) {
		session->sent_down_event = 1;
		session->sent_up_event = 0;
		l2tp_session_inc_use_count(session);
		result = (*l2tp_session_down_hook)(session, l2tp_tunnel_id(tunnel), session->status.session_id);
		l2tp_session_dec_use_count(session);
	}

	return result;
}

static int l2tp_session_up_ind(struct l2tp_tunnel *tunnel, struct l2tp_session *session)
{
	int result = 0;

	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s", __func__, session->fsmi.name);

	if (session->retry_timer != NULL) {
		/* Session is up so stop retry timer */
		usl_timer_stop(session->retry_timer);
	}
	if ((l2tp_session_up_hook != NULL) && (!session->sent_up_event)) {
		session->sent_up_event = 1;
		session->sent_down_event = 0;
		l2tp_session_inc_use_count(session);
		result = (*l2tp_session_up_hook)(session, l2tp_tunnel_id(tunnel), session->status.session_id,
						 l2tp_tunnel_peer_id(tunnel), session->status.peer_session_id);
		l2tp_session_dec_use_count(session);
	}

	return result;
}


/*****************************************************************************
 * Session context management.
 *****************************************************************************/

static struct l2tp_session *l2tp_session_find_by_id(struct l2tp_tunnel *tunnel, uint16_t session_id)
{
	struct usl_hlist_head *session_list = l2tp_tunnel_session_id_hlist(tunnel, session_id);
	struct usl_hlist_node *tmp;
	struct usl_hlist_node *walk;
	struct l2tp_session *session;

	usl_hlist_for_each(walk, tmp, session_list) {
		session = usl_hlist_entry(walk, struct l2tp_session, session_id_hlist);
		if (session->status.session_id == session_id) {
			l2tp_test_session_id_hash_inc_stats(1);
			return session;
		}
		l2tp_test_session_id_hash_inc_stats(0);
	}

	return NULL;
}

static struct l2tp_session *l2tp_session_find_by_name(struct l2tp_tunnel *tunnel, char *session_name)
{
	struct usl_list_head *session_list = l2tp_tunnel_session_list(tunnel);
	struct usl_list_head *tmp;
	struct usl_list_head *walk;
	struct l2tp_session *session;

	usl_list_for_each(walk, tmp, session_list) {
		session = usl_list_entry(walk, struct l2tp_session, session_list);
		if ((session->config.session_name != NULL) && (strcmp(session->config.session_name, session_name) == 0)) {
			return session;
		}
	}

	return NULL;
}

static struct l2tp_session *l2tp_session_find(uint16_t tunnel_id, uint16_t session_id)
{
	struct l2tp_tunnel *tunnel;

	/* Find the tunnel context */
	if (tunnel_id == 0) {
		return NULL;
	}
	tunnel = l2tp_tunnel_find_by_id(tunnel_id);
	if (tunnel == NULL) {
		return NULL;
	}

	return l2tp_session_find_by_id(tunnel, session_id);
}

static uint16_t l2tp_session_allocate_id(struct l2tp_tunnel *tunnel)
{
	uint16_t session_id;
	int tries;
	struct l2tp_session *session;

	for (tries = 0; tries < 10; tries++) {
		if (!l2tp_test_is_no_random_ids()) {
			session_id = l2tp_make_random_id();
		} else {
			session_id = l2tp_test_alloc_session_id();
		}
		if (session_id == 0) {
			continue;
		}
		session = l2tp_session_find_by_id(tunnel, session_id);
		if (session == NULL) {
			return session_id;
		}
	}

	return 0;
}

/*****************************************************************************
 * Session profiles
 *****************************************************************************/

static struct l2tp_session_profile *l2tp_session_profile_find(const char *name)
{
	struct usl_list_head *tmp;
	struct usl_list_head *walk;
	struct l2tp_session_profile *profile;

	usl_list_for_each(walk, tmp, &l2tp_session_profile_list) {
		profile = usl_list_entry(walk, struct l2tp_session_profile, list);
		if (strcmp(&profile->profile_name[0], name) == 0) {
			return profile;
		}
	}

	return NULL;
}

/* For use by PPP and plugins to obtain session and ppp profile names
 * being used for a given session.
 */
int l2tp_session_profile_names_get(uint16_t tunnel_id, uint16_t session_id, char **session_profile_name, char **ppp_profile_name)
{
	char *default_session_profile_name;
	char *default_ppp_profile_name;
	int result = 0;
	struct l2tp_session *session;
	struct l2tp_tunnel *tunnel;

	if ((tunnel_id == 0) || (session_id == 0)) {
		result = -L2TP_ERR_SESSION_SPEC_MISSING;
		goto out;
	}

	session = l2tp_session_find(tunnel_id, session_id);
	if (session == NULL) {
		result = -L2TP_ERR_SESSION_NOT_FOUND;
		goto out;
	}

	tunnel = session->my_tunnel;
	if (tunnel == NULL) {
		tunnel = l2tp_tunnel_find_by_id(tunnel_id);
		if (tunnel == NULL) {
			result = -L2TP_ERR_TUNNEL_NOT_FOUND;
			goto out;
		}
	}

	l2tp_tunnel_get_profile_names(tunnel, NULL, &default_session_profile_name, &default_ppp_profile_name);
	
	if (session_profile_name != NULL) {
		*session_profile_name = session->config.profile_name ? session->config.profile_name : default_session_profile_name;
	}
	if (ppp_profile_name != NULL) {
		*ppp_profile_name = session->config.ppp_profile_name ? session->config.ppp_profile_name : default_ppp_profile_name;
	}

out:
	return result;
}

/* When a message is received, it is decoded into an AVP array. We
 * come here to store the AVPs in the session context.
 */
static int l2tp_session_store_avps(struct l2tp_session *session, struct l2tp_tunnel *tunnel, 
				   struct l2tp_avp_desc *avps)
{
	int result = 0;
	int avp_len;

	if (avps[L2TP_AVP_TYPE_BEARER_TYPE].value != NULL) {
		session->peer.bearer_type.value = avps[L2TP_AVP_TYPE_BEARER_TYPE].value->bearer_type.value;
	}
	if (avps[L2TP_AVP_TYPE_FRAMING_TYPE].value != NULL) {
		session->peer.framing_type.value = avps[L2TP_AVP_TYPE_FRAMING_TYPE].value->framing_type.value;
	}
	if (avps[L2TP_AVP_TYPE_PHYSICAL_CHANNEL_ID].value != NULL) {
		session->peer.physical_channel_id.value = avps[L2TP_AVP_TYPE_BEARER_TYPE].value->physical_channel_id.value;
	}
	if (avps[L2TP_AVP_TYPE_CALLING_NUMBER].value != NULL) {
		if (session->peer.calling_number != NULL) {
			free(session->peer.calling_number);
		}
		session->peer.calling_number = (void *) strdup(avps[L2TP_AVP_TYPE_CALLING_NUMBER].value->calling_number.string);
		if (session->peer.calling_number == NULL) {
			goto out_nomem;
		}
	}
	if (avps[L2TP_AVP_TYPE_CALLED_NUMBER].value != NULL) {
		if (session->peer.called_number != NULL) {
			free(session->peer.called_number);
		}
		session->peer.called_number = (void *) strdup(avps[L2TP_AVP_TYPE_CALLED_NUMBER].value->called_number.string);
		if (session->peer.called_number == NULL) {
			goto out_nomem;
		}
	}
	if (avps[L2TP_AVP_TYPE_SUB_ADDRESS].value != NULL) {
		if (session->peer.sub_address != NULL) {
			free(session->peer.sub_address);
		}
		session->peer.sub_address = (void *) strdup(avps[L2TP_AVP_TYPE_SUB_ADDRESS].value->sub_address.string);
		if (session->peer.sub_address == NULL) {
			goto out_nomem;
		}
	}
	if (avps[L2TP_AVP_TYPE_Q931_CAUSE_CODE].value != NULL) {
		if (session->peer.q931_cause_code != NULL) {
			free(session->peer.q931_cause_code);
		}
		avp_len = avps[L2TP_AVP_TYPE_Q931_CAUSE_CODE].value_len;
		if (avp_len < 4) avp_len = 4;
		session->peer.q931_cause_code = malloc(avp_len + 1);
		memcpy(session->peer.q931_cause_code, avps[L2TP_AVP_TYPE_Q931_CAUSE_CODE].value, avp_len);
		session->peer.q931_cause_code->advisory_msg[avp_len - 4] = '\0';
	}
	if (avps[L2TP_AVP_TYPE_CALL_SERIAL_NUMBER].value != NULL) {
		session->peer.call_serial_number.value = avps[L2TP_AVP_TYPE_CALL_SERIAL_NUMBER].value->call_serial_number.value;
	}
	if (avps[L2TP_AVP_TYPE_MINIMUM_BPS].value != NULL) {
		session->peer.minimum_bps.value = avps[L2TP_AVP_TYPE_MINIMUM_BPS].value->minimum_bps.value;
	}
	if (avps[L2TP_AVP_TYPE_MAXIMUM_BPS].value != NULL) {
		session->peer.maximum_bps.value = avps[L2TP_AVP_TYPE_MAXIMUM_BPS].value->maximum_bps.value;
	}
	if (avps[L2TP_AVP_TYPE_CONNECT_SPEED].value != NULL) {
		session->peer.connect_speed.value = avps[L2TP_AVP_TYPE_CONNECT_SPEED].value->connect_speed.value;
	}
	if (avps[L2TP_AVP_TYPE_RX_CONNECT_SPEED].value != NULL) {
		session->peer.rx_connect_speed.value = avps[L2TP_AVP_TYPE_RX_CONNECT_SPEED].value->rx_connect_speed.value;
	}
	if (avps[L2TP_AVP_TYPE_PHYSICAL_CHANNEL_ID].value != NULL) {
		session->peer.physical_channel_id.value = avps[L2TP_AVP_TYPE_PHYSICAL_CHANNEL_ID].value->physical_channel_id.value;
	}
	if (avps[L2TP_AVP_TYPE_PRIV_GROUP_ID].value != NULL) {
		if (session->peer.priv_group_id != NULL) {
			free(session->peer.priv_group_id);
		}
		avp_len = avps[L2TP_AVP_TYPE_PRIV_GROUP_ID].value_len;
		session->peer.priv_group_id = malloc(avp_len + 1);
		if (session->peer.priv_group_id == NULL) {
			l2tp_stats.no_avp_resources++;
			goto out_nomem;
		}
		memcpy(&session->peer.priv_group_id->string[0], avps[L2TP_AVP_TYPE_PRIV_GROUP_ID].value, avp_len);
		session->peer.priv_group_id->string[avp_len] = '\0';
	}
	if (avps[L2TP_AVP_TYPE_SEQUENCING_REQUIRED].value != NULL) {
		session->peer.sequencing_required = 1;
	}
	if (avps[L2TP_AVP_TYPE_INITIAL_RCVD_LCP_CONFREQ].value != NULL) {
		if (session->peer.initial_rcvd_lcp_confreq != NULL) {
			free(session->peer.initial_rcvd_lcp_confreq);
		}
		avp_len = avps[L2TP_AVP_TYPE_INITIAL_RCVD_LCP_CONFREQ].value_len;
		session->peer.initial_rcvd_lcp_confreq = malloc(avp_len);
		if (session->peer.initial_rcvd_lcp_confreq == NULL) {
			l2tp_stats.no_avp_resources++;
			goto out_nomem;
		}
		memcpy(&session->peer.initial_rcvd_lcp_confreq[0], avps[L2TP_AVP_TYPE_INITIAL_RCVD_LCP_CONFREQ].value, avp_len);
		session->peer.initial_rcvd_lcp_confreq_len = avp_len;
	}
	if (avps[L2TP_AVP_TYPE_LAST_SENT_LCP_CONFREQ].value != NULL) {
		if (session->peer.last_sent_lcp_confreq != NULL) {
			free(session->peer.last_sent_lcp_confreq);
		}
		avp_len = avps[L2TP_AVP_TYPE_LAST_SENT_LCP_CONFREQ].value_len;
		session->peer.last_sent_lcp_confreq = malloc(avp_len);
		if (session->peer.last_sent_lcp_confreq == NULL) {
			l2tp_stats.no_avp_resources++;
			goto out_nomem;
		}
		memcpy(&session->peer.last_sent_lcp_confreq[0], avps[L2TP_AVP_TYPE_LAST_SENT_LCP_CONFREQ].value, avp_len);
		session->peer.last_sent_lcp_confreq_len = avp_len;
	}
	if (avps[L2TP_AVP_TYPE_LAST_RCVD_LCP_CONFREQ].value != NULL) {
		if (session->peer.last_rcvd_lcp_confreq != NULL) {
			free(session->peer.last_rcvd_lcp_confreq);
		}
		avp_len = avps[L2TP_AVP_TYPE_LAST_RCVD_LCP_CONFREQ].value_len;
		session->peer.last_rcvd_lcp_confreq = malloc(avp_len);
		if (session->peer.last_rcvd_lcp_confreq == NULL) {
			l2tp_stats.no_avp_resources++;
			goto out_nomem;
		}
		memcpy(&session->peer.last_rcvd_lcp_confreq[0], avps[L2TP_AVP_TYPE_LAST_RCVD_LCP_CONFREQ].value, avp_len);
		session->peer.last_rcvd_lcp_confreq_len = avp_len;
	}
	if (avps[L2TP_AVP_TYPE_PROXY_AUTH_TYPE].value != NULL) {
		session->peer.proxy_auth_type.value = avps[L2TP_AVP_TYPE_PROXY_AUTH_TYPE].value->proxy_auth_type.value;
	}
	if (avps[L2TP_AVP_TYPE_PROXY_AUTH_NAME].value != NULL) {
		if (session->peer.proxy_auth_name != NULL) {
			free(session->peer.proxy_auth_name);
		}
		avp_len = avps[L2TP_AVP_TYPE_PROXY_AUTH_NAME].value_len;
		session->peer.proxy_auth_name = malloc(avp_len + 1);
		if (session->peer.proxy_auth_name == NULL) {
			l2tp_stats.no_avp_resources++;
			goto out_nomem;
		}
		memcpy(&session->peer.proxy_auth_name->string[0], avps[L2TP_AVP_TYPE_PROXY_AUTH_NAME].value, avp_len);
		session->peer.proxy_auth_name->string[avp_len] = '\0';
	}
	if (avps[L2TP_AVP_TYPE_PROXY_AUTH_CHALLENGE].value != NULL) {
		if (session->peer.proxy_auth_challenge != NULL) {
			free(session->peer.proxy_auth_challenge);
		}
		avp_len = avps[L2TP_AVP_TYPE_PROXY_AUTH_CHALLENGE].value_len;
		session->peer.proxy_auth_challenge = malloc(avp_len);
		if (session->peer.proxy_auth_challenge == NULL) {
			l2tp_stats.no_avp_resources++;
			goto out_nomem;
		}
		memcpy(&session->peer.proxy_auth_challenge[0], avps[L2TP_AVP_TYPE_PROXY_AUTH_CHALLENGE].value, avp_len);
		session->peer.proxy_auth_challenge_len = avp_len;
	}
	if (avps[L2TP_AVP_TYPE_PROXY_AUTH_ID].value != NULL) {
		session->peer.proxy_auth_id.id = avps[L2TP_AVP_TYPE_PROXY_AUTH_ID].value->proxy_auth_id.id;
	}
	if (avps[L2TP_AVP_TYPE_PROXY_AUTH_RESPONSE].value != NULL) {
		if (session->peer.proxy_auth_response != NULL) {
			free(session->peer.proxy_auth_response);
		}
		avp_len = avps[L2TP_AVP_TYPE_PROXY_AUTH_RESPONSE].value_len;
		session->peer.proxy_auth_response = malloc(avp_len);
		if (session->peer.proxy_auth_response == NULL) {
			l2tp_stats.no_avp_resources++;
			goto out_nomem;
		}
		memcpy(&session->peer.proxy_auth_response[0], avps[L2TP_AVP_TYPE_PROXY_AUTH_RESPONSE].value, avp_len);
		session->peer.proxy_auth_response_len = avp_len;
	}
	if (avps[L2TP_AVP_TYPE_CALL_ERRORS].value != NULL) {
		if (session->peer.call_errors != NULL) {
			free(session->peer.call_errors);
		}
		avp_len = avps[L2TP_AVP_TYPE_CALL_ERRORS].value_len;
		if (avp_len < sizeof(struct l2tp_avp_call_errors)) {
			goto out_badmsg;
		}
		session->peer.call_errors = calloc(1, sizeof(struct l2tp_avp_call_errors));
		if (session->peer.call_errors == NULL) {
			l2tp_stats.no_avp_resources++;
			goto out_nomem;
		}
		session->peer.call_errors->crc_errors = avps[L2TP_AVP_TYPE_CALL_ERRORS].value->call_errors.crc_errors;
		session->peer.call_errors->framing_errors = avps[L2TP_AVP_TYPE_CALL_ERRORS].value->call_errors.framing_errors;
		session->peer.call_errors->hardware_overruns = avps[L2TP_AVP_TYPE_CALL_ERRORS].value->call_errors.hardware_overruns;
		session->peer.call_errors->buffer_overruns = avps[L2TP_AVP_TYPE_CALL_ERRORS].value->call_errors.buffer_overruns;
		session->peer.call_errors->timeout_errors = avps[L2TP_AVP_TYPE_CALL_ERRORS].value->call_errors.timeout_errors;
		session->peer.call_errors->alignment_errors = avps[L2TP_AVP_TYPE_CALL_ERRORS].value->call_errors.alignment_errors;
	}
	if (avps[L2TP_AVP_TYPE_ACCM].value != NULL) {
		if (session->peer.accm != NULL) {
			free(session->peer.accm);
		}
		avp_len = avps[L2TP_AVP_TYPE_ACCM].value_len;
		if (avp_len < sizeof(struct l2tp_avp_accm)) {
			goto out_badmsg;
		}
		session->peer.accm = calloc(1, sizeof(struct l2tp_avp_accm));
		if (session->peer.accm == NULL) {
			l2tp_stats.no_avp_resources++;
			goto out_nomem;
		}
		session->peer.accm->send_accm = avps[L2TP_AVP_TYPE_ACCM].value->accm.send_accm;
		session->peer.accm->recv_accm = avps[L2TP_AVP_TYPE_ACCM].value->accm.recv_accm;
	}

out:
	return result;

out_nomem:
	l2tp_stats.no_session_resources++;
	result = -ENOMEM;
	goto out;

out_badmsg:
	l2tp_stats.bad_rcvd_frames++;
	result = -EBADMSG;
	goto out;
}

/* Timer callback to timeout session establishment.
 */
static void l2tp_session_establish_timer_expired(void *arg)
{
	struct l2tp_session *session = arg;

	l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_NO_ANSWER, 0, NULL);
	l2tp_session_handle_event(session, session->close_event);
}

/*****************************************************************************
 * Message handlers.
 * These are called by tunnel_recv(), registered for each message type 
 * in l2tp_session_init().
 *****************************************************************************/

#ifdef L2TP_FEATURE_LAC_SUPPORT

static void l2tp_session_handle_msg_ocrq(struct l2tp_peer *peer, struct l2tp_tunnel *tunnel,
					 uint16_t session_id, struct l2tp_avp_desc *avps)
{
	int result;
	struct l2tp_session *session = NULL;

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu: OCRQ received from peer %hu", 
			l2tp_tunnel_id(tunnel), l2tp_tunnel_peer_id(tunnel));

	if (session_id != 0) {
		L2TP_DEBUG(L2TP_PROTOCOL, "tunl %hu: OCRQ has unexpected session_id %hu",
			   l2tp_tunnel_id(tunnel), session_id);
		l2tp_stats.messages[L2TP_AVP_MSG_OCRQ].rx_bad++;
		goto out;
	}

	if ((avps[L2TP_AVP_TYPE_SESSION_ID].value == NULL) ||
	    (avps[L2TP_AVP_TYPE_CALL_SERIAL_NUMBER].value == NULL) ||
	    (avps[L2TP_AVP_TYPE_MINIMUM_BPS].value == NULL) ||
	    (avps[L2TP_AVP_TYPE_MAXIMUM_BPS].value == NULL) ||
	    (avps[L2TP_AVP_TYPE_BEARER_TYPE].value == NULL) ||
	    (avps[L2TP_AVP_TYPE_FRAMING_TYPE].value == NULL) ||
	    (avps[L2TP_AVP_TYPE_CALLED_NUMBER].value == NULL)) {
		l2tp_stats.messages[L2TP_AVP_MSG_OCRQ].rx_bad++;
		goto out;
	}

	l2tp_stats.messages[L2TP_AVP_MSG_OCRQ].rx++;

	/* Check that the session doesn't already exist. If it does, check it's the
	 * correct type and then pass an event to the state machine.
	 */
	session = l2tp_session_find_by_id(tunnel, avps[L2TP_AVP_TYPE_SESSION_ID].value->session_id.value);
	if (session != NULL) {
		if (session->type != L2TP_API_SESSION_TYPE_LAOC) {
			l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_NO_ERROR, 
						"OCRQ received by incompatible session type");
			l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_OCRQ, MSG_DENY));
			goto out;
		}
		goto do_event;
	}

	session = l2tp_session_alloc(tunnel, L2TP_API_SESSION_TYPE_LAOC, 0, NULL, 0, &result);
	if (session == NULL) {
		goto out;
	}

	session->status.peer_session_id = avps[L2TP_AVP_TYPE_SESSION_ID].value->session_id.value;
	session->status.call_serial_number = avps[L2TP_AVP_TYPE_CALL_SERIAL_NUMBER].value->call_serial_number.value;

	result = l2tp_session_store_avps(session, tunnel, avps);
	if (result < 0) {
		l2tp_session_set_result_from_error_code(session, result);
		l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_OCRQ, MSG_DENY));
		goto out;
	}

	if (!l2tp_tunnel_can_be_lac(tunnel)) {
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_NOT_AVAILABLE, 0, "LAC support disabled");
		l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_OCRQ, MSG_DENY));
		goto out;
	}

	result = l2tp_session_link(session);
	if (result < 0) {
		l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_OCRQ, MSG_DENY));
		goto out;
	}

	/* Give plugins visibility of session created */
	result = l2tp_session_created_ind(tunnel, session);
	if (result < 0) {
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_NO_ERROR, 
					"Session create completion failed");
		l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_OCRQ, MSG_DENY));
		goto out;
	}

do_event:
	l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_OCRQ, MSG_ACCEPT));
out:
	return;
}

#endif /* L2TP_FEATURE_LAC_SUPPORT */

#ifdef L2TP_FEATURE_LNS_SUPPORT

static void l2tp_session_handle_msg_ocrp(struct l2tp_peer *peer, struct l2tp_tunnel *tunnel,
					 uint16_t session_id, struct l2tp_avp_desc *avps)
{
	struct l2tp_session *session = NULL;
	int result;

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: OCRP received from peer %hu/%hu", 
			l2tp_tunnel_id(tunnel), session_id, l2tp_tunnel_peer_id(tunnel), avps[L2TP_AVP_TYPE_SESSION_ID].value->session_id.value);

	if (session_id == 0) {
		l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_ERR, "PROTO: tunl %hu: OCRP has unexpected session_id %hu",
				l2tp_tunnel_id(tunnel), session_id);
		l2tp_stats.messages[L2TP_AVP_MSG_OCRP].rx_bad++;
		goto out;
	}

	if (avps[L2TP_AVP_TYPE_SESSION_ID].value == NULL) {
		l2tp_stats.messages[L2TP_AVP_MSG_OCRP].rx_bad++;
		goto out;
	}

	l2tp_stats.messages[L2TP_AVP_MSG_OCRP].rx++;

	/* Check that the session already exists. If it does not, ignore the message.
	 */
	session = l2tp_session_find_by_id(tunnel, session_id);
	if (session == NULL) {
		goto out;
	}

	if ((session->type == L2TP_API_SESSION_TYPE_LAIC) || (session->type == L2TP_API_SESSION_TYPE_LNIC)) {
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_NO_ERROR, 
					"OCRP received unexpectedly by Incoming Call context");
		goto error;
	}

	result = l2tp_session_store_avps(session, tunnel, avps);
	if (result < 0) {
		l2tp_session_set_result_from_error_code(session, result);
		goto error;
	}

	l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_OCRP, MSG_ACCEPT));

out:
	return;

error:
	l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_OCRP, MSG_DENY));
}

#endif /* L2TP_FEATURE_LNS_SUPPORT */

#ifdef L2TP_FEATURE_LNS_SUPPORT

static void l2tp_session_handle_msg_occn(struct l2tp_peer *peer, struct l2tp_tunnel *tunnel,
					 uint16_t session_id, struct l2tp_avp_desc *avps)
{
	struct l2tp_session *session;
	int result;

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: OCCN received from peer %hu", 
			l2tp_tunnel_id(tunnel), session_id, l2tp_tunnel_peer_id(tunnel));

	if (session_id == 0) {
		l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_ERR, "PROTO: tunl %hu: OCCN has unexpected session_id %hu",
				l2tp_tunnel_id(tunnel), session_id);
		l2tp_stats.messages[L2TP_AVP_MSG_OCCN].rx_bad++;
		goto out;
	}

	l2tp_stats.messages[L2TP_AVP_MSG_OCCN].rx++;

	/* Check that the session already exists. If it does not, ignore the message.
	 */
	session = l2tp_session_find_by_id(tunnel, session_id);
	if (session == NULL) {
		goto out;
	}

	if ((session->type == L2TP_API_SESSION_TYPE_LAIC) || (session->type == L2TP_API_SESSION_TYPE_LNIC)) {
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_NO_ERROR, 
					"OCCN received unexpectedly by Incoming Call context");
		goto error;
	}

	if ((avps[L2TP_AVP_TYPE_CONNECT_SPEED].value == NULL) ||
	    (avps[L2TP_AVP_TYPE_FRAMING_TYPE].value == NULL)) {
		l2tp_stats.messages[L2TP_AVP_MSG_OCCN].rx_bad++;
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_MBIT_SHUTDOWN, NULL);
		goto error;
	}

	result = l2tp_session_store_avps(session, tunnel, avps);
	if (result < 0) {
		l2tp_session_set_result_from_error_code(session, result);
		goto error;
	}

	l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_OCCN, MSG_ACCEPT));
out:
	return;

error:
	l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_OCCN, MSG_DENY));
}

#endif /* L2TP_FEATURE_LNS_SUPPORT */

#ifdef L2TP_FEATURE_LNS_SUPPORT

static void l2tp_session_handle_msg_icrq(struct l2tp_peer *peer, struct l2tp_tunnel *tunnel,
					 uint16_t session_id, struct l2tp_avp_desc *avps)
{
	int result;
	struct l2tp_session *session = NULL;

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: ICRQ received from peer %hu", 
			l2tp_tunnel_id(tunnel), session_id, l2tp_tunnel_peer_id(tunnel));

	if (session_id != 0) {
		l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_ERR, "PROTO: tunl %hu/%hu: ICRQ has unexpected session_id %d",
				l2tp_tunnel_id(tunnel), session_id, l2tp_tunnel_peer_id(tunnel), session_id);
		goto out;
	}

	if ((avps[L2TP_AVP_TYPE_SESSION_ID].value == NULL) ||
	    (avps[L2TP_AVP_TYPE_CALL_SERIAL_NUMBER].value == NULL)) {
		l2tp_stats.messages[L2TP_AVP_MSG_ICRQ].rx_bad++;
		goto out;
	}

	l2tp_stats.messages[L2TP_AVP_MSG_ICRQ].rx++;

	/* Check that the session doesn't already exist. If it does, check it's the
	 * correct type and then pass an event to the state machine.
	 */
	session = l2tp_session_find_by_id(tunnel, avps[L2TP_AVP_TYPE_SESSION_ID].value->session_id.value);
	if (session != NULL) {
		if (session->type != L2TP_API_SESSION_TYPE_LNIC) {
			l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_NO_ERROR, 
						"ICRQ received by incompatible session type");
			l2tp_session_handle_event(session, session->close_event);
			goto out;
		}
		goto do_event;
	}

	session = l2tp_session_alloc(tunnel, L2TP_API_SESSION_TYPE_LNIC, 0, NULL, 0, &result);
	if (session == NULL) {
		goto out;
	}

	session->status.peer_session_id = avps[L2TP_AVP_TYPE_SESSION_ID].value->session_id.value;
	session->status.call_serial_number = avps[L2TP_AVP_TYPE_CALL_SERIAL_NUMBER].value->call_serial_number.value;

	result = l2tp_session_store_avps(session, tunnel, avps);
	if (result < 0) {
		l2tp_session_set_result_from_error_code(session, result);
		l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_ICRQ, MSG_DENY));
		goto out;
	}

	if (!l2tp_tunnel_can_be_lns(tunnel)) {
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_NOT_AVAILABLE, 0, "LNS support disabled");
		l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_OCRQ, MSG_DENY));
		goto out;
	}

	result = l2tp_session_link(session);
	if (result < 0) {
		l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_ICRQ, MSG_DENY));
		goto out;
	}

	/* Give plugins visibility of session created */
	result = l2tp_session_created_ind(tunnel, session);
	if (result < 0) {
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_NO_ERROR, 
					"Session create completion failed");
		l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_ICRQ, MSG_DENY));
		goto out;
	}

do_event:
	l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_ICRQ, MSG_ACCEPT));
out:
	return;
}

#endif /* L2TP_FEATURE_LNS_SUPPORT */

#ifdef L2TP_FEATURE_LAC_SUPPORT

static void l2tp_session_handle_msg_icrp(struct l2tp_peer *peer, struct l2tp_tunnel *tunnel,
					 uint16_t session_id, struct l2tp_avp_desc *avps)
{
	int result;
	struct l2tp_session *session = NULL;

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: ICRP received from peer %hu", 
			l2tp_tunnel_id(tunnel), session_id, l2tp_tunnel_peer_id(tunnel));

	if (avps[L2TP_AVP_TYPE_SESSION_ID].value == NULL) {
		l2tp_stats.messages[L2TP_AVP_MSG_ICRP].rx_bad++;
		goto out;
	}

	if (session_id == 0) {
		l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_ERR, "PROTO: tunl %hu/%hu: ICRP from peer %hu has unexpected session_id 0",
			   l2tp_tunnel_id(tunnel), session_id, avps[L2TP_AVP_TYPE_SESSION_ID].value);
		l2tp_stats.messages[L2TP_AVP_MSG_ICRP].rx_bad++;
		goto out;
	}

	l2tp_stats.messages[L2TP_AVP_MSG_ICRP].rx++;

	session = l2tp_session_find_by_id(tunnel, session_id);
	if (session == NULL) {
		l2tp_stats.no_matching_session_id_discards++;
		l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: Dropping ICRP - no session context for session", 
				l2tp_tunnel_id(tunnel), session_id);
		goto out;
	}

	if ((session->type == L2TP_API_SESSION_TYPE_LAOC) || (session->type == L2TP_API_SESSION_TYPE_LNOC)) {
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_NO_ERROR, 
					"ICRP received unexpectedly by Outgoing Call context");
		goto error;
	}

	session->status.peer_session_id = avps[L2TP_AVP_TYPE_SESSION_ID].value->session_id.value;

	result = l2tp_session_store_avps(session, tunnel, avps);
	if (result < 0) {
		l2tp_stats.no_control_frame_resources++;
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_NO_RESOURCES, 0, NULL);
		goto error;
	}

	l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_ICRP, MSG_ACCEPT));
out:
	return;

error:
	l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_ICRP, MSG_DENY));
}

#endif /* L2TP_FEATURE_LAC_SUPPORT */

#ifdef L2TP_FEATURE_LNS_SUPPORT

static void l2tp_session_handle_msg_iccn(struct l2tp_peer *peer, struct l2tp_tunnel *tunnel,
					 uint16_t session_id, struct l2tp_avp_desc *avps)
{
	int result;
	struct l2tp_session *session;

	l2tp_stats.messages[L2TP_AVP_MSG_ICCN].rx++;

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: ICCN received from peer %hu", 
			l2tp_tunnel_id(tunnel), session_id, l2tp_tunnel_peer_id(tunnel));

	session = l2tp_session_find_by_id(tunnel, session_id);
	if (session == NULL) {
		l2tp_stats.no_matching_session_id_discards++;
		l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu, Dropping ICCN - no session context for session", 
				l2tp_tunnel_id(tunnel), session_id);
		goto out;
	}

	result = l2tp_session_store_avps(session, tunnel, avps);
	if (result < 0) {
		l2tp_stats.no_control_frame_resources++;
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_NO_RESOURCES, 0, NULL);
		l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_ICCN, MSG_DENY));
		goto out;
	}

	l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_ICCN, MSG_ACCEPT));

out:
	return;
}

#endif /* L2TP_FEATURE_LNS_SUPPORT */

static void l2tp_session_handle_msg_cdn(struct l2tp_peer *peer, struct l2tp_tunnel *tunnel,
					uint16_t session_id, struct l2tp_avp_desc *avps)
{
	struct l2tp_session *session;

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: CDN received from peer %hu", 
			l2tp_tunnel_id(tunnel), session_id, l2tp_tunnel_peer_id(tunnel));

	l2tp_stats.messages[L2TP_AVP_MSG_CDN].rx++;

	session = l2tp_session_find_by_id(tunnel, session_id);
	if (session == NULL) {
		/* Session context has already been deleted. Silently ignore. */
		goto out;
	}

	if ((avps[L2TP_AVP_TYPE_SESSION_ID].value == NULL) ||
	    (avps[L2TP_AVP_TYPE_RESULT_CODE].value == NULL)) {
		l2tp_stats.messages[L2TP_AVP_MSG_CDN].rx_bad++;
		goto out;
	}
	if (avps[L2TP_AVP_TYPE_SESSION_ID].value->session_id.value != session->status.peer_session_id) {
		l2tp_stats.mismatched_session_ids++;
		goto out;
	}
	if (avps[L2TP_AVP_TYPE_RESULT_CODE].value != NULL) {
		if (session->result_code != NULL) {
			free(session->result_code);
		}
		session->result_code_len = avps[L2TP_AVP_TYPE_RESULT_CODE].value_len;
		session->result_code = malloc(session->result_code_len + 1);
		if (session->result_code == NULL) {
			l2tp_stats.no_control_frame_resources++;
			l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_NO_RESOURCES, 0, NULL);
			goto out2;
		}
		memcpy(session->result_code, avps[L2TP_AVP_TYPE_RESULT_CODE].value, session->result_code_len);
		if (session->result_code_len >= 4) {
			session->result_code->error_message[session->result_code_len - 4] = '\0';
		}
	}

	l2tp_session_log_error(session, session->result_code, session->result_code_len);
out2:
	l2tp_session_handle_event(session, l2tp_session_map_event(session->type, L2TP_AVP_MSG_CDN, MSG_ACCEPT));
out:
	return;
}

#ifdef L2TP_FEATURE_LNS_SUPPORT

static void l2tp_session_handle_msg_wen(struct l2tp_peer *peer, struct l2tp_tunnel *tunnel,
					uint16_t session_id, struct l2tp_avp_desc *avps)
{
	l2tp_stats.messages[L2TP_AVP_MSG_WEN].rx++;

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: WEN received from peer %hu", 
			l2tp_tunnel_id(tunnel), session_id, l2tp_tunnel_peer_id(tunnel));

	/* FIXME: what do we need to do with this message? */
}

#endif /* L2TP_FEATURE_LNS_SUPPORT */

#ifdef L2TP_FEATURE_LAC_SUPPORT

static void l2tp_session_handle_msg_sli(struct l2tp_peer *peer, struct l2tp_tunnel *tunnel,
					uint16_t session_id, struct l2tp_avp_desc *avps)
{
	l2tp_stats.messages[L2TP_AVP_MSG_SLI].rx++;

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: SLI received from peer %hu", 
			l2tp_tunnel_id(tunnel), session_id, l2tp_tunnel_peer_id(tunnel));

	/* FIXME: what do we need to do with this message? */
}

#endif /* L2TP_FEATURE_LAC_SUPPORT */

/*****************************************************************************
 * Control message send helpers
 *****************************************************************************/

#ifdef L2TP_FEATURE_LAIC_SUPPORT

static int l2tp_session_send_icrq(void *tun, void *sess)
{
	struct l2tp_tunnel *tunnel = tun;
	struct l2tp_session *session = sess;
	struct l2tp_avp_desc avps[L2TP_AVP_TYPE_NUM_AVPS];
	struct l2tp_avp_message_type msg_type;
	struct l2tp_avp_session_id session_id;
	struct l2tp_avp_call_serial_number call_serial_number;
	struct l2tp_avp_bearer_type bearer_type;
	struct l2tp_avp_physical_channel_id physical_channel_id;
	struct l2tp_packet *pkt;
	int result;

	memset(&avps, 0, sizeof(avps));

	/* First, setup the mandatory AVPs */
	msg_type.type = L2TP_AVP_TYPE_MESSAGE;
	avps[L2TP_AVP_TYPE_MESSAGE].value = (void *) &msg_type;
	avps[L2TP_AVP_TYPE_MESSAGE].value_len = sizeof(msg_type);
	session_id.value = session->status.session_id;
	avps[L2TP_AVP_TYPE_SESSION_ID].value = (void *) &session_id;
	avps[L2TP_AVP_TYPE_SESSION_ID].value_len = sizeof(session_id);
	call_serial_number.value = session->status.call_serial_number;
	avps[L2TP_AVP_TYPE_CALL_SERIAL_NUMBER].value = (void *) &call_serial_number;
	avps[L2TP_AVP_TYPE_CALL_SERIAL_NUMBER].value_len = sizeof(call_serial_number);

	/* Now the optional AVPs */
	bearer_type.value = 0;
	bearer_type.bits.digital = session->config.bearer_type_digital ? -1 : 0;
	bearer_type.bits.analog = session->config.bearer_type_analog ? -1 : 0;
	avps[L2TP_AVP_TYPE_BEARER_TYPE].value = (void *) &bearer_type;
	avps[L2TP_AVP_TYPE_BEARER_TYPE].value_len = sizeof(bearer_type);
	if (session->status.physical_channel_id != 0) {
		physical_channel_id.value = session->status.physical_channel_id;
		avps[L2TP_AVP_TYPE_PHYSICAL_CHANNEL_ID].value = (void *) &physical_channel_id;
		avps[L2TP_AVP_TYPE_PHYSICAL_CHANNEL_ID].value_len = sizeof(physical_channel_id);
	}
	if (session->config.calling_number != NULL) {
		avps[L2TP_AVP_TYPE_CALLING_NUMBER].value = (void *) session->config.calling_number;
		avps[L2TP_AVP_TYPE_CALLING_NUMBER].value_len = strlen(session->config.calling_number);
	}
	if (session->config.called_number != NULL) {
		avps[L2TP_AVP_TYPE_CALLED_NUMBER].value = (void *) session->config.called_number;
		avps[L2TP_AVP_TYPE_CALLED_NUMBER].value_len = strlen(session->config.called_number);
	}
	if (session->config.sub_address != NULL) {
		avps[L2TP_AVP_TYPE_SUB_ADDRESS].value = (void *) session->config.sub_address;
		avps[L2TP_AVP_TYPE_SUB_ADDRESS].value_len = strlen(session->config.sub_address);
	}
	
	/* build and send ICRQ */
	result = l2tp_avp_message_encode(L2TP_AVP_MSG_ICRQ, &pkt, l2tp_tunnel_is_hide_avps(tunnel), avps, tunnel);
	if (result < 0) {
		l2tp_stats.encode_message_fails++;
		goto error;
	}

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: sending ICRQ to peer %hu/0", 
			l2tp_tunnel_id(tunnel), session_id, l2tp_tunnel_peer_id(tunnel));

	result = l2tp_net_send(tunnel, l2tp_tunnel_peer_id(tunnel), 0, pkt, L2TP_AVP_MSG_ICRQ);

out:
	return result;

error:
	if (pkt != NULL) {
		l2tp_pkt_free(pkt);
	}
	goto out;
}

#endif /* L2TP_FEATURE_LAIC_SUPPORT */

#ifdef L2TP_FEATURE_LAIC_SUPPORT

static int l2tp_session_send_iccn(void *tun, void *sess)
{
	struct l2tp_tunnel *tunnel = tun;
	struct l2tp_session *session = sess;
	struct l2tp_avp_desc avps[L2TP_AVP_TYPE_NUM_AVPS];
	struct l2tp_avp_message_type msg_type;
	struct l2tp_avp_connect_speed connect_speed;
	struct l2tp_avp_framing_type framing_type;
	struct l2tp_avp_proxy_auth_type proxy_auth_type;
	struct l2tp_avp_proxy_auth_id proxy_auth_id;
	struct l2tp_avp_rx_connect_speed rx_connect_speed;
	struct l2tp_avp_sequencing_required sequencing_required;
	struct l2tp_packet *pkt;
	int result;

	memset(&avps, 0, sizeof(avps));

	/* First, setup the mandatory AVPs */
	msg_type.type = L2TP_AVP_TYPE_MESSAGE;
	avps[L2TP_AVP_TYPE_MESSAGE].value = (void *) &msg_type;
	avps[L2TP_AVP_TYPE_MESSAGE].value_len = sizeof(msg_type);
	connect_speed.value = session->config.connect_speed;
	avps[L2TP_AVP_TYPE_CONNECT_SPEED].value = (void *) &connect_speed;
	avps[L2TP_AVP_TYPE_CONNECT_SPEED].value_len = sizeof(connect_speed);
	framing_type.value = 0;
	framing_type.bits.async = session->config.framing_type_async ? -1 : 0;
	framing_type.bits.sync = session->config.framing_type_sync ? -1 : 0;
	avps[L2TP_AVP_TYPE_FRAMING_TYPE].value = (void *) &framing_type;
	avps[L2TP_AVP_TYPE_FRAMING_TYPE].value_len = sizeof(framing_type);

	/* Now the optional AVPs */
	if (session->config.initial_rcvd_lcp_confreq_len != 0) {
		avps[L2TP_AVP_TYPE_INITIAL_RCVD_LCP_CONFREQ].value = (void *) session->config.initial_rcvd_lcp_confreq;
		avps[L2TP_AVP_TYPE_INITIAL_RCVD_LCP_CONFREQ].value_len = session->config.initial_rcvd_lcp_confreq_len;
	}
	if (session->config.last_sent_lcp_confreq_len != 0) {
		avps[L2TP_AVP_TYPE_LAST_SENT_LCP_CONFREQ].value = (void *) session->config.last_sent_lcp_confreq;
		avps[L2TP_AVP_TYPE_LAST_SENT_LCP_CONFREQ].value_len = session->config.last_sent_lcp_confreq_len;
	}
	if (session->config.last_rcvd_lcp_confreq_len != 0) {
		avps[L2TP_AVP_TYPE_LAST_RCVD_LCP_CONFREQ].value = (void *) session->config.last_rcvd_lcp_confreq;
		avps[L2TP_AVP_TYPE_LAST_RCVD_LCP_CONFREQ].value_len = session->config.last_rcvd_lcp_confreq_len;
	}
	if (session->config.proxy_auth_type != 0) {
		proxy_auth_type.value = session->config.proxy_auth_type;
		avps[L2TP_AVP_TYPE_PROXY_AUTH_TYPE].value = (void *) &proxy_auth_type;
		avps[L2TP_AVP_TYPE_PROXY_AUTH_TYPE].value_len = sizeof(proxy_auth_type);
	}
	if (session->config.proxy_auth_name != NULL) {
		avps[L2TP_AVP_TYPE_PROXY_AUTH_NAME].value = (void *) session->config.proxy_auth_name;
		avps[L2TP_AVP_TYPE_PROXY_AUTH_NAME].value_len = strlen(session->config.proxy_auth_name);
	}
	if ((session->config.proxy_auth_challenge_len > 0) && (session->config.proxy_auth_challenge != NULL)) {
		avps[L2TP_AVP_TYPE_PROXY_AUTH_CHALLENGE].value = (void *) session->config.proxy_auth_challenge;
		avps[L2TP_AVP_TYPE_PROXY_AUTH_CHALLENGE].value_len = strlen((const char *) session->config.proxy_auth_challenge);
	}
	if (session->config.proxy_auth_id != 0) {
		proxy_auth_id.id = session->config.proxy_auth_id;
		proxy_auth_id.reserved = 0;
		avps[L2TP_AVP_TYPE_PROXY_AUTH_ID].value = (void *) &proxy_auth_id;
		avps[L2TP_AVP_TYPE_PROXY_AUTH_ID].value_len = sizeof(proxy_auth_id);
	}
	if ((session->config.proxy_auth_response_len > 0) && (session->config.proxy_auth_response != NULL)) {
		avps[L2TP_AVP_TYPE_PROXY_AUTH_RESPONSE].value = (void *) session->config.proxy_auth_response;
		avps[L2TP_AVP_TYPE_PROXY_AUTH_RESPONSE].value_len = strlen((const char *) session->config.proxy_auth_response);
	}
	if (session->config.priv_group_id != 0) {
		avps[L2TP_AVP_TYPE_PRIV_GROUP_ID].value = (void *) session->config.priv_group_id;
		avps[L2TP_AVP_TYPE_PRIV_GROUP_ID].value_len = strlen(session->config.priv_group_id);
	}
	if (session->config.rx_connect_speed != 0) {
		rx_connect_speed.value = session->config.rx_connect_speed;
		avps[L2TP_AVP_TYPE_RX_CONNECT_SPEED].value = (void *) &rx_connect_speed;
		avps[L2TP_AVP_TYPE_RX_CONNECT_SPEED].value_len = sizeof(rx_connect_speed);
	}
	if (session->config.sequencing_required != 0) {
		avps[L2TP_AVP_TYPE_SEQUENCING_REQUIRED].value = (void *) &sequencing_required;
		avps[L2TP_AVP_TYPE_SEQUENCING_REQUIRED].value_len = 0;
	}
	
	/* build and send ICCN */
	result = l2tp_avp_message_encode(L2TP_AVP_MSG_ICCN, &pkt, l2tp_tunnel_is_hide_avps(tunnel), avps, tunnel);
	if (result < 0) {
		l2tp_stats.encode_message_fails++;
		goto error;
	}

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: sending ICCN to peer %hu/%hu", 
			l2tp_tunnel_id(tunnel), session->status.session_id, l2tp_tunnel_peer_id(tunnel),
			session->status.peer_session_id);

	result = l2tp_net_send(tunnel, l2tp_tunnel_peer_id(tunnel), session->status.peer_session_id, pkt, L2TP_AVP_MSG_ICCN);

	/* Stop session establish timer */
	if (session->timer != NULL) {
		usl_timer_delete(session->timer);
		session->timer = NULL;
	}

	/* Give plugins visibility of session up */
	result = l2tp_session_up_ind(tunnel, session);
	if (result < 0) {
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_VENDOR_ERROR, 
					"failed to start ppp");
	}

out:
	return result;

error:
	if (pkt != NULL) {
		l2tp_pkt_free(pkt);
	}
	goto out;
}

#endif /* L2TP_FEATURE_LAIC_SUPPORT */

static int l2tp_session_send_cdn(void *tun, void *sess)
{
	struct l2tp_tunnel *tunnel = tun;
	struct l2tp_session *session = sess;
	struct l2tp_avp_desc avps[L2TP_AVP_TYPE_NUM_AVPS];
	struct l2tp_avp_message_type msg_type;
	struct l2tp_avp_session_id session_id;
	struct l2tp_avp_q931_cause_code q931_cause_code;
	struct l2tp_packet *pkt;
	int result;

	memset(&avps, 0, sizeof(avps));

	/* If no result_code has been recorded, set one for "normal close" */
	if (session->result_code == NULL) {
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_ADMIN, L2TP_AVP_ERROR_NO_ERROR, NULL);
	}

	/* First, setup the mandatory AVPs */
	msg_type.type = L2TP_AVP_TYPE_MESSAGE;
	avps[L2TP_AVP_TYPE_MESSAGE].value = (void *) &msg_type;
	avps[L2TP_AVP_TYPE_MESSAGE].value_len = sizeof(msg_type);
	avps[L2TP_AVP_TYPE_RESULT_CODE].value = (void *) session->result_code;
	avps[L2TP_AVP_TYPE_RESULT_CODE].value_len = session->result_code_len;
	session_id.value = session->status.session_id;
	avps[L2TP_AVP_TYPE_SESSION_ID].value = (void *) &session_id;
	avps[L2TP_AVP_TYPE_SESSION_ID].value_len = sizeof(session_id);

	/* Now the optional AVPs */
	if (session->q931_cause_code != NULL) {
		q931_cause_code.cause_code = session->q931_cause_code->cause_code;
		q931_cause_code.cause_msg = session->q931_cause_code->cause_msg;
		avps[L2TP_AVP_TYPE_Q931_CAUSE_CODE].value = (void *) session->q931_cause_code;
		avps[L2TP_AVP_TYPE_Q931_CAUSE_CODE].value_len = session->q931_cause_code_len;
	}
	
	/* build and send CDN */
	result = l2tp_avp_message_encode(L2TP_AVP_MSG_CDN, &pkt, l2tp_tunnel_is_hide_avps(tunnel), avps, tunnel);
	if (result < 0) {
		l2tp_stats.encode_message_fails++;
		goto error;
	}

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: sending CDN to peer %hu/%hu", 
			l2tp_tunnel_id(tunnel), session_id, l2tp_tunnel_peer_id(tunnel),
			session->status.peer_session_id);

	result = l2tp_net_send(tunnel, l2tp_tunnel_peer_id(tunnel), session->status.peer_session_id, pkt, L2TP_AVP_MSG_CDN);

out:
	return result;

error:
	if (pkt != NULL) {
		l2tp_pkt_free(pkt);
	}

	goto out;
}

#ifdef L2TP_FEATURE_LNIC_SUPPORT

static int l2tp_session_send_icrp(void *tun, void *sess)
{
	struct l2tp_tunnel *tunnel = tun;
	struct l2tp_session *session = sess;
	struct l2tp_avp_desc avps[L2TP_AVP_TYPE_NUM_AVPS];
	struct l2tp_avp_message_type msg_type;
	struct l2tp_avp_session_id session_id;
	struct l2tp_packet *pkt;
	int result;

	memset(&avps, 0, sizeof(avps));

	/* First, setup the mandatory AVPs */
	msg_type.type = L2TP_AVP_TYPE_MESSAGE;
	avps[L2TP_AVP_TYPE_MESSAGE].value = (void *) &msg_type;
	avps[L2TP_AVP_TYPE_MESSAGE].value_len = sizeof(msg_type);
	session_id.value = session->status.session_id;
	avps[L2TP_AVP_TYPE_SESSION_ID].value = (void *) &session_id;
	avps[L2TP_AVP_TYPE_SESSION_ID].value_len = sizeof(session_id);
	
	/* build and send ICRP */
	result = l2tp_avp_message_encode(L2TP_AVP_MSG_ICRP, &pkt, l2tp_tunnel_is_hide_avps(tunnel), avps, tunnel);
	if (result < 0) {
		l2tp_stats.encode_message_fails++;
		goto error;
	}

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: sending ICRP to peer %hu/%hu", 
			l2tp_tunnel_id(tunnel), session_id, l2tp_tunnel_peer_id(tunnel), session->status.peer_session_id);

	result = l2tp_net_send(tunnel, l2tp_tunnel_peer_id(tunnel), session->status.peer_session_id, pkt, L2TP_AVP_MSG_ICRP);

out:
	return result;

error:
	if (pkt != NULL) {
		l2tp_pkt_free(pkt);
	}
	goto out;
}

#endif /* L2TP_FEATURE_LNIC_SUPPORT */

#ifdef L2TP_FEATURE_LAOC_SUPPORT

static int l2tp_session_send_ocrp(void *tun, void *sess)
{
	struct l2tp_tunnel *tunnel = tun;
	struct l2tp_session *session = sess;
	struct l2tp_avp_desc avps[L2TP_AVP_TYPE_NUM_AVPS];
	struct l2tp_avp_message_type msg_type;
	struct l2tp_avp_session_id session_id;
	struct l2tp_avp_physical_channel_id physical_channel_id;
	struct l2tp_packet *pkt;
	int result;

	memset(&avps, 0, sizeof(avps));

	/* First, setup the mandatory AVPs */
	msg_type.type = L2TP_AVP_TYPE_MESSAGE;
	avps[L2TP_AVP_TYPE_MESSAGE].value = (void *) &msg_type;
	avps[L2TP_AVP_TYPE_MESSAGE].value_len = sizeof(msg_type);
	session_id.value = session->status.session_id;
	avps[L2TP_AVP_TYPE_SESSION_ID].value = (void *) &session_id;
	avps[L2TP_AVP_TYPE_SESSION_ID].value_len = sizeof(session_id);

	/* Now the optional AVPs */
	if (session->status.physical_channel_id != 0) {
		physical_channel_id.value = session->status.physical_channel_id;
		avps[L2TP_AVP_TYPE_PHYSICAL_CHANNEL_ID].value = (void *) &physical_channel_id;
		avps[L2TP_AVP_TYPE_PHYSICAL_CHANNEL_ID].value_len = sizeof(physical_channel_id);
	}
	
	/* build and send OCRQ */
	result = l2tp_avp_message_encode(L2TP_AVP_MSG_OCRP, &pkt, l2tp_tunnel_is_hide_avps(tunnel), avps, tunnel);
	if (result < 0) {
		l2tp_stats.encode_message_fails++;
		goto error;
	}

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: sending OCRP to peer %hu/%hu", 
			l2tp_tunnel_id(tunnel), session_id, l2tp_tunnel_peer_id(tunnel), session->status.peer_session_id);

	result = l2tp_net_send(tunnel, l2tp_tunnel_peer_id(tunnel), session->status.peer_session_id, pkt, L2TP_AVP_MSG_OCRP);

out:
	return result;

error:
	if (pkt != NULL) {
		l2tp_pkt_free(pkt);
	}
	goto out;
}

#endif /* L2TP_FEATURE_LAOC_SUPPORT */

#ifdef L2TP_FEATURE_LAOC_SUPPORT

static int l2tp_session_send_occn(void *tun, void *sess)
{
	struct l2tp_tunnel *tunnel = tun;
	struct l2tp_session *session = sess;
	struct l2tp_avp_desc avps[L2TP_AVP_TYPE_NUM_AVPS];
	struct l2tp_avp_message_type msg_type;
	struct l2tp_avp_connect_speed connect_speed;
	struct l2tp_avp_framing_type framing_type;
	struct l2tp_avp_rx_connect_speed rx_connect_speed;
	struct l2tp_avp_sequencing_required sequencing_required;
	struct l2tp_packet *pkt;
	int result;

	memset(&avps, 0, sizeof(avps));

	/* First, setup the mandatory AVPs */
	msg_type.type = L2TP_AVP_TYPE_MESSAGE;
	avps[L2TP_AVP_TYPE_MESSAGE].value = (void *) &msg_type;
	avps[L2TP_AVP_TYPE_MESSAGE].value_len = sizeof(msg_type);
	connect_speed.value = session->config.connect_speed;
	avps[L2TP_AVP_TYPE_CONNECT_SPEED].value = (void *) &connect_speed;
	avps[L2TP_AVP_TYPE_CONNECT_SPEED].value_len = sizeof(connect_speed);
	framing_type.value = 0;
	framing_type.bits.async = session->config.framing_type_async ? -1 : 0;
	framing_type.bits.sync = session->config.framing_type_sync ? -1 : 0;
	avps[L2TP_AVP_TYPE_FRAMING_TYPE].value = (void *) &framing_type;
	avps[L2TP_AVP_TYPE_FRAMING_TYPE].value_len = sizeof(framing_type);

	/* Now the optional AVPs */
	if (session->config.rx_connect_speed != 0) {
		rx_connect_speed.value = session->config.rx_connect_speed;
		avps[L2TP_AVP_TYPE_RX_CONNECT_SPEED].value = (void *) &rx_connect_speed;
		avps[L2TP_AVP_TYPE_RX_CONNECT_SPEED].value_len = sizeof(rx_connect_speed);
	}
	if (session->config.sequencing_required != 0) {
		avps[L2TP_AVP_TYPE_SEQUENCING_REQUIRED].value = (void *) &sequencing_required;
		avps[L2TP_AVP_TYPE_SEQUENCING_REQUIRED].value_len = 0;
	}
	
	/* build and send OCCN */
	result = l2tp_avp_message_encode(L2TP_AVP_MSG_OCCN, &pkt, l2tp_tunnel_is_hide_avps(tunnel), avps, tunnel);
	if (result < 0) {
		l2tp_stats.encode_message_fails++;
		goto error;
	}

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: sending OCCN to peer %hu/%hu", 
			l2tp_tunnel_id(tunnel), session->status.session_id, l2tp_tunnel_peer_id(tunnel),
			session->status.peer_session_id);

	result = l2tp_net_send(tunnel, l2tp_tunnel_peer_id(tunnel), session->status.peer_session_id, pkt, L2TP_AVP_MSG_OCCN);

out:
	return result;

error:
	if (pkt != NULL) {
		l2tp_pkt_free(pkt);
	}
	goto out;
}

#endif /* L2TP_FEATURE_LAOC_SUPPORT */

#ifdef L2TP_FEATURE_LNOC_SUPPORT

static int l2tp_session_send_ocrq(void *tun, void *sess)
{
	struct l2tp_tunnel *tunnel = tun;
	struct l2tp_session *session = sess;
	struct l2tp_avp_desc avps[L2TP_AVP_TYPE_NUM_AVPS];
	struct l2tp_avp_message_type msg_type;
	struct l2tp_avp_session_id session_id;
	struct l2tp_avp_call_serial_number call_serial_number;
	struct l2tp_avp_minimum_bps minimum_bps;
	struct l2tp_avp_maximum_bps maximum_bps;
	struct l2tp_avp_bearer_type bearer_type;
	struct l2tp_avp_framing_type framing_type;
	struct l2tp_packet *pkt;
	int result = -EBADMSG;

	memset(&avps, 0, sizeof(avps));

	/* First, setup the mandatory AVPs */
	msg_type.type = L2TP_AVP_TYPE_MESSAGE;
	avps[L2TP_AVP_TYPE_MESSAGE].value = (void *) &msg_type;
	avps[L2TP_AVP_TYPE_MESSAGE].value_len = sizeof(msg_type);
	session_id.value = session->status.session_id;
	avps[L2TP_AVP_TYPE_SESSION_ID].value = (void *) &session_id;
	avps[L2TP_AVP_TYPE_SESSION_ID].value_len = sizeof(session_id);
	call_serial_number.value = session->status.call_serial_number;
	avps[L2TP_AVP_TYPE_CALL_SERIAL_NUMBER].value = (void *) &call_serial_number;
	avps[L2TP_AVP_TYPE_CALL_SERIAL_NUMBER].value_len = sizeof(call_serial_number);
	minimum_bps.value = session->config.minimum_bps;
	avps[L2TP_AVP_TYPE_MINIMUM_BPS].value = (void *) &minimum_bps;
	avps[L2TP_AVP_TYPE_MINIMUM_BPS].value_len = sizeof(minimum_bps);
	maximum_bps.value = session->config.maximum_bps;
	avps[L2TP_AVP_TYPE_MAXIMUM_BPS].value = (void *) &maximum_bps;
	avps[L2TP_AVP_TYPE_MAXIMUM_BPS].value_len = sizeof(maximum_bps);
	bearer_type.value = 0;
	bearer_type.bits.digital = session->config.bearer_type_digital ? -1 : 0;
	bearer_type.bits.analog = session->config.bearer_type_analog ? -1 : 0;
	avps[L2TP_AVP_TYPE_BEARER_TYPE].value = (void *) &bearer_type;
	avps[L2TP_AVP_TYPE_BEARER_TYPE].value_len = sizeof(bearer_type);
	framing_type.value = 0;
	framing_type.bits.async = session->config.framing_type_async ? -1 : 0;
	framing_type.bits.sync = session->config.framing_type_sync ? -1 : 0;
	avps[L2TP_AVP_TYPE_FRAMING_TYPE].value = (void *) &framing_type;
	avps[L2TP_AVP_TYPE_FRAMING_TYPE].value_len = sizeof(framing_type);

	/* Now the optional AVPs */
	if (session->config.called_number == NULL) {
		session->config.called_number = strdup("");
		if (session->config.called_number == NULL) {
			l2tp_stats.no_session_resources++;
			result = -ENOMEM;
			goto error;
		}
	}
	avps[L2TP_AVP_TYPE_CALLED_NUMBER].value = (void *) session->config.called_number;
	avps[L2TP_AVP_TYPE_CALLED_NUMBER].value_len = strlen(session->config.called_number);

	if (session->config.sub_address != 0) {
		avps[L2TP_AVP_TYPE_SUB_ADDRESS].value = (void *) session->config.sub_address;
		avps[L2TP_AVP_TYPE_SUB_ADDRESS].value_len = strlen(session->config.sub_address);
	}
	
	/* build and send OCRQ */
	result = l2tp_avp_message_encode(L2TP_AVP_MSG_OCRQ, &pkt, l2tp_tunnel_is_hide_avps(tunnel), avps, tunnel);
	if (result < 0) {
		l2tp_stats.encode_message_fails++;
		goto error;
	}

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: sending OCRQ to peer %hu/0", 
			l2tp_tunnel_id(tunnel), session_id, l2tp_tunnel_peer_id(tunnel));

	result = l2tp_net_send(tunnel, l2tp_tunnel_peer_id(tunnel), 0, pkt, L2TP_AVP_MSG_OCRQ);

out:
	return result;

error:
	if (pkt != NULL) {
		l2tp_pkt_free(pkt);
	}
	goto out;
}

#endif /* L2TP_FEATURE_LNOC_SUPPORT */

static int l2tp_session_send_sli(void *tun, void *sess)
{
#ifdef L2TP_FEATURE_LNS_SUPPORT
	struct l2tp_tunnel *tunnel = tun;
	struct l2tp_session *session = sess;
	struct l2tp_avp_desc avps[L2TP_AVP_TYPE_NUM_AVPS];
	struct l2tp_avp_message_type msg_type;
	struct l2tp_avp_accm accm;

	struct l2tp_packet *pkt;
	int result = 0;

	/* Only LNS send SLI */
	if ((session->type == L2TP_API_SESSION_TYPE_LAIC) ||
	    (session->type == L2TP_API_SESSION_TYPE_LAOC)) {
		goto out;
	}

	memset(&avps, 0, sizeof(avps));

	/* First, setup the mandatory AVPs */
	msg_type.type = L2TP_AVP_TYPE_MESSAGE;
	avps[L2TP_AVP_TYPE_MESSAGE].value = (void *) &msg_type;
	avps[L2TP_AVP_TYPE_MESSAGE].value_len = sizeof(msg_type);
	memset(&accm, 0, sizeof(accm));
	avps[L2TP_AVP_TYPE_ACCM].value = (void *) &accm;
	avps[L2TP_AVP_TYPE_ACCM].value_len = sizeof(accm);
	avps[L2TP_AVP_TYPE_ACCM].value->accm.send_accm = session->send_accm;
	avps[L2TP_AVP_TYPE_ACCM].value->accm.recv_accm = session->recv_accm;

	/* build and send SLI */
	result = l2tp_avp_message_encode(L2TP_AVP_MSG_SLI, &pkt, l2tp_tunnel_is_hide_avps(tunnel), avps, tunnel);
	if (result < 0) {
		l2tp_stats.encode_message_fails++;
		goto error;
	}

	l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %hu/%hu: sending SLI to peer %hu/%hu, accm=%x/%x", 
			l2tp_tunnel_id(tunnel), session->status.session_id, l2tp_tunnel_peer_id(tunnel), 
			session->status.peer_session_id, session->send_accm, session->recv_accm);

	result = l2tp_net_send(tunnel, l2tp_tunnel_peer_id(tunnel), session->status.peer_session_id, pkt, L2TP_AVP_MSG_OCRQ);

out:
	return result;

error:
	if (pkt != NULL) {
		l2tp_pkt_free(pkt);
	}
	goto out;

#else
	return 0;

#endif /* L2TP_FEATURE_LNS_SUPPORT */
}


/*****************************************************************************
 * LAC Incoming Call machine.
 * Ref RFC2661, 7.4.1
 * The OPEN_REQ, XPRT_DOWN, CLOSE_REQ events are not defined in the RFC.
 * These are added so that all control passes through the state machine.
 *****************************************************************************/

#ifdef L2TP_FEATURE_LAIC_SUPPORT

#define L2TP_LAIC_STATE_IDLE		0
#define L2TP_LAIC_STATE_WAITTUNNEL	1
#define L2TP_LAIC_STATE_WAITREPLY	2
#define L2TP_LAIC_STATE_ESTABLISHED	3
#define L2TP_LAIC_STATE_COUNT		4

#define L2TP_LAIC_STATE_NAMES {			\
	"IDLE",					\
	"WAITTUNNEL",				\
	"WAITREPLY",				\
	"ESTABLISHED" }

#define STATE(state)			L2TP_LAIC_STATE_##state
#define EVENT(event)			L2TP_LAIC_EVENT_##event
#define ACTION(stem)			l2tp_session_laic_##stem

static void l2tp_session_laic_null(void *tunnel, void *session, void *arg3);
static void l2tp_session_laic_initiate_tun_open(void *tunnel, void *session, void *arg3);
static void l2tp_session_laic_cleanup(void *tunnel, void *session, void *arg3);
static void l2tp_session_laic_send_icrq(void *tunnel, void *session, void *arg3);
static void l2tp_session_laic_send_iccn(void *tunnel, void *session, void *arg3);
static void l2tp_session_laic_send_cdn(void *tunnel, void *session, void *arg3);

static const char *l2tp_session_laic_state_names[] = L2TP_LAIC_STATE_NAMES;
static const char *l2tp_session_laic_event_names[] = L2TP_LAIC_EVENT_NAMES;

static const struct usl_fsm_table l2tp_session_laic_fsm = {
	"LAIC", 
	l2tp_session_fsm_log,
	L2TP_LAIC_STATE_COUNT,
	&l2tp_session_laic_state_names[0],
	L2TP_LAIC_EVENT_COUNT, 
	&l2tp_session_laic_event_names[0],
	/* state		event			action				new state */
	{
	{ STATE(IDLE),		EVENT(INCALL_IND),	ACTION(initiate_tun_open),	STATE(WAITTUNNEL) },
	{ STATE(IDLE),		EVENT(ICCN),		ACTION(cleanup),		STATE(IDLE) },
	{ STATE(IDLE),		EVENT(ICRP_ACCEPT),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(IDLE),		EVENT(ICRP_DENY),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(IDLE),		EVENT(CDN),		ACTION(cleanup),		STATE(IDLE) },
	{ STATE(IDLE),		EVENT(CLOSE_REQ),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(IDLE),		EVENT(XPRT_DOWN),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(IDLE),		EVENT(OPEN_REQ),	ACTION(send_icrq),		STATE(WAITREPLY) },
	{ STATE(IDLE),		EVENT(TUNNEL_OPEN_IND),	ACTION(send_icrq),		STATE(WAITREPLY) },
	{ STATE(WAITTUNNEL),	EVENT(ENDCALL_IND),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(WAITTUNNEL),	EVENT(TUNNEL_OPEN_IND),	ACTION(send_icrq),		STATE(WAITREPLY) },
	{ STATE(WAITTUNNEL),	EVENT(OPEN_REQ),	ACTION(send_icrq),		STATE(WAITREPLY) },
	{ STATE(WAITTUNNEL),	EVENT(XPRT_DOWN),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(WAITTUNNEL),	EVENT(CLOSE_REQ),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(WAITREPLY),	EVENT(ICRP_ACCEPT),	ACTION(send_iccn),		STATE(ESTABLISHED) },
	{ STATE(WAITREPLY),	EVENT(ICRP_DENY),	ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(WAITREPLY),	EVENT(ICRQ),		ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(WAITREPLY),	EVENT(CDN),		ACTION(cleanup),		STATE(IDLE) },
	{ STATE(WAITREPLY),	EVENT(ICCN),		ACTION(cleanup),		STATE(IDLE) },
	{ STATE(WAITREPLY),	EVENT(ENDCALL_IND),	ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(WAITREPLY),	EVENT(OPEN_REQ),	ACTION(null),			STATE(WAITREPLY) },
	{ STATE(WAITREPLY),	EVENT(CLOSE_REQ),	ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(WAITREPLY),	EVENT(XPRT_DOWN),	ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(WAITREPLY),	EVENT(TUNNEL_OPEN_IND),	ACTION(null),			STATE(WAITREPLY) },
	{ STATE(ESTABLISHED),	EVENT(CDN),		ACTION(cleanup),		STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(ICRQ),		ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(ICRP_ACCEPT),	ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(ICRP_DENY),	ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(ICCN),		ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(ENDCALL_IND),	ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(OPEN_REQ),	ACTION(null),			STATE(ESTABLISHED) },
	{ STATE(ESTABLISHED),	EVENT(CLOSE_REQ),	ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(XPRT_DOWN),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(TUNNEL_OPEN_IND),	ACTION(null),			STATE(ESTABLISHED) },
	{ 0,			0,			NULL,				0 }
	}
};	

#undef STATE
#undef EVENT
#undef ACTION

static void l2tp_session_laic_null(void *tunnel, void *session, void *arg3)
{
}

static void l2tp_session_laic_initiate_tun_open(void *tunnel, void *sess, void *arg3)
{
	struct l2tp_session *session = sess;

	L2TP_SESSION_CHECK(session, tunnel);

	/* If the tunnel is already up, its peer_tunnel_id will be
	 * non-zero.  If that's the case, send a TUNNEL_OPEN_IND event
	 * to the session.  The tunnel will send the TUNNEL_OPEN_IND
	 * event to each pending session when it finally comes up.
	 */
	if (l2tp_tunnel_peer_id(tunnel) != 0) {
		l2tp_session_queue_event(l2tp_tunnel_id(tunnel), session->status.session_id, L2TP_LAIC_EVENT_TUNNEL_OPEN_IND);
	} else {
		l2tp_session_log(session, L2TP_PROTOCOL, LOG_INFO, "PROTO: tunl %s: waiting for tunnel up", session->fsmi.name);
		session->open_event = L2TP_LAIC_EVENT_TUNNEL_OPEN_IND;
	}
}

static void l2tp_session_laic_cleanup(void *tunnel, void *sess, void *arg3)
{
	struct l2tp_session *session = sess;

	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s", __func__, session->fsmi.name);

	L2TP_SESSION_CHECK(session, tunnel);

	/* Stop retry timer if it is running */
	if ((session->retry_timer != NULL) && (usl_timer_is_running(session->retry_timer))) {
		L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s: stop persist timer", __func__, session->fsmi.name);
		usl_timer_stop(session->retry_timer);
	}

	/* Give plugins visibility of session down */
	(void) l2tp_session_down_ind(tunnel, session);

	/* Give plugins visibility of session deleted */
	(void) l2tp_session_deleted_ind(tunnel, session);

	/* Handle persisting sessions */
	if (session->status.persist) {
		/* Detach session from tunnel if tunnel is down and not already detached */
		if ((session->status.tunnel_down) && (!session->status.detached)) {
			l2tp_session_tunnel_detach(tunnel, session);
		}
		l2tp_session_persist(session);
	} else {
		/* Protect against multiple (queued) close requests */
		if (!session->cleaned_up) {
			session->cleaned_up = 1;
			l2tp_session_dec_use_count(session);
		}
	}
}

static void l2tp_session_laic_send_icrq(void *tunnel, void *sess, void *arg3)
{
	int result;
	struct l2tp_session *session = sess;

	L2TP_SESSION_CHECK(session, tunnel);
	result = l2tp_session_send_icrq(tunnel, session);
	if (result < 0) {
		l2tp_session_queue_event(l2tp_tunnel_id(tunnel), session->status.session_id, L2TP_LAIC_EVENT_XPRT_DOWN);
	}
}

static void l2tp_session_laic_send_iccn(void *tunnel, void *sess, void *arg3)
{
	int result;
	struct l2tp_session *session = sess;

	L2TP_SESSION_CHECK(session, tunnel);
	result = l2tp_session_send_iccn(tunnel, session);
	if (result < 0) {
		l2tp_session_queue_event(l2tp_tunnel_id(tunnel), session->status.session_id, L2TP_LAIC_EVENT_XPRT_DOWN);
	}
}

static void l2tp_session_laic_send_cdn(void *tunnel, void *sess, void *arg3)
{
	struct l2tp_session *session = sess;

	L2TP_SESSION_CHECK(session, tunnel);
	(void) l2tp_session_send_cdn(tunnel, session);
	l2tp_session_laic_cleanup(tunnel, session, arg3);
}

#endif /* L2TP_FEATURE_LAIC_SUPPORT */

/*****************************************************************************
 * LNS Incoming Call machine.
 * Ref RFC2661, 7.4.2
 * The OPEN_REQ, XPRT_DOWN, CLOSE_REQ events are not defined in the RFC.
 * These are added so that all control passes through the state machine.
 *****************************************************************************/

#ifdef L2TP_FEATURE_LNIC_SUPPORT

#define L2TP_LNIC_STATE_IDLE		0
#define L2TP_LNIC_STATE_WAITCONNECT	1
#define L2TP_LNIC_STATE_ESTABLISHED	2
#define L2TP_LNIC_STATE_COUNT		3

#define L2TP_LNIC_STATE_NAMES {			\
	"IDLE",					\
	"WAITCONNECT",				\
	"ESTABLISHED" }


#define STATE(state)			L2TP_LNIC_STATE_##state
#define EVENT(event)			L2TP_LNIC_EVENT_##event
#define ACTION(stem)			l2tp_session_lnic_##stem

static void l2tp_session_lnic_null(void *tunnel, void *session, void *arg3);
static void l2tp_session_lnic_send_icrp(void *tunnel, void *session, void *arg3);
static void l2tp_session_lnic_send_cdn(void *tunnel, void *session, void *arg3);
static void l2tp_session_lnic_cleanup(void *tunnel, void *session, void *arg3);
static void l2tp_session_lnic_prep_data(void *tunnel, void *session, void *arg3);

static const char *l2tp_session_lnic_state_names[] = L2TP_LNIC_STATE_NAMES;
static const char *l2tp_session_lnic_event_names[] = L2TP_LNIC_EVENT_NAMES;

static const struct usl_fsm_table l2tp_session_lnic_fsm = {
	"LNIC", 
	l2tp_session_fsm_log,
	L2TP_LNIC_STATE_COUNT,
	&l2tp_session_lnic_state_names[0],
	L2TP_LNIC_EVENT_COUNT, 
	&l2tp_session_lnic_event_names[0],
	{
	/* state		event			action			new state */
	{ STATE(IDLE),		EVENT(ICRQ_ACCEPT),	ACTION(send_icrp),	STATE(WAITCONNECT) },
	{ STATE(IDLE),		EVENT(ICRQ_DENY),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(IDLE),		EVENT(ICRP),		ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(IDLE),		EVENT(ICCN_ACCEPT),	ACTION(cleanup),	STATE(IDLE) },
	{ STATE(IDLE),		EVENT(ICCN_DENY),	ACTION(cleanup),	STATE(IDLE) },
	{ STATE(IDLE),		EVENT(CLOSE_REQ),	ACTION(cleanup),	STATE(IDLE) },
	{ STATE(IDLE),		EVENT(XPRT_DOWN),	ACTION(null),		STATE(IDLE) },
	{ STATE(IDLE),		EVENT(CDN),		ACTION(cleanup),	STATE(IDLE) },
	{ STATE(WAITCONNECT),	EVENT(ICCN_ACCEPT),	ACTION(prep_data),	STATE(ESTABLISHED) },
	{ STATE(WAITCONNECT),	EVENT(ICCN_DENY),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(WAITCONNECT),	EVENT(ICRQ_ACCEPT),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(WAITCONNECT),	EVENT(ICRQ_DENY),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(WAITCONNECT),	EVENT(ICRP),		ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(WAITCONNECT),	EVENT(CDN),		ACTION(cleanup),	STATE(IDLE) },
	{ STATE(WAITCONNECT),	EVENT(XPRT_DOWN),	ACTION(cleanup),	STATE(IDLE) },
	{ STATE(WAITCONNECT),	EVENT(CLOSE_REQ),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(CDN),		ACTION(cleanup),	STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(CLOSE_REQ),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(ICRQ_ACCEPT),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(ICRQ_DENY),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(ICRP),		ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(ICCN_ACCEPT),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(ICCN_DENY),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(XPRT_DOWN),	ACTION(cleanup),	STATE(IDLE) },
	{ 0,			0,			NULL,			0 }
	}
};	

#undef STATE
#undef EVENT
#undef ACTION

static void l2tp_session_lnic_null(void *tunnel, void *sess, void *arg3)
{
}

static void l2tp_session_lnic_send_icrp(void *tunnel, void *sess, void *arg3)
{
	int result;
	struct l2tp_session *session = sess;

	L2TP_SESSION_CHECK(session, tunnel);
	result = l2tp_session_send_icrp(tunnel, session);
	if (result < 0) {
		l2tp_session_queue_event(l2tp_tunnel_id(tunnel), session->status.session_id, L2TP_LNIC_EVENT_XPRT_DOWN);
	}
}

static void l2tp_session_lnic_send_cdn(void *tunnel, void *sess, void *arg3)
{
	struct l2tp_session *session = sess;

	L2TP_SESSION_CHECK(session, tunnel);
	(void) l2tp_session_send_cdn(tunnel, session);
	l2tp_session_lnic_cleanup(tunnel, session, arg3);
}

static void l2tp_session_lnic_cleanup(void *tunnel, void *sess, void *arg3)
{
	struct l2tp_session *session = sess;

	L2TP_SESSION_CHECK(session, tunnel);

	/* Give plugins visibility of session down */
	(void) l2tp_session_down_ind(tunnel, session);

	/* Give plugins visibility of session deleted */
	(void) l2tp_session_deleted_ind(tunnel, session);

	/* Protect against multiple (queued) close requests */
	if (!session->cleaned_up) {
		session->cleaned_up = 1;
		l2tp_session_dec_use_count(session);
	}
}

static void l2tp_session_lnic_prep_data(void *tunnel, void *sess, void *arg3)
{
	struct l2tp_session *session = sess;
	int result;

	L2TP_SESSION_CHECK(session, tunnel);

	/* Stop session establish timer */
	if (session->timer != NULL) {
		usl_timer_delete(session->timer);
		session->timer = NULL;
	}

	/* Give plugins visibility of session up */
	result = l2tp_session_up_ind(tunnel, session);
	if (result < 0) {
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_VENDOR_ERROR, 
					"failed to start ppp");
		l2tp_session_queue_event(l2tp_tunnel_id(tunnel), session->status.session_id, session->close_event);
	}
}

#endif /* L2TP_FEATURE_LNIC_SUPPORT */

/*****************************************************************************
 * LAC Outgoing Call state machine.
 * Ref RFC2661, 7.5.1
 * The OPEN_REQ, XPRT_DOWN, CLOSE_REQ events are not defined in the RFC.
 * These are added so that all control passes through the state machine.
 *****************************************************************************/

#ifdef L2TP_FEATURE_LAOC_SUPPORT

#define L2TP_LAOC_STATE_IDLE		0
#define L2TP_LAOC_STATE_WAITCSANSWER	1
#define L2TP_LAOC_STATE_ESTABLISHED	2
#define L2TP_LAOC_STATE_COUNT		3

#define L2TP_LAOC_STATE_NAMES {			\
	"IDLE",					\
	"WAITCSANSWER",				\
	"ESTABLISHED" }

#define STATE(state)			L2TP_LAOC_STATE_##state
#define EVENT(event)			L2TP_LAOC_EVENT_##event
#define ACTION(stem)			l2tp_session_laoc_##stem

static void l2tp_session_laoc_send_ocrp(void *tunnel, void *session, void *arg3);
static void l2tp_session_laoc_send_cdn(void *tunnel, void *session, void *arg3);
static void l2tp_session_laoc_cleanup(void *tunnel, void *session, void *arg3);
static void l2tp_session_laoc_send_occn(void *tunnel, void *session, void *arg3);

static const char *l2tp_session_laoc_state_names[] = L2TP_LAOC_STATE_NAMES;
static const char *l2tp_session_laoc_event_names[] = L2TP_LAOC_EVENT_NAMES;

static const struct usl_fsm_table l2tp_session_laoc_fsm = {
	"LAOC", 
	l2tp_session_fsm_log,
	L2TP_LAOC_STATE_COUNT,
	&l2tp_session_laoc_state_names[0],
	L2TP_LAOC_EVENT_COUNT, 
	&l2tp_session_laoc_event_names[0],
	{
	/* state		event			action			new state */
	{ STATE(IDLE),		EVENT(OCRQ_ACCEPT),	ACTION(send_ocrp),	STATE(WAITCSANSWER) },
	{ STATE(IDLE),		EVENT(OCRQ_DENY),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(IDLE),		EVENT(OCRP),		ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(IDLE),		EVENT(OCCN),		ACTION(cleanup),	STATE(IDLE) },
	{ STATE(IDLE),		EVENT(CDN),		ACTION(cleanup),	STATE(IDLE) },
	{ STATE(IDLE),		EVENT(CLOSE_REQ),	ACTION(cleanup),	STATE(IDLE) },
	{ STATE(IDLE),		EVENT(XPRT_DOWN),	ACTION(cleanup),	STATE(IDLE) },
	{ STATE(WAITCSANSWER),	EVENT(BEARER_UP),	ACTION(send_occn),	STATE(ESTABLISHED) },
	{ STATE(WAITCSANSWER),	EVENT(BEARER_DOWN),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(WAITCSANSWER),	EVENT(OCRQ_ACCEPT),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(WAITCSANSWER),	EVENT(OCRQ_DENY),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(WAITCSANSWER),	EVENT(OCRP),		ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(WAITCSANSWER),	EVENT(OCCN),		ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(WAITCSANSWER),	EVENT(CDN),		ACTION(cleanup),	STATE(IDLE) },
	{ STATE(WAITCSANSWER),	EVENT(CLOSE_REQ),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(WAITCSANSWER),	EVENT(XPRT_DOWN),	ACTION(cleanup),	STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(OCRQ_ACCEPT),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(OCRQ_DENY),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(OCRP),		ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(OCCN),		ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(CDN),		ACTION(cleanup),	STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(BEARER_DOWN),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(CLOSE_REQ),	ACTION(send_cdn),	STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(XPRT_DOWN),	ACTION(cleanup),	STATE(IDLE) },
	{ 0,			0,			NULL,			0 }
	}
};

#undef STATE
#undef EVENT
#undef ACTION

static void l2tp_session_laoc_send_ocrp(void *tunnel, void *sess, void *arg3)
{
	int result;
	struct l2tp_session *session = sess;

	L2TP_SESSION_CHECK(session, tunnel);
	result = l2tp_session_send_ocrp(tunnel, session);
	if (result < 0) {
		l2tp_session_queue_event(l2tp_tunnel_id(tunnel), session->status.session_id, L2TP_LAOC_EVENT_XPRT_DOWN);
		goto out;
	}

	/* Use plugins to open bearer using called_number */
	if (l2tp_session_open_bearer_hook != NULL) {
		result = (*l2tp_session_open_bearer_hook)(session, session->config.called_number);
		if (result < 0) {
			l2tp_session_queue_event(l2tp_tunnel_id(tunnel), session->status.session_id, L2TP_LAOC_EVENT_BEARER_DOWN);
			goto out;
		}
	} else {
		l2tp_session_queue_event(l2tp_tunnel_id(tunnel), session->status.session_id, L2TP_LAOC_EVENT_BEARER_UP);
	}
out:
	return;
}

static void l2tp_session_laoc_send_cdn(void *tunnel, void *sess, void *arg3)
{
	struct l2tp_session *session = sess;

	L2TP_SESSION_CHECK(session, tunnel);
	(void) l2tp_session_send_cdn(tunnel, session);

	/* Use plugins to close bearer */
	if (l2tp_session_close_bearer_hook != NULL) {
		(void) (*l2tp_session_close_bearer_hook)(session, session->config.called_number);
	}

	l2tp_session_laoc_cleanup(tunnel, session, arg3);
}

static void l2tp_session_laoc_cleanup(void *tunnel, void *sess, void *arg3)
{
	struct l2tp_session *session = sess;

	L2TP_SESSION_CHECK(session, tunnel);

	/* Give plugins visibility of session down */
	(void) l2tp_session_down_ind(tunnel, session);

	/* Give plugins visibility of session deleted */
	(void) l2tp_session_deleted_ind(tunnel, session);

	/* Protect against multiple (queued) close requests */
	if (!session->cleaned_up) {
		session->cleaned_up = 1;
		l2tp_session_dec_use_count(session);
	}
}

static void l2tp_session_laoc_send_occn(void *tunnel, void *sess, void *arg3)
{
	int result;
	struct l2tp_session *session = sess;

	L2TP_SESSION_CHECK(session, tunnel);
	result = l2tp_session_send_occn(tunnel, session);
	if (result < 0) {
		l2tp_session_queue_event(l2tp_tunnel_id(tunnel), session->status.session_id, L2TP_LAOC_EVENT_XPRT_DOWN);
	}

	/* Stop session establish timer */
	if (session->timer != NULL) {
		usl_timer_delete(session->timer);
		session->timer = NULL;
	}

	/* Give plugins visibility of session up */
	result = l2tp_session_up_ind(tunnel, session);
	if (result < 0) {
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_VENDOR_ERROR, 
					"failed to start ppp");
		l2tp_session_queue_event(l2tp_tunnel_id(tunnel), session->status.session_id, session->close_event);
	}
}

#endif /* L2TP_FEATURE_LAOC_SUPPORT */

/*****************************************************************************
 * LNS Outgoing Call state machine.
 * Ref RFC2661, 7.5.2
 * The OPEN_REQ, XPRT_DOWN, CLOSE_REQ events are not defined in the RFC.
 * These are added so that all control passes through the state machine.
 *****************************************************************************/

#ifdef L2TP_FEATURE_LNOC_SUPPORT

#define L2TP_LNOC_STATE_IDLE		0
#define L2TP_LNOC_STATE_WAITTUNNEL	1
#define L2TP_LNOC_STATE_WAITREPLY	2
#define L2TP_LNOC_STATE_WAITCONNECT	3
#define L2TP_LNOC_STATE_ESTABLISHED	4
#define L2TP_LNOC_STATE_COUNT		5

#define L2TP_LNOC_STATE_NAMES {			\
	"IDLE",					\
	"WAITTUNNEL",				\
	"WAITREPLY",				\
	"WAITCONNECT",				\
	"ESTABLISHED" }

#define STATE(state)			L2TP_LNOC_STATE_##state
#define EVENT(event)			L2TP_LNOC_EVENT_##event
#define ACTION(stem)			l2tp_session_lnoc_##stem

static void l2tp_session_lnoc_initiate_tun_open(void *tunnel, void *session, void *arg3);
static void l2tp_session_lnoc_cleanup(void *tunnel, void *session, void *arg3);
static void l2tp_session_lnoc_send_ocrq(void *tunnel, void *session, void *arg3);
static void l2tp_session_lnoc_null(void *tunnel, void *session, void *arg3);
static void l2tp_session_lnoc_send_cdn(void *tunnel, void *session, void *arg3);

static const char *l2tp_session_lnoc_state_names[] = L2TP_LNOC_STATE_NAMES;
static const char *l2tp_session_lnoc_event_names[] = L2TP_LNOC_EVENT_NAMES;

static const struct usl_fsm_table l2tp_session_lnoc_fsm = {
	"LNOC", 
	l2tp_session_fsm_log,
	L2TP_LNOC_STATE_COUNT,
	&l2tp_session_lnoc_state_names[0],
	L2TP_LNOC_EVENT_COUNT, 
	&l2tp_session_lnoc_event_names[0],
	{
	/* state		event			action				new state */
	{ STATE(IDLE),		EVENT(OPEN_REQ),	ACTION(initiate_tun_open),	STATE(WAITTUNNEL) },
	{ STATE(IDLE),		EVENT(OCCN),		ACTION(cleanup),		STATE(IDLE) },
	{ STATE(IDLE),		EVENT(OCRP_ACCEPT),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(IDLE),		EVENT(OCRP_DENY),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(IDLE),		EVENT(CDN),		ACTION(cleanup),		STATE(IDLE) },
	{ STATE(IDLE),		EVENT(OPEN_REQ),	ACTION(send_ocrq),		STATE(WAITREPLY) },
	{ STATE(IDLE),		EVENT(CLOSE_REQ),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(IDLE),		EVENT(XPRT_DOWN),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(IDLE),		EVENT(CDN),		ACTION(cleanup),		STATE(IDLE) },
	{ STATE(IDLE),		EVENT(TUNNEL_OPEN),	ACTION(send_ocrq),		STATE(WAITREPLY) },
	{ STATE(WAITTUNNEL),	EVENT(TUNNEL_OPEN),	ACTION(send_ocrq),		STATE(WAITREPLY) },
	{ STATE(WAITTUNNEL),	EVENT(OPEN_REQ),	ACTION(send_ocrq),		STATE(WAITREPLY) },
	{ STATE(WAITTUNNEL),	EVENT(CLOSE_REQ),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(WAITTUNNEL),	EVENT(XPRT_DOWN),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(WAITREPLY),	EVENT(OCRP_ACCEPT),	ACTION(null),			STATE(WAITCONNECT) },
	{ STATE(WAITREPLY),	EVENT(OCRP_DENY),	ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(WAITREPLY),	EVENT(OCCN),		ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(WAITREPLY),	EVENT(OCRQ),		ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(WAITREPLY),	EVENT(CLOSE_REQ),	ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(WAITREPLY),	EVENT(CDN),		ACTION(cleanup),		STATE(IDLE) },
	{ STATE(WAITREPLY),	EVENT(XPRT_DOWN),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(WAITCONNECT),	EVENT(OCCN),		ACTION(null),			STATE(ESTABLISHED) },
	{ STATE(WAITCONNECT),	EVENT(OCRQ),		ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(WAITCONNECT),	EVENT(OCRP_ACCEPT),	ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(WAITCONNECT),	EVENT(OCRP_DENY),	ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(WAITCONNECT),	EVENT(CLOSE_REQ),	ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(WAITCONNECT),	EVENT(CDN),		ACTION(cleanup),		STATE(IDLE) },
	{ STATE(WAITCONNECT),	EVENT(XPRT_DOWN),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(CDN),		ACTION(cleanup),		STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(XPRT_DOWN),	ACTION(cleanup),		STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(OCRQ),		ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(OCRP_ACCEPT),	ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(OCRP_DENY),	ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(OCCN),		ACTION(send_cdn),		STATE(IDLE) },
	{ STATE(ESTABLISHED),	EVENT(CLOSE_REQ),	ACTION(send_cdn),		STATE(IDLE) },
	{ 0,			0,			NULL,				0 }
	}
};

#undef STATE
#undef EVENT
#undef ACTION

static void l2tp_session_lnoc_initiate_tun_open(void *tunnel, void *sess, void *arg3)
{
	struct l2tp_session *session = sess;

	L2TP_SESSION_CHECK(session, tunnel);

	/* If the tunnel is already up, its peer_tunnel_id will be
	 * non-zero.  If that's the case, send a TUNNEL_OPEN_IND event
	 * to the session.  The tunnel will send the TUNNEL_OPEN_IND
	 * event to each pending session when it finally comes up.
	 */
	if (l2tp_tunnel_peer_id(tunnel) != 0) {
		l2tp_session_queue_event(l2tp_tunnel_id(tunnel), session->status.session_id, L2TP_LNOC_EVENT_TUNNEL_OPEN);
	} else {
		session->open_event = L2TP_LNOC_EVENT_TUNNEL_OPEN;
	}
}

static void l2tp_session_lnoc_cleanup(void *tunnel, void *sess, void *arg3)
{
	struct l2tp_session *session = sess;

	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s", __func__, session->fsmi.name);

	L2TP_SESSION_CHECK(session, tunnel);

	/* Stop retry timer if it is running */
	if ((session->retry_timer != NULL) && (usl_timer_is_running(session->retry_timer))) {
		L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s: stop persist timer", __func__, session->fsmi.name);
		usl_timer_stop(session->retry_timer);
	}

	/* Give plugins visibility of session down */
	(void) l2tp_session_down_ind(tunnel, session);

	/* Give plugins visibility of session deleted */
	(void) l2tp_session_deleted_ind(tunnel, session);

	/* Handle persisting sessions */
	if (session->status.persist) {
		/* Detach session from tunnel if tunnel is down and not already detached */
		if ((session->status.tunnel_down) && (!session->status.detached)) {
			l2tp_session_tunnel_detach(tunnel, session);
		}
		l2tp_session_persist(session);
	} else {
		/* Protect against multiple (queued) close requests */
		if (!session->cleaned_up) {
			session->cleaned_up = 1;
			l2tp_session_dec_use_count(session);
		}
	}
}

static void l2tp_session_lnoc_send_ocrq(void *tunnel, void *sess, void *arg3)
{
	int result;
	struct l2tp_session *session = sess;

	L2TP_SESSION_CHECK(session, tunnel);
	result = l2tp_session_send_ocrq(tunnel, session);
	if (result < 0) {
		l2tp_session_queue_event(l2tp_tunnel_id(tunnel), session->status.session_id, L2TP_LNOC_EVENT_XPRT_DOWN);
	}
}

static void l2tp_session_lnoc_null(void *tunnel, void *sess, void *arg3)
{
}

static void l2tp_session_lnoc_send_cdn(void *tunnel, void *sess, void *arg3)
{
	struct l2tp_session *session = sess;

	L2TP_SESSION_CHECK(session, tunnel);
	(void) l2tp_session_send_cdn(tunnel, session);
	l2tp_session_lnoc_cleanup(tunnel, session, arg3);
}

#endif /* L2TP_FEATURE_LNOC_SUPPORT */

/*****************************************************************************
 * Event map for each session type.
 *
 * There are 4 session types, LAC Incoming Call (LAIC), LAC Outgoing
 * Call (LAOC), LNS Incoming Call (LNIC) and LNS Outgoing Call
 * (LNOC). Each session type may receive session control protocol
 * messages. RFC2661 gives state machine specifications for each type,
 * which means we must generate different events in message handlers
 * according to session type. To simplify the message handlers, we use
 * an event table calling out the event to be generated per message in
 * the case when the message is accepted or rejected. In cases where
 * a reject event does not make sense, it is given the same as 
 * the accept event.
 * Indexed by message type.
 *****************************************************************************/

struct l2tp_session_event_entry {
	int accept_event;
	int deny_event;
};

#ifdef L2TP_FEATURE_LAIC_SUPPORT

#undef EVENT
#define EVENT(event)	L2TP_LAIC_EVENT_##event

static const struct l2tp_session_event_entry l2tp_session_laic_event_table[] = {
	{ -1, 			-1 },			/* OCRQ */
	{ -1, 			-1 },			/* OCRP */
	{ -1, 			-1 },			/* OCCN */
	{ EVENT(ICRQ), 		EVENT(CLOSE_REQ) },	/* ICRQ */
	{ EVENT(ICRP_ACCEPT), 	EVENT(ICRP_DENY) },	/* ICRP */
	{ EVENT(ICCN), 		EVENT(CLOSE_REQ) },	/* ICCN */
	{ -1, 			-1 },			/* RESERVED2 */
	{ EVENT(CDN), 		EVENT(CDN) },		/* CDN */
};

#endif /* L2TP_FEATURE_LAIC_SUPPORT */

#ifdef L2TP_FEATURE_LAOC_SUPPORT

#undef EVENT
#define EVENT(event)	L2TP_LAOC_EVENT_##event

static const struct l2tp_session_event_entry l2tp_session_laoc_event_table[] = {
	{ EVENT(OCRQ_ACCEPT),	EVENT(OCRQ_DENY) },	/* OCRQ */
	{ EVENT(OCRP), 		EVENT(CLOSE_REQ) },	/* OCRP */
	{ EVENT(OCCN), 		EVENT(CLOSE_REQ) },	/* OCCN */
	{ -1, 			-1 },			/* ICRQ */
	{ -1, 			-1 },			/* ICRP */
	{ -1, 			-1 },			/* ICCN */
	{ -1, 			-1 },			/* RESERVED2 */
	{ EVENT(CDN), 		EVENT(CDN) },		/* CDN */
};

#endif /* L2TP_FEATURE_LAOC_SUPPORT */

#ifdef L2TP_FEATURE_LNIC_SUPPORT

#undef EVENT
#define EVENT(event)	L2TP_LNIC_EVENT_##event

static const struct l2tp_session_event_entry l2tp_session_lnic_event_table[] = {
	{ -1, 			-1 },			/* OCRQ */
	{ -1, 			-1 },			/* OCRP */
	{ -1, 			-1 },			/* OCCN */
	{ EVENT(ICRQ_ACCEPT),	EVENT(ICRQ_DENY) },	/* ICRQ */
	{ EVENT(ICRP), 		EVENT(CLOSE_REQ) },	/* ICRP */
	{ EVENT(ICCN_ACCEPT), 	EVENT(ICCN_DENY) },	/* ICCN */
	{ -1, 			-1 },			/* RESERVED2 */
	{ EVENT(CDN), 		EVENT(CDN) },		/* CDN */
};

#endif /* L2TP_FEATURE_LNIC_SUPPORT */

#ifdef L2TP_FEATURE_LNOC_SUPPORT

#undef EVENT
#define EVENT(event)	L2TP_LNOC_EVENT_##event

static const struct l2tp_session_event_entry l2tp_session_lnoc_event_table[] = {
	{ EVENT(OCRQ), 		EVENT(CLOSE_REQ) },	/* OCRQ */
	{ EVENT(OCRP_ACCEPT), 	EVENT(OCRP_DENY) },	/* OCRP */
	{ EVENT(OCCN), 		EVENT(CLOSE_REQ) },	/* OCCN */
	{ -1, 			-1 },			/* ICRQ */
	{ -1, 			-1 },			/* ICRP */
	{ -1, 			-1 },			/* ICCN */
	{ -1, 			-1 },			/* RESERVED2 */
	{ EVENT(CDN), 		EVENT(CDN) },		/* CDN */
};

#endif /* L2TP_FEATURE_LNOC_SUPPORT */

/* Indexed by session type. See L2TP_API_SESSION_TYPE_XXX values. */
static const struct l2tp_session_event_entry *l2tp_session_event_table[] = {
	NULL,
#ifdef L2TP_FEATURE_LAIC_SUPPORT
	&l2tp_session_laic_event_table[0],
#else
	NULL,
#endif
#ifdef L2TP_FEATURE_LAOC_SUPPORT
	&l2tp_session_laoc_event_table[0],
#else
	NULL,
#endif
#ifdef L2TP_FEATURE_LNIC_SUPPORT
	&l2tp_session_lnic_event_table[0],
#else
	NULL,
#endif
#ifdef L2TP_FEATURE_LNOC_SUPPORT
	&l2tp_session_lnoc_event_table[0],
#else
	NULL,
#endif
};

/* Called by message handlers to derive an event for a given message
 * type, depending on session type and whether an accept or deny event
 * is required.
 */
static int l2tp_session_map_event(enum l2tp_api_session_type session_type, int msg_type, enum l2tp_session_msg_action accept)
{
	const struct l2tp_session_event_entry *entry;
	int event;

	if ((msg_type < L2TP_AVP_MSG_OCRQ) || (msg_type > L2TP_AVP_MSG_CDN)) {
		event = -EINVAL;
		goto out;
	}
	if ((session_type < L2TP_API_SESSION_TYPE_LAIC) || (session_type > L2TP_API_SESSION_TYPE_LNOC)) {
		event = -EINVAL;
		goto out;
	}

	entry = l2tp_session_event_table[session_type];
	if (entry == NULL) {
		event = -EOPNOTSUPP;
		goto out;
	}

	event = (accept == MSG_ACCEPT) ? entry[msg_type - L2TP_AVP_MSG_OCRQ].accept_event : entry[msg_type - L2TP_AVP_MSG_OCRQ].deny_event;
	if (event < 0) {
		event = -EBADMSG;
		goto out;
	}

out:
	return event;
}

/*****************************************************************************
 * Internal event queue.
 *
 * Events are short messages indicating the session instance and the
 * event id. The messages are written to a pipe (socket), which is
 * serviced by the USL l2tp_session_do_queued_event() callback.
 *****************************************************************************/

struct l2tp_session_event_msg {
	uint16_t	tunnel_id;
	uint16_t	session_id;
	int		event;
};

static void l2tp_session_do_queued_event(int fd, void *arg)
{
	struct l2tp_session_event_msg msg;
	struct l2tp_tunnel *tunnel;
	struct l2tp_session *session;
	int result;
	int bytes_read = 0;

	for (;;) {
		result = read(fd, ((void *) &msg) + bytes_read, sizeof(msg) - bytes_read);
		if (result <= 0) {
			if (errno == EINTR) {
				continue;
			}
			break;
		}
		bytes_read += result;
		if (bytes_read == sizeof(msg)) {
			tunnel = l2tp_tunnel_find_by_id(msg.tunnel_id);
			if (tunnel != NULL) {
				session = l2tp_session_find_by_id(tunnel, msg.session_id);
				if (session != NULL) {
					l2tp_session_handle_event(session, msg.event);
				} else {
					L2TP_DEBUG(L2TP_FUNC, "%s: tunl %hu/%hu: session not found: discarded event %d", __func__, 
						   msg.tunnel_id, msg.session_id, msg.event);
				}
			} else {
				L2TP_DEBUG(L2TP_FUNC, "%s: tunl %hu/%hu: tunnel not found: discarded event %d", __func__, 
					   msg.tunnel_id, msg.session_id, msg.event);
			}
			break;
		}
	}
}

static int l2tp_session_queue_event(uint16_t tunnel_id, uint16_t session_id, int event)
{
	struct l2tp_session_event_msg msg;
	int result;

	msg.tunnel_id = tunnel_id;
	msg.session_id = session_id;
	msg.event = event;

	result = write(l2tp_session_event_pipe[1], &msg, sizeof(msg));
	if (result < 0) {
		l2tp_stats.event_queue_full_errors++;
		l2tp_log(LOG_ERR, "L2TP: session event pipe full: event %d for session %hu/%hu lost", event, tunnel_id, session_id);
	}

	return result;
}

/*****************************************************************************
 * Session contexts.
 * External modules that hold references to session contexts should bump
 * the session's reference count until the reference is no longer held.
 * When the last reference is dropped, the session context is freed.
 *****************************************************************************/

void l2tp_session_inc_use_count(struct l2tp_session *session)
{
	session->use_count++;
	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s: count now %d", __func__, session->fsmi.name, session->use_count);
}

void l2tp_session_dec_use_count(struct l2tp_session *session)
{
	session->use_count--;
	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s: count now %d", __func__, session->fsmi.name, session->use_count);
	if (session->use_count < 0) {
		l2tp_log(LOG_ERR, "L2TP: Session %hu/%hu use count gone negative! Caller %p",
			 session->my_tunnel ? l2tp_tunnel_id(session->my_tunnel) : 0, 
			 session->status.session_id, __builtin_return_address(0));
		return;
	}
	if (session->use_count == 0) {
		l2tp_session_unlink(session, 0);
	}
}

static int l2tp_session_link(struct l2tp_session *session)
{
	int result = 0;

	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s", __func__, session->fsmi.name);

	if (l2tp_session_max_count != 0) {
		if (l2tp_session_count > l2tp_session_max_count) {
			result = -L2TP_ERR_SESSION_LIMIT_EXCEEDED;
			l2tp_stats.too_many_sessions++;
			l2tp_session_log(session, L2TP_FUNC, LOG_INFO, "FUNC: tunl %s: rejected: session limit (%d) exceeded", 
					session->fsmi.name, l2tp_session_max_count);
			l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_NO_ERROR, 
						"Session limit exceeded");
			goto out;
		}
	}

	usl_list_add(&session->list, &l2tp_session_list);

	/* Add session to the tunnel's session list */
	result = l2tp_tunnel_session_add(session->my_tunnel, &session->session_list, &session->session_id_hlist, session->status.session_id);
	if (result < 0) {
		switch (result) {
		case -L2TP_ERR_TUNNEL_ADD_ADMIN_DISABLED:
			l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_NO_ERROR, 
						"Session creation administratively disabled");
			break;
		case -L2TP_ERR_TUNNEL_TOO_MANY_SESSIONS:
			l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_NO_ERROR, 
						"Tunnel session limit exceeded");
			break;
		default:
			l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_NO_ERROR, NULL);
			break;
		}
		usl_list_del_init(&session->list);
	}

out:
	return result;
}

/* XXX */
static void l2tp_session_tunnel_detach(struct l2tp_tunnel *tunnel, struct l2tp_session *session)
{
	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %hu: detaching %s with use count %d", __func__, 
		   l2tp_tunnel_id(tunnel), session->fsmi.name, session->use_count);

	BUG_TRAP(session->status.detached);

	session->status.detached = -1;

	/* Unlink session from the tunnel's session list */
	l2tp_tunnel_session_remove(session->my_tunnel, &session->session_list, &session->session_id_hlist, 1);

	l2tp_tunnel_dec_use_count(session->my_tunnel);
	session->my_tunnel = NULL;
	session->status.peer_session_id = 0;

	/* Link session to our detached list */
	usl_list_add(&session->detached_list, &l2tp_session_detached_list);
}

static void l2tp_session_tunnel_attach(struct l2tp_tunnel *tunnel, struct l2tp_session *session)
{
	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %hu: attaching %s with use count %d", __func__, 
		   l2tp_tunnel_id(tunnel), session->fsmi.name, session->use_count);

	BUG_TRAP(!session->status.detached);
	BUG_TRAP(session->my_tunnel != NULL);

	/* Session is being reattached to a persistent tunnel. Rebuild the
	 * internal session name (ids might have changed) and re-add the session
	 * to the tunnel's hashed list.
	 */
	l2tp_tunnel_inc_use_count(tunnel);
	session->my_tunnel = tunnel;
	
	sprintf(&session->fsmi.name[0], "%hu/%hu", l2tp_tunnel_id(tunnel), session->status.session_id);
	session->sent_created_event = 0;
	session->sent_deleted_event = 1;
	session->sent_up_event = 0;
	session->sent_down_event = 1;
	session->cleaned_up = 0;

	usl_list_del_init(&session->detached_list);
	session->status.detached = 0;

	l2tp_tunnel_session_add_again(tunnel, &session->session_id_hlist, session->status.session_id);
	l2tp_session_created_ind(tunnel, session);
}

/* Timer callback to timeout session establish retries.
 */
static void l2tp_session_retry_timer_expired(void *arg)
{
	struct l2tp_session *session = arg;

	BUG_TRAP(session->my_tunnel == NULL);
	BUG_TRAP(session->status.detached);

	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s: retrying persistent session in tunnel %hu", __func__,
		   session->fsmi.name, l2tp_tunnel_id(session->my_tunnel));
	session->sent_created_event = 0;
	session->sent_deleted_event = 1;
	session->sent_up_event = 0;
	session->sent_down_event = 1;
	session->cleaned_up = 0;

	l2tp_session_created_ind(session->my_tunnel, session);
	l2tp_session_queue_event(l2tp_tunnel_id(session->my_tunnel), session->status.session_id, 
				 session->open_event);
}

/* Handle persisting sessions for sessions that have gone down in tunnels
 * that are still up.
 */
static void l2tp_session_persist(struct l2tp_session *session)
{
	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s: tunnel_down=%d", __func__, session->fsmi.name, session->status.tunnel_down);

	BUG_TRAP(!session->status.persist);

	if ((!session->status.tunnel_down) && (l2tp_session_persist_pend_timeout != 0)) {
		/* If tunnel still up, start a timer such that we automatically
		 * recreate the session later.
		 */
		BUG_TRAP(session->status.detached);

		l2tp_session_log(session, L2TP_FUNC, LOG_DEBUG, 
				 "FUNC: tunl %s: holding locally created session in persistent tunnel for retry",
				 session->fsmi.name);
		
		switch (session->type) {
		case L2TP_API_SESSION_TYPE_LAIC:
			session->open_event = L2TP_LAIC_EVENT_TUNNEL_OPEN_IND;
			break;
		case L2TP_API_SESSION_TYPE_LNOC:
			session->open_event = L2TP_LNOC_EVENT_TUNNEL_OPEN;
			break;
		default:
			l2tp_log(LOG_ERR, "L2TP: Internal session error. Unexpected type: %d", session->type);
			goto err;
		}

		if (session->retry_timer != NULL) {
			usl_timer_restart(session->retry_timer);
		} else {
			session->retry_timer = usl_timer_create(USL_TIMER_TICKS(l2tp_session_persist_pend_timeout),
								USL_TIMER_TICKS(l2tp_session_persist_pend_timeout),
								l2tp_session_retry_timer_expired,
								session, NULL);
			if (session->retry_timer == NULL) {
				goto err;
			}
			L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s: allocated retry timer=%p", __func__, 
				   session->fsmi.name, session->retry_timer);
		}
	} else if (session->retry_timer != NULL) {
		/* Tunnel is up so stop retry timer */
		usl_timer_stop(session->retry_timer);
	}

	return;

err:
	session->status.persist = 0;
	l2tp_session_dec_use_count(session);
}

/* If the use count is zero, remove the session from the tunnel and delete it.
 */
static void l2tp_session_unlink(struct l2tp_session *session, int force)
{
	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s: force=%d", __func__, session->fsmi.name, force);

	if ((session->use_count == 0) || force) {
		/* Unlink session from the tunnel's session list */
		l2tp_tunnel_session_remove(session->my_tunnel, &session->session_list, &session->session_id_hlist, 0);

		/* Unlink session from our list */
		usl_list_del(&session->list);

		l2tp_session_free(session);
	}
}

static struct l2tp_session *l2tp_session_alloc(struct l2tp_tunnel *tunnel, enum l2tp_api_session_type session_type, uint16_t session_id,
					       char *session_profile_name, int created_by_admin, int *ret)
{
	struct l2tp_session *session = NULL;
	int result = 0;

	/* Allocate a new session context */
	session = calloc(1, sizeof(struct l2tp_session));
	if (session == NULL) {
		l2tp_stats.no_session_resources++;
		result = -ENOMEM;
		goto error;
	}
	session->status.created_by_admin = created_by_admin;
	l2tp_session_count++;

	/* Use caller's session_id, else let the system allocate one.*/
	if (session_id == 0) {
		session_id = l2tp_session_allocate_id(tunnel);
	}
	if (session_id == 0) {
		l2tp_stats.session_id_alloc_fails++;
		result = -L2TP_ERR_SESSION_ID_ALLOC_FAILURE;
		goto error;
	}
	/* Store its id and name */
	session->status.session_id = session_id;
	sprintf(&session->fsmi.name[0], "%hu/%hu", l2tp_tunnel_id(tunnel), session_id);

	/* Don't send down/deleted event until the up/created events */
	session->sent_deleted_event = 1;
	session->sent_down_event = 1;

	/* We keep a reference to the tunnel so make sure it doesn't go away while we are using it */
	l2tp_tunnel_inc_use_count(tunnel);
	session->my_tunnel = tunnel;

	/* If session type is unspecified, derive its type from
	 * whether the session is being created at the LAC or LNS.
	 */
	if (session_type == L2TP_API_SESSION_TYPE_UNSPECIFIED) {
		if (l2tp_tunnel_is_lac(tunnel)) {
			session_type = created_by_admin ? L2TP_API_SESSION_TYPE_LAIC : L2TP_API_SESSION_TYPE_LAOC;
		} else if (l2tp_tunnel_is_lns(tunnel)) {
			session_type = created_by_admin ? L2TP_API_SESSION_TYPE_LNOC : L2TP_API_SESSION_TYPE_LNIC;
		} else {
			result = -L2TP_ERR_SESSION_TYPE_ILLEGAL_FOR_TUNNEL;
			l2tp_tunnel_log(tunnel, L2TP_API, LOG_ERR, "API: tunl %hu: Unable to derive session type for generic LAC/LNS tunnel", 
					l2tp_tunnel_id(tunnel));
			l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_BAD_VALUE, "unable to derive session type");
			goto error;
		}
	}

	/* Check that the requested session type is compatible with the tunnel mode */
	if (((session_type == L2TP_API_SESSION_TYPE_LAIC) && l2tp_tunnel_is_lns(tunnel)) ||
	    ((session_type == L2TP_API_SESSION_TYPE_LAOC) && l2tp_tunnel_is_lns(tunnel)) ||
	    ((session_type == L2TP_API_SESSION_TYPE_LNIC) && l2tp_tunnel_is_lac(tunnel)) ||
	    ((session_type == L2TP_API_SESSION_TYPE_LNOC) && l2tp_tunnel_is_lac(tunnel))) {
		result = -L2TP_ERR_SESSION_TYPE_ILLEGAL_FOR_TUNNEL;
		l2tp_tunnel_log(tunnel, L2TP_API, LOG_ERR, "API: tunl %hu: Incompatible session type %d for %s tunnel type", 
				l2tp_tunnel_id(tunnel), session_type, 
				l2tp_tunnel_is_lac(tunnel) ? "LAC" : 
				l2tp_tunnel_is_lns(tunnel) ? "LNS" : "LACLNS");
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_BAD_VALUE, "wrong tunnel mode");
		goto error;
	}

	session->type = session_type;
	switch (session->type) {
	case L2TP_API_SESSION_TYPE_LAIC:
#ifdef L2TP_FEATURE_LAIC_SUPPORT
		session->fsmi.table = &l2tp_session_laic_fsm;
		session->open_event = L2TP_LAIC_EVENT_INCALL_IND;
		session->close_event = L2TP_LAIC_EVENT_CLOSE_REQ;
#else
		l2tp_tunnel_log(tunnel, L2TP_API, LOG_ERR, "API: tunl %hu: LAIC sessions not supported", l2tp_tunnel_id(tunnel));
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_NOT_AVAILABLE, 0, "LAIC sessions not supported");
		result = -L2TP_ERR_SESSION_TYPE_NOT_SUPPORTED;
		goto error;
#endif /* L2TP_FEATURE_LAIC_SUPPORT */
		break;
	case L2TP_API_SESSION_TYPE_LAOC:
#ifdef L2TP_FEATURE_LAOC_SUPPORT
		session->fsmi.table = &l2tp_session_laoc_fsm;
		session->open_event = L2TP_LAOC_EVENT_OCRQ_ACCEPT;
		session->close_event = L2TP_LAOC_EVENT_CLOSE_REQ;
#else
		l2tp_tunnel_log(tunnel, L2TP_API, LOG_ERR, "API: tunl %hu: LAOC sessions not supported", l2tp_tunnel_id(tunnel));
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_NOT_AVAILABLE, 0, "LAOC sessions not supported");
		result = -L2TP_ERR_SESSION_TYPE_NOT_SUPPORTED;
		goto error;
#endif /* L2TP_FEATURE_LAOC_SUPPORT */
		break;
	case L2TP_API_SESSION_TYPE_LNIC:
#ifdef L2TP_FEATURE_LNIC_SUPPORT
		session->fsmi.table = &l2tp_session_lnic_fsm;
		session->open_event = L2TP_LNIC_EVENT_ICRQ_ACCEPT;
		session->close_event = L2TP_LNIC_EVENT_CLOSE_REQ;
#else
		l2tp_tunnel_log(tunnel, L2TP_API, LOG_ERR, "API: tunl %hu: LNIC sessions not supported", l2tp_tunnel_id(tunnel));
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_NOT_AVAILABLE, 0, "LNIC sessions not supported");
		result = -L2TP_ERR_SESSION_TYPE_NOT_SUPPORTED;
		goto error;
#endif /* L2TP_FEATURE_LNIC_SUPPORT */
		break;
	case L2TP_API_SESSION_TYPE_LNOC:
#ifdef L2TP_FEATURE_LNOC_SUPPORT
		session->fsmi.table = &l2tp_session_lnoc_fsm;
		session->open_event = L2TP_LNOC_EVENT_OPEN_REQ;
		session->close_event = L2TP_LNOC_EVENT_CLOSE_REQ;
#else
		l2tp_tunnel_log(tunnel, L2TP_API, LOG_ERR, "API: tunl %hu: LNOC sessions not supported", l2tp_tunnel_id(tunnel));
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_NOT_AVAILABLE, 0, "LNOC sessions not supported");
		result = -L2TP_ERR_SESSION_TYPE_NOT_SUPPORTED;
		goto error;
#endif /* L2TP_FEATURE_LNOC_SUPPORT */
		break;
	case L2TP_API_SESSION_TYPE_UNSPECIFIED:
		/* should not get here because of checks done above */
		result = -L2TP_ERR_SESSION_TYPE_BAD;
		goto error;
		/* NODEFAULT */
	}

	/* ACCM default values (sent in SLI message, possibly modified during PPP negotiation). */
	session->send_accm = 0xffffffff;
	session->recv_accm = 0xffffffff;

	/* If session is being created in a persistent tunnel, bump its use count such that
	 * it does not get deleted if it goes down. This count is decremented when the
	 * session is administratively deleted or its tunnel's persist state changes.
	 */
	session->status.persist = l2tp_tunnel_is_persistent(tunnel);
	if (session->status.persist) {
		L2TP_DEBUG(L2TP_FUNC, "%s: marking tunl %s persistent", __func__, session->fsmi.name);
		l2tp_session_inc_use_count(session);
	}

	/* Fill with default values. Use profile if one is specified. */
	result = l2tp_session_param_defaults(session, session_profile_name);
	if (result < 0) {
		goto error;
	}

	USL_LIST_HEAD_INIT(&session->list);
	USL_LIST_HEAD_INIT(&session->session_list);
	USL_LIST_HEAD_INIT(&session->detached_list);

	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s: attaching to tunnel %hu", __func__,
		   session->fsmi.name, l2tp_tunnel_id(tunnel));

	/* Start session establish timer */
	if (l2tp_session_establish_timeout != 0) {
		session->timer = usl_timer_create(USL_TIMER_TICKS(l2tp_session_establish_timeout), 0,
						  l2tp_session_establish_timer_expired,
						  session, NULL);
		if (session->timer == NULL) {
			result = -ENOMEM;
			goto error;
		}
		L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s: allocated timer=%p", __func__, session->fsmi.name, session->timer);
	}

	session->status.create_time = l2tp_system_time();
	if (session->status.create_time == NULL) {
		result = -ENOMEM;
		goto error;
	}

	session->status.call_serial_number = ++l2tp_session_call_serial_number;

	/* Bump the session's use count. When this drops to zero, the session context will be freed */
	l2tp_session_inc_use_count(session);

out:
	*ret = result;
	return session;

error:
	if (session != NULL) {
		/* Send CDN to peer if we refused to create
		 * session. We can't let the session state machine do
		 * this since we have no session context when we fail
		 * this early. 
		 */
		if ((session->status.session_id != 0) && (!created_by_admin)) {
			l2tp_session_send_cdn(tunnel, session);
		}
		l2tp_session_free(session);
	}
	session = NULL;
	goto out;
}

static void l2tp_session_free(struct l2tp_session *session)
{
	L2TP_DEBUG(L2TP_FUNC, "%s: %hu", __func__, session->status.session_id);
	if (session->use_count != 0) {
		l2tp_log(LOG_ERR, "L2TP: tunl %s: free when use_count=%d", session->fsmi.name, session->use_count);
		return;
	}

	if (session->timer != NULL) {
		usl_timer_delete(session->timer);
		session->timer = NULL;
	}
	if (session->retry_timer != NULL) {
		usl_timer_delete(session->retry_timer);
		session->retry_timer = NULL;
	}
	if (session->result_code != NULL) {
		free(session->result_code);
	}
	if (session->q931_cause_code != NULL) {
		free(session->q931_cause_code);
	}
	if (session->status.create_time != NULL) {
		free(session->status.create_time);
	}

	/* Delete all allocated parameters */
	if (session->config.priv_group_id != NULL) {
		free(session->config.priv_group_id);
	}
	if (session->config.proxy_auth_name != NULL) {
		free(session->config.proxy_auth_name);
	}
	if (session->config.proxy_auth_challenge != NULL) {
		free(session->config.proxy_auth_challenge);
	}
	if (session->config.proxy_auth_response != NULL) {
		free(session->config.proxy_auth_response);
	}
	if (session->config.calling_number != NULL) {
		free(session->config.calling_number);
	}
	if (session->config.called_number != NULL) {
		free(session->config.called_number);
	}
	if (session->config.sub_address != NULL) {
		free(session->config.sub_address);
	}
	if (session->config.initial_rcvd_lcp_confreq != NULL) {
		free(session->config.initial_rcvd_lcp_confreq);
	}
	if (session->config.last_sent_lcp_confreq != NULL) {
		free(session->config.last_sent_lcp_confreq);
	}
	if (session->config.last_rcvd_lcp_confreq != NULL) {
		free(session->config.last_rcvd_lcp_confreq);
	}
	if (session->q931_cause_code != NULL) {
		free(session->q931_cause_code);
	}
	if (session->config.profile_name != NULL) {
		free(session->config.profile_name);
	}
	if (session->config.ppp_profile_name != NULL) {
		free(session->config.ppp_profile_name);
	}
	if (session->config.user_name != NULL) {
		free(session->config.user_name);
	}
	if (session->config.user_password != NULL) {
		free(session->config.user_password);
	}
	if (session->config.interface_name != NULL) {
		free(session->config.interface_name);
	}
	if (session->config.session_name != NULL) {
		free(session->config.session_name);
	}

	if (session->peer.q931_cause_code != NULL) {
		free(session->peer.q931_cause_code);
	}
	if (session->peer.proxy_auth_name != NULL) {
		free(session->peer.proxy_auth_name);
	}
	if (session->peer.proxy_auth_challenge != NULL) {
		free(session->peer.proxy_auth_challenge);
	}
	if (session->peer.proxy_auth_response != NULL) {
		free(session->peer.proxy_auth_response);
	}
	if (session->peer.priv_group_id != NULL) {
		free(session->peer.priv_group_id);
	}
	if (session->peer.calling_number != NULL) {
		free(session->peer.calling_number);
	}
	if (session->peer.called_number != NULL) {
		free(session->peer.called_number);
	}
	if (session->peer.sub_address != NULL) {
		free(session->peer.sub_address);
	}
	if (session->peer.initial_rcvd_lcp_confreq != NULL) {
		free(session->peer.initial_rcvd_lcp_confreq);
	}
	if (session->peer.last_sent_lcp_confreq != NULL) {
		free(session->peer.last_sent_lcp_confreq);
	}
	if (session->peer.last_rcvd_lcp_confreq != NULL) {
		free(session->peer.last_rcvd_lcp_confreq);
	}
	if (session->peer.call_errors != NULL) {
		free(session->peer.call_errors);
	}
	if (session->peer.accm != NULL) {
		free(session->peer.accm);
	}

	if (session->my_tunnel != NULL) {
		L2TP_DEBUG(L2TP_FUNC, "%s: tunl %s: detaching from tunnel %hu", __func__,
			   session->fsmi.name, l2tp_tunnel_id(session->my_tunnel));
		l2tp_tunnel_dec_use_count(session->my_tunnel);
		session->my_tunnel = NULL;
		session->status.peer_session_id = 0;
	}
	l2tp_session_count--;

	USL_POISON_MEMORY(session, 0xec, sizeof(*session));
	free(session);
}

/* Come here to tell all sessions in a tunnel that the tunnel has changed
 * state. Note that sessions can be attached to tunnels before the tunnel
 * comes up. 
 */
void l2tp_session_tunnel_updown_event(struct l2tp_tunnel *tunnel, int up)
{
	struct l2tp_session *session;
	struct usl_list_head *tmp;
	struct usl_list_head *walk;
	struct usl_list_head *list;

	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %hu: %s", __func__, l2tp_tunnel_id(tunnel), up ? "up" : "down");

	list = l2tp_tunnel_session_list(tunnel);
	usl_list_for_each(walk, tmp, list) {
		session = usl_list_entry(walk, struct l2tp_session, session_list);
		session->status.tunnel_down = up ? 0 : -1; 
		if (up && (session->status.detached)) {
			l2tp_session_tunnel_attach(tunnel, session);
		}
		l2tp_session_queue_event(l2tp_tunnel_id(tunnel), session->status.session_id, 
					 up ? session->open_event : session->close_event);
	}
}

/* Come here to tell all sessions in a tunnel that the tunnel has changed
 * config. Note that sessions can be attached to tunnels before the tunnel
 * comes up. 
 */
void l2tp_session_tunnel_modified(struct l2tp_tunnel *tunnel)
{
	struct l2tp_session *session;
	struct usl_list_head *tmp;
	struct usl_list_head *walk;
	struct usl_list_head *list;

	list = l2tp_tunnel_session_list(tunnel);
	usl_list_for_each(walk, tmp, list) {
		session = usl_list_entry(walk, struct l2tp_session, session_list);
		if (session->my_tunnel == NULL) {
			continue;
		}

		/* Call any registered hooks */
		if (l2tp_session_modified_hook != NULL) {
			l2tp_session_modified_hook(session, l2tp_tunnel_id(session->my_tunnel), session->status.session_id);
		}

		/* Handle changes to tunnel persist setting. */
		if (session->status.persist && !l2tp_tunnel_is_persistent(tunnel)) {
			session->status.persist = 0;
			l2tp_session_dec_use_count(session);
		}
	}
}

/* For use by plugins to tell that a session should be taken down.
 */
void l2tp_session_close_event(uint16_t tunnel_id, uint16_t session_id)
{
	struct l2tp_session *session;

	session = l2tp_session_find(tunnel_id, session_id);
	if (session != NULL) {
		l2tp_session_queue_event(tunnel_id, session_id, session->close_event);
	}
}

/*****************************************************************************
 * Management API
 * All l2tp_session_xxx_1_svc() functions are RPC callbacks.
 *****************************************************************************/

/* Modify session global parameters. We come here via the handler for
 * l2tp_app_config_modify().
 */
void l2tp_session_globals_modify(struct l2tp_api_system_msg_data *msg, int *result)
{
	if (msg->config.flags & L2TP_API_CONFIG_FLAG_MAX_SESSIONS) {
		l2tp_session_max_count = msg->config.max_sessions;
	}
	if (msg->config.flags & L2TP_API_CONFIG_FLAG_SESSION_ESTABLISH_TIMEOUT) {
		l2tp_session_establish_timeout = msg->config.session_establish_timeout;
	}
	if (msg->config.flags & L2TP_API_CONFIG_FLAG_SESSION_PERSIST_PEND_TIMEOUT) {
		l2tp_session_persist_pend_timeout = msg->config.session_persist_pend_timeout;
	}
}

/* Get session global parameters. We come here via the handler for
 * l2tp_app_config_get().
 */
void l2tp_session_globals_get(struct l2tp_api_system_msg_data *msg)
{
	msg->config.max_sessions = l2tp_session_max_count;
	msg->config.session_establish_timeout = l2tp_session_establish_timeout;
	msg->config.session_persist_pend_timeout = l2tp_session_persist_pend_timeout;
	msg->status.num_sessions = l2tp_session_count;
}

/* Internal helper to set (modify) a session's config. Only supplied
 * elements in the passed data structure are copied. Called by Create
 * and Modify management operations.
 */
static int l2tp_session_config_set(struct l2tp_session *session, struct l2tp_api_session_msg_data *msg, int create)
{
	int result = 0;

	if (create) {
		/* Optional create parameters. These can never be modified. */
		if (msg->flags & L2TP_API_SESSION_FLAG_PROFILE_NAME) {
			l2tp_session_log(session, L2TP_FUNC, LOG_INFO,
					 "FUNC: tunl %s: using session profile '%s'",
					 session->fsmi.name, session->config.profile_name);
			L2TP_SET_OPTSTRING_VAR(&session->config, profile_name);
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_PPP_PROFILE_NAME) {
			l2tp_session_log(session, L2TP_FUNC, LOG_INFO,
					 "FUNC: tunl %s: using ppp profile '%s'",
					 session->fsmi.name, session->config.ppp_profile_name);
			L2TP_SET_OPTSTRING_VAR(&session->config, ppp_profile_name);
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_PRIV_GROUP_ID) {
			L2TP_SET_OPTSTRING_VAR(&session->config, priv_group_id);
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_USER_NAME) {
			L2TP_SET_OPTSTRING_VAR(&session->config, user_name);
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_USER_PASSWORD) {
			L2TP_SET_OPTSTRING_VAR(&session->config, user_password);
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_INTERFACE_NAME) {
			L2TP_SET_OPTSTRING_VAR(&session->config, interface_name);
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_FRAMING_TYPE) {
			session->config.framing_type_async = msg->framing_type_async ? -1 : 0;
			session->config.framing_type_sync = msg->framing_type_sync ? -1 : 0;
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_BEARER_TYPE) {
			session->config.bearer_type_digital = msg->bearer_type_digital ? -1 : 0;
			session->config.bearer_type_analog = msg->bearer_type_analog ? -1 : 0;
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_PROXY_AUTH_NAME) {
			L2TP_SET_OPTSTRING_VAR(&session->config, proxy_auth_name);
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_PROXY_AUTH_TYPE) {
			session->config.proxy_auth_type = msg->proxy_auth_type;
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_CALLING_NUMBER) {
			L2TP_SET_OPTSTRING_VAR(&session->config, calling_number);
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_CALLED_NUMBER) {
			L2TP_SET_OPTSTRING_VAR(&session->config, called_number);
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_SUB_ADDRESS) {
			L2TP_SET_OPTSTRING_VAR(&session->config, sub_address);
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_PROXY_AUTH_CHALLENGE) {
			if (msg->proxy_auth_challenge.proxy_auth_challenge_len > 0) {
				session->config.proxy_auth_challenge = malloc(msg->proxy_auth_challenge.proxy_auth_challenge_len);
				if (session->config.proxy_auth_challenge == NULL) {
					result = -ENOMEM;
					goto out;
				}
				memcpy(&session->config.proxy_auth_challenge[0], 
				       &msg->proxy_auth_challenge.proxy_auth_challenge_val[0],
				       msg->proxy_auth_challenge.proxy_auth_challenge_len);
				session->config.proxy_auth_challenge_len = msg->proxy_auth_challenge.proxy_auth_challenge_len;
			}
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_PROXY_AUTH_RESPONSE) {
			if (msg->proxy_auth_response.proxy_auth_response_len > 0) {
				session->config.proxy_auth_response = malloc(msg->proxy_auth_response.proxy_auth_response_len);
				if (session->config.proxy_auth_response == NULL) {
					result = -ENOMEM;
					goto out;
				}
				memcpy(&session->config.proxy_auth_response[0], 
				       &msg->proxy_auth_response.proxy_auth_response_val[0],
				       msg->proxy_auth_response.proxy_auth_response_len);
				session->config.proxy_auth_response_len = msg->proxy_auth_response.proxy_auth_response_len;
			}
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_INITIAL_RCVD_LCP_CONFREQ) {
			if (msg->initial_rcvd_lcp_confreq.initial_rcvd_lcp_confreq_len > 0) {
				session->config.initial_rcvd_lcp_confreq = malloc(msg->initial_rcvd_lcp_confreq.initial_rcvd_lcp_confreq_len);
				if (session->config.initial_rcvd_lcp_confreq == NULL) {
					result = -ENOMEM;
					goto out;
				}
				memcpy(session->config.initial_rcvd_lcp_confreq, 
				       &msg->initial_rcvd_lcp_confreq.initial_rcvd_lcp_confreq_val[0], 
				       msg->initial_rcvd_lcp_confreq.initial_rcvd_lcp_confreq_len);
				session->config.initial_rcvd_lcp_confreq_len = msg->initial_rcvd_lcp_confreq.initial_rcvd_lcp_confreq_len;
			}
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_LAST_RCVD_LCP_CONFREQ) {
			if (msg->last_rcvd_lcp_confreq.last_rcvd_lcp_confreq_len > 0) {
				session->config.last_rcvd_lcp_confreq = malloc(msg->last_rcvd_lcp_confreq.last_rcvd_lcp_confreq_len);
				if (session->config.last_rcvd_lcp_confreq == NULL) {
					result = -ENOMEM;
					goto out;
				}
				memcpy(session->config.last_rcvd_lcp_confreq, 
				       &msg->last_rcvd_lcp_confreq.last_rcvd_lcp_confreq_val[0], 
				       msg->last_rcvd_lcp_confreq.last_rcvd_lcp_confreq_len);
				session->config.last_rcvd_lcp_confreq_len = msg->last_rcvd_lcp_confreq.last_rcvd_lcp_confreq_len;
			}
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_LAST_SENT_LCP_CONFREQ) {
			if (msg->last_sent_lcp_confreq.last_sent_lcp_confreq_len > 0) {
				session->config.last_sent_lcp_confreq = malloc(msg->last_sent_lcp_confreq.last_sent_lcp_confreq_len);
				if (session->config.last_sent_lcp_confreq == NULL) {
					result = -ENOMEM;
					goto out;
				}
				memcpy(session->config.last_sent_lcp_confreq, 
				       &msg->last_sent_lcp_confreq.last_sent_lcp_confreq_val[0], 
				       msg->last_sent_lcp_confreq.last_sent_lcp_confreq_len);
				session->config.last_sent_lcp_confreq_len = msg->last_sent_lcp_confreq.last_sent_lcp_confreq_len;
			}
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_USE_PPP_PROXY) {
			session->config.use_ppp_proxy = msg->use_ppp_proxy;
		}
		if (msg->flags & L2TP_API_SESSION_FLAG_NO_PPP) {
			session->config.no_ppp = msg->no_ppp;
		}
	} else {
		if (msg->flags & (L2TP_API_SESSION_FLAG_PROFILE_NAME |
				  L2TP_API_SESSION_FLAG_PPP_PROFILE_NAME |
				  L2TP_API_SESSION_FLAG_PRIV_GROUP_ID |
				  L2TP_API_SESSION_FLAG_USER_NAME |
				  L2TP_API_SESSION_FLAG_USER_PASSWORD |
				  L2TP_API_SESSION_FLAG_INTERFACE_NAME |
				  L2TP_API_SESSION_FLAG_FRAMING_TYPE |
				  L2TP_API_SESSION_FLAG_BEARER_TYPE |
				  L2TP_API_SESSION_FLAG_USE_PPP_PROXY |
				  L2TP_API_SESSION_FLAG_NO_PPP |
				  L2TP_API_SESSION_FLAG_CALLING_NUMBER |
				  L2TP_API_SESSION_FLAG_CALLED_NUMBER |
				  L2TP_API_SESSION_FLAG_SUB_ADDRESS |
				  L2TP_API_SESSION_FLAG_PROXY_AUTH_NAME |
				  L2TP_API_SESSION_FLAG_PROXY_AUTH_TYPE |
				  L2TP_API_SESSION_FLAG_PROXY_AUTH_CHALLENGE |
				  L2TP_API_SESSION_FLAG_PROXY_AUTH_RESPONSE |
				  L2TP_API_SESSION_FLAG_INITIAL_RCVD_LCP_CONFREQ |
				  L2TP_API_SESSION_FLAG_LAST_RCVD_LCP_CONFREQ |
				  L2TP_API_SESSION_FLAG_LAST_SENT_LCP_CONFREQ)) {
			result = -L2TP_ERR_PARAM_NOT_MODIFIABLE;
			goto out;
		}
	}

	/* The following parameters are modifiable */

	if (msg->flags & L2TP_API_SESSION_FLAG_TRACE_FLAGS) {
		if (msg->trace_flags_mask == 0) {
			msg->trace_flags_mask = 0xffffffff;
		}
		session->config.trace_flags &= ~(msg->trace_flags_mask);
		session->config.trace_flags |= (msg->trace_flags & msg->trace_flags_mask);
	}
	if (msg->flags & L2TP_API_SESSION_FLAG_SEQUENCING_REQUIRED) {
		session->config.sequencing_required = msg->sequencing_required;
	}
	if (msg->flags & L2TP_API_SESSION_FLAG_USE_SEQUENCE_NUMBERS) {
		session->config.use_sequence_numbers = msg->use_sequence_numbers;
	}
	if (msg->flags & L2TP_API_SESSION_FLAG_REORDER_TIMEOUT) {
		session->config.reorder_timeout = msg->reorder_timeout;
	}
	if (msg->flags & L2TP_API_SESSION_FLAG_SESSION_NAME) {
		L2TP_SET_OPTSTRING_VAR(&session->config, session_name);
	}
	if (msg->flags & L2TP_API_SESSION_FLAG_MINIMUM_BPS) {
		session->config.minimum_bps = msg->minimum_bps;
	}
	if (msg->flags & L2TP_API_SESSION_FLAG_MAXIMUM_BPS) {
		session->config.maximum_bps = msg->maximum_bps;
	}
	if (msg->flags & L2TP_API_SESSION_FLAG_CONNECT_SPEED) {
		session->config.connect_speed = msg->tx_connect_speed;
		if (msg->rx_connect_speed > 0) {
			session->config.rx_connect_speed = msg->rx_connect_speed;
		} else {
			session->config.rx_connect_speed = session->config.connect_speed;
		}
	}

out:
	if (result == -ENOMEM) {
		l2tp_stats.no_session_resources++;
	}
	return result;
}

/* Sessions can be referenced by session_id or session_name. This function is
 * called by each management operation to locate the session instance.
 * If both id and name are supplied, just use the id.
 */
static struct l2tp_session *l2tp_session_get_instance(struct l2tp_tunnel *tunnel,
						     uint16_t session_id, char *session_name)
{
	struct l2tp_session *session = NULL;

	/* If caller specified a session_id, find the session by id. */
	if (session_id != 0) {
		session = l2tp_session_find_by_id(tunnel, session_id);
		if (session != NULL) {
			goto out;
		}
	}

	/* If caller specified a name, find the session by name. */
	if (session_name != NULL) {
		session = l2tp_session_find_by_name(tunnel, session_name);
	}

out:
	return session;
}

bool_t l2tp_session_create_1_svc(struct l2tp_api_session_msg_data msg, int *retval, struct svc_req *req)
{
	struct l2tp_tunnel *tunnel = NULL;
	struct l2tp_session *session = NULL;
	int result = 0;

	L2TP_DEBUG(L2TP_API, "%s: tunl %hu(%s)/%hu", __func__, msg.tunnel_id, 
		   OPTSTRING_PTR(msg.tunnel_name) ? OPTSTRING_PTR(msg.tunnel_name) : "",
		   msg.session_id);

	if ((msg.tunnel_id == 0) && (!msg.tunnel_name.valid)) {
		result = -L2TP_ERR_TUNNEL_SPEC_MISSING;
		goto out;
	}

	/* Find the tunnel context */
	if (msg.tunnel_id != 0) {
		tunnel = l2tp_tunnel_find_by_id(msg.tunnel_id);
	} else {
		tunnel = l2tp_tunnel_find_by_name(OPTSTRING_PTR(msg.tunnel_name));
	}
	if (tunnel == NULL) {
		result = -L2TP_ERR_TUNNEL_NOT_FOUND;
		goto out;
	}

	/* If caller specified a session_id or name, make sure that it doesn't exist */
	session = l2tp_session_get_instance(tunnel, msg.session_id, OPTSTRING_PTR(msg.session_name));
	if (session != NULL) {
		result = -L2TP_ERR_SESSION_ALREADY_EXISTS;
		goto out;
	}

	/* LAC Outgoing Call and LNS Incoming Call session types are created by OCRQ and ICRQ
	 * requests from the network so disallow attempts to create those types.
	 */
	if ((msg.session_type == L2TP_API_SESSION_TYPE_LAOC) ||
	    (msg.session_type == L2TP_API_SESSION_TYPE_LNIC)) {
		result = -L2TP_ERR_SESSION_TYPE_BAD;
		l2tp_tunnel_log(tunnel, L2TP_API, LOG_ERR, "API: tunl %hu: Illegal session type %d", l2tp_tunnel_id(tunnel), msg.session_type);
		goto out;
	}

	/* Allocate a new session context */
	session = l2tp_session_alloc(tunnel, msg.session_type, msg.session_id, OPTSTRING_PTR(msg.profile_name), 1, &result);
	if (session == NULL) {
		goto out;
	}

	result = l2tp_session_config_set(session, &msg, 1);
	if (result < 0) {
		l2tp_session_dec_use_count(session);
		goto out;
	}

	l2tp_session_handle_event(session, session->open_event);

	result = l2tp_session_link(session);
	if (result < 0) {
		l2tp_session_handle_event(session, session->close_event);
		goto out;
	}

	/* Give plugins visibility of session created */
	result = l2tp_session_created_ind(tunnel, session);
	if ((result != -EEXIST) && (result < 0)) {
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, L2TP_AVP_ERROR_VENDOR_ERROR, 
					"failed to start ppp");
		l2tp_session_queue_event(l2tp_tunnel_id(tunnel), session->status.session_id, session->close_event);
		goto out;
	}

	/* Remember all non-default parameters */
	session->config.flags |= msg.flags;

	result = session->status.session_id;

out:
	*retval = result;
	L2TP_DEBUG(L2TP_API, "%s: result=%d", __func__, *retval);

	return TRUE;
}

/* Alternative interface for creating sessions, for use by applications such
 * as a B-RAS to create sessions for tunneling incoming PPP sessions.
 */
bool_t l2tp_session_incall_ind_1_svc(struct l2tp_api_session_incall_msg_data msg, int *result, struct svc_req *req)
{
	*result = -EOPNOTSUPP;
	return FALSE;
}

bool_t l2tp_session_delete_1_svc(u_short tunnel_id, optstring tunnel_name, 
				 u_short session_id, optstring session_name,
				 optstring reason, int *result, struct svc_req *req)
{
	struct l2tp_tunnel *tunnel = NULL;
	struct l2tp_session *session = NULL;

	if ((tunnel_id == 0) && (!tunnel_name.valid)) {
		*result = -L2TP_ERR_TUNNEL_SPEC_MISSING;
		goto out;
	}
	if ((session_id == 0) && (!session_name.valid)) {
		*result = -L2TP_ERR_SESSION_SPEC_MISSING;
		goto out;
	}

	/* Find the tunnel context */
	if (tunnel_id != 0) {
		tunnel = l2tp_tunnel_find_by_id(tunnel_id);
	} else {
		tunnel = l2tp_tunnel_find_by_name(OPTSTRING_PTR(tunnel_name));
	}
	if (tunnel == NULL) {
		*result = -L2TP_ERR_TUNNEL_NOT_FOUND;
		goto out;
	}

	/* Get the session context */
	session = l2tp_session_get_instance(tunnel, session_id, OPTSTRING_PTR(session_name));
	if (session == NULL) {
		*result = -L2TP_ERR_SESSION_NOT_FOUND;
		goto out;
	}

#ifdef DEBUG
	if (session->my_tunnel != tunnel) {
		l2tp_log(LOG_ERR, "L2TP: Corrupted session context: session %hu/%hu", 
			 l2tp_tunnel_id(session->my_tunnel), session->status.session_id);
	}
#endif

	if (OPTSTRING_PTR(reason)) {
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_GENERAL_ERROR, 0, OPTSTRING_PTR(reason));
	} else {
		l2tp_session_set_result(session, L2TP_AVP_RESULT_CDN_ADMIN, 0, NULL);
	}

	/* Hold the session while we deal with closing it */
	l2tp_session_inc_use_count(session);

	/* Change in tunnel persist status */
	if (session->status.persist) {
		session->status.persist = 0;
		l2tp_session_dec_use_count(session);
	}

	l2tp_session_handle_event(session, session->close_event);

	/* We're done closing it now */
	l2tp_session_dec_use_count(session);

	*result = 0;

out:	
	L2TP_DEBUG(L2TP_API, "%s: result=%d", __func__, *result);
	return TRUE;
}

bool_t l2tp_session_modify_1_svc(struct l2tp_api_session_msg_data msg, int *result, struct svc_req *req)
{
	struct l2tp_tunnel *tunnel = NULL;
	struct l2tp_session *session = NULL;

	if ((msg.tunnel_id == 0) && (!msg.tunnel_name.valid)) {
		*result = -L2TP_ERR_TUNNEL_SPEC_MISSING;
		goto out;
	}
	if ((msg.session_id == 0) && (!msg.session_name.valid)) {
		*result = -L2TP_ERR_SESSION_SPEC_MISSING;
		goto out;
	}

	/* Find the tunnel context */
	if (msg.tunnel_id != 0) {
		tunnel = l2tp_tunnel_find_by_id(msg.tunnel_id);
	} else {
		tunnel = l2tp_tunnel_find_by_name(OPTSTRING_PTR(msg.tunnel_name));
	}
	if (tunnel == NULL) {
		*result = -L2TP_ERR_TUNNEL_NOT_FOUND;
		goto out;
	}

	/* Get the session context */
	session = l2tp_session_get_instance(tunnel, msg.session_id, OPTSTRING_PTR(msg.session_name));
	if (session == NULL) {
		*result = -L2TP_ERR_SESSION_NOT_FOUND;
		goto out;
	}

	*result = l2tp_session_config_set(session, &msg, 0);
	if (*result < 0) {
		goto out;
	}	

	/* Remember all non-default parameters */
	session->config.flags |= msg.flags;

	/* Give plugins visibility of session modified */
	if (l2tp_session_modified_hook != NULL) {
		(*l2tp_session_modified_hook)(session, l2tp_tunnel_id(session->my_tunnel), session->status.session_id);
	}
out:
	L2TP_DEBUG(L2TP_API, "%s: result=%d", __func__, *result);
	return TRUE;
}

/*
 * Typically used by plugins when using the session hooks.
 * Note that any non-null pointers in the result struct are allocated and so must be freed 
 * by the caller.
 */
int l2tp_session_info_get(struct l2tp_session const *session, uint16_t tunnel_id, uint16_t session_id, struct l2tp_api_session_msg_data *result)
{
	if (session == NULL) {
		if ((tunnel_id == 0) || (session_id == 0)) {
			result->result_code = -L2TP_ERR_SESSION_SPEC_MISSING;
			goto out;
		}

		session = l2tp_session_find(tunnel_id, session_id);
		if (session == NULL) {
			result->result_code = -L2TP_ERR_SESSION_NOT_FOUND;
			goto out;
		}
	}
	
	memset(result, 0, sizeof(*result));
	result->flags = session->config.flags;
	result->tunnel_id = tunnel_id;
	result->session_id = session_id;
	result->peer_session_id = session->status.peer_session_id;
	OPTSTRING(result->state) = strdup(usl_fsm_state_name((void *) &session->fsmi));
	result->state.valid = 1;
	result->session_type = session->type;
	result->trace_flags = session->config.trace_flags;
	if (session->config.profile_name != NULL) {
		OPTSTRING(result->profile_name) = strdup(session->config.profile_name);
		if (OPTSTRING(result->profile_name) == NULL) {
			goto nomem;
		}
		result->profile_name.valid = 1;
	} else {
		result->profile_name.valid = 0;
	}
	if (session->config.ppp_profile_name != NULL) {
		OPTSTRING(result->ppp_profile_name) = strdup(session->config.ppp_profile_name);
		if (OPTSTRING(result->ppp_profile_name) == NULL) {
			goto nomem;
		}
		result->ppp_profile_name.valid = 1;
	} else {
		result->ppp_profile_name.valid = 0;
	}
	result->created_by_admin = session->status.created_by_admin;
	result->sequencing_required = session->config.sequencing_required;
	result->use_sequence_numbers = session->config.use_sequence_numbers;
	result->no_ppp = session->config.no_ppp;
	result->reorder_timeout = session->config.reorder_timeout;
	result->framing_type_sync = session->config.framing_type_sync ? 1 : 0;
	result->framing_type_async = session->config.framing_type_async ? 1 : 0;
	result->bearer_type_digital = session->config.bearer_type_digital ? 1 : 0;
	result->bearer_type_analog = session->config.bearer_type_analog ? 1 : 0;
	result->call_serial_number = session->status.call_serial_number;
#ifdef L2TP_TEST
	if (l2tp_test_is_no_random_ids()) {
		result->call_serial_number = 0;
	}
#endif
	result->physical_channel_id = session->status.physical_channel_id;
	result->trace_flags = session->config.trace_flags;
	result->minimum_bps = session->config.minimum_bps;
	result->maximum_bps = session->config.maximum_bps;
	result->tx_connect_speed = session->config.connect_speed;
	result->rx_connect_speed = session->config.rx_connect_speed;
	if (session->config.priv_group_id != NULL) {
		OPTSTRING(result->priv_group_id) = strdup(session->config.priv_group_id);
		if (OPTSTRING(result->priv_group_id) == NULL) {
			goto nomem;
		}
		result->priv_group_id.valid = 1;
	} else {
		result->priv_group_id.valid = 0;
	}
	if (session->config.user_name != NULL) {
		OPTSTRING(result->user_name) = strdup(session->config.user_name);
		if (OPTSTRING(result->user_name) == NULL) {
			goto nomem;
		}
		result->user_name.valid = 1;
	} else {
		result->user_name.valid = 0;
	}
	if (session->config.session_name != NULL) {
		OPTSTRING(result->session_name) = strdup(session->config.session_name);
		if (OPTSTRING(result->session_name) == NULL) {
			goto nomem;
		}
		result->session_name.valid = 1;
	} else {
		result->session_name.valid = 0;
	}
	if (session->config.interface_name != NULL) {
		OPTSTRING(result->interface_name) = strdup(session->config.interface_name);
		if (OPTSTRING(result->interface_name) == NULL) {
			goto nomem;
		}
		result->interface_name.valid = 1;
	} else {
		result->interface_name.valid = 0;
	}
	if (session->config.user_password != NULL) {
		OPTSTRING(result->user_password) = strdup(session->config.user_password);
		if (OPTSTRING(result->user_password) == NULL) {
			goto nomem;
		}
		result->user_password.valid = 1;
	} else {
		result->user_password.valid = 0;
	}
	result->use_ppp_proxy = session->config.use_ppp_proxy;
	result->proxy_auth_type = session->config.proxy_auth_type;
	if (session->config.proxy_auth_name != NULL) {
		OPTSTRING(result->proxy_auth_name) = strdup(session->config.proxy_auth_name);
		if (OPTSTRING(result->proxy_auth_name) == NULL) {
			goto nomem;
		}
		result->proxy_auth_name.valid = 1;
	} else {
		result->proxy_auth_name.valid = 0;
	}
	if (session->config.proxy_auth_challenge != NULL) {
		result->proxy_auth_challenge.proxy_auth_challenge_val = malloc(session->config.proxy_auth_challenge_len);
		if (result->proxy_auth_challenge.proxy_auth_challenge_val == NULL) {
			goto nomem;
		}
		memcpy(&result->proxy_auth_challenge.proxy_auth_challenge_val[0],
		       session->config.proxy_auth_challenge,
		       session->config.proxy_auth_challenge_len);
		result->proxy_auth_challenge.proxy_auth_challenge_len = session->config.proxy_auth_challenge_len;
	} else {
		result->proxy_auth_challenge.proxy_auth_challenge_val = NULL;
		result->proxy_auth_challenge.proxy_auth_challenge_len = 0;
	}
	if (session->config.proxy_auth_response != NULL) {
		result->proxy_auth_response.proxy_auth_response_val = malloc(session->config.proxy_auth_response_len);
		if (result->proxy_auth_response.proxy_auth_response_val == NULL) {
			goto nomem;
		}
		memcpy(&result->proxy_auth_response.proxy_auth_response_val[0],
		       session->config.proxy_auth_response,
		       session->config.proxy_auth_response_len);
		result->proxy_auth_response.proxy_auth_response_len = session->config.proxy_auth_response_len;
	} else {
		result->proxy_auth_response.proxy_auth_response_val = NULL;
		result->proxy_auth_response.proxy_auth_response_len = 0;
	}
	if (session->config.calling_number != NULL) {
		OPTSTRING(result->calling_number) = strdup(session->config.calling_number);
		if (OPTSTRING(result->calling_number) == NULL) {
			goto nomem;
		}
		result->calling_number.valid = 1;
	} else {
		result->calling_number.valid = 0;
	}
	if (session->config.called_number != NULL) {
		OPTSTRING(result->called_number) = strdup(session->config.called_number);
		if (OPTSTRING(result->called_number) == NULL) {
			goto nomem;
		}
		result->called_number.valid = 1;
	} else {
		result->called_number.valid = 0;
	}
	if (session->config.sub_address != NULL) {
		OPTSTRING(result->sub_address) = strdup(session->config.sub_address);
		if (OPTSTRING(result->sub_address) == NULL) {
			goto nomem;
		}
		result->sub_address.valid = 1;
	} else {
		result->sub_address.valid = 0;
	}
	result->initial_rcvd_lcp_confreq.initial_rcvd_lcp_confreq_len = session->config.initial_rcvd_lcp_confreq_len;
	if (session->config.initial_rcvd_lcp_confreq_len > 0) {
		result->initial_rcvd_lcp_confreq.initial_rcvd_lcp_confreq_val = malloc(session->config.initial_rcvd_lcp_confreq_len);
		if (result->initial_rcvd_lcp_confreq.initial_rcvd_lcp_confreq_val == NULL) {
			goto nomem;
		}
		memcpy(result->initial_rcvd_lcp_confreq.initial_rcvd_lcp_confreq_val, 
		       session->config.initial_rcvd_lcp_confreq,
		       session->config.initial_rcvd_lcp_confreq_len);
	}
	result->last_rcvd_lcp_confreq.last_rcvd_lcp_confreq_len = session->config.last_rcvd_lcp_confreq_len;
	if (session->config.last_rcvd_lcp_confreq_len > 0) {
		result->last_rcvd_lcp_confreq.last_rcvd_lcp_confreq_val = malloc(session->config.last_rcvd_lcp_confreq_len);
		if (result->last_rcvd_lcp_confreq.last_rcvd_lcp_confreq_val == NULL) {
			goto nomem;
		}
		memcpy(result->last_rcvd_lcp_confreq.last_rcvd_lcp_confreq_val, 
		       session->config.last_rcvd_lcp_confreq,
		       session->config.last_rcvd_lcp_confreq_len);
	}
	result->last_sent_lcp_confreq.last_sent_lcp_confreq_len = session->config.last_sent_lcp_confreq_len;
	if (session->config.last_sent_lcp_confreq_len > 0) {
		result->last_sent_lcp_confreq.last_sent_lcp_confreq_val = malloc(session->config.last_sent_lcp_confreq_len);
		if (result->last_sent_lcp_confreq.last_sent_lcp_confreq_val == NULL) {
			goto nomem;
		}
		memcpy(result->last_sent_lcp_confreq.last_sent_lcp_confreq_val, 
		       session->config.last_sent_lcp_confreq,
		       session->config.last_sent_lcp_confreq_len);
	}
	if (session->result_code != NULL) {
		result->peer.result_code = session->result_code->result_code;
		result->peer.error_code = session->result_code->error_code;
		if (session->result_code_len > 4) { 
			OPTSTRING(result->peer.error_message) = strndup(session->result_code->error_message, session->result_code_len - 4);
			result->peer.error_message.valid = 1;
		} else {
			result->peer.error_message.valid = 0;
		}
	} else {
		result->peer.result_code = 0;
		result->peer.error_code = 0;
		result->peer.error_message.valid = 0;
	}

	/* Don't return the abbreviated weekday (first 4 chars) in the create time */
	if (!l2tp_test_is_no_random_ids()) {
		OPTSTRING(result->create_time) = strdup(&session->status.create_time[3]);
		if (OPTSTRING(result->create_time) == NULL) {
			goto nomem;
		}
		result->create_time.valid = 1;
	}

	/* Fill in peer section */
	result->peer.minimum_bps = session->peer.minimum_bps.value;
	result->peer.maximum_bps = session->peer.maximum_bps.value;
	result->peer.connect_speed = session->peer.connect_speed.value;
	result->peer.rx_connect_speed = session->peer.rx_connect_speed.value;
	result->peer.proxy_auth_type = session->peer.proxy_auth_type.value;
	result->peer.sequencing_required = session->peer.sequencing_required;
	result->peer.proxy_auth_id = session->peer.proxy_auth_id.id;
	if (session->peer.proxy_auth_name != NULL) {
		OPTSTRING(result->peer.proxy_auth_name) = strdup(session->peer.proxy_auth_name->string);
		if (OPTSTRING(result->peer.proxy_auth_name) == NULL) {
			goto nomem;
		}
		result->peer.proxy_auth_name.valid = 1;
	} else {
		result->peer.proxy_auth_name.valid = 0;
	}
	if (session->peer.proxy_auth_challenge != NULL) {
		result->peer.proxy_auth_challenge.proxy_auth_challenge_val = malloc(session->peer.proxy_auth_challenge_len);
		if (result->peer.proxy_auth_challenge.proxy_auth_challenge_val == NULL) {
			goto nomem;
		}
		memcpy(&result->peer.proxy_auth_challenge.proxy_auth_challenge_val[0],
		       session->peer.proxy_auth_challenge,
		       session->peer.proxy_auth_challenge_len);
		result->peer.proxy_auth_challenge.proxy_auth_challenge_len = session->peer.proxy_auth_challenge_len;
	} else {
		result->peer.proxy_auth_challenge.proxy_auth_challenge_val = NULL;
		result->peer.proxy_auth_challenge.proxy_auth_challenge_len = 0;
	}
	if (session->peer.proxy_auth_response != NULL) {
		result->peer.proxy_auth_response.proxy_auth_response_val = malloc(session->peer.proxy_auth_response_len);
		if (result->peer.proxy_auth_response.proxy_auth_response_val == NULL) {
			goto nomem;
		}
		memcpy(&result->peer.proxy_auth_response.proxy_auth_response_val[0],
		       session->peer.proxy_auth_response,
		       session->peer.proxy_auth_response_len);
		result->peer.proxy_auth_response.proxy_auth_response_len = session->peer.proxy_auth_response_len;
	} else {
		result->peer.proxy_auth_response.proxy_auth_response_val = NULL;
		result->peer.proxy_auth_response.proxy_auth_response_len = 0;
	}
	if (session->peer.priv_group_id != NULL) {
		OPTSTRING(result->peer.private_group_id) = strdup(session->peer.priv_group_id->string);
		if (OPTSTRING(result->peer.private_group_id) == NULL) {
			goto nomem;
		}
		result->peer.private_group_id.valid = 1;
	} else {
		result->peer.private_group_id.valid = 0;
	}
	result->peer.framing_type_sync = session->peer.framing_type.bits.sync ? 1 : 0;
	result->peer.framing_type_async = session->peer.framing_type.bits.async ? 1 : 0;
	result->peer.bearer_type_digital = session->peer.bearer_type.bits.digital ? 1 : 0;
	result->peer.bearer_type_analog = session->peer.bearer_type.bits.analog ? 1 : 0;
	result->peer.call_serial_number = session->status.call_serial_number;
#ifdef L2TP_TEST
	if (l2tp_test_is_no_random_ids()) {
		result->peer.call_serial_number = 0;
	}
#endif
	result->peer.physical_channel_id = session->status.physical_channel_id;
	result->peer.initial_rcvd_lcp_confreq.initial_rcvd_lcp_confreq_len = session->peer.initial_rcvd_lcp_confreq_len;
	if (session->peer.initial_rcvd_lcp_confreq_len > 0) {
		result->peer.initial_rcvd_lcp_confreq.initial_rcvd_lcp_confreq_val = malloc(session->peer.initial_rcvd_lcp_confreq_len);
		if (result->peer.initial_rcvd_lcp_confreq.initial_rcvd_lcp_confreq_val == NULL) {
			goto nomem;
		}
		memcpy(result->peer.initial_rcvd_lcp_confreq.initial_rcvd_lcp_confreq_val, 
		       session->peer.initial_rcvd_lcp_confreq,
		       session->peer.initial_rcvd_lcp_confreq_len);
	}
	result->peer.last_rcvd_lcp_confreq.last_rcvd_lcp_confreq_len = session->peer.last_rcvd_lcp_confreq_len;
	if (session->peer.last_rcvd_lcp_confreq_len > 0) {
		result->peer.last_rcvd_lcp_confreq.last_rcvd_lcp_confreq_val = malloc(session->peer.last_rcvd_lcp_confreq_len);
		if (result->peer.last_rcvd_lcp_confreq.last_rcvd_lcp_confreq_val == NULL) {
			goto nomem;
		}
		memcpy(result->peer.last_rcvd_lcp_confreq.last_rcvd_lcp_confreq_val, 
		       session->peer.last_rcvd_lcp_confreq,
		       session->peer.last_rcvd_lcp_confreq_len);
	}
	result->peer.last_sent_lcp_confreq.last_sent_lcp_confreq_len = session->peer.last_sent_lcp_confreq_len;
	if (session->peer.last_sent_lcp_confreq_len > 0) {
		result->peer.last_sent_lcp_confreq.last_sent_lcp_confreq_val = malloc(session->peer.last_sent_lcp_confreq_len);
		if (result->peer.last_sent_lcp_confreq.last_sent_lcp_confreq_val == NULL) {
			goto nomem;
		}
		memcpy(result->peer.last_sent_lcp_confreq.last_sent_lcp_confreq_val, 
		       session->peer.last_sent_lcp_confreq,
		       session->peer.last_sent_lcp_confreq_len);
	}
	if (session->result_code != NULL) {
		result->peer.result_code = session->result_code->result_code;
		result->peer.error_code = session->result_code->error_code;
		if (session->result_code_len > 4) { 
			OPTSTRING(result->peer.error_message) = strndup(session->result_code->error_message, session->result_code_len - 4);
			result->peer.error_message.valid = 1;
		} else {
			result->peer.error_message.valid = 0;
		}
	} else {
		result->peer.result_code = 0;
		result->peer.error_code = 0;
		result->peer.error_message.valid = 0;
	}
	if (session->peer.calling_number != NULL) {
		OPTSTRING(result->peer.calling_number) = strdup(session->peer.calling_number->string);
		if (OPTSTRING(result->peer.calling_number) == NULL) {
			goto nomem;
		}
		result->peer.calling_number.valid = 1;
	} else {
		result->peer.calling_number.valid = 0;
	}
	if (session->peer.called_number != NULL) {
		OPTSTRING(result->peer.called_number) = strdup(session->peer.called_number->string);
		if (OPTSTRING(result->peer.called_number) == NULL) {
			goto nomem;
		}
		result->peer.called_number.valid = 1;
	} else {
		result->peer.called_number.valid = 0;
	}
	if (session->peer.sub_address != NULL) {
		OPTSTRING(result->peer.sub_address) = strdup(session->peer.sub_address->string);
		if (OPTSTRING(result->peer.sub_address) == NULL) {
			goto nomem;
		}
		result->peer.sub_address.valid = 1;
	} else {
		result->peer.sub_address.valid = 0;
	}
	if (session->peer.call_errors != NULL) {
		result->peer.last_sent_lcp_confreq.last_sent_lcp_confreq_val = malloc(sizeof(*session->peer.call_errors));
		if (result->peer.last_sent_lcp_confreq.last_sent_lcp_confreq_val == NULL) {
			goto nomem;
		}
		memcpy(&result->peer.call_errors, session->peer.call_errors, sizeof(*session->peer.call_errors));
	}
	if (session->peer.accm != NULL) {
		result->peer.send_accm = session->peer.accm->send_accm;
		result->peer.recv_accm = session->peer.accm->recv_accm;
	}
	result->peer.q931_advisory_msg.valid = 0;
	if (session->peer.q931_cause_code != NULL) {
		result->peer.q931_cause_code = session->peer.q931_cause_code->cause_code;
		result->peer.q931_cause_msg = session->peer.q931_cause_code->cause_msg;
		OPTSTRING(result->peer.q931_advisory_msg) = strdup(session->peer.q931_cause_code->advisory_msg);
		if (OPTSTRING(result->peer.q931_advisory_msg) == NULL) {
			goto nomem;
		}
		result->peer.q931_advisory_msg.valid = 1;
	}

	result->result_code = 0;

out:
	return result->result_code;

nomem:
	result->result_code = -ENOMEM;
	l2tp_session_msg_free(result);
	goto out;
}

void l2tp_session_msg_free(struct l2tp_api_session_msg_data *msg)
{
	if (OPTSTRING(msg->create_time) != NULL) {
		free(OPTSTRING(msg->create_time));
	}
	if (OPTSTRING(msg->profile_name) != NULL) {
		free(OPTSTRING(msg->profile_name));
	}
	if (OPTSTRING(msg->ppp_profile_name) != NULL) {
		free(OPTSTRING(msg->ppp_profile_name));
	}
	if (OPTSTRING(msg->proxy_auth_name) != NULL) {
		free(OPTSTRING(msg->proxy_auth_name));
	}
	if (msg->proxy_auth_challenge.proxy_auth_challenge_val != NULL) {
		free(msg->proxy_auth_challenge.proxy_auth_challenge_val);
	}
	if (msg->proxy_auth_response.proxy_auth_response_val != NULL) {
		free(msg->proxy_auth_response.proxy_auth_response_val);
	}
	if (OPTSTRING(msg->calling_number) != NULL) {
		free(OPTSTRING(msg->calling_number));
	}
	if (OPTSTRING(msg->called_number) != NULL) {
		free(OPTSTRING(msg->called_number));
	}
	if (OPTSTRING(msg->sub_address) != NULL) {
		free(OPTSTRING(msg->sub_address));
	}
	if (msg->initial_rcvd_lcp_confreq.initial_rcvd_lcp_confreq_val != NULL) {
		free(msg->initial_rcvd_lcp_confreq.initial_rcvd_lcp_confreq_val);
	}
	if (msg->last_rcvd_lcp_confreq.last_rcvd_lcp_confreq_val != NULL) {
		free(msg->last_rcvd_lcp_confreq.last_rcvd_lcp_confreq_val);
	}
	if (msg->last_sent_lcp_confreq.last_sent_lcp_confreq_val != NULL) {
		free(msg->last_sent_lcp_confreq.last_sent_lcp_confreq_val);
	}
	if (OPTSTRING(msg->peer.error_message) != NULL) {
		free(OPTSTRING(msg->peer.error_message));
	}
}

bool_t l2tp_session_get_1_svc(u_short tunnel_id, optstring tunnel_name,
			      u_short session_id, optstring session_name,
			      l2tp_api_session_msg_data *result, struct svc_req *req)
{
	struct pppol2tp_ioc_stats stats;
	struct l2tp_tunnel *tunnel = NULL;
	struct l2tp_session *session = NULL;

	memset(result, 0, sizeof(*result));

	if ((tunnel_id == 0) && (!tunnel_name.valid)) {
		result->result_code = -L2TP_ERR_TUNNEL_SPEC_MISSING;
		goto out;
	}
	if ((session_id == 0) && (!session_name.valid)) {
		result->result_code = -L2TP_ERR_SESSION_SPEC_MISSING;
		goto out;
	}

	/* Find the tunnel context */
	if (tunnel_id != 0) {
		tunnel = l2tp_tunnel_find_by_id(tunnel_id);
	} else {
		tunnel = l2tp_tunnel_find_by_name(OPTSTRING_PTR(tunnel_name));
	}
	if (tunnel == NULL) {
		result->result_code = -L2TP_ERR_TUNNEL_NOT_FOUND;
		goto out;
	}
	tunnel_id = l2tp_tunnel_id(tunnel);

	/* Get the session context */
	session = l2tp_session_get_instance(tunnel, session_id, OPTSTRING_PTR(session_name));
	if (session == NULL) {
		result->result_code = -L2TP_ERR_SESSION_NOT_FOUND;
		goto out;
	}
	session_id = session->status.session_id;

	result->result_code = l2tp_session_info_get(session, tunnel_id, session_id, result);
	if (result->result_code == 0) {
		memset(&stats, 0, sizeof(stats));
		if (l2tp_session_get_stats_hook != NULL) {
			if ((*l2tp_session_get_stats_hook)(session, tunnel_id, session_id, &stats) == 0) {
				result->stats.data_rx_packets = stats.rx_packets;
				result->stats.data_rx_bytes = stats.rx_bytes;
				result->stats.data_rx_errors = stats.rx_errors;
				result->stats.data_rx_oos_packets = stats.rx_oos_packets;
				result->stats.data_rx_oos_discards = stats.rx_seq_discards;
				result->stats.data_tx_packets = stats.tx_packets;
				result->stats.data_tx_bytes = stats.tx_bytes;
				result->stats.data_tx_errors = stats.tx_errors;
			}
		}
	}
out:
	L2TP_DEBUG(L2TP_API, "%s: result=%d", __func__, result->result_code);
	return TRUE;
}

bool_t l2tp_session_list_1_svc(u_short tunnel_id, optstring tunnel_name, struct l2tp_api_session_list_msg_data *result, struct svc_req *req)
{
	struct l2tp_tunnel *tunnel;
	struct l2tp_session *session;
	struct usl_list_head *walk;
	struct usl_list_head *list;
	struct usl_list_head *tmp;
	int count = 0;
	int max_count;

	memset(result, 0, sizeof(*result));

	if ((tunnel_id == 0) && (!tunnel_name.valid)) {
		result->result = -L2TP_ERR_TUNNEL_SPEC_MISSING;
		goto out;
	}

	/* Find the tunnel context */
	if (tunnel_id != 0) {
		tunnel = l2tp_tunnel_find_by_id(tunnel_id);
	} else {
		tunnel = l2tp_tunnel_find_by_name(OPTSTRING_PTR(tunnel_name));
	}
	if (tunnel == NULL) {
		result->result = -L2TP_ERR_TUNNEL_NOT_FOUND;
		goto out;
	}
	tunnel_id = l2tp_tunnel_id(tunnel);

	list = l2tp_tunnel_session_list(tunnel);

	/* Count number of sessions in the tunnel */
	max_count = 0;
	usl_list_for_each(walk, tmp, list) {
		max_count++;
	}

	if (max_count == 0) {
		goto out;
	}

	/* Allocate an array to put the session_ids */
	result->session_ids.session_ids_val = malloc(max_count * sizeof(*result->session_ids.session_ids_val));
	if (result->session_ids.session_ids_val == NULL) {
		result->result = -ENOMEM;
		goto out;
	}
	memset(result->session_ids.session_ids_val, 0, max_count * sizeof(*result->session_ids.session_ids_val));

	/* Fill in session_ids array */
	usl_list_for_each(walk, tmp, list) {
		session = usl_list_entry(walk, struct l2tp_session, session_list);
		result->session_ids.session_ids_val[count] = session->status.session_id;
		count++;
		if (count > max_count) {
			/* Another session was added since we counted the list above. */
			break;
		}
	}

out:
	result->session_ids.session_ids_len = count;
	if (count == 0) {
		result->session_ids.session_ids_val = calloc(1, sizeof(result->session_ids.session_ids_val[0]));
		if (result->session_ids.session_ids_val == NULL) {
			return FALSE;
		}
	}

	return TRUE;
}

void l2tp_session_ppp_updown_ind(uint16_t tunnel_id, uint16_t session_id, int unit, int up, char *ifname, char *user_name)
{
	struct l2tp_tunnel *tunnel = NULL;
	struct l2tp_session *session = NULL;

	L2TP_DEBUG(L2TP_API, "tunl %hu/%hu: up=%d, unit=%d ifname=%s username=%s", tunnel_id, session_id, up,
		   unit, ifname ? ifname : "<NULL>", user_name ? user_name : "<NULL>");
	if (tunnel_id == 0) {
		goto out;
	}
	if (session_id == 0) {
		goto out;
	}

	/* Find the tunnel context */
	tunnel = l2tp_tunnel_find_by_id(tunnel_id);
	if (tunnel == NULL) {
		goto out;
	}

	/* Get the session context */
	session = l2tp_session_get_instance(tunnel, session_id, NULL);
	if (session == NULL) {
		goto out;
	}

	/* We use this event to discover the PPP interface name and username. */
	if (up) {
		if ((session->config.interface_name == NULL) &&
		    (ifname != NULL) && (ifname[0] != '\0')) {
			session->config.interface_name = strdup(ifname);
			l2tp_session_log(session, L2TP_FUNC, LOG_INFO, "FUNC: tunl %s: using interface %s", 
					 session->fsmi.name, ifname);
		}
		if ((session->config.user_name == NULL) &&
		    (user_name != NULL) && (user_name[0] != '\0')) {
			session->config.user_name = strdup(user_name);
			l2tp_session_log(session, L2TP_FUNC, LOG_INFO, "FUNC: tunl %s: user is %s", 
					 session->fsmi.name, user_name);
		}
	} else {
		/* Do not clear the configured ifname. If the name was
		 * set above on the up event, the string pointer is
		 * non-NULL but the session's management bitmask
		 * doesn't have the interface_name bit set.
		 */
		if ((session->config.interface_name != NULL) &&
		    ((session->config.flags & L2TP_API_SESSION_FLAG_INTERFACE_NAME) == 0)) {
			free(session->config.interface_name);
			session->config.interface_name = NULL;
		}
	}

	/* Call PPP hooks. */
	if (up) {
		if (l2tp_session_ppp_created_hook != NULL) {
			(*l2tp_session_ppp_created_hook)(session, tunnel_id, session_id, unit);
		}
	} else {
		if (l2tp_session_ppp_deleted_hook != NULL) {
			(*l2tp_session_ppp_deleted_hook)(session, tunnel_id, session_id);
		}
	}
out:
	return;
}

void l2tp_session_ppp_accm_ind(uint16_t tunnel_id, uint16_t session_id, uint32_t send_accm, uint32_t recv_accm)
{
	struct l2tp_tunnel *tunnel = NULL;
	struct l2tp_session *session = NULL;

	L2TP_DEBUG(L2TP_API, "tunl %hu/%hu: send_accm=%x recv_accm=%x", tunnel_id, session_id, send_accm, recv_accm);
	if (tunnel_id == 0) {
		goto out;
	}
	if (session_id == 0) {
		goto out;
	}

	/* Find the tunnel context */
	tunnel = l2tp_tunnel_find_by_id(tunnel_id);
	if (tunnel == NULL) {
		goto out;
	}

	/* Get the session context */
	session = l2tp_session_get_instance(tunnel, session_id, NULL);
	if (session == NULL) {
		goto out;
	}

	session->send_accm = send_accm;
	session->recv_accm = recv_accm;

	l2tp_session_send_sli(tunnel, session);
out:
	return;
}

/******************************************************************************
 * Session profile management API.
 * Profiles simply contain a bunch of configuration parameters. They
 * are created and destroyed only by management request.
 *****************************************************************************/

static int l2tp_session_profile_set(struct l2tp_session_profile *session_profile, struct l2tp_api_session_profile_msg_data *msg)
{
	if (msg->flags & L2TP_API_SESSION_PROFILE_FLAG_TRACE_FLAGS) {
		if (msg->trace_flags_mask == 0) {
			msg->trace_flags_mask = 0xffffffff;
		}
		session_profile->trace_flags &= ~(msg->trace_flags_mask);
		session_profile->trace_flags |= (msg->trace_flags & msg->trace_flags_mask);
	}
	if (msg->flags & L2TP_API_SESSION_PROFILE_FLAG_SEQUENCING_REQUIRED) {
		session_profile->sequencing_required = msg->sequencing_required;
	}
	if (msg->flags & L2TP_API_SESSION_PROFILE_FLAG_USE_SEQUENCE_NUMBERS) {
		session_profile->use_sequence_numbers = msg->use_sequence_numbers;
	}
	if (msg->flags & L2TP_API_SESSION_PROFILE_FLAG_NO_PPP) {
		session_profile->no_ppp = msg->no_ppp;
	}
	if (msg->flags & L2TP_API_SESSION_PROFILE_FLAG_REORDER_TIMEOUT) {
		session_profile->reorder_timeout = msg->reorder_timeout;
	}
	if (msg->flags & L2TP_API_SESSION_PROFILE_FLAG_PPP_PROFILE_NAME) {
		if (OPTSTRING_PTR(msg->ppp_profile_name) == NULL) {
			goto inval;
		}
		if (session_profile->ppp_profile_name != NULL) {
			free(session_profile->ppp_profile_name);
		}
		session_profile->ppp_profile_name = strdup(OPTSTRING(msg->ppp_profile_name));
		if (session_profile->ppp_profile_name == NULL) {
			goto nomem;
		}
	}
	if (msg->flags & L2TP_API_SESSION_PROFILE_FLAG_PRIV_GROUP_ID) {
		if (OPTSTRING_PTR(msg->priv_group_id) == NULL) {
			goto inval;
		}
		if (session_profile->priv_group_id != NULL) {
			free(session_profile->priv_group_id);
		}
		session_profile->priv_group_id = strdup(OPTSTRING(msg->priv_group_id));
		if (session_profile->priv_group_id == NULL) {
			goto nomem;
		}
	}
	if (msg->flags & L2TP_API_SESSION_PROFILE_FLAG_MINIMUM_BPS) {
		session_profile->minimum_bps = msg->minimum_bps;
	}
	if (msg->flags & L2TP_API_SESSION_PROFILE_FLAG_MAXIMUM_BPS) {
		session_profile->maximum_bps = msg->maximum_bps;
	}
	if (msg->flags & L2TP_API_SESSION_PROFILE_FLAG_CONNECT_SPEED) {
		session_profile->connect_speed = msg->tx_connect_speed;
		if (msg->rx_connect_speed > 0) {
			session_profile->rx_connect_speed = msg->rx_connect_speed;
		} else {
			session_profile->rx_connect_speed = session_profile->connect_speed;
		}
	}
	if (msg->flags & L2TP_API_SESSION_PROFILE_FLAG_USE_PPP_PROXY) {
		session_profile->use_ppp_proxy = msg->use_ppp_proxy;
	}
	if (msg->flags & L2TP_API_SESSION_FLAG_FRAMING_TYPE) {
		session_profile->framing_type_async = msg->framing_type_async ? -1 : 0;
		session_profile->framing_type_sync = msg->framing_type_sync ? -1 : 0;
	}
	if (msg->flags & L2TP_API_SESSION_FLAG_BEARER_TYPE) {
		session_profile->bearer_type_digital = msg->bearer_type_digital ? -1 : 0;
		session_profile->bearer_type_analog = msg->bearer_type_analog ? -1 : 0;
	}

	return 0;

nomem:
	l2tp_stats.no_session_resources++;
	return -ENOMEM;
	
inval:
	/* buggy client */
	return -EINVAL;
}

static void l2tp_session_profile_free(struct l2tp_session_profile *session_profile)
{
	if (session_profile->profile_name != NULL) {
		free(session_profile->profile_name);
	}
	if (session_profile->ppp_profile_name != NULL) {
		free(session_profile->ppp_profile_name);
	}
	if (session_profile->priv_group_id != NULL) {
		free(session_profile->priv_group_id);
	}
	USL_POISON_MEMORY(session_profile, 0xeb, sizeof(*session_profile));
	free(session_profile);
}

static struct l2tp_session_profile *l2tp_session_profile_alloc(const char *profile_name)
{
	struct l2tp_session_profile *session_profile;

	session_profile = calloc(1, sizeof(struct l2tp_session_profile));
	if (session_profile == NULL) {
		goto nomem1;
	}
	session_profile->profile_name = strdup(profile_name);
	if (session_profile->profile_name == NULL) {
		goto nomem2;
	}

	/* Fill with default values */
	session_profile->sequencing_required = l2tp_session_defaults->sequencing_required;
	session_profile->use_sequence_numbers = l2tp_session_defaults->use_sequence_numbers;
	session_profile->no_ppp = l2tp_session_defaults->no_ppp;
	session_profile->reorder_timeout = l2tp_session_defaults->reorder_timeout;
	session_profile->trace_flags = l2tp_session_defaults->trace_flags;
	session_profile->framing_type_sync = l2tp_session_defaults->framing_type_sync;
	session_profile->framing_type_async = l2tp_session_defaults->framing_type_async;
	session_profile->bearer_type_analog = l2tp_session_defaults->bearer_type_analog;
	session_profile->bearer_type_digital = l2tp_session_defaults->bearer_type_digital;
	session_profile->minimum_bps = l2tp_session_defaults->minimum_bps;
	session_profile->maximum_bps = l2tp_session_defaults->maximum_bps;
	session_profile->connect_speed = l2tp_session_defaults->connect_speed;
	session_profile->rx_connect_speed = l2tp_session_defaults->rx_connect_speed;
	session_profile->use_ppp_proxy = l2tp_session_defaults->use_ppp_proxy;

	return session_profile;

 nomem2:
	l2tp_session_profile_free(session_profile);
 nomem1:

	return NULL;
}

bool_t l2tp_session_profile_create_1_svc(struct l2tp_api_session_profile_msg_data msg, int *result, struct svc_req *req)
{
	struct l2tp_session_profile *profile;
	char *name;

	if ((msg.profile_name == NULL) || strlen(msg.profile_name) == 0) {
		*result = -L2TP_ERR_PROFILE_NAME_MISSING;
		goto out;
	}
	name = msg.profile_name;

	if (msg.flags & L2TP_API_SESSION_PROFILE_FLAG_PPP_PROFILE_NAME) {
		if (OPTSTRING_PTR(msg.ppp_profile_name) == NULL) {
			*result = -EINVAL;
			goto out;
		}
	}

	profile = l2tp_session_profile_find(name);
	if (profile != NULL) {
		*result = -L2TP_ERR_PROFILE_ALREADY_EXISTS;
		goto out;
	}
	
	profile = l2tp_session_profile_alloc(name);
	if (profile == NULL) {
		goto nomem1;
	}

	/* Override defaults by user-supplied params */
	*result = l2tp_session_profile_set(profile, &msg);
	if (*result < 0) {
		goto error;
	}

	/* Remember all non-default parameters */
	profile->flags |= msg.flags;
	
	USL_LIST_HEAD_INIT(&profile->list);
	usl_list_add(&profile->list, &l2tp_session_profile_list);

	L2TP_DEBUG(L2TP_FUNC, "FUNC: session profile %s created", profile->profile_name);

	/* Give plugins visibility of session profile created */
	if (l2tp_profile_created_hook != NULL) {
		(*l2tp_profile_created_hook)(L2TP_PROFILE_TYPE_SESSION, profile->profile_name);
	}

out:
	L2TP_DEBUG(L2TP_API, "%s: result=%d", __func__, *result);
	return TRUE;

error:
	l2tp_session_profile_free(profile);
	goto out;

nomem1:
	*result = -ENOMEM;
	goto out;
}

bool_t l2tp_session_profile_delete_1_svc(char *name, int *result, struct svc_req *req)
{
	struct l2tp_session_profile *profile;

	if ((name == NULL) || strlen(name) == 0) {
		*result = -L2TP_ERR_PROFILE_NAME_MISSING;
		goto out;
	}

	/* Prevent deletion of default profile */
	if (strcmp(name, L2TP_API_SESSION_PROFILE_DEFAULT_PROFILE_NAME) == 0) {
		*result = -L2TP_ERR_PROFILE_NAME_ILLEGAL;
		goto out;
	}

	profile = l2tp_session_profile_find(name);
	if (profile == NULL) {
		*result = -L2TP_ERR_SESSION_PROFILE_NOT_FOUND;
		goto out;
	}

	L2TP_DEBUG(L2TP_FUNC, "FUNC: session profile %s deleted", profile->profile_name);

	/* Give plugins visibility of session profile deleted */
	if (l2tp_profile_deleted_hook != NULL) {
		(*l2tp_profile_deleted_hook)(L2TP_PROFILE_TYPE_SESSION, profile->profile_name);
	}

	usl_list_del(&profile->list);
	l2tp_session_profile_free(profile);
	*result = 0;
	
out:	
	L2TP_DEBUG(L2TP_API, "%s: result=%d", __func__, *result);
	return TRUE;
}

bool_t l2tp_session_profile_modify_1_svc(l2tp_api_session_profile_msg_data msg, int *result, struct svc_req *req)
{
	struct l2tp_session_profile *profile;
	char *name;

	if ((msg.profile_name == NULL) || strlen(msg.profile_name) == 0) {
		*result = -L2TP_ERR_PROFILE_NAME_MISSING;
		goto out;
	}
	name = msg.profile_name;

	if (msg.flags & L2TP_API_SESSION_PROFILE_FLAG_PPP_PROFILE_NAME) {
		if (OPTSTRING_PTR(msg.ppp_profile_name) == NULL) {
			*result = -EINVAL;
			goto out;
		}
	}

	profile = l2tp_session_profile_find(name);
	if (profile == NULL) {
		*result = -L2TP_ERR_SESSION_PROFILE_NOT_FOUND;
		goto out;
	}
	
	*result = l2tp_session_profile_set(profile, &msg);
	if (*result < 0) {
		goto out;
	}

	/* Remember all non-default parameters */
	profile->flags |= msg.flags;

	L2TP_DEBUG(L2TP_FUNC, "FUNC: session profile %s modified", profile->profile_name);

	/* Give plugins visibility of session profile modified */
	if (l2tp_profile_modified_hook != NULL) {
		(*l2tp_profile_modified_hook)(L2TP_PROFILE_TYPE_SESSION, profile->profile_name);
	}

out:
	L2TP_DEBUG(L2TP_API, "%s: result=%d", __func__, *result);
	return TRUE;
}

bool_t l2tp_session_profile_get_1_svc(char *name, l2tp_api_session_profile_msg_data *result, struct svc_req *req)
{
	struct l2tp_session_profile *profile;

	memset(result, 0, sizeof(*result));

	if ((name == NULL) || strlen(name) == 0) {
		result->profile_name = strdup("");
		result->result_code = -L2TP_ERR_PROFILE_NAME_MISSING;
		goto out;
	}

	profile = l2tp_session_profile_find(name);
	if (profile == NULL) {
		result->profile_name = strdup(name);
		result->result_code = -L2TP_ERR_SESSION_PROFILE_NOT_FOUND;
		goto out;
	}
	
	memset(result, 0, sizeof(*result));
	result->flags = profile->flags;
	result->profile_name = strdup(profile->profile_name);
	if (result->profile_name == NULL) {
		result->result_code = -ENOMEM;
	}
	result->sequencing_required = profile->sequencing_required;
	result->use_sequence_numbers = profile->use_sequence_numbers;
	result->no_ppp = profile->no_ppp;
	result->reorder_timeout = profile->reorder_timeout;
	result->trace_flags = profile->trace_flags;
	if (profile->ppp_profile_name != NULL) {
		OPTSTRING(result->ppp_profile_name) = strdup(profile->ppp_profile_name);
	} else {
		OPTSTRING(result->ppp_profile_name) = strdup(L2TP_API_PPP_PROFILE_DEFAULT_PROFILE_NAME);
	}
	if (OPTSTRING(result->ppp_profile_name) == NULL) {
		result->result_code = -ENOMEM;
		goto out;
	}
	result->ppp_profile_name.valid = 1;
	if (profile->priv_group_id != NULL) {
		OPTSTRING(result->priv_group_id) = strdup(profile->priv_group_id);
		if (OPTSTRING(result->priv_group_id) == NULL) {
			result->result_code = -ENOMEM;
			goto out;
		}
		result->priv_group_id.valid = 1;
	}
	result->minimum_bps = profile->minimum_bps;
	result->maximum_bps = profile->maximum_bps;
	result->tx_connect_speed = profile->connect_speed;
	result->rx_connect_speed = profile->rx_connect_speed;
	result->framing_type_sync = profile->framing_type_sync ? 1 : 0;
	result->framing_type_async = profile->framing_type_async ? 1 : 0;
	result->bearer_type_analog = profile->bearer_type_analog ? 1 : 0;
	result->bearer_type_digital = profile->bearer_type_digital ? 1 : 0;
	result->use_ppp_proxy = profile->use_ppp_proxy;

out:
	L2TP_DEBUG(L2TP_API, "%s: result=%d", __func__, result->result_code);
	return TRUE;
}

bool_t l2tp_session_profile_list_1_svc(l2tp_api_session_profile_list_msg_data *result, struct svc_req *req)
{
	struct usl_list_head *tmp;
	struct usl_list_head *walk;
	struct l2tp_session_profile *profile;
	struct l2tp_api_session_profile_list_entry *entry;
	struct l2tp_api_session_profile_list_entry *tmpe;
	int num_profiles = 0;

	memset(result, 0, sizeof(*result));

	result->profiles = calloc(1, sizeof(*result->profiles));
	if (result->profiles == NULL) {
		result->result = -ENOMEM;
		goto error;
	}
	entry = result->profiles;
	usl_list_for_each(walk, tmp, &l2tp_session_profile_list) {
		profile = usl_list_entry(walk, struct l2tp_session_profile, list);

		entry->profile_name = strdup(profile->profile_name);
		if (entry->profile_name == NULL) {
			result->result = -ENOMEM;
			goto error;
		}

		tmpe = calloc(1, sizeof(*result->profiles));
		if (tmpe == NULL) {
			result->result = -ENOMEM;
			goto error;
		}
		entry->next = tmpe;
		entry = tmpe;
		num_profiles++;
	}

	entry->profile_name = strdup("");
	if (entry->profile_name == NULL) {
		goto error;
	}

	result->num_profiles = num_profiles;
out:
	L2TP_DEBUG(L2TP_API, "%s: result=%d", __func__, result->result);
	return TRUE;

error:
	for (entry = result->profiles; entry != NULL; ) {
		tmpe = entry->next;
		free(entry->profile_name);
		free(entry);
		entry = tmpe;
	}

	goto out;
}

bool_t l2tp_session_profile_unset_1_svc(l2tp_api_session_profile_unset_msg_data msg, int *result, struct svc_req *req)
{
	struct l2tp_session_profile *profile;

	if ((msg.profile_name == NULL) || strlen(msg.profile_name) == 0) {
		*result = -L2TP_ERR_PROFILE_NAME_MISSING;
		goto out;
	}

	profile = l2tp_session_profile_find(msg.profile_name);
	if (profile == NULL) {
		*result = -L2TP_ERR_SESSION_PROFILE_NOT_FOUND;
		goto out;
	}

	/* Set parameters of requested fields back to default values */
	if (msg.flags & L2TP_API_SESSION_PROFILE_FLAG_TRACE_FLAGS) {
		if (l2tp_opt_debug) {
			profile->trace_flags = -1;
		} else {
			profile->trace_flags = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_TRACE_FLAGS;
		}
	}
	if (msg.flags & L2TP_API_SESSION_PROFILE_FLAG_SEQUENCING_REQUIRED) {
		profile->sequencing_required = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_SEQUENCING_REQUIRED;
	}
	if (msg.flags & L2TP_API_SESSION_PROFILE_FLAG_USE_SEQUENCE_NUMBERS) {
		profile->use_sequence_numbers = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_USE_SEQUENCE_NUMBERS;
	}
	if (msg.flags & L2TP_API_SESSION_PROFILE_FLAG_NO_PPP) {
		profile->no_ppp = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_NO_PPP;
	}
	if (msg.flags & L2TP_API_SESSION_PROFILE_FLAG_REORDER_TIMEOUT) {
		profile->reorder_timeout = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_REORDER_TIMEOUT;
	}
	if (msg.flags & L2TP_API_SESSION_PROFILE_FLAG_PPP_PROFILE_NAME) {
		if (profile->ppp_profile_name != NULL) {
			free(profile->ppp_profile_name);
		}
		profile->ppp_profile_name = NULL;
	}
	if (msg.flags & L2TP_API_SESSION_PROFILE_FLAG_PRIV_GROUP_ID) {
		if (profile->priv_group_id != NULL) {
			free(profile->priv_group_id);
		}
		profile->priv_group_id = NULL;
	}
	if (msg.flags & L2TP_API_SESSION_PROFILE_FLAG_MINIMUM_BPS) {
		profile->minimum_bps = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_MINIMUM_BPS;
	}
	if (msg.flags & L2TP_API_SESSION_PROFILE_FLAG_MAXIMUM_BPS) {
		profile->maximum_bps = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_MAXIMUM_BPS;
	}
	if (msg.flags & L2TP_API_SESSION_PROFILE_FLAG_CONNECT_SPEED) {
		profile->connect_speed = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_TX_CONNECT_SPEED;
		profile->rx_connect_speed = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_RX_CONNECT_SPEED;
	}
	if (msg.flags & L2TP_API_SESSION_PROFILE_FLAG_USE_PPP_PROXY) {
		profile->use_ppp_proxy = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_USE_PPP_PROXY;
	}
	if (msg.flags & L2TP_API_SESSION_FLAG_FRAMING_TYPE) {
		profile->framing_type_async = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_FRAMING_TYPE_ASYNC;
		profile->framing_type_sync = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_FRAMING_TYPE_SYNC;
	}
	if (msg.flags & L2TP_API_SESSION_FLAG_BEARER_TYPE) {
		profile->bearer_type_digital = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_BEARER_TYPE_DIGITAL;
		profile->bearer_type_analog = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_BEARER_TYPE_ANALOG;
	}

	/* Unset all requested flags */
	profile->flags &= ~(msg.flags);

	L2TP_DEBUG(L2TP_FUNC, "FUNC: session profile %s unset", profile->profile_name);

	/* Give plugins visibility of session profile modified */
	if (l2tp_profile_modified_hook != NULL) {
		(*l2tp_profile_modified_hook)(L2TP_PROFILE_TYPE_SESSION, profile->profile_name);
	}

out:
	L2TP_DEBUG(L2TP_API, "%s: result=%d", __func__, *result);
	return TRUE;
}


/*****************************************************************************
 * Users
 *****************************************************************************/

bool_t l2tp_user_list_1_svc(struct l2tp_api_user_list_msg_data *result, struct svc_req *req)
{
	struct usl_list_head *tmp;
	struct usl_list_head *walk;
	struct l2tp_session *session;
	struct l2tp_api_user_list_entry *entry;
	struct l2tp_api_user_list_entry *tmpe;
	int num_users = 0;

	memset(result, 0, sizeof(*result));

	result->users = calloc(1, sizeof(*result->users));
	if (result->users == NULL) {
		result->result = -ENOMEM;
		goto error;
	}
	entry = result->users;
	usl_list_for_each(walk, tmp, &l2tp_session_list) {
		session = usl_list_entry(walk, struct l2tp_session, list);

		if ((session->config.user_name == NULL) ||
		    (session->status.create_time == NULL) ||
		    (session->my_tunnel == NULL)) {
			continue;
		}
		entry->user_name = strdup(session->config.user_name);
		entry->create_time = strdup(session->status.create_time);
		if ((entry->user_name == NULL) || (entry->create_time == NULL)) {
			result->result = -ENOMEM;
			goto error;
		}
		entry->created_by_admin = session->status.created_by_admin ? 1 : 0;
		entry->tunnel_id = l2tp_tunnel_id(session->my_tunnel);
		entry->session_id = session->status.session_id;
		
		tmpe = calloc(1, sizeof(*result->users));
		if (tmpe == NULL) {
			result->result = -ENOMEM;
			goto error;
		}
		num_users++;
		entry->next = tmpe;
		entry = tmpe;
	}

	/* RPC requires non-NULL strings. Sigh. */
	if (entry->user_name == NULL) {
		entry->user_name = strdup("");
	}
	if (entry->create_time == NULL) {
		entry->create_time = strdup("");
	}
	if ((entry->user_name == NULL) || (entry->create_time == NULL)) {
		result->result = -ENOMEM;
		goto error;
	}

	result->num_users = num_users;

	return TRUE;

error:
	for (entry = result->users; entry != NULL; ) {
		tmpe = entry->next;
		if (tmpe->user_name != NULL) {
			free(tmpe->user_name);
		}
		if (tmpe->create_time != NULL) {
			free(tmpe->create_time);
		}
		free(entry);
		entry = tmpe;
	}

	return TRUE;
}

/*****************************************************************************
 * Init and cleanup
 *****************************************************************************/

void l2tp_session_reinit(void)
{
	struct usl_list_head *tmp;
	struct usl_list_head *walk;
	struct l2tp_session_profile *profile;

	l2tp_session_establish_timeout = 120;
	l2tp_session_persist_pend_timeout = 60;
	l2tp_session_max_count = 0;

	/* First remove all profiles */
	usl_list_for_each(walk, tmp, &l2tp_session_profile_list) {
		profile = usl_list_entry(walk, struct l2tp_session_profile, list);
		usl_list_del(&profile->list);
		l2tp_session_profile_free(profile);
	}

	/* Now create the default profile */
	l2tp_session_defaults = calloc(1, sizeof(*l2tp_session_defaults));
	if (l2tp_session_defaults == NULL) {
		fprintf(stderr, "Out of memory\n");
		exit(1);
	}

	l2tp_session_defaults->profile_name = strdup(L2TP_API_SESSION_PROFILE_DEFAULT_PROFILE_NAME);
	if (l2tp_opt_debug) {
		l2tp_session_defaults->trace_flags = -1;
	} else {
		l2tp_session_defaults->trace_flags = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_TRACE_FLAGS;
	}
	l2tp_session_defaults->ppp_profile_name = strdup(L2TP_API_SESSION_PROFILE_DEFAULT_PROFILE_NAME);
	l2tp_session_defaults->sequencing_required = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_SEQUENCING_REQUIRED;
	l2tp_session_defaults->use_sequence_numbers = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_USE_SEQUENCE_NUMBERS;
	l2tp_session_defaults->no_ppp = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_NO_PPP;
	l2tp_session_defaults->reorder_timeout = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_REORDER_TIMEOUT;
	l2tp_session_defaults->framing_type_sync = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_FRAMING_TYPE_SYNC;
	l2tp_session_defaults->framing_type_async = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_FRAMING_TYPE_ASYNC;
	l2tp_session_defaults->bearer_type_analog = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_BEARER_TYPE_ANALOG;
	l2tp_session_defaults->bearer_type_digital = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_BEARER_TYPE_DIGITAL;
	l2tp_session_defaults->minimum_bps = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_MINIMUM_BPS;
	l2tp_session_defaults->maximum_bps = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_MAXIMUM_BPS;
	l2tp_session_defaults->connect_speed = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_TX_CONNECT_SPEED;
	l2tp_session_defaults->rx_connect_speed = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_RX_CONNECT_SPEED;
	l2tp_session_defaults->use_ppp_proxy = L2TP_API_SESSION_PROFILE_DEFAULT_SESSION_USE_PPP_PROXY;
	
	USL_LIST_HEAD_INIT(&l2tp_session_defaults->list);
	usl_list_add(&l2tp_session_defaults->list, &l2tp_session_profile_list);
}

void l2tp_session_init(void)
{
	int result;

	l2tp_session_count = 0;

	l2tp_session_reinit();

	l2tp_session_emergency_result_code = malloc(sizeof(struct l2tp_avp_result_code) + 
						    L2TP_SES_EMERG_RESULT_CODE_SIZE);
	if (l2tp_session_emergency_result_code == NULL) {
		fprintf(stderr, "Out of memory\n");
		exit(1);
	}
	memset(l2tp_session_emergency_result_code, 0, 
	       sizeof(struct l2tp_avp_result_code) + L2TP_SES_EMERG_RESULT_CODE_SIZE);

#ifdef L2TP_FEATURE_LAC_SUPPORT
	(void) l2tp_tunnel_register_message_handler(L2TP_AVP_MSG_OCRQ, l2tp_session_handle_msg_ocrq);
#endif
#ifdef L2TP_FEATURE_LNS_SUPPORT
	(void) l2tp_tunnel_register_message_handler(L2TP_AVP_MSG_OCRP, l2tp_session_handle_msg_ocrp);
#endif
#ifdef L2TP_FEATURE_LNS_SUPPORT
	(void) l2tp_tunnel_register_message_handler(L2TP_AVP_MSG_OCCN, l2tp_session_handle_msg_occn);
	(void) l2tp_tunnel_register_message_handler(L2TP_AVP_MSG_ICRQ, l2tp_session_handle_msg_icrq);
#endif
#ifdef L2TP_FEATURE_LAC_SUPPORT
	(void) l2tp_tunnel_register_message_handler(L2TP_AVP_MSG_ICRP, l2tp_session_handle_msg_icrp);
#endif
#ifdef L2TP_FEATURE_LNS_SUPPORT
	(void) l2tp_tunnel_register_message_handler(L2TP_AVP_MSG_ICCN, l2tp_session_handle_msg_iccn);
#endif
	(void) l2tp_tunnel_register_message_handler(L2TP_AVP_MSG_CDN, l2tp_session_handle_msg_cdn);
#ifdef L2TP_FEATURE_LNS_SUPPORT
	(void) l2tp_tunnel_register_message_handler(L2TP_AVP_MSG_WEN, l2tp_session_handle_msg_wen);
#endif
#ifdef L2TP_FEATURE_LAC_SUPPORT
	(void) l2tp_tunnel_register_message_handler(L2TP_AVP_MSG_SLI, l2tp_session_handle_msg_sli);
#endif

	result = pipe(&l2tp_session_event_pipe[0]);
	if (result < 0) {
		fprintf(stderr, "Failed to create internal session event pipe: %s\n", strerror(-errno));
		exit(1);
	}
	(void) fcntl(l2tp_session_event_pipe[0], F_SETFD, FD_CLOEXEC);
	(void) fcntl(l2tp_session_event_pipe[1], F_SETFD, FD_CLOEXEC);
	result = usl_fd_add_fd(l2tp_session_event_pipe[0], l2tp_session_do_queued_event, NULL);
	if (result < 0) {
		exit(1);
	}

	L2TP_DEBUG(L2TP_FUNC, "session: size=%zu", sizeof(struct l2tp_session));
}

void l2tp_session_cleanup(void)
{
	struct usl_list_head *tmp;
	struct usl_list_head *walk;
	struct l2tp_session_profile *profile;
	struct l2tp_session *session;

	l2tp_tunnel_unregister_message_handler(L2TP_AVP_MSG_OCRQ);
	l2tp_tunnel_unregister_message_handler(L2TP_AVP_MSG_OCRP);
	l2tp_tunnel_unregister_message_handler(L2TP_AVP_MSG_OCCN);
	l2tp_tunnel_unregister_message_handler(L2TP_AVP_MSG_ICRQ);
	l2tp_tunnel_unregister_message_handler(L2TP_AVP_MSG_ICRP);
	l2tp_tunnel_unregister_message_handler(L2TP_AVP_MSG_ICCN);
	l2tp_tunnel_unregister_message_handler(L2TP_AVP_MSG_CDN);
	l2tp_tunnel_unregister_message_handler(L2TP_AVP_MSG_WEN);
	l2tp_tunnel_unregister_message_handler(L2TP_AVP_MSG_SLI);

	usl_list_for_each(walk, tmp, &l2tp_session_list) {
		session = usl_list_entry(walk, struct l2tp_session, list);
		l2tp_session_unlink(session, 1);
	}

	usl_list_for_each(walk, tmp, &l2tp_session_detached_list) {
		session = usl_list_entry(walk, struct l2tp_session, detached_list);
		l2tp_session_unlink(session, 1);
	}

	if (l2tp_session_event_pipe[0] >= 0) {
		usl_fd_remove_fd(l2tp_session_event_pipe[0]);
		close(l2tp_session_event_pipe[0]);
		l2tp_session_event_pipe[0] = -1;
	}
	if (l2tp_session_event_pipe[1] >= 0) {
		close(l2tp_session_event_pipe[1]);
		l2tp_session_event_pipe[1] = -1;
	}

	if (l2tp_session_emergency_result_code != NULL) {
		free(l2tp_session_emergency_result_code);
		l2tp_session_emergency_result_code = NULL;
	}

	usl_list_for_each(walk, tmp, &l2tp_session_profile_list) {
		profile = usl_list_entry(walk, struct l2tp_session_profile, list);
		usl_list_del(&profile->list);
		l2tp_session_profile_free(profile);
	}
}

