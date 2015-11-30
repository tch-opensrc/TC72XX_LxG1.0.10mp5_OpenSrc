#ifndef __PKTDMASTRUCTS_H_INCLUDED__
#define __PKTDMASTRUCTS_H_INCLUDED__

#include "6362_map.h"

typedef struct BcmPktDma_LocalEthRxDma
{
   int      enetrxchannel_isr_enable;
   volatile DmaChannelCfg *rxDma;
   volatile DmaDesc *rxBds;
   int      rxAssignedBds;
   int      rxHeadIndex;
   int      rxTailIndex;
   int      numRxBds;
   volatile int  rxEnabled;
} BcmPktDma_LocalEthRxDma;
                        
typedef struct BcmPktDma_LocalEthTxDma
{
   int      txFreeBds; /* # of free transmit bds */
   int      txHeadIndex;
   int      txTailIndex;
   volatile DmaChannelCfg *txDma; /* location of transmit DMA register set */
#if (defined(CONFIG_BCM93329) && defined(DBL_DESC))
    volatile DmaDesc16 *txBds; /* Memory location of tx Dma BD ring */
#else
    volatile DmaDesc *txBds;   /* Memory location of tx Dma BD ring */
#endif
   uint32   *   pKeyPtr;
   int      *   pTxSource;   /* HOST_VIA_LINUX, HOST_VIA_DQM, FAP_XTM_RX, FAP_ETH_RX per BD */
   uint32   *   pTxAddress;  /* Shadow copy of virtual TxBD address to be used in free of tx buffer */
   volatile int txEnabled;
} BcmPktDma_LocalEthTxDma;


#endif
