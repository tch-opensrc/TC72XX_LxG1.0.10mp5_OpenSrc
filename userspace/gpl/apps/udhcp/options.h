/* options.h */
#ifndef _OPTIONS_H
#define _OPTIONS_H

#include "packet.h"

#define TYPE_MASK	0x0F

enum {
	OPTION_IP=1,
	OPTION_IP_PAIR,
	OPTION_STRING,
	OPTION_BOOLEAN,
	OPTION_U8,
	OPTION_U16,
	OPTION_S16,
	OPTION_U32,
	OPTION_S32
};

#define OPTION_LIST	0x80

struct dhcp_option {
	char name[10];
	char flags;
	char code;
};

extern struct dhcp_option options[];
extern int option_lengths[];

unsigned char *get_option(struct dhcpMessage *packet, int code);
int end_option(unsigned char *optionptr);
int add_option_string(unsigned char *optionptr, unsigned char *string);
int add_simple_option(unsigned char *optionptr, unsigned char code, u_int32_t data);
struct option_set *find_option(struct option_set *opt_list, char code);
void attach_option(struct option_set **opt_list, struct dhcp_option *option, char *buffer, int length);

// brcm
extern char session_path[];

// brcm
int createVIoption(int type, char *VIinfo);
int CreateOption125(int type, char *oui, char *sn, char *prod, char *VIinfo);
int CreateClntId(char *iaid, char *duid, char *out_op);
int saveVIoption(char *option, struct dhcpOfferedAddr *lease);

#define _PATH_RESOLV	 "/var/fyi/sys/dns"
#define _PATH_GW	 "/var/fyi/sys/gateway"
#define _PATH_SYS_DIR	 "/var/fyi/sys"

#define _PATH_WAN_DIR	"/proc/var/fyi/wan"
#define _PATH_MSG	"daemonstatus"
#define _PATH_IP	"ipaddress"
#define _PATH_MASK	"subnetmask"
#define _PATH_PID	"pid"

/* option header: 2 bytes + subcode headers (6) + option data (64*2+6);
   these are TR111 option data.    Option can be longer.  */
#define VENDOR_BRCM_ENTERPRISE_NUMBER     4413
#define VENDOR_ENTERPRISE_LEN             4    /* 4 bytes */
#define VENDOR_IDENTIFYING_INFO_LEN       142
#define VENDOR_IDENTIFYING_OPTION_CODE    125
#define VENDOR_OPTION_CODE_OFFSET         0
#define VENDOR_OPTION_LEN_OFFSET          1
#define VENDOR_OPTION_ENTERPRISE_OFFSET   2
#define VENDOR_OPTION_DATA_OFFSET         6
#define VENDOR_OPTION_DATA_LEN            1
#define VENDOR_OPTION_SUBCODE_LEN         1
#define VENDOR_SUBCODE_AND_LEN_BYTES      2
#define VENDOR_DEVICE_OUI_SUBCODE            1
#define VENDOR_DEVICE_SERIAL_NUMBER_SUBCODE  2
#define VENDOR_DEVICE_PRODUCT_CLASS_SUBCODE  3
#define VENDOR_GATEWAY_OUI_SUBCODE           4
#define VENDOR_GATEWAY_SERIAL_NUMBER_SUBCODE 5
#define VENDOR_GATEWAY_PRODUCT_CLASS_SUBCODE 6
#define VENDOR_IDENTIFYING_FOR_GATEWAY       1
#define VENDOR_IDENTIFYING_FOR_DEVICE        2
#define VENDOR_GATEWAY_OUI_MAX_LEN           6
#define VENDOR_GATEWAY_SERIAL_NUMBER_MAX_LEN 64 
#define VENDOR_GATEWAY_PRODUCT_CLASS_MAX_LEN 64

#endif
