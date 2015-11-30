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

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/nbuff.h>

#ifdef CC_CONFIG_FKB_FN_INLINE
#define DECLARE_FKB_FN(fn_identifier, fn_signature, body)
#else
#define DECLARE_FKB_FN(fn_identifier, fn_signature, body)                     \
fn_signature { body; }                                                        \
EXPORT_SYMBOL(fn_identifier);
#endif

#ifdef CC_CONFIG_FKB_COLOR
#define COLOR(clr_code)     clr_code
#else
#define COLOR(clr_code)
#endif
#define CLRb                COLOR("\e[0;34m")       /* blue */
#define CLRc                COLOR("\e[0;36m")       /* cyan */
#define CLRn                COLOR("\e[0m")          /* normal */
#define CLRerr              COLOR("\e[0;33;41m")    /* yellow on red */
#define CLRN                CLRn"\n"                /* normal newline */

int nbuff_dbg = 0;
#if defined(CC_CONFIG_FKB_DEBUG)
#define fkb_print(fmt, arg...)                                          \
    printk( CLRc "FKB %s :" fmt CLRN, __FUNCTION__, ##arg )
#define fkb_assertv(cond)                                               \
    if ( !cond ) {                                                      \
        printk( CLRerr "FKB ASSERT %s : " #cond CLRN, __FUNCTION__ );   \
        return;                                                         \
    }
#define fkb_assertr(cond, rtn)                                          \
    if ( !cond ) {                                                      \
        printk( CLRerr "FKB ASSERT %s : " #cond CLRN, __FUNCTION__ );   \
        return rtn;                                                     \
    }
#define FKB_DBG(debug_code)     do { debug_code } while(0)
#else
#define fkb_print(fmt, arg...)  NULL_STMT
#define fkb_assertv(cond)       NULL_STMT
#define fkb_assertr(cond, rtn)  NULL_STMT
#define FKB_DBG(debug_code)     NULL_STMT
#endif


/*
 *------------------------------------------------------------------------------
 * Pre-allocated Pool of FkBuff_t objects for use in fkb_clone
 *------------------------------------------------------------------------------ */

/* Global pointer to the free pool of FKBs */
static FkBuff_t * fkb_freelist_gp = FKB_NULL;
static int fkb_extends = 0;

#if defined(CC_CONFIG_FKB_STATS)
static int fkb_cnt_free = 0;        /* Number of FKB objects that are free    */
static int fkb_cnt_used = 0;        /* Number of in use FKB objects           */
static int fkb_cnt_hwm  = 0;        /* In use high water mark for engineering */
static int fkb_cnt_fails = 0;

void fkb_stats(void)
{
    printk("\textends<%d> free<%d> used<%d> HWM<%d> fails<%d>\n",
           fkb_extends, fkb_cnt_free, fkb_cnt_used, fkb_cnt_hwm, fkb_cnt_fails);
}
#define FKB_STATS(stats_code)   do { stats_code } while(0)
#else
#define FKB_STATS(stats_code)   NULL_STMT
void fkb_stats(void) {}
#define fkb_stats()             NULL_STMT
#endif


/*
 *------------------------------------------------------------------------------
 * Test whether an FKB may be translated onto a skb.
 *------------------------------------------------------------------------------
 */
int fkb_in_skb_test(int fkb_offset, int list_offset, int blog_p_offset,
                    int data_offset, int len_offset, int mark_offset,
                    int priority_offset, int recycle_hook_offset,
                    int recycle_context_offset)
{
#undef OFFSETOF
#define OFFSETOF(stype, member) ((int)&((struct stype*)0)->member)       
#define FKBOFFSETOF(member)   (member##_offset)
#define SKBOFFSETOF(member)   (((int)&((struct sk_buff*)0)->member)-fkb_offset)
#define FKBSIZEOF(member)     (sizeof(((struct fkbuff*)0)->member))
#define SKBSIZEOF(member)     (sizeof(((struct sk_buff*)0)->member))
#define FKBINSKB_TEST(member) ((FKBOFFSETOF(member) == SKBOFFSETOF(member)) \
                              && (FKBSIZEOF(member) == SKBSIZEOF(member)))

    if ( OFFSETOF(sk_buff, fkbInSkb) != fkb_offset)
        return 0;
    if (  FKBINSKB_TEST(list) && FKBINSKB_TEST(blog_p) && FKBINSKB_TEST(data)
       && FKBINSKB_TEST(len) && FKBINSKB_TEST(mark) && FKBINSKB_TEST(priority)
       && FKBINSKB_TEST(recycle_hook) && FKBINSKB_TEST(recycle_context)
       // && sizeof(struct fkbuff) == (2 * sizeof cacheline)
       && ((fkb_offset & 0xF) == 0) ) /* ____cacheline_aligned */
        return 1;
    return 0;
}

/*
 *------------------------------------------------------------------------------
 * Function   : fkb_extend
 * Description: Create a pool of FKB objects. When a pool is exhausted
 *              this function may be invoked to extend the pool. The pool is
 *              identified by a global pointer, fkb_freelist_gp. All objects in
 *              the pool chained together in a single linked list.
 * Parameters :
 *   num      : Number of FKB objects to be allocated.
 * Returns    : Number of FKB objects allocated in pool.
 *------------------------------------------------------------------------------
 */
static uint32_t fkb_extend(uint32_t num)
{
    register int i;
    register FkBuff_t * list;

    fkb_print( "fkb_extend %u", num );

    list = (FkBuff_t *) kmalloc( num * sizeof(FkBuff_t), GFP_ATOMIC);
    if ( unlikely(list == FKB_NULL) )   /* may fail if in_interrupt or oom */
    {
        FKB_STATS( fkb_cnt_fails++; );
        fkb_print( "WARNING: Failure to initialize %d FKB objects", num );
        return 0;
    }
    fkb_extends++;

    /* memset( (void *)list, 0, (sizeof(FkBuff_t) * num ) ); */
    for ( i = 0; i < num; i++ )
        list[i].list = &list[i+1];

    local_irq_disable();

    FKB_STATS( fkb_cnt_free += num; );

    list[num-1].list = fkb_freelist_gp; /* chain last FKB object */
    fkb_freelist_gp = list;             /* head of list */

    local_irq_enable();

    return num;
}

/*
 *------------------------------------------------------------------------------
 * Function     : fkb_construct
 * Description  : Incarnates the fkb system pool during kernel/module init
 *------------------------------------------------------------------------------
 */
int fkb_construct(int fkb_in_skb_offset)
{
#undef FKBOFFSETOF
#define FKBOFFSETOF(member)   ((int)&((struct fkbuff*)0)->member)
    if ( fkb_in_skb_test(fkb_in_skb_offset,
                FKBOFFSETOF(list), FKBOFFSETOF(blog_p), FKBOFFSETOF(data),
                FKBOFFSETOF(len), FKBOFFSETOF(mark), FKBOFFSETOF(priority),
                FKBOFFSETOF(recycle_hook), FKBOFFSETOF(recycle_context)) == 0 )
        return -1;
    else
        FKB_DBG( printk(CLRb "Fkbuff compatible with SkBuff" CLRN); );

    fkb_extend( FKB_POOL_SIZE_ENGG );

    FKB_DBG( printk(CLRb "NBUFF nbuff_dbg<0x%08x> = %d\n"
                         "%d FkBuffs allocated of size %d" CLRN,
                         (int)&nbuff_dbg, nbuff_dbg,
                         FKB_POOL_SIZE_ENGG, sizeof(FkBuff_t) ); );

    printk( CLRb "NBUFF %s Initialized" CLRN, NBUFF_VERSION );
    return 0;
}

/*
 *------------------------------------------------------------------------------
 * Function     : fkb_alloc
 * Description  : Allocate an FKB from the free list
 * Returns      : Pointer to an FKB or NULL, on depletion.
 *------------------------------------------------------------------------------
 */
FkBuff_t * fkb_alloc(void)
{
    register FkBuff_t * fkb_p;

    fkb_dbg(2, "fkb_freelist_gp<0x%08x>", (int)fkb_freelist_gp);
    if ( unlikely(fkb_freelist_gp == FKB_NULL) )
    {
#ifdef SUPPORT_FKB_EXTEND
        if ( (fkb_extends >= FKB_EXTEND_MAX_ENGG)  /* Try extending free pool */
          || (fkb_extend( FKB_EXTEND_SIZE_ENGG ) != FKB_EXTEND_SIZE_ENGG) )
        {
            fkb_print( "WARNING: free list exhausted" );
            return FKB_NULL;
        }
#else
        if ( fkb_extend( FKB_EXTEND_SIZE_ENGG ) == 0 )
        {
            fkb_print( "WARNING: out of memory" );
            return FKB_NULL;
        }
#endif
    }

    local_irq_disable();

    FKB_STATS(
        fkb_cnt_free--;
        if ( ++fkb_cnt_used > fkb_cnt_hwm )
            fkb_cnt_hwm = fkb_cnt_used;
        );

    fkb_p = fkb_freelist_gp;
    fkb_freelist_gp = fkb_freelist_gp->list;

    local_irq_enable();

    fkb_set_ref(fkb_p, 0);
    fkb_dbg(1, "fkb_p<0x%08x>", (int)fkb_p);

    return fkb_p;
}

/*
 *------------------------------------------------------------------------------
 * Function     : fkb_free
 * Description  : Free an FKB and associated buffer if reference count of the
 *                buffer is 0. If the fkb came from the private free list then
 *                it is returned. All cloned fkb's are allocated from the global
 *                private free list.
 *------------------------------------------------------------------------------
 */
void fkb_free(FkBuff_t * fkb_p)
{
    FkBuff_t * fkbM_p;

    fkb_assertv( (fkb_p!=FKB_NULL) );
    fkb_dbg(1, "fkb_p<0x%08x>", (int)fkb_p);

    /* FkBuff should never point to a Blog, so no need to free fkb_p->blog_p */
    if ( unlikely(IS_FKB_POOL(fkb_p)) )
    {
        fkbM_p = fkb_p->master_p;

        local_irq_disable();

        FKB_STATS( fkb_cnt_used--; fkb_cnt_free++; );

        fkb_p->list = fkb_freelist_gp;
        fkb_freelist_gp = fkb_p;    /* link into free pool of fkb */

        local_irq_enable();
    }
    else
        fkbM_p = fkb_p;

    fkb_assertv( (atomic_read(&fkbM_p->users) > 0) );

    if ( likely(atomic_dec_and_test(&fkbM_p->users)) )
    {
        fkbM_p->recycle_hook(FKBUFF_2_PNBUFF(fkbM_p),
                             fkbM_p->recycle_context, 0);
    }

    fkb_dbg(2, "fkbM_p<0x%08x>", (int)fkbM_p);
}

/*
 *------------------------------------------------------------------------------
 * Function     : nbuff_xlate
 * Description  : Translates an fkb to an skb allocated from kernel skb cache.
 *                The fkb may be from the pre-allocated pool or created via
 *                placement in the RxBuffer.
 *------------------------------------------------------------------------------
 */
extern struct sk_buff * skb_xlate(struct fkbuff * fkb_p);

struct sk_buff * nbuff_xlate( pNBuff_t pNBuff )
{
    if ( unlikely(IS_SKBUFF_PTR(pNBuff)) )
        return PNBUFF_2_SKBUFF(pNBuff);
    /* else if IS_FPBUFF_PTR, else if IS_TGBUFF_PTR */
    else    /* if IS_FKBUFF_PTR(pNBuff) */
    {
        FkBuff_t * fkb_p;
        struct sk_buff * skb_p;

        fkb_p = (FkBuff_t *)PNBUFF_2_PBUF(pNBuff); 

        if ( unlikely(fkb_p == FKB_NULL) )
            return (struct sk_buff *)NULL;

        if ( unlikely(IS_FKB_CLONE(fkb_p)) )
        {
            /* Unshare the fkb first ..., fail for now */
            printk( "FKB MCAST FIXME : xlate cloned fkb <%d>\n", 
                    atomic_read(&fkb_p->users) );
            goto clone_fail;
        }

        fkb_p->blog_p = BLOG_NULL;

        /* Translate the fkb_p to a skb_p  */
        skb_p = skb_xlate(fkb_p);

        if ( unlikely(skb_p == (struct sk_buff *)NULL) )
            goto clone_fail;

        /* pNBuff may not be used henceforth */
        return skb_p;

clone_fail:
        fkb_free( fkb_p );
        return (struct sk_buff *)NULL;
    }

}

/* Non-inlined API versions */
DECLARE_FKB_FN( fkb_set_ref,
    void fkb_set_ref(FkBuff_t * fkb_p, const int count),
    _fkb_set_ref(fkb_p, count) )

DECLARE_FKB_FN( fkb_inc_ref,
    void fkb_inc_ref(FkBuff_t * fkb_p),
    _fkb_inc_ref(fkb_p) )

DECLARE_FKB_FN( fkb_dec_ref,
    void fkb_dec_ref(FkBuff_t * fkb_p),
    _fkb_dec_ref(fkb_p) )

DECLARE_FKB_FN( fkb_init,
    FkBuff_t * fkb_init(uint8_t * pBuf, uint32_t headroom,
                        uint8_t * pData, uint32_t len),
    return _fkb_init(pBuf, headroom, pData, len) )

DECLARE_FKB_FN( fkb_qinit,
    FkBuff_t * fkb_qinit(uint8_t * pBuf, uint32_t headroom,
                        uint8_t * pData, uint32_t len, uint32_t qcontext),
    return _fkb_qinit(pBuf, headroom, pData, len, qcontext) )

DECLARE_FKB_FN( fkb_release,
    void fkb_release(FkBuff_t * fkb_p),
    _fkb_release(fkb_p) )

DECLARE_FKB_FN( fkb_headroom,
    int fkb_headroom(const FkBuff_t *fkb_p),
    return _fkb_headroom(fkb_p) )

DECLARE_FKB_FN( fkb_push,
    uint8_t * fkb_push(FkBuff_t * fkb_p, uint32_t len),
    return _fkb_push(fkb_p, len) )

DECLARE_FKB_FN( fkb_pull,
    uint8_t * fkb_pull(FkBuff_t * fkb_p, uint32_t len),
    return _fkb_pull(fkb_p, len) )

DECLARE_FKB_FN( fkb_put,
    uint8_t * fkb_put(FkBuff_t * fkb_p, uint32_t len),
    return _fkb_put(fkb_p, len) )

DECLARE_FKB_FN( fkb_pad,
    uint32_t fkb_pad(FkBuff_t * fkb_p, uint32_t padding),
    return _fkb_pad(fkb_p, padding) )

DECLARE_FKB_FN( fkb_len,
    uint32_t fkb_len(FkBuff_t * fkb_p),
    return _fkb_len(fkb_p) )

DECLARE_FKB_FN( fkb_data,
    uint8_t * fkb_data(FkBuff_t * fkb_p),
    return _fkb_data(fkb_p) )

DECLARE_FKB_FN( fkb_blog,
    struct blog_t * fkb_blog(FkBuff_t * fkb_p),
    return _fkb_blog(fkb_p) )

DECLARE_FKB_FN( fkb_clone,
    FkBuff_t * fkb_clone(FkBuff_t * fkbM_p),
    return _fkb_clone(fkbM_p) )

EXPORT_SYMBOL(fkb_in_skb_test);
EXPORT_SYMBOL(fkb_construct);
EXPORT_SYMBOL(fkb_stats);

EXPORT_SYMBOL(fkb_alloc);
EXPORT_SYMBOL(fkb_free);

EXPORT_SYMBOL(nbuff_dbg);
EXPORT_SYMBOL(nbuff_xlate);
