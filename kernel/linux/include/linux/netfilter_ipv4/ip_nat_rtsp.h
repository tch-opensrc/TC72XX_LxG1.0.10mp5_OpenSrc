#ifndef _IP_NAT_RTSP_H
#define _IP_NAT_RTSP_H
/* RTSP extension for UDP NAT alteration. */

#ifndef __KERNEL__
#error Only in kernel.
#endif

/* Protects RTSP part of conntracks */
DECLARE_LOCK_EXTERN(ip_rtp_lock);

struct ip_nat_rtsp_info
{
	u_int32_t	orig_port;  /* Client Port       */
};

#endif /* _IP_NAT_RTSP_H */
