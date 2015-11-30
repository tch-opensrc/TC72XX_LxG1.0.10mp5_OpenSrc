#ifndef _NF_CONNTRACK_RTSP_H
#define _NF_CONNTRACK_RTSP_H

#ifdef __KERNEL__

/* This structure exists only once per master */
struct nf_ct_rtsp_master {
	/* The client has sent PAUSE message and not replied */
	int paused;
};

/* Single data channel */
extern int (*nat_rtsp_channel_hook) (struct sk_buff *skb,
				     struct nf_conn *ct,
				     enum ip_conntrack_info ctinfo,
				     unsigned int matchoff,
				     unsigned int matchlen,
				     struct nf_conntrack_expect *exp,
				     int *delta);

/* A pair of data channels (RTP/RTCP) */
extern int (*nat_rtsp_channel2_hook) (struct sk_buff *skb,
				      struct nf_conn *ct,
				      enum ip_conntrack_info ctinfo,
				      unsigned int matchoff,
				      unsigned int matchlen,
				      struct nf_conntrack_expect *rtp_exp,
				      struct nf_conntrack_expect *rtcp_exp,
				      char dash, int *delta);

/* Modify parameters like client_port in Transport for single data channel */
extern int (*nat_rtsp_modify_port_hook) (struct sk_buff *skb,
					 struct nf_conn *ct,
			      	  	 enum ip_conntrack_info ctinfo,
			      	  	 unsigned int matchoff,
					 unsigned int matchlen,
			      	  	 __be16 rtpport, int *delta);

/* Modify parameters like client_port in Transport for multiple data channels*/
extern int (*nat_rtsp_modify_port2_hook) (struct sk_buff *skb,
					  struct nf_conn *ct,
			       	   	  enum ip_conntrack_info ctinfo,
			       	   	  unsigned int matchoff,
					  unsigned int matchlen,
			       	   	  __be16 rtpport, __be16 rtcpport,
				   	  char dash, int *delta);

/* Modify parameters like destination in Transport */
extern int (*nat_rtsp_modify_addr_hook) (struct sk_buff *skb,
					 struct nf_conn *ct,
				 	 enum ip_conntrack_info ctinfo,
					 int matchoff, int matchlen,
					 int *delta);
#endif /* __KERNEL__ */

#endif /* _NF_CONNTRACK_RTSP_H */
