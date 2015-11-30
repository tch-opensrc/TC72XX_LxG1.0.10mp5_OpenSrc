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
 * All data passing through an L2TP tunnel goes through a dedicated
 * socket. Each tunnel has one socket. 
 *
 * Userspace l2tpd receives only control packets on its UDP sockets
 * and should send only control packets. Control frames are passed
 * through a reliable transport (implemented in this module) which
 * retransmits frames until they are acknowledged or time out..
 */

#include "usl.h"
#include "l2tp_private.h"

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

/*****************************************************************************
 * Transport contexts
 *****************************************************************************/

struct l2tp_xprt_params {
	uint16_t			max_retries;
	uint16_t			rx_window_size;		/* max length of receive pipe */
	uint16_t			tx_window_size;		/* max unack'd transmits */
	uint16_t			retry_timeout;
	uint16_t			hello_timeout;
	struct in_addr			our_addr;
	struct in_addr			peer_addr;
};

struct l2tp_xprt_stats {
	uint32_t			retransmits;
	uint32_t			oos_control_packets;
	uint32_t			oos_control_discards;
	uint32_t			tx_zlbs;
	uint32_t			tx_zlb_fails;
	uint32_t			rx_zlbs;
	uint32_t			data_pkt_discards;
	uint32_t			duplicate_pkt_discards;
	uint32_t			rx_hellos;
	uint32_t			tx_hellos;
	uint32_t			tx_hello_fails;
	uint64_t			rx_packets;
	uint64_t			rx_bytes;
	uint64_t			tx_packets;
	uint64_t			tx_bytes;
};

struct l2tp_xprt {
	uint16_t			tunnel_id;
	uint16_t			peer_tunnel_id;
	uint16_t			ns; 			/* next ns to use for transmit control packets */
	uint16_t			nr; 			/* nr expected to be received next (control) */
	uint16_t			peer_ns;
	uint16_t			peer_nr;
	int				fd;
	int				kernel_fd;
	struct l2tp_xprt_params 	params;
	struct l2tp_xprt_stats		stats;
	void 				*retry_timer;
	void				*hello_timer;
	void				*zlb_timer;
	int				retry_duration;		/* exponentially increases */
	uint16_t			cwnd; 			/* slow start congestion window */
	uint16_t			ssthresh; 		/* slow start threshold */
	uint16_t			congpkt_acc;		/* congestion packet accumulator */
	uint16_t			is_congested:1;		/* 0=>slow start, 1=>congestion avoidance */
	uint16_t			is_lac:1;		/* are we the LAC? */
	uint16_t			is_closing:1;
	uint16_t			is_tx_stalled:1;
	uint16_t			has_acked:1;
	struct usl_list_head		ackq;	 		/* queue of transmitted pkts waiting acks */
	struct usl_list_head		txq; 			/* queue of pkts waiting transmit slot */
	struct usl_list_head		rxq; 			/* in-sequence queue of received pkts waiting to go up to protocol */
	uint16_t			nr_next;		/* nr to deliver next */
	uint16_t			nr_last_dequeued;	/* nr last delivered */
	int				is_reordering;		/* are pkts waiting in rxq? */
	struct usl_list_head		list; 			/* the list we're on */
	void				*tunnel;
	struct l2tp_peer		*peer;
	unsigned long			data_rx_packets;	/* to check if data packets rx'd since last hello */
};

/* Local tick count for low resolution packet resequencing timers */
static unsigned int l2tp_xprt_jiffies;

#ifdef DEBUG
/* To check for seq number wrap test cases */
static int l2tp_xprt_nr_wraps = 0;
static int l2tp_xprt_ns_wraps = 0;
#endif

static void l2tp_xprt_retry_timeout(void *data);
static void l2tp_xprt_zlb_ack_send(struct l2tp_xprt *xprt, uint16_t nr);
static void l2tp_xprt_hello_timeout(void *data);
static void l2tp_xprt_send_hello(struct l2tp_xprt *xprt);
static void l2tp_xprt_process_txq(struct l2tp_xprt *xprt);
static void l2tp_xprt_zlb_timeout(void *data);
static int l2tp_xprt_sendmsg(struct l2tp_xprt *xprt, struct l2tp_packet *pkt);

static USL_LIST_HEAD(l2tp_xprt_list); 		/* active tunnels */
static USL_LIST_HEAD(l2tp_xprt_delete_list); 	/* deleting tunnels */

/*****************************************************************************
 * Implements the slow start algorithm, defined in RFC2661 Appendix A.
 *****************************************************************************/

static int l2tp_xprt_is_tx_window_open(struct l2tp_xprt *xprt, uint16_t ns)
{
	uint16_t tws;		/* current transmit window used */

	if (ns >= xprt->peer_nr) {
		tws = ns - xprt->peer_nr;
	} else {
		tws = (0xffff - xprt->peer_nr) + ns;
	}

	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: ns=%hu, ns/nr=%hu/%hu peer=%hu/%hu tws=%hu", __func__, 
		    xprt->tunnel_id, ns, xprt->ns, xprt->nr, xprt->peer_ns, xprt->peer_nr, tws);

	if (xprt->is_congested == 0) {
		/* slow start phase */
		if (tws < xprt->cwnd) {
			return 1;
		}
	} else {
		/* congested phase */
		if (tws < xprt->params.tx_window_size) {
			return 1;
		}
	}
	l2tp_tunnel_log(xprt->tunnel, L2TP_XPRT, LOG_DEBUG, "XPRT: tunl %hu: tx window closed", xprt->tunnel_id);
	return 0;
}

static void l2tp_xprt_tx_window_ack(struct l2tp_xprt *xprt)
{
	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu", __func__, xprt->tunnel_id);

	/* Since this is called for all packets ack'd, including ZLBs,
	 * we do not update ns/nr here.
	 */
	if (xprt->is_congested == 0) {
		/* slow start phase */
		L2TP_DEBUG(L2TP_XPRT, "%s: tid=%d slow start: cwnd=%hu ssthresh=%hu acc=%hu", __func__,
			   xprt->tunnel_id, xprt->cwnd, xprt->ssthresh, xprt->congpkt_acc);
		xprt->cwnd++;
		if (xprt->cwnd >= xprt->ssthresh) {
			xprt->congpkt_acc = 0;
			xprt->is_congested = 1;
		}
	} else {
		/* congested phase */
		L2TP_DEBUG(L2TP_XPRT, "%s: tid=%d congested: cwnd=%hu acc=%hu", __func__,
			   xprt->tunnel_id, xprt->cwnd, xprt->congpkt_acc);
		xprt->congpkt_acc++;
		if (xprt->congpkt_acc >= xprt->cwnd) {
			xprt->cwnd++;
			if (xprt->cwnd > xprt->params.tx_window_size) {
				xprt->cwnd = xprt->params.tx_window_size;
			}
			xprt->congpkt_acc = 0;
		}
			
	}
}

static void l2tp_xprt_tx_window_retransmit_ind(struct l2tp_xprt *xprt)
{
	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: cwnd=%hu ssthresh=%hu", __func__,
		   xprt->tunnel_id, xprt->cwnd, xprt->ssthresh);

	if (xprt->ssthresh < 2) {
		xprt->ssthresh = 1;
	} else {
		xprt->ssthresh = xprt->cwnd / 2;
	}
	xprt->cwnd = 1;
	xprt->is_congested = 0; /* re-enter slow-start phase */
}

/* Determine whether the sequence number nr is "less than or equal" as defined
 * by RFC2661, section 5.8. Return 1 if less than or equal, else 0.
 */
static inline int l2tp_xprt_is_less_than_equal(struct l2tp_xprt *xprt, uint16_t nr1, uint16_t nr2)
{
	uint16_t low = nr1;
	uint16_t mid = nr2;
	uint16_t high = nr1 + 0x8000;

	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: n1=%hu n2=%hu", __func__, 
		   l2tp_tunnel_id(xprt->tunnel), nr1, nr2);
	if (low < high) {
		if ((low <= mid) && (mid < high)) {
			return 1;
		} else {
			return 0;
		}
	} else if ((low <= mid) || (mid < high)) {
		return 1;
	}
	return 0;
}

/******************************************************************************
 * In-sequence delivery to protocol.
 * If packets are received out-of-sequence, they are held in xprt->rxq
 * for resequencing.
 *****************************************************************************/

static void l2tp_xprt_rxq_dequeue(struct l2tp_xprt *xprt, struct l2tp_packet *pkt)
{
	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: pkt %hu, nr_next=%hu", __func__, xprt->tunnel_id, pkt->ns, xprt->nr_next);

	usl_list_del_init(&pkt->list);
	if (l2tp_xprt_is_less_than_equal(xprt, xprt->nr_last_dequeued + 1, pkt->ns)) {
		xprt->nr_last_dequeued = pkt->ns;
		L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: update nr_last_dequeued to %hu", __func__, xprt->tunnel_id, xprt->nr_last_dequeued);
	}
	if (l2tp_xprt_is_less_than_equal(xprt, xprt->nr_next, pkt->ns)) {
		L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: update nr_next from %hu to %hu", __func__, xprt->tunnel_id, xprt->nr_next, pkt->ns + 1);
		xprt->nr_next = pkt->ns + 1;
	}
}

static void l2tp_xprt_deliver_up(struct l2tp_xprt *xprt, struct l2tp_packet *pkt)
{
	struct sockaddr_in const *peer_addr;

	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu", __func__, xprt->tunnel_id);

	if (pkt->xprt != xprt) {
		L2TP_DEBUG(L2TP_XPRT, "tunl %hu: tunnel and packet inconsistent: %p/%p",
			   xprt->tunnel_id, pkt->xprt, xprt);
		goto out;
	}

	peer_addr = l2tp_tunnel_get_peer_addr(xprt->tunnel);
	l2tp_tunnel_log(xprt->tunnel, L2TP_DATA, LOG_DEBUG, "DATA: RX: tunl %hu/%hu: rcv %d bytes from peer %s, packet ns/nr %hu/%hu type %d",
			xprt->tunnel_id, pkt->session_id, pkt->total_len, inet_ntoa(peer_addr->sin_addr), pkt->ns, pkt->nr, pkt->msg_type);

	/* Tunnel nr state is updated when packets are delivered in-sequence to protocol. */
	l2tp_tunnel_log(xprt->tunnel, L2TP_XPRT, LOG_DEBUG, "XPRT: tunl %hu: update nr from %hu to %hu",
			xprt->tunnel_id, xprt->nr, pkt->ns + 1);
	xprt->nr = pkt->ns + 1;

#ifdef DEBUG
	if (xprt->nr == 0) {
		l2tp_xprt_nr_wraps++;
	}
#endif /* DEBUG */

	l2tp_tunnel_recv(xprt->tunnel, xprt->tunnel_id, pkt->session_id, pkt);

out:
	l2tp_pkt_free(pkt);
}

/* Deliver pkt in order of nr. The packet is first queued in rxq,
 * which ensures in-order delivery of management frames to the
 * protocol as required by RFC2661, section 5.8.
 */
static void l2tp_xprt_deliver_to_protocol(struct l2tp_xprt *xprt, struct l2tp_packet *pkt)
{
	struct usl_list_head *walk;
	struct usl_list_head *tmp;
	struct l2tp_packet *q_pkt;

	pkt->expires_at = l2tp_xprt_jiffies + USL_TIMER_HZ - 1;	/* hold for approx 1 second */

	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: ns=%hu, nr_next=%hu, expires=%u", __func__, xprt->tunnel_id, pkt->ns, xprt->nr_next, pkt->expires_at);

	usl_list_for_each(walk, tmp, &xprt->rxq) {
		q_pkt = usl_list_entry(walk, struct l2tp_packet, list);
		if (q_pkt->ns > pkt->ns) {
			L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: pkt %hu, inserted before %hu", __func__, xprt->tunnel_id, pkt->ns, q_pkt->ns);
			xprt->stats.oos_control_packets++;
			usl_list_add(&pkt->list, q_pkt->list.prev);
			goto dequeue;
		}
	}
	usl_list_add(&pkt->list, &xprt->rxq);

dequeue:
	/* If the pkt at the head of the queue has the nr that we
	 * expect to send up next, dequeue it and any other 
	 * in-sequence packets behind it.
	 */
	usl_list_for_each(walk, tmp, &xprt->rxq) {
		q_pkt = usl_list_entry(walk, struct l2tp_packet, list);
		L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: looking at pkt %hu", __func__, xprt->tunnel_id, q_pkt->ns);
		if (q_pkt->ns == xprt->nr_next) {
			l2tp_xprt_rxq_dequeue(xprt, q_pkt);
			l2tp_xprt_deliver_up(xprt, q_pkt);
		} else {
			l2tp_tunnel_log(xprt->tunnel, L2TP_XPRT, LOG_DEBUG, "tunl %hu: holding out-of-sequence pkt %hu len=%d/%d, waiting for %hu, last_dequeud=%hu", 
					xprt->tunnel_id, q_pkt->ns, q_pkt->total_len, q_pkt->avp_len, xprt->nr_next, xprt->nr_last_dequeued);
			break;
		}
	}

	/* if there are packets waiting, enable reorder poll */
	if (!usl_list_empty(&xprt->rxq)) {
		xprt->is_reordering = 1;
	} else {
		xprt->is_reordering = 0;
	}
}

/* Come here on every timer tick if the transport's is_reordering flag
 * is set, i.e. packets are waiting in rxq for out-of-sequence
 * packets to release them.
 */
static void l2tp_xprt_rxq_dequeue_poll(struct l2tp_xprt *xprt)
{
	struct usl_list_head *walk;
	struct usl_list_head *tmp;
	struct l2tp_packet *q_pkt;

	/* dequeue packets that are in sequence */
	usl_list_for_each(walk, tmp, &xprt->rxq) {
		q_pkt = usl_list_entry(walk, struct l2tp_packet, list);
		if (q_pkt->ns == xprt->nr_next) {
			l2tp_xprt_rxq_dequeue(xprt, q_pkt);
			l2tp_xprt_deliver_up(xprt, q_pkt);
		} else {
			L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: looking at pkt %hu, nr_next=%hu", __func__, xprt->tunnel_id, q_pkt->ns, xprt->nr_next);
			break;
		}
	}

	/* check for expired packets */
	usl_list_for_each(walk, tmp, &xprt->rxq) {
		q_pkt = usl_list_entry(walk, struct l2tp_packet, list);
		if (q_pkt->expires_at < l2tp_xprt_jiffies) {
			l2tp_tunnel_log(xprt->tunnel, L2TP_XPRT, LOG_DEBUG, "tunl %hu: out-of-sequence pkt %hu dropped, expecting %hu", xprt->tunnel_id, q_pkt->ns, xprt->nr_next);
			l2tp_xprt_rxq_dequeue(xprt, q_pkt);
			l2tp_pkt_free(q_pkt);
			xprt->stats.oos_control_discards++;
		}
	}

	/* if reorder queue now empty, update state */
	if (usl_list_empty(&xprt->rxq)) {
		L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: queue empty when nr_next=%hu", __func__, xprt->tunnel_id, xprt->nr_next);
		if (l2tp_xprt_is_less_than_equal(xprt, xprt->nr_next, xprt->nr_last_dequeued)) {
			L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: update nr_next from %hu to %hu", __func__, xprt->tunnel_id, xprt->nr_next, xprt->nr_last_dequeued + 1);
			xprt->nr_next = xprt->nr_last_dequeued + 1;
		}
		xprt->is_reordering = 0;
	}
}

/*****************************************************************************
 * Receive handling
 *****************************************************************************/

/* All packets with ns <= nr have been acked. Remove them from the
 * ack-pending list.
 */
static void l2tp_xprt_flush_ack_list(struct l2tp_xprt *xprt, uint16_t nr)
{
	struct l2tp_packet *pkt;

	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: nr=%hu", __func__, xprt->tunnel_id, nr);

	/* Walk the ack queue, flushing all entries with nr "less than or equal to"
	 * the supplied nr.
	 */
	for (;;) {
		pkt = l2tp_pkt_peek(&xprt->ackq);
		if (pkt == NULL) {
			break;
		}
		L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: looking at pkt with ns/nr=%hu/%hu", __func__,
			   xprt->tunnel_id, pkt->ns, pkt->nr);
		if (l2tp_xprt_is_less_than_equal(xprt, pkt->ns + 1, nr)) {
			l2tp_tunnel_log(xprt->tunnel, L2TP_XPRT, LOG_DEBUG, "XPRT: tunl %hu: pkt %hu/%hu is acked by nr %hu",
					xprt->tunnel_id, pkt->ns, pkt->nr, nr);
			xprt->has_acked = 1;
			l2tp_pkt_unlink(pkt);
			l2tp_pkt_free(pkt);

			/* RFC2661: indicate ack to slow start algorithm */
			l2tp_xprt_tx_window_ack(xprt);
		} else {
			break;
		}
	}
}

/* We come here when a valid HELLO message is received on a tunnel
 * See l2tp_tunnel_recv().
 */
void l2tp_xprt_hello_rcvd(struct l2tp_xprt *xprt)
{
	xprt->stats.rx_hellos++;
}

/* Process received frames. We don't expect data frames here. 
 * We allocate an l2tp_packet data structure for the packet
 * and pass it up.
 */
int l2tp_xprt_recv(struct l2tp_xprt *xprt, struct l2tp_packet *pkt)
{
	if (xprt == NULL) {
		goto drop;
	}

	L2TP_DEBUG(L2TP_XPRT, "%s: %hu: pktlen=%d", __func__, l2tp_tunnel_id(xprt->tunnel), pkt->total_len);

	pkt->xprt = xprt;
	xprt->stats.rx_packets++;
	xprt->stats.rx_bytes += pkt->total_len;

	l2tp_tunnel_log(xprt->tunnel, L2TP_XPRT, LOG_DEBUG, "XPRT: RX: tunl %hu/%hu: len=%d ns/nr=%hu/%hu, our ns/nr=%hu/%hu, peer ns/nr=%hu/%hu", 
			xprt->tunnel_id, pkt->session_id, pkt->total_len, pkt->ns, pkt->nr, xprt->ns, xprt->nr, xprt->peer_ns, xprt->peer_nr);

	if (pkt->avp_len > 0) {

		/* Check ns is not "less than or equal to" the last received nr.
		 * If so, we treat the packet as a duplicate. See RFC2661, section 5.8.
		 */
		if (l2tp_xprt_is_less_than_equal(xprt, pkt->ns + 1, xprt->nr)) {
			l2tp_tunnel_log(xprt->tunnel, L2TP_XPRT, LOG_DEBUG, 
					"XPRT: tunl %hu: duplicate packet ns/nr=%hu/%hu, our ns/nr=%hu/%hu",
					xprt->tunnel_id, pkt->ns, pkt->nr, xprt->ns, xprt->nr);
			l2tp_xprt_zlb_ack_send(xprt, pkt->ns + 1);
			xprt->stats.duplicate_pkt_discards++;
			goto drop;
		}
	} else {
		l2tp_tunnel_log(xprt->tunnel, L2TP_XPRT, LOG_DEBUG, "XPRT: tunl %hu: zlb ack received: ns/nr=%hu/%hu",
				xprt->tunnel_id, pkt->ns, pkt->nr);
		xprt->stats.rx_zlbs++;
	}

	/* Take care not to move the peer ns/nr backwards */
	if (l2tp_xprt_is_less_than_equal(xprt, xprt->peer_ns, pkt->ns)) {
		xprt->peer_ns = pkt->ns;
	}
	if (l2tp_xprt_is_less_than_equal(xprt, xprt->peer_nr, pkt->nr)) {
		xprt->peer_nr = pkt->nr;
	}

	l2tp_tunnel_log(xprt->tunnel, L2TP_XPRT, LOG_DEBUG, 
			"XPRT: tunl %hu: peer ns/nr is %hu/%hu", xprt->tunnel_id, xprt->peer_ns, xprt->peer_nr);

	/* Restart the hello timer. Since Hello messages are used to
	 * query whether the peer is still operational, there is no
	 * need to send Hellos if we are successfully receiving frames
	 * from him. So we restart the hello timer when a good (seq
	 * numbers ok) control frame is received. Thus, this timer
	 * expires when the control channel is idle.
	 */
	if ((!xprt->is_closing) && (xprt->hello_timer != NULL)) {
		usl_timer_restart(xprt->hello_timer);
	}

	/* RFC2661: nr is used to acknowledge messages from the peer. We can flush 
	 * all entries in our ack-pending queue that have ns <= nr.
	 */
	l2tp_xprt_flush_ack_list(xprt, xprt->peer_nr);

	/* Queue the packet on the tunnel's receive queue to enforce
	 * in-order delivery to userspace. If the nr isn't in sequence,
	 * packets wait in the queue until all preceding nr's have
	 * been received, at which point they are pulled from the
	 * queue and delivered upwards. Normal case for in-order
	 * delivery is that the packet is immediately pulled from the
	 * queue and delivered upwards.
	 *
	 * The tunnel xprt->nr is only updated when packets are delivered
	 * in-sequence to the upper protocol.
	 */
	if (pkt->avp_len > 0) {
		l2tp_xprt_deliver_to_protocol(xprt, pkt);

		/* If the txq is empty, it means we don't have any packets queued to
		 * the peer. Since we're here because we've received a packet from our peer,
		 * start the ZLB timer which will send a ZLB ack if/when it expires.
		 */
		if (l2tp_pkt_queue_empty(&xprt->txq) && (!usl_timer_is_running(xprt->zlb_timer))) {
			L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: start zlb timer", __func__, xprt->tunnel_id);
			usl_timer_restart(xprt->zlb_timer);
		}
	} else {
		l2tp_pkt_free(pkt);
	}

out:
	/* We received a packet so now try to make progress on our txq */
	l2tp_xprt_process_txq(xprt);

	return 0;

drop:
	l2tp_pkt_free(pkt);
	goto out;
}

/*****************************************************************************
 * Timers
 *****************************************************************************/

static void l2tp_xprt_retry_timeout(void *data)
{
	struct l2tp_xprt *xprt = data;
	struct l2tp_packet *pkt;
	struct usl_list_head *walk;
	struct usl_list_head *tmp;
	int result;

	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: has_acked=%d period=%d, timer=%p", __func__, xprt->tunnel_id, 
		   xprt->has_acked ? 1 : 0, xprt->retry_duration, xprt->retry_timer);

	/* Stop the retry timer if both ackq & txq are empty. */
	pkt = l2tp_pkt_peek(&xprt->ackq);
	if (pkt == NULL) {
		L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: ackq empty", __func__, xprt->tunnel_id);
		if (l2tp_pkt_queue_empty(&xprt->txq)) {
			L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: txq empty", __func__, xprt->tunnel_id);
			usl_timer_stop(xprt->retry_timer);
		}
		if (xprt->retry_duration != xprt->params.retry_timeout) {
			xprt->retry_duration = xprt->params.retry_timeout;
			L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: reset retry interval", __func__, 
				   xprt->tunnel_id);
		}
		xprt->has_acked = 0;
		goto out;
	} else {
		L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: ackq has pkt ns/nr %hu/%hu", __func__,
			   xprt->tunnel_id, pkt->ns, pkt->nr);
	}

	/* Check if packets have been acked since the last timer
	 * expiry. If so, don't try to retransmit any from the ackq.
	 */
	if (xprt->has_acked) {
		xprt->has_acked = 0;
		if (xprt->retry_duration != xprt->params.retry_timeout) {
			xprt->retry_duration = xprt->params.retry_timeout;
			L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: reset retry interval", __func__, 
				   xprt->tunnel_id);
		}
		goto out;
	}

	/* Exponentially increase the retry timer until it reaches a maximum 8 seconds */
	if (xprt->retry_duration < 8) {
		xprt->retry_duration <<= 1;
		l2tp_tunnel_log(xprt->tunnel, L2TP_XPRT, LOG_DEBUG, "XPRT: tunl %hu: set retry interval to %d", 
				xprt->tunnel_id, xprt->retry_duration);
	}

	/* RFC2661: indicate retransmission to slow start algororithm */
	l2tp_xprt_tx_window_retransmit_ind(xprt);

	/* Retransmit all packets in the ack queue.
	 * Don't retransmit the packet if it has only just been put in the ackq.
	 */
	usl_list_for_each(walk, tmp, (struct usl_list_head *) &xprt->ackq) {
		pkt = usl_list_entry(walk, struct l2tp_packet, list);
		pkt->requeue_count++;
		if (pkt->requeue_count == 0) {
			break;
		}

		xprt->stats.retransmits++;
		l2tp_stats.total_retransmitted_control_frames++;

		if (pkt->requeue_count > xprt->params.max_retries) {
			goto too_many_retries;
		}

		pkt->nr = xprt->nr;
		l2tp_xprt_sendmsg(xprt, pkt);
	}

out:
	l2tp_xprt_process_txq(xprt);
	usl_timer_interval_set(xprt->retry_timer, USL_TIMER_TICKS(xprt->retry_duration));

	return;

too_many_retries:
	/* If we've reached the retry count, give up. Purge the ack-pending
	 * and tx-pending queues, stop the timer and move the tunnel into
	 * a delete-pending list.
	 */
	l2tp_tunnel_log(xprt->tunnel, L2TP_XPRT, LOG_DEBUG, 
			"XPRT: tunl %hu: retry failure", xprt->tunnel_id);
	usl_list_del_init(&xprt->list);
	usl_list_add_tail(&xprt->list, &l2tp_xprt_delete_list);

	xprt->retry_duration = xprt->params.retry_timeout;
	usl_timer_stop(xprt->retry_timer);

	/* tell that tunnel should be cleaned up */
	result = l2tp_tunnel_queue_event(xprt->tunnel_id, L2TP_CCE_EVENT_XPRT_DOWN);
	if (result < 0) {
		L2TP_DEBUG(L2TP_XPRT, "Failed to send internal event for tunnel %d",
			   xprt->tunnel_id);
		l2tp_stats.event_queue_full_errors++;
	}
}

/* Called when hello timer expires.
 * The timer is reset when a control message is sent or received so we should
 * only get here if the control channel is idle. We send a Hello only if our
 * transmit queue is empty and no data packets have been received on the tunnel
 * since last time.
 */
static void l2tp_xprt_hello_timeout(void *data)
{
	struct l2tp_api_tunnel_stats stats;
	struct l2tp_xprt *xprt = data;
	int txq_empty = l2tp_pkt_queue_empty(&xprt->txq);

	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: hello timeout, txq_empty=%d, timer=%p", __func__, 
		   xprt->tunnel_id, txq_empty, xprt->hello_timer);

	if ((xprt->params.hello_timeout != 0) && (txq_empty)) {
		if (l2tp_xprt_kernel_get(xprt, &stats) < 0) {
			stats.data_rx_packets = 0;
		}
		if ((stats.data_rx_packets == 0) || 
		    (stats.data_rx_packets == xprt->data_rx_packets)) {
			l2tp_xprt_send_hello(xprt);
		}
		xprt->data_rx_packets = stats.data_rx_packets;
	} else {
		L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: hello timeout, rx_pkts=%lu", __func__, 
			   xprt->tunnel_id, xprt->data_rx_packets);
	}
}

/* Timer adjust function for the hello timer. 
 * This is used to add some randomness to the periodic hello interval.
 */
static int l2tp_xprt_hello_timer_adjust(int start_interval, int current_interval)
{
	int adjust = 0;

	if (start_interval > 4) {
#if USL_TIMER_HZ != 4
#error Fix code here to match new USL_TIMER_HZ value
#endif
		adjust = l2tp_random(-3, 3);
		L2TP_DEBUG(L2TP_FUNC, "%s: adjust=%d interval=%d", __func__, adjust,
			   start_interval + adjust);
	}

	return start_interval + adjust;
}

/* The ZLB timer expires if we receive a message and we don't send a 
 * message back to the peer for a period.
 */
static void l2tp_xprt_zlb_timeout(void *data)
{
	struct l2tp_xprt *xprt = data;
	int txq_empty = l2tp_pkt_queue_empty(&xprt->txq);

	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %hu: ns/nr=%hu/%hu, txq empty=%d", __func__,
		   xprt->tunnel_id, xprt->ns, xprt->nr, txq_empty);

	if (txq_empty) {
		l2tp_xprt_zlb_ack_send(xprt, xprt->nr);
	}
}

/* Come here on every timer tick. We poll each transport regularly to try
 * to make progress on any stalled transmits.
 */
static void l2tp_xprt_tick(void)
{
	struct usl_list_head *tmp;
	struct usl_list_head *walk;
	struct l2tp_xprt *xprt;

	l2tp_xprt_jiffies++;

	usl_list_for_each(walk, tmp, &l2tp_xprt_list) {
		xprt = usl_list_entry(walk, struct l2tp_xprt, list);
		if (!xprt->is_tx_stalled) {
			l2tp_xprt_process_txq(xprt);
		}
		if (xprt->is_reordering) {
			l2tp_xprt_rxq_dequeue_poll(xprt);
		}
	}
}

/*****************************************************************************
 * Transmit code
 *****************************************************************************/

/* A ZLB is an L2TP control frame with 0 length (no AVPs).
 */
static void l2tp_xprt_zlb_ack_send(struct l2tp_xprt *xprt, uint16_t nr)
{
	struct l2tp_packet *pkt;
	void *l2tph;
	int l2tph_len;

	if (xprt->peer_tunnel_id == 0) {
		return;
	}

	pkt = l2tp_pkt_alloc(1);
	if (pkt == NULL) {
		goto nomem_pkt;
	}

	if (l2tp_net_build_header(&l2tph, &l2tph_len, xprt->ns, nr, xprt->peer_tunnel_id, 0) < 0) {
		goto nomem_lh;
	}

	l2tp_tunnel_log(xprt->tunnel, L2TP_XPRT, LOG_DEBUG, "XPRT: tunl %hu: send zlb ack, ns/nr=%hu/%hu",
			xprt->tunnel_id, xprt->ns, nr);

	pkt->iov[0].iov_base = l2tph;
	pkt->iov[0].iov_len = l2tph_len;
	pkt->total_len = l2tph_len;
	pkt->ns = xprt->ns;
	pkt->nr = nr;
	pkt->tunnel_id = xprt->tunnel_id;
	pkt->session_id = 0;

	(void) l2tp_xprt_sendmsg(xprt, pkt);
	l2tp_pkt_free(pkt);

	return;

nomem_lh:
	l2tp_pkt_free(pkt);
nomem_pkt:
	xprt->stats.tx_zlb_fails++;
}

/* A HELLO is sent to the peer after a period of inactivity as a tunnel keepalive.
 */
static void l2tp_xprt_send_hello(struct l2tp_xprt *xprt)
{
	int result;

	if (xprt->peer_tunnel_id == 0) {
		return;
	}

	result = l2tp_tunnel_send_hello(xprt->tunnel);
	if (result < 0) {
		xprt->stats.tx_hello_fails++;
	} else {
		xprt->stats.tx_hellos++;
	}
}

static int l2tp_xprt_sendmsg(struct l2tp_xprt *xprt, struct l2tp_packet *pkt)
{
	int result;
	struct msghdr msg;
	struct l2tp_peer *peer;
	struct l2tp_tunnel *tunnel;
	struct sockaddr_in const *peer_addr;

	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: fd=%d len=%d", __func__, xprt->tunnel_id, xprt->fd, pkt->total_len);

	/* If the ZLB timer is running, stop it now since we're about to send 
	 * a message which will include the ack.
	 */
	if (usl_timer_is_running(xprt->zlb_timer)) {
		L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: stop zlb timer", __func__, xprt->tunnel_id);
		usl_timer_stop(xprt->zlb_timer);
	}

#ifdef L2TP_TEST
	if (l2tp_test_is_fake_tx_drop(xprt->tunnel_id, pkt->session_id)) {
		L2TP_DEBUG(L2TP_DATA, "%s: fake tx drop: tid=%hu/%hu, ns/nr=%hu/%hu, msg=%d", __func__,
			   xprt->tunnel_id, pkt->session_id, pkt->ns, pkt->nr, pkt->msg_type);
		result = 0;
		goto out;
	}
#endif

	/* Build the struct msghdr to send the packet */
	memset(&msg, 0, sizeof(msg));
	result = l2tp_net_prepare_msghdr(&msg, pkt);
	if (result < 0) {
		l2tp_stats.no_control_frame_resources++;
		goto out;
	}

	tunnel = xprt->tunnel;

	/* If peer socket isn't yet connected, use sendto() semantics.
	 * The socket will be connected as soon as we know the peer's
	 * port number, which we can derive from his first packet.
	 */
	peer = xprt->peer;
	peer_addr = l2tp_tunnel_get_peer_addr(tunnel);
	if (!l2tp_tunnel_is_fd_connected(tunnel)) {
		msg.msg_namelen = sizeof(*peer_addr);
		msg.msg_name = (struct sockaddr_in *) peer_addr;
		L2TP_DEBUG(L2TP_DATA, "%s: sendto(): peer %x/%hu on fd=%d", __func__, 
			   htonl(peer_addr->sin_addr.s_addr), htons(peer_addr->sin_port), l2tp_tunnel_get_fd(tunnel));
	}

	if (pkt->avp_len > 0) {
		l2tp_tunnel_log(xprt->tunnel, L2TP_DATA, LOG_DEBUG, "DATA: TX: tunl %hu/%hu: %ssend %d bytes to peer %s, "
				"packet ns/nr %hu/%hu type %d, retry %d",
				xprt->tunnel_id, pkt->session_id, (pkt->requeue_count > 0) ? "re" : "", pkt->total_len, 
				inet_ntoa(peer_addr->sin_addr), pkt->ns, pkt->nr, pkt->msg_type, 
				(pkt->requeue_count >= 0) ? pkt->requeue_count : 0);
	}

	result = sendmsg(xprt->fd, &msg, MSG_DONTWAIT | MSG_NOSIGNAL);
	if (result < pkt->total_len) {
		L2TP_DEBUG(L2TP_DATA, "%s: send() failed: peer %x/%hu, fd %d, result=%d, total_len=%d", __func__,
			   htonl(peer_addr->sin_addr.s_addr), htons(peer_addr->sin_port), l2tp_tunnel_get_fd(tunnel), 
			   result, pkt->total_len);
		result = -EIO;
		if (pkt->avp_len > 0) {
			l2tp_stats.total_control_frame_send_fails++;
		} else {
			xprt->stats.tx_zlb_fails++;
		}
	} else {
		xprt->stats.tx_packets++;
		xprt->stats.tx_bytes += pkt->total_len;
		if (pkt->avp_len > 0) {
			if ((pkt->msg_type > 0) && (pkt->msg_type < L2TP_AVP_MSG_COUNT)) {
				l2tp_stats.total_sent_control_frames++;
				l2tp_stats.messages[pkt->msg_type].tx++;
			}
		} else {
			xprt->stats.tx_zlbs++;
		}
	}

out:
	return result;
}

/* Process a transport's transmit queue.
 * We come here whenever a timer fires or a packet is received to try to make
 * progress transmitting any queued packets to our peer. Packets are held in the
 * transmit queue when the transmit window is full, i.e. we're waiting for other
 * packets to be acked.
 */
static void l2tp_xprt_process_txq(struct l2tp_xprt *xprt)
{
	struct l2tp_packet *pkt;

	/* Dequeue as many control packets as we can, subject to the sliding window.
	 * ZLB's are not subject to the sliding window and are never queued to txq.
	 */
	for (;;) {
		int status;

		pkt = l2tp_pkt_peek(&xprt->txq);
		if (pkt == NULL) {
			break;
		}

#ifdef DEBUG
		if (pkt->avp_len == 0) {
			L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: zlb pkt ns/nr=%hu/%hu wrongly queued",
				   __func__, xprt->tunnel_id, xprt->ns, xprt->nr);
			l2tp_pkt_unlink(pkt);
			l2tp_pkt_free(pkt);
			continue;
		}
#endif /* DEBUG */

		if (!l2tp_xprt_is_tx_window_open(xprt, xprt->ns)) {
			if (!xprt->is_tx_stalled) {
				xprt->is_tx_stalled = 1;
				L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: ns/nr=%hu/%hu, tx window is closed", __func__,
					   xprt->tunnel_id, xprt->ns, xprt->nr);
			}
			break;
		} else {
			if (xprt->is_tx_stalled) {
				xprt->is_tx_stalled = 0;
				L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: ns/nr=%hu/%hu, tx window now open", __func__,
					   xprt->tunnel_id, xprt->ns, xprt->nr);
			}
		}

		/* Dequeue packet */
		pkt = l2tp_pkt_dequeue(&xprt->txq);
		pkt->ns = xprt->ns;
		pkt->nr = xprt->nr;
		xprt->ns++;
		l2tp_tunnel_log(xprt->tunnel, L2TP_XPRT, LOG_DEBUG, "XPRT: tunl %hu: update ns to %hu", 
				xprt->tunnel_id, xprt->ns);
#ifdef DEBUG
		if (xprt->ns == 0) {
			l2tp_xprt_ns_wraps++;
		}
#endif /* DEBUG */

		l2tp_net_update_header(pkt->iov[0].iov_base, pkt->ns, pkt->nr);
		l2tp_tunnel_log(xprt->tunnel, L2TP_XPRT, LOG_DEBUG, "XPRT: tunl %hu: adding packet to ackq, type %hu, len %d, ns/nr %hu/%hu", 
				xprt->tunnel_id, pkt->msg_type, pkt->total_len, pkt->ns, pkt->nr);
		l2tp_pkt_queue_add(&xprt->ackq, pkt);

		status = l2tp_xprt_sendmsg(xprt, pkt);
		if (status < 0) {
			break;
		}
	}

	/* If we're closing and txq and ackq are both empty, the peer has acknowledged 
	 * any tunnel close STOPCCN that we sent. We can safely delete the tunnel
	 * now. Note that RFC2661 says we should hang around long enough for control
	 * protocol messages to be acknowledged - they are now.
	 */
	if (xprt->is_closing && l2tp_pkt_queue_empty(&xprt->ackq) && l2tp_pkt_queue_empty(&xprt->txq)) {
		l2tp_tunnel_close_now(xprt->tunnel);
		goto out;
	}

	/* Restart retry timer if ackq is not empty */
	if ((!l2tp_pkt_queue_empty(&xprt->ackq)) && (!usl_timer_is_running(xprt->retry_timer))) {
		usl_timer_restart(xprt->retry_timer);
	}

out:
	return;
}

/*****************************************************************************
 * Public interface
 *****************************************************************************/

/* Give plugins access to our kernel file descriptor.
 */
int l2tp_xprt_get_kernel_fd(struct l2tp_tunnel const *tunnel)
{
	struct l2tp_xprt *xprt = l2tp_tunnel_get_xprt(tunnel);

	return xprt->kernel_fd;
}

/* Try to transmit any queued packets before sending the new packet.
 * Packets are queued subject to the sliding window.
 */
int l2tp_xprt_send(struct l2tp_xprt *xprt, struct l2tp_packet *pkt)
{
	int result = 0;
	uint8_t *data;
	struct sockaddr_in const *peer_addr;

	pkt->ns = xprt->ns;
	pkt->nr = xprt->nr;

	peer_addr = l2tp_tunnel_get_peer_addr(xprt->tunnel);
 	L2TP_DEBUG(L2TP_FUNC, "%s: tunl %hu: pkt=%p len=%d msgtype=%hu", __func__, xprt->tunnel_id, pkt, pkt->total_len, pkt->msg_type);
	L2TP_DEBUG(L2TP_DATA, "%s: tunl %hu: fd=%d peer=%x/%hu", __func__, xprt->tunnel_id,
		   l2tp_tunnel_get_fd(xprt->tunnel), ntohl(peer_addr->sin_addr.s_addr), ntohs(peer_addr->sin_port));

	data = pkt->iov[0].iov_base;

 	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: pkt=%p len=%d", __func__, xprt->tunnel_id, pkt, pkt->total_len);
	L2TP_DEBUG(L2TP_DATA, "%s: data=%02x %02x %02x %02x %02x %02x %02x %02x", __func__,
		    data[0], data[1], data[2], data[3],
		    data[4], data[5], data[6], data[7]);

	pkt->xprt = xprt;

#ifdef DEBUG
	/* ZLB messages should not be queued here */
	if (pkt->avp_len == 0) {
		L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: zlb pkt @ ns/nr=%hu/%hu wrongly queued",
			   __func__, xprt->tunnel_id, xprt->ns, xprt->nr);
		l2tp_pkt_free(pkt);
		goto dequeue;
	}
#endif /* DEBUG */

	/* control packets are queued */
	l2tp_tunnel_log(xprt->tunnel, L2TP_XPRT, LOG_DEBUG, "XPRT: tunl %hu: queuing tx packet, type %hu, len %d, ns/nr %hu/%hu", 
			xprt->tunnel_id, pkt->msg_type, pkt->total_len, pkt->ns, pkt->nr);
	l2tp_pkt_queue_add(&xprt->txq, pkt);

#ifdef DEBUG
dequeue:
#endif
	/* and now dequeue as many control packets as we can, subject to the sliding window */
	l2tp_xprt_process_txq(xprt);

	return result;
}

/* We come here when a tunnel is established, once its peer tunnel id is known.
 * The final transport initialization is done here.
 */
int l2tp_xprt_set_peer_tunnel_id(struct l2tp_xprt *xprt, uint16_t peer_tunnel_id)
{
	int result = 0;
	int kernel_fd;

	if (xprt->peer_tunnel_id == 0) {
		L2TP_DEBUG(L2TP_XPRT, "%s: set peer tid=%hu for tid %hu", __func__,
			   peer_tunnel_id, xprt->tunnel_id);
		xprt->peer_tunnel_id = peer_tunnel_id;

		/* Create a socket for communicating with the kernel */
		kernel_fd = socket(AF_PPPOX, SOCK_DGRAM, PX_PROTO_OL2TP);
		if (kernel_fd >= 0) {
			struct sockaddr_pppol2tp sax;
			struct sockaddr_in const *peer_addr;

			/* Prevent children inheriting our socket */
			fcntl(kernel_fd, F_SETFD, FD_CLOEXEC); 

			peer_addr = l2tp_tunnel_get_peer_addr(xprt->tunnel);
			memset(&sax, 0, sizeof(sax));
			sax.sa_family = AF_PPPOX;
			sax.sa_protocol = PX_PROTO_OL2TP;
			sax.pppol2tp.fd = xprt->fd;
			sax.pppol2tp.addr.sin_addr.s_addr = peer_addr->sin_addr.s_addr;
			sax.pppol2tp.addr.sin_port = peer_addr->sin_port;
			sax.pppol2tp.addr.sin_family = AF_INET;
			sax.pppol2tp.s_tunnel = xprt->tunnel_id;
			sax.pppol2tp.s_session = 0;
			sax.pppol2tp.d_tunnel = xprt->peer_tunnel_id;
			sax.pppol2tp.d_session = 0;
  
			for (;;) {
				if (connect(kernel_fd, (struct sockaddr *)&sax, sizeof(sax) ) < 0 ) {
					if ((errno == EINTR) || (errno == ERESTART)) {
						continue;
					}
					l2tp_log(LOG_ERR, "tunl %hu: kernel socket connect failed: %s", xprt->tunnel_id, strerror(errno));
					result = -errno;
					goto err;
				}
				break;
			}
			xprt->kernel_fd = kernel_fd;
		} else {
			/* Failed to create kernel socket, probably because the kernel does
			 * not have PPPoL2TP support enabled. We'll abort with a loud error,
			 * but because it is useful to run l2tpd on kernels without
			 * PPPoL2TP support for testing, we'll abort only if debug is disabled.
			 */
#ifdef L2TP_DEBUG
#define NO_KERNEL_OK 1
#else
#define NO_KERNEL_OK 0
#endif
			if ((l2tp_opt_debug) || (l2tp_opt_trace_flags != 0) || NO_KERNEL_OK) {
				l2tp_tunnel_log(xprt->tunnel, L2TP_API, LOG_WARNING, "API: tunl %hu: continuing with no kernel support", xprt->tunnel_id);
				goto out;
			}
			l2tp_log(LOG_ERR, "tunl %hu: no kernel PPPoL2TP support: %s. Aborting.", xprt->tunnel_id, strerror(errno));
			result = -errno;
			goto err;
		}
	}

out:
	if (xprt->hello_timer != NULL) {
		usl_timer_delete(xprt->hello_timer);
		xprt->hello_timer = NULL;
	}
	if (xprt->params.hello_timeout != 0) {
		xprt->hello_timer = usl_timer_create(USL_TIMER_TICKS(xprt->params.hello_timeout), USL_TIMER_TICKS(xprt->params.hello_timeout), 
						     l2tp_xprt_hello_timeout, xprt, l2tp_xprt_hello_timer_adjust);
		L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: allocated hello_timer=%p", __func__, xprt->tunnel_id, xprt->retry_timer);
	}

err:
	return result;
}

/* Come here when the tunnel is being closed down, but is being held
 * around for a while while the tunnel tries to resend any queued
 * messages. We stop the tunnel's hello timer.  The retry timer is
 * left running, to give the tunnel a chance to retry any queued
 * messages to the peer.
 */
void l2tp_xprt_tunnel_going_down(struct l2tp_xprt *xprt)
{
	if ((xprt == NULL) || (xprt->is_closing)) {
		return;
	}

	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu", __func__, xprt->tunnel_id);

	if (xprt->hello_timer != NULL) {
		usl_timer_stop(xprt->hello_timer);
	}
	xprt->is_closing = 1;
}

/* Come here when a tunnel has closed down. Timers are stopped and queues
 * are purged.
 */
void l2tp_xprt_tunnel_down(struct l2tp_xprt *xprt)
{
	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu", __func__, xprt->tunnel_id);

	if (xprt->retry_timer != NULL) {
		usl_timer_stop(xprt->retry_timer);
	}
	if (xprt->hello_timer != NULL) {
		usl_timer_stop(xprt->hello_timer);
	}
	if (xprt->zlb_timer != NULL) {
		usl_timer_stop(xprt->zlb_timer);
	}

	/* Purge ack-pending, tx-pending and rx queues */
	L2TP_DEBUG(L2TP_DATA, "%s: ackq", __func__);
	l2tp_pkt_queue_purge(&xprt->ackq);
	L2TP_DEBUG(L2TP_DATA, "%s: txq", __func__);
	l2tp_pkt_queue_purge(&xprt->txq);
	L2TP_DEBUG(L2TP_DATA, "%s: rxq", __func__);
	l2tp_pkt_queue_purge(&xprt->rxq);

	xprt->is_closing = 1;
}

/* Create a new transport instance for a tunnel.
 */
int l2tp_xprt_tunnel_create(int fd, uint16_t tunnel_id, struct l2tp_xprt_tunnel_create_data *msg, void *tunnel, void **handle)
{
	struct l2tp_xprt *xprt = NULL;
	int result;
	int kernel_fd = -1;

	L2TP_DEBUG(L2TP_XPRT, "%s: ip %x/%x", __func__, ntohl(msg->our_addr), ntohl(msg->peer_addr));

	xprt = calloc(1, sizeof(*xprt));
	if (xprt == NULL) {
		result = -ENOMEM;
		goto err;
	}

	USL_LIST_HEAD_INIT(&xprt->list);
	USL_LIST_HEAD_INIT(&xprt->ackq);
	USL_LIST_HEAD_INIT(&xprt->txq);
	USL_LIST_HEAD_INIT(&xprt->rxq);

	/* Copy tunnel parameters. */
	xprt->params.max_retries = msg->max_retries;
	xprt->params.rx_window_size = msg->rx_window_size;
	xprt->params.tx_window_size = msg->tx_window_size;
	xprt->params.retry_timeout = msg->retry_timeout;
	xprt->params.hello_timeout = msg->hello_timeout;
	xprt->params.our_addr.s_addr = msg->our_addr;
	xprt->params.peer_addr.s_addr = msg->peer_addr;

	xprt->tunnel_id = tunnel_id;
	xprt->fd = fd;
	xprt->kernel_fd = -1;

	/* Slow start variables. See RFC2661 Appendix A */
	xprt->cwnd = 1;
	xprt->ssthresh = xprt->params.rx_window_size;
	xprt->is_congested = 0; 		/* slow start */
	xprt->congpkt_acc = 0; 

	*handle = xprt;

	xprt->retry_duration = xprt->params.retry_timeout;
	xprt->retry_timer = usl_timer_create(USL_TIMER_TICKS(xprt->params.retry_timeout), USL_TIMER_TICKS(xprt->retry_duration), 
					     l2tp_xprt_retry_timeout, xprt, NULL);
	if (xprt->retry_timer == NULL) {
		result = -ENOMEM;
		goto err;
	}
	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: allocated retry_timer=%p", __func__, xprt->tunnel_id, xprt->retry_timer);

	xprt->zlb_timer = usl_timer_create(USL_TIMER_500MSEC, 0, l2tp_xprt_zlb_timeout, xprt, NULL);
	if (xprt->zlb_timer == NULL) {
		result = -ENOMEM;
		goto err;
	}
	usl_timer_stop(xprt->zlb_timer);
	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu: allocated zlb_timer=%p", __func__, xprt->tunnel_id, xprt->zlb_timer);

	usl_list_add_tail(&xprt->list, &l2tp_xprt_list);

	xprt->tunnel = tunnel;
	xprt->peer = l2tp_tunnel_get_peer(tunnel);

	return 0;

err:
	if (xprt->retry_timer != NULL) {
		usl_timer_delete(xprt->retry_timer);
		xprt->retry_timer = NULL;
	}
	if (xprt->zlb_timer != NULL) {
		usl_timer_delete(xprt->zlb_timer);
		xprt->zlb_timer = NULL;
	}
	l2tp_stats.no_tunnel_resources++;

	if (kernel_fd >= 0) {
		close(kernel_fd);
	}
	if (xprt != NULL) {
		free(xprt);
	}
	return result;
}

/* Delete a transport instance.
 * Should be called only when the tunnel has shut down and is idle.
 */
int l2tp_xprt_tunnel_delete(struct l2tp_xprt *xprt)
{
	L2TP_DEBUG(L2TP_XPRT, "%s: tunl %hu", __func__, xprt->tunnel_id);

	l2tp_xprt_tunnel_down(xprt);

	usl_list_del(&xprt->list);
	if (xprt->retry_timer != NULL) {
		usl_timer_delete(xprt->retry_timer);
		xprt->retry_timer = NULL;
	}
	if (xprt->hello_timer != NULL) {
		usl_timer_delete(xprt->hello_timer);
		xprt->hello_timer = NULL;
	}
	if (xprt->zlb_timer != NULL) {
		usl_timer_delete(xprt->zlb_timer);
		xprt->zlb_timer = NULL;
	}

	if (xprt->kernel_fd >= 0) {
		close(xprt->kernel_fd);
	}
	if (xprt->fd >= 0) {
		usl_fd_remove_fd(xprt->fd);
		close(xprt->fd);
	}

	USL_POISON_MEMORY(xprt, 0xea, sizeof(*xprt));
	free(xprt);

	return 0;
}

/* Modify parameters of a transport context.
 * Called via tunnel's management interface.
 */
int l2tp_xprt_tunnel_modify(struct l2tp_xprt *xprt, struct l2tp_xprt_tunnel_modify_data *msg)
{
	L2TP_DEBUG(L2TP_API, "%s: tunl %hu", __func__, xprt->tunnel_id);

	if (msg->flags & L2TP_XPRT_TUN_FLAG_MAX_RETRIES) {
		xprt->params.max_retries = msg->max_retries;
		L2TP_DEBUG(L2TP_API, "%s: tunl %hu:, modify max_retries to %hu", __func__,
			   xprt->tunnel_id, msg->max_retries);
	}
	if (msg->flags & L2TP_XPRT_TUN_FLAG_RX_WINDOW_SIZE) {
		xprt->params.rx_window_size = msg->rx_window_size;
		L2TP_DEBUG(L2TP_API, "%s: tunl %hu:, modify rx_window_size to %hu", __func__,
			   xprt->tunnel_id, msg->rx_window_size);
	}
	if (msg->flags & L2TP_XPRT_TUN_FLAG_TX_WINDOW_SIZE) {
		xprt->params.tx_window_size = msg->tx_window_size;
		L2TP_DEBUG(L2TP_API, "%s: tunl %hu:, modify tx_window_size to %hu", __func__,
			   xprt->tunnel_id, msg->tx_window_size);
	}
	if (msg->flags & L2TP_XPRT_TUN_FLAG_RETRY_TIMEOUT) {
		xprt->params.retry_timeout = msg->retry_timeout;
		if (xprt->retry_timer != NULL) {
			usl_timer_interval_set(xprt->retry_timer, USL_TIMER_TICKS(xprt->params.retry_timeout));
		}
		L2TP_DEBUG(L2TP_API, "%s: tunl %hu:, modify retry_timeout to %hu", __func__,
			   xprt->tunnel_id, msg->retry_timeout);
	}
	if (msg->flags & L2TP_XPRT_TUN_FLAG_HELLO_TIMEOUT) {
		xprt->params.hello_timeout = msg->hello_timeout;
		if (xprt->hello_timer != NULL) {
			usl_timer_interval_set(xprt->hello_timer, USL_TIMER_TICKS(xprt->params.hello_timeout));
		}
		L2TP_DEBUG(L2TP_API, "%s: tunl %hu:, modify hello_timeout to %hu", __func__,
			   xprt->tunnel_id, msg->hello_timeout);
	}

	return 0;
}

/* Get kernel statistics of a transport context.
 */
int l2tp_xprt_kernel_get(struct l2tp_xprt *xprt, struct l2tp_api_tunnel_stats *msg)
{
	int result = 0;

	L2TP_DEBUG(L2TP_API, "%s: tunl %hu", __func__, xprt->tunnel_id);

	/* get tunnel data counters from kernel */
	if (xprt->kernel_fd >= 0) {
		struct pppol2tp_ioc_stats stats;
		memset(&stats, 0, sizeof(stats));
		stats.tunnel_id = xprt->tunnel_id;
		result = ioctl(xprt->kernel_fd, PPPIOCGL2TPSTATS, &stats);
		if (result < 0) {
			l2tp_tunnel_log(xprt->tunnel, L2TP_API, LOG_ERR, "API: tunl %hu: ioctl(PPPIOCGL2TPSTATS) failed: %m", xprt->tunnel_id);
			result = -errno;
			goto out;
		}
		msg->data_rx_oos_packets = stats.rx_oos_packets;
		msg->data_rx_oos_discards = stats.rx_seq_discards;
		msg->data_rx_packets = stats.rx_packets;
		msg->data_rx_bytes = stats.rx_bytes;
		msg->data_rx_errors = stats.rx_errors;
		msg->data_tx_packets = stats.tx_packets;
		msg->data_tx_bytes = stats.tx_bytes;
		msg->data_tx_errors = stats.tx_errors;
		msg->using_ipsec = stats.using_ipsec;
	} else {
		msg->data_rx_oos_packets = 0ULL;
		msg->data_rx_oos_discards = 0ULL;
		msg->data_rx_packets = 0ULL;
		msg->data_rx_bytes = 0ULL;
		msg->data_rx_errors = 0ULL;
		msg->data_tx_packets = 0ULL;
		msg->data_tx_bytes = 0ULL;
		msg->data_tx_errors = 0ULL;
		msg->using_ipsec = 0;
	}

out:
	return result;
}

/* Get parameters and statistics of a transport context.
 */
int l2tp_xprt_tunnel_get(struct l2tp_xprt *xprt, struct l2tp_api_tunnel_msg_data *msg)
{
	int result;

	L2TP_DEBUG(L2TP_API, "%s: tunl %hu", __func__, xprt->tunnel_id);

	msg->stats.retransmits = xprt->stats.retransmits;
	msg->stats.tx_zlbs = xprt->stats.tx_zlbs;
	msg->stats.tx_zlb_fails = xprt->stats.tx_zlb_fails;
	msg->stats.rx_zlbs = xprt->stats.rx_zlbs;
	msg->stats.data_pkt_discards = xprt->stats.data_pkt_discards;
	msg->stats.duplicate_pkt_discards = xprt->stats.duplicate_pkt_discards;
	msg->stats.rx_hellos = xprt->stats.rx_hellos;
	msg->stats.tx_hellos = xprt->stats.tx_hellos;
	msg->stats.tx_hello_fails = xprt->stats.tx_hello_fails;
	msg->stats.ns = xprt->ns;
	msg->stats.nr = xprt->nr;
	msg->stats.peer_ns = xprt->peer_ns;
	msg->stats.peer_nr = xprt->peer_nr;
	msg->stats.cwnd = xprt->cwnd;
	msg->stats.ssthresh = xprt->ssthresh;
	msg->stats.congpkt_acc = xprt->congpkt_acc;
	msg->stats.control_rx_packets = xprt->stats.rx_packets;
	msg->stats.control_rx_bytes = xprt->stats.rx_bytes;
	msg->stats.control_rx_oos_packets = xprt->stats.oos_control_packets;
	msg->stats.control_rx_oos_discards = xprt->stats.oos_control_discards;
	msg->stats.control_tx_packets = xprt->stats.tx_packets;
	msg->stats.control_tx_bytes = xprt->stats.tx_bytes;

	result = l2tp_xprt_kernel_get(xprt, &msg->stats);

	return result;
}

/*****************************************************************************
 * Init and cleanup
 *****************************************************************************/

void l2tp_xprt_init(void)
{
	l2tp_xprt_jiffies = 0;

	usl_timer_tick_hook = l2tp_xprt_tick;
}

void l2tp_xprt_cleanup(void)
{
}

/*****************************************************************************
 * Test code
 *****************************************************************************/

#ifdef L2TP_TEST

int l2tp_xprt_test(uint16_t tunnel_id)
{
	struct l2tp_tunnel *tunnel;
	int count;

	tunnel = l2tp_tunnel_find_by_id(tunnel_id);
	if (tunnel == NULL) {
		return -ENOENT;
	}

	/* Try to send 100 HELLO messages as quickly as possible.
	 * The transport slow-start and sliding-window mechanisms should
	 * kick in, but no frames should be lost.
	 */
	for (count = 0; count < 100; count++) {
		l2tp_xprt_send_hello(l2tp_tunnel_get_xprt(tunnel));
	}
	
	return 0;
}

#endif /* L2TP_TEST */
