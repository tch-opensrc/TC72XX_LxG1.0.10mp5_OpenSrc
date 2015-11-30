/*
<:copyright-gpl 
 Copyright 2007 Broadcom Corp. All Rights Reserved. 
 
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
/**************************************************************************
 * File Name  : bcmxtmrtimpl.h
 *
 * Description: This file contains constant definitions and structure
 *              definitions for the BCM6368 ATM/PTM network device driver.
 ***************************************************************************/

#if !defined(_BCMXTMRTIMPL_H)
#define _BCMXTMRTIMPL_H

#define XTM_CACHE_SMARTFLUSH

#define CHIP_REV_BCM6368B0          0x636800b0
#define MAX_DEV_CTXS                16
#define MAX_MATCH_IDS               128
#define MAX_DEFAULT_MATCH_IDS       16
#define ENET_8021Q_SIZE             4
#define MAX_MTU_SIZE                (1500 + 14 + 4 + ENET_8021Q_SIZE)
#ifdef XTM_CACHE_SMARTFLUSH
#define MAX_RFC2684_HDR_SIZE        10
#else
#define MAX_RFC2684_HDR_SIZE        SKB_DATA_ALIGN(10)
#endif
#define SAR_DMA_MAX_BURST_LENGTH    8
#define RXBUF_FKB_INPLACE           ((sizeof(FkBuff_t) + 0x0f) & ~0x0f)
#define RXBUF_HEAD_RESERVE          ((176 + 0x0f) & ~0x0f)
#define RXBUF_SIZE                  SKB_DATA_ALIGN(MAX_MTU_SIZE +       \
                                        MAX_RFC2684_HDR_SIZE +          \
                                        SAR_DMA_MAX_BURST_LENGTH)
#define RXBUF_ALLOC_SIZE            SKB_DATA_ALIGN(RXBUF_FKB_INPLACE +  \
                                        RXBUF_HEAD_RESERVE +            \
                                        RXBUF_SIZE+                     \
                                        sizeof(struct skb_shared_info))
#define MAX_BUFMEM_BLOCKS           64
#define SKB_ALIGNED_SIZE            ((sizeof(struct sk_buff) + 0x0f) & ~0x0f)
#define RFC1626_MTU                 9180

#define SAR_RX_INT_ID_BASE          INTERRUPT_ID_ATM_DMA_0
#define SAR_TX_INT_ID_BASE          INTERRUPT_ID_ATM_DMA_4
#define SAR_RX_DMA_BASE_CHAN        0
#define NR_SAR_RX_DMA_CHANS         2
#define SAR_TX_DMA_BASE_CHAN        4
#define NR_SAR_TX_DMA_CHANS         16
#define SAR_TIMEOUT                 (HZ/20)
#define INVALID_VCID                0xff
#ifndef CARDNAME
#define CARDNAME                    "bcmxtmrt"
#endif

#define CACHE_TO_NONCACHE(x)        KSEG1ADDR(x)
#define NONCACHE_TO_CACHE(x)        KSEG0ADDR(x)

#define XTMRT_UNINITIALIZED         0
#define XTMRT_INITIALIZED           1
#define XTMRT_RUNNING               2

#define SKB_PROTO_ATM_CELL          0xf000
#define XTM_POLL_DONE               0x80000000

/* Circuit types. */
#define XCT_TRANSPARENT             0x00000001
#define XCT_AAL0_PKT                0x00000002
#define XCT_AAL0_CELL               0x00000003
#define XCT_OAM_F5_SEG              0x00000004
#define XCT_OAM_F5_E2E              0x00000005
#define XCT_RM                      0x00000006
#define XCT_AAL5                    0x00000007
#define XCT_ASM_P0                  0x00000008
#define XCT_ASM_P1                  0x00000009
#define XCT_ASM_P2                  0x0000000a
#define XCT_ASM_P3                  0x0000000b
#define XCT_OAM_F4_SEG              0x0000000c
#define XCT_OAM_F4_E2E              0x0000000d
#define XCT_TEQ                     0x0000000e
#define XCT_PTM                     0x0000000f

/* Transmit Buffer Descriptor frame status word for ATM/PTM. */
#define FSTAT_MASK                  0x00000fff
#define FSTAT_ATM_VCID_MASK         0x0000000f
#define FSTAT_ATM_VCID_SHIFT        0
#define FSTAT_PTM_CRC               0x00000001
#define FSTAT_PTM_ENET_FCS          0x00000002
#define FSTAT_CT_MASK               0x000000f0
#define FSTAT_CT_SHIFT              4
#define FSTAT_CT_TRANSPARENT        (XCT_TRANSPARENT << FSTAT_CT_SHIFT)
#define FSTAT_CT_AAL0_PKT           (XCT_AAL0_PKT    << FSTAT_CT_SHIFT)
#define FSTAT_CT_AAL0_CELL          (XCT_AAL0_CELL   << FSTAT_CT_SHIFT)
#define FSTAT_CT_OAM_F5_SEG         (XCT_OAM_F5_SEG  << FSTAT_CT_SHIFT)
#define FSTAT_CT_OAM_F5_E2E         (XCT_OAM_F5_E2E  << FSTAT_CT_SHIFT)
#define FSTAT_CT_RM                 (XCT_RM          << FSTAT_CT_SHIFT)
#define FSTAT_CT_AAL5               (XCT_AAL5        << FSTAT_CT_SHIFT)
#define FSTAT_CT_ASM_P0             (XCT_ASM_P0      << FSTAT_CT_SHIFT)
#define FSTAT_CT_ASM_P1             (XCT_ASM_P1      << FSTAT_CT_SHIFT)
#define FSTAT_CT_ASM_P2             (XCT_ASM_P2      << FSTAT_CT_SHIFT)
#define FSTAT_CT_ASM_P3             (XCT_ASM_P3      << FSTAT_CT_SHIFT)
#define FSTAT_CT_OAM_F4_SEG         (XCT_OAM_F4_SEG  << FSTAT_CT_SHIFT)
#define FSTAT_CT_OAM_F4_E2E         (XCT_OAM_F4_E2E  << FSTAT_CT_SHIFT)
#define FSTAT_CT_TEQ                (XCT_TEQ         << FSTAT_CT_SHIFT)
#define FSTAT_CT_PTM                (XCT_PTM         << FSTAT_CT_SHIFT)
#define FSTAT_COMMON_INS_HDR_EN     0x00000100
#define FSTAT_COMMON_HDR_INDEX_MASK 0x00000600
#define FSTAT_COMMON_HDR_INDEX_SHIFT 9
#define FSTAT_INDEX_CI              0x00000100
#define FSTAT_INDEX_CLP             0x00000200
#define FSTAT_INDEX_USE_ALT_GFC     0x00000400
#define FSTAT_MODE_INDEX            0x00000000
#define FSTAT_MODE_COMMON           0x00000800

/* Receive Buffer Descriptor frame status word for ATM/PTM. */
#define FSTAT_MATCH_ID_MASK         0x0000007f             
#define FSTAT_MATCH_ID_SHIFT        0
#define FSTAT_PACKET_CELL_MASK      0x00000400
#define FSTAT_PACKET                0x00000000
#define FSTAT_CELL                  0x00000400
#define FSTAT_ERROR                 0x00000800

/* First byte of a received cell. */
#define CHDR_CT_MASK                0x0f
#define CHDR_CT_SHIFT               0
#define CHDR_CT_OAM_F5_SEG          (XCT_OAM_F5_SEG  << CHDR_CT_SHIFT)
#define CHDR_CT_OAM_F5_E2E          (XCT_OAM_F5_E2E  << CHDR_CT_SHIFT)
#define CHDR_CT_RM                  (XCT_RM          << CHDR_CT_SHIFT)
#define CHDR_CT_ASM_P0              (XCT_ASM_P0      << CHDR_CT_SHIFT)
#define CHDR_CT_ASM_P1              (XCT_ASM_P1      << CHDR_CT_SHIFT)
#define CHDR_CT_ASM_P2              (XCT_ASM_P2      << CHDR_CT_SHIFT)
#define CHDR_CT_ASM_P3              (XCT_ASM_P3      << CHDR_CT_SHIFT)
#define CHDR_CT_OAM_F4_SEG          (XCT_OAM_F4_SEG  << CHDR_CT_SHIFT)
#define CHDR_CT_OAM_F4_E2E          (XCT_OAM_F4_E2E  << CHDR_CT_SHIFT)
#define CHDR_PORT_MASK              0x60
#define CHDR_PORT_SHIFT             5
#define CHDR_ERROR                  0x80
#define CHDR_ERROR_MISC             0x01
#define CHDR_ERROR_CRC              0x02
#define CHDR_ERROR_CAM              0x04
#define CHDR_ERROR_HEC              0x08
#define CHDR_ERROR_PORT             0x10

/* Information about a DMA transmit channel. A device instance may use more
 * than one transmit DMA channel. A DMA channel corresponds to a transmit queue.
 */
 
struct bcmxtmrt_dev_context;   /* forward reference */

typedef struct TxQInfo
{
    UINT32 ulPort;
    UINT32 ulPtmPriority;
    UINT32 ulSubPriority;
    UINT32 ulQueueSize;
    UINT32 ulDmaIndex;

    UINT8 *pMemBuf;
    volatile DmaChannelCfg *pDma;
    volatile DmaDesc *pBds;
    void **ppNBuffs;
    UINT32 ulNumTxBufsQdOne;
    UINT32 ulFreeBds;
    UINT32 ulHead;
    UINT32 ulTail;
    int    txSource[NR_TX_BDS];   /* HOST_VIA_LINUX, HOST_VIA_DQM, FAP_XTM_RX, FAP_ETH_RX */
    struct bcmxtmrt_dev_context * pdevctx;  /* ptr to bcmxtmrt_dev_context containing this TxQInfo */
    volatile int txEnabled;
} TXQINFO, *PTXQINFO;

/* The definition of the driver control structure */
typedef struct bcmxtmrt_dev_context
{
    /* Linux structures. */
    struct net_device *pDev;        
    struct net_device_stats DevStats;
    IOCTL_MIB_INFO MibInfo;
    struct ppp_channel Chan;

    /* ATM/PTM fields. */
    XTM_ADDR Addr;
    UINT32 ulLinkState;
    UINT32 ulPortDataMask ;
    UINT32 ulOpenState;
    UINT32 ulAdminStatus;
    UINT32 ulHdrType;
    UINT32 ulEncapType; /* IPoA, PPPoA, or EoA[bridge,MER,PPPoE] */
    UINT32 ulFlags;

    /* Transmit fields. */
    UINT8 ucTxVcid;
    UINT32 ulTxQInfosSize;
    TXQINFO TxQInfos[MAX_TRANSMIT_QUEUES];
    PTXQINFO pTxPriorities[MAX_PTM_PRIORITIES][MAX_PHY_PORTS][MAX_SUB_PRIORITIES];

    /*Port Mirroring fields*/
    char szMirrorIntfIn[MIRROR_INTF_SIZE];
    char szMirrorIntfOut[MIRROR_INTF_SIZE];

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    struct napi_struct napi;
#endif  
} BCMXTMRT_DEV_CONTEXT, *PBCMXTMRT_DEV_CONTEXT;

typedef struct RxBdInfo
{
    volatile DmaDesc *pBdBase;
    volatile DmaDesc *pBdHead;
    volatile DmaDesc *pBdTail;
    volatile DmaChannelCfg *pDma;
    volatile int      rxEnabled;
} RXBDINFO, *PRXBDINFO;

/* PTM Bonding Common Definitions */

#define ETH_FCS_LEN                          4
#define XTMRT_PTM_BOND_FRAG_HDR_SIZE         2
#define XTMRT_PTM_BOND_FRAG_HDR_EOP          1
#define XTMRT_PTM_BOND_FRAG_HDR_SOP          2

typedef struct _XtmRtPtmBondFragHdr {
   UINT16 seqNo      : 14 ;
   UINT16 rxFlags    :  2 ;
} XtmRtPtmBondFragHdr ;

/* PTM Tx Bonding Definitions */

#define XTMRT_PTM_BOND_MAX_FRAG_PER_PKT      8
#define XTMRT_PTM_BOND_CRC_SIZE              2
#define XTMRT_PTM_BOND_TX_MAX_FRAGMENT_SIZE  500
#define XTMRT_PTM_BOND_TX_MIN_FRAGMENT_SIZE  72
#define XTMRT_PTM_BOND_HDR_NON_EOP           0

typedef union _XtmRtPtmTxBondHeader {
   struct _sVal {
      UINT16 portSel    : 1 ;
   UINT16 Reserved   : 2 ;
   UINT16 PktEop     : 1 ;
      UINT16 FragSize   : 12 ;  /* Includes size of the fragment + 2bytes of frag hdr + 2bytes of CRC-16 */
   } sVal ;
   UINT16  usVal ;
} XtmRtPtmTxBondHeader ;

/* PTM Rx Bonding Definitions */

/* tune both the following timeout factors together on receive queue */
#define RX_SEQ_TIMEOUT        (msecs_to_jiffies(1000))
#define MAX_TICK_COUNT        (500/SAR_TIMEOUT)

/* Commands for the resync method */
#define RESYNC_TIMEOUT        0x01
#define RESYNC_OVERFLOW       0x02
#define RESYNC_PORT           0x04
#define RESYNC_FLUSH          0x08
#define RESYNC_STARTUP        0x10

#define RESYNC_LIMIT          10
#define DROP_WEIGHT           5
#define SEND_WEIGHT           1

#define Q_INDEX(i)            ((i)&XTMRT_PTM_BOND_RX_QUEUE_LEN_MASK)

                              /* Change all if required */
#define MAX_SEQ_NR            (0x3fff)

/* Split horizon Delta between two sequence numbers */
/* result is > 0 if i > j, < 0 otherwise */
/* if i<j:
 *  i-j > -R/2 => D = i-j
 *  i-j < -R/2 => D = i-j+R+1
 * if i>= j:
 *  i-j < R/2 => D = i-j
 *  i-j > R/2 => D = i-j-R-1
 */
#define DELTA_SEQ_NR(j, i)		\
	((i)<(j)) ? (((i)-(j)>-MAX_SEQ_NR/2)? ((i)-(j)) : ((i)-(j)+MAX_SEQ_NR+1)) : (((i)-(j)>(MAX_SEQ_NR+1)/2)? ((i)-(j)-MAX_SEQ_NR-1) : ((i)-(j)))

#define SEQ_NR(fkb)    (((XtmRtPtmBondFragHdr*)(fkb)->data)->seqNo)
#define RX_FLAGS(fkb)  (((XtmRtPtmBondFragHdr*)(fkb)->data)->rxFlags)
#define ABSOLUTE(i)         (((i)>=0)?(i):-(i))
#define MAX_SEQ_NR_DEVIATION XTMRT_PTM_BOND_RX_QUEUE_LEN
#define NEXT_SEQ_NR(i) (((i)+1)&MAX_SEQ_NR)
#define INC_SEQ_NR(i,seq) (((i)+seq)&MAX_SEQ_NR)

                                            /* Change both the following
                                             * together, if..reqd. *
                                             * For 2ms delay differential.
                                             * @ rates 50 ms DS
                                             */
#define XTMRT_PTM_BOND_RX_QUEUE_LEN          2048
#define XTMRT_PTM_BOND_RX_QUEUE_LEN_MASK     2047

typedef struct _XtmRtPtmBondRxQInfo {
	FkBuff_t               *rxFkb ;
   PBCMXTMRT_DEV_CONTEXT  pDevCtx ;
   UINT16                 rxFkbStatus ;
   UINT8                  phyPort ;
   UINT8                  valid ;
} XtmRtPtmBondRxQInfo ;

#define PACKET_BLOG           0
#define PACKET_NORMAL         1

typedef struct _XtmRtPtmBondInfo {
	XtmRtPtmBondRxQInfo  rxq [XTMRT_PTM_BOND_RX_QUEUE_LEN] ;   /* reordering fkb buff queue */
#define PTMBOND_FWD_BUF_INDEX             0
#define PTMBOND_SCRATCHPAD_BUF_INDEX      1
	XtmRtPtmBondRxQInfo  fwd [2];          /* fwd[0], current forward buffer,
                                             fwd[1], scratchpad, used for processing */
   //int                  lastRxPktSz ;
   int                  lastTxPktSz ;
   int                  lastTxFrags ;
	int                  expectedSeqNum;	/* expected rx seq number */
	int                  lastSeqNum [MAX_BOND_PORTS];   /* last rx seq number per port */
	int                  firstSeqNum [MAX_BOND_PORTS];   /* first rx seq number per port */
	int                  lastRxFlags;		/* last rx flags */
	int                  dropMonitor ;		/* successive rx errors force queue resync */
	int                  rxFragQueued;     /* rx number of fragments that are in the queue */
	unsigned long        rxLastFrag;		   /* time last fragment was received */
   unsigned long        tickCount ;

	/* (Error) counters */
   struct _bStats {
	UINT32               rfw;				   /* rx forwarded fragments */
	UINT32               rxo;				   /* rx forwarded octets counter */
	UINT32               rxp;				   /* rx forwarded packets counter */
      UINT32               rxpl;				   /* rx forwarded packets counter */
	UINT32               old;              /* rx old sequence number received (past) */
	UINT32               dup;              /* rx duplicated sequence number received */
	UINT32               oos;              /* rx out-of-sync fragments dropped */
	UINT32               usq;              /* rx unexpected sequence number from queue store */
	UINT32               eop;              /* rx unexpected in-sequence mid- or end-of-packet */
	UINT32               sop;              /* rx unexpected in-sequence start-of-packet */
	UINT32               ess;              /* rx expected sequence number skip */
	UINT32               end;              /* rx artificially ended packets */
      UINT32               stp;              /* rx dropped due to sync at the startup */
	UINT32               bqo;              /* rx dropped due to flush after buffer queue overrun. */
	UINT32               flu;              /* rx dropped due to queue flush */
	UINT32               tim;              /* rx dropped due to expected sequence number timeout */
	UINT32               dro;              /* rx dropped due to line removal */
	UINT32               inl;              /* rx fragment received from inactive line */
	UINT32               cpe;              /* times of buffer expand and copy */

	/* Per line counters */
	UINT32               rfg [MAX_BOND_PORTS];	/* rx fragments counter */
   } bStats ;
	
#ifdef PTM_BONDING_DEBUG
   int                  logLevel ;
#define MAX_SEQ_DUMP 750
	int                  seqNum;
	UINT32               seqLen[MAX_SEQ_DUMP];
	XtmRtPtmBondFragHdr  seqFlags[MAX_SEQ_DUMP];
	UINT32               port[MAX_SEQ_DUMP];
#endif
} XtmRtPtmBondInfo ;

/* Information that is global to all network device instances. */
typedef struct bcmxtmrt_global_info
{
    /* Linux structures. */
    PBCMXTMRT_DEV_CONTEXT pDevCtxs[MAX_DEV_CTXS];
    PBCMXTMRT_DEV_CONTEXT pDevCtxsByMatchId[MAX_MATCH_IDS];
    struct timer_list Timer;
    struct atm_dev *pAtmDev;

    spinlock_t xtmlock_tx;
    spinlock_t xtmlock_rx;
    spinlock_t xtmlock_rx_regs;
    spinlock_t xtmlock_rx_bond;

    /* DMA, BD and buffer fields. */
    volatile DmaRegs *pDmaRegs;
    volatile DmaChannelCfg *pTxDmaBase;
    volatile DmaChannelCfg *pRxDmaBase;
    volatile DmaDesc *pRxBdMem;
    RXBDINFO RxBdInfos[MAX_RECEIVE_QUEUES];
    struct sk_buff *pFreeRxSkbList;
    UINT8 *pBufMem[MAX_BUFMEM_BLOCKS];
    UINT32 ulIntEnableMask;
    int rxIrq[MAX_RECEIVE_QUEUES];   /* rx dma irq */
    
    /* Global transmit queue fields. */
    UINT32 ulNumExtBufs;
    UINT32 ulNumExtBufsRsrvd;
    UINT32 ulNumExtBufs90Pct;
    UINT32 ulNumExtBufs50Pct;
    UINT32 ulNumTxQs;
    UINT32 ulNumTxBufsQdAll;
    UINT32 ulDbgQ1;
    UINT32 ulDbgQ2;
    UINT32 ulDbgQ3;
    UINT32 ulDbgD1;
    UINT32 ulDbgD2;
    UINT32 ulDbgD3;

    /* Callback functions. */
    XTMRT_CALLBACK pfnOamHandler;
    void *pOamContext;
    XTMRT_CALLBACK pfnAsmHandler;
    void *pAsmContext;

    /* MIB counter registers. */
    UINT32 *pulMibTxOctetCountBase;
    UINT32 ulMibRxClrOnRead;
    UINT32 *pulMibRxCtrl;
    UINT32 *pulMibRxMatch;
    UINT32 *pulMibRxOctetCount;
    UINT32 *pulMibRxPacketCount;
    UINT32 *pulRxCamBase;

    /* Bonding information */
    UINT32                 bondVersionMajor ;
    UINT32                 bondVersionMinor ;
    UINT32                 bondVersionBuild ;
    XtmBondConfig          bondConfig ;
    XtmRtPtmTxBondHeader   ptmBondHdr [XTMRT_PTM_BOND_MAX_FRAG_PER_PKT] ;
    XtmRtPtmBondInfo       ptmBondInfo ;

    /* Everything else. */
    UINT32 ulChipRev;
    UINT32 ulDrvState;
} BCMXTMRT_GLOBAL_INFO, *PBCMXTMRT_GLOBAL_INFO;

extern BCMXTMRT_GLOBAL_INFO g_GlobalInfo;

#define PTM_BOND_INITIALIZE_LOCAL      0
#define PTM_BOND_INITIALIZE_GLOBAL     1
/* Function Prototypes */
void FlushAssignRxBuffer(PRXBDINFO pRxBdInfo, UINT8 *pucData, UINT8 *pucEnd);
void bcmxtmrt_ptmbond_initialize (int) ;
UINT32 bcmxtmrt_process_rx_pkt( PBCMXTMRT_DEV_CONTEXT pDevCtx,
                              FkBuff_t *pFkb, UINT16 bufStatus, int delLen) ;
void bcmxtmrt_ptmbond_add_hdr (PBCMXTMRT_DEV_CONTEXT pDevCtx, pNBuff_t *ppNBuff,
                               struct sk_buff **ppNBuffSkb, UINT8 **ppData, 
                               int *pLen) ;
void bcmxtmrt_ptmbond_receive_rx_fragment (PBCMXTMRT_DEV_CONTEXT pDevCtx, FkBuff_t *pFkb,
                                           UINT16 bufStatus) ;
void bcmxtmrt_ptmbond_handle_port_status_change (XtmRtPtmBondInfo *pBondInfo,
                                                 UINT32 ulLinkState, UINT32 ulPortMask) ;
void bcmxtmrt_ptmbond_tick (XtmRtPtmBondInfo *pBondInfo) ;
void *bond_memcpy (void * dest, void const * src, size_t cnt) ;

int ProcRxBondCtrs(char *page, char **start, off_t off, int cnt, int *eof, void *data);
int ProcRxBondSeq0(char *page, char **start, off_t off, int cnt, int *eof, void *data);
int ProcRxBondSeq1(char *page, char **start, off_t off, int cnt, int *eof, void *data);
int ProcTxBondInfo(char *page, char **start, off_t off, int cnt, int *eof, void *data);

#endif /* _BCMXTMRTIMPL_H */
