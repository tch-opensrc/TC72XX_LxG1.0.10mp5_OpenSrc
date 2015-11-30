/*
<:copyright-gpl
 Copyright 2004 Broadcom Corp. All Rights Reserved.

 This program is free software; you can distribute it and/or modify it
 under the terms of the GNU General Public License (Version 2) as
 published by the Free Software Foundation.

 This program is distributed in the hope it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
:>
*/

/***********************************************************************/
/*                                                                     */
/*   MODULE:  bcmnet.h                                                 */
/*   DATE:    05/16/02                                                 */
/*   PURPOSE: network interface ioctl definition                       */
/*                                                                     */
/***********************************************************************/
#ifndef _IF_NET_H_
#define _IF_NET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/sockios.h>
#include "skb_defines.h"

#define LINKSTATE_DOWN      0
#define LINKSTATE_UP        1

#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
#define NR_RX_BDS_MAX           300    /* reduce the number of rx BDs to prevent DQM overflow */
#define NR_RX_BDS(x)            NR_RX_BDS_MAX
#define NR_TX_BDS               90
#else
#define NR_RX_BDS_MAX           400
#define NR_RX_BDS(x)            NR_RX_BDS_MAX
#define NR_TX_BDS               90
#endif

#ifndef IFNAMSIZ
#define IFNAMSIZ  16
#endif

/*---------------------------------------------------------------------*/
/* Ethernet Switch Type                                                */
/*---------------------------------------------------------------------*/
#define ESW_TYPE_UNDEFINED                  0
#define ESW_TYPE_BCM5325M                   1
#define ESW_TYPE_BCM5325E                   2
#define ESW_TYPE_BCM5325F                   3
#define ESW_TYPE_BCM53101                   4

/*
 * Ioctl definitions.
 */
/* Note1: The maximum device private ioctls allowed are 16 */
/* Note2: SIOCDEVPRIVATE is reserved */
enum {
    SIOCGLINKSTATE = SIOCDEVPRIVATE + 1,
    SIOCSCLEARMIBCNTR,
    SIOCMIBINFO,
#if defined(CONFIG_BCM93383) && defined(CONFIG_BCM_LOT1)
    SIOCGAPPSTATE,
    SIOCSAPPSTATE,
#else
    SIOCGENABLEVLAN,
    SIOCGDISABLEVLAN,
#endif
    SIOCGQUERYNUMVLANPORTS,
    SIOCGQUERYNUMPORTS,
    SIOCPORTMIRROR,
    SIOCSWANPORT,
    SIOCGWANPORT,
    SIOCETHCTLOPS,
    SIOCRMTAPI,      /*usurp this spot - SIOCGPONIF not used */
    SIOCETHSWCTLOPS,
    SIOCLAST,
};

/* Various operations through the SIOCETHCTLOPS */
enum {
    ETHGETNUMTXDMACHANNELS = 0,
    ETHSETNUMTXDMACHANNELS,
    ETHGETNUMRXDMACHANNELS,
    ETHSETNUMRXDMACHANNELS,
};

struct ethctl_data {
    /* ethctl ioctl operation */
    int op;
    /* number of DMA channels */
    int num_channels;
    /* return value for get operations */
    int ret_val;
};

/* Various operations through the SIOCGPONIF */
enum {
    GETFREEGEMIDMAP = 0,
    SETGEMIDMAP,
    GETGEMIDMAP,
    CREATEGPONVPORT,
    DELETEGPONVPORT,
    DELETEALLGPONVPORTS,
};

struct gponif_data{
    /* gponif ioctl operation */
    int op;
    /* GEM ID map for addgem and remgem operations */
    unsigned int gem_map;
    /* ifnumber for show all operation */
    int ifnumber;
    /* interface name for create, delete, addgem, remgem, and show ops */
    char ifname[IFNAMSIZ];
};

#if 0
/* Various operations through the SIOCETHSWCTLOPS */
enum {
    ETHSWDUMPPAGE = 0,
    ETHSWDUMPMIB,
    ETHSWSWITCHING,
    ETHSWRXSCHEDULING,
    ETHSWWRRPARAM,
    ETHSWCONTROL,
    ETHSWPRIOCONTROL,
    ETHSWPORTTAGREPLACE,
    ETHSWPORTTAGMANGLE,
    ETHSWPORTTAGMANGLEMATCHVID,
    ETHSWPORTTAGSTRIP,
    ETHSWPORTPAUSECAPABILITY,
    ETHSWVLAN,
    ETHSWGETRXCOUNTERS,
    ETHSWRESETRXCOUNTERS,
    ETHSWPBVLAN,
    ETHSWCOSCONF,
    ETHSWCOSSCHED,
    ETHSWCOSPORTMAP,
    ETHSWCOSRXCHMAP,
    ETHSWCOSTXCHMAP,
    ETHSWCOSTXQSEL,
    ETHSWSTATCLR,
    ETHSWSTATPORTCLR,
    ETHSWSTATSYNC,
    ETHSWSTATGET,
    ETHSWPORTRXRATE,
    ETHSWPORTTXRATE,
};

#if defined(CONFIG_BCM96816) || defined(CHIP_6816)
#define BCM_COS_COUNT  8
#else
#define BCM_COS_COUNT  4
#endif

struct ethswctl_data {
    /* ethswctl ioctl operation */
    int op;
    /* page number */
    int page;
    /* switch port number */
    int port;
    /* dump subset or all; or indicates enable/disable/get op */
    int type;
#define TYPE_SUBSET  0
#define TYPE_ALL     1
#define TYPE_DISABLE 0
#define TYPE_ENABLE  1
#define TYPE_GET     2
#define TYPE_SET     3
    /* hardware switching enable/disable status */ 
    int status;
#define STATUS_DISABLED 0
#define STATUS_ENABLED  1
    int vlan_tag;
    int vlan_param;
    int replace_type;
/* Defines for indicating the parameter in tag replace register */
#define  REPLACE_VLAN_TAG    0
#define  REPLACE_VLAN_TPID   1
#define  REPLACE_VLAN_TCI    2
#define  REPLACE_VLAN_VID    3
#define  REPLACE_VLAN_8021P  4
#define  REPLACE_VLAN_CFI	 5
    int op_map;
	int ret_val;
	int val;
	int max_pkts_per_iter;
	int weights[BCM_COS_COUNT];
	int priority;
    int sw_ctrl_type;
    /* scheduling value for ETHSETSCHEDULING */
    int scheduling;
	int vid;
	int fwd_map;
	int untag_map;
	int queue;
	int channel;
	int numq;
	unsigned long limit;
	unsigned long burst_size;
	unsigned long long counter_val;
	int counter_type;
};

/* Defines for set/get of various fields in VLAN TAG */
#define BCM_NET_VLAN_TPID_S        16
#define BCM_NET_VLAN_TPID_M        0xFFFF
#define BCM_NET_VLAN_VID_S         0
#define BCM_NET_VLAN_VID_M		   0xFFF
#define BCM_NET_VLAN_TCI_S         0
#define BCM_NET_VLAN_TCI_M		   0xFFFF
#define BCM_NET_VLAN_8021P_S       13
#define BCM_NET_VLAN_8021P_M       0x7
#define BCM_NET_VLAN_CFI_S         12
#define BCM_NET_VLAN_CFI_M         0x1

#define VLAN_FWD_MAP_M   0x1FF
#define VLAN_UNTAG_MAP_M 0x1FF
#define VLAN_UNTAG_MAP_S 9

/* Switch controls for sw_ctrl_type */
typedef enum bcm_switch_control_e {
	bcmSwitchBufferControl, 			/* Enable/Disable Total/TxQ Pause/Drop */
	bcmSwitch8021QControl, 			    /* Enable/Disable 802.1Q */
	bcmSwitchTotalDropThreshold,		/* Configure Total Drop Threshold */
	bcmSwitchTotalPauseThreshold,		/* Configure Total Pause Threshold */
	bcmSwitchTotalHysteresisThreshold,	/* Configure Total Hysteresis Threshold */
	bcmSwitchTxQHiDropThreshold,		/* Configure TxQ Hi Drop Threshold */
	bcmSwitchTxQHiPauseThreshold,		/* Configure TxQ Hi Pause Threshold */
	bcmSwitchTxQHiHysteresisThreshold,	/* Configure TxQ Hi Hysteresis Threshold */
	bcmSwitchTxQLowDropThreshold,		/* Configure TxQ LOW Drop Threshold */
	bcmSwitch__Count 
} bcm_switch_control_t;


/* For bcmSwitchBufferControl */
#define BCM_SWITCH_TXQ_PAUSE	   0x1		  /* Enable/Disable TXQ DROP. */
#define BCM_SWITCH_TXQ_DROP 	   0x2		  /* Enable/Disable TXQ PAUSE. */
#define BCM_SWITCH_TOTAL_PAUSE	   0x4		  /* Enable/Disable TOTAL DROP. */
#define BCM_SWITCH_TOTAL_DROP	   0x8		  /* Enable/Disable TOTAL PAUSE. */

/* Defines for the op_map in tag_mangle_set/get */
#define BCM_PORT_REPLACE_TPID		 0x8000  /* Replace TPID */
#define BCM_PORT_REPLACE_8021P		 0x4000  /* Replace 802.1p bits */
#define BCM_PORT_REPLACE_CFI		 0x2000  /* Replace CFI bit */
#define BCM_PORT_REPLACE_VID		 0x1000  /* Replace VLAN ID */
#define BCM_PORT_TAG_MANGLE_OP_MAP_M 0xF000  /* Mask for all tag mangling ops */


/*	For scheduling mechanism selection */
#define SP_SCHEDULING  0
#define WRR_SCHEDULING 1
#define BCM_COSQ_STRICT  0
#define BCM_COSQ_WRR     1
#define BCM_COSQ_COMBO   2

#if defined(CONFIG_BCM96816) || defined(CHIP_6816)
#define NUM_EGRESS_QUEUES  8
#else
#define NUM_EGRESS_QUEUES  4
#endif

#define QOS_PAUSE_DROP_EN_MAP 0xF
#define MAX_PRIORITY_VALUE    7

/* Return Value Definitions */
#define BCM_E_ERROR  1;

/* For pause_capability set/get functions */
#define PAUSE_FLOW_CTRL_NONE 0
#define PAUSE_FLOW_CTRL_AUTO 1
#define PAUSE_FLOW_CTRL_BOTH 2
#define PAUSE_FLOW_CTRL_TX   3
#define PAUSE_FLOW_CTRL_RX   4

#endif

/* The enet driver subdivides queue field (mark[4:0]) in the skb->mark into 
   priority and channel */
/* priority = queue[2:0] (=>mark[2:0]) */
#define SKBMARK_Q_PRIO_S        (SKBMARK_Q_S)
#define SKBMARK_Q_PRIO_M        (0x07 << SKBMARK_Q_PRIO_S)
#define SKBMARK_GET_Q_PRIO(MARK) \
    ((MARK & SKBMARK_Q_PRIO_M) >> SKBMARK_Q_PRIO_S)
#define SKBMARK_SET_Q_PRIO(MARK, Q) \
    ((MARK & ~SKBMARK_Q_PRIO_M) | (Q << SKBMARK_Q_PRIO_S))
/* channel = queue[4:3] (=>mark[4:3]) */
#define SKBMARK_Q_CH_S          (SKBMARK_Q_S + 3)
#define SKBMARK_Q_CH_M          (0x03 << SKBMARK_Q_CH_S)
#define SKBMARK_GET_Q_CHANNEL(MARK) ((MARK & SKBMARK_Q_CH_M) >> SKBMARK_Q_CH_S)
#define SKBMARK_SET_Q_CHANNEL(MARK, CH) \
    ((MARK & ~SKBMARK_Q_CH_M) | (CH << SKBMARK_Q_CH_S))

#define SPEED_10MBIT        10000000
#define SPEED_100MBIT       100000000

typedef struct IoctlMibInfo
{
    unsigned long ulIfLastChange;
    unsigned long ulIfSpeed;
} IOCTL_MIB_INFO, *PIOCTL_MIB_INFO;


#define MIRROR_INTF_SIZE    32
#define MIRROR_DIR_IN       0
#define MIRROR_DIR_OUT      1
#define MIRROR_DISABLED     0
#define MIRROR_ENABLED      1

typedef struct _MirrorCfg
{
    char szMonitorInterface[MIRROR_INTF_SIZE];
    char szMirrorInterface[MIRROR_INTF_SIZE];
    int nDirection;
    int nStatus;
} MirrorCfg ;

#ifdef __cplusplus
}
#endif

#endif /* _IF_NET_H_ */
