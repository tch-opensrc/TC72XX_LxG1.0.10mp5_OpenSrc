#ifndef _IP_NAT_IPSEC_H
#define _IP_NAT_IPSEC_H
/* IPSec extension for UDP NAT alteration. */

#ifndef __KERNEL__
#error Only in kernel.
#endif

/* Protects IPSec part of conntracks */
DECLARE_LOCK_EXTERN(ip_ipsec_lock);

struct ip_nat_ipsec_info
{
	u_int32_t	initcookie[2];  /* Initiator cookie       */
	u_int32_t	respcookie[2];  /* Responder cookie       */
	u_int32_t	saddr;          /* Orig Source IP address */
};

#endif /* _IP_NAT_IPSEC_H */
