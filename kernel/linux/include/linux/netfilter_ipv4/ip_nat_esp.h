#ifndef _IP_ALG_ESP_H
#define _IP_ALG_ESP_H
/* ESP extension for NAT alteration. */

#ifndef __KERNEL__
#error Only in kernel.
#endif

/* Protects IPSec part of conntracks */
DECLARE_LOCK_EXTERN(ip_esp_lock);

struct ip_nat_esp_info
{
	u_int32_t	spi;     /* Security Parameter Id  */
	u_int32_t	saddr;   /* Orig Source IP address */
	u_int32_t	daddr;   /* Remote IP address      */
};

#endif /* _IP_ALG_ESP_H */
