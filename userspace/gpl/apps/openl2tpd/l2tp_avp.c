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

#define _GNU_SOURCE	/* for strndup() */
#include <string.h>

#include "usl.h"
#include "md5.h"
#include "l2tp_private.h"

#define L2TP_AVP_MAX_HIDE_PADDING	16

#define NO	0
#define YES	1
#define MAY	2

/* Builder may override the vendor name reported to peer */
#ifndef L2TP_VENDOR_INFO
#define L2TP_VENDOR_INFO L2TP_APP_VENDOR_INFO
#endif

/* This should be defined, but just in case... */
#ifndef _SC_HOST_NAME_MAX
#define _SC_HOST_NAME_MAX	256
#endif

struct l2tp_tunnel;
struct l2tp_msg_info;

typedef int (*encode_fn)(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
			 struct l2tp_msg_info const *msg_info,
			 struct l2tp_tunnel const *tunnel, int hide);
typedef int (*decode_fn)(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);

struct l2tp_avp_info {
	int		type;		/* AVP type */
	uint16_t	vendor;		/* the vendor id associated w/ AVP */
	uint8_t		hide;		/* can the AVP be hidden? */
 	uint8_t		mandatory;	/* is AVP mandatory? */	
	encode_fn	encode;		/* encode an AVP at hdr address  */
	decode_fn	decode;		/* decode an AVP at hdr address to result */
	const char	*name;		/* for debugging */
};

/* Lazy typing macros... */
#define TYPE(name)			L2TP_AVP_TYPE_##name
#define FUNCS(stem)			l2tp_avp_encode_##stem, l2tp_avp_decode_##stem

#ifdef DEBUG
#define L2TP_AVP_HEXDUMP(flags, data, data_len)				\
	do {								\
		if (flags & L2TP_AVPDATA) {				\
			l2tp_mem_dump(LOG_DEBUG, data, data_len, 1);	\
		}							\
	} while(0)
#else
#define L2TP_AVP_HEXDUMP(flags, data, data_len)	do { } while(0)
#endif

/* These AVPs are constant so are built at init time */
struct l2tp_avp_firmware_revision 	l2tp_avp_my_firmware_revision;
struct l2tp_avp_host_name 		*l2tp_avp_my_host_name;
struct l2tp_avp_vendor_name 		*l2tp_avp_my_vendor_name;
struct l2tp_avp_protocol_version 	l2tp_avp_my_protocol_version;
int					l2tp_avp_my_host_name_len;
int					l2tp_avp_my_vendor_name_len;

static int l2tp_avp_encode_message_type(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					struct l2tp_msg_info const *msg_info,
					struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_message_type(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_result_code(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
				       struct l2tp_msg_info const *msg_info,
				       struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_result_code(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_protocol_version(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					    struct l2tp_msg_info const *msg_info,
					    struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_protocol_version(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_framing_cap(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
				       struct l2tp_msg_info const *msg_info,
				       struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_framing_cap(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_bearer_cap(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
				      struct l2tp_msg_info const *msg_info,
				      struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_bearer_cap(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_tiebreaker(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
				       struct l2tp_msg_info const *msg_info,
				       struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_tiebreaker(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_firmware_revision(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					     struct l2tp_msg_info const *msg_info,
					     struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_firmware_revision(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_host_name(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
				     struct l2tp_msg_info const *msg_info,
				     struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_host_name(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_vendor_name(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
				       struct l2tp_msg_info const *msg_info,
				       struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_vendor_name(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_tunnel_id(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
				     struct l2tp_msg_info const *msg_info,
				     struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_tunnel_id(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_rx_window_size(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					  struct l2tp_msg_info const *msg_info,
					  struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_rx_window_size(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_challenge(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
				     struct l2tp_msg_info const *msg_info,
				     struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_challenge(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_q931_cause_code(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					   struct l2tp_msg_info const *msg_info,
					   struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_q931_cause_code(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_challenge_response(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					      struct l2tp_msg_info const *msg_info,
					      struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_challenge_response(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_session_id(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
				      struct l2tp_msg_info const *msg_info,
				      struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_session_id(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_call_serial_number(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					      struct l2tp_msg_info const *msg_info,
					      struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_call_serial_number(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_minimum_bps(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
				       struct l2tp_msg_info const *msg_info,
				       struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_minimum_bps(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_maximum_bps(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
				       struct l2tp_msg_info const *msg_info,
				       struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_maximum_bps(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_bearer_type(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
				       struct l2tp_msg_info const *msg_info,
				       struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_bearer_type(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_framing_type(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					struct l2tp_msg_info const *msg_info,
					struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_framing_type(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_packet_proc_delay(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					     struct l2tp_msg_info const *msg_info,
					     struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_packet_proc_delay(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_called_number(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					 struct l2tp_msg_info const *msg_info,
					 struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_called_number(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_calling_number(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					  struct l2tp_msg_info const *msg_info,
					  struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_calling_number(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_sub_address(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
				       struct l2tp_msg_info const *msg_info,
				       struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_sub_address(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_tx_connect_speed(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					    struct l2tp_msg_info const *msg_info,
					    struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_tx_connect_speed(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_physical_channel_id(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					       struct l2tp_msg_info const *msg_info,
					       struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_physical_channel_id(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_initial_lcp_confreq(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					       struct l2tp_msg_info const *msg_info,
					       struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_initial_lcp_confreq(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_last_sent_lcp_confreq(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
						 struct l2tp_msg_info const *msg_info,
						 struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_last_sent_lcp_confreq(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_last_rcvd_lcp_confreq(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
						 struct l2tp_msg_info const *msg_info,
						 struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_last_rcvd_lcp_confreq(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_proxy_auth_type(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					   struct l2tp_msg_info const *msg_info,
					   struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_proxy_auth_type(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_proxy_auth_name(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					   struct l2tp_msg_info const *msg_info,
					   struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_proxy_auth_name(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_proxy_auth_chall(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					    struct l2tp_msg_info const *msg_info,
					    struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_proxy_auth_chall(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_proxy_auth_id(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					 struct l2tp_msg_info const *msg_info,
					 struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_proxy_auth_id(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_proxy_auth_rsp(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					  struct l2tp_msg_info const *msg_info,
					  struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_proxy_auth_rsp(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_call_errors(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
				       struct l2tp_msg_info const *msg_info,
				       struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_call_errors(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_accm(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
				struct l2tp_msg_info const *msg_info,
				struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_accm(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_random_vector(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					 struct l2tp_msg_info const *msg_info,
					 struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_random_vector(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_priv_group_id(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					 struct l2tp_msg_info const *msg_info,
					 struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_priv_group_id(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_rx_connect_speed(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					    struct l2tp_msg_info const *msg_info,
					    struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_rx_connect_speed(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);
static int l2tp_avp_encode_sequencing_required(void *buffer, int buffer_len, struct l2tp_avp_desc const *data, 
					       struct l2tp_msg_info const *msg_info,
					       struct l2tp_tunnel const *tunnel, int hide);
static int l2tp_avp_decode_sequencing_required(struct l2tp_avp_hdr *hdr, struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel);

static int l2tp_avp_count_avps(struct l2tp_avp_desc *avp_data);
static void l2tp_avp_log_avp(struct l2tp_avp_hdr *avp, struct l2tp_tunnel const *tunnel);

/* avp_info array contains per generic AVP information including description. 
 * It also shows whether a given AVP is manditory, or hidden. 
 * Note that the array is sorted by AVP number.	 message avp 0 is index 0.
 * This allows direct indexing (no searching required) for all AVPs with
 * vendor_id 0.	 Don't change the order!
 */
static const struct l2tp_avp_info l2tp_avp_info[] = 
{
  /*   type			vend	hide	mand,	encode/decode funcs		name */
  { TYPE(MESSAGE),		0,	NO,	YES,	FUNCS(message_type),		"Message Type" },
  { TYPE(RESULT_CODE),		0,	NO,	YES,	FUNCS(result_code),		"Result Code" },
  { TYPE(PROTOCOL_VERSION),	0,	NO,	YES,	FUNCS(protocol_version),	"Protocol Version" },
  { TYPE(FRAMING_CAP),		0,	MAY,	YES,	FUNCS(framing_cap),		"Framing Capabilities" },
  { TYPE(BEARER_CAP),		0,	MAY,	YES,	FUNCS(bearer_cap),		"Bearer Capabilities" },
  { TYPE(TIEBREAKER),		0,	NO,	NO,	FUNCS(tiebreaker),		"Tie Breaker" },
  { TYPE(FIRMWARE_REVISION),	0,	MAY,	NO,	FUNCS(firmware_revision),	"Firmware Revision" },
  { TYPE(HOST_NAME),		0,	NO,	YES,	FUNCS(host_name),		"Host Name" },
  { TYPE(VENDOR_NAME),		0,	MAY,	NO,	FUNCS(vendor_name),		"Vendor Name" },
  { TYPE(TUNNEL_ID),		0,	MAY,	YES,	FUNCS(tunnel_id),		"Assigned Tunnel ID" },
  { TYPE(RX_WINDOW_SIZE),	0,	NO,	YES,	FUNCS(rx_window_size),		"Receive Window Size" },
  { TYPE(CHALLENGE),		0,	MAY,	YES,	FUNCS(challenge),		"Challenge" },
  { TYPE(Q931_CAUSE_CODE),	0,	NO,	YES,	FUNCS(q931_cause_code),		"Q.931 Cause Code" },
  { TYPE(CHALLENGE_RESPONSE),	0,	MAY,	YES,	FUNCS(challenge_response),	"Challenge Response" },
  { TYPE(SESSION_ID),		0,	MAY,	YES,	FUNCS(session_id),		"Assigned Session ID" },
  { TYPE(CALL_SERIAL_NUMBER),	0,	MAY,	YES,	FUNCS(call_serial_number),	"Call Serial Number" },
  { TYPE(MINIMUM_BPS),		0,	MAY,	YES,	FUNCS(minimum_bps),		"Minimum BPS" },
  { TYPE(MAXIMUM_BPS),		0,	MAY,	YES,	FUNCS(maximum_bps),		"Maximum BPS" },
  { TYPE(BEARER_TYPE),		0,	MAY,	YES,	FUNCS(bearer_type),		"Bearer Type" },
  { TYPE(FRAMING_TYPE),		0,	MAY,	YES,	FUNCS(framing_type),		"Framing Type" },
  { TYPE(PACKET_PROC_DELAY),	0,	NO,	NO,	FUNCS(packet_proc_delay),	"Packet Process Delay" },
  { TYPE(CALLED_NUMBER),	0,	MAY,	YES,	FUNCS(called_number),		"Dialed Number" },
  { TYPE(CALLING_NUMBER),	0,	MAY,	YES,	FUNCS(calling_number),		"Dialing Number" },
  { TYPE(SUB_ADDRESS),		0,	MAY,	YES,	FUNCS(sub_address),		"Sub-Address" },
  { TYPE(CONNECT_SPEED),	0,	MAY,	YES,	FUNCS(tx_connect_speed),	"Transmit Connect Speed" },
  { TYPE(PHYSICAL_CHANNEL_ID),	0,	MAY,	NO,	FUNCS(physical_channel_id),	"Physical channel ID" },
  { TYPE(INITIAL_RCVD_LCP_CONFREQ), 0,	MAY,	NO,	FUNCS(initial_lcp_confreq),	"Initial Received LCP Confreq" },
  { TYPE(LAST_SENT_LCP_CONFREQ),0,	MAY,	NO,	FUNCS(last_sent_lcp_confreq),	"Last Sent LCP Confreq" },
  { TYPE(LAST_RCVD_LCP_CONFREQ),0,	MAY,	NO,	FUNCS(last_rcvd_lcp_confreq),	"Last Received LCP Confreq" },
  { TYPE(PROXY_AUTH_TYPE),	0,	MAY,	NO,	FUNCS(proxy_auth_type),		"Proxy Authen Type" },
  { TYPE(PROXY_AUTH_NAME),	0,	MAY,	NO,	FUNCS(proxy_auth_name),		"Proxy Authen Name" },
  { TYPE(PROXY_AUTH_CHALLENGE),	0,	MAY,	NO,	FUNCS(proxy_auth_chall),	"Proxy Authen Challenge" },
  { TYPE(PROXY_AUTH_ID),	0,	MAY,	NO,	FUNCS(proxy_auth_id),		"Proxy Authen ID" },
  { TYPE(PROXY_AUTH_RESPONSE),	0,	MAY,	NO,	FUNCS(proxy_auth_rsp),		"Proxy Authen Response" },
  { TYPE(CALL_ERRORS),		0,	MAY,	YES,	FUNCS(call_errors),		"Call Errors" },
  { TYPE(ACCM),			0,	MAY,	YES,	FUNCS(accm),			"ACCM" },
  { TYPE(RANDOM_VECTOR),	0,	NO,	YES,	FUNCS(random_vector),		"Random Vector" },
  { TYPE(PRIV_GROUP_ID),	0,	MAY,	NO,	FUNCS(priv_group_id),		"Private Group ID" },
  { TYPE(RX_CONNECT_SPEED),	0,	MAY,	NO,	FUNCS(rx_connect_speed),	"Receive Connect Speed" },
  { TYPE(SEQUENCING_REQUIRED),	0,	NO,	YES,	FUNCS(sequencing_required),	"Sequencing Required" },
};

#define L2TP_AVP_TYPE_COUNT (sizeof(l2tp_avp_info) / sizeof(l2tp_avp_info[0]))

/* Message AVP parse tables */

struct l2tp_msg_info {
	int		type;		/* message type */
	uint64_t	mandatory; 	/* bit set at (1 << avp_type) implies mandatory */
	uint64_t	optional; 	/* bit set at (1 << avp_type) implies optional */
	const char	*name;		/* for debugging */
};

#define FBIT(avptype)   (1LL << L2TP_AVP_TYPE_##avptype)

static const struct l2tp_msg_info l2tp_msg_info[] = {
	{ L2TP_AVP_MSG_ILLEGAL,
	  0,
	  0, 
	  "ILLEGAL" },
	{ L2TP_AVP_MSG_SCCRQ, 
	  FBIT(PROTOCOL_VERSION) | FBIT(HOST_NAME) | FBIT(FRAMING_CAP) | FBIT(TUNNEL_ID),
	  FBIT(BEARER_CAP) | FBIT(RX_WINDOW_SIZE) | FBIT(CHALLENGE) | FBIT(TIEBREAKER) |
	  FBIT(FIRMWARE_REVISION) | FBIT(VENDOR_NAME) | FBIT(RANDOM_VECTOR),
	  "SCCRQ" },
	{ L2TP_AVP_MSG_SCCRP, 
	  FBIT(PROTOCOL_VERSION) | FBIT(HOST_NAME) | FBIT(FRAMING_CAP) | FBIT(TUNNEL_ID),
	  FBIT(BEARER_CAP) | FBIT(RX_WINDOW_SIZE) | FBIT(CHALLENGE) | 
	  FBIT(FIRMWARE_REVISION) | FBIT(VENDOR_NAME) | FBIT(CHALLENGE_RESPONSE) | FBIT(RANDOM_VECTOR),
	  "SCCRP" },
	{ L2TP_AVP_MSG_SCCCN,
	  0,
	  FBIT(CHALLENGE_RESPONSE) | FBIT(RANDOM_VECTOR),
	  "SCCCN" },
	{ L2TP_AVP_MSG_STOPCCN,
	  FBIT(TUNNEL_ID) | FBIT(RESULT_CODE),
	  FBIT(RANDOM_VECTOR),
	  "STOPCCN" },
	{ L2TP_AVP_MSG_RESERVED1,
	  0, 
	  0,
	  "RESERVED" },
	{ L2TP_AVP_MSG_HELLO,
	  0, 
	  0,
	  "HELLO" },
	{ L2TP_AVP_MSG_OCRQ,
	  FBIT(SESSION_ID) | FBIT(CALL_SERIAL_NUMBER) | FBIT(MINIMUM_BPS) |
	  FBIT(MAXIMUM_BPS) | FBIT(BEARER_TYPE) | FBIT(FRAMING_TYPE) | FBIT(CALLED_NUMBER),
	  FBIT(SUB_ADDRESS) | FBIT(RANDOM_VECTOR),
	  "OCRQ" },
	{ L2TP_AVP_MSG_OCRP,
	  FBIT(SESSION_ID),
	  FBIT(PHYSICAL_CHANNEL_ID) | FBIT(RANDOM_VECTOR),
	  "OCRP" },
	{ L2TP_AVP_MSG_OCCN,
	  FBIT(CONNECT_SPEED) | FBIT(FRAMING_TYPE),
	  FBIT(RX_CONNECT_SPEED) | FBIT(SEQUENCING_REQUIRED) | FBIT(RANDOM_VECTOR),
	  "OCCN" },
	{ L2TP_AVP_MSG_ICRQ,
	  FBIT(SESSION_ID) | FBIT(CALL_SERIAL_NUMBER),
	  FBIT(BEARER_TYPE) | FBIT(PHYSICAL_CHANNEL_ID) | FBIT(CALLING_NUMBER) |
	  FBIT(CALLED_NUMBER) | FBIT(SUB_ADDRESS) | FBIT(RANDOM_VECTOR),
	  "ICRQ" },
	{ L2TP_AVP_MSG_ICRP,
	  FBIT(SESSION_ID),
	  FBIT(RANDOM_VECTOR),
	  "ICRP" },
	{ L2TP_AVP_MSG_ICCN,
	  FBIT(CONNECT_SPEED) | FBIT(FRAMING_TYPE),
	  FBIT(INITIAL_RCVD_LCP_CONFREQ) | FBIT(LAST_SENT_LCP_CONFREQ) |
	  FBIT(LAST_RCVD_LCP_CONFREQ) | FBIT(PROXY_AUTH_NAME) |
	  FBIT(PROXY_AUTH_TYPE) | FBIT(PROXY_AUTH_CHALLENGE) | FBIT(PROXY_AUTH_ID) |
	  FBIT(PROXY_AUTH_RESPONSE) | FBIT(PRIV_GROUP_ID) | FBIT(RX_CONNECT_SPEED) |
	  FBIT(SEQUENCING_REQUIRED) | FBIT(RANDOM_VECTOR),
	  "ICCN" },
	{ L2TP_AVP_MSG_RESERVED2,
	  0, 
	  0,
	  "RESERVED" },
	{ L2TP_AVP_MSG_CDN,
	  FBIT(RESULT_CODE) | FBIT(SESSION_ID),
	  FBIT(Q931_CAUSE_CODE) | FBIT(RANDOM_VECTOR),
	  "CDN" },
	{ L2TP_AVP_MSG_WEN,
	  FBIT(CALL_ERRORS),
	  FBIT(RANDOM_VECTOR),
	  "WEN" },
	{ L2TP_AVP_MSG_SLI,
	  FBIT(ACCM),
	  FBIT(RANDOM_VECTOR),
	  "SLI" }
};

#define L2TP_MSG_INFO_COUNT (sizeof(l2tp_msg_info) / sizeof(l2tp_msg_info[0]))

#undef FBIT

/*
 * From RFC2661...
 *
 * Hiding an AVP value is done in several steps. The first step is to
 * take the length and value fields of the original (cleartext) AVP and
 * encode them into a Hidden AVP Subformat as follows:
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   Length of Original Value    |   Original Attribute Value ...
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    ...                          |             Padding ...
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Length of Original Attribute Value:  This is length of the Original
 * Attribute Value to be obscured in octets. This is necessary to
 * determine the original length of the Attribute Value which is lost
 * when the additional Padding is added.
 *
 * Original Attribute Value:  Attribute Value that is to be obscured.
 *
 * Padding:  Random additional octets used to obscure length of the
 * Attribute Value that is being hidden.
 *
 * To mask the size of the data being hidden, the resulting subformat
 * MAY be padded as shown above. Padding does NOT alter the value placed
 * in the Length of Original Attribute Value field, but does alter the
 * length of the resultant AVP that is being created. For example, If an
 * Attribute Value to be hidden is 4 octets in length, the unhidden AVP
 * length would be 10 octets (6 + Attribute Value length). After hiding,
 * the length of the AVP will become 6 + Attribute Value length + size
 * of the Length of Original Attribute Value field + Padding. Thus, if
 * Padding is 12 octets, the AVP length will be 6 + 4 + 2 + 12 = 24
 * octets.
 *
 * Next, An MD5 hash is performed on the concatenation of:
 *
 * + the 2 octet Attribute number of the AVP
 * + the shared secret
 * + an arbitrary length random vector
 *
 * The value of the random vector used in this hash is passed in the
 * value field of a Random Vector AVP. This Random Vector AVP must be
 * placed in the message by the sender before any hidden AVPs. The same
 * random vector may be used for more than one hidden AVP in the same
 * message. If a different random vector is used for the hiding of
 * subsequent AVPs then a new Random Vector AVP must be placed in the
 * command message before the first AVP to which it applies.
 *
 * The MD5 hash value is then XORed with the first 16 octet (or less)
 * segment of the Hidden AVP Subformat and placed in the Attribute Value
 * field of the Hidden AVP.  If the Hidden AVP Subformat is less than 16
 * octets, the Subformat is transformed as if the Attribute Value field
 * had been padded to 16 octets before the XOR, but only the actual
 * octets present in the Subformat are modified, and the length of the
 * AVP is not altered.
 *
 * If the Subformat is longer than 16 octets, a second one-way MD5 hash
 * is calculated over a stream of octets consisting of the shared secret
 * followed by the result of the first XOR.  That hash is XORed with the
 * second 16 octet (or less) segment of the Subformat and placed in the
 * corresponding octets of the Value field of the Hidden AVP.
 *
 * If necessary, this operation is repeated, with the shared secret used
 * along with each XOR result to generate the next hash to XOR the next
 * segment of the value with.
 */
static int l2tp_avp_hide_unhide(int hide, struct l2tp_avp_hdr *hdr, void *buffer, int buffer_len,
				unsigned char const *secret, int secret_len,
				unsigned char const *random_vector, int random_vector_len)
{
	uint8_t *data = buffer;
	struct MD5Context ctx;
	int buffer_pos;
	int digest_pos;
	uint8_t digest[16];

	/* Compute initial digest */
	MD5Init(&ctx);
	MD5Update(&ctx, &hdr->type, 2);
	MD5Update(&ctx, secret, secret_len);
	MD5Update(&ctx, random_vector, random_vector_len);
	MD5Final(digest, &ctx);

	/* Insert value */
	for (buffer_pos = 0, digest_pos = 0; buffer_pos < buffer_len; buffer_pos++, digest_pos++) {
		if (digest_pos == 16) {
			/* Compute new digest */
			digest_pos = 0;
			MD5Init(&ctx);
			MD5Update(&ctx, secret, secret_len);
			MD5Update(&ctx, &data[buffer_pos] - 16, 16);
			MD5Final(digest, &ctx);
		}
		data[buffer_pos] ^= digest[digest_pos];
	}

	L2TP_DEBUG(L2TP_AVPDATA, "%s: avp=%hu flag_len=%hx vend=%hu, hidden data is %s", __func__,
		   ntohs(hdr->type), ntohs(hdr->flag_len), ntohs(hdr->vendor_id),
		   l2tp_buffer_hexify(buffer, buffer_len));

	return 0;
}

static int l2tp_avp_hide(void **buffer, int *buffer_len,
			 unsigned char const *secret, int secret_len,
			 unsigned char const *random_vector, int random_vector_len)
{
	int orig_buffer_len = *buffer_len;
	void *orig_buffer = *buffer;
	void *new_buffer;
	uint16_t *orig_len;
	uint16_t *flag_len;
	uint16_t tmp;
	int new_buffer_len;
	int pad;

	if ((secret == NULL) || (secret_len == 0)) {
		return -EINVAL;
	}
	if ((random_vector == NULL) || (random_vector_len == 0)) {
		return -EINVAL;
	}
	if (orig_buffer_len < L2TP_AVP_HEADER_LEN) {
		return -EBADMSG;
	}

	/* Add a random amount of padding, between 1 and 
	 * L2TP_AVP_MAX_HIDE_PADDING bytes. The method used here is
	 * copied from the rand() man page. 
	 */
#ifdef BRCM_L2TP_SUPPORT
	pad = 1 + (int) (((L2TP_AVP_MAX_HIDE_PADDING) * rand()) / (RAND_MAX + 1.0));
#else
	pad = 1 + (int) ((((float) L2TP_AVP_MAX_HIDE_PADDING) * rand()) / (RAND_MAX + 1.0));
#endif

	L2TP_DEBUG(L2TP_AVPDATA, "%s: pad=%d, orig_len=%d, data to hide is %s", __func__,
		   pad, orig_buffer_len, l2tp_buffer_hexify(orig_buffer + L2TP_AVP_HEADER_LEN, orig_buffer_len - L2TP_AVP_HEADER_LEN));

	/* Allocate a new buffer big enough for the hidden AVP. Allow 2 bytes at the head
	 * for original length, plus 16 bytes slack for the MD5 algorithm. Use memmove()
	 * to copy data to new buffer because realloc() might return the original buffer
	 * and we just need to shift the data up 2 bytes.
	 */
	new_buffer_len = orig_buffer_len + 2 + pad + 16;
	new_buffer = realloc(orig_buffer, new_buffer_len + L2TP_AVP_HEADER_LEN);
	if (new_buffer == NULL) {
		return -ENOMEM;
	}
	memmove(new_buffer + L2TP_AVP_HEADER_LEN + 2, orig_buffer + L2TP_AVP_HEADER_LEN, orig_buffer_len - L2TP_AVP_HEADER_LEN);
	orig_len = new_buffer + L2TP_AVP_HEADER_LEN;
	*orig_len = htons(orig_buffer_len - L2TP_AVP_HEADER_LEN);
	if (new_buffer != orig_buffer) {
		*buffer = new_buffer;
	}
	flag_len = new_buffer;
	tmp = ntohs(*flag_len);
	*flag_len = htons(tmp + 2 + pad);
	*buffer_len = orig_buffer_len + 2 + pad;

	L2TP_DEBUG(L2TP_AVPDATA, "%s: modified data to hide is %s", __func__,
		   l2tp_buffer_hexify(new_buffer + L2TP_AVP_HEADER_LEN, *buffer_len - L2TP_AVP_HEADER_LEN));

	return l2tp_avp_hide_unhide(1 /* hide */, (void *) new_buffer, new_buffer + L2TP_AVP_HEADER_LEN, *buffer_len - L2TP_AVP_HEADER_LEN,
				    secret, secret_len, random_vector, random_vector_len);
}

static int l2tp_avp_unhide(void *buffer, int *buffer_len,
			   unsigned char const *secret, int secret_len,
			   unsigned char const *random_vector, int random_vector_len)
{
	struct l2tp_avp_hdr *hdr = buffer;
	uint16_t *orig_len = buffer + L2TP_AVP_HEADER_LEN;
	uint16_t orig_buffer_len;
	int result;
	uint16_t flag_len;

	if ((secret == NULL) || (secret_len == 0)) {
		return -EINVAL;
	}
	if ((random_vector == NULL) || (random_vector_len == 0)) {
		return -EINVAL;
	}

	L2TP_DEBUG(L2TP_AVPDATA, "%s: buffer_len=%d, data to unhide is %s", __func__,
		   *buffer_len, l2tp_buffer_hexify(buffer + L2TP_AVP_HEADER_LEN, *buffer_len - L2TP_AVP_HEADER_LEN));

	if (*buffer_len < L2TP_AVP_HEADER_LEN) {
		L2TP_DEBUG(L2TP_AVPDATA, "%s: bad message, buffer_len=%d", __func__, *buffer_len);
		return -EBADMSG;
	}

	result = l2tp_avp_hide_unhide(0 /* unhide */, hdr, buffer + L2TP_AVP_HEADER_LEN, *buffer_len - L2TP_AVP_HEADER_LEN,
				      secret, secret_len, random_vector, random_vector_len);
	if (result == 0) {
		orig_buffer_len = ntohs(*orig_len);
		if (orig_buffer_len > (*buffer_len - L2TP_AVP_HEADER_LEN)) {
			L2TP_DEBUG(L2TP_AVPDATA, "%s: bad message, orig_len=%d", __func__, orig_buffer_len);
			return -EBADMSG;
		}
		flag_len = ntohs(hdr->flag_len);
		BUG_TRAP((flag_len & L2TP_AVP_HDR_HBIT) == 0);
		flag_len &= ~0x03ff;
		flag_len |= (orig_buffer_len + L2TP_AVP_HEADER_LEN);
		hdr->flag_len = htons(flag_len);
		memmove(buffer + L2TP_AVP_HEADER_LEN, buffer + L2TP_AVP_HEADER_LEN + 2, orig_buffer_len);
		*buffer_len = orig_buffer_len + L2TP_AVP_HEADER_LEN;
	}

	return 0;
}

static void l2tp_avp_hdr_write(struct l2tp_avp_hdr *hdr, 
			       struct l2tp_msg_info const *msg_info, 
			       const uint16_t avp_type, const int var_len, int hide)
{
	uint16_t flag_len;

	flag_len = L2TP_AVP_HDR_LEN(L2TP_AVP_HEADER_LEN + var_len);
	if (l2tp_avp_info[avp_type].mandatory) {
		flag_len |= L2TP_AVP_HDR_MBIT;
	}
	if (hide && l2tp_avp_info[avp_type].hide) {
		flag_len |= L2TP_AVP_HDR_HBIT;
	}
	hdr->vendor_id = 0;
	hdr->type = htons(avp_type);
	hdr->flag_len = htons(flag_len);
}

static char *l2tp_avp_stringify(void *buf, int buf_len)
{
	static char string_buf[256];
	int max_len = (buf_len >= (sizeof(string_buf) - 1)) ? sizeof(string_buf) - 1 : buf_len;

	strncpy(&string_buf[0], buf, max_len);
	string_buf[max_len] = '\0';
	
	return &string_buf[0];
}
 
static int l2tp_avp_encode_message_type(void *buffer,
					int buffer_len,
					struct l2tp_avp_desc const *data,
					struct l2tp_msg_info const *msg_info,
					struct l2tp_tunnel const *tunnel, int hide)
{
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_message_type *avp_data = (void *) &hdr->value[0];
	
	l2tp_avp_hdr_write(hdr, msg_info, TYPE(MESSAGE), sizeof(struct l2tp_avp_message_type), 0);
	avp_data->type = htons(data->value->message_type.type);
	return 0;
}

static int l2tp_avp_decode_message_type(struct l2tp_avp_hdr *hdr,
					struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_message_type) + L2TP_AVP_HEADER_LEN)) {
		l2tp_tunnel_log(tunnel, L2TP_AVP, LOG_ERR, "AVP: tunl %hu: bad MESSAGE_TYPE message length=%hu", 
				l2tp_tunnel_id(tunnel), avp_len);
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->message_type.type = ntohs(result->value->message_type.type);
	return avp_len;
}

static int l2tp_avp_encode_result_code(void *buffer,
				       int buffer_len,
				       struct l2tp_avp_desc const *data,
				       struct l2tp_msg_info const *msg_info,

				       struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_result_code *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(RESULT_CODE), data->value_len, hide);
	avp_data->result_code = htons(data->value->result_code.result_code);
	if (data->value_len > 2) {
		avp_data->error_code = htons(data->value->result_code.error_code);
	}
	if (data->value_len > 4) {
		memcpy(&avp_data->error_message[0], data->value->result_code.error_message, data->value_len - 4);
	}
	return result;
}

static int l2tp_avp_decode_result_code(struct l2tp_avp_hdr *hdr,
				       struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len < (sizeof(result->value->result_code.result_code) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->result_code.result_code = ntohs(result->value->result_code.result_code);
	if (result->value_len > 2) {
		result->value->result_code.error_code = ntohs(result->value->result_code.error_code);
	}
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: RESULT_CODE: result=%hu error=%hu msg=%s", 
			result->value->result_code.result_code,
			result->value_len > 2 ? result->value->result_code.error_code : 0,
			result->value_len > 4 ? l2tp_avp_stringify(result->value->result_code.error_message,
								   result->value_len - 4) : "");
	return avp_len;
}

static int l2tp_avp_encode_protocol_version(void *buffer,
					    int buffer_len,
					    struct l2tp_avp_desc const *data,
					    struct l2tp_msg_info const *msg_info,
  
					    struct l2tp_tunnel const *tunnel, int hide)
{
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_protocol_version *avp_data = (void *) &hdr->value[0];
	
	l2tp_avp_hdr_write(hdr, msg_info, TYPE(PROTOCOL_VERSION), sizeof(struct l2tp_avp_protocol_version), 0);
	avp_data->ver = data->value->protocol_version.ver;
	avp_data->rev = data->value->protocol_version.rev;
	return 0;
}

static int l2tp_avp_decode_protocol_version(struct l2tp_avp_hdr *hdr,
					    struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_protocol_version) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: PROTOCOL_VERSION: ver=%d rev=%d", 
			result->value->protocol_version.ver, result->value->protocol_version.rev); 
	return avp_len;
}

static int l2tp_avp_encode_framing_cap(void *buffer,
				       int buffer_len,
				       struct l2tp_avp_desc const *data,
				       struct l2tp_msg_info const *msg_info,

				       struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_framing_cap *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(FRAMING_CAP), sizeof(struct l2tp_avp_framing_cap), hide);
	avp_data->value = htonl(data->value->framing_cap.value);
	return result;
}

static int l2tp_avp_decode_framing_cap(struct l2tp_avp_hdr *hdr,
				       struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_framing_cap) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->framing_cap.value = ntohl(result->value->framing_cap.value);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: FRAMING_CAP: cap=%hu", result->value->framing_cap.value);
	return avp_len;
}

static int l2tp_avp_encode_bearer_cap(void *buffer,
				      int buffer_len,
				      struct l2tp_avp_desc const *data,
				      struct l2tp_msg_info const *msg_info,

				      struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_bearer_cap *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(BEARER_CAP), sizeof(struct l2tp_avp_bearer_cap), hide);
	avp_data->value = htonl(data->value->bearer_cap.value);
	return result;
}

static int l2tp_avp_decode_bearer_cap(struct l2tp_avp_hdr *hdr,
				      struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_bearer_cap) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->bearer_cap.value = ntohl(result->value->bearer_cap.value);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: BEARER_CAP: cap=%hu", result->value->bearer_cap.value);
	return avp_len;
}

static int l2tp_avp_encode_tiebreaker(void *buffer,
				       int buffer_len,
				       struct l2tp_avp_desc const *data,
				       struct l2tp_msg_info const *msg_info,

				       struct l2tp_tunnel const *tunnel, int hide)
{
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_tiebreaker *avp_data = (void *) &hdr->value[0];
	
	l2tp_avp_hdr_write(hdr, msg_info, TYPE(TIEBREAKER), sizeof(struct l2tp_avp_tiebreaker), 0);
	memcpy(&avp_data->value[0], &data->value->tiebreaker.value[0], sizeof(struct l2tp_avp_tiebreaker));
	return 0;
}

static int l2tp_avp_decode_tiebreaker(struct l2tp_avp_hdr *hdr,
				       struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_tiebreaker) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: TIEBREAKER: value=%02x%02x%02x%02x%02x%02x%02x%02x", 
			result->value->tiebreaker.value[0], result->value->tiebreaker.value[1],
			result->value->tiebreaker.value[2], result->value->tiebreaker.value[3],
			result->value->tiebreaker.value[4], result->value->tiebreaker.value[5],
			result->value->tiebreaker.value[6], result->value->tiebreaker.value[7]);
	return avp_len;
}

static int l2tp_avp_encode_firmware_revision(void *buffer,
					     int buffer_len,
					     struct l2tp_avp_desc const *data,
					     struct l2tp_msg_info const *msg_info,

					     struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_firmware_revision *avp_data = (void *) &hdr->value[0];

	hdr = buffer;
	l2tp_avp_hdr_write(hdr, msg_info, TYPE(FIRMWARE_REVISION), sizeof(struct l2tp_avp_firmware_revision), hide);
	avp_data->value = htons(data->value->firmware_revision.value);
	return result;
}

static int l2tp_avp_decode_firmware_revision(struct l2tp_avp_hdr *hdr,
					     struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_firmware_revision) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->firmware_revision.value = ntohs(result->value->firmware_revision.value);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: FIRMWARE_VERSION: revision=%hu", result->value->firmware_revision.value);
	return avp_len;
}

static int l2tp_avp_encode_host_name(void *buffer,
				     int buffer_len,
				     struct l2tp_avp_desc const *data,
				     struct l2tp_msg_info const *msg_info,

				     struct l2tp_tunnel const *tunnel, int hide)
{
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_host_name *avp_data = (void *) &hdr->value[0];
	
	l2tp_avp_hdr_write(hdr, msg_info, TYPE(HOST_NAME), data->value_len, 0);
	memcpy(&avp_data->string[0], &data->value->host_name.string[0], data->value_len);
	return 0;
}

static int l2tp_avp_decode_host_name(struct l2tp_avp_hdr *hdr,
				     struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: HOST_NAME: name=%s", 
			l2tp_avp_stringify(result->value->host_name.string, result->value_len));
	return avp_len;
}

static int l2tp_avp_encode_vendor_name(void *buffer,
				       int buffer_len,
				       struct l2tp_avp_desc const *data,
				       struct l2tp_msg_info const *msg_info,

				       struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_vendor_name *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(VENDOR_NAME), data->value_len, hide);
	memcpy(&avp_data->string[0], &data->value->vendor_name.string[0], data->value_len);
	return result;
}

static int l2tp_avp_decode_vendor_name(struct l2tp_avp_hdr *hdr,
				       struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: VENDOR_NAME: name=%s", 
			l2tp_avp_stringify(result->value->vendor_name.string, result->value_len));
	return avp_len;
}

static int l2tp_avp_encode_tunnel_id(void *buffer,
				     int buffer_len,
				     struct l2tp_avp_desc const *data,
				     struct l2tp_msg_info const *msg_info,

				     struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_tunnel_id *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(TUNNEL_ID), sizeof(struct l2tp_avp_tunnel_id), hide);
	avp_data->value = htons(data->value->tunnel_id.value);
	return result;
}

static int l2tp_avp_decode_tunnel_id(struct l2tp_avp_hdr *hdr,
				     struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_tunnel_id) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->tunnel_id.value = ntohs(result->value->tunnel_id.value);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: TUNNEL_ID: id=%hu", result->value->tunnel_id.value);
	return avp_len;
}

static int l2tp_avp_encode_rx_window_size(void *buffer,
					  int buffer_len,
					  struct l2tp_avp_desc const *data,
					  struct l2tp_msg_info const *msg_info,

					  struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_rx_window_size *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(RX_WINDOW_SIZE), sizeof(struct l2tp_avp_rx_window_size), hide);
	avp_data->value = htons(data->value->rx_window_size.value);
	return result;
}

static int l2tp_avp_decode_rx_window_size(struct l2tp_avp_hdr *hdr,
					  struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_rx_window_size) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->rx_window_size.value = ntohs(result->value->rx_window_size.value);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: RX_WINDOW_SIZE: size=%hu", result->value->rx_window_size.value);
	return avp_len;
}

static int l2tp_avp_encode_challenge(void *buffer,
				     int buffer_len,
				     struct l2tp_avp_desc const *data,
				     struct l2tp_msg_info const *msg_info,

				     struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_challenge *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(CHALLENGE), data->value_len, hide);
	memcpy(&avp_data->value[0], data->value->challenge.value, data->value_len);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: challenge data is %s", 
			l2tp_buffer_hexify(data->value->challenge.value, data->value_len));
	return result;
}

static int l2tp_avp_decode_challenge(struct l2tp_avp_hdr *hdr,
				     struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: CHALLENGE: challenge=%s", 
			l2tp_buffer_hexify(result->value, result->value_len));
	return avp_len;
}

static int l2tp_avp_encode_q931_cause_code(void *buffer,
					   int buffer_len,
					   struct l2tp_avp_desc const *data,
					   struct l2tp_msg_info const *msg_info,

					   struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_q931_cause_code *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(Q931_CAUSE_CODE), data->value_len, hide);
	avp_data->cause_code = htons(data->value->q931_cause_code.cause_code);
	avp_data->cause_msg = htons(data->value->q931_cause_code.cause_msg);
	memcpy(&avp_data->advisory_msg[0], &data->value->q931_cause_code.advisory_msg[0], data->value_len - sizeof(struct l2tp_avp_q931_cause_code));
	return result;
}

static int l2tp_avp_decode_q931_cause_code(struct l2tp_avp_hdr *hdr,
					   struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_q931_cause_code) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->q931_cause_code.cause_code = ntohs(result->value->q931_cause_code.cause_code);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: Q931_CAUSE_CODE: code=%hu", result->value->q931_cause_code.cause_code);
	return avp_len;
}

static int l2tp_avp_encode_challenge_response(void *buffer,
					      int buffer_len,
					      struct l2tp_avp_desc const *data,
					      struct l2tp_msg_info const *msg_info,

					      struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_challenge_response *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(CHALLENGE_RESPONSE), sizeof(struct l2tp_avp_challenge_response), hide);
	memcpy(&avp_data->value[0], data->value->challenge_response.value, sizeof(struct l2tp_avp_challenge_response));
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: challenge response data is %s", l2tp_buffer_hexify(avp_data, sizeof(*avp_data)));
	return result;
}

static int l2tp_avp_decode_challenge_response(struct l2tp_avp_hdr *hdr,
					      struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	result->value = (void *) &hdr->value[0];
	result->value_len = 16;	/* cisco responses can have an extra 6 bytes! Ignore them */
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: CHALLENGE_RESPONSE: value=%s", l2tp_buffer_hexify(result->value, result->value_len));
	if ((avp_len - L2TP_AVP_HEADER_LEN) < 16) {
		return -EBADMSG;
	}
	return avp_len;
}

static int l2tp_avp_encode_session_id(void *buffer,
				      int buffer_len,
				      struct l2tp_avp_desc const *data,
				      struct l2tp_msg_info const *msg_info,

				      struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_session_id *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(SESSION_ID), sizeof(struct l2tp_avp_session_id), hide);
	avp_data->value = htons(data->value->session_id.value);
	return result;
}

static int l2tp_avp_decode_session_id(struct l2tp_avp_hdr *hdr,
				      struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_session_id) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->session_id.value = ntohs(result->value->session_id.value);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: SESSION_ID: id=%hu", result->value->session_id.value);
	return avp_len;
}

static int l2tp_avp_encode_call_serial_number(void *buffer,
					      int buffer_len,
					      struct l2tp_avp_desc const *data,
					      struct l2tp_msg_info const *msg_info,

					      struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_call_serial_number *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(CALL_SERIAL_NUMBER), sizeof(struct l2tp_avp_call_serial_number), hide);
	avp_data->value = htonl(data->value->call_serial_number.value);
	return result;
}

static int l2tp_avp_decode_call_serial_number(struct l2tp_avp_hdr *hdr,
					      struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_call_serial_number) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->call_serial_number.value = ntohl(result->value->call_serial_number.value);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: CALL_SERIAL_NUMBER: value=%u", result->value->call_serial_number.value);
	return avp_len;
}

static int l2tp_avp_encode_minimum_bps(void *buffer,
				       int buffer_len,
				       struct l2tp_avp_desc const *data,
				       struct l2tp_msg_info const *msg_info,

				       struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_minimum_bps *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(MINIMUM_BPS), sizeof(struct l2tp_avp_minimum_bps), hide);
	avp_data->value = htonl(data->value->minimum_bps.value);
	return result;
}

static int l2tp_avp_decode_minimum_bps(struct l2tp_avp_hdr *hdr,
				       struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_minimum_bps) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->minimum_bps.value = ntohl(result->value->minimum_bps.value);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: MINIMUM_BPS: value=%u", result->value->minimum_bps.value);
	return avp_len;
}

static int l2tp_avp_encode_maximum_bps(void *buffer,
				       int buffer_len,
				       struct l2tp_avp_desc const *data,
				       struct l2tp_msg_info const *msg_info,

				       struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_maximum_bps *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(MAXIMUM_BPS), sizeof(struct l2tp_avp_maximum_bps), hide);
	avp_data->value = htonl(data->value->maximum_bps.value);
	return result;
}

static int l2tp_avp_decode_maximum_bps(struct l2tp_avp_hdr *hdr,
				       struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_maximum_bps) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->maximum_bps.value = ntohl(result->value->maximum_bps.value);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: MAXIMUM_BPS: value=%u", result->value->maximum_bps.value);
	return avp_len;
}

static int l2tp_avp_encode_bearer_type(void *buffer,
				       int buffer_len,
				       struct l2tp_avp_desc const *data,
				       struct l2tp_msg_info const *msg_info,

				       struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_bearer_type *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(BEARER_TYPE), sizeof(struct l2tp_avp_bearer_type), hide);
	avp_data->value = htonl(data->value->bearer_type.value);
	return result;
}

static int l2tp_avp_decode_bearer_type(struct l2tp_avp_hdr *hdr,
				       struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_bearer_type) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->bearer_type.value = ntohl(result->value->bearer_type.value);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: BEARER_TYPE: type=%u", result->value->bearer_type.value);
	return avp_len;
}

static int l2tp_avp_encode_framing_type(void *buffer,
					int buffer_len,
					struct l2tp_avp_desc const *data,
					struct l2tp_msg_info const *msg_info,

					struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_framing_type *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(FRAMING_TYPE), sizeof(struct l2tp_avp_framing_type), hide);
	avp_data->value = htonl(data->value->framing_type.value);
	return result;
}

static int l2tp_avp_decode_framing_type(struct l2tp_avp_hdr *hdr,
					struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_framing_type) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->framing_type.value = ntohl(result->value->framing_type.value);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: FRAMING_TYPE: type=%u", result->value->framing_type.value);
	return avp_len;
}

static int l2tp_avp_encode_packet_proc_delay(void *buffer,
					     int buffer_len,
					     struct l2tp_avp_desc const *data,
					     struct l2tp_msg_info const *msg_info,

					     struct l2tp_tunnel const *tunnel, int hide)
{
	/* Ignored. This AVP was only present in the draft RFC */
	return 0;
}

static int l2tp_avp_decode_packet_proc_delay(struct l2tp_avp_hdr *hdr,
					     struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	/* Ignored. This AVP was only present in the draft RFC */
	return 0;
}

static int l2tp_avp_encode_called_number(void *buffer,
					 int buffer_len,
					 struct l2tp_avp_desc const *data,
					 struct l2tp_msg_info const *msg_info,

					 struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_called_number *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(CALLED_NUMBER), data->value_len, hide);
	memcpy(&avp_data->string[0], &data->value->called_number.string[0], data->value_len);
	return result;
}

static int l2tp_avp_decode_called_number(struct l2tp_avp_hdr *hdr,
					 struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	struct l2tp_avp_called_number *called_number = (void *) &hdr->value[0];

	result->value = (void *) called_number->string;
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: CALLED_NUMBER: value=%s", 
			l2tp_avp_stringify(&result->value->called_number.string[0], result->value_len));
	return avp_len;
}

static int l2tp_avp_encode_calling_number(void *buffer,
					  int buffer_len,
					  struct l2tp_avp_desc const *data,
					  struct l2tp_msg_info const *msg_info,

					  struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_calling_number *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(CALLING_NUMBER), data->value_len, hide);
	memcpy(&avp_data->string[0], &data->value->calling_number.string[0], data->value_len);
	return result;
}

static int l2tp_avp_decode_calling_number(struct l2tp_avp_hdr *hdr,
					  struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);
	struct l2tp_avp_calling_number *calling_number = (void *) &hdr->value[0];

	result->value = (void *) calling_number->string;
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: CALLING_NUMBER: value=%s", 
			l2tp_avp_stringify(&result->value->calling_number.string[0], result->value_len));
	return avp_len;
}

static int l2tp_avp_encode_sub_address(void *buffer,
				       int buffer_len,
				       struct l2tp_avp_desc const *data,
				       struct l2tp_msg_info const *msg_info,

				       struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_sub_address *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(SUB_ADDRESS), data->value_len, hide);
	memcpy(&avp_data->string[0], &data->value->sub_address.string[0], data->value_len);
	return result;
}

static int l2tp_avp_decode_sub_address(struct l2tp_avp_hdr *hdr,
				       struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);
	struct l2tp_avp_sub_address *sub_address = (void *) &hdr->value[0];

	result->value = (void *) sub_address->string;
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: SUB_ADDRESS: value=%s", 
			l2tp_avp_stringify(&result->value->sub_address.string[0], result->value_len));
	return avp_len;
}

static int l2tp_avp_encode_tx_connect_speed(void *buffer,
					    int buffer_len,
					    struct l2tp_avp_desc const *data,
					    struct l2tp_msg_info const *msg_info,

					    struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_connect_speed *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(CONNECT_SPEED), sizeof(struct l2tp_avp_connect_speed), hide);
	avp_data->value = htonl(data->value->connect_speed.value);
	return result;
}

static int l2tp_avp_decode_tx_connect_speed(struct l2tp_avp_hdr *hdr,
					    struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_connect_speed) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->connect_speed.value = ntohl(result->value->connect_speed.value);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: TX_CONNECT_SPEED: value=%u", result->value->connect_speed.value);
	return avp_len;
}

static int l2tp_avp_encode_physical_channel_id(void *buffer,
					       int buffer_len,
					       struct l2tp_avp_desc const *data,
					       struct l2tp_msg_info const *msg_info,

					       struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_physical_channel_id *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(PHYSICAL_CHANNEL_ID), sizeof(struct l2tp_avp_physical_channel_id), hide);
	avp_data->value = htons(data->value->physical_channel_id.value);
	return result;
}

static int l2tp_avp_decode_physical_channel_id(struct l2tp_avp_hdr *hdr,
					       struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_physical_channel_id) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->physical_channel_id.value = ntohl(result->value->physical_channel_id.value);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: PHYSICAL_CHANNEL_ID: value=%u", result->value->physical_channel_id.value);
	return avp_len;
}

static int l2tp_avp_encode_initial_lcp_confreq(void *buffer,
					       int buffer_len,
					       struct l2tp_avp_desc const *data,
					       struct l2tp_msg_info const *msg_info,

					       struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_initial_rcvd_lcp_confreq *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(INITIAL_RCVD_LCP_CONFREQ), data->value_len, hide);
	memcpy(&avp_data->value[0], &data->value->initial_rcvd_lcp_confreq.value[0], data->value_len);
	return result;
}

static int l2tp_avp_decode_initial_lcp_confreq(struct l2tp_avp_hdr *hdr,
					       struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: INITIAL_LCP_CONFREQ: value=%s", 
			l2tp_buffer_hexify(result->value, result->value_len));
	return avp_len;
}

static int l2tp_avp_encode_last_sent_lcp_confreq(void *buffer,
						 int buffer_len,
						 struct l2tp_avp_desc const *data,
						 struct l2tp_msg_info const *msg_info,

						 struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_last_sent_lcp_confreq *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(LAST_SENT_LCP_CONFREQ), data->value_len, hide);
	memcpy(&avp_data->value[0], data->value->last_sent_lcp_confreq.value, data->value_len);
	return result;
}

static int l2tp_avp_decode_last_sent_lcp_confreq(struct l2tp_avp_hdr *hdr,
						 struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: LAST_SENT_LCP_CONFREQ: value=%s", 
			l2tp_buffer_hexify(result->value, result->value_len));
	return avp_len;
}

static int l2tp_avp_encode_last_rcvd_lcp_confreq(void *buffer,
						 int buffer_len,
						 struct l2tp_avp_desc const *data,
						 struct l2tp_msg_info const *msg_info,

						 struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_last_rcvd_lcp_confreq *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(LAST_RCVD_LCP_CONFREQ), data->value_len, hide);
	memcpy(&avp_data->value[0], data->value->last_rcvd_lcp_confreq.value, data->value_len);
	return result;
}

static int l2tp_avp_decode_last_rcvd_lcp_confreq(struct l2tp_avp_hdr *hdr,
						 struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: LAST_RCVD_LCP_CONFREQ: value=%s", 
			l2tp_buffer_hexify(result->value, result->value_len));
	return avp_len;
}

static int l2tp_avp_encode_proxy_auth_type(void *buffer,
					   int buffer_len,
					   struct l2tp_avp_desc const *data,
					   struct l2tp_msg_info const *msg_info,

					   struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_proxy_auth_type *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(PROXY_AUTH_TYPE), sizeof(struct l2tp_avp_proxy_auth_type), hide);
	avp_data->value = htons(data->value->proxy_auth_type.value);
	return result;
}

static int l2tp_avp_decode_proxy_auth_type(struct l2tp_avp_hdr *hdr,
					   struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_proxy_auth_type) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->proxy_auth_type.value = ntohs(result->value->proxy_auth_type.value);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: PROXY_AUTH_TYPE: value=%hu", result->value->proxy_auth_type.value);
	return avp_len;
}

static int l2tp_avp_encode_proxy_auth_name(void *buffer,
					   int buffer_len,
					   struct l2tp_avp_desc const *data,
					   struct l2tp_msg_info const *msg_info,

					   struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_proxy_auth_name *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(PROXY_AUTH_NAME), data->value_len, hide);
	memcpy(&avp_data->string[0], &data->value->proxy_auth_name.string[0], data->value_len);
	return result;
}

static int l2tp_avp_decode_proxy_auth_name(struct l2tp_avp_hdr *hdr,
					   struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: PROXY_AUTH_NAME: value=%s", 
			l2tp_avp_stringify(result->value, result->value_len));
	return avp_len;
}

static int l2tp_avp_encode_proxy_auth_chall(void *buffer,
					    int buffer_len,
					    struct l2tp_avp_desc const *data,
					    struct l2tp_msg_info const *msg_info,

					    struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_proxy_auth_challenge *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(PROXY_AUTH_CHALLENGE), data->value_len, hide);
	memcpy(&avp_data->value[0], &data->value->proxy_auth_challenge.value[0], data->value_len);
	return result;
}

static int l2tp_avp_decode_proxy_auth_chall(struct l2tp_avp_hdr *hdr,
					    struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: PROXY_AUTH_CHALLENGE: value=%s", 
			l2tp_buffer_hexify(result->value, result->value_len));
	return avp_len;
}

static int l2tp_avp_encode_proxy_auth_id(void *buffer,
					 int buffer_len,
					 struct l2tp_avp_desc const *data,
					 struct l2tp_msg_info const *msg_info,

					 struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_proxy_auth_id *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(PROXY_AUTH_ID), sizeof(struct l2tp_avp_proxy_auth_id), hide);
	avp_data->id = data->value->proxy_auth_id.id;
	return result;
}

static int l2tp_avp_decode_proxy_auth_id(struct l2tp_avp_hdr *hdr,
					 struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	if (result->value_len != sizeof(struct l2tp_avp_proxy_auth_id)) {
		return -EBADMSG;
	}
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: PROXY_AUTH_ID: value=%hu", result->value->proxy_auth_id.id);
	return avp_len;
}

static int l2tp_avp_encode_proxy_auth_rsp(void *buffer,
					  int buffer_len,
					  struct l2tp_avp_desc const *data,
					  struct l2tp_msg_info const *msg_info,

					  struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_proxy_auth_response *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(PROXY_AUTH_RESPONSE), data->value_len, hide);
	memcpy(&avp_data->value[0], &data->value->proxy_auth_response.value[0], data->value_len);
	return result;
}

static int l2tp_avp_decode_proxy_auth_rsp(struct l2tp_avp_hdr *hdr,
					  struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: PROXY_AUTH_RSP: value=%s", 
			l2tp_buffer_hexify(result->value, result->value_len));
	return avp_len;
}

static int l2tp_avp_encode_call_errors(void *buffer,
				       int buffer_len,
				       struct l2tp_avp_desc const *data,
				       struct l2tp_msg_info const *msg_info,

				       struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_call_errors *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(CALL_ERRORS), sizeof(struct l2tp_avp_call_errors), hide);
	avp_data->crc_errors = htonl(data->value->call_errors.crc_errors);
	avp_data->framing_errors = htonl(data->value->call_errors.framing_errors);
	avp_data->hardware_overruns = htonl(data->value->call_errors.hardware_overruns);
	avp_data->buffer_overruns = htonl(data->value->call_errors.buffer_overruns);
	avp_data->timeout_errors = htonl(data->value->call_errors.timeout_errors);
	avp_data->alignment_errors = htonl(data->value->call_errors.alignment_errors);
	return result;
}

static int l2tp_avp_decode_call_errors(struct l2tp_avp_hdr *hdr,
				       struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_call_errors) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->call_errors.crc_errors = ntohl(result->value->call_errors.crc_errors);
	result->value->call_errors.framing_errors = ntohl(result->value->call_errors.framing_errors);
	result->value->call_errors.hardware_overruns = ntohl(result->value->call_errors.hardware_overruns);
	result->value->call_errors.buffer_overruns = ntohl(result->value->call_errors.buffer_overruns);
	result->value->call_errors.timeout_errors = ntohl(result->value->call_errors.timeout_errors);
	result->value->call_errors.alignment_errors = ntohl(result->value->call_errors.alignment_errors);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: CALL_ERRORS: crc=%hu framing=%hu hw=%d bufover=%hu timeout=%hu align=%hu", 
			result->value->call_errors.crc_errors,
			result->value->call_errors.framing_errors,
			result->value->call_errors.hardware_overruns,
			result->value->call_errors.buffer_overruns,
			result->value->call_errors.timeout_errors,
			result->value->call_errors.alignment_errors);
	return avp_len;
}

static int l2tp_avp_encode_accm(void *buffer,
				int buffer_len,
				struct l2tp_avp_desc const *data,
				struct l2tp_msg_info const *msg_info,

				struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_accm *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(ACCM), sizeof(struct l2tp_avp_accm), hide);
	avp_data->send_accm = htonl(data->value->accm.send_accm);
	avp_data->recv_accm = htonl(data->value->accm.recv_accm);
	return result;
}

static int l2tp_avp_decode_accm(struct l2tp_avp_hdr *hdr,
				struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_accm) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->accm.send_accm = ntohl(result->value->accm.send_accm);
	result->value->accm.recv_accm = ntohl(result->value->accm.recv_accm);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: ACCM: send=%08x recv=%08x", 
			result->value->accm.send_accm, result->value->accm.recv_accm);
	return avp_len;
}

static int l2tp_avp_encode_random_vector(void *buffer,
					 int buffer_len,
					 struct l2tp_avp_desc const *data,
					 struct l2tp_msg_info const *msg_info,

					 struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_random_vector *avp_data = (void *) &hdr->value[0];
	
	l2tp_avp_hdr_write(hdr, msg_info, TYPE(RANDOM_VECTOR), data->value_len, 0);
	memcpy(&avp_data->value[0], &data->value->random_vector.value[0], data->value_len);
	return result;
}

static int l2tp_avp_decode_random_vector(struct l2tp_avp_hdr *hdr,
					 struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: RANDOM_VECTOR: value=%s", 
			l2tp_buffer_hexify(result->value, result->value_len));
	return avp_len;
}

static int l2tp_avp_encode_priv_group_id(void *buffer,
					 int buffer_len,
					 struct l2tp_avp_desc const *data,
					 struct l2tp_msg_info const *msg_info,

					 struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_priv_group_id *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(PRIV_GROUP_ID), data->value_len, hide);
	memcpy(&avp_data->string[0], &data->value->priv_group_id.string[0], data->value_len);
	return result;
}

static int l2tp_avp_decode_priv_group_id(struct l2tp_avp_hdr *hdr,
					 struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: PRIV_GROUP_ID: value=%s", 
			l2tp_avp_stringify(result->value, result->value_len));
	return avp_len;
}

static int l2tp_avp_encode_rx_connect_speed(void *buffer,
					    int buffer_len,
					    struct l2tp_avp_desc const *data,
					    struct l2tp_msg_info const *msg_info,

					    struct l2tp_tunnel const *tunnel, int hide)
{
	int result = buffer_len;
	struct l2tp_avp_hdr *hdr = buffer;
	struct l2tp_avp_rx_connect_speed *avp_data = (void *) &hdr->value[0];

	l2tp_avp_hdr_write(hdr, msg_info, TYPE(RX_CONNECT_SPEED), sizeof(struct l2tp_avp_rx_connect_speed), hide);
	avp_data->value = htons(data->value->rx_connect_speed.value);
	return result;
}

static int l2tp_avp_decode_rx_connect_speed(struct l2tp_avp_hdr *hdr,
					    struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != (sizeof(struct l2tp_avp_rx_connect_speed) + L2TP_AVP_HEADER_LEN)) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = avp_len - L2TP_AVP_HEADER_LEN;
	result->value->rx_connect_speed.value = ntohl(result->value->rx_connect_speed.value);
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: RX_CONNECT_SPEED: value=%u", result->value->rx_connect_speed.value);
	return avp_len;
}

static int l2tp_avp_encode_sequencing_required(void *buffer,
					       int buffer_len,
					       struct l2tp_avp_desc const *data,
					       struct l2tp_msg_info const *msg_info,

					       struct l2tp_tunnel const *tunnel, int hide)
{
	struct l2tp_avp_hdr *hdr = buffer;
	
	l2tp_avp_hdr_write(hdr, msg_info, TYPE(SEQUENCING_REQUIRED), 0, 0);
	L2TP_DEBUG(L2TP_AVPDATA, "AVPDATA: SEQ_REQUIRED");
	return 0;
}

static int l2tp_avp_decode_sequencing_required(struct l2tp_avp_hdr *hdr,
					       struct l2tp_avp_desc *result, struct l2tp_tunnel const *tunnel)
{
	uint16_t flag_len = ntohs(hdr->flag_len);
	int avp_len = L2TP_AVP_HDR_LEN(flag_len);

	if (avp_len != L2TP_AVP_HEADER_LEN) {
		return -EBADMSG;
	}
	result->value = (void *) &hdr->value[0];
	result->value_len = 0;
	l2tp_tunnel_log(tunnel, L2TP_AVPDATA, LOG_DEBUG, "AVPDATA: SEQ_REQD");
	return avp_len;
}

int l2tp_avp_hidable(struct l2tp_avp_hdr *avp)
{
	uint16_t type = ntohs(avp->type);

	if (avp->vendor_id == 0) {
		if (type < L2TP_AVP_TYPE_COUNT) {
			return l2tp_avp_info[type].hide != NO ? 1 : 0;
		}
	} else {
		/* no vendor specific AVPs supported */
	}

	return -EBADMSG;
}

int l2tp_avp_message_decode(int msg_len, struct l2tp_avp_hdr *first_avp, struct l2tp_avp_desc *data, 
			    struct l2tp_tunnel const *tunnel)
{
	int result = 0;
	int len = msg_len;
	struct l2tp_msg_info const *info;
	uint16_t avp_type;
	uint16_t msg_type;
	uint16_t avp_flag_len;
	int avp_len;
	int unhidden_avp_len;
	struct l2tp_avp_hdr *avp = first_avp;
	uint64_t avp_parse_flags = 0;
	char *secret;
	int secret_len;

	if (len <= 0) {
		return 0;
	}

	l2tp_tunnel_get_secret(tunnel, &secret, &secret_len);

	/* avp points to the first AVP, which is always MESSAGE_TYPE.
	 * Get the message type now to find out how to parse the
	 * remainder.
	 */
	if (avp->type != L2TP_AVP_TYPE_MESSAGE) {
		l2tp_tunnel_log(tunnel, L2TP_AVP, LOG_ERR, "AVP: tunl %hu: first AVP is not MESSAGE_TYPE: type=%hu", 
				l2tp_tunnel_id(tunnel), avp->type);
		l2tp_stats.illegal_messages++;
		goto illegal_message;
	}
	avp_len = l2tp_avp_decode_message_type(avp, data, tunnel);
	if (avp_len < 0) {
		l2tp_stats.illegal_messages++;
		goto bad_message;
	}
	msg_type = data->value->message_type.type;

	avp = ((void *) avp) + avp_len;
	len -= avp_len;

	if (msg_type < L2TP_AVP_MSG_COUNT) {
		info = &l2tp_msg_info[msg_type];
		l2tp_tunnel_log(tunnel, L2TP_AVP, LOG_DEBUG, "AVP: tunl %hu: %s message decode of %d bytes started", 
				l2tp_tunnel_id(tunnel), info->name, len);
		while (len > 0) {
			avp_type = ntohs(avp->type);
			avp_flag_len = ntohs(avp->flag_len);
			avp_len = unhidden_avp_len = L2TP_AVP_HDR_LEN(avp_flag_len);
			if ((avp_len == 0) && (avp_type == 0)) {
				l2tp_tunnel_log(tunnel, L2TP_AVP, LOG_WARNING, "AVP: tunl %hu: ignoring %d bytes of control frame", l2tp_tunnel_id(tunnel), len);
				L2TP_AVP_HEXDUMP(l2tp_tunnel_get_trace_flags(tunnel), avp, len);
				break;
			}
			if ((avp_len >= 1024) || (avp_len < L2TP_AVP_HEADER_LEN) || (avp_len > len)) {
				l2tp_tunnel_log(tunnel, L2TP_AVP, LOG_ERR, "AVP: tunl %hu: avp=%hu, bad length %hu", l2tp_tunnel_id(tunnel), avp_type, avp_len);
				l2tp_stats.illegal_messages++;
				l2tp_stats.messages[msg_type].rx_bad++;
				goto illegal_avp;
			}

			/* check if avp is illegally hidden */
			if ((avp_flag_len & L2TP_AVP_HDR_HBIT) && (l2tp_avp_hidable(avp) == 0)) {
				l2tp_tunnel_log(tunnel, L2TP_AVP, LOG_ERR, "AVP: tunl %hu: avp=%hu, illegally hidden", l2tp_tunnel_id(tunnel), avp_type);
				L2TP_AVP_HEXDUMP(l2tp_tunnel_get_trace_flags(tunnel), avp, avp_len);
				l2tp_stats.illegal_messages++;
				l2tp_stats.messages[msg_type].rx_bad++;
				goto illegal_avp;
			}

			/* Ref: RFC2661: 4.4.2
			 * If AVP is hidden, check we already have the random_vector 
			 */
			if ((avp_flag_len & L2TP_AVP_HDR_HBIT) && (data[TYPE(RANDOM_VECTOR)].value == NULL)) {
				l2tp_tunnel_log(tunnel, L2TP_AVP, LOG_ERR,
					   "AVP: tunl %hu: hidden AVP before random vector in %s: parse_flags=%llx",
					   l2tp_tunnel_id(tunnel), info->name, avp_parse_flags);
				l2tp_stats.illegal_messages++;
				l2tp_stats.messages[msg_type].rx_bad++;
				goto illegal_message;
			}
 
			/* If AVP is hidden, unhide it */
			if (avp_flag_len & L2TP_AVP_HDR_HBIT) {
				result = l2tp_avp_unhide(avp, &unhidden_avp_len, 
							 (unsigned char *const) secret, secret_len,
							 (unsigned char *const) data[TYPE(RANDOM_VECTOR)].value, 
							 data[TYPE(RANDOM_VECTOR].value_len));
				if (result < 0) {
					l2tp_tunnel_log(tunnel, L2TP_AVPHIDE, LOG_ERR, "AVPHIDE: tunl %hu: avp unhide error: %s", 
							l2tp_tunnel_id(tunnel), l2tp_strerror(-result));
					l2tp_stats.messages[msg_type].rx_bad++;
					goto unhide_error;
				}
				L2TP_DEBUG(L2TP_AVPHIDE, "%s: avp=%hu, unhidden avp now has the following data:", __func__, avp_type);
				if (l2tp_tunnel_get_trace_flags(tunnel) & L2TP_AVPHIDE) {
					L2TP_AVP_HEXDUMP(L2TP_AVPDATA, avp, unhidden_avp_len);
				}
			}

			/* Decode the AVP. */
			if ((avp_type < L2TP_AVP_TYPE_COUNT) && (avp->vendor_id == 0)) {
				L2TP_DEBUG(L2TP_AVP, "AVP: %s, len %d", l2tp_avp_info[avp_type].name, unhidden_avp_len);

				result = (*l2tp_avp_info[avp_type].decode)(avp, &data[avp_type], tunnel);
				if (result < 0) {
					l2tp_tunnel_log(tunnel, L2TP_AVP, LOG_ERR, "AVP: tunl %hu: avp_decode: %s", 
							l2tp_tunnel_id(tunnel), l2tp_strerror(-result));

					/* If AVP could not be decoded and has M bit set,
					 * reject the whole message.
					 */
					if (avp_flag_len & L2TP_AVP_HDR_MBIT) {
						l2tp_stats.unsupported_messages++;
						l2tp_stats.messages[msg_type].rx_bad++;
						goto unsupported_message;
					}

					l2tp_stats.ignored_avps++;
					goto skip_avp;
				}
			} else {
				/* Unsupported AVP. Ignore it, unless its mandatory bit
				 * is set, in which case reject the whole message 
				 */
				l2tp_stats.ignored_avps++;
				if (avp->vendor_id != 0) {
					l2tp_stats.vendor_avps++;
				}
				if (avp_flag_len & L2TP_AVP_HDR_MBIT) {
					l2tp_tunnel_log(tunnel, L2TP_AVP, LOG_ERR, "AVP: tunl %hu: unsupported mandatory vendor AVP, type=%hu, vendor=%hu",
							l2tp_tunnel_id(tunnel), avp_type, avp->vendor_id);
					l2tp_stats.unsupported_messages++;
					l2tp_stats.messages[msg_type].rx_bad++;
					goto unsupported_message;
				}
			}

			/* keep track of the AVPs we've seen. Only care about standard ones in the RFC */
			if (avp->vendor_id == 0) {
				avp_parse_flags |= (1ULL << avp_type);
			}

		skip_avp:
			avp = ((void *) avp) + avp_len;
			len -= avp_len;
		}
		avp = NULL;
	} else {
		l2tp_tunnel_log(tunnel, L2TP_AVP, LOG_ERR, "AVP: tunl %hu: bad message type %hu received", l2tp_tunnel_id(tunnel), msg_type);
		l2tp_stats.illegal_messages++;
		goto bad_message;
	}

	/* verify that all required AVPs are present for this message */
	if ((info->mandatory & avp_parse_flags) != info->mandatory) {
		l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_ERR,
				"PROTO: tunl %hu: Required AVPs missing for %s: parse_flags=%llx",
				l2tp_tunnel_id(tunnel), info->name, avp_parse_flags);
		l2tp_stats.illegal_messages++;
		l2tp_stats.messages[msg_type].rx_bad++;
		goto illegal_message;
	}

	/* check for optional AVPs that shouldn't be present for this message */
	avp_parse_flags &= ~info->mandatory;
	if ((~info->optional & avp_parse_flags) != 0) {
		l2tp_tunnel_log(tunnel, L2TP_PROTOCOL, LOG_ERR,
				"PROTO: tunl %hu: Optional AVPs ignored for message %s: parse_flags=%llx",
				l2tp_tunnel_id(tunnel), info->name, avp_parse_flags);
	}

	return 0;

illegal_avp:
	l2tp_tunnel_protocol_error(tunnel, L2TP_AVP_RESULT_STOPCCN_GENERAL_ERROR,
				   "illegal AVP");
	goto out;

illegal_message:
	l2tp_tunnel_protocol_error(tunnel, L2TP_AVP_RESULT_STOPCCN_GENERAL_ERROR,
				   "required AVP missing");
	goto out;

bad_message:
	l2tp_tunnel_protocol_error(tunnel, L2TP_AVP_RESULT_STOPCCN_GENERAL_ERROR,
				   "bad message type");
	goto out;

unhide_error:
	l2tp_tunnel_protocol_error(tunnel, L2TP_AVP_RESULT_STOPCCN_GENERAL_ERROR,
				   "AVP unhide error");
	goto out;

unsupported_message:
	l2tp_tunnel_protocol_error(tunnel, L2TP_AVP_RESULT_STOPCCN_GENERAL_ERROR,
				   "unsupported mandatory vendor AVP");
	goto out;

out:
	l2tp_tunnel_log(tunnel, L2TP_AVP, LOG_ERR, "AVP: tunl %hu: bad message received", l2tp_tunnel_id(tunnel));
	if (avp != NULL) {
		l2tp_avp_log_avp(avp, tunnel);
	}

	return -EPROTO;
}

/* Encode a message into an iov.
 * This route allocates an iov[] array with enough elements for one iov buffer per AVP.
 * iov[0] is reserved for the l2tp header. iov[0] must be initialized to zero here.
 */
int l2tp_avp_message_encode(uint16_t msg_type, struct l2tp_packet **ppkt, int hide, 
			    struct l2tp_avp_desc *avp_data, struct l2tp_tunnel *tunnel)
{
	int num_avps = 0;
	void *buffer = NULL;
	int buffer_len = 0;
	int avp_index = 1;			/* pktbuf[0] reserved for l2tp header */
	struct l2tp_msg_info const *msg_info;
	struct l2tp_packet *pkt = NULL;
	struct l2tp_avp_message_type type;
	int need_random_vector = hide;
	void *random_vector = NULL;
	int random_vector_len = 0;
	uint16_t avp_type;
	int result = 0;
	char *secret = NULL;
	int secret_len = 0;

	if (msg_type >= L2TP_MSG_INFO_COUNT) {
		result = -EBADMSG;
		goto out;
	}
	msg_info = &l2tp_msg_info[msg_type];

	if (need_random_vector) {
		/* For now, always use a fixed length random vector */
		random_vector_len = 32;
		random_vector = malloc(random_vector_len);
		if (random_vector == NULL) {
			result = -ENOMEM;
			goto out;
		}
		l2tp_make_random_vector(random_vector, random_vector_len);
		avp_data[TYPE(RANDOM_VECTOR)].value = random_vector;
		avp_data[TYPE(RANDOM_VECTOR)].value_len = random_vector_len;
	}

	/* Error is AVP hiding is enabled but no secret is available */
	if (hide) {
		l2tp_tunnel_get_secret(tunnel, &secret, &secret_len);
		if (secret == NULL) {
			result = -EPERM;
			goto err;
		}
	}

	/* We always have a MESSAGE_TYPE AVP */
	type.type = msg_type;
	avp_data[L2TP_AVP_TYPE_MESSAGE].value = (void *) &type;
	avp_data[L2TP_AVP_TYPE_MESSAGE].value_len = sizeof(type);

	/* Derive how many AVPs are being added to size the pktbuf array.
	 * We put each AVP into a separate pktbuf, and reserve the first 
	 * pktbuf for the L2TP header.
	 */
	num_avps = 1 + l2tp_avp_count_avps(avp_data);

	/* Allocate a l2tp_packet */
	pkt = l2tp_pkt_alloc(num_avps);
	if (pkt == NULL) {
		result = -ENOMEM;
		goto err;
	}

	l2tp_tunnel_log(tunnel, L2TP_AVP, LOG_DEBUG, "AVP: tunl %hu: building %s message, %d AVPs", 
			l2tp_tunnel_id(tunnel), msg_info->name, num_avps - 1);

	/* Encode each AVP in turn. If avp_data ptr is non NULL, an AVP is
	 * required to carry the data. Note that avp_data->len could be 0
	 * for AVPs that are header only.
	 *
	 * Note that the MESSAGE_TYPE AVP must always be first, and the
	 * RANDOM_VECTOR AVP must always come before the first hidden AVP.
	 * Otherwise, the AVPs may be encoded in any order.
	 * Fortunately, MESSAGE_TYPE is AVP 0 (zero) but RANDOM_VECTOR
	 * does not have a value smaller than the lowest hidable AVP.
	 * So we must special case for it in the loop below.
	 */
	for (avp_type = 0; avp_type < L2TP_AVP_TYPE_COUNT; avp_type++) {
		if (avp_data[avp_type].value != NULL) {

#ifdef L2TP_CISCO_SCCRQ_BUG_WORKAROUND
			/* Cisco can't handle SCCRQ with hidden AVPs... */
			if (msg_type == L2TP_AVP_MSG_SCCRQ) {
				hide = 0;
			}
#endif /* L2TP_CISCO_SCCRQ_BUG_WORKAROUND */

			buffer_len = L2TP_AVP_HEADER_LEN + avp_data[avp_type].value_len;
			buffer = malloc(buffer_len);
			if (buffer == NULL) {
				result = -ENOMEM;
				goto err;
			}
			L2TP_DEBUG(L2TP_DATA, "%s: allocated %d bytes @ %p for AVP %d", __func__,
				   buffer_len, buffer, avp_type);

			memset(buffer, 0, buffer_len);
			result = (*l2tp_avp_info[avp_type].encode)(buffer, 
								   buffer_len, 
								   &avp_data[avp_type], 
								   msg_info,
								   tunnel, hide);
			if (result < 0) {	
				goto err;
			}

			if (hide && l2tp_avp_info[avp_type].hide) {
				result = l2tp_avp_hide(&buffer, &buffer_len, 
						       (unsigned char *const) secret, secret_len,
						       random_vector, random_vector_len);
				if (result < 0) {	
					goto err;
				}
			}

			L2TP_DEBUG(L2TP_AVPDATA, "%s: avp=%hu", __func__, avp_type);

			pkt->iov[avp_index].iov_base = buffer;
			pkt->iov[avp_index].iov_len = buffer_len;
			pkt->total_len += buffer_len;
			pkt->avp_len += buffer_len;
			avp_index++;

			/* Special case for RANDOM_VECTOR which is never hidden and must come after the
			 * MESSAGE_TYPE AVP (possibly added above if this is the first time around the loop).
			 */
			if (need_random_vector) {
				need_random_vector = 0;
				buffer_len = L2TP_AVP_HEADER_LEN + avp_data[TYPE(RANDOM_VECTOR)].value_len;
				buffer = malloc(buffer_len);
				if (buffer == NULL) {
					result = -ENOMEM;
					goto err;
				}

				L2TP_DEBUG(L2TP_DATA, "%s: allocated %d bytes @ %p for AVP %d", __func__,
					   buffer_len, buffer, TYPE(RANDOM_VECTOR));

				memset(buffer, 0, buffer_len);
				result = (*l2tp_avp_info[TYPE(RANDOM_VECTOR)].encode)(buffer, 
										      buffer_len,
										      &avp_data[TYPE(RANDOM_VECTOR)], 
										      msg_info,
										      tunnel, 0);
				if (result < 0) {
					goto err;
				}

				L2TP_DEBUG(L2TP_AVPDATA, "%s: avp=%hu", __func__, TYPE(RANDOM_VECTOR));

				pkt->iov[avp_index].iov_base = buffer;
				pkt->iov[avp_index].iov_len = buffer_len;
				pkt->total_len += buffer_len;
				pkt->avp_len += buffer_len;
				avp_index++;

				/* Don't let the main loop add the RANDOM_VECTOR AVP again... */
				avp_data[TYPE(RANDOM_VECTOR)].value = NULL;
				avp_data[TYPE(RANDOM_VECTOR)].value_len = 0;
			}
		}
	}

	*ppkt = pkt;

	if (random_vector != NULL) {
		free(random_vector);
	}

out:
	return result;

err:
	if (buffer != NULL) {
		free(buffer);
	}
	if (random_vector != NULL) {
		free(random_vector);
	}
	if (pkt != NULL) {
		l2tp_pkt_free(pkt);
	}

	goto out;
}

/* Called when handling received messages to tunnel_id zero. We need to get the hostname
 * and message_type from the message before new contexts can be created to handle the
 * message fully.
 */
int l2tp_avp_preparse(struct l2tp_avp_hdr *first_avp, int avp_data_len, char **host_name, uint16_t *msg_type)
{
	struct l2tp_avp_message_type *msg_type_avp = NULL;
	struct l2tp_avp_host_name *host_name_avp = NULL;
	int len = avp_data_len;
	int avp_len;
	struct l2tp_avp_hdr *avp = first_avp;
	uint16_t avp_type;
	int result = -EBADMSG;

	*host_name = NULL;
	*msg_type = 0;

	L2TP_DEBUG(L2TP_AVPDATA, "%s: len=%d", __func__, avp_data_len);
	while (len > 0) {
		avp_type = ntohs(avp->type);
		avp_len = L2TP_AVP_HDR_LEN(ntohs(avp->flag_len));
		if ((avp_len == 0) && (avp_type == 0)) {
			break;
		}
		if ((avp_len >= 1024) || (avp_len < L2TP_AVP_HEADER_LEN) || (avp_len > len)) {
			break;
		}

		if (l2tp_opt_trace_flags & L2TP_AVPDATA) {
			l2tp_log(LOG_DEBUG, "AVPDATA: type=%hd len=%d of %d", avp_type, avp_len, avp_data_len);
		}

		switch (avp_type) {
		case L2TP_AVP_TYPE_MESSAGE:
			msg_type_avp = (void *) &avp->value[0];
			*msg_type = ntohs(msg_type_avp->type);
			break;
		case L2TP_AVP_TYPE_HOST_NAME:
			host_name_avp = (void *) &avp->value[0];
			*host_name = strndup(&host_name_avp->string[0], avp_len - L2TP_AVP_HEADER_LEN);
			if (*host_name == NULL) {
				l2tp_stats.no_control_frame_resources++;
				result = -ENOMEM;
				goto out;
			}
			if (l2tp_opt_trace_flags & L2TP_AVPDATA) {
				l2tp_log(LOG_DEBUG, "AVPDATA: host_name=%s", *host_name);
			}
			break;
		default:
			break;
		}

		if ((msg_type_avp != NULL) && (host_name_avp!= NULL)) {
			result = 0;
			goto out;
		}

		avp = ((void *) avp) + avp_len;
		len -= avp_len;
	}

out:
	return result;
}

static void l2tp_avp_log_avp(struct l2tp_avp_hdr *avp, struct l2tp_tunnel const *tunnel)
{
	if (l2tp_tunnel_get_trace_flags(tunnel) & L2TP_DATA) {
		uint16_t flag_len = ntohs(avp->flag_len);
		l2tp_tunnel_log(tunnel, L2TP_DATA, LOG_DEBUG, "DATA: tunl %hu: flags=%s%s, vendor=%hu, type=%hu",
				l2tp_tunnel_id(tunnel), 
				(flag_len & L2TP_AVP_HDR_MBIT) ? "M" : "",
				(flag_len & L2TP_AVP_HDR_HBIT) ? "H" : "",
				ntohs(avp->vendor_id),
				ntohs(avp->type));
		l2tp_mem_dump(LOG_DEBUG, avp, L2TP_AVP_HDR_LEN(flag_len), 1);
	}
}

static int l2tp_avp_count_avps(struct l2tp_avp_desc *avp_data)
{
	int index;
	int count = 0;

	for (index = 0; index < L2TP_AVP_TYPE_COUNT; index++, avp_data++) {
		if (avp_data->value != NULL) {
			count++;
		}
	}
	return count;
}

/*****************************************************************************
 * Init and cleanup
 *****************************************************************************/

void l2tp_avp_init(void)
{
	char host_name[_SC_HOST_NAME_MAX];
	int result;

	l2tp_avp_my_firmware_revision.value = l2tp_firmware_revision;

#ifdef L2TP_TEST
	/* For testing, use a fixed vendor_name string, or test scripts will fail */
	l2tp_avp_my_vendor_name = (void *) strdup(L2TP_APP_VENDOR_INFO " Linux");
	l2tp_avp_my_vendor_name_len = strlen(l2tp_avp_my_vendor_name->string);
#else
	/* Vendor string includes OS version and cpu type: <vendor>, linux-<version> (<cpu>) */
	l2tp_avp_my_vendor_name_len = strlen(L2TP_VENDOR_INFO) + 1 + strlen("Linux-") + strlen(l2tp_kernel_version) + 2 + strlen(l2tp_cpu_name) + 1;
	l2tp_avp_my_vendor_name = calloc(1, l2tp_avp_my_vendor_name_len + 1);
	if (l2tp_avp_my_vendor_name == NULL) {
		l2tp_log(LOG_ERR, "AVP init: out of memory");
		exit(1);
	}
	sprintf(&l2tp_avp_my_vendor_name->string[0], "%s Linux-%s (%s)", L2TP_VENDOR_INFO, l2tp_kernel_version, l2tp_cpu_name);
#endif /* L2TP_TEST */

	result = gethostname(&host_name[0], sizeof(host_name) - 1);
	if (result < 0) {
		l2tp_log(LOG_ERR, "AVP init: failed to get hostname");
		exit(1);		
	}
	l2tp_avp_my_host_name_len = strlen(host_name);
	l2tp_avp_my_host_name = calloc(1, l2tp_avp_my_host_name_len + 1);
	if (l2tp_avp_my_host_name == NULL) {
		l2tp_log(LOG_ERR, "AVP init: out of memory");
		exit(1);
	}
	memcpy(&l2tp_avp_my_host_name->string[0], host_name, l2tp_avp_my_host_name_len);

	l2tp_avp_my_protocol_version.ver = L2TP_AVP_VERSION_RFC2661;
	l2tp_avp_my_protocol_version.rev = 0;
}

void l2tp_avp_cleanup(void)
{
	free(l2tp_avp_my_vendor_name);
	free(l2tp_avp_my_host_name);
}
