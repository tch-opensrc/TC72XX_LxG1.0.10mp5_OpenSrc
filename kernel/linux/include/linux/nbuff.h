#ifndef __NBUFF_H_INCLUDED__
#define __NBUFF_H_INCLUDED__

/*
<:copyright-gpl

 Copyright 2009 Broadcom Corp. All Rights Reserved.

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
 * File Name  : nbuff.h
 * Description: Definition of a network buffer to support various forms of
 *      network buffer, to include Linux socket buff (SKB), lightweight
 *      fast kernel buff (FKB), BRCM Free Pool buffer (FPB), and traffic
 *      generator support buffer (TGB)
 *
 *      nbuff.h may also be used to provide an interface to common APIs 
 *      available on other OS (in particular BSD style mbuf).
 *
 * Common APIs provided: pushing, pulling, reading, writing, cloning, freeing
 *
 * Implementation Note:
 *
 * One may view NBuff as a base class from which other buff types are derived.
 * Examples of derived network buffer types are sk_buff, fkbuff, fpbuff, tgbuff
 *
 * A pointer to a buffer is converted to a pointer to a special (derived) 
 * network buffer type by encoding the type into the least significant 2 bits
 * of a word aligned buffer pointer. pBuf points to the real network 
 * buffer and pNBuff refers to pBuf ANDed with the Network Buffer Type.
 * C++ this pointer to a virtual class (vtable based virtual function thunks).
 *
 * Thunk functions to redirect the calls to the appropriate buffer type, e.g.
 * skb or fkb uses the Network Buffer Pointer type information.
 *
 * This file also implements the Fast Kernel Buffer API. The fast kernel buffer
 * carries a minimal context of the received buffer and associated buffer
 * recycling information.
 *
 ******************************************************************************* */

#include <linux/autoconf.h>
#include <linux/types.h>            /* include ISO C99 inttypes.h             */
#include <linux/skbuff.h>           /* include corresponding BSD style mbuf   */
#include <linux/blog.h>

#define NBUFF_VERSION              "v1.0"

/* Engineering Constants for Fast Kernel Buffer Global Pool (used for clones) */
#define SUPPORT_FKB_EXTEND
#define FKB_POOL_SIZE_ENGG          400
#define FKB_EXTEND_SIZE_ENGG        32      /* Number of FkBuf_t per extension*/
#define FKB_EXTEND_MAX_ENGG         16      /* Maximum extensions allowed     */

#define FKB_XLATE_SKB_HEADROOM      176
#define FKB_XLATE_SKB_TAILROOM      32

/* Conditional compile of inline fkb functions */
#define CC_CONFIG_FKB_FN_INLINE
#ifdef CC_CONFIG_FKB_FN_INLINE
#define DEFINE_FKB_FN(fn_signature, body)                                      \
static inline fn_signature { body; }
#else
#define DEFINE_FKB_FN(fn_signature, body)                                      \
extern fn_signature;
#endif

/* LAB ONLY: Design development */
// #define CC_CONFIG_FKB_STATS
// #define CC_CONFIG_FKB_COLOR
// #define CC_CONFIG_FKB_DEBUG
extern int nbuff_dbg;
#if defined(CC_CONFIG_FKB_DEBUG)
#define fkb_dbg(lvl, fmt, arg...) \
    if (nbuff_dbg >= lvl) printk( "FKB %s :" fmt "[<%08x>]\n", \
        __FUNCTION__, ##arg, (int)__builtin_return_address(0) )
#else
#define fkb_dbg(lvl, fmt, arg...)      do {} while(0)
#endif

#define CC_NBUFF_FLUSH_OPTIMIZATION

/*
 * For BSD style mbuf with FKB : 
 * generate nbuff.h by replacing "SKBUFF" to "BCMMBUF", and,
 * use custom arg1 and arg2 instead of mark and priority, respectively.
 */

struct sk_buff;
struct blog_t;
struct net_device;
typedef int (*HardStartXmitFuncP) (struct sk_buff *skb,
                                   struct net_device *dev);

struct fkbuff;
typedef struct fkbuff FkBuff_t;

/* virtual network buffer pointer to SKB|FPB|TGB|FKB  */
typedef void * pNBuff_t;
#define PNBUFF_NULL     ((pNBuff_t)NULL)

/* Test if kernel pointer (i.e. in KSEG0|1|2) */
#define _IS_KPTR_(v)                (((uint32_t)(v)) > 0x0FFFFFFF)

#define MUST_BE_ZERO            0
typedef enum NBuffPtrType
{
    SKBUFF_PTR = MUST_BE_ZERO,      /* Default Linux networking socket buffer */
    FPBUFF_PTR,                     /* Experimental BRCM IuDMA freepool buffer*/
    TGBUFF_PTR,                     /* LAB Traffic generated network buffer   */
    FKBUFF_PTR,                     /* Lightweight fast kernel network buffer */
    /* Do not add new ptr types */
} NBuffPtrType_t;

                                    /* 2lsbits in pointer encode NbuffType_t  */
#define NBUFF_TYPE_MASK             0x3u
#define NBUFF_PTR_MASK              (~NBUFF_TYPE_MASK)
#define NBUFF_PTR_TYPE(pNBuff)      ((uint32_t)(pNBuff) & NBUFF_TYPE_MASK)


#define IS_SKBUFF_PTR(pNBuff)       ( NBUFF_PTR_TYPE(pNBuff) == SKBUFF_PTR )
#define IS_FPBUFF_PTR(pNBuff)       ( NBUFF_PTR_TYPE(pNBuff) == FPBUFF_PTR )
#define IS_TGBUFF_PTR(pNBuff)       ( NBUFF_PTR_TYPE(pNBuff) == TGBUFF_PTR )
#define IS_FKBUFF_PTR(pNBuff)       ( NBUFF_PTR_TYPE(pNBuff) == FKBUFF_PTR )

/*
 *------------------------------------------------------------------------------
 *
 * Pointer conversion between pBuf and pNBuff encoded buffer pointers
 * uint8_t * pBuf;
 * pNBuff_t  pNBuff;
 * ...
 * // overlays FKBUFF_PTR into pointer to build a virtual pNBuff_t
 * pNBuff = PBUF_2_PNBUFF(pBuf,FKBUFF_PTR);
 * ...
 * // extracts a real uint8_t * from a virtual pNBuff_t
 * pBuf = PNBUFF_2_PBUF(pNBuff);
 *
 *------------------------------------------------------------------------------
 */
#define PBUF_2_PNBUFF(pBuf,realType) \
            ( (pNBuff_t) ((uint32_t)(pBuf)   | (uint32_t)(realType)) )
#define PNBUFF_2_PBUF(pNBuff)       \
            ( (uint8_t*) ((uint32_t)(pNBuff) & (uint32_t)NBUFF_PTR_MASK) )

#if (MUST_BE_ZERO != 0)
#error  "Design assumption SKBUFF_PTR == 0"
#endif
#define PNBUFF_2_SKBUFF(pNBuff)     ((struct sk_buff *)(pNBuff))

#define SKBUFF_2_PNBUFF(skb)        ((pNBuff_t)(skb)) /* see MUST_BE_ZERO */
#define FKBUFF_2_PNBUFF(fkb)        PBUF_2_PNBUFF(fkb,FKBUFF_PTR)

/*
 *------------------------------------------------------------------------------
 *
 * Cast from/to virtual "pNBuff_t" to/from real typed pointers
 *
 *  pNBuff_t pNBuff2Skb, pNBuff2Fkb;    // "void *" with NBuffPtrType_t
 *  struct sk_buff * skb_p;
 *  struct fkbuff  * fkb_p;
 *  ...
 *  pNBuff2Skb = CAST_REAL_TO_VIRT_PNBUFF(skb_p,SKBUFF_PTR);
 *  pNBuff2Fkb = CAST_REAL_TO_VIRT_PNBUFF(fkb_p,FKBUFF_PTR);
 *  ...
 *  skb_p = CAST_VIRT_TO_REAL_PNBUFF(pNBuff2Skb, struct sk_buff *);
 *  fkb_p = CAST_VIRT_TO_REAL_PNBUFF(pNBuff2Fkb, struct fkbuff  *);
 * or,
 *  fkb_p = PNBUFF_2_FKBUFF(pNBuff2Fkb);  
 *------------------------------------------------------------------------------
 */

#define CAST_REAL_TO_VIRT_PNBUFF(pRealNBuff,realType) \
            ( (pNBuff_t) (PBUF_2_PNBUFF((pRealNBuff),(realType))) )

#define CAST_VIRT_TO_REAL_PNBUFF(pVirtNBuff,realType) \
            ( (realType) PNBUFF_2_PBUF(pVirtNBuff) )

#define PNBUFF_2_FKBUFF(pNBuff) CAST_VIRT_TO_REAL_PNBUFF((pNBuff),struct fkbuff*)

/*
 *------------------------------------------------------------------------------
 *  FKB: Fast Kernel Buffers placed directly into Rx DMA Buffer
 *  May be used ONLY for common APIs such as those available in BSD-Style mbuf
 *------------------------------------------------------------------------------
 */

struct fkbuff
{
    union {
        FkBuff_t  * list;           /* SLL of free FKBs for cloning           */
        FkBuff_t  * master_p;       /* Clone FKB to point to master FKB       */
        atomic_t  users;            /* (private) # of references to FKB       */
    };
    union {                         /* Use _IS_KPTR_ to determine if ptr      */
        struct blog_t *blog_p;      /* Pointer to a blog                      */
        uint8_t       *dirty_p;
        union {
          struct {
            uint32_t   ptr   : 8;   /* Identifies whether pointer             */
            uint32_t   in_skb:24;   /* Member of skb : See FKB_IN_SKB         */
          };
          uint32_t     in_skb_tag;
        };
    };
    uint8_t       * data;           /* Pointer to packet data                 */
    uint32_t      len;              /* Packet length                          */

    uint32_t      mark;             /* Custom arg1, e.g. tag or mark field    */
    uint32_t      priority;         /* Custom arg2, packet priority, tx info  */
    RecycleFuncP  recycle_hook;     /* Nbuff recycle handler                  */
    uint32_t      recycle_context;  /* Rx network device/channel or pool      */

} ____cacheline_aligned;   /* 2 cache lines wide */

#define FKB_NULL                    ((FkBuff_t *)NULL)

/* Verify whether there is a valid blog attached to Fkb */
#define _IS_BPTR_(b)                ( _IS_KPTR_(b) && !((uint32_t)(b) & 0x1))

/* Tag the pointer (and with 0x1) to signify a dirty pointer */
#define _TO_DPTR_(d)                ((uint8_t*)((uint32_t)(d) | 0x1))

/*
 * An fkbuff may be referred to as a:
 *  master - a pre-allocated rxBuffer, inplaced ahead of the headroom.
 *  cloned - allocated from a free pool of fkbuff and points to a master.
 *  in_skb - member of a sk_buff structure.
 */
#define FKB_IN_SKB                  (0x00FFFFFE)

/*
 * fkbuff member master_p and users aliased to same storage. When a fkbuff is
 * cloned, the storage is used as a pointer to the master fkbuff which 
 * contains the users reference count. 
 */
#define IS_FKB_POOL(fkb_p)          ( (uint32_t) _IS_KPTR_((fkb_p)->master_p) )
#define IS_FKB_CLONE(fkb_p)         ( IS_FKB_POOL(fkb_p)                      \
                                    || ( atomic_read(&((fkb_p)->users)) > 1 ))

/*
 *------------------------------------------------------------------------------
 * Placement of a FKB object in the Rx DMA buffer:
 *
 * RX DMA Buffer:   |----- FKB ----|--- reserve headroom ---|---...... 
 *                  ^              ^                        ^
 *                pFkb           pHead                    pData
 *                pBuf
 *------------------------------------------------------------------------------
 */
#define PFKBUFF_PHEAD_OFFSET        sizeof(FkBuff_t)
#define PFKBUFF_TO_PHEAD(pFkb)      ((uint8_t*)((FkBuff_t*)(pFkb) + 1))

#define PDATA_TO_PFKBUFF(pData,headroom)    \
            (FkBuff_t *)((uint8_t*)(pData)-(headroom)-PFKBUFF_PHEAD_OFFSET)
#define PFKBUFF_TO_PDATA(pFkb,headroom)     \
            (uint8_t*)((uint8_t*)(pFkb) + PFKBUFF_PHEAD_OFFSET + (headroom))

/*
 * fkb_construct() validates that the layout of fkbuff members in sk_buff
 * is the same. An sk_buff contains an fkbuff and permits a quick translation
 * to and from a fkbuff. It also preallocates the pool of fkbs for fkb_clone()
 */
extern int fkb_in_skb_test( int fkb_in_skb_offset,
                            int list_offset, int blog_p_offset,
                            int data_offset, int len_offset, int mark_offset,
                            int priority_offset, int recycle_hook_offset,
                            int recycle_context_offset );
extern int fkb_construct(int fkb_in_skb_offset);

extern void fkb_stats(void);    /* allocator statistics : CC_CONFIG_FKB_STATS */

extern FkBuff_t * fkb_alloc(void);
extern void fkb_free(FkBuff_t * fkb_p);

extern struct sk_buff * nbuff_xlate( pNBuff_t pNBuff );



/* Set reference count to an FKB */
static inline void _fkb_set_ref(FkBuff_t * fkb_p, const int count)
{
    atomic_set(&fkb_p->users, count);
}
DEFINE_FKB_FN( void fkb_set_ref(FkBuff_t * fkb_p, const int count),
               _fkb_set_ref(fkb_p, count) )

/* Increment references to an FKB */
static inline void _fkb_inc_ref(FkBuff_t * fkb_p)
{
    atomic_inc(&fkb_p->users);
}
DEFINE_FKB_FN( void fkb_inc_ref(FkBuff_t * fkb_p), _fkb_inc_ref(fkb_p) )

/* Decrement references to an FKB */
static inline void _fkb_dec_ref(FkBuff_t * fkb_p)
{
    atomic_dec(&fkb_p->users);
}
DEFINE_FKB_FN( void fkb_dec_ref(FkBuff_t * fkb_p), _fkb_dec_ref(fkb_p) )


/*
 * Pre-initialization of FKB object that is placed into rx buffer descriptors
 * when they are created. FKB objects preceeds the reserved headroom.
 */
static inline void fkb_preinit(uint8_t * pBuf, RecycleFuncP recycle_hook,
                               uint32_t recycle_context)
{
    FkBuff_t * fkb_p = (FkBuff_t *)pBuf;
    fkb_p->recycle_hook = recycle_hook;         /* never modified */
    fkb_p->recycle_context = recycle_context;   /* never modified */

    fkb_p->blog_p = BLOG_NULL;
    fkb_p->data = (uint8_t*)NULL;
    fkb_p->len  = fkb_p->mark  = fkb_p->priority = 0;
    fkb_set_ref( fkb_p, 0 );
}

/* Initialize the FKB context for a received packet */
static inline FkBuff_t * _fkb_init(uint8_t * pBuf, uint32_t headroom,
                                   uint8_t * pData, uint32_t len)
{
    FkBuff_t * fkb_p = PDATA_TO_PFKBUFF(pBuf, headroom);
    fkb_dbg( 1, "fkb_p<0x%08x> pBuf<0x%08x> headroom<%u> pData<0x%08x> len<%d>",
              (int)fkb_p, (int)pBuf, (int)headroom, (int)pData, len );
    fkb_p->data      = pData;
    fkb_p->len       = len;
    fkb_p->blog_p    = BLOG_NULL;   /* resets in_skb_tag */

    fkb_set_ref( fkb_p, 1 );

    return fkb_p;
}
DEFINE_FKB_FN( 
    FkBuff_t * fkb_init(uint8_t * pBuf, uint32_t headroom,
                        uint8_t * pData, uint32_t len),
    return _fkb_init(pBuf, headroom, pData, len) )

/* Initialize the FKB context for a received packet, specifying recycle queue */
static inline FkBuff_t * _fkb_qinit(uint8_t * pBuf, uint32_t headroom,
                    uint8_t * pData, uint32_t len, uint32_t qcontext)
{
    FkBuff_t * fkb_p = PDATA_TO_PFKBUFF(pBuf, headroom);
    fkb_dbg(1, "fkb_p<0x%08x> pBuf<0x%08x> headroom<%u>"
            " pData<0x%08x> len<%d> context<0x%08x>",
            (int)fkb_p, (int)pBuf, headroom, (int)pData, len, qcontext );
    fkb_p->data             = pData;
    fkb_p->len              = len;
    fkb_p->blog_p           = BLOG_NULL;
    fkb_p->recycle_context  = qcontext;

    fkb_set_ref( fkb_p, 1 );

    return fkb_p;
}
DEFINE_FKB_FN(
    FkBuff_t * fkb_qinit(uint8_t * pBuf, uint32_t headroom,
                         uint8_t * pData, uint32_t len, uint32_t qcontext),
    return _fkb_qinit(pBuf, headroom, pData, len, qcontext) )

/* Release any associated blog and set ref count to 0 */
void blog_put(struct blog_t * blog_p);
static inline void _fkb_release(FkBuff_t * fkb_p)
{
    fkb_dbg(1, "fkb_p<0x%08x> fkb_p->blog_p<0x%08x>",
            (int)fkb_p, (int)fkb_p->blog_p );
    if ( _IS_KPTR_( fkb_p->blog_p ) )
        blog_put(fkb_p->blog_p);
    fkb_p->blog_p = BLOG_NULL;  /* resets in_skb_tag */

    fkb_set_ref( fkb_p, 0 );
}
DEFINE_FKB_FN( void fkb_release(FkBuff_t * fkb_p), _fkb_release(fkb_p) )

static inline int _fkb_headroom(const FkBuff_t *fkb_p)
{
    return (int)( (uint32_t)(fkb_p->data) - (uint32_t)(fkb_p+1) );
}
DEFINE_FKB_FN( int fkb_headroom(const FkBuff_t *fkb_p),
               return _fkb_headroom(fkb_p) )

/* Prepare space for data at head of buffer pointed by FKB */
static inline uint8_t * _fkb_push(FkBuff_t * fkb_p, uint32_t len)
{
    fkb_p->len  += len;
    fkb_p->data -= len;
    return fkb_p->data;
}
DEFINE_FKB_FN( uint8_t * fkb_push(FkBuff_t * fkb_p, uint32_t len),
               return _fkb_push(fkb_p, len) )

/* Delete data from head of buffer pointed by FKB */
static inline uint8_t * _fkb_pull(FkBuff_t * fkb_p, uint32_t len)
{
    fkb_p->len  -= len;
    fkb_p->data += len;
    return fkb_p->data;
}
DEFINE_FKB_FN( uint8_t * fkb_pull(FkBuff_t * fkb_p, uint32_t len),
               return _fkb_pull(fkb_p, len) )

/* Prepare space for data at tail of buffer pointed by FKB */
static inline uint8_t * _fkb_put(FkBuff_t * fkb_p, uint32_t len)
{
    uint8_t * tail_p = fkb_p->data + fkb_p->len; 
    fkb_p->len  += len;
#if defined(CC_NBUFF_FLUSH_OPTIMIZATION)
    fkb_p->dirty_p = _TO_DPTR_(tail_p + len);
#endif
    return tail_p;
}
DEFINE_FKB_FN( uint8_t * fkb_put(FkBuff_t * fkb_p, uint32_t len),
               return _fkb_put(fkb_p, len) )

/* Pad the packet */
static inline uint32_t _fkb_pad(FkBuff_t * fkb_p, uint32_t padding)
{
    fkb_p->len  += padding;
    return fkb_p->len;
}
DEFINE_FKB_FN( uint32_t fkb_pad(FkBuff_t * fkb_p, uint32_t padding),
               return _fkb_pad(fkb_p, padding) )


static inline uint32_t _fkb_len(FkBuff_t * fkb_p)
{
    return fkb_p->len;
}
DEFINE_FKB_FN( uint32_t fkb_len(FkBuff_t * fkb_p),
               return _fkb_len(fkb_p) )

static inline uint8_t * _fkb_data(FkBuff_t * fkb_p)
{
    return fkb_p->data;
}
DEFINE_FKB_FN( uint8_t * fkb_data(FkBuff_t * fkb_p),
               return _fkb_data(fkb_p) )

static inline struct blog_t * _fkb_blog(FkBuff_t * fkb_p)
{
    return fkb_p->blog_p;
}
DEFINE_FKB_FN( struct blog_t * fkb_blog(FkBuff_t * fkb_p),
               return _fkb_blog(fkb_p) )

/* Clone a Master FKB into a fkb from a pool */
static inline FkBuff_t * _fkb_clone(FkBuff_t * fkbM_p)
{
    FkBuff_t * fkbC_p;

    fkbC_p = fkb_alloc();         /* Allocate an FKB */

    if ( unlikely(fkbC_p != FKB_NULL) )
    {
        fkb_inc_ref( fkbM_p );
        fkbC_p->master_p   = fkbM_p;

        fkbC_p->data       = fkbM_p->data;
        fkbC_p->len        = fkbM_p->len;
    }

    fkb_dbg(1, "fkbM_p<0x%08x> fkbC_p<0x%08x>", (int)fkbM_p, (int)fkbC_p );

    return fkbC_p;       /* May be null */
}
DEFINE_FKB_FN( FkBuff_t * fkb_clone(FkBuff_t * fkbM_p),
               return _fkb_clone(fkbM_p) )


/*
 *------------------------------------------------------------------------------
 * Virtual accessors to common members of network kernel buffer
 *------------------------------------------------------------------------------
 */

#define __BUILD_NBUFF_SET_ACCESSOR( TYPE, MEMBER )                             \
static inline void nbuff_set_##MEMBER(pNBuff_t pNBuff, TYPE MEMBER) \
{                                                                              \
    void * pBuf = PNBUFF_2_PBUF(pNBuff);                                       \
    if ( IS_SKBUFF_PTR(pNBuff) )                                               \
        ((struct sk_buff *)pBuf)->MEMBER = MEMBER;                             \
    /* else if IS_FPBUFF_PTR, else if IS_TGBUFF_PTR */                         \
    else                                                                       \
        ((FkBuff_t *)pBuf)->MEMBER = MEMBER;                                   \
}

#define __BUILD_NBUFF_GET_ACCESSOR( TYPE, MEMBER )                             \
static inline TYPE nbuff_get_##MEMBER(pNBuff_t pNBuff)                         \
{                                                                              \
    void * pBuf = PNBUFF_2_PBUF(pNBuff);                                       \
    if ( IS_SKBUFF_PTR(pNBuff) )                                               \
        return (TYPE)(((struct sk_buff *)pBuf)->MEMBER);                       \
    /* else if IS_FPBUFF_PTR, else if IS_TGBUFF_PTR */                         \
    else                                                                       \
        return (TYPE)(((FkBuff_t *)pBuf)->MEMBER);                             \
}

/* Common accessor of base network buffer: */
__BUILD_NBUFF_SET_ACCESSOR(uint8_t *, data) 
__BUILD_NBUFF_SET_ACCESSOR(uint32_t, len) 
__BUILD_NBUFF_SET_ACCESSOR(uint32_t, mark)      /* Custom network buffer arg1 */
__BUILD_NBUFF_SET_ACCESSOR(uint32_t, priority)  /* Custom network buffer arg2 */

__BUILD_NBUFF_GET_ACCESSOR(uint8_t *, data)
__BUILD_NBUFF_GET_ACCESSOR(uint32_t, len)
__BUILD_NBUFF_GET_ACCESSOR(uint32_t, mark)      /* Custom network buffer arg1 */
__BUILD_NBUFF_GET_ACCESSOR(uint32_t, priority)  /* Custom network buffer arg2 */

static inline void * nbuff_get_context(pNBuff_t pNBuff,
                                     uint8_t ** data_p, uint32_t *len_p)
{
    void * pBuf = PNBUFF_2_PBUF(pNBuff);
    if ( pBuf == (void*) NULL )
        return pBuf;
    if ( IS_SKBUFF_PTR(pNBuff) )
    {
        *data_p     = ((struct sk_buff *)pBuf)->data;
        *len_p      = ((struct sk_buff *)pBuf)->len;
    }
    else
    {
        *data_p     = ((FkBuff_t *)pBuf)->data;
        *len_p      = ((FkBuff_t *)pBuf)->len;
    }
    fkb_dbg(1, "pNBuff<0x%08x> pBuf<0x%08x> data_p<0x%08x>",
           (int)pNBuff, (int)pBuf, (int)*data_p );
    return pBuf;
}

static inline void * nbuff_get_params(pNBuff_t pNBuff,
                                     uint8_t ** data_p, uint32_t *len_p,
                                     uint32_t * mark_p, uint32_t *priority_p)
{
    void * pBuf = PNBUFF_2_PBUF(pNBuff);
    if ( pBuf == (void*) NULL )
        return pBuf;
    if ( IS_SKBUFF_PTR(pNBuff) )
    {
        *data_p     = ((struct sk_buff *)pBuf)->data;
        *len_p      = ((struct sk_buff *)pBuf)->len;
        *mark_p     = ((struct sk_buff *)pBuf)->mark;
        *priority_p = ((struct sk_buff *)pBuf)->priority;
    }
    else
    {
        *data_p     = ((FkBuff_t *)pBuf)->data;
        *len_p      = ((FkBuff_t *)pBuf)->len;
        *mark_p     = ((FkBuff_t *)pBuf)->mark;
        *priority_p = ((FkBuff_t *)pBuf)->priority;
    }
    fkb_dbg(1, "pNBuff<0x%08x> pBuf<0x%08x> data_p<0x%08x>",
            (int)pNBuff, (int)pBuf, (int)*data_p );
    return pBuf;
}

/* adds recycle flags/context to nbuff_get_params used in impl4 enet */
static inline void * nbuff_get_params_ext(pNBuff_t pNBuff, uint8_t **data_p, 
                                          uint32_t *len_p, uint32_t *mark_p, 
                                          uint32_t *priority_p, 
                                          uint32_t *rflags_p)
{
    void * pBuf = PNBUFF_2_PBUF(pNBuff);
    if ( pBuf == (void*) NULL )
        return pBuf;
    if ( IS_SKBUFF_PTR(pNBuff) )
    {
        *data_p     = ((struct sk_buff *)pBuf)->data;
        *len_p      = ((struct sk_buff *)pBuf)->len;
        *mark_p     = ((struct sk_buff *)pBuf)->mark;
        *priority_p = ((struct sk_buff *)pBuf)->priority;
        *rflags_p   = ((struct sk_buff *)pBuf)->recycle_flags;
    }
    else
    {
        *data_p     = ((FkBuff_t *)pBuf)->data;
        *len_p      = ((FkBuff_t *)pBuf)->len;
        *mark_p     = ((FkBuff_t *)pBuf)->mark;
        *priority_p = ((FkBuff_t *)pBuf)->priority;
        *rflags_p   = ((FkBuff_t *)pBuf)->recycle_context;
    }
    fkb_dbg(1, "pNBuff<0x%08x> pBuf<0x%08x> data_p<0x%08x>",
            (int)pNBuff, (int)pBuf, (int)*data_p );
    return pBuf;
}
    
/*
 *------------------------------------------------------------------------------
 * Virtual common functional apis of a network kernel buffer
 *------------------------------------------------------------------------------
 */

/* Make space at the start of a network buffer */
static inline uint8_t * nbuff_push(pNBuff_t pNBuff, uint32_t len)
{
    uint8_t * data;
    void * pBuf = PNBUFF_2_PBUF(pNBuff);
    if ( IS_SKBUFF_PTR(pNBuff) )
        data = skb_push(((struct sk_buff *)pBuf), len);
    /* else if IS_FPBUFF_PTR, else if IS_TGBUFF_PTR */
    else
        data = fkb_push((FkBuff_t*)pBuf, len);
    fkb_dbg(1, "pNBuff<0x%08x> pBuf<0x%08x> data<0x%08x> len<%u>",
            (int)pNBuff,(int)pBuf, (int)data, len );
    return data;
}

/* Delete data from start of a network buffer */
static inline uint8_t * nbuff_pull(pNBuff_t pNBuff, uint32_t len)
{
    uint8_t * data;
    void * pBuf = PNBUFF_2_PBUF(pNBuff);
    if ( IS_SKBUFF_PTR(pNBuff) )
        data = skb_pull(((struct sk_buff *)pBuf), len);
    /* else if IS_FPBUFF_PTR, else if IS_TGBUFF_PTR */
    else
        data = fkb_pull((FkBuff_t *)pBuf, len);
    fkb_dbg(1, "pNBuff<0x%08x> pBuf<0x%08x> data<0x%08x> len<%u>",
            (int)pNBuff,(int)pBuf, (int)data, len );
    return data;
}

static inline uint8_t * nbuff_put(pNBuff_t pNBuff, uint32_t len)
{
    uint8_t * tail;
    void * pBuf = PNBUFF_2_PBUF(pNBuff);
    if ( IS_SKBUFF_PTR(pNBuff) )
        tail = skb_put(((struct sk_buff *)pBuf), len);
    /* else if IS_FPBUFF_PTR, else if IS_TGBUFF_PTR */
    else
        tail = fkb_put((FkBuff_t *)pBuf, len);
    fkb_dbg(1, "pNBuff<0x%08x> pBuf<0x%08x> tail<0x%08x> len<%u>",
            (int)pNBuff,(int)pBuf, (int)tail, len );
    return tail;
}

/* Free|Recycle a network buffer and associated data */
extern void dev_kfree_skb_any(struct sk_buff *skb);
static inline void nbuff_free(pNBuff_t pNBuff)
{
    void * pBuf = PNBUFF_2_PBUF(pNBuff);
    fkb_dbg(1, "pNBuff<0x%08x> pBuf<0x%08x>", (int)pNBuff,(int)pBuf);
    if ( IS_SKBUFF_PTR(pNBuff) )
        dev_kfree_skb_any((struct sk_buff *)pBuf);
    /* else if IS_FPBUFF_PTR, else if IS_TGBUFF_PTR */
    else
        fkb_free(pBuf);
    fkb_dbg(2, "<<");
}

/* OS Specific Section Begin */
#if defined(__KERNEL__)     /* Linux MIPS Cache Specific */
/*
 *------------------------------------------------------------------------------
 * common cache operations:
 *
 * - addr is rounded down to the cache line
 * - end is rounded up to cache line.
 *
 * - if ((addr == end) and (addr was cache aligned before rounding))
 *       no operation is performed.
 *   else
 *       flush data cache line UPTO but NOT INCLUDING rounded up end.
 *
 * Note:
 * if before rounding, (addr == end)  AND addr was not cache aligned,
 *      we would flush at least one line.
 *
 * Uses: L1_CACHE_BYTES
 *------------------------------------------------------------------------------
 */
#include <asm/cache.h>
#include <asm/r4kcache.h>

/*
 * Macros to round down and up, an address to a cachealigned address
 */
#define ADDR_ALIGN_DN(addr, align)  ( (addr) & ~((align) - 1) )
#define ADDR_ALIGN_UP(addr, align)  ( ((addr) + (align) - 1) & ~((align) - 1) )

/*
 *------------------------------------------------------------------------------
 * Writeback flush, then invalidate a region demarcated by addr to end.
 * Cache line following rounded up end is not flushed.
 *------------------------------------------------------------------------------
 */
static inline void cache_flush_region(void *addr, void *end)
{
    unsigned long a = ADDR_ALIGN_DN( (unsigned long)addr, L1_CACHE_BYTES );
    unsigned long e = ADDR_ALIGN_UP( (unsigned long)end, L1_CACHE_BYTES );
    while ( a < e )
    {
        flush_dcache_line(a);   /* Hit_Writeback_Inv_D */
        a += L1_CACHE_BYTES;    /* next cache line base */
    }
}

/*
 *------------------------------------------------------------------------------
 * Writeback flush, then invalidate a region given an address and a length.
 * The demarcation end is computed by applying length to address before
 * rounding down address. End is rounded up.
 * Cache line following rounded up end is not flushed.
 *------------------------------------------------------------------------------
 */
static inline void cache_flush_len(void *addr, int len)
{
    unsigned long a = ADDR_ALIGN_DN( (unsigned long)addr, L1_CACHE_BYTES );
    unsigned long e = ADDR_ALIGN_UP( ((unsigned long)addr + len),
                                     L1_CACHE_BYTES );
    while ( a < e )
    {
        flush_dcache_line(a);   /* Hit_Writeback_Inv_D */
        a += L1_CACHE_BYTES;    /* next cache line base */
    }
}

#endif  /* defined(__KERNEL__) Linux MIPS Cache Specific */
/* OS Specific Section End */


static inline void nbuff_flush(pNBuff_t pNBuff, uint8_t * data, int len)
{
    fkb_dbg(1, "pNBuff<0x%08x> data<0x%08x> len<%d>",
            (int)pNBuff, (int)data, len);
    if ( IS_SKBUFF_PTR(pNBuff) )
        cache_flush_len(data, len);
    else
    {
#if defined(CC_NBUFF_FLUSH_OPTIMIZATION)
        /* Flush on L1 dirty cache lines */
        uint32_t dirty_p;
        FkBuff_t * fkb_p = (FkBuff_t *)PNBUFF_2_PBUF(pNBuff);
        dirty_p = (uint32_t)(fkb_p->dirty_p) & ~1U;
        cache_flush_region(data, (uint8_t*)dirty_p);
#else
        cache_flush_len(data, len);
#endif
    }
    fkb_dbg(2, "<<");
}

/* Flush and free a pNBuff, for error paths. */
static inline void nbuff_flushfree(pNBuff_t pNBuff)
{
    void * pBuf = PNBUFF_2_PBUF(pNBuff);
    fkb_dbg(1, "pNBuff<0x%08x> pBuf<0x%08x>", (int)pNBuff,(int)pBuf);
    if ( IS_SKBUFF_PTR(pNBuff) )
    {
        struct sk_buff * skb_p = (struct sk_buff *)pBuf;
        cache_flush_len(skb_p->data, skb_p->len);
        dev_kfree_skb_any(skb_p);
    }
    /* else if IS_FPBUFF_PTR, else if IS_TGBUFF_PTR */
    else
    {
        FkBuff_t * fkb_p = (FkBuff_t *)pBuf;
        /* Did not optimize error path: see CC_NBUFF_FLUSH_OPTIMIZATION */
        cache_flush_len(fkb_p->data, fkb_p->len);
        fkb_free(fkb_p);
    }
    fkb_dbg(2, "<<");
}

static inline void u16cpy( void * dst_p, const void * src_p, uint32_t bytes )
{
    uint16_t * dst16_p = (uint16_t*)dst_p;
    uint16_t * src16_p = (uint16_t*)src_p;
    do { // assuming: (bytes % sizeof(uint16_t) == 0 !!!
        *dst16_p++ = *src16_p++;
    } while ( bytes -= sizeof(uint16_t) );
}

static inline int u16cmp( void * dst_p, const void * src_p,
                          uint32_t bytes )
{
    uint16_t * dst16_p = (uint16_t*)dst_p;
    uint16_t * src16_p = (uint16_t*)src_p;
    do { // assuming: (bytes % sizeof(uint16_t) == 0 !!!
        if ( *dst16_p++ != *src16_p++ )
            return -1;
    } while ( bytes -= sizeof(uint16_t) );

    return 0;
}

#ifdef DUMP_DATA
/* dumpHexData dump out the hex base binary data */
static inline void dumpHexData(uint8_t *pHead, uint32_t len)
{
    uint32_t i;
    uint8_t *c = pHead;
    for (i = 0; i < len; ++i) {
        if (i % 16 == 0)
            printk("\n");
        printk("0x%02X, ", *c++);
    }
    printk("\n");
}

inline void dump_pkt(const char * fname, uint8_t * pBuf, uint32_t len)
{
    int dump_len = ( len < 64) ? len : 64;
    printk("%s: data<0x%08x> len<%u>", fname, (int)pBuf, len);
    dumpHexData(pBuf, dump_len);
    cache_flush_len((void*)pBuf, dump_len);
}
#define DUMP_PKT(pBuf,len)      dump_pkt(__FUNCTION__, (pBuf), (len))
#else   /* !defined(DUMP_DATA) */
#define DUMP_PKT(pBuf,len)      do {} while(0)
#endif

#endif  /* defined(__NBUFF_H_INCLUDED__) */
