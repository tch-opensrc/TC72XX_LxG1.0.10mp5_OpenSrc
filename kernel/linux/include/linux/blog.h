#ifndef __BLOG_H_INCLUDED__
#define __BLOG_H_INCLUDED__

/*
<:copyright-gpl

 Copyright 2003 Broadcom Corp. All Rights Reserved.

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

/*
 *******************************************************************************
 *
 * File Name  : blog.h
 *
 * Description: This file implements the interface for debug logging of protocol
 *              header information of a packet buffer as it traverses the Linux
 *              networking stack. The header information is logged into a
 *              buffer log 'Blog_t' object associated with the sk_buff that
 *              is managing the packet. Logging is only performed when a sk_buff
 *              has an associated Blog_t object.
 *
 *              Uses the nbuff APIs for isolation of OS native networking buffer
 *              e.g. Linux sk_buff and VxWorks BSD style mbuf.
 *
 *              For Linux:
 *              When an sk_buff is cloned or copied, the associated Blog_t
 *              object is transfered to the new sk_buff.
 *
 *              Interface to log receive and transmit net_device specific 
 *              information is also provided.
 *
 *              Logging includes L3 IPv4 tuple information and L4 port number.
 *
 *              Mechanism to skip further logging, when non-interesting scenario
 *              such as, non-ipv4, ipv4 flow has associated helper, etc is also
 *              provided.
 *
 *              The three main interfaces are: blog_init(), blog_emit(), blog().
 *
 *              Two versions of blog_init interfaces are:
 *                  blog_sinit(struct sk_buff * skb_p, ...)
 *                  blog_finit(struct fkbuff * fkb_p, ...)
 *
 *              blog_init() will setup the L1 coarse key<channel,phy> and invoke
 *              the receive blog hook. If a NULL receive hook is configured,
 *              then PKT_NORM is returned. The receive hook may allocate a
 *              Blog_t object and associate with the FkBuff_t. The caller of
 *              blog_init is responsible to free the Blog_t object if the packet
 *              is not passed up to the stack via netif_receive_skb(), using
 *              the api fkb_put().
 *
 *              blog_emit() will invoke the transmit blog hook and subsequently
 *              dis-associate the Blog_t object from the skb, and recycle.
 *
 *              Physical Network devices may be instrumented as follows:
 *              - prior to netif_receive_skb() invoke blog_init().
 *              - in hard_start_xmit, prior to initiating a dma transfer,
 *                invoke blog_emit().
 *
 *              A receive and transmit blog hook is provided. These hooks
 *              may be initialized to do some basic processing when a packet
 *              is received or transmitted via a physical network device. By
 *              default, these hooks are NULL and hence no logging occurs.
 *              PS. blog_init() will associate a Blog_t only if the rx hook is
 *              defined.
 *              The receive hook may return an action PKT_DONE signifying that
 *              the packet has been consumed and no further processing via an
 *              skb is needed.
 *
 *              The receive function may be used to prototype various form of
 *              DOS/LAND attacks, experimental rate control algorithms, tracing
 *              and network monitoring tools, traffic generators, etc.
 *
 *              blog() may be inserted in various L2 protocol decoders/encoders
 *              to record the L2 header information. blog() may also be used to
 *              record the IP tuple.
 *
 *              While blog may be used as a simple tracing tool, a traffic
 *              generator to analyze the networking path may be envisioned,
 *              wherein one (or more) packets are recycled from tx to rx.
 *              A connection may be tracked by netfilter conntrack which
 *              expects packets to keep itself refreshed. If the traffic
 *              needs to hold on to the packets to insert a burst followed
 *              by very large idle periods, it should refresh the conntrack.
 *              Likewise a conntrack may be destroyed, and the traffic flow
 *              would be notified using a flow key saved in the conntrack.
 *
 * Engineering: The data structues are defined to pack each blog into 9 16byte
 *              cachelines. Layout organized for data locality.
 *
 * Implementation: Most internal Blog APIs operate on sk_buff.
 *              fkbuff variant APIs that are provided are:
 *                  blog_fkb(), blog_fnull(), blog_finit()
 *
 *  Version 1.0 SKB based blogging
 *  Version 2.0 NBuff/FKB based blogging (mbuf)
 *  Version 2.1 IPv6 Support
 *
 *******************************************************************************
 */

#define BLOG_VERSION                "v2.1"

#include <linux/autoconf.h>         /* LINUX kernel configuration */
#include <linux/types.h>            /* LINUX ISO C99 7.18 Integer types */

#ifndef NULL_STMT
#define NULL_STMT                   do { /* NULL BODY */ } while (0)
#endif

#undef  BLOG_DECL
#define BLOG_DECL(x)                x,

/*
 *------------------------------------------------------------------------------
 * Layer 2 encapsulations logged.
 * Implementation constraint: max 8 proto types.
 *------------------------------------------------------------------------------
 */

typedef enum {
        BLOG_DECL(BCM_XPHY)         /* e.g. BLOG_XTMPHY, BLOG_GPONPHY */
        BLOG_DECL(BCM_SWC)          /* BRCM LAN Switch Tag/Header */
        BLOG_DECL(ETH_802x)         /* Ethernet */
        BLOG_DECL(VLAN_8021Q)       /* Vlan 8021Q (incld stacked) */
        BLOG_DECL(PPPoE_2516)       /* PPPoE RFC 2516 */
        BLOG_DECL(PPP_1661)         /* PPP RFC 1661 */
        BLOG_DECL(L3_IPv4)          /* L3 IPv4 */
        BLOG_DECL(L3_IPv6)          /* L3 IPv6 */
        BLOG_DECL(PROTO_MAX)
} BlogEncap_t;

                                            /* First encapsulation type */
#define TYPE_ETH                    0x0000  /* LAN: ETH, WAN: EoA, MER, PPPoE */
#define TYPE_PPP                    0x0001  /*           WAN: PPPoA */
#define TYPE_IP                     0x0002  /*           WAN: IPoA */

                                            /* Ethernet Encapsulations */
#define TYPE_ETH_P_IP               0x0800  /* IPv4 in Ethernet */
#define TYPE_ETH_P_IPV6             0x86DD  /* IPv6 in Ethernet */
#define TYPE_ETH_P_8021Q            0x8100  /* VLAN in Ethernet */
#define TYPE_ETH_P_8021AD           0x88A8
#define TYPE_ETH_P_PPP_DIS          0x8863  /* PPPoE Discovery in Ethernet */
#define TYPE_ETH_P_PPP_SES          0x8864  /* PPPoE Session in Ethernet */
#define TYPE_ETH_P_BCM              0x8874  /* BCM Switch Hdr */
#define TYPE_ETH_P_MPLS_UC          0x8847  /* MPLS Unicast */
#define TYPE_ETH_P_MPLS_MC          0x8848  /* MPLS Multicast */

                                            /* PPP Encapsulations */
#define TYPE_PPP_IP                 0x0021  /* IPv4 in PPP */
#define TYPE_PPP_IPV6               0x0057  /* IPv6 in PPP */
#define TYPE_PPP_IPCP               0x8021  /* PPP IP Control Protocol */
#define TYPE_PPP_LCP                0xC021  /* PPP Link Control Protocol */
#define TYPE_PPP_MP                 0x003D  /* PPP Multilink Protocol */
#define TYPE_PPP_IPV6CP             0x8057  /* PPP IPv6 Control Protocol */
#define TYPE_PPP_MPLSCP             0x80FD  /* PPP MPLS Control Protocol */
#define TYPE_PPP_MPLS_UC            0x0281  /* PPP MPLS Unicast */
#define TYPE_PPP_MPLS_MC            0x0283  /* PPP MPLS Multicast */


typedef enum {
        BLOG_DECL(PKT_DONE)         /* packet consumed and freed */
        BLOG_DECL(PKT_NORM)         /* continue normal stack processing */
        BLOG_DECL(PKT_BLOG)         /* continue stack with blogging */
} BlogAction_t;

typedef enum {
        BLOG_DECL(BSTATS_CLR)
        BLOG_DECL(BSTATS_NOCLR)
} StatsClr_t;

typedef enum {
        BLOG_DECL(DIR_RX)           /* Receive path */
        BLOG_DECL(DIR_TX)           /* Transmit path */
        BLOG_DECL(DIR_MAX)
} BlogDir_t;


typedef enum {
    BLOG_DECL(BLOG_EVENT_STOP)      /* Destruction of a flow */
    BLOG_DECL(BLOG_EVENT_DTOS)      /* Dynamic tos change notification */
    BLOG_DECL(BLOG_EVENT_MAX)
} BlogEvent_t;

/*
 *------------------------------------------------------------------------------
 * RFC 2684 header logging.
 * CAUTION: 0'th enum corresponds to either header was stripped or zero length
 *          header. VC_MUX_PPPOA and VC_MUX_IPOA have 0 length RFC2684 header.
 *          PTM does not have an rfc2684 header.
 *------------------------------------------------------------------------------
 */
typedef enum {
        BLOG_DECL(RFC2684_NONE)         /*                               */
        BLOG_DECL(LLC_SNAP_ETHERNET)    /* AA AA 03 00 80 C2 00 07 00 00 */
        BLOG_DECL(LLC_SNAP_ROUTE_IP)    /* AA AA 03 00 00 00 08 00       */
        BLOG_DECL(LLC_ENCAPS_PPP)       /* FE FE 03 CF                   */
        BLOG_DECL(VC_MUX_ETHERNET)      /* 00 00                         */
        BLOG_DECL(VC_MUX_IPOA)          /*                               */
        BLOG_DECL(VC_MUX_PPPOA)         /*                               */
        BLOG_DECL(PTM)                  /*                               */
        BLOG_DECL(RFC2684_MAX)
} Rfc2684_t;

typedef enum {
    BLOG_DECL(BLOG_PHY_NONE)
    BLOG_DECL(BLOG_XTMPHY_LLC_SNAP_ETHERNET)
    BLOG_DECL(BLOG_XTMPHY_LLC_SNAP_ROUTE_IP)
    BLOG_DECL(BLOG_XTMPHY_LLC_ENCAPS_PPP)
    BLOG_DECL(BLOG_XTMPHY_VC_MUX_ETHERNET)
    BLOG_DECL(BLOG_XTMPHY_VC_MUX_IPOA)
    BLOG_DECL(BLOG_XTMPHY_VC_MUX_PPPOA)
    BLOG_DECL(BLOG_XTMPHY_PTM)
    BLOG_DECL(BLOG_ENETPHY)
    BLOG_DECL(BLOG_GPONPHY)
    BLOG_DECL(BLOG_USBPHY)
    BLOG_DECL(BLOG_WLANPHY)
    BLOG_DECL(BLOG_MAXPHY)
} BlogPhy_t;

/* Forward declarations */
struct sk_buff;
struct net_device;
struct nf_conn;
struct net_bridge_fdb_entry;
struct blog_t;
typedef struct blog_t Blog_t;

struct fkbuff;  /* linux/nbuff.h */

#define BLOG_NULL                   ((Blog_t*)NULL)

/* RFC 4008 */
extern uint32_t blog_nat_tcp_def_idle_timeout;
extern uint32_t blog_nat_udp_def_idle_timeout;

#define MAX_VIRT_DEV           4

#define DEV_DIR_MASK           0x3u
#define DEV_PTR_MASK           (~DEV_DIR_MASK)
#define DEV_DIR(ptr)           ((uint32_t)(ptr) & DEV_DIR_MASK)

#define IS_RX_DIR(ptr)         ( DEV_DIR(ptr) == DIR_RX )
#define IS_TX_DIR(ptr)         ( DEV_DIR(ptr) == DIR_TX )

/*
 *------------------------------------------------------------------------------
 * Device pointer conversion between with and without embeded direction info
 *------------------------------------------------------------------------------
 */
#define DEVP_APPEND_DIR(ptr,dir) \
            ( (struct net_device *) ((uint32_t)(ptr)   | (uint32_t)(dir)) )
#define DEVP_DETACH_DIR(ptr)       \
            ( (struct net_device *) ((uint32_t)(ptr) & (uint32_t)DEV_PTR_MASK) )

/*
 *------------------------------------------------------------------------------
 * Blog statistics structure
 *------------------------------------------------------------------------------
 */
typedef struct{
    unsigned long	rx_packets;		/* total blog packets received	*/
    unsigned long	tx_packets;		/* total blog packets transmitted	*/
    unsigned long	rx_bytes;		/* total blog bytes received 	*/
    unsigned long	tx_bytes;		/* total blog bytes transmitted	*/
    unsigned long	multicast;		/* total blog multicast packets	*/
}BlogStats_t;

#if defined(CONFIG_BLOG)

/* LAB ONLY: Design development */
// #define CC_CONFIG_BLOG_COLOR
// #define CC_CONFIG_BLOG_DEBUG

#define BLOG_ENCAP_MAX              6       /* Maximum number of L2 encaps */
#define BLOG_HDRSZ_MAX              32      /* Maximum size of L2 encaps */

#define BLOG_ENET_MTU               1500    /* Initial minMtu value */

/*
 * Engineering constants: Pre-allocated pool size 400 blogs Ucast+Mcast
 *
 * Extensions done in #blogs carved from a 2x4K page (external fragmentation)
 * IPv6: Blog size = 208, 8192/208 = 39 extension 80bytes internal fragmentation
 * IPv4: Blog size = 160, 8192/160 = 51 extension 32bytes internal fragmentation
 *
 * Number of extensions engineered to permit approximately another 400 blogs.
 */
#define CC_SUPPORT_BLOG_EXTEND              /* Conditional compile            */
#define BLOG_POOL_SIZE_ENGG         400     /* Pre-allocated pool size        */

#if defined(CONFIG_BLOG_IPV6)
#define BLOG_EXTEND_SIZE_ENGG       39      /* Number of Blog_t per extension */
#define BLOG_EXTEND_MAX_ENGG        10      /* Maximum extensions allowed     */
#else
#define BLOG_EXTEND_SIZE_ENGG       51      /* Number of Blog_t per extension */
#define BLOG_EXTEND_MAX_ENGG         8      /* Maximum extensions allowed     */
#endif


/* Support blogging of multicast packets */
#ifdef CONFIG_BLOG_MCAST
#define CC_SUPPORT_BLOG_MCAST          1
#else
#define CC_SUPPORT_BLOG_MCAST          0
#endif
extern int blog_mcast_g;
extern void blog_mcast(int enable);

typedef uint16_t hProto_t;

struct bcmhdr {
    uint32_t brcm_tag;
    uint16_t h_proto;
} __attribute__((packed));

extern const uint8_t rfc2684HdrLength[];
extern const uint8_t rfc2684HdrData[][16];
extern const char    * strRfc2684[];    /* for debug printing */

typedef struct {
    uint8_t             channel;        /* e.g. port number, txchannel, ... */
    uint8_t             count;          /* Number of L2 encapsulations */

    struct {
        uint8_t             hw_support  :1;     /* e.g. hw acceleration */
        uint8_t             wan_qdisc   :1;     /* device type */
        uint8_t             multicast   :1;     /* multicast flag */
        uint8_t             fkbInSkb    :1;     /* fkb from skb */
        uint8_t             phyHdr      :4;     /* e.g. Rfc2684_t hdr type */
    };

    union {
        struct {
            uint8_t         L3_IPv6     : 1;
            uint8_t         L3_IPv4     : 1;
            uint8_t         PPP_1661    : 1;
            uint8_t         PPPoE_2516  : 1;
            uint8_t         VLAN_8021Q  : 1;    /* also 8021Qad stacked */
            uint8_t         ETH_802x    : 1;
            uint8_t         BCM_SWC     : 1;
            uint8_t         BCM_XPHY    : 1;    /* e.g. BCM_XTM */
        }               bmap;/* as per order of BlogEncap_t enums declaration */
        uint8_t         hdrs;
    };
} BlogInfo_t;

/*
 *------------------------------------------------------------------------------
 * Buffer to log IP Tuple.
 * Packed: 1 16byte cacheline.
 *------------------------------------------------------------------------------
 */
struct blogTuple_t {
    uint32_t        saddr;          /* IP header saddr */
    uint32_t        daddr;          /* IP header daddr */

    union {
        struct {
            uint16_t    source;     /* L4 source port */
            uint16_t    dest;       /* L4 dest port */
        }           port;
        uint32_t    ports;
    };

    uint8_t         ttl;            /* IP header ttl */
    uint8_t         tos;            /* IP header tos */
    uint16_t        check;          /* checksum: rx tuple=l3, tx tuple=l4 */

} ____cacheline_aligned;
typedef struct blogTuple_t BlogTuple_t;

#if defined(CONFIG_BLOG_IPV6)
#define NEXTHDR_IPV4 IPPROTO_IPIP

#define HDRS_IPinIP     (( 1 << L3_IPv4 ) | ( 1 << L3_IPv6 )) // 0xC0
#define RX_IPinIP(b)    (((b)->rx.info.hdrs & HDRS_IPinIP)==HDRS_IPinIP)
#define TX_IPinIP(b)    (((b)->tx.info.hdrs & HDRS_IPinIP)==HDRS_IPinIP)
#define RX_IPV6(b)      ((b)->rx.info.bmap.L3_IPv6)
#define RX_IPV4(b)      ((b)->rx.info.bmap.L3_IPv4)
#define TX_IPV6(b)      ((b)->tx.info.bmap.L3_IPv6)
#define TX_IPV4(b)      ((b)->tx.info.bmap.L3_IPv4)

#define T4in6UP(b)      (RX_IPV4(b) && TX_IPinIP(b))
#define T4in6DN(b)      (RX_IPinIP(b) && TX_IPV4(b))

typedef struct ip6_addr {
    union {
        uint8_t     p8[16];
        uint16_t    p16[8];
        uint32_t    p32[4];
    };
} ip6_addr_t;

/*
 *------------------------------------------------------------------------------
 * Buffer to log IPv6 Tuple.
 *------------------------------------------------------------------------------
 */
struct blogTupleV6_t {
    union {
        uint32_t    word0;
        struct {
            uint32_t ver:4; 
            uint32_t tclass:8; 
            uint32_t flow_label:20;
        };
    };

    union {
        uint32_t    word1;
        struct {
            uint16_t length; 
            uint8_t next_hdr; 
            uint8_t hop_limit;
        };
    };

    ip6_addr_t      saddr;
    ip6_addr_t      daddr;

    union {
        struct {
            uint16_t    source;     /* L4 source port */
            uint16_t    dest;       /* L4 dest port */
        }           port;
        uint32_t    ports;
    };

    uint16_t        exthdrs; /* Bit field of IPv6 extension headers */
    uint16_t        t4in6offset; /* 4in6 tunnel */

} ____cacheline_aligned;
typedef struct blogTupleV6_t BlogTupleV6_t;

/* Layout of IPv6 Extension header (multiple of 8octet) for host endian access*/
typedef struct Ipv6ExtHdr {
    union {
        uint32_t    word0;
        struct {
            uint8_t next_hdr; uint8_t hdr_len; uint16_t u16;
        };
    };
    uint32_t    word1;
} Ipv6ExtHdr_t; /* First Octet */

#endif  /* defined(CONFIG_BLOG_IPV6) */


/*
 *------------------------------------------------------------------------------
 * Buffer to log Layer 2 and IP Tuple headers.
 * Packed: 4 16byte cachelines
 *------------------------------------------------------------------------------
 */
struct blogHeader_t {

    BlogTuple_t         tuple;          /* L3+L4 IP Tuple log */

    union {
        struct net_device * dev_p;
        struct nf_conn    * ct_p;
    };

    union {
        BlogInfo_t      info;
        uint32_t        word;           /* channel, count, rfc2684, bmap */
        uint32_t        pktlen;         /* stats info */
    };

    union {
        uint8_t         reserved;
        uint8_t         nf_dir;
    };
    uint8_t             length;         /* L2 header total length */
    uint8_t /*BlogEncap_t*/ encap[ BLOG_ENCAP_MAX ];/* All L2 header types */

    uint8_t             l2hdr[ BLOG_HDRSZ_MAX ];    /* Data of all L2 headers */

} ____cacheline_aligned;

typedef struct blogHeader_t BlogHeader_t;           /* L2 and L3+4 tuple */

/* Coarse key: L1, L3, L4 hash */
union blogKey_t {
    uint32_t        match;
    struct {
        uint8_t     hash;               /* Hash of Rx IP tuple */
        uint8_t     protocol;           /* IP protocol */

        struct {
            uint8_t channel;
            uint8_t phy;
        } l1_tuple;
    };
};

typedef union blogKey_t BlogKey_t;

/*
 *------------------------------------------------------------------------------
 * Buffer log structure.
 * Packed: 10 16 byte cachelines, 160bytes per blog.
 *------------------------------------------------------------------------------
 */
struct blog_t {

    union {
        void            * void_p;
        struct blog_t   * blog_p;       /* Free list of Blog_t */
        struct sk_buff  * skb_p;        /* Associated sk_buff */
    };
    BlogKey_t           key;            /* Coarse search key */
    uint32_t            mark;           /* NF mark value on tx */
    uint32_t            priority;       /* Tx  priority */

    struct net_bridge_fdb_entry * fdb_src;
    struct net_bridge_fdb_entry * fdb_dst;
    int8_t              delta[MAX_VIRT_DEV];  /* octet delta info */
    uint32_t            minMtu;

    /* pointers to the devices which the flow goes thru */
    struct net_device * virt_dev_p[MAX_VIRT_DEV];

    BlogHeader_t        tx;             /* Transmit path headers */
    BlogHeader_t        rx;             /* Receive path headers */

#if defined(CONFIG_BLOG_IPV6)
    BlogTupleV6_t       tupleV6;        /* L3+L4 IP Tuple log */
#endif

} ____cacheline_aligned;


/*
 * -----------------------------------------------------------------------------
 *
 * Blog defines four hooks:
 *
 *  RX Hook: If this hook is defined then blog_init() will pass the packet to
 *           the Rx Hook using the FkBuff_t context. L1 and encap information
 *           are passed to the receive hook. The private network device context 
 *           may be extracted using the passed net_device object, if needed.
 *
 *  TX Hook: If this hook is defined then blog_emit() will check to see whether
 *           the NBuff has a Blog_t, and if so pass the NBuff and Blog to the
 *           bound Tx hook.
 *
 *  NotifHook: When blog_notify is invoked, the bound hook is invoked. Based on
 *           event type the bound application may perform a custom action. E.g.
 *           Use Stop event, to stop associated traffic flow when a conntrack
 *           is destroyed. 
 *
 *  StatsHook: When blog_gstats() is invoked, the bound hook is invoked.
 *            Use of Stats hook to record statistics of associated traffic.
 *
 * -----------------------------------------------------------------------------
 */
typedef BlogAction_t (* BlogHook_t)(void * fkb_skb_p, struct net_device * dev_p,
                                    uint32_t encap, uint32_t blogKey);
typedef void (* BlogNotify_t)(struct net_bridge_fdb_entry * fdb_p,
                              struct nf_conn * ct_p, uint32_t event);
typedef void (* BlogStFnc_t)(struct net_device * dev_p, BlogStats_t *stats, 
                             StatsClr_t clr);
extern void blog_bind(BlogHook_t rx_hook, BlogHook_t tx_hook,
                      BlogStFnc_t stats_hook, BlogNotify_t xx_hook);

/*
 * -----------------------------------------------------------------------------
 * Blog functional interface
 * -----------------------------------------------------------------------------
 */

/*
 *------------------------------------------------------------------------------
 * BLOG to Netfilter Conntrack interface for flows tracked by Netfilter.
 * Associate a nf_conn with an skb's blog object.
 * Associate a traffic flow key with each direction of a blogged conntrack.
 * - Down call blog_notify() invoked when a conntrack is destroyed.
 * - Up   call blog_time() invoke to refresh a conntrack.
 *------------------------------------------------------------------------------
 */

/* Log a Netfilter Conntrack and events into a blog on nf_conntrack_in */
extern void blog_nfct(struct sk_buff * skb_p, struct nf_conn * nfct_p);

/* Log a bridge forward info into a blog at br_handle_frame_finish */
extern void blog_br_fdb(struct sk_buff * skb_p, 
                        struct net_bridge_fdb_entry* fdb_src,
                        struct net_bridge_fdb_entry* fdb_dst);
/*
 * Bind a traffic flow to blogged conntrack using a 16bit traffic flow key.
 */
extern void blog_flow(Blog_t * blog_p, uint32_t key);
extern void blog_notify(struct net_bridge_fdb_entry * fdb_p,
                        struct nf_conn * nfct_p, uint32_t event);
/* Get the statistics of the associated traffic */
extern void blog_gstats(struct net_device * dev_p, BlogStats_t *bstats_p, 
                        StatsClr_t clr);

/* Update the statistics of the associated traffic */
extern void blog_pstats(struct net_device * dev_p, BlogStats_t *bstats_p ); 

/* Refresh a blogged conntrack on behalf of associated traffic flow */
extern void blog_time(Blog_t * blog_p);
typedef void (*blog_refresh_t)(struct nf_conn * nfct, uint32_t ctinfo,
                               struct sk_buff * skb_p,
                               uint32_t jiffies, int do_acct);
extern blog_refresh_t blog_refresh_fn;

/* Refresh a blogged bridge forward entry on behalf of associated flow */
extern void blog_refresh_br( Blog_t * blog_p );

#define BLOG(skb_p, dir, encap, len, hdr_p)                         \
        do {                                                        \
            if ( skb_p->blog_p )                                    \
                blog( skb_p, dir, encap, len, hdr_p );              \
        } while(0)

/* Debug display of enums */
extern const char * strBlogAction[];
extern const char * strBlogEncap[];

#else   /* else ! defined(CONFIG_BLOG) */

#define blog_nfct(skb, nfct)                    NULL_STMT
#define blog_refresh_br(blog)                   NULL_STMT
#define blog_br_fdb(skb, fdb_src, fdb_dst)      NULL_STMT
#define blog_flow(blog, key)                    NULL_STMT
#define blog_notify(fdb,nfct,event)             NULL_STMT
#define blog_time(blog)                         NULL_STMT
#define blog_gstats(dev,stats,clr)              NULL_STMT
#define blog_pstats(dev,stats)                  NULL_STMT

#define BLOG(skb, dir, encap, len, hdr)         NULL_STMT

#define blog_bind(rx_hook, tx_hook, stats_hook, xx_hook)  NULL_STMT

#endif  /* ! defined(CONFIG_BLOG) */

/* Free a Blog_t */
void blog_put(Blog_t * blog_p);

/* Allocate a Blog_t and associate with sk_buff or fkbuff */
extern Blog_t * blog_fkb(struct fkbuff  * fkb_p);

/* Clear association of Blog_t with sk_buff */
extern Blog_t * blog_snull(struct sk_buff * skb_p);
extern Blog_t * blog_fnull(struct fkbuff  * fkb_p);

/* Clear association of Blog_t with sk_buff and free Blog_t object */
extern void blog_free(struct sk_buff * skb_p);

/* Dump a Blog_t object */
extern void blog_dump(Blog_t * blog_p);

/* Disable further logging. Dis-associate with skb and free Blog object */
extern void blog_skip(struct sk_buff * skb_p);

/*
 * Transfer association of a Blog_t object between two sk_buffs or from fkbuff.
 * May be used to implement transfer of Blog_t object from one sk_buff to
 * another, e.g. to permit Blogging when sk_buffs are cloned. Currently,
 * when an sk_buff is cloned, any associated non-multicast blog-t object 
 * is cleared and freed explicitly !!!
 */
extern void blog_xfer(struct sk_buff * skb_p, const struct sk_buff * prev_p);

/* Duplicate a Blog_t object for another skb. */
extern void blog_clone(struct sk_buff * skb_p, const struct blog_t * prev_p);

/*
 *------------------------------------------------------------------------------
 * If rx hook is defined,
 *  blog_sinit(): initialize a fkb from skb, and pass to hook
 *          if packet is consumed, skb is released.
 *          if packet is blogged, the blog is associated with skb.
 *  blog_finit(): pass to hook
 *          if packet is to be blogged, the blog is associated with fkb.
 *------------------------------------------------------------------------------
 */
extern BlogAction_t blog_sinit(struct sk_buff *skb_p, struct net_device * dev_p,
                             uint32_t encap, uint32_t channel, uint32_t phyHdr);
extern BlogAction_t blog_finit(struct fkbuff *fkb_p, struct net_device * dev_p,
                             uint32_t encap, uint32_t channel, uint32_t phyHdr);

/*
 *------------------------------------------------------------------------------
 * If tx hook is defined, invoke tx hook, dis-associate and free Blog_t
 *------------------------------------------------------------------------------
 */
extern BlogAction_t blog_emit(void * nbuff_p, struct net_device * dev_p,
                             uint32_t encap, uint32_t channel, uint32_t phyHdr);

/* Log devices which the flow goes through */
extern void blog_dev(const struct sk_buff * skb_p, 
                     const struct net_device * dev_p,
                     BlogDir_t action, unsigned int length);

/* Logging of L2|L3 headers */
extern void blog(struct sk_buff * skb_p, BlogDir_t dir, BlogEncap_t encap,  
                 size_t len, void * data_p);

#endif /* defined(__BLOG_H_INCLUDED__) */
