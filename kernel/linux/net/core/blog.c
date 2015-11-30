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
 * File Name  : blog.c
 * Description: Implements the tracing of L2 and L3 modifications to a packet
 * 		buffer while it traverses the Linux networking stack.
 *******************************************************************************
 */

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/blog.h>
#include <linux/nbuff.h>

#if defined(CONFIG_BLOG)
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/if_ether.h>
#include <linux/if_pppox.h>
#include <linux/if_vlan.h>
#include <linux/in.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <net/ip.h>
#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)   
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_helper.h>
#endif //#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)   

#include "../bridge/br_private.h"

#ifdef CC_CONFIG_BLOG_COLOR
#define COLOR(clr_code)     clr_code
#else
#define COLOR(clr_code)
#endif
#define CLRb                COLOR("\e[0;34m")       /* blue */
#define CLRc                COLOR("\e[0;36m")       /* cyan */
#define CLRn                COLOR("\e[0m")          /* normal */
#define CLRerr              COLOR("\e[0;33;41m")    /* yellow on red */
#define CLRN                CLRn"\n"                /* normal newline */

/*--- globals ---*/

/* RFC4008 */
uint32_t blog_nat_tcp_def_idle_timeout = 86400 *HZ;  /* 5 DAYS */
uint32_t blog_nat_udp_def_idle_timeout = 300 *HZ;    /* 300 seconds */

EXPORT_SYMBOL(blog_nat_tcp_def_idle_timeout);
EXPORT_SYMBOL(blog_nat_udp_def_idle_timeout);

/* Debug macros */
int blog_dbg = 0;

#if defined(CC_CONFIG_BLOG_DEBUG)
#define blog_print(fmt, arg...)                                         \
    if ( blog_dbg )                                                     \
    printk( CLRc "BLOG %s :" fmt CLRN, __FUNCTION__, ##arg )
#define blog_assertv(cond)                                              \
    if ( !cond ) {                                                      \
        printk( CLRerr "BLOG ASSERT %s : " #cond CLRN, __FUNCTION__ );  \
        return;                                                         \
    }
#define blog_assertr(cond, rtn)                                         \
    if ( !cond ) {                                                      \
        printk( CLRerr "BLOG ASSERT %s : " #cond CLRN, __FUNCTION__ );  \
        return rtn;                                                     \
    }
#define BLOG_DBG(debug_code)    do { debug_code } while(0)
#else
#define blog_print(fmt, arg...) NULL_STMT
#define blog_assertv(cond)      NULL_STMT
#define blog_assertr(cond, rtn) NULL_STMT
#define BLOG_DBG(debug_code)    NULL_STMT
#endif

#define blog_error(fmt, arg...)                                         \
    printk( CLRerr "BLOG ERROR %s :" fmt CLRN, __FUNCTION__, ##arg)

#undef  BLOG_DECL
#define BLOG_DECL(x)        #x,         /* string declaration */

/*--- globals ---*/

int blog_mcast_g = CC_SUPPORT_BLOG_MCAST;
void blog_mcast(int enable) { blog_mcast_g = enable; }

/*
 * Traffic flow generator, keep conntrack alive during idle traffic periods
 * by refreshing the conntrack. Dummy sk_buff passed to nf_conn.
 * Netfilter may not be statically loaded.
 */
blog_refresh_t blog_refresh_fn;

struct sk_buff * nfskb_p = (struct sk_buff*) NULL;

/*----- Constant string representation of enums for print -----*/
const char * strBlogAction[3] =
{
    BLOG_DECL(PKT_DONE)
    BLOG_DECL(PKT_NORM)
    BLOG_DECL(PKT_BLOG)
};

const char * strBlogEncap[PROTO_MAX] =
{
    BLOG_DECL(BCM_XPHY)
    BLOG_DECL(BCM_SWC)
    BLOG_DECL(ETH_802x)
    BLOG_DECL(VLAN_8021Q)
    BLOG_DECL(PPPoE_2516)
    BLOG_DECL(PPP_1661)
    BLOG_DECL(L3_IPv4)
    BLOG_DECL(L3_IPv6)
};

const char * strIpctDir[] = {   /* in reference to enum ip_conntrack_dir */
    BLOG_DECL(DIR_ORIG)
    BLOG_DECL(DIR_RPLY)
    BLOG_DECL(DIR_UNKN)
};

const char * strBlogEvent[] = {
    BLOG_DECL(BLOG_EVENT_STOP)      /* Destruction of a flow */
    BLOG_DECL(BLOG_EVENT_DTOS)      /* Dynamic tos change notification */
    BLOG_DECL(BLOG_EVENT_MAX)
};

const char * strIpctStatus[] =  /* in reference to enum ip_conntrack_status */
{
    BLOG_DECL(EXPECTED)
    BLOG_DECL(SEEN_REPLY)
    BLOG_DECL(ASSURED)
    BLOG_DECL(CONFIRMED)
    BLOG_DECL(SRC_NAT)
    BLOG_DECL(DST_NAT)
    BLOG_DECL(SEQ_ADJUST)
    BLOG_DECL(SRC_NAT_DONE)
    BLOG_DECL(DST_NAT_DONE)
    BLOG_DECL(DYING)
    BLOG_DECL(FIXED_TIMEOUT)
    BLOG_DECL(BLOG)
};

/*
 *------------------------------------------------------------------------------
 * Support for RFC 2684 headers logging.
 *------------------------------------------------------------------------------
 */
const char * strRfc2684[RFC2684_MAX] =
{
        BLOG_DECL(RFC2684_NONE)         /*                               */
        BLOG_DECL(LLC_SNAP_ETHERNET)    /* AA AA 03 00 80 C2 00 07 00 00 */
        BLOG_DECL(LLC_SNAP_ROUTE_IP)    /* AA AA 03 00 00 00 08 00       */
        BLOG_DECL(LLC_ENCAPS_PPP)       /* FE FE 03 CF                   */
        BLOG_DECL(VC_MUX_ETHERNET)      /* 00 00                         */
        BLOG_DECL(VC_MUX_IPOA)          /*                               */
        BLOG_DECL(VC_MUX_PPPOA)         /*                               */
        BLOG_DECL(PTM)                  /*                               */
};

const uint8_t rfc2684HdrLength[BLOG_MAXPHY] =
{
     0, /* header was already stripped. :                               */
    10, /* LLC_SNAP_ETHERNET            : AA AA 03 00 80 C2 00 07 00 00 */
     8, /* LLC_SNAP_ROUTE_IP            : AA AA 03 00 00 00 08 00       */
     4, /* LLC_ENCAPS_PPP               : FE FE 03 CF                   */
     2, /* VC_MUX_ETHERNET              : 00 00                         */
     0, /* VC_MUX_IPOA                  :                               */
     0, /* VC_MUX_PPPOA                 :                               */
     0, /* PTM                          :                               */
     0, /* ENET                                                         */
     0, /* GPON                                                         */
     0, /* USB                                                          */
     0, /* WLAN                                                         */
};

const uint8_t rfc2684HdrData[RFC2684_MAX][16] =
{
    {},
    { 0xAA, 0xAA, 0x03, 0x00, 0x80, 0xC2, 0x00, 0x07, 0x00, 0x00 },
    { 0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00 },
    { 0xFE, 0xFE, 0x03, 0xCF },
    { 0x00, 0x00 },
    {},
    {},
    {}
};

const char * strBlogPhy[BLOG_MAXPHY] =
{
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
};

/*
 *------------------------------------------------------------------------------
 * Default Rx and Tx hooks.
 *------------------------------------------------------------------------------
 */
static BlogHook_t blog_rx_hook_g = (BlogHook_t)NULL;
static BlogHook_t blog_tx_hook_g = (BlogHook_t)NULL;
static BlogStFnc_t blog_st_hook_g = (BlogStFnc_t)NULL;
static BlogNotify_t blog_xx_hook_g = (BlogNotify_t)NULL;

/*
 *------------------------------------------------------------------------------
 * Network Utilities  : Network Order IP Address access (in Big Endian) format
 *------------------------------------------------------------------------------
 */
#define _IPFMT_                 "<%03u.%03u.%03u.%03u>"
#define _IPPFMT_                "<%03u.%03u.%03u.%03u:%u>"
#define _IP4_(ip)               ((uint8_t*)&ip)[0], ((uint8_t*)&ip)[1],     \
                                ((uint8_t*)&ip)[2], ((uint8_t*)&ip)[3]

#define _IP6FMT_                "<%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x>"
#define _IP6PFMT_               "<%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x:%u>"
#define _IP6_(ip)               ((uint16_t*)&ip)[0], ((uint16_t*)&ip)[1],   \
                                ((uint16_t*)&ip)[2], ((uint16_t*)&ip)[3],   \
                                ((uint16_t*)&ip)[4], ((uint16_t*)&ip)[5],   \
                                ((uint16_t*)&ip)[6], ((uint16_t*)&ip)[7]

/*
 *------------------------------------------------------------------------------
 * Function     : _read32_align16
 * Description  : Read a 32bit value from a 16 byte aligned data stream
 *------------------------------------------------------------------------------
 */
static inline uint32_t _read32_align16( uint16_t * from )
{
    return (__force uint32_t)( (from[0] << 16) | (from[1]) );
}

/*
 *------------------------------------------------------------------------------
 * Function     : _write32_align16
 * Description  : Write a 32bit value to a 16bit aligned data stream
 *------------------------------------------------------------------------------
 */
static inline void _write32_align16( uint16_t * to, uint32_t from )
{
    to[0] = (__force uint16_t)(from >> 16);
    to[1] = (__force uint16_t)(from >>  0);
}


/*
 *------------------------------------------------------------------------------
 * Blog_t Free Pool Management.
 * The free pool of Blog_t is self growing (extends upto an engineered
 * value). Could have used a kernel slab cache. 
 *------------------------------------------------------------------------------
 */

/* Global pointer to the free pool of Blog_t */
static Blog_t * blog_list_gp = BLOG_NULL;

static int blog_extends = 0;        /* Extension of Pool on depletion */
#if defined(CC_CONFIG_BLOG_DEBUG)
static int blog_cnt_free = 0;       /* Number of Blog_t free */
static int blog_cnt_used = 0;       /* Number of in use Blog_t */
static int blog_cnt_hwm  = 0;       /* In use high water mark for engineering */
static int blog_cnt_fails = 0;
#endif

/*
 *------------------------------------------------------------------------------
 * Function   : blog_extend
 * Description: Create a pool of Blog_t objects. When a pool is exhausted
 *              this function may be invoked to extend the pool. The pool is
 *              identified by a global pointer, blog_list_gp. All objects in
 *              the pool chained together in a single linked list.
 * Parameters :
 *   num      : Number of Blog_t objects to be allocated.
 * Returns    : Number of Blog_t objects allocated in pool.
 *------------------------------------------------------------------------------
 */
static uint32_t blog_extend( uint32_t num )
{
    register int i;
    register Blog_t * list_p;

    blog_print( "%u", num );

    list_p = (Blog_t *) kmalloc( num * sizeof(Blog_t), GFP_ATOMIC);
    if ( list_p == BLOG_NULL )
    {
#if defined(CC_CONFIG_BLOG_DEBUG)
        blog_cnt_fails++;
#endif
        blog_print( "WARNING: Failure to initialize %d Blog_t", num );
        return 0;
    }
    blog_extends++;

    /* memset( (void *)list_p, 0, (sizeof(Blog_t) * num ); */
    for ( i = 0; i < num; i++ )
        list_p[i].blog_p = &list_p[i+1];

    local_irq_disable();

    BLOG_DBG( blog_cnt_free += num; );
    list_p[num-1].blog_p = blog_list_gp; /* chain last Blog_t object */
    blog_list_gp = list_p;  /* Head of list */

    local_irq_enable();


    return num;
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_clr
 * Description  : Clear the data of a Blog_t
 *------------------------------------------------------------------------------
 */
static inline void blog_clr( Blog_t * blog_p )
{
    blog_assertv( ((blog_p != BLOG_NULL) && _IS_BPTR_(blog_p)) );
    BLOG_DBG( memset( (void*)blog_p, 0, sizeof(Blog_t) ); );

    /* clear phyHdr, count, bmap, and channel */
    blog_p->rx.word = 0;
    blog_p->tx.word = 0;
    blog_p->key.match = 0;    /* clears hash, protocol, l1_tuple */
    blog_p->rx.ct_p = (struct nf_conn*)NULL;
    blog_p->tx.dev_p = (struct net_device *)NULL;
    blog_p->fdb_src = (struct net_bridge_fdb_entry*)NULL;
    blog_p->fdb_dst = (struct net_bridge_fdb_entry*)NULL;
    blog_p->minMtu = BLOG_ENET_MTU;
    memset( (void*)blog_p->virt_dev_p, 0, 
            sizeof(struct net_device *) * MAX_VIRT_DEV);

    blog_print( "blog<0x%08x>", (int)blog_p );
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_get
 * Description  : Allocate a Blog_t from the free list
 * Returns      : Pointer to an Blog_t or NULL, on depletion.
 *------------------------------------------------------------------------------
 */
static Blog_t * blog_get( void )
{
    register Blog_t * blog_p;

    if ( blog_list_gp == BLOG_NULL )
    {
#ifdef CC_SUPPORT_BLOG_EXTEND
        if ( (blog_extends >= BLOG_EXTEND_MAX_ENGG)/* Try extending free pool */
          || (blog_extend( BLOG_EXTEND_SIZE_ENGG ) != BLOG_EXTEND_SIZE_ENGG))
        {
            blog_print( "WARNING: free list exhausted" );
            return BLOG_NULL;
        }
#else
        if ( blog_extend( BLOG_EXTEND_SIZE_ENGG ) == 0 )
        {
            blog_print( "WARNING: out of memory" );
            return BLOG_NULL;
        }
#endif
    }

    local_irq_disable();

    BLOG_DBG(
        blog_cnt_free--;
        if ( ++blog_cnt_used > blog_cnt_hwm )
            blog_cnt_hwm = blog_cnt_used;
        );
    blog_p = blog_list_gp;
    blog_list_gp = blog_list_gp->blog_p;

    local_irq_enable();

    blog_clr( blog_p );     /* quickly clear the contents */

    blog_print( "blog<0x%08x>", (int)blog_p );

    return blog_p;
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_put
 * Description  : Release a Blog_t back into the free pool
 * Parameters   :
 *  blog_p      : Pointer to a non-null Blog_t to be freed.
 *------------------------------------------------------------------------------
 */
void blog_put( Blog_t * blog_p )
{
    blog_assertv( ((blog_p != BLOG_NULL) && _IS_BPTR_(blog_p)) );
    blog_clr( blog_p );

    local_irq_disable();

    BLOG_DBG( blog_cnt_used--; blog_cnt_free++; );
    blog_p->blog_p = blog_list_gp;  /* clear pointer to skb_p */
    blog_list_gp = blog_p;          /* link into free pool */

    local_irq_enable();

    blog_print( "blog<0x%08x>", (int)blog_p );
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_fkb
 * Description  : Allocate and associate a Blog_t with an fkb.
 * Parameters   :
 *  fkb_p       : pointer to a non-null FkBuff_t
 * Returns      : A Blog_t object or NULL,
 *------------------------------------------------------------------------------
 */
Blog_t * blog_fkb( struct fkbuff * fkb_p )
{
    int in_skb_tag;
    blog_assertr( (fkb_p != (FkBuff_t *)NULL), BLOG_NULL );
    blog_assertr( (!_IS_BPTR_(fkb_p->blog_p)), BLOG_NULL ); /* avoid leak */

    in_skb_tag = (fkb_p->in_skb_tag == FKB_IN_SKB);

    fkb_p->blog_p = blog_get(); /* Allocate and associate with fkb */

    if ( fkb_p->blog_p != BLOG_NULL )   /* Move in_skb_tag to blog rx info */
        fkb_p->blog_p->rx.info.fkbInSkb = in_skb_tag;

    blog_print( "fkb<0x%08x> blog<0x%08x> in_skb_tag<%u>",
                (int)fkb_p, (int)fkb_p->blog_p, in_skb_tag );
    return fkb_p->blog_p;       /* May be null */
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_snull
 * Description  : Dis-associate a sk_buff with any Blog_t
 * Parameters   :
 *  skb_p       : Pointer to a non-null sk_buff
 * Returns      : Previous Blog_t associated with sk_buff
 *------------------------------------------------------------------------------
 */
inline Blog_t * _blog_snull( struct sk_buff * skb_p )
{
    register Blog_t * blog_p;
    blog_p = skb_p->blog_p;
    skb_p->blog_p = BLOG_NULL;
    return blog_p;
}

Blog_t * blog_snull( struct sk_buff * skb_p )
{
    blog_assertr( (skb_p != (struct sk_buff *)NULL), BLOG_NULL );
    blog_print( "skb<0x%08x> blog<0x%08x>", (int)skb_p, (int)skb_p->blog_p );
    return _blog_snull( skb_p );
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_fnull
 * Description  : Dis-associate a fkbuff with any Blog_t
 * Parameters   :
 *  fkb_p       : Pointer to a non-null fkbuff
 * Returns      : Previous Blog_t associated with fkbuff
 *------------------------------------------------------------------------------
 */
inline Blog_t * _blog_fnull( struct fkbuff * fkb_p )
{
    register Blog_t * blog_p;
    blog_p = fkb_p->blog_p;
    fkb_p->blog_p = BLOG_NULL;
    return blog_p;
}

Blog_t * blog_fnull( struct fkbuff * fkb_p )
{
    blog_assertr( (fkb_p != (struct fkbuff *)NULL), BLOG_NULL );
    blog_print( "fkb<0x%08x> blogp<0x%08x>", (int)fkb_p, (int)fkb_p->blog_p );
    return _blog_fnull( fkb_p );
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_free
 * Description  : Free any Blog_t associated with a sk_buff
 * Parameters   :
 *  skb_p       : Pointer to a non-null sk_buff
 *------------------------------------------------------------------------------
 */
inline void _blog_free( struct sk_buff * skb_p )
{
    register Blog_t * blog_p;
    blog_p = _blog_snull( skb_p );   /* Dis-associate Blog_t from skb_p */
    if ( likely(blog_p != BLOG_NULL) )
        blog_put( blog_p );         /* Recycle blog_p into free list */
}

void blog_free( struct sk_buff * skb_p )
{
    blog_assertv( (skb_p != (struct sk_buff *)NULL) );
    BLOG_DBG(
        if ( skb_p->blog_p != BLOG_NULL )
            blog_print( "skb<0x%08x> blog<0x%08x> [<%08x>]",
                        (int)skb_p, (int)skb_p->blog_p,
                        (int)__builtin_return_address(0) ); );
    _blog_free( skb_p );
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_skip
 * Description  : Disable further tracing of sk_buff by freeing associated
 *                Blog_t (if any)
 * Parameters   :
 *  skb_p       : Pointer to a sk_buff
 *------------------------------------------------------------------------------
 */
void blog_skip( struct sk_buff * skb_p )
{
    blog_print( "skb<0x%08x> [<%08x>]",
                (int)skb_p, (int)__builtin_return_address(0) );
    blog_assertv( (skb_p != (struct sk_buff *)NULL) );
    _blog_free( skb_p );
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_xfer
 * Description  : Transfer ownership of a Blog_t between two sk_buff(s)
 * Parameters   :
 *  skb_p       : New owner of Blog_t object 
 *  prev_p      : Former owner of Blog_t object
 *------------------------------------------------------------------------------
 */
void blog_xfer( struct sk_buff * skb_p, const struct sk_buff * prev_p )
{
    Blog_t * blog_p;
    struct sk_buff * mod_prev_p;
    blog_assertv( (prev_p != (struct sk_buff *)NULL) );
    blog_assertv( (skb_p != (struct sk_buff *)NULL) );

    mod_prev_p = (struct sk_buff *) prev_p; /* const removal without warning */
    blog_p = _blog_snull( mod_prev_p );
    skb_p->blog_p = blog_p;

    if ( likely(blog_p != BLOG_NULL) )
    {
        blog_print( "skb<0x%08x> to new<0x%08x> blog<0x%08x> [<%08x>]",
                    (int)prev_p, (int)skb_p, (int)blog_p,
                    (int)__builtin_return_address(0) );
        blog_assertv( _IS_BPTR_(blog_p) );
        blog_p->skb_p = skb_p;
    }
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_clone
 * Description  : Duplicate a Blog_t for another sk_buff
 * Parameters   :
 *  skb_p       : New owner of cloned Blog_t object 
 *  prev_p      : Blog_t object to be cloned
 *------------------------------------------------------------------------------
 */
void blog_clone( struct sk_buff * skb_p, const struct blog_t * prev_p )
{
    blog_assertv( (skb_p != (struct sk_buff *)NULL) );

    if ( likely(prev_p != BLOG_NULL) )
    {
        Blog_t * blog_p;
        
        blog_assertv( _IS_BPTR_(prev_p) );
        
        skb_p->blog_p = blog_get(); /* Allocate and associate with skb */
        blog_p = skb_p->blog_p;

        blog_print( "orig blog<0x%08x> new skb<0x%08x> blog<0x%08x> [<%08x>]",
                    (int)prev_p, (int)skb_p, (int)blog_p,
                    (int)__builtin_return_address(0) );

        if ( likely(blog_p != BLOG_NULL) )
        {
            blog_p->skb_p = skb_p;
#define CPY(x) blog_p->x = prev_p->x
            CPY(key.match);
            CPY(mark);
            CPY(priority);
            CPY(rx);
            blog_p->tx.word = 0;
        }
    }
}

#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)   

/*
 *------------------------------------------------------------------------------
 * Function     : blog_nfct [Down call from Netfilter]
 * Description  : Associate a nf_conn with an skb's blog object
 *                See: resolve_normal_ct() nf_conntrack_core.c
 * Parameters   :
 *  skb_p       : Pointer to a sk_buff
 *  ct_p        : Pointer to a nf_conn
 *------------------------------------------------------------------------------
 */
void blog_nfct( struct sk_buff * skb_p, struct nf_conn * ct_p )
{
    enum ip_conntrack_info ctinfo;
    struct nf_conn * ct;
    
    blog_assertv( (ct_p != (struct nf_conn *)NULL) );
    blog_assertv( (skb_p != (struct sk_buff *)NULL) );

    if ( unlikely(skb_p->blog_p == BLOG_NULL) )
        return;

    blog_assertv( _IS_BPTR_(skb_p->blog_p) );

    if ( unlikely(skb_p->blog_p->rx.info.multicast) )
        return;

    blog_print( "skb<0x%08x> blog<0x%08x> nfct<0x%08x> [<%08x>]",
                (int)skb_p, (int)skb_p->blog_p, (int)ct_p,
                (int)__builtin_return_address(0) );

    skb_p->blog_p->rx.ct_p = ct_p;                  /* Pointer to conntrack */

    ct = nf_ct_get(skb_p, &ctinfo);
    blog_assertv( (ct == ct_p) );
    skb_p->blog_p->rx.nf_dir = CTINFO2DIR(ctinfo);  /* Conntrack direction */
}
#endif

/*
 *------------------------------------------------------------------------------
 * Function     : blog_dev
 * Description  : Record devices which the flow goes through
 * Parameters   :
 *  skb_p       : Pointer to a sk_buff
 *  dev_p       : Pointer to a net_device
 *  action      : indicate recv or xmit
 *  length      : length update
 *------------------------------------------------------------------------------
 */
void blog_dev(const struct sk_buff * skb_p, const struct net_device * dev_p, 
              BlogDir_t action, unsigned int length)
{
    int i;
    Blog_t *blog_p = skb_p->blog_p;

    blog_assertv( ( (skb_p != (const struct sk_buff *)NULL) && 
                  (dev_p != (const struct net_device *)NULL) ) );
    blog_print( "dev<0x%08x> skb<0x%08x> blog<0x%08x> action<%08x>",
                (int)dev_p, (int)skb_p, (int)blog_p, action );

    if ( unlikely(blog_p == BLOG_NULL) )
        return;

    for (i=0; i<MAX_VIRT_DEV; i++)
    {
        /* A flow should not rx and tx with the same device!!  */
        blog_assertv( (dev_p != DEVP_DETACH_DIR(blog_p->virt_dev_p[i])) );

        if ( blog_p->virt_dev_p[i] == NULL )
        {
            blog_p->virt_dev_p[i] = DEVP_APPEND_DIR(dev_p, action);
            blog_p->delta[i] = length - blog_p->tx.pktlen;
            break;
        }
    }
    
    blog_assertv( ( i != MAX_VIRT_DEV ) );
}

#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)   

/*
 *------------------------------------------------------------------------------
 * Function     : blog_flow [Up call to Netfilter]
 * Description  : Associates a traffic flow key with the blogged nf_conntack
 * Parameters   :
 *  blog_p      : Pointer to a Blog_t
 *  key         : Key of the traffic flow
 *------------------------------------------------------------------------------
 */
void blog_flow( Blog_t * blog_p, uint32_t key )
{
    blog_assertv( ((blog_p != BLOG_NULL) && _IS_BPTR_(blog_p)) );
    blog_assertv( (blog_p->rx.ct_p != (struct nf_conn*)NULL) );
    blog_print( "blog<0x%08x> ct<0x%08x> key<%u> [<%08x>]",
                (int)blog_p, (int)blog_p->rx.ct_p, key,
                (int)__builtin_return_address(0) );
    blog_p->rx.ct_p->blog_key[ blog_p->rx.nf_dir ] = key;
}
#endif

/*
 *------------------------------------------------------------------------------
 * Function     : blog_br_fdb [Down call from bridge]
 * Description  : Associate a net_bridge_fdb_entry with an skb's blog object
 *                See: br_handle_frame_finish() br_input.c
 * Parameters   :
 *  skb_p       : Pointer to a sk_buff
 *  fdb_src     : Pointer to a net_bridge_fdb_entry of packet source
 *  fdb_dst     : Pointer to a net_bridge_fdb_entry of packet destination
 *------------------------------------------------------------------------------
 */
void blog_br_fdb( struct sk_buff * skb_p, struct net_bridge_fdb_entry * fdb_src,
                  struct net_bridge_fdb_entry * fdb_dst )
{
    blog_assertv( ((fdb_src != (struct net_bridge_fdb_entry *)NULL) || 
                   (fdb_dst != (struct net_bridge_fdb_entry *)NULL)) );
    blog_assertv( (skb_p != (struct sk_buff *)NULL) );

    if ( unlikely(skb_p->blog_p == BLOG_NULL) )
        return;

    blog_assertv( (_IS_BPTR_(skb_p->blog_p)) );

    blog_print("skb<0x%08x> blog<0x%08x> fdb_src<0x%08x> fdb_dst<0x%08x>"
               " [<%08x>]",
               (int)skb_p, (int)skb_p->blog_p, (int)fdb_src, (int)fdb_dst,
               (int)__builtin_return_address(0) );

    skb_p->blog_p->fdb_src = fdb_src;      /* Pointer to net_bridge_fdb_entry */
    skb_p->blog_p->fdb_dst = fdb_dst;      /* Pointer to net_bridge_fdb_entry */
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_notify [Down call from Netfilter/bridge]
 * Description  : Invokes the bound notify function passing either 
 *                      the flow key(netfilter) or net_bridge_fdb_entry(bridge)
 *                See destroy_conntrack() in nf_conntrack_core.c
 *                       fdb_delete() in br_fdb.c
 * Parameters   :
 *  fdb_p       : Pointer to a bridge forward database that is being destroyed.
 *  ct_p        : Pointer to a conntrack that is being destroyed.
 *  event       : Notification event, see BlogNotify_t
 *------------------------------------------------------------------------------
 */
void blog_notify( struct net_bridge_fdb_entry * fdb_p, struct nf_conn * ct_p,
                  uint32_t event )
{
    if ( likely(blog_xx_hook_g != (BlogNotify_t)NULL) )
    {
        blog_print( "fdb<0x%08x> ct<0x%08x> event<%s> [<%08x>]",
                    (int)fdb_p, (int)ct_p, (event < BLOG_EVENT_MAX) ?
                    strBlogEvent[event] : "UNKN_EVENT",
                    (int)__builtin_return_address(0) );

        local_irq_disable();
        blog_xx_hook_g( fdb_p, ct_p, event );
        local_irq_enable();
    }
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_time [Up call to Netfilter]
 * Description  : Refresh the nf_conn associated with this blog.
 *   blog_p     : Pointer to a blog
 *------------------------------------------------------------------------------
 */

#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)   
void blog_time( Blog_t * blog_p )
{
    if ( blog_refresh_fn  && (blog_p->rx.ct_p != (struct nf_conn *)NULL) )
    {
        uint32_t jiffies;

        if ( blog_p->key.protocol == IPPROTO_TCP )
            jiffies = blog_nat_tcp_def_idle_timeout;
        else if ( blog_p->key.protocol == IPPROTO_UDP )
            jiffies = blog_nat_udp_def_idle_timeout;

        nfskb_p->nfct = (struct nf_conntrack *)blog_p->rx.ct_p;
        (*blog_refresh_fn)( blog_p->rx.ct_p, 0, nfskb_p, jiffies, 0 );
        // blog_print( "blog<0x%08x> ct<0x%08x> [<%08x>]",
        //            (int)blog_p, (int)blog_p->rx.ct_p,
        //            (int)__builtin_return_address(0) );
    }
}
#endif

/*
 *------------------------------------------------------------------------------
 * Function     : blog_refresh_br [Up call to bridge]
 * Description  : Refresh the bridge forward entry associated with this blog.
 *   blog_p     : Pointer to a blog
 *------------------------------------------------------------------------------
 */
extern void br_fdb_refresh( struct net_bridge_fdb_entry *fdb ); /* br_fdb.c */
void blog_refresh_br( Blog_t * blog_p )
{
    if ( blog_p->fdb_src != (struct net_bridge_fdb_entry *)NULL )
    {
        br_fdb_refresh( blog_p->fdb_src );
        // blog_print( "blog<0x%08x> fdb_src<0x%08x> [<%08x>]",
        //             (int)blog_p, blog_p->fdb_src,
        //             (int)__builtin_return_address(0) );
    }
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_finit, blog_sinit
 * Description  : This function may be inserted in a physical network device's
 *                packet receive handler. A receive handler typically extracts
 *                the packet data from the rx DMA buffer ring, allocates and
 *                sets up a sk_buff, decodes the l2 headers and passes the
 *                sk_buff into the network stack via netif_receive_skb/netif_rx.
 *
 *                Prior to constructing a sk_buff, blog_finit() may be invoked
 *                using a fast kernel buffer to carry the received buffer's
 *                context <data,len>, and the receive net_device and l1 info.
 *
 *                This function invokes the bound receive blog hook.
 *
 * Parameters   :
 *  blog_finit() fkb_p: Pointer to a fast kernel buffer<data,len>
 *  blog_sinit() skb_p: Pointer to a Linux kernel skbuff
 *  dev_p       : Pointer to the net_device on which the packet arrived.
 *  encap       : First encapsulation type
 *  channel     : Channel/Port number on which the packet arrived.
 *  phyHdr      : e.g. XTM device RFC2684 header type
 *
 * Returns      :
 *  PKT_DONE    : The fkb|skb is consumed and device should not process fkb|skb.
 *
 *  PKT_NORM    : Device may invoke netif_receive_skb for normal processing.
 *                No Blog is associated and fkb reference count = 0.
 *                [invoking fkb_release() has no effect]
 *
 *  PKT_BLOG    : PKT_NORM behaviour + Blogging enabled.
 *                Must call fkb_release() to free associated Blog
 *
 *------------------------------------------------------------------------------
 */
inline
BlogAction_t blog_finit(struct fkbuff * fkb_p, struct net_device * dev_p,
                        uint32_t encap, uint32_t channel, uint32_t phyHdr )
{
    BlogKey_t blogKey;
    BlogAction_t action = PKT_NORM;

    blogKey.match = 0U;     /* also clears hash, protocol = 0 */

    if ( unlikely(blog_rx_hook_g == (BlogHook_t)NULL) )
        goto bypass;

    blogKey.l1_tuple.channel = (uint8_t)channel;
    blogKey.l1_tuple.phy     = phyHdr;

    blog_assertr( (phyHdr < BLOG_MAXPHY), PKT_NORM);
    blog_print( "fkb<0x%08x:%x> pData<0x%08x> length<%d> dev<0x%08x>"
                " chnl<%u> %s PhyHdrLen<%u> key<0x%08x>",
                (int)fkb_p, (fkb_p->in_skb_tag)? 1:0,
                (int)fkb_p->data, fkb_p->len, (int)dev_p,
                channel, strBlogPhy[phyHdr],
                (phyHdr <= BLOG_XTMPHY_PTM) ? rfc2684HdrLength[phyHdr] : 0,
                blogKey.match );

    action = blog_rx_hook_g( fkb_p, dev_p, encap, blogKey.match );

    if ( unlikely(action == PKT_NORM) )
        fkb_release( fkb_p );

bypass:
    return action;
}

/*
 * blog_sinit serves as a wrapper to blog_finit() by overlaying an fkb into a
 * skb and invoking blog_finit().
 */
BlogAction_t blog_sinit(struct sk_buff * skb_p, struct net_device * dev_p,
                        uint32_t encap, uint32_t channel, uint32_t phyHdr)
{
    struct fkbuff * fkb_p;
    BlogAction_t action = PKT_NORM;

    if ( unlikely(blog_rx_hook_g == (BlogHook_t)NULL) )
        goto bypass;

    blog_assertr( (phyHdr < BLOG_MAXPHY), PKT_NORM);
    blog_print( "skb<0x%08x> pData<0x%08x> length<%d> dev<0x%08x>"
                " chnl<%u> %s PhyHdrLen<%u>",
                (int)skb_p, (int)skb_p->data, skb_p->len, (int)dev_p,
                channel, strBlogPhy[phyHdr],
                (phyHdr <= BLOG_XTMPHY_PTM) ? rfc2684HdrLength[phyHdr] : 0 );

    /* CAUTION: Tag that the fkbuff is from sk_buff */
    fkb_p = (FkBuff_t *) &skb_p->fkbInSkb;
    fkb_p->in_skb_tag = FKB_IN_SKB;

    action = blog_finit( fkb_p, dev_p, encap, channel, phyHdr );

bypass:
    return action;
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_emit
 * Description  : This function may be inserted in a physical network device's
 *                hard_start_xmit function just before the packet data is
 *                extracted from the sk_buff and enqueued for DMA transfer.
 *
 *                This function invokes the transmit blog hook.
 * Parameters   :
 *  nbuff_p     : Pointer to a NBuff
 *  dev_p       : Pointer to the net_device on which the packet is transmited.
 *  encap       : First encapsulation type
 *  channel     : Channel/Port number on which the packet is transmited.
 *  phyHdr      : e.g. XTM device RFC2684 header type
 *
 * Returns      :
 *  PKT_DONE    : The skb_p is consumed and device should not process skb_p.
 *  PKT_NORM    : Device may use skb_p and proceed with hard xmit 
 *                Blog object is disassociated and freed.
 *------------------------------------------------------------------------------
 */
BlogAction_t blog_emit( void * nbuff_p, struct net_device * dev_p,
                        uint32_t encap, uint32_t channel, uint32_t phyHdr )
{
    BlogKey_t blogKey;
    struct sk_buff * skb_p;
    Blog_t * blog_p;
    BlogAction_t action = PKT_NORM;

    blog_assertr( (nbuff_p != (void *)NULL), PKT_NORM );

    if ( !IS_SKBUFF_PTR(nbuff_p) )
        goto bypass;
    skb_p = PNBUFF_2_SKBUFF(nbuff_p);   /* same as nbuff_p */

    blog_p = skb_p->blog_p;
    if ( blog_p == BLOG_NULL )
        goto bypass;

    blog_assertr( (_IS_BPTR_(blog_p)), PKT_NORM );

    blogKey.match = 0U;

    if ( likely(blog_tx_hook_g != (BlogHook_t)NULL) )
    {
        blog_p->tx.dev_p = dev_p;                   /* Log device info */

        blogKey.l1_tuple.channel = (uint8_t)channel;
        blogKey.l1_tuple.phy     = phyHdr;

        blog_p->priority = skb_p->priority;         /* Log skb info */
        blog_p->mark = skb_p->mark;

        blog_assertr( (phyHdr < BLOG_MAXPHY), PKT_NORM);
        blog_print( "skb<0x%08x> blog<0x%08x> pData<0x%08x> length<%d>"
                    " dev<0x%08x> chnl<%u> %s PhyHdrLen<%u> key<0x%08x>",
            (int)skb_p, (int)blog_p, (int)skb_p->data, skb_p->len,
            (int)dev_p, channel, strBlogPhy[phyHdr],
            (phyHdr <= BLOG_XTMPHY_PTM) ? rfc2684HdrLength[phyHdr] : 0,
            blogKey.match );

        action = blog_tx_hook_g( skb_p, skb_p->dev, encap, blogKey.match );
    }

    blog_free( skb_p );                             /* Dis-associate w/ skb */

bypass:
    return action;
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_gstats
 * Description  : Get the statistics related to the blog.
 *   dev_p      : Pointer to a device
 *   bstats_p   : Pointer to a BlogStats_t
 *   clr        : Indicate whether reset the statistics or not
 *------------------------------------------------------------------------------
 */
void blog_gstats( struct net_device * dev_p, BlogStats_t *bstats_p, 
                  StatsClr_t clr )
{
    if ( likely(blog_st_hook_g != (BlogStFnc_t)NULL) )
    {
        local_irq_disable();
        blog_st_hook_g( dev_p, bstats_p, clr );
        local_irq_enable();
    }
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_pstats
 * Description  : Update the statistics related to the blog.
 *   dev_p      : Pointer to a device
 *   bstats_p   : Pointer to a BlogStats_t
 *------------------------------------------------------------------------------
 */
void blog_pstats( struct net_device * dev_p, BlogStats_t *bstats_p ) 
{
    blog_print("dev_p<0x%08x> rx_pkt<%lu> rx_byte<%lu> tx_pkt<%lu>"
               " tx_byte<%lu> multicast<%lu>", (int)dev_p, 
                bstats_p->rx_packets, bstats_p->rx_bytes, bstats_p->tx_packets,
                bstats_p->tx_bytes, bstats_p->multicast);

    if ( dev_p->put_stats )
        dev_p->put_stats( dev_p, bstats_p );
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog
 * Description  : Log the L2 or L3+4 tuple information
 * Parameters   :
 *  skb_p       : Pointer to the sk_buff
 *  dir         : rx or tx path
 *  encap       : Encapsulation type
 *  len         : Length of header
 *  data_p      : Pointer to encapsulation header data.
 *------------------------------------------------------------------------------
 */
void blog( struct sk_buff * skb_p, BlogDir_t dir, BlogEncap_t encap,
           size_t len, void * data_p )
{
    BlogHeader_t * bHdr_p;
    Blog_t * blog_p;

    blog_assertv( (skb_p != (struct sk_buff *)NULL ) );
    blog_assertv( (skb_p->blog_p != BLOG_NULL) );
    blog_assertv( (_IS_BPTR_(skb_p->blog_p)) );
    blog_assertv( (data_p != (void *)NULL ) );
    blog_assertv( (len <= BLOG_HDRSZ_MAX) );
    blog_assertv( (encap < PROTO_MAX ) );

    blog_p = skb_p->blog_p;
    blog_assertv( (blog_p->skb_p == skb_p) );

    bHdr_p = &blog_p->rx + dir;

    if ( encap == L3_IPv4 )    /* Log the IP Tuple */
    {
        BlogTuple_t * bTuple_p = &bHdr_p->tuple;
        struct iphdr * ip_p    = (struct iphdr *)data_p;

        /* Discontinue if non IPv4 or with IP options, or fragmented */
        if ( (ip_p->version != 4) || (ip_p->ihl != 5)
             || ( ip_p->frag_off & htons(IP_OFFSET | IP_MF)) )
            goto skip;

        if ( ip_p->protocol == IPPROTO_TCP )
        {
            struct tcphdr * th_p;
            th_p = (struct tcphdr *)( (uint8_t *)ip_p + sizeof(struct iphdr) );
            if ( th_p->rst | th_p->fin )    /* Discontinue if TCP RST/FIN */
                goto skip;
            bTuple_p->port.source = th_p->source;
            bTuple_p->port.dest = th_p->dest;
        }
        else if ( ip_p->protocol == IPPROTO_UDP )
        {
            struct udphdr * uh_p;
            uh_p = (struct udphdr *)( (uint8_t *)ip_p + sizeof(struct iphdr) );
            bTuple_p->port.source = uh_p->source;
            bTuple_p->port.dest = uh_p->dest;
        }
        else
            goto skip;  /* Discontinue if non TCP or UDP upper layer protocol */

        bTuple_p->ttl = ip_p->ttl;
        bTuple_p->tos = ip_p->tos;
        bTuple_p->check = ip_p->check;
        bTuple_p->saddr = _read32_align16( (uint16_t *)&ip_p->saddr );
        bTuple_p->daddr = _read32_align16( (uint16_t *)&ip_p->daddr );
        blog_p->key.protocol = ip_p->protocol;
    }
#ifdef CONFIG_BLOG_IPV6
    else if ( encap == L3_IPv6 )    /* Log the IPv6 Tuple */
    {
        printk("FIXME blog encap L3_IPv6 \n");
    }
#endif
    else    /* L2 encapsulation */
    {
        register short int * d;
        register const short int * s;

        blog_assertv( (bHdr_p->info.count < BLOG_ENCAP_MAX) );
        blog_assertv( ((len<=20) && ((len & 0x1)==0)) );
        blog_assertv( ((bHdr_p->length + len) < BLOG_HDRSZ_MAX) );

        bHdr_p->info.hdrs |= (1U << encap);
        bHdr_p->encap[ bHdr_p->info.count++ ] = encap;
        s = (const short int *)data_p;
        d = (short int *)&(bHdr_p->l2hdr[bHdr_p->length]);
        bHdr_p->length += len;

        switch ( len ) /* common lengths, using half word alignment copy */
        {
            case 20: *(d+9)=*(s+9);
                     *(d+8)=*(s+8);
                     *(d+7)=*(s+7);
            case 14: *(d+6)=*(s+6);
            case 12: *(d+5)=*(s+5);
            case 10: *(d+4)=*(s+4);
            case  8: *(d+3)=*(s+3);
            case  6: *(d+2)=*(s+2);
            case  4: *(d+1)=*(s+1);
            case  2: *(d+0)=*(s+0);
                 break;
            default:
                 goto skip;
        }
    }

    return;

skip:   /* Discontinue further logging by dis-associating Blog_t object */

    blog_skip( skb_p );

    /* DO NOT ACCESS blog_p !!! */
}


#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)   

/*
 *------------------------------------------------------------------------------
 * Function     : blog_nfct_dump
 * Description  : Dump the nf_conn context
 *  dev_p       : Pointer to a net_device object
 * CAUTION      : nf_conn is not held !!!
 *------------------------------------------------------------------------------
 */
void blog_nfct_dump( struct sk_buff * skb_p, struct nf_conn * ct, uint32_t dir )
{
    struct nf_conn_help *help_p;
    struct nf_conn_nat  *nat_p;
    int bitix;
    if ( ct == NULL )
    {
        blog_error( "NULL NFCT error" );
        return;
    }

#ifdef CONFIG_NF_NAT_NEEDED
    nat_p = nfct_nat(ct);
#else
    nat_p = (struct nf_conn_nat*)NULL;
#endif

    help_p = nfct_help(ct);
    printk("\tNFCT: ct<0x%p>, info<%x> master<0x%p> mark<0x%p>\n"
           "\t\tF_NAT<%p> keys[%u %u] dir<%s>\n"
           "\t\thelp<0x%p> helper<%s>\n",
            ct, 
            (int)skb_p->nfctinfo, 
            ct->master,
#if defined(CONFIG_NF_CONNTRACK_MARK)
            ct->mark,
#else
            (void *)-1,
#endif
            nat_p, 
            ct->blog_key[IP_CT_DIR_ORIGINAL], 
            ct->blog_key[IP_CT_DIR_REPLY],
            (dir<IP_CT_DIR_MAX)?strIpctDir[dir]:strIpctDir[IP_CT_DIR_MAX],
            help_p,
            (help_p && help_p->helper) ? help_p->helper->name : "NONE" );

    printk( "\t\tSTATUS[ " );
    for ( bitix = 0; bitix <= IPS_BLOG_BIT; bitix++ )
        if ( ct->status & (1 << bitix) )
            printk( "%s ", strIpctStatus[bitix] );
    printk( "]\n" );
}
#endif //#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)   


/*
 *------------------------------------------------------------------------------
 * Function     : blog_netdev_dump
 * Description  : Dump the contents of a net_device object.
 *  dev_p       : Pointer to a net_device object
 *
 * CAUTION      : Net device is not held !!!
 *
 *------------------------------------------------------------------------------
 */
static void blog_netdev_dump( struct net_device * dev_p )
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    int i;
    printk( "\tDEVICE: %s dev<0x%08x> ndo_start_xmit[<%08x>]\n"
            "\t  dev_addr[ ", dev_p->name,
            (int)dev_p, dev_p->netdev_ops->ndo_start_xmit );
    for ( i=0; i<dev_p->addr_len; i++ )
        printk( "%02x ", *((uint8_t *)(dev_p->dev_addr) + i) );
    printk( "]\n" );
#else
    int i;
    printk( "\tDEVICE: %s dev<0x%08x>: poll[<%08x>] hard_start_xmit[<%08x>]\n"
            "\t  hard_header[<%08x>] hard_header_cache[<%08x>]\n"
            "\t  dev_addr[ ", dev_p->name,
            (int)dev_p, (int)dev_p->poll, (int)dev_p->hard_start_xmit,
            (int)dev_p->hard_header, (int)dev_p->hard_header_cache );
    for ( i=0; i<dev_p->addr_len; i++ )
        printk( "%02x ", *((uint8_t *)(dev_p->dev_addr) + i) );
    printk( "]\n" );
#endif //LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_tuple_dump
 * Description  : Dump the contents of a BlogTuple_t object.
 *  bTuple_p    : Pointer to the BlogTuple_t object
 *------------------------------------------------------------------------------
 */
static void blog_tuple_dump( BlogTuple_t * bTuple_p )
{
    printk( "\tIPv4:\n"
            "\t\tSrc" _IPPFMT_ " Dst" _IPPFMT_ "\n"
            "\t\tttl<%3u> tos<%3u> check<0x%04x>\n",
            _IP4_(bTuple_p->saddr), bTuple_p->port.source,
            _IP4_(bTuple_p->daddr), bTuple_p->port.dest,
            bTuple_p->ttl, bTuple_p->tos, bTuple_p->check );
}
 
#if defined(CONFIG_BLOG_IPV6)
/*
 *------------------------------------------------------------------------------
 * Function     : blog_tupleV6_dump
 * Description  : Dump the contents of a BlogTupleV6_t object.
 *  bTupleV6_p    : Pointer to the BlogTupleV6_t object
 *------------------------------------------------------------------------------
 */
static void blog_tupleV6_dump( BlogTupleV6_t * bTupleV6_p )
{
    printk( "\tIPv6:\n"
            "\t\tSrc" _IP6PFMT_ "\n"
            "\t\tDst" _IP6PFMT_ "\n"
            "\t\thop_limit<%3u>\n",
            _IP6_(bTupleV6_p->saddr), bTupleV6_p->port.source,
            _IP6_(bTupleV6_p->daddr), bTupleV6_p->port.dest,
            bTupleV6_p->hop_limit );
}
#endif

/*
 *------------------------------------------------------------------------------
 * Function     : blog_l2_dump
 * Description  : parse and dump the contents of all L2 headers
 *  bHdr_p      : Pointer to logged header
 *------------------------------------------------------------------------------
 */
void blog_l2_dump( BlogHeader_t * bHdr_p)
{
    register int i, ix, length, offset = 0;
    BlogEncap_t type;
    char * value = bHdr_p->l2hdr;

    for ( ix=0; ix<bHdr_p->info.count; ix++ )
    {
        type = bHdr_p->encap[ix];

        switch ( type )
        {
            case PPP_1661   : length = sizeof(hProto_t);        break;
            case PPPoE_2516 : length = sizeof(struct pppoe_hdr)
                                     + sizeof(uint16_t);        break;
            case VLAN_8021Q : length = sizeof(struct vlan_hdr); break;
            case ETH_802x   : length = sizeof(struct ethhdr);   break;
            case BCM_SWC    : length = sizeof(struct bcmhdr);   break;

            case L3_IPv4    :
            case L3_IPv6    :
            case BCM_XPHY   :
            default         : printk( "Unsupported type %d\n", type );
                              return;
        }

        printk( "\tENCAP %d. %10s +%2d %2d [ ",
                ix, strBlogEncap[type], offset, length );

        for ( i=0; i<length; i++ )
            printk( "%02x ", (uint8_t)value[i] );

        offset += length;
        value += length;

        printk( "]\n" );
    }
}

void blog_virdev_dump( Blog_t * blog_p )
{
    int i;

    printk( " VirtDev: ");

    for (i=0; i<MAX_VIRT_DEV; i++)
        printk("<0x%08x> ", (int)blog_p->virt_dev_p[i]);

    printk("\n");
}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_dump
 * Description  : Dump the contents of a Blog object.
 *  blog_p      : Pointer to the Blog_t object
 *------------------------------------------------------------------------------
 */
void blog_dump( Blog_t * blog_p )
{
    if ( blog_p == BLOG_NULL )
        return;

    blog_assertv( (_IS_BPTR_(blog_p)) );

    printk( "BLOG <0x%08x> owner<0x%08x> ct<0x%08x>\n"
            "\t\tL1 channel<%u> phyLen<%u> phy<%u> <%s>\n"
            "\t\tfdb_src<0x%08x> fdb_dst<0x%08x>\n"
            "\t\thash<%u> prot<%u> prio<0x%08x> mark<0x%08x> Mtu<%u>\n",
            (int)blog_p, (int)blog_p->skb_p, (int)blog_p->rx.ct_p,
            blog_p->key.l1_tuple.channel,
            rfc2684HdrLength[blog_p->key.l1_tuple.phy],
            blog_p->key.l1_tuple.phy,
            strBlogPhy[blog_p->key.l1_tuple.phy],
            (int)blog_p->fdb_src, (int)blog_p->fdb_dst,
            blog_p->key.hash, blog_p->key.protocol,
            blog_p->priority, blog_p->mark, blog_p->minMtu);

#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)   
    if ( blog_p->rx.ct_p )
        blog_nfct_dump( blog_p->skb_p, blog_p->rx.ct_p, blog_p->rx.nf_dir );
#endif

    printk( "  RX count<%u> channel<%02u> bmap<0x%x> phyLen<%u> phyHdr<%u> %s\n"
            "     hw_support<%u> wan_qdisc<%u> multicast<%u> fkbInSkb<%u>\n",
            blog_p->rx.info.count, blog_p->rx.info.channel,
            blog_p->rx.info.hdrs,
            rfc2684HdrLength[blog_p->rx.info.phyHdr],
            blog_p->rx.info.phyHdr, strBlogPhy[blog_p->rx.info.phyHdr],
            blog_p->rx.info.hw_support, blog_p->rx.info.wan_qdisc,
            blog_p->rx.info.multicast, blog_p->rx.info.fkbInSkb );
    if ( blog_p->rx.info.bmap.L3_IPv4 )
        blog_tuple_dump( &blog_p->rx.tuple );
    blog_l2_dump( &blog_p->rx );

    printk("  TX count<%u> channel<%02u> bmap<0x%x> phyLen<%u> phyHdr<%u> %s\n",
            blog_p->tx.info.count, blog_p->tx.info.channel,
            blog_p->tx.info.hdrs, rfc2684HdrLength[blog_p->tx.info.phyHdr],
            blog_p->tx.info.phyHdr, strBlogPhy[blog_p->tx.info.phyHdr] );
    if ( blog_p->tx.dev_p )
        blog_netdev_dump( blog_p->tx.dev_p );
    if ( blog_p->rx.info.bmap.L3_IPv4 )
        blog_tuple_dump( &blog_p->tx.tuple );
    blog_l2_dump( &blog_p->tx );
    blog_virdev_dump( blog_p );

#if defined(CONFIG_BLOG_IPV6)
    if ( blog_p->rx.info.bmap.L3_IPv6 )
        blog_tupleV6_dump( &blog_p->tupleV6 );
#endif

#if defined(CC_CONFIG_BLOG_DEBUG)
    printk( "\t\textends<%d> free<%d> used<%d> HWM<%d> fails<%d>\n",
            blog_extends, blog_cnt_free, blog_cnt_used, blog_cnt_hwm,
            blog_cnt_fails );
#endif

}

/*
 *------------------------------------------------------------------------------
 * Function     : blog_bind
 * Description  : Override default rx and tx hooks.
 *  blog_rx     : Function pointer to be invoked in blog_finit(), blog_sinit()
 *  blog_tx     : Function pointer to be invoked in blog_emit()
 *  blog_st     : Function pointer to be invoked in blog_gstats()
 *  blog_xx     : Function pointer to be invoked in blog_notify()
 *------------------------------------------------------------------------------
 */
void blog_bind( BlogHook_t blog_rx, BlogHook_t blog_tx, 
                BlogStFnc_t blog_st, BlogNotify_t blog_xx )
{
    blog_print( "Bind Rx[<%08x>] Tx[<%08x>] Stats[<%08x>] Notify[<%08x>]",
                (int)blog_rx, (int)blog_tx, (int)blog_st, (int)blog_xx );
    blog_rx_hook_g = blog_rx;   /* Receive  hook */
    blog_tx_hook_g = blog_tx;   /* Transmit hook */
    blog_st_hook_g = blog_st;   /* Statistics hook */
    blog_xx_hook_g = blog_xx;   /* Notify   hook */
}

/*
 *------------------------------------------------------------------------------
 * Function     : __init_blog
 * Description  : Incarnates the blog system during kernel boot sequence,
 *                in phase subsys_initcall()
 *------------------------------------------------------------------------------
 */
static int __init __init_blog( void )
{
    nfskb_p = alloc_skb( 0, GFP_ATOMIC );
    blog_refresh_fn = (blog_refresh_t)NULL;
    blog_extend( BLOG_POOL_SIZE_ENGG ); /* Build preallocated pool */
    BLOG_DBG( printk( CLRb "BLOG blog_dbg<0x%08x> = %d\n"
                           "%d Blogs allocated of size %d" CLRN,
                           (int)&blog_dbg, blog_dbg,
                           BLOG_POOL_SIZE_ENGG, sizeof(Blog_t) ););
    printk( CLRb "BLOG %s Initialized" CLRN, BLOG_VERSION );
    return 0;
}

subsys_initcall(__init_blog);

EXPORT_SYMBOL(strBlogAction);
EXPORT_SYMBOL(strBlogEncap);

EXPORT_SYMBOL(strRfc2684);
EXPORT_SYMBOL(rfc2684HdrLength);
EXPORT_SYMBOL(rfc2684HdrData);

EXPORT_SYMBOL(blog_refresh_fn);
EXPORT_SYMBOL(blog_refresh_br);

EXPORT_SYMBOL(blog_br_fdb);

#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)   
EXPORT_SYMBOL(blog_nfct);
EXPORT_SYMBOL(blog_time);
EXPORT_SYMBOL(blog_flow);
#endif

EXPORT_SYMBOL(blog_notify);
EXPORT_SYMBOL(blog_gstats);
EXPORT_SYMBOL(blog_pstats);

EXPORT_SYMBOL(blog_bind);

#else   /* !defined(CONFIG_BLOG) */

int blog_dbg = 0;
int blog_mcast_g = 0; /* = CC_SUPPORT_BLOG_MCAST; */
void blog_mcast(int enable) { blog_mcast_g = 0; /* = enable; */ }

/* Stub functions for Blog APIs that may be used by modules */
void blog_put( Blog_t * blog_p ) { return; }
Blog_t * blog_fkb( struct fkbuff * fkb_p ) { return BLOG_NULL; }
Blog_t * blog_snull( struct sk_buff * skb_p ) { return BLOG_NULL; }
Blog_t * blog_fnull( struct fkbuff * fkb_p ) { return BLOG_NULL; }
void blog_free(struct sk_buff * skb_p) { return; }
void blog_dump(Blog_t * blog_p) { return; }
void blog_skip(struct sk_buff * skb_p) { return; }
void blog_xfer(struct sk_buff * skb_p, const struct sk_buff * prev_p){ return; }
void blog_clone(struct sk_buff * skb_p, const struct blog_t * prev_p){ return; }

BlogAction_t blog_sinit(struct sk_buff * skb_p, struct net_device * dev_p,
                        uint32_t encap, uint32_t channel, uint32_t phyHdr)
{ return PKT_NORM; }

BlogAction_t blog_finit(struct fkbuff * fkb_p, struct net_device * dev_p,
                       uint32_t encap, uint32_t channel, uint32_t phyHdr)
{ return PKT_NORM; }

BlogAction_t blog_emit(void * nbuff_p, struct net_device * dev_p,
                       uint32_t encap, uint32_t channel, uint32_t phyHdr)
{ return PKT_NORM; }

void blog(struct sk_buff * skb_p, BlogDir_t dir, BlogEncap_t encap,
          size_t len, void * data_p)
{ return; }

#endif  /* else !defined(CONFIG_BLOG) */

EXPORT_SYMBOL(blog_dbg);
EXPORT_SYMBOL(blog_mcast_g);
EXPORT_SYMBOL(blog_mcast);

EXPORT_SYMBOL(blog_put);
EXPORT_SYMBOL(blog_fkb);
EXPORT_SYMBOL(blog_snull);
EXPORT_SYMBOL(blog_fnull);
EXPORT_SYMBOL(blog_free);
EXPORT_SYMBOL(blog_dump);
EXPORT_SYMBOL(blog_skip);
EXPORT_SYMBOL(blog_xfer);
EXPORT_SYMBOL(blog_clone);
EXPORT_SYMBOL(blog_sinit);
EXPORT_SYMBOL(blog_finit);
EXPORT_SYMBOL(blog_emit);
EXPORT_SYMBOL(blog);
