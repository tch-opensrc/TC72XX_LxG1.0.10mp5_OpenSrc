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
 * File Name  : bcmxtmrt.c
 *
 * Description: This file implements BCM6368 ATM/PTM network device driver
 *              runtime processing - sending and receiving data.
 ***************************************************************************/
 
/* Defines. */
#define VERSION     "0.3"
#define VER_STR     "v" VERSION " " __DATE__ " " __TIME__


/* Includes. */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/ethtool.h>
#include <linux/if_arp.h>
#include <linux/ppp_channel.h>
#include <linux/ppp_defs.h>
#include <linux/if_ppp.h>
#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/atmppp.h>
#include <linux/blog.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <bcmtypes.h>
#include <bcm_map_part.h>
#include <bcm_intr.h>
#include <board.h>
#include "bcmnet.h"
#include "bcmxtmcfg.h"
#include "bcmxtmrt.h"
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/nbuff.h>
#include "bcmxtmrtimpl.h"

static UINT32 gs_ulLogPorts[]  = {0, 3, 1, 2};
#define PORT_PHYS_TO_LOG(PP) gs_ulLogPorts[PP]

/* Externs. */
extern unsigned long getMemorySize(void);
extern int kerSysGetMacAddress(UINT8 *pucaMacAddr, UINT32 ulId);

/* Prototypes. */
int __init bcmxtmrt_init( void );
static void bcmxtmrt_cleanup( void );
static int bcmxtmrt_open( struct net_device *dev );
static int bcmxtmrt_close( struct net_device *dev );
static void bcmxtmrt_timeout( struct net_device *dev );
static struct net_device_stats *bcmxtmrt_query(struct net_device *dev);
static void bcmxtmrt_clrStats(struct net_device *dev);
static int bcmxtmrt_ioctl(struct net_device *dev, struct ifreq *Req, int nCmd);
static int bcmxtmrt_ethtool_ioctl(PBCMXTMRT_DEV_CONTEXT pDevCtx,void *useraddr);
static int bcmxtmrt_atm_ioctl(struct socket *sock, unsigned int cmd,
    unsigned long arg);
static PBCMXTMRT_DEV_CONTEXT FindDevCtx( short vpi, int vci );
static int bcmxtmrt_atmdev_open(struct atm_vcc *pVcc);
static void bcmxtmrt_atmdev_close(struct atm_vcc *pVcc);
static int bcmxtmrt_atmdev_send(struct atm_vcc *pVcc, struct sk_buff *skb);
static int bcmxtmrt_pppoatm_send(struct ppp_channel *pChan,struct sk_buff *skb);
static int bcmxtmrt_xmit(pNBuff_t pNBuff, struct net_device *dev);
static void AddRfc2684Hdr(pNBuff_t *ppNBuff, struct sk_buff **ppNbuffSkb,
    UINT8 **ppData, int * pLen, UINT32 ulHdrType);
static void bcmxtmrt_align(pNBuff_t *pNBuff, struct sk_buff **ppNBuffSkb,
    UINT8 **ppData, int * pLen, UINT32 alignmask );
static void AssignRxBuffer(PRXBDINFO pRxBdInfo, UINT8 *pucData);
static void bcmxtmrt_recycle_skb_or_data(struct sk_buff *skb, unsigned context,
    UINT32 nFlag );
static void bcmxtmrt_recycle(pNBuff_t pNBuff, unsigned context, UINT32 nFlag);
static irqreturn_t bcmxtmrt_rxisr(int nIrq, void *pRxDma);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
static int bcmxtmrt_poll_napi(struct napi_struct * napi, int budget);
#else
static int bcmxtmrt_poll(struct net_device * dev, int * budget);
#endif
static UINT32 bcmxtmrt_rxtask( UINT32 ulBudget, UINT32 *pulMoreToDo );
static void ProcessRxCell( PBCMXTMRT_GLOBAL_INFO pGi, PRXBDINFO pRxBdInfo,
    UINT8 *pucData );
static void MirrorPacket( struct sk_buff *skb, char *intfName );
static void bcmxtmrt_timer( PBCMXTMRT_GLOBAL_INFO pGi );
static int DoGlobInitReq( PXTMRT_GLOBAL_INIT_PARMS pGip );
static int DoGlobUninitReq( void );
static int DoCreateDeviceReq( PXTMRT_CREATE_NETWORK_DEVICE pCnd );
static int DoRegCellHdlrReq( PXTMRT_CELL_HDLR pCh );
static int DoUnregCellHdlrReq( PXTMRT_CELL_HDLR pCh );
static int DoLinkStsChangedReq( PBCMXTMRT_DEV_CONTEXT pDevCtx,
     PXTMRT_LINK_STATUS_CHANGE pLsc );
static int DoLinkUp( PBCMXTMRT_DEV_CONTEXT pDevCtx,
     PXTMRT_LINK_STATUS_CHANGE pLsc );
static int DoLinkDownRx( UINT32 ulPortId );
static int DoLinkDownTx( PBCMXTMRT_DEV_CONTEXT pDevCtx,
     PXTMRT_LINK_STATUS_CHANGE pLsc );
static int DoSetTxQueue( PBCMXTMRT_DEV_CONTEXT pDevCtx,
    PXTMRT_TRANSMIT_QUEUE_ID pTxQId );
static int DoUnsetTxQueue( PBCMXTMRT_DEV_CONTEXT pDevCtx,
    PXTMRT_TRANSMIT_QUEUE_ID pTxQId );
static int DoSendCellReq( PBCMXTMRT_DEV_CONTEXT pDevCtx, PXTMRT_CELL pC );
static int DoDeleteDeviceReq( PBCMXTMRT_DEV_CONTEXT pDevCtx );
static int DoGetNetDevTxChannel( PXTMRT_NETDEV_TXCHANNEL pParm );
static int bcmxtmrt_add_proc_files( void );
static int bcmxtmrt_del_proc_files( void );
static int ProcDmaTxInfo(char *page, char **start, off_t off, int cnt, 
    int *eof, void *data);


/* Globals. */
BCMXTMRT_GLOBAL_INFO g_GlobalInfo;
static struct atm_ioctl g_PppoAtmIoctlOps =
    {
        .ioctl    = bcmxtmrt_atm_ioctl,
    };
static struct ppp_channel_ops g_PppoAtmOps =
    {
        .start_xmit = bcmxtmrt_pppoatm_send
    };
static const struct atmdev_ops g_AtmDevOps =
    {
        .open       = bcmxtmrt_atmdev_open,
        .close      = bcmxtmrt_atmdev_close,
        .send       = bcmxtmrt_atmdev_send,
    };

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
static const struct header_ops bcmXtmRt_headerOps = {
    .parse = NULL
};

static const struct net_device_ops bcmXtmRt_netdevops = {

    .ndo_open           = bcmxtmrt_open,
    .ndo_stop           = bcmxtmrt_close,
    .ndo_start_xmit     = (HardStartXmitFuncP)bcmxtmrt_xmit,
    .ndo_do_ioctl       = bcmxtmrt_ioctl,
    .ndo_tx_timeout     = bcmxtmrt_timeout,
    .ndo_get_stats      = bcmxtmrt_query
 };
#endif


/***************************************************************************
 * Function Name: bcmxtmrt_init
 * Description  : Called when the driver is loaded.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
int __init bcmxtmrt_init( void )
{
    UINT16 usChipId  = (PERF->RevID & 0xFFFE0000) >> 16;
    UINT16 usChipRev = (PERF->RevID & 0xFF);

    printk(CARDNAME ": Broadcom BCM%X%X ATM/PTM Network Device ", usChipId,
        usChipRev);
    printk(VER_STR "\n");

    memset(&g_GlobalInfo, 0x00, sizeof(g_GlobalInfo));

    g_GlobalInfo.ulChipRev = PERF->RevID;
    register_atm_ioctl(&g_PppoAtmIoctlOps);
    g_GlobalInfo.pAtmDev = atm_dev_register("bcmxtmrt_atmdev", &g_AtmDevOps,
        -1, NULL);
    if( g_GlobalInfo.pAtmDev )
    {
        g_GlobalInfo.pAtmDev->ci_range.vpi_bits = 12;
        g_GlobalInfo.pAtmDev->ci_range.vci_bits = 16;
    }

    g_GlobalInfo.ulNumExtBufs = NR_RX_BDS(getMemorySize());
    g_GlobalInfo.ulNumExtBufsRsrvd = g_GlobalInfo.ulNumExtBufs / 5;
    g_GlobalInfo.ulNumExtBufs90Pct = (g_GlobalInfo.ulNumExtBufs * 9) / 10;
    g_GlobalInfo.ulNumExtBufs50Pct = g_GlobalInfo.ulNumExtBufs / 2;

    bcmxtmrt_add_proc_files();

    return( 0 );
} /* bcmxtmrt_init */


/***************************************************************************
 * Function Name: bcmxtmrt_cleanup
 * Description  : Called when the driver is unloaded.
 * Returns      : None.
 ***************************************************************************/
static void bcmxtmrt_cleanup( void )
{
    bcmxtmrt_del_proc_files();
    deregister_atm_ioctl(&g_PppoAtmIoctlOps);
    if( g_GlobalInfo.pAtmDev )
    {
        atm_dev_deregister( g_GlobalInfo.pAtmDev );
        g_GlobalInfo.pAtmDev = NULL;
    }
} /* bcmxtmrt_cleanup */


/***************************************************************************
 * Function Name: bcmxtmrt_open
 * Description  : Called to make the device operational.  Called due to shell
 *                command, "ifconfig <device_name> up".
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_open( struct net_device *dev )
{
    int nRet = 0;
 #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
   PBCMXTMRT_DEV_CONTEXT pDevCtx = netdev_priv(dev);
 #else
   PBCMXTMRT_DEV_CONTEXT pDevCtx = dev->priv;
 #endif

    netif_start_queue(dev);

    if( pDevCtx->ulAdminStatus == ADMSTS_UP )
        pDevCtx->ulOpenState = XTMRT_DEV_OPENED;
    else
        nRet = -EIO;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    napi_enable(&pDevCtx->napi);
#endif

    return( nRet );
} /* bcmxtmrt_open */


/***************************************************************************
 * Function Name: bcmxtmrt_close
 * Description  : Called to stop the device.  Called due to shell command,
 *                "ifconfig <device_name> down".
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_close( struct net_device *dev )
{
 #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
   PBCMXTMRT_DEV_CONTEXT pDevCtx = netdev_priv(dev);
 #else
   PBCMXTMRT_DEV_CONTEXT pDevCtx = dev->priv;
 #endif

    pDevCtx->ulOpenState = XTMRT_DEV_CLOSED;
    netif_stop_queue(dev);
    
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    napi_disable(&pDevCtx->napi);
#endif
    
    return 0;
} /* bcmxtmrt_close */


/***************************************************************************
 * Function Name: bcmxtmrt_timeout
 * Description  : Called when there is a transmit timeout. 
 * Returns      : None.
 ***************************************************************************/
static void bcmxtmrt_timeout( struct net_device *dev )
{
    dev->trans_start = jiffies;
    netif_wake_queue(dev);
} /* bcmxtmrt_timeout */


/***************************************************************************
 * Function Name: bcmxtmrt_query
 * Description  : Called to return device statistics. 
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static struct net_device_stats *bcmxtmrt_query(struct net_device *dev)
{
 #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
   PBCMXTMRT_DEV_CONTEXT pDevCtx = netdev_priv(dev);
 #else
   PBCMXTMRT_DEV_CONTEXT pDevCtx = dev->priv;
 #endif
    struct net_device_stats *pStats = &pDevCtx->DevStats;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 i;

    if( pDevCtx->ucTxVcid != INVALID_VCID ) {
       pStats->tx_bytes += pGi->pulMibTxOctetCountBase[pDevCtx->ucTxVcid ];
    }

    if (pGi->bondConfig.sConfig.ptmBond != BC_PTM_BONDING_ENABLE) {
       for( i = 0; i < MAX_DEFAULT_MATCH_IDS; i++ )
       {
          if( pGi->pDevCtxsByMatchId[i] == pDevCtx )
          {
             *pGi->pulMibRxMatch = i;
             pStats->rx_bytes = *pGi->pulMibRxOctetCount;
             *pGi->pulMibRxMatch = i;
             pStats->rx_packets = *pGi->pulMibRxPacketCount;

             /* By convension, VCID 0 collects statistics for packets that use
              * Packet CMF rules.
              */
             if( i == 0 )
             {
                UINT32 j;
                for( j = MAX_DEFAULT_MATCH_IDS + 1; j < MAX_MATCH_IDS; j++ )
                {
                   *pGi->pulMibRxMatch = j;
                   pStats->rx_bytes += *pGi->pulMibRxOctetCount;
                   *pGi->pulMibRxMatch = j;
                   pStats->rx_packets += *pGi->pulMibRxPacketCount;
                }
             } /* if (i) */
          }
       } /* for (i) */
    }
    else {
       pStats->rx_bytes   = pGi->ptmBondInfo.bStats.rxo ;
       pStats->rx_packets = (pGi->ptmBondInfo.bStats.rxp +
                             pGi->ptmBondInfo.bStats.rxpl) ;
    }

    return( pStats );
} /* bcmxtmrt_query */

/***************************************************************************
 * Function Name: bcmxtmrt_clrStats
 * Description  : Called to clear device statistics. 
 * Returns      : None
 ***************************************************************************/
static void bcmxtmrt_clrStats(struct net_device *dev)
{
 #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
   PBCMXTMRT_DEV_CONTEXT pDevCtx = netdev_priv(dev);
 #else
   PBCMXTMRT_DEV_CONTEXT pDevCtx = dev->priv;
 #endif
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;

    *pGi->pulMibRxCtrl |= pGi->ulMibRxClrOnRead;
    bcmxtmrt_query(dev);
    *pGi->pulMibRxCtrl &= ~pGi->ulMibRxClrOnRead;
    memset(&pDevCtx->DevStats, 0, sizeof(struct net_device_stats));
} /* bcmxtmrt_clrStats */

/***************************************************************************
 * Function Name: bcmxtmrt_ioctl
 * Description  : Driver IOCTL entry point.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_ioctl(struct net_device *dev, struct ifreq *Req, int nCmd)
{
 #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
   PBCMXTMRT_DEV_CONTEXT pDevCtx = netdev_priv(dev);
 #else
   PBCMXTMRT_DEV_CONTEXT pDevCtx = dev->priv;
 #endif
    int *data=(int*)Req->ifr_data;
    int status;
    MirrorCfg mirrorCfg;
    int nRet = 0;

    switch (nCmd)
    {
    case SIOCGLINKSTATE:
        if( pDevCtx->ulLinkState == LINK_UP )
            status = LINKSTATE_UP;
        else
            status = LINKSTATE_DOWN;
        if (copy_to_user((void*)data, (void*)&status, sizeof(int)))
            nRet = -EFAULT;
        break;

    case SIOCSCLEARMIBCNTR:
        bcmxtmrt_clrStats(dev);
        break;

    case SIOCMIBINFO:
        if (copy_to_user((void*)data, (void*)&pDevCtx->MibInfo,
            sizeof(pDevCtx->MibInfo)))
        {
            nRet = -EFAULT;
        }
        break;

    case SIOCPORTMIRROR:
        if(copy_from_user((void*)&mirrorCfg,data,sizeof(MirrorCfg)))
            nRet=-EFAULT;
        else
        {
            if( mirrorCfg.nDirection == MIRROR_DIR_IN )
            {
                if( mirrorCfg.nStatus == MIRROR_ENABLED )
                    strcpy(pDevCtx->szMirrorIntfIn, mirrorCfg.szMirrorInterface);
                else
                    memset(pDevCtx->szMirrorIntfIn, 0x00, MIRROR_INTF_SIZE);
            }
            else /* MIRROR_DIR_OUT */
            {
                if( mirrorCfg.nStatus == MIRROR_ENABLED )
                    strcpy(pDevCtx->szMirrorIntfOut, mirrorCfg.szMirrorInterface);
                else
                    memset(pDevCtx->szMirrorIntfOut, 0x00, MIRROR_INTF_SIZE);
            }
        }
        break;

    case SIOCETHTOOL:
        nRet = bcmxtmrt_ethtool_ioctl(pDevCtx, (void *) Req->ifr_data);
        break;

    default:
        nRet = -EOPNOTSUPP;    
        break;
    }

    return( nRet );
} /* bcmxtmrt_ioctl */


/***************************************************************************
 * Function Name: bcmxtmrt_ethtool_ioctl
 * Description  : Driver ethtool IOCTL entry point.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_ethtool_ioctl(PBCMXTMRT_DEV_CONTEXT pDevCtx, void *useraddr)
{
    struct ethtool_drvinfo info;
    struct ethtool_cmd ecmd;
    unsigned long ethcmd;
    int nRet = 0;

    if( copy_from_user(&ethcmd, useraddr, sizeof(ethcmd)) == 0 )
    {
        switch (ethcmd)
        {
        case ETHTOOL_GDRVINFO:
            info.cmd = ETHTOOL_GDRVINFO;
            strncpy(info.driver, CARDNAME, sizeof(info.driver)-1);
            strncpy(info.version, VERSION, sizeof(info.version)-1);
            if (copy_to_user(useraddr, &info, sizeof(info)))
                nRet = -EFAULT;
            break;

        case ETHTOOL_GSET:
            ecmd.cmd = ETHTOOL_GSET;
            ecmd.speed = pDevCtx->MibInfo.ulIfSpeed / (1024 * 1024);
            if (copy_to_user(useraddr, &ecmd, sizeof(ecmd)))
                nRet = -EFAULT;
            break;

        default:
            nRet = -EOPNOTSUPP;    
            break;
        }
    }
    else
       nRet = -EFAULT;

    return( nRet );
}

/***************************************************************************
 * Function Name: bcmxtmrt_atm_ioctl
 * Description  : Driver ethtool IOCTL entry point.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_atm_ioctl(struct socket *sock, unsigned int cmd,
    unsigned long arg)
{
    struct atm_vcc *pAtmVcc = ATM_SD(sock);
    void __user *argp = (void __user *)arg;
    atm_backend_t b;
    PBCMXTMRT_DEV_CONTEXT pDevCtx;
    int nRet = -ENOIOCTLCMD;

    switch( cmd )
    {
    case ATM_SETBACKEND:
        if( get_user(b, (atm_backend_t __user *) argp) == 0 )
        {
            switch (b)
            {
            case ATM_BACKEND_PPP_BCM:
                if( (pDevCtx = FindDevCtx(pAtmVcc->vpi, pAtmVcc->vci))!=NULL &&
                    pDevCtx->Chan.private == NULL )
                {
                    pDevCtx->Chan.private = pDevCtx->pDev;
                    pDevCtx->Chan.ops = &g_PppoAtmOps;
                    pDevCtx->Chan.mtu = 1500; /* TBD. Calc value. */
                    pAtmVcc->user_back = pDevCtx;
                    if( ppp_register_channel(&pDevCtx->Chan) == 0 )
                        nRet = 0;
                    else
                        nRet = -EFAULT;
                }
                else
                    nRet = (pDevCtx) ? 0 : -EFAULT;
                break;

            case ATM_BACKEND_PPP_BCM_DISCONN:
                /* This is a patch for PPP reconnection.
                 * ppp daemon wants us to send out an LCP termination request
                 * to let the BRAS ppp server terminate the old ppp connection.
                 */
                if((pDevCtx = FindDevCtx(pAtmVcc->vpi, pAtmVcc->vci)) != NULL)
                {
                    struct sk_buff *skb;
                    int size = 6;
                    int eff  = (size+3) & ~3; /* align to word boundary */

                    while (!(skb = alloc_skb(eff, GFP_KERNEL)))
                        schedule();

                    skb->dev = NULL; /* for paths shared with net_device interfaces */
                    skb_put(skb, size);

                    skb->data[0] = 0xc0;  /* PPP_LCP == 0xc021 */
                    skb->data[1] = 0x21;
                    skb->data[2] = 0x05;  /* TERMREQ == 5 */
                    skb->data[3] = 0x02;  /* id == 2 */
                    skb->data[4] = 0x00;  /* HEADERLEN == 4 */
                    skb->data[5] = 0x04;

                    if (eff > size)
                        memset(skb->data+size,0,eff-size);

                    nRet = bcmxtmrt_xmit( SKBUFF_2_PNBUFF(skb), pDevCtx->pDev );
                }
                else
                    nRet = -EFAULT;
                break;

            case ATM_BACKEND_PPP_BCM_CLOSE_DEV:
                if( (pDevCtx = FindDevCtx(pAtmVcc->vpi, pAtmVcc->vci)) != NULL)
                {
                    bcmxtmrt_pppoatm_send(&pDevCtx->Chan, NULL);
                    ppp_unregister_channel(&pDevCtx->Chan);
                    pDevCtx->Chan.private = NULL;
                }
                nRet = 0;
                break;

            default:
                break;
            }
        }
        else
            nRet = -EFAULT;
        break;

    case PPPIOCGCHAN:
        if( (pDevCtx = FindDevCtx(pAtmVcc->vpi, pAtmVcc->vci)) != NULL )
        {
            nRet = put_user(ppp_channel_index(&pDevCtx->Chan),
                (int __user *) argp) ? -EFAULT : 0;
        }
        else
            nRet = -EFAULT;
        break;

    case PPPIOCGUNIT:
        if( (pDevCtx = FindDevCtx(pAtmVcc->vpi, pAtmVcc->vci)) != NULL )
        {
            nRet = put_user(ppp_unit_number(&pDevCtx->Chan),
                (int __user *) argp) ? -EFAULT : 0;
        }
        else
            nRet = -EFAULT;
        break;
    default:
        break;
    }

    return( nRet );
} /* bcmxtmrt_atm_ioctl */


/***************************************************************************
 * Function Name: FindDevCtx
 * Description  : Finds a device context structure for a VCC.
 * Returns      : Pointer to a device context structure or NULL.
 ***************************************************************************/
static PBCMXTMRT_DEV_CONTEXT FindDevCtx( short vpi, int vci )
{
    PBCMXTMRT_DEV_CONTEXT pDevCtx = NULL;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 i;

    for( i = 0; i < MAX_DEV_CTXS; i++ )
    {
        if( (pDevCtx = pGi->pDevCtxs[i]) != NULL )
        {
            if( pDevCtx->Addr.u.Vcc.usVpi == vpi &&
                pDevCtx->Addr.u.Vcc.usVci == vci )
            {
                break;
            }

            pDevCtx = NULL;
        }
    }

    return( pDevCtx );
} /* FindDevCtx */


/***************************************************************************
 * Function Name: bcmxtmrt_atmdev_open
 * Description  : ATM device open
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_atmdev_open(struct atm_vcc *pVcc)
{
    set_bit(ATM_VF_READY,&pVcc->flags);
    return( 0 );
} /* bcmxtmrt_atmdev_open */


/***************************************************************************
 * Function Name: bcmxtmrt_atmdev_close
 * Description  : ATM device open
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static void bcmxtmrt_atmdev_close(struct atm_vcc *pVcc)
{
    clear_bit(ATM_VF_READY,&pVcc->flags);
    clear_bit(ATM_VF_ADDR,&pVcc->flags);
} /* bcmxtmrt_atmdev_close */


/***************************************************************************
 * Function Name: bcmxtmrt_atmdev_send
 * Description  : send data
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_atmdev_send(struct atm_vcc *pVcc, struct sk_buff *skb)
{
    PBCMXTMRT_DEV_CONTEXT pDevCtx = FindDevCtx( pVcc->vpi, pVcc->vci );
    int nRet;

    if( pDevCtx )
        nRet = bcmxtmrt_xmit( SKBUFF_2_PNBUFF(skb), pDevCtx->pDev );
    else
        nRet = -EIO;

    return( nRet );
} /* bcmxtmrt_atmdev_send */



/***************************************************************************
 * Function Name: bcmxtmrt_pppoatm_send
 * Description  : Called by the PPP driver to send data.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_pppoatm_send(struct ppp_channel *pChan, struct sk_buff *skb)
{
    bcmxtmrt_xmit( SKBUFF_2_PNBUFF(skb), (struct net_device *) pChan->private );
    return(1);
} /* bcmxtmrt_pppoatm_send */


/***************************************************************************
 * Function Name: QueuePacket
 * Description  : Determines whether to queue a packet for transmission based
 *                on the number of total external (ie Ethernet) buffers and
 *                buffers already queued.
 * Returns      : 1 to queue packet, 0 to drop packet
 ***************************************************************************/
inline int QueuePacket( PBCMXTMRT_GLOBAL_INFO pGi, PTXQINFO pTqi )
{
    int nRet = 0; /* default to drop packet */

    if( pGi->ulNumTxQs == 1 )
    {
        /* One total transmit queue.  Allow up to 90% of external buffers to
         * be queued on this transmit queue.
         */
        if( pTqi->ulNumTxBufsQdOne < pGi->ulNumExtBufs90Pct )
        {
            nRet = 1; /* queue packet */
            pGi->ulDbgQ1++;
        }
        else
            pGi->ulDbgD1++;
    }
    else
    {
        if(pGi->ulNumExtBufs - pGi->ulNumTxBufsQdAll > pGi->ulNumExtBufsRsrvd)
        {
            /* The available number of external buffers is greater than the
             * reserved value.  Allow up to 50% of external buffers to be
             * queued on this transmit queue.
             */
            if( pTqi->ulNumTxBufsQdOne < pGi->ulNumExtBufs50Pct )
            {
                nRet = 1; /* queue packet */
                pGi->ulDbgQ2++;
            }
            else
                pGi->ulDbgD2++;
        }
        else
        {
            /* Divide the reserved number of external buffers evenly among all
             * of the transmit queues.
             */
            if(pTqi->ulNumTxBufsQdOne < pGi->ulNumExtBufsRsrvd / pGi->ulNumTxQs)
            {
                nRet = 1; /* queue packet */
                pGi->ulDbgQ3++;
            }
            else
                pGi->ulDbgD3++;
        }
    }

    return( nRet );
} /* QueuePacket */


/***************************************************************************
 * Function Name: bcmxtmrt_xmit
 * Description  : Check for transmitted packets to free and, if skb is
 *                non-NULL, transmit a packet. Transmit may be invoked for
 *                a packet originating from the network stack or from a
 *                packet received from another interface.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_xmit( pNBuff_t pNBuff, struct net_device *dev )
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
 #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
   PBCMXTMRT_DEV_CONTEXT pDevCtx = netdev_priv(dev);
 #else
   PBCMXTMRT_DEV_CONTEXT pDevCtx = dev->priv;
 #endif

    spin_lock_bh(&pGi->xtmlock_tx);

    if( pDevCtx->ulLinkState == LINK_UP )
    {
        PTXQINFO pTqi;
        UINT8 * pData;
        UINT32 i;
        unsigned len, uMark, uPriority;
        struct sk_buff * pNBuffSkb; /* If pNBuff is sk_buff: protocol access */

        /* Free packets that have been transmitted. */
        for(i=0, pTqi=pDevCtx->TxQInfos; i<pDevCtx->ulTxQInfosSize; i++,pTqi++)
        {
            DmaDesc dmaDesc;
            while( pTqi->ulFreeBds < pTqi->ulQueueSize )
            {
                dmaDesc.word0 = pTqi->pBds[pTqi->ulHead].word0;
                if ( dmaDesc.status & DMA_OWN )
                    break;

                if( pTqi->ppNBuffs[pTqi->ulHead] != PNBUFF_NULL )
                {
                    nbuff_free(pTqi->ppNBuffs[pTqi->ulHead]);
                    pTqi->ppNBuffs[pTqi->ulHead] = PNBUFF_NULL;
                    pTqi->pBds[pTqi->ulHead].address = 0; /* optimize */
                }

                if( !(dmaDesc.status & DMA_WRAP) )
                    pTqi->ulHead++;
                else
                    pTqi->ulHead = 0;

                pTqi->ulFreeBds++;
                pTqi->ulNumTxBufsQdOne--;
                pGi->ulNumTxBufsQdAll--;
            }
        }

        pTqi = NULL;

        if( nbuff_get_params(pNBuff, &pData, &len, &uMark, &uPriority)
            == (void*)NULL )
        {
            goto unlock_done_xmit;
        }

        pNBuffSkb = (IS_SKBUFF_PTR(pNBuff)) ? PNBUFF_2_SKBUFF(pNBuff) : NULL;

        if( pDevCtx->ulTxQInfosSize )
        {
            /* Find a transmit queue to send on. */
            UINT32 ulPort = 0;
            UINT32 ulPtmPriority = 0;

#ifdef CONFIG_NETFILTER
            /* bit 2-0 of the 32-bit nfmark is the subpriority (0 to 7) set by ebtables.
             * bit 3   of the 32-bit nfmark is the DSL latency, 0=PATH0, 1=PATH1
             * bit 4   of the 32-bit nfmark is the PTM priority, 0=LOW,  1=HIGH
             */
            uPriority = uMark & 0x07;

            ulPort = (uMark >> 3) & 0x1;  //DSL latency

            if (pDevCtx->Addr.ulTrafficType == TRAFFIC_TYPE_PTM)
               ulPtmPriority = (uMark >> 4) & 0x1;

            pTqi=pDevCtx->pTxPriorities[ulPtmPriority][ulPort][uPriority];
        
            /* If a transmit queue was not found, use the existing highest priority queue
             * that had been configured with the default Ptm priority and DSL latency (port).
             */
            if (pTqi == NULL && uPriority > 1)
            {
               UINT32 ulPtmPriorityDflt;
               UINT32 ulPortDflt;

               if (pDevCtx->pTxPriorities[0][0][0] == &pDevCtx->TxQInfos[0])
               {
                  ulPtmPriorityDflt = 0;
                  ulPortDflt        = 0;
               }
               else if (pDevCtx->pTxPriorities[0][1][0] == &pDevCtx->TxQInfos[0])
               {
                  ulPtmPriorityDflt = 0;
                  ulPortDflt        = 1;
               }
               else if (pDevCtx->pTxPriorities[1][0][0] == &pDevCtx->TxQInfos[0])
               {
                  ulPtmPriorityDflt = 1;
                  ulPortDflt        = 0;
               }
               else
               {
                  ulPtmPriorityDflt = 1;
                  ulPortDflt        = 1;
               }
               for (i = uPriority - 1; pTqi == NULL && i >= 1; i--)
                  pTqi = pDevCtx->pTxPriorities[ulPtmPriorityDflt][ulPortDflt][i];
            }
#endif

            /* If a transmit queue was not found, use the first one. */
            if( pTqi == NULL )
               pTqi = &pDevCtx->TxQInfos[0];

            if( pTqi->ulFreeBds && QueuePacket(pGi, pTqi) )
            {
                volatile DmaDesc *pBd;
                DmaDesc dmaDesc;
                UINT32 ulRfc2684_type; /* Not needed as CMF "F in software" */
                UINT32 ulHdrType = pDevCtx->ulHdrType;

                UINT32 isAtmCell = ( pNBuffSkb &&
                    pDevCtx->Addr.ulTrafficType == TRAFFIC_TYPE_ATM &&
                    (pNBuffSkb->protocol & ~FSTAT_CT_MASK) == SKB_PROTO_ATM_CELL
                    );

                /* Decrement total BD count.  Increment global count. */
                pTqi->ulFreeBds--;
                pTqi->ulNumTxBufsQdOne++;
                pGi->ulNumTxBufsQdAll++;

                if( pDevCtx->szMirrorIntfOut[0] != '\0' &&
                    pNBuffSkb && !isAtmCell &&
                    (ulHdrType ==  HT_PTM ||
                     ulHdrType ==  HT_LLC_SNAP_ETHERNET ||
                     ulHdrType ==  HT_VC_MUX_ETHERNET) )
                {
#ifdef CONFIG_BLOG     
                    blog_skip( pNBuffSkb );/* Mirroring not supported for FKB */
#endif
                    MirrorPacket( pNBuffSkb, pDevCtx->szMirrorIntfOut );
                }

                if( (pDevCtx->ulFlags & CNI_HW_ADD_HEADER) == 0 &&
                     HT_LEN(ulHdrType) != 0 && !isAtmCell )
                {
                    ulRfc2684_type = HT_TYPE(ulHdrType);
                }
                else
                    ulRfc2684_type = RFC2684_NONE;

                spin_unlock_bh(&pGi->xtmlock_tx);
#ifdef CONFIG_BLOG
                blog_emit( pNBuff, dev, pDevCtx->ulEncapType,
                           (unsigned int)pTqi->ulDmaIndex, ulRfc2684_type );
#endif
                spin_lock_bh(&pGi->xtmlock_tx);

                if ( ulRfc2684_type )
                {
                    AddRfc2684Hdr(&pNBuff, &pNBuffSkb, &pData, &len, ulHdrType);
                }

                if (pGi->bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE)
                  bcmxtmrt_ptmbond_add_hdr (pDevCtx, &pNBuff, &pNBuffSkb, &pData, &len) ;

                //DUMP_PKT(pData, 32) ;

                if( pGi->ulChipRev == CHIP_REV_BCM6368B0 &&
                    ((UINT32) pData & 0x07) != 0 )
                {
                    bcmxtmrt_align(&pNBuff, &pNBuffSkb, &pData, &len, 0x7);
                }

                if( len < ETH_ZLEN && !isAtmCell &&
                    (ulHdrType == HT_PTM ||
                     ulHdrType == HT_LLC_SNAP_ETHERNET ||
                     ulHdrType == HT_VC_MUX_ETHERNET) )
                {
                    len = ETH_ZLEN;
                }

                nbuff_flush(pNBuff, pData, len);

                /* Track sent SKB, so we can release them later */
                pTqi->ppNBuffs[pTqi->ulTail] = pNBuff;

                pBd = &pTqi->pBds[pTqi->ulTail];
                pBd->address = (UINT32) VIRT_TO_PHY(pData);
                dmaDesc.length = len;
                dmaDesc.status = (pBd->status & DMA_WRAP) | DMA_SOP | DMA_EOP |
                    pDevCtx->ucTxVcid ;

                if ( pDevCtx->Addr.ulTrafficType == TRAFFIC_TYPE_ATM )
                {
                    if( isAtmCell )
                    {
                        dmaDesc.status |= pNBuffSkb->protocol & FSTAT_CT_MASK;
                        if( (pDevCtx->ulFlags & CNI_USE_ALT_FSTAT) != 0 )
                        {
                            dmaDesc.status |= FSTAT_MODE_COMMON;
                            dmaDesc.status &= ~(FSTAT_COMMON_INS_HDR_EN |
                                FSTAT_COMMON_HDR_INDEX_MASK);
                        }
                    }
                    else
                    {
                        dmaDesc.status |= FSTAT_CT_AAL5;
                        if( (pDevCtx->ulFlags & CNI_USE_ALT_FSTAT) != 0 )
                        {
                            dmaDesc.status |= FSTAT_MODE_COMMON;
                            if(HT_LEN(ulHdrType) != 0 &&
                               (pDevCtx->ulFlags & CNI_HW_ADD_HEADER) != 0)
                            {
                                dmaDesc.status |= FSTAT_COMMON_INS_HDR_EN |
                                    ((HT_TYPE(ulHdrType) - 1) <<
                                    FSTAT_COMMON_HDR_INDEX_SHIFT);
                            }
                            else
                            {
                                dmaDesc.status &= ~(FSTAT_COMMON_INS_HDR_EN |
                                    FSTAT_COMMON_HDR_INDEX_MASK);
                            }
                        }
                    }
                }
                else
                    dmaDesc.status |= FSTAT_CT_PTM | FSTAT_PTM_ENET_FCS |
                            FSTAT_PTM_CRC;

                dmaDesc.status |= DMA_OWN;

                pBd->word0 = dmaDesc.word0; /* uncached write <status,length> */

                /* advance BD pointer to next in the chain. */
                if( !(dmaDesc.status & DMA_WRAP) )
                    pTqi->ulTail++;
                else
                    pTqi->ulTail = 0;

                /* Enable DMA for this channel */
                pTqi->pDma->cfg = DMA_ENABLE;

                /* Transmitted bytes are counted in hardware. */
                pDevCtx->DevStats.tx_packets++;
                pDevCtx->pDev->trans_start = jiffies;
            }
            else
            {
                /* Transmit queue is full.  Free the socket buffer.  Don't call
                 * netif_stop_queue because this device may use more than one
                 * queue.
                 */
                nbuff_flushfree(pNBuff);                
                pDevCtx->DevStats.tx_errors++;
            }
        }
        else
        {
            nbuff_flushfree(pNBuff);            
            pDevCtx->DevStats.tx_dropped++;
        }
    }
    else
    {
        if( pNBuff )
        {
            nbuff_flushfree(pNBuff);
            pDevCtx->DevStats.tx_dropped++;
        }
    }

unlock_done_xmit:
    spin_unlock_bh(&pGi->xtmlock_tx);

    return 0;
} /* bcmxtmrt_xmit */

/***************************************************************************
 * Function Name: bcmxtmrt_align
 * Description  : Enforce an alignment of the packet data (temp WORKAROUND)
 * Returns      : None.
 ***************************************************************************/
static void bcmxtmrt_align(pNBuff_t *ppNBuff, struct sk_buff **ppNBuffSkb,
    UINT8 **ppData, int *pLen, UINT32 alignmask )
{
    UINT32 ulShiftBytes;
    UINT16 *pBuf;

    pBuf = (UINT16*)*ppData;
    ulShiftBytes = (((UINT32)pBuf + alignmask) & ~alignmask) - (UINT32)pBuf;

    if ( *ppNBuffSkb )
    {
        struct sk_buff *skb = *ppNBuffSkb;
        struct sk_buff *skb2 = skb_copy_expand(skb, 0, ulShiftBytes,GFP_ATOMIC);
        if( skb2 )
        {
            dev_kfree_skb_any(skb);
            *ppData = skb2->data;
            *pLen   = skb2->len;
            *ppNBuffSkb = skb2;
            *ppNBuff = SKBUFF_2_PNBUFF(skb2);
        }
        // else ?
    }
    else // if( IS_FKBUFF_PTR(*ppNBuff) )
    {
        UINT16 *pDst16, *pSrc16;
        /* Assumes ulShiftBytes is a multiple of 2 */
        /* no need to check for tailroom in FKB */
        pDst16 = (UINT16*)((UINT32)pBuf + *pLen - sizeof(UINT16) +ulShiftBytes);
        pSrc16 = (UINT16*)((UINT32)pBuf + *pLen - sizeof(UINT16));
        while (pSrc16 >= pBuf) 
            *pDst16-- = *pSrc16--;
        *ppData += ulShiftBytes; /* *ppNBuff->data not updated */

        cache_flush_len( pBuf, *pLen + ulShiftBytes);
    }
}

/***************************************************************************
 * Function Name: AddRfc2684Hdr
 * Description  : Adds the RFC2684 header to an ATM packet before transmitting
 *                it.
 * Returns      : None.
 ***************************************************************************/
static void AddRfc2684Hdr(pNBuff_t *ppNBuff, struct sk_buff **ppNBuffSkb,
                          UINT8 **ppData, int * pLen, UINT32 ulHdrType)
{
    UINT8 ucHdrs[][16] =
        {{},
         {0xAA, 0xAA, 0x03, 0x00, 0x80, 0xC2, 0x00, 0x07, 0x00, 0x00},
         {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00},
         {0xFE, 0xFE, 0x03, 0xCF},
         {0x00, 0x00}};
    int minheadroom = HT_LEN(ulHdrType);

    if ( *ppNBuffSkb )
    {
        struct sk_buff *skb = *ppNBuffSkb;
        int headroom = skb_headroom(skb);

        if (headroom < minheadroom)
        {
            struct sk_buff *skb2 = skb_realloc_headroom(skb, minheadroom);

            dev_kfree_skb_any(skb);
            skb = (skb2 == NULL) ? NULL : skb2;
        }

        if( skb )
        {
            *ppData = skb_push(skb, minheadroom);
            *pLen = skb->len;
            u16cpy(*ppData, ucHdrs[HT_TYPE(ulHdrType)], minheadroom);
        }
        // else ?
        *ppNBuffSkb = skb;
        *ppNBuff = SKBUFF_2_PNBUFF(skb);
    }
    else // if( IS_FKBUFF_PTR(*ppNBuff) )
    {
        struct fkbuff *fkb = PNBUFF_2_FKBUFF(*ppNBuff);
        int headroom = fkb_headroom(fkb);
        if (headroom >= minheadroom)
        {
            *ppData = fkb_push(fkb, minheadroom);
            *pLen += minheadroom;
            u16cpy(*ppData, ucHdrs[HT_TYPE(ulHdrType)], minheadroom);
        }
        else
            printk(CARDNAME ": FKB not enough headroom.\n");
    }

} /* AddRfc2684Hdr */


/***************************************************************************
 * Function Name: AssignRxBuffer
 * Description  : Put a data buffer back on to the receive BD ring. 
 * Returns      : None.
 ***************************************************************************/
static void AssignRxBuffer(PRXBDINFO pRxBdInfo, UINT8 *pucData)
{
    volatile DmaDesc *pRxBd;
    volatile DmaChannelCfg *pRxDma;
    DmaDesc dmaDesc;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 flags;

    spin_lock_bh(&pGi->xtmlock_rx);

    pRxBd = pRxBdInfo->pBdTail;
    pRxDma = pRxBdInfo->pDma;

    dmaDesc.status = (pRxBd->status & DMA_WRAP) | DMA_OWN;
    dmaDesc.length = RXBUF_SIZE;

    if( pRxBd->address == 0 )
    {
        pRxBd->address = VIRT_TO_PHY(pucData);
        pRxBd->word0   = dmaDesc.word0;

        if ( dmaDesc.status & DMA_WRAP )
            pRxBdInfo->pBdTail = pRxBdInfo->pBdBase;
        else
            pRxBdInfo->pBdTail++;
    }
    else
    {
        /* This should not happen. */
        printk(CARDNAME ": No place to put free buffer.\n");
    }

    spin_unlock_bh(&pGi->xtmlock_rx);

    /* Restart DMA in case the DMA ran out of descriptors, and
     * is currently shut down.
     */
    spin_lock_irqsave(&pGi->xtmlock_rx_regs, flags);
    if( (pRxDma->intStat & DMA_NO_DESC) != 0 )
    {
        pRxDma->intStat = DMA_NO_DESC;
        pRxDma->cfg = DMA_ENABLE;
    }
    spin_unlock_irqrestore(&pGi->xtmlock_rx_regs, flags);
}

/***************************************************************************
 * Function Name: FlushAssignRxBuffer
 * Description  : Flush then assign RxBdInfo to the receive BD ring. 
 * Returns      : None.
 ***************************************************************************/
void FlushAssignRxBuffer(PRXBDINFO pRxBdInfo, UINT8 *pucData,
    UINT8 *pucEnd)
{
    cache_flush_region(pucData, pucEnd);
    AssignRxBuffer(pRxBdInfo, pucData);
}

/***************************************************************************
 * Function Name: bcmxtmrt_recycle_skb_or_data
 * Description  : Put socket buffer header back onto the free list or a data
 *                buffer back on to the BD ring. 
 * Returns      : None.
 ***************************************************************************/
static void bcmxtmrt_recycle_skb_or_data(struct sk_buff *skb, unsigned context,
    UINT32 nFlag )
{
    if( nFlag & SKB_RECYCLE )
    {
        PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
        spin_lock_bh(&pGi->xtmlock_rx);
        skb->next_free = pGi->pFreeRxSkbList;
        pGi->pFreeRxSkbList = skb;
        spin_unlock_bh(&pGi->xtmlock_rx);
    }
    else
    {
        UINT8 *pucData, *pucEnd;
        PRXBDINFO pRxBdInfo = (PRXBDINFO) context;

        pucData = skb->head + RXBUF_HEAD_RESERVE;
#ifdef XTM_CACHE_SMARTFLUSH
        pucEnd = (UINT8*)(skb_shinfo(skb)) + sizeof(struct skb_shared_info);
#else
        pucEnd = pucData + RXBUF_SIZE;
#endif
        FlushAssignRxBuffer(pRxBdInfo, pucData, pucEnd);
    }
} /* bcmxtmrt_recycle_skb_or_data */

/***************************************************************************
 * Function Name: _bcmxtmrt_recycle_fkb
 * Description  : Put fkb buffer back on to the BD ring.
 * Returns      : None.
 ***************************************************************************/
static inline void _bcmxtmrt_recycle_fkb(struct fkbuff *pFkb,
                                              unsigned context)
{
    PRXBDINFO pRxBdInfo = (PRXBDINFO) context;
    UINT8 *pucData = (UINT8*) PFKBUFF_TO_PDATA(pFkb, RXBUF_HEAD_RESERVE);
    AssignRxBuffer(pRxBdInfo, pucData); /* No cache flush */
} /* _bcmxtmrt_recycle_fkb */

/***************************************************************************
 * Function Name: bcmxtmrt_recycle
 * Description  : Recycle a fkb or skb or skb->data
 * Returns      : None.
 ***************************************************************************/
static void bcmxtmrt_recycle(pNBuff_t pNBuff, unsigned context, UINT32 flags)
{
    if (IS_FKBUFF_PTR(pNBuff))
        _bcmxtmrt_recycle_fkb( PNBUFF_2_FKBUFF(pNBuff), context );
    else // if (IS_SKBUFF_PTR(pNBuff))
        bcmxtmrt_recycle_skb_or_data( PNBUFF_2_SKBUFF(pNBuff), context, flags );
}

/***************************************************************************
 * Function Name: bcmxtmrt_rxisr
 * Description  : Hardware interrupt that is called when a packet is received
 *                on one of the receive queues.
 * Returns      : IRQ_HANDLED
 ***************************************************************************/
static irqreturn_t bcmxtmrt_rxisr(int nIrq, void *pRxDma)
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    PBCMXTMRT_DEV_CONTEXT pDevCtx;
    UINT32 i;
    UINT32 ulScheduled = 0;

    spin_lock(&pGi->xtmlock_rx_regs);

    for( i = 0; i < MAX_DEV_CTXS; i++ )
    {
        if( (pDevCtx = pGi->pDevCtxs[i]) != NULL &&
            pDevCtx->ulOpenState == XTMRT_DEV_OPENED )
        {
            /* Device is open.  Schedule the poll function. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
            napi_schedule(&pDevCtx->napi);
#else
            netif_rx_schedule(pDevCtx->pDev);
#endif
            ((volatile DmaChannelCfg *) pRxDma)->intStat = DMA_BUFF_DONE;
            pGi->ulIntEnableMask |=
                1 << (((UINT32) pRxDma - (UINT32)pGi->pRxDmaBase) /
                sizeof(DmaChannelCfg));
            ulScheduled = 1;
        }
    }

    if( ulScheduled == 0 )
    {
        /* Device is not open.  Reenable interrupt. */
        ((volatile DmaChannelCfg *) pRxDma)->intStat = DMA_BUFF_DONE;
        BcmHalInterruptEnable(SAR_RX_INT_ID_BASE + (((UINT32)pRxDma -
            (UINT32)pGi->pRxDmaBase) / sizeof(DmaChannelCfg)));
    }

    spin_unlock(&pGi->xtmlock_rx_regs);

    return( IRQ_HANDLED );
} /* bcmxtmrt_rxisr */


/***************************************************************************
 * Function Name: bcmxtmrt_poll
 * Description  : Hardware interrupt that is called when a packet is received
 *                on one of the receive queues.
 * Returns      : IRQ_HANDLED
 ***************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)

static int bcmxtmrt_poll_napi(struct napi_struct* napi, int budget)
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 ulMask;
    UINT32 i;
    UINT32 work_done;
    UINT32 ret_done;
    UINT32 flags;
    UINT32 more_to_do;

    spin_lock_irqsave(&pGi->xtmlock_rx_regs, flags);
    ulMask = pGi->ulIntEnableMask;
    pGi->ulIntEnableMask = 0;
    spin_unlock_irqrestore(&pGi->xtmlock_rx_regs,flags);

    work_done = bcmxtmrt_rxtask(budget, &more_to_do);
    ret_done = work_done & XTM_POLL_DONE;
    work_done &= ~XTM_POLL_DONE;

    /* JU: You may not call napi_complete if work_done == budget...
       this causes the framework to crash (specifically, you get
       napi->poll_list.next=0x00100100).  So, in this case
       you have to just return work_done.  */
    if (work_done == budget || ret_done != XTM_POLL_DONE)
    {
        /* We have either exhausted our budget or there are
           more packets on the DMA (or both) */
        spin_lock_irqsave(&pGi->xtmlock_rx_regs, flags);
        pGi->ulIntEnableMask |= ulMask;
        spin_unlock_irqrestore(&pGi->xtmlock_rx_regs,flags);        
        return work_done;
    }

    /* We are done */
    napi_complete(napi);

    /* Renable interrupts. */
    for( i = 0; ulMask && i < MAX_RECEIVE_QUEUES; i++, ulMask >>= 1 )
        if( (ulMask & 0x01) == 0x01 )
            BcmHalInterruptEnable(SAR_RX_INT_ID_BASE + i);

    return work_done;
}


#else
static int bcmxtmrt_poll(struct net_device * dev, int * budget)
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 ulMask;
    UINT32 i;
    UINT32 work_to_do = min(dev->quota, *budget);
    UINT32 work_done;
    UINT32 ret_done;
    UINT32 more_to_do = 0;
    UINT32 flags;

    spin_lock_irqsave(&pGi->xtmlock_rx_regs, flags);
    ulMask = pGi->ulIntEnableMask;
    pGi->ulIntEnableMask = 0;
    spin_unlock_irqrestore(&pGi->xtmlock_rx_regs,flags);

    work_done = bcmxtmrt_rxtask(work_to_do, &more_to_do);
    ret_done = work_done & XTM_POLL_DONE;
    work_done &= ~XTM_POLL_DONE;

    *budget -= work_done;
    dev->quota -= work_done;

    /* JU I think it should be (work_done >= work_to_do).... */
    if (work_done < work_to_do && ret_done != XTM_POLL_DONE)
    {
        /* Did as much as could, but we are not done yet */
        spin_lock_irqsave(&pGi->xtmlock_rx_regs, flags);
        pGi->ulIntEnableMask |= ulMask;
        spin_unlock_irqrestore(&pGi->xtmlock_rx_regs,flags);
        return 1;
    }

    /* We are done */
    netif_rx_complete(dev);

    /* Renable interrupts. */
    for( i = 0; ulMask && i < MAX_RECEIVE_QUEUES; i++, ulMask >>= 1 )
        if( (ulMask & 0x01) == 0x01 )
            BcmHalInterruptEnable(SAR_RX_INT_ID_BASE + i);

    return 0;
} /* bcmxtmrt_poll */
#endif

/***************************************************************************
 * Function Name: bcmxtmrt_rxtask
 * Description  : Linux Tasklet that processes received packets.
 * Returns      : None.
 ***************************************************************************/
static UINT32 bcmxtmrt_rxtask( UINT32 ulBudget, UINT32 *pulMoreToDo )
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 ulMoreToReceive;
    UINT32 i;
    PRXBDINFO pRxBdInfo;
    volatile DmaDesc *pRxBd;
    DmaDesc dmaDesc;
    UINT8 *pBuf, *pucData;
    PBCMXTMRT_DEV_CONTEXT pDevCtx;

    UINT32 ulRxPktGood = 0;
    UINT32 ulRxPktProcessed = 0;
    UINT32 ulRxPktMax = ulBudget + (ulBudget / 2);

    /* Receive packets from every receive queue in a round robin order until
     * there are no more packets to receive.
     */
    do
    {
        ulMoreToReceive = 0;

        for( i = 0, pRxBdInfo = pGi->RxBdInfos; i < MAX_RECEIVE_QUEUES;
             i++, pRxBdInfo++ )
        {
            UINT32 ulCell;
            if( ulBudget == 0 )
            {
                *pulMoreToDo = 1;
                break;
            }

            spin_lock_bh(&pGi->xtmlock_rx);

            pRxBd = pRxBdInfo->pBdHead;
            if (pRxBd == NULL)
            {
                spin_unlock_bh(&pGi->xtmlock_rx);
                continue;   /* next RxBdInfos */
            }

            dmaDesc.word0 = pRxBd->word0;   /* uncached read <status,length> */
            pBuf = (UINT8 *) pRxBd->address;

            if( (dmaDesc.status & DMA_OWN) || (pBuf == NULL) )
            {
                ulRxPktGood |= XTM_POLL_DONE;
                spin_unlock_bh(&pGi->xtmlock_rx);

                continue;   /* next RxBdInfos */
            }
            pucData = pBuf = (UINT8 *) KSEG0ADDR((UINT32)pBuf);

            pRxBd->address = 0;
            pRxBd->status = dmaDesc.status & DMA_WRAP;

            if( (dmaDesc.status & DMA_WRAP) != 0 )
                pRxBdInfo->pBdHead = pRxBdInfo->pBdBase;
            else
                pRxBdInfo->pBdHead++;

            ulRxPktProcessed++;

            pDevCtx = pGi->pDevCtxsByMatchId[dmaDesc.status & FSTAT_MATCH_ID_MASK];
            ulCell  = (dmaDesc.status & FSTAT_PACKET_CELL_MASK) == FSTAT_CELL;

            /* error status, or packet with no pDev */
            if(((dmaDesc.status & FSTAT_ERROR) != 0) ||
                ((!ulCell) && (pDevCtx == NULL)))   /* packet */
            {
                spin_unlock_bh(&pGi->xtmlock_rx);

                AssignRxBuffer(pRxBdInfo, pBuf);
                if( pDevCtx )
                    pDevCtx->DevStats.rx_errors++;
            }
            else if ( !ulCell ) /* process packet, pDev != NULL */
            {
                FkBuff_t * pFkb;
                UINT16 usLength = dmaDesc.length;
                int delLen = 0;

                ulRxPktGood++;
                ulBudget--;

                if( (pDevCtx->ulFlags & LSC_RAW_ENET_MODE) != 0 )
                    usLength -= 4; /* ETH CRC Len */

                spin_unlock_bh(&pGi->xtmlock_rx);

                if( (pDevCtx->ulFlags & CNI_HW_REMOVE_HEADER) == 0 )
                {
                   delLen = HT_LEN(pDevCtx->ulHdrType);

                   /* For PTM flow, this will not take effect and hence so for
                    * bonding flow as wel. So we do not need checks here to not
                    * make it happen here.
                    */
                    if( delLen > 0)
                    {
                        pucData += delLen;
                        usLength -= delLen;
                    }
                }

                if( usLength < ETH_ZLEN )
                    usLength = ETH_ZLEN;

                pFkb = fkb_qinit (pBuf, RXBUF_HEAD_RESERVE, pucData, usLength,
                                  (uint32_t)pRxBdInfo) ;

                if (pGi->bondConfig.sConfig.ptmBond != BC_PTM_BONDING_ENABLE) {
                    bcmxtmrt_process_rx_pkt (pDevCtx, pFkb, dmaDesc.status, delLen) ;
                }
                else
                {
                    spin_lock_bh (&pGi->xtmlock_rx_bond) ;
                    bcmxtmrt_ptmbond_receive_rx_fragment (pDevCtx, pFkb, dmaDesc.status) ;
                    spin_unlock_bh (&pGi->xtmlock_rx_bond);
                }
            }
            else                /* process cell */
            {
                spin_unlock_bh(&pGi->xtmlock_rx);
                ProcessRxCell( pGi, pRxBdInfo, pucData );
            }

            if( ulRxPktProcessed >= ulRxPktMax )
                break;
            else
                ulMoreToReceive = 1; /* more packets to receive on Rx queue? */

        } /* For loop */

    } while( ulMoreToReceive );

    return( ulRxPktGood );

} /* bcmxtmrt_rxtask */


/***************************************************************************
 * Function Name: bcmxtmrt_process_rx_pkt
 * Description  : Processes a received packet.
 *                Responsible to send the packet up to the blog and network stack.
 * Returns      : Status as the packet thro BLOG/NORMAL path.
 ***************************************************************************/

UINT32 bcmxtmrt_process_rx_pkt ( PBCMXTMRT_DEV_CONTEXT pDevCtx,
                                 FkBuff_t *pFkb, UINT16 bufStatus, int delLen )
{
   PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
   struct sk_buff *skb ;
   UINT8  *pucData  = pFkb->data ;
   UINT32 ulHdrType = pDevCtx->ulHdrType ;
   UINT32 ulRfc2684_type = RFC2684_NONE; /* blog.h: Rfc2684_t */
   UINT8  *pBuf = PFKBUFF_TO_PDATA(pFkb, RXBUF_HEAD_RESERVE) ;
   UINT32 retStatus ;

   pDevCtx->pDev->last_rx = jiffies ;

   pDevCtx->DevStats.rx_bytes += pFkb->len ;

   if( (ulHdrType ==  HT_PTM || ulHdrType ==  HT_LLC_SNAP_ETHERNET || 
        ulHdrType ==  HT_VC_MUX_ETHERNET) 
                     &&
         ((pucData[0] & 0x01) == 0x01) )
   {
      pDevCtx->DevStats.multicast++;
   }

   if( (pDevCtx->ulFlags & CNI_HW_REMOVE_HEADER) == 0 )
   {  /* cannot be an AtmCell, also do not use delLen (bonding), recompute */
      if( HT_LEN(ulHdrType) > 0)
         ulRfc2684_type = HT_TYPE(ulHdrType); /* blog.h: Rfc2684_t */
   }

   //dumpaddr(pFkb->data, 16) ;
#ifdef CONFIG_BLOG
   if( blog_finit( pFkb, pDevCtx->pDev, pDevCtx->ulEncapType,
               (bufStatus & FSTAT_MATCH_ID_MASK), ulRfc2684_type ) != PKT_DONE )
#else
   if (1)
#endif
   {
      spin_lock_bh(&pGi->xtmlock_rx) ;

      if( pGi->pFreeRxSkbList == NULL )
      {
         spin_unlock_bh(&pGi->xtmlock_rx);

         fkb_release(pFkb);  /* releases allocated blog */
         FlushAssignRxBuffer((PRXBDINFO) pFkb->recycle_context, pBuf, pBuf) ;
         pDevCtx->DevStats.rx_dropped++;
         return PACKET_NORMAL;
      }

      /* Get an skb to return to the network stack. */
      skb = pGi->pFreeRxSkbList;
      pGi->pFreeRxSkbList = pGi->pFreeRxSkbList->next_free;

      spin_unlock_bh(&pGi->xtmlock_rx);

      skb_headerinit( RXBUF_HEAD_RESERVE,
#ifdef XTM_CACHE_SMARTFLUSH
            pFkb->len + delLen + SAR_DMA_MAX_BURST_LENGTH,
#else
            RXBUF_SIZE,
#endif
            skb, pBuf,
            (RecycleFuncP)bcmxtmrt_recycle_skb_or_data,
            (unsigned) pFkb->recycle_context, (void*)fkb_blog(pFkb));

      if ( delLen )
         __skb_pull(skb, delLen);
      __skb_trim(skb, pFkb->len);
      skb->dev = pDevCtx->pDev ;

      if( pDevCtx->szMirrorIntfIn[0] != '\0' &&
            (ulHdrType ==  HT_PTM ||
             ulHdrType ==  HT_LLC_SNAP_ETHERNET ||
             ulHdrType ==  HT_VC_MUX_ETHERNET) )
      {
#ifdef CONFIG_BLOG
         blog_skip(skb);
#endif // CONFIG_BLOG

         MirrorPacket( skb, pDevCtx->szMirrorIntfIn );
      }

      switch( ulHdrType )
      {
         case HT_LLC_SNAP_ROUTE_IP:
         case HT_VC_MUX_IPOA:
            /* IPoA */
            skb->protocol = htons(ETH_P_IP);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
            skb_reset_mac_header(skb);
#else
            skb->mac.raw = skb->data;
#endif
            /* Give the received packet to the network stack. */
            netif_receive_skb(skb);
            break;

         case HT_LLC_ENCAPS_PPP:
         case HT_VC_MUX_PPPOA:
            /*PPPoA*/
            ppp_input(&pDevCtx->Chan, skb);
            break;

         default:
            /* bridge, MER, PPPoE */
            skb->protocol = eth_type_trans(skb,pDevCtx->pDev);

            /* Give the received packet to the network stack. */
            netif_receive_skb(skb);
            break;
      }

      retStatus = PACKET_NORMAL ;
   }
   else
      retStatus = PACKET_BLOG ;
   return (retStatus) ;
}

/***************************************************************************
 * Function Name: ProcessRxCell
 * Description  : Processes a received cell.
 * Returns      : None.
 ***************************************************************************/
static void ProcessRxCell( PBCMXTMRT_GLOBAL_INFO pGi, PRXBDINFO pRxBdInfo,
    UINT8 *pucData )
{
    const UINT16 usOamF4VciSeg = 3;
    const UINT16 usOamF4VciEnd = 4;
    UINT8 ucCts[] = {0, 0, 0, 0, CTYPE_OAM_F5_SEGMENT, CTYPE_OAM_F5_END_TO_END,
        0, 0, CTYPE_ASM_P0, CTYPE_ASM_P1, CTYPE_ASM_P2, CTYPE_ASM_P3,
        CTYPE_OAM_F4_SEGMENT, CTYPE_OAM_F4_END_TO_END};
    XTMRT_CELL Cell;
    UINT8 ucCHdr = *pucData;
    UINT8 *pucAtmHdr = pucData + sizeof(char);
    UINT8 ucLogPort;
    PBCMXTMRT_DEV_CONTEXT pDevCtx;

    /* Fill in the XTMRT_CELL structure */
    Cell.ConnAddr.ulTrafficType = TRAFFIC_TYPE_ATM;
    ucLogPort = PORT_PHYS_TO_LOG((ucCHdr & CHDR_PORT_MASK) >> CHDR_PORT_SHIFT);
    Cell.ConnAddr.u.Vcc.ulPortMask = PORT_TO_PORTID(ucLogPort);
    Cell.ConnAddr.u.Vcc.usVpi = (((UINT16) pucAtmHdr[0] << 8) +
        ((UINT16) pucAtmHdr[1])) >> 4;
    Cell.ConnAddr.u.Vcc.usVci = (UINT16)
        (((UINT32) (pucAtmHdr[1] & 0x0f) << 16) +
         ((UINT32) pucAtmHdr[2] << 8) +
         ((UINT32) pucAtmHdr[3])) >> 4;

    if( Cell.ConnAddr.u.Vcc.usVci == usOamF4VciSeg )
    {
        ucCHdr = CHDR_CT_OAM_F4_SEG;
        pDevCtx = pGi->pDevCtxs[0];
    }
    else
        if( Cell.ConnAddr.u.Vcc.usVci == usOamF4VciEnd )
        {
            ucCHdr = CHDR_CT_OAM_F4_E2E;
            pDevCtx = pGi->pDevCtxs[0];
        }
        else
        {
            pDevCtx = FindDevCtx( (short) Cell.ConnAddr.u.Vcc.usVpi,
                (int) Cell.ConnAddr.u.Vcc.usVci);
        }

    Cell.ucCircuitType = ucCts[(ucCHdr & CHDR_CT_MASK) >> CHDR_CT_SHIFT];

    if( (ucCHdr & CHDR_ERROR) == 0 )
    {
        memcpy(Cell.ucData, pucData + sizeof(char), sizeof(Cell.ucData));

        /* Call the registered OAM or ASM callback function. */
        switch( ucCHdr & CHDR_CT_MASK )
        {
        case CHDR_CT_OAM_F5_SEG:
        case CHDR_CT_OAM_F5_E2E:
        case CHDR_CT_OAM_F4_SEG:
        case CHDR_CT_OAM_F4_E2E:
            if( pGi->pfnOamHandler && pDevCtx )
            {
                (*pGi->pfnOamHandler) ((XTMRT_HANDLE)pDevCtx,
                    XTMRTCB_CMD_CELL_RECEIVED, &Cell,
                    pGi->pOamContext);
            }
            break;

        case CHDR_CT_ASM_P0:
        case CHDR_CT_ASM_P1:
        case CHDR_CT_ASM_P2:
        case CHDR_CT_ASM_P3:
            if( pGi->pfnAsmHandler && pDevCtx )
            {
                (*pGi->pfnAsmHandler) ((XTMRT_HANDLE)pDevCtx,
                    XTMRTCB_CMD_CELL_RECEIVED, &Cell,
                    pGi->pAsmContext);
            }
            break;

        default:
            break;
        }
    }
    else
        if( pDevCtx )
            pDevCtx->DevStats.rx_errors++;

    /* Put the buffer back onto the BD ring. */
    FlushAssignRxBuffer(pRxBdInfo, pucData, pucData + RXBUF_SIZE);

} /* ProcessRxCell */

/***************************************************************************
 * Function Name: MirrorPacket
 * Description  : This function sends a sent or received packet to a LAN port.
 *                The purpose is to allow packets sent and received on the WAN
 *                to be captured by a protocol analyzer on the Lan for debugging
 *                purposes.
 * Returns      : None.
 ***************************************************************************/
static void MirrorPacket( struct sk_buff *skb, char *intfName )
{
    struct sk_buff *skbClone;
    struct net_device *netDev;

    if( (skbClone = skb_clone(skb, GFP_ATOMIC)) != NULL )
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
        if( (netDev = __dev_get_by_name(&init_net, intfName)) != NULL )
#else
        if( (netDev = __dev_get_by_name(intfName)) != NULL )
#endif
        {
            unsigned long flags;

            // blog_xfer(skb, skbClone);
            skbClone->dev = netDev;
            skbClone->protocol = htons(ETH_P_802_3);
            local_irq_save(flags);
            local_irq_enable();
            dev_queue_xmit(skbClone) ;
            local_irq_restore(flags);
        }
        else
            dev_kfree_skb(skbClone);
    }
} /* MirrorPacket */

/***************************************************************************
 * Function Name: bcmxtmrt_timer
 * Description  : Periodic timer that calls the send function to free packets
 *                that have been transmitted.
 * Returns      : None.
 ***************************************************************************/
static void bcmxtmrt_timer( PBCMXTMRT_GLOBAL_INFO pGi )
{
    UINT32 i;

    /* Free transmitted buffers. */
    for( i = 0; i < MAX_DEV_CTXS; i++ )
        if( pGi->pDevCtxs[i] )
            bcmxtmrt_xmit( PNBUFF_NULL, pGi->pDevCtxs[i]->pDev );

    if (pGi->bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE) {
       if (pGi->ptmBondInfo.tickCount >= MAX_TICK_COUNT) {
          spin_lock_bh(&pGi->xtmlock_rx_bond);
          bcmxtmrt_ptmbond_tick (&pGi->ptmBondInfo) ;
          spin_unlock_bh(&pGi->xtmlock_rx_bond);
          pGi->ptmBondInfo.tickCount = 0 ;
       }
       else
          pGi->ptmBondInfo.tickCount++ ;
    }

    /* Restart the timer. */
    pGi->Timer.expires = jiffies + SAR_TIMEOUT;
    add_timer(&pGi->Timer);
} /* bcmxtmrt_timer */


/***************************************************************************
 * Function Name: bcmxtmrt_request
 * Description  : Request from the bcmxtmcfg driver.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
int bcmxtmrt_request( XTMRT_HANDLE hDev, UINT32 ulCommand, void *pParm )
{
    PBCMXTMRT_DEV_CONTEXT pDevCtx = (PBCMXTMRT_DEV_CONTEXT) hDev;
    int nRet = 0;

    switch( ulCommand )
    {
    case XTMRT_CMD_GLOBAL_INITIALIZATION:
        nRet = DoGlobInitReq( (PXTMRT_GLOBAL_INIT_PARMS) pParm );
        break;

    case XTMRT_CMD_GLOBAL_UNINITIALIZATION:
        nRet = DoGlobUninitReq();
        break;

    case XTMRT_CMD_CREATE_DEVICE:
        nRet = DoCreateDeviceReq( (PXTMRT_CREATE_NETWORK_DEVICE) pParm );
        break;

    case XTMRT_CMD_GET_DEVICE_STATE:
        *(UINT32 *) pParm = pDevCtx->ulOpenState;
        break;

    case XTMRT_CMD_SET_ADMIN_STATUS:
        pDevCtx->ulAdminStatus = (UINT32) pParm;
        break;

    case XTMRT_CMD_REGISTER_CELL_HANDLER:
        nRet = DoRegCellHdlrReq( (PXTMRT_CELL_HDLR) pParm );
        break;

    case XTMRT_CMD_UNREGISTER_CELL_HANDLER:
        nRet = DoUnregCellHdlrReq( (PXTMRT_CELL_HDLR) pParm );
        break;

    case XTMRT_CMD_LINK_STATUS_CHANGED:
        nRet = DoLinkStsChangedReq(pDevCtx, (PXTMRT_LINK_STATUS_CHANGE)pParm);
        break;

    case XTMRT_CMD_SEND_CELL:
        nRet = DoSendCellReq( pDevCtx, (PXTMRT_CELL) pParm );
        break;

    case XTMRT_CMD_DELETE_DEVICE:
        nRet = DoDeleteDeviceReq( pDevCtx );
        break;

    case XTMRT_CMD_SET_TX_QUEUE:
        nRet = DoSetTxQueue( pDevCtx, (PXTMRT_TRANSMIT_QUEUE_ID) pParm );
        break;

    case XTMRT_CMD_UNSET_TX_QUEUE:
        nRet = DoUnsetTxQueue( pDevCtx, (PXTMRT_TRANSMIT_QUEUE_ID) pParm );
        break;

    case XTMRT_CMD_GET_NETDEV_TXCHANNEL:
        nRet = DoGetNetDevTxChannel( (PXTMRT_NETDEV_TXCHANNEL) pParm );
        break;

    default:
        nRet = -EINVAL;
        break;
    }

    return( nRet );
} /* bcmxtmrt_request */


/***************************************************************************
 * Function Name: DoGlobInitReq
 * Description  : Processes an XTMRT_CMD_GLOBAL_INITIALIZATION command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoGlobInitReq( PXTMRT_GLOBAL_INIT_PARMS pGip )
{
    int nRet = 0;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    struct DmaDesc *pBd, *pBdBase = NULL;
    UINT32 ulBufsToAlloc;
    UINT32 ulAllocAmt;
    UINT8 *p;
    UINT32 i, j, ulSize;

    if( pGi->ulDrvState != XTMRT_UNINITIALIZED )
        return -EPERM;

    spin_lock_init(&pGi->xtmlock_tx);
    spin_lock_init(&pGi->xtmlock_rx);
    spin_lock_init(&pGi->xtmlock_rx_regs);

    /* Save MIB counter/Cam registers. */
    pGi->pulMibTxOctetCountBase = pGip->pulMibTxOctetCountBase;
    pGi->ulMibRxClrOnRead = pGip->ulMibRxClrOnRead;
    pGi->pulMibRxCtrl = pGip->pulMibRxCtrl;
    pGi->pulMibRxMatch = pGip->pulMibRxMatch;
    pGi->pulMibRxOctetCount = pGip->pulMibRxOctetCount;
    pGi->pulMibRxPacketCount = pGip->pulMibRxPacketCount;
    pGi->pulRxCamBase = pGip->pulRxCamBase;

    /* Determine the number of receive buffers. */
    for( i = 0, ulBufsToAlloc = 0; i < MAX_RECEIVE_QUEUES; i++ )
        ulBufsToAlloc += pGip->ulReceiveQueueSizes[i];

    /* Allocate receive DMA buffer descriptors. */
    ulSize = (ulBufsToAlloc * sizeof(struct DmaDesc)) + 0x10;
    if( nRet == 0 && (pGi->pRxBdMem = (struct DmaDesc *)
        kmalloc(ulSize, GFP_KERNEL)) != NULL )
    {
        pBdBase = (struct DmaDesc *) (((UINT32) pGi->pRxBdMem + 0x0f) & ~0x0f);
        p = (UINT8 *) pGi->pRxBdMem;
        cache_flush_len(p, ulSize);
        pBdBase = (struct DmaDesc *) CACHE_TO_NONCACHE(pBdBase);
    }
    else
        nRet = -ENOMEM;

    /* Allocate receive socket buffers and data buffers. */
    if( nRet == 0 )
    {
        const UINT32 ulRxAllocSize = SKB_ALIGNED_SIZE + RXBUF_ALLOC_SIZE;
        const UINT32 ulBlockSize = (64 * 1024);
        const UINT32 ulBufsPerBlock = ulBlockSize / ulRxAllocSize;

        j = 0;
        pBd = pBdBase;
        pGi->pFreeRxSkbList = (struct sk_buff *)NULL;
        while( ulBufsToAlloc )
        {
            ulAllocAmt = (ulBufsPerBlock < ulBufsToAlloc)
                ? ulBufsPerBlock : ulBufsToAlloc;

            ulSize = ulAllocAmt * ulRxAllocSize;
            ulSize = (ulSize + 0x0f) & ~0x0f;
            if( j < MAX_BUFMEM_BLOCKS &&
                (p = kmalloc(ulSize, GFP_KERNEL)) != NULL )
            {
                UINT8 *p2, *pData;
                pGi->pBufMem[j++] = p;
                memset(p, 0x00, ulSize);
                cache_flush_len(p, ulSize);

                p = (UINT8 *) (((UINT32) p + 0x0f) & ~0x0f);
                for( i = 0; i < ulAllocAmt; i++ )
                {
                    pData = PFKBUFF_TO_PDATA(p,RXBUF_HEAD_RESERVE);
                    /* FKB recycle context set in fkb_qinit() */
                    fkb_preinit(p,(RecycleFuncP)bcmxtmrt_recycle, 0);
                    pBd->status = DMA_OWN;
                    pBd->length = RXBUF_SIZE;
                    pBd->address = (UINT32)VIRT_TO_PHY(pData);
                    pBd++;

                    p2 = p + RXBUF_ALLOC_SIZE;
                    ((struct sk_buff *) p2)->next_free =
                        pGi->pFreeRxSkbList;
                    pGi->pFreeRxSkbList = (struct sk_buff *) p2;
                    p += ulRxAllocSize;
                }
                ulBufsToAlloc -= ulAllocAmt;
            }
            else
            {
                /* Allocation error. */
                for (i = 0; i < MAX_BUFMEM_BLOCKS; i++)
                {
                    if (pGi->pBufMem[i])
                    {
                        kfree(pGi->pBufMem[i]);
                        pGi->pBufMem[i] = NULL;
                    }
                }
                kfree((const UINT8 *) pGi->pRxBdMem);
                pGi->pRxBdMem = NULL;
                nRet = -ENOMEM;
            }
        }
    }

    pGi->bondConfig.uConfig = pGip->bondConfig.uConfig ;
    if (pGi->bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE) {
       bcmxtmrt_ptmbond_initialize (PTM_BOND_INITIALIZE_GLOBAL) ;
       spin_lock_init(&pGi->xtmlock_rx_bond);
    }
    /* Initialize receive DMA registers. */
    if( nRet == 0 )
    {
        volatile DmaChannelCfg *pRxDma, *pTxDma;
        volatile DmaStateRam *pStRam;

        /* Clear State RAM for receive DMA Channels. */
        pGi->pDmaRegs = (DmaRegs *) SAR_DMA_BASE;
        pStRam = pGi->pDmaRegs->stram.s;
        memset((char *) &pStRam[SAR_RX_DMA_BASE_CHAN], 0x00,
            sizeof(DmaStateRam) * NR_SAR_RX_DMA_CHANS);
        pGi->pRxDmaBase = pGi->pDmaRegs->chcfg+SAR_RX_DMA_BASE_CHAN;
        pRxDma = pGi->pRxDmaBase;
        pBd = pBdBase;

        for(i=0, pRxDma=pGi->pRxDmaBase; i < MAX_RECEIVE_QUEUES; i++, pRxDma++)
        {
            pRxDma->cfg = 0;
            BcmHalInterruptDisable(SAR_RX_INT_ID_BASE + i);
            if( pGip->ulReceiveQueueSizes[i] )
            {
                PRXBDINFO pRxBdInfo = &pGi->RxBdInfos[i];

                pRxDma->maxBurst = SAR_DMA_MAX_BURST_LENGTH;
                pRxDma->intStat = DMA_DONE | DMA_NO_DESC | DMA_BUFF_DONE;
                pRxDma->intMask = DMA_BUFF_DONE;
                pRxBdInfo->pBdBase = pBd;
                pRxBdInfo->pBdHead = pBd;
                pRxBdInfo->pBdTail = pBd;
                pRxBdInfo->pDma = pRxDma;
                pStRam[SAR_RX_DMA_BASE_CHAN+i].baseDescPtr = (UINT32)
                    VIRT_TO_PHY(pRxBdInfo->pBdBase);
                pBd += pGip->ulReceiveQueueSizes[i] - 1;
                pBd++->status |= DMA_WRAP;

                BcmHalMapInterrupt((FN_HANDLER) bcmxtmrt_rxisr, (UINT32)
                    pRxDma, SAR_RX_INT_ID_BASE + i);
            }
            else
                memset(&pGi->RxBdInfos[i], 0x00, sizeof(RXBDINFO));
        }

        pGi->pDmaRegs->controller_cfg |= DMA_MASTER_EN;

        /* Clear State RAM for transmit DMA Channels. */
        memset( (char *) &pStRam[SAR_TX_DMA_BASE_CHAN], 0x00,
            sizeof(DmaStateRam) * NR_SAR_TX_DMA_CHANS );
        pGi->pTxDmaBase = pGi->pDmaRegs->chcfg+SAR_TX_DMA_BASE_CHAN;

        for(i=0, pTxDma=pGi->pTxDmaBase; i < MAX_TRANSMIT_QUEUES; i++, pTxDma++)
        {
            pTxDma->cfg = 0;
            BcmHalInterruptDisable(SAR_TX_INT_ID_BASE + i);
        }

        /* Initialize a timer function to free transmit buffers. */
        init_timer(&pGi->Timer);
        pGi->Timer.data = (unsigned long) pGi;
        pGi->Timer.function = (void *) bcmxtmrt_timer;

        pGi->ulDrvState = XTMRT_INITIALIZED;
    }

    return( nRet );
} /* DoGlobInitReq */


/***************************************************************************
 * Function Name: DoGlobUninitReq
 * Description  : Processes an XTMRT_CMD_GLOBAL_UNINITIALIZATION command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoGlobUninitReq( void )
{
    int nRet = 0;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 i;

    if( pGi->ulDrvState == XTMRT_UNINITIALIZED )
    {
        nRet = -EPERM;
    }
    else
    {
        pGi->ulDrvState = XTMRT_UNINITIALIZED;

        for( i = 0; i < MAX_RECEIVE_QUEUES; i++ )
            BcmHalInterruptDisable(SAR_RX_INT_ID_BASE + i);
        del_timer_sync(&pGi->Timer);

        for (i = 0; i < MAX_BUFMEM_BLOCKS; i++)
        {
            if (pGi->pBufMem[i])
            {
                kfree(pGi->pBufMem[i]);
                pGi->pBufMem[i] = NULL;
            }
        }
        kfree((const UINT8 *) pGi->pRxBdMem);
        pGi->pRxBdMem = NULL;
    }
    return( nRet );

} /* DoGlobUninitReq */


/***************************************************************************
 * Function Name: DoCreateDeviceReq
 * Description  : Processes an XTMRT_CMD_CREATE_DEVICE command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoCreateDeviceReq( PXTMRT_CREATE_NETWORK_DEVICE pCnd )
{
    int nRet = 0 ;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    PBCMXTMRT_DEV_CONTEXT pDevCtx = NULL;
    struct net_device *dev = NULL;
    int i;
    UINT32 unit = 0;
    UINT32 macId = 0;

    if( pGi->ulDrvState != XTMRT_UNINITIALIZED &&
        (dev = alloc_netdev( sizeof(BCMXTMRT_DEV_CONTEXT),
         pCnd->szNetworkDeviceName, ether_setup )) != NULL )
    {
        dev_alloc_name(dev, dev->name);
        SET_MODULE_OWNER(dev);

 #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
        pDevCtx = (PBCMXTMRT_DEV_CONTEXT) netdev_priv(dev);
 #else
        pDevCtx = (PBCMXTMRT_DEV_CONTEXT) dev->priv;
 #endif
        memset(pDevCtx, 0x00, sizeof(BCMXTMRT_DEV_CONTEXT));
        memcpy(&pDevCtx->Addr, &pCnd->ConnAddr, sizeof(XTM_ADDR));
        if( pCnd->ConnAddr.ulTrafficType == TRAFFIC_TYPE_ATM )
            pDevCtx->ulHdrType = pCnd->ulHeaderType;
        else
            pDevCtx->ulHdrType = HT_PTM;
        pDevCtx->ulFlags = pCnd->ulFlags;
        pDevCtx->pDev = dev;
        pDevCtx->ulAdminStatus = ADMSTS_UP;
        pDevCtx->ucTxVcid = INVALID_VCID;

        /* Read and display the MAC address. */
        dev->dev_addr[0] = 0xff;

        /* format the mac id */
        i = strcspn(dev->name, "0123456789");
        if (i > 0)
           unit = simple_strtoul(&(dev->name[i]), (char **)NULL, 10);

        if (pDevCtx->ulHdrType == HT_PTM)
           macId = MAC_ADDRESS_PTM;
        else
           macId = MAC_ADDRESS_ATM;
        /* set unit number to bit 20-27 */
        macId |= ((unit & 0xff) << 20);

        kerSysGetMacAddress(dev->dev_addr, macId);

        if( (dev->dev_addr[0] & 0x01) == 0x01 )
        {
            printk( KERN_ERR CARDNAME": Unable to read MAC address from "
                "persistent storage.  Using default address.\n" );
            memcpy( dev->dev_addr, "\x02\x10\x18\x02\x00\x01", 6 );
        }

        printk( CARDNAME": MAC address: %2.2x %2.2x %2.2x %2.2x %2.2x "
            "%2.2x\n", dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
            dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5] );
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
        dev->netdev_ops = &bcmXtmRt_netdevops;
#else
        /* Setup the callback functions. */
        dev->open               = bcmxtmrt_open;
        dev->stop               = bcmxtmrt_close;
        dev->hard_start_xmit    = (HardStartXmitFuncP) bcmxtmrt_xmit;
        dev->tx_timeout         = bcmxtmrt_timeout;
        dev->set_multicast_list = NULL;
        dev->do_ioctl           = &bcmxtmrt_ioctl;
        dev->poll               = bcmxtmrt_poll;
        dev->weight             = 64;
        dev->get_stats          = bcmxtmrt_query;
#endif
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
        dev->clr_stats          = bcmxtmrt_clrStats;
#endif
        dev->watchdog_timeo     = SAR_TIMEOUT;

        /* identify as a WAN interface to block WAN-WAN traffic */
        dev->priv_flags |= IFF_WANDEV;

        switch( pDevCtx->ulHdrType )
        {
        case HT_LLC_SNAP_ROUTE_IP:
        case HT_VC_MUX_IPOA:
            pDevCtx->ulEncapType = TYPE_IP;     /* IPoA */

            /* Since IPoA does not need a Ethernet header,
             * set the pointers below to NULL. Refer to kernel rt2684.c.
             */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
            dev->header_ops = &bcmXtmRt_headerOps;
#else
            dev->hard_header = NULL;
            dev->rebuild_header = NULL;
            dev->set_mac_address = NULL;
            dev->hard_header_parse = NULL;
            dev->hard_header_cache = NULL;
            dev->header_cache_update = NULL;
            dev->change_mtu = NULL;
#endif //LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)

            dev->type = ARPHRD_PPP;
            dev->hard_header_len = HT_LEN_LLC_SNAP_ROUTE_IP;
            dev->mtu = RFC1626_MTU;
            dev->addr_len = 0;
            dev->tx_queue_len = 100;
            dev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
            break;

        case HT_LLC_ENCAPS_PPP:
        case HT_VC_MUX_PPPOA:
            pDevCtx->ulEncapType = TYPE_PPP;    /*PPPoA*/
            break;

        default:
            pDevCtx->ulEncapType = TYPE_ETH;    /* bridge, MER, PPPoE, PTM */
            dev->flags = IFF_BROADCAST | IFF_MULTICAST;
            break;
        }
        
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
        netif_napi_add(dev, &pDevCtx->napi, bcmxtmrt_poll_napi, 64);
#endif      
        /* Don't reset or enable the device yet. "Open" does that. */
        printk("[%s.%d]: register_netdev\n", __func__, __LINE__);
        nRet = register_netdev(dev);
        printk("[%s.%d]: register_netdev done\n", __func__, __LINE__);
        if (nRet == 0) 
        {
            UINT32 i;
            for( i = 0; i < MAX_DEV_CTXS; i++ )
                if( pGi->pDevCtxs[i] == NULL )
                {
                    pGi->pDevCtxs[i] = pDevCtx;
                    break;
                }

            pCnd->hDev = (XTMRT_HANDLE) pDevCtx;
        }
        else
        {
            printk(KERN_ERR CARDNAME": register_netdev failed\n");
            free_netdev(dev);
        }

        if( nRet != 0 )
            kfree(pDevCtx);
    }
    else
    {
        printk(KERN_ERR CARDNAME": alloc_netdev failed\n");
        nRet = -ENOMEM;
    }

    return( nRet );
} /* DoCreateDeviceReq */


/***************************************************************************
 * Function Name: DoRegCellHdlrReq
 * Description  : Processes an XTMRT_CMD_REGISTER_CELL_HANDLER command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoRegCellHdlrReq( PXTMRT_CELL_HDLR pCh )
{
    int nRet = 0;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;

    switch( pCh->ulCellHandlerType )
    {
    case CELL_HDLR_OAM:
        if( pGi->pfnOamHandler == NULL )
        {
            pGi->pfnOamHandler = pCh->pfnCellHandler;
            pGi->pOamContext = pCh->pContext;
        }
        else
            nRet = -EEXIST;
        break;

    case CELL_HDLR_ASM:
        if( pGi->pfnAsmHandler == NULL )
        {
            pGi->pfnAsmHandler = pCh->pfnCellHandler;
            pGi->pAsmContext = pCh->pContext;
        }
        else
            nRet = -EEXIST;
        break;
    }

    return( nRet );
} /* DoRegCellHdlrReq */


/***************************************************************************
 * Function Name: DoUnregCellHdlrReq
 * Description  : Processes an XTMRT_CMD_UNREGISTER_CELL_HANDLER command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoUnregCellHdlrReq( PXTMRT_CELL_HDLR pCh )
{
    int nRet = 0;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;

    switch( pCh->ulCellHandlerType )
    {
    case CELL_HDLR_OAM:
        if( pGi->pfnOamHandler == pCh->pfnCellHandler )
        {
            pGi->pfnOamHandler = NULL;
            pGi->pOamContext = NULL;
        }
        else
            nRet = -EPERM;
        break;

    case CELL_HDLR_ASM:
        if( pGi->pfnAsmHandler == pCh->pfnCellHandler )
        {
            pGi->pfnAsmHandler = NULL;
            pGi->pAsmContext = NULL;
        }
        else
            nRet = -EPERM;
        break;
    }

    return( nRet );
} /* DoUnregCellHdlrReq */


/***************************************************************************
 * Function Name: DoLinkStsChangedReq
 * Description  : Processes an XTMRT_CMD_LINK_STATUS_CHANGED command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoLinkStsChangedReq( PBCMXTMRT_DEV_CONTEXT pDevCtx,
     PXTMRT_LINK_STATUS_CHANGE pLsc )
{
   PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
   int nRet = -EPERM;

   local_bh_disable();

   if( pDevCtx )
   {
      UINT32 i;

      for( i = 0; i < MAX_DEV_CTXS; i++ )
      {
         if( pGi->pDevCtxs[i] == pDevCtx )
         {
            pDevCtx->ulFlags |= pLsc->ulLinkState & LSC_RAW_ENET_MODE;
            pLsc->ulLinkState &= ~LSC_RAW_ENET_MODE;
            pDevCtx->MibInfo.ulIfLastChange = (jiffies * 100) / HZ;
            pDevCtx->MibInfo.ulIfSpeed = pLsc->ulLinkUsRate;

            if( pLsc->ulLinkState == LINK_UP ) {
               nRet = DoLinkUp( pDevCtx, pLsc );
            }
            else {
               spin_lock(&pGi->xtmlock_tx);
               nRet = DoLinkDownTx( pDevCtx, pLsc );
               spin_unlock(&pGi->xtmlock_tx);
            }
            break;
         }
      }

      if (pGi->bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE) {
         spin_lock (&pGi->xtmlock_tx) ;
         pGi->ptmBondInfo.lastTxPktSz = 0 ;
         spin_unlock (&pGi->xtmlock_tx) ;

         spin_lock (&pGi->xtmlock_rx_bond) ;
         bcmxtmrt_ptmbond_handle_port_status_change (&pGi->ptmBondInfo, pLsc->ulLinkState,
                                                     pDevCtx->ulPortDataMask) ;
         spin_unlock (&pGi->xtmlock_rx_bond) ;
      }
   }
   else
   {
      /* No device context indicates that the link is down.  Do global link
       * down processing.  pLsc is really an unsigned long containing the
       * port id.
       */
      spin_lock(&pGi->xtmlock_rx);
      nRet = DoLinkDownRx( (UINT32) pLsc );
      spin_unlock(&pGi->xtmlock_rx);
   }

   local_bh_enable();

   return( nRet );
} /* DoLinkStsChangedReq */


/***************************************************************************
 * Function Name: DoLinkUp
 * Description  : Processes a "link up" condition.
 *                In bonding case, successive links may be coming UP one after
 *                another, accordingly the processing differs. 
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoLinkUp( PBCMXTMRT_DEV_CONTEXT pDevCtx,
     PXTMRT_LINK_STATUS_CHANGE pLsc )
{
    int nRet = 0;
    volatile DmaChannelCfg *pRxDma;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    PXTMRT_TRANSMIT_QUEUE_ID pTxQId;
    PTXQINFO pTxQInfo;
    UINT32 i ;

    if (pDevCtx->ulLinkState != pLsc->ulLinkState) {
       /* Initialize transmit DMA channel information. */
       pDevCtx->ucTxVcid = pLsc->ucTxVcid;
       pDevCtx->ulLinkState = pLsc->ulLinkState;
       pDevCtx->ulTxQInfosSize = 0;

       /* Use each Rx vcid as an index into an array of bcmxtmrt devices
        * context structures.
        */
       for( i = 0; i < pLsc->ulRxVcidsSize ; i++ ) {
            pGi->pDevCtxsByMatchId[pLsc->ucRxVcids[i] ] = pDevCtx;
       }

       for(i = 0,pTxQInfo = pDevCtx->TxQInfos, pTxQId = pLsc->TransitQueueIds;
             i < pLsc->ulTransmitQueueIdsSize && nRet == 0; i++, pTxQInfo++, pTxQId++)
       {
          nRet = DoSetTxQueue(pDevCtx, pTxQId );
       }

       if( nRet == 0 )
       {
          /* If it is not already there, put the driver into a "ready to send and
           * receive state".
           */
          if( pGi->ulDrvState == XTMRT_INITIALIZED )
          {
             /* Enable receive interrupts and start a timer. */
             for( i = 0, pRxDma = pGi->pRxDmaBase; i < MAX_RECEIVE_QUEUES;
                   i++, pRxDma++ )
             {
                if( pGi->RxBdInfos[i].pBdBase )
                {
                   pRxDma->cfg = DMA_ENABLE;
                   BcmHalInterruptEnable(SAR_RX_INT_ID_BASE + i);
                }
             }

             pGi->Timer.expires = jiffies + SAR_TIMEOUT;
             add_timer(&pGi->Timer);

             if( pDevCtx->ulOpenState == XTMRT_DEV_OPENED )
                netif_start_queue(pDevCtx->pDev);

             pGi->ulDrvState = XTMRT_RUNNING;
          }
       }
       else
       {
          /*Memory allocation error. Free memory that was previously allocated.*/
          for(i = 0, pTxQInfo = pDevCtx->TxQInfos; i < pDevCtx->ulTxQInfosSize;
                i++, pTxQInfo++ )
          {
             if( pTxQInfo->pMemBuf )
             {
                kfree(pTxQInfo->pMemBuf);
                pTxQInfo->pMemBuf = NULL;
             }
          }
          pDevCtx->ulTxQInfosSize = 0;
       }
    }
    pDevCtx->ulPortDataMask = pLsc->ulLinkDataMask ;

    return( nRet );
} /* DoLinkUp */


/***************************************************************************
 * Function Name: DoLinkDownRx
 * Description  : Processes a "link down" condition for receive only.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoLinkDownRx( UINT32 ulPortId )
{
    int nRet = 0;
    volatile DmaChannelCfg *pRxDma;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 i, ulStopRunning;

    /* If all links are down, put the driver into an "initialized" state. */
    for( i = 0, ulStopRunning = 1; i < MAX_DEV_CTXS; i++ )
    {
        if( pGi->pDevCtxs[i] )
        {
            PBCMXTMRT_DEV_CONTEXT pDevCtx = pGi->pDevCtxs[i];
            UINT32 ulDevPortId = pDevCtx->ulPortDataMask ;
            if( (ulDevPortId & ~ulPortId) != 0 )
            {
                /* At least one link that uses a different port is up.
                 * For Ex., in bonding case, one of the links can be up
                 */
                ulStopRunning = 0;
                break;
            }
        }
    }

    if( ulStopRunning )
    {
        /* Disable receive interrupts and stop the timer. */
        for( i = 0, pRxDma = pGi->pRxDmaBase; i < MAX_RECEIVE_QUEUES;
             i++, pRxDma++ )
        {
            if( pGi->RxBdInfos[i].pBdBase )
            {
                pRxDma->cfg = 0;
                BcmHalInterruptDisable(SAR_RX_INT_ID_BASE + i);
            }
        }

        del_timer_sync(&pGi->Timer);

        pGi->ulDrvState = XTMRT_INITIALIZED;
    }

    return( nRet );
} /* DoLinkDownRx */


/***************************************************************************
 * Function Name: DoLinkDownTx
 * Description  : Processes a "link down" condition for transmit only.
 *                In bonding case, one of the links could still be UP, in which
 *                case only the link data status is updated.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoLinkDownTx( PBCMXTMRT_DEV_CONTEXT pDevCtx,
     						    PXTMRT_LINK_STATUS_CHANGE pLsc )
{
    int nRet = 0;
    volatile DmaStateRam  *pStRam;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    PTXQINFO pTxQInfo;
    UINT32 i, j, ulIdx;

    if (pLsc->ulLinkDataMask == 0) {
       /* Disable transmit DMA. */
       pDevCtx->ulLinkState = LINK_DOWN;
       pStRam = pGi->pDmaRegs->stram.s;
       for( i = 0, pTxQInfo = pDevCtx->TxQInfos; i < pDevCtx->ulTxQInfosSize;
             i++, pTxQInfo++ )
       {
          /* Changing the link state to LINK_DOWN prevents any more packets
           * from being queued on a transmit DMA channel.  Allow all currenlty
           * queued transmit packets to be transmitted before disabling the DMA.
           */
          for( j = 0; j < 2000 &&
                (pTxQInfo->pDma->cfg & DMA_ENABLE) == DMA_ENABLE; j++ )
          {
             udelay(500);
          }

          if( (pTxQInfo->pDma->cfg & DMA_ENABLE) == DMA_ENABLE )
          {
             /* This should not happen. */
             printk(KERN_ERR CARDNAME ":**** DMA_PKT_HALT ****\n");
             pTxQInfo->pDma->cfg = DMA_PKT_HALT;
             udelay(500);
             pTxQInfo->pDma->cfg = 0;
          }
          ulIdx = SAR_TX_DMA_BASE_CHAN + pTxQInfo->ulDmaIndex;
          pStRam[ulIdx].baseDescPtr = 0;
          pStRam[ulIdx].state_data = 0;
          pStRam[ulIdx].desc_len_status = 0;
          pStRam[ulIdx].desc_base_bufptr = 0;

          /* Free transmitted packets. */
          for( j = 0; j < pTxQInfo->ulQueueSize; j++ )
             if( pTxQInfo->ppNBuffs[j] && pTxQInfo->pBds[j].address )
             {
                nbuff_free(pTxQInfo->ppNBuffs[j]);
                pTxQInfo->ppNBuffs[j] = PNBUFF_NULL;
             }

          pTxQInfo->ulFreeBds = pTxQInfo->ulQueueSize = 0;
          pTxQInfo->ulNumTxBufsQdOne = 0;

          /* Free memory used for transmit queue. */
          if( pTxQInfo->pMemBuf )
          {
             kfree(pTxQInfo->pMemBuf);
             pTxQInfo->pMemBuf = NULL;
          }
       }

       /* Zero transmit related data structures. */
       pDevCtx->ulTxQInfosSize = 0;
       memset(pDevCtx->TxQInfos, 0x00, sizeof(pDevCtx->TxQInfos));
       memset(pDevCtx->pTxPriorities, 0x00, sizeof(pDevCtx->pTxPriorities));
       pDevCtx->ucTxVcid = INVALID_VCID;

       pGi->ulNumTxBufsQdAll = 0;

       /* Zero receive vcids. */
       for( i = 0; i < MAX_MATCH_IDS; i++ )
          if( pGi->pDevCtxsByMatchId[i] == pDevCtx )
             pGi->pDevCtxsByMatchId[i] = NULL;
    }

    pDevCtx->ulPortDataMask = pLsc->ulLinkDataMask ;

    return( nRet );
} /* DoLinkDownTx */


/***************************************************************************
 * Function Name: DoSetTxQueue
 * Description  : Allocate memory for and initialize a transmit queue.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoSetTxQueue( PBCMXTMRT_DEV_CONTEXT pDevCtx,
    PXTMRT_TRANSMIT_QUEUE_ID pTxQId )
{
    int nRet = 0;
    UINT32 ulQueueSize, ulPort, ulSize;
    UINT8 *p;
    PTXQINFO pTxQInfo =  &pDevCtx->TxQInfos[pDevCtx->ulTxQInfosSize++];
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;

    /* Set every transmit queue size to the number of external buffers.
     * The QueuePacket function will control how many packets are queued.
     */
    ulQueueSize = pGi->ulNumExtBufs;

    /* 0x20 for alignment */
    ulSize = (ulQueueSize * (sizeof(pNBuff_t) + sizeof(struct DmaDesc))) + 0x20;

    if( (pTxQInfo->pMemBuf = kmalloc(ulSize, GFP_ATOMIC)) != NULL )
    {
        memset(pTxQInfo->pMemBuf, 0x00, ulSize);
        cache_flush_len(pTxQInfo->pMemBuf, ulSize);


        ulPort = PORTID_TO_PORT(pTxQId->ulPortId);
        if( ulPort < MAX_PHY_PORTS &&
            pTxQId->ulSubPriority < MAX_SUB_PRIORITIES )
        {
            volatile DmaStateRam *pStRam = pGi->pDmaRegs->stram.s;
            UINT32 i, ulTxQs, ulPtmPriority = 0;

            p = (UINT8 *) (((UINT32) pTxQInfo->pMemBuf + 0x0f) & ~0x0f);
            pTxQInfo->ulPort = ulPort;
            pTxQInfo->ulPtmPriority = pTxQId->ulPtmPriority;
            pTxQInfo->ulSubPriority = pTxQId->ulSubPriority;
            pTxQInfo->ulQueueSize = ulQueueSize;
            pTxQInfo->ulDmaIndex = pTxQId->ulQueueIndex;
            pTxQInfo->pDma = pGi->pTxDmaBase + pTxQInfo->ulDmaIndex;
            pTxQInfo->pBds = (volatile DmaDesc *) CACHE_TO_NONCACHE(p);
            pTxQInfo->pBds[pTxQInfo->ulQueueSize - 1].status |= DMA_WRAP;
            p += ((sizeof(struct DmaDesc) * ulQueueSize)+0x0f) & ~0x0f;
            pTxQInfo->ppNBuffs = (pNBuff_t *) p;
            pTxQInfo->ulNumTxBufsQdOne = 0;
            pTxQInfo->ulFreeBds = pTxQInfo->ulQueueSize;
            pTxQInfo->ulHead = pTxQInfo->ulTail = 0;

            pTxQInfo->pDma->cfg = 0;
            pTxQInfo->pDma->maxBurst = SAR_DMA_MAX_BURST_LENGTH;
            pTxQInfo->pDma->intStat = DMA_DONE | DMA_NO_DESC | DMA_BUFF_DONE;
            memset((UINT8 *)&pStRam[SAR_TX_DMA_BASE_CHAN+pTxQInfo->ulDmaIndex],
                0x00, sizeof(DmaStateRam));
            pStRam[SAR_TX_DMA_BASE_CHAN + pTxQInfo->ulDmaIndex].baseDescPtr =
                (UINT32) VIRT_TO_PHY(pTxQInfo->pBds);

            if (pDevCtx->Addr.ulTrafficType == TRAFFIC_TYPE_PTM)
               ulPtmPriority = (pTxQInfo->ulPtmPriority == PTM_PRI_HIGH)? 1 : 0;
            pDevCtx->pTxPriorities[ulPtmPriority][ulPort][pTxQInfo->ulSubPriority] = pTxQInfo;

            /* Count the total number of transmit queues used across all device
             * interfaces.
             */
            for( i = 0, ulTxQs = 0; i < MAX_DEV_CTXS; i++ )
                if( pGi->pDevCtxs[i] )
                    ulTxQs += pGi->pDevCtxs[i]->ulTxQInfosSize;
            pGi->ulNumTxQs = ulTxQs;
        }
        else
        {
            printk(CARDNAME ": Invalid transmit queue port/priority\n");
            nRet = -EFAULT;
        }
    }
    else
        nRet = -ENOMEM;

    return( nRet );
} /* DoSetTxQueue */


/***************************************************************************
 * Function Name: DoUnsetTxQueue
 * Description  : Frees memory for a transmit queue.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoUnsetTxQueue( PBCMXTMRT_DEV_CONTEXT pDevCtx,
    PXTMRT_TRANSMIT_QUEUE_ID pTxQId )
{
    int nRet = 0;
    UINT32 i, j, ulTxQs;
    PTXQINFO pTxQInfo;

    for( i = 0, pTxQInfo = pDevCtx->TxQInfos; i < pDevCtx->ulTxQInfosSize;
        i++, pTxQInfo++ )
    {
        if( pTxQId->ulQueueIndex == pTxQInfo->ulDmaIndex )
        {
            UINT32 ulPort = PORTID_TO_PORT(pTxQId->ulPortId);
            UINT32 ulIdx = SAR_TX_DMA_BASE_CHAN+pTxQInfo->ulDmaIndex;
            PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
            volatile DmaStateRam *pStRam = pGi->pDmaRegs->stram.s;
            UINT32 ulPtmPriority = 0;

            pTxQInfo->pDma->cfg = DMA_BURST_HALT;
            pStRam[ulIdx].baseDescPtr = 0;
            pStRam[ulIdx].state_data = 0;
            pStRam[ulIdx].desc_len_status = 0;
            pStRam[ulIdx].desc_base_bufptr = 0;

            if( pTxQInfo->pMemBuf )
            {
                kfree(pTxQInfo->pMemBuf);
                pTxQInfo->pMemBuf = NULL;
            }

            if (pDevCtx->Addr.ulTrafficType == TRAFFIC_TYPE_PTM)
               ulPtmPriority = (pTxQInfo->ulPtmPriority == PTM_PRI_HIGH)? 1 : 0;
            pDevCtx->pTxPriorities[ulPtmPriority][ulPort][pTxQInfo->ulSubPriority] = NULL;

            /* Shift remaining array elements down by one element. */
            memmove(pTxQInfo, pTxQInfo + 1, (pDevCtx->ulTxQInfosSize - i - 1) *
                sizeof(TXQINFO));
            pDevCtx->ulTxQInfosSize--;

            for (ulPtmPriority = 0; ulPtmPriority < MAX_PTM_PRIORITIES; ulPtmPriority++)
               for (ulPort = 0; ulPort < MAX_PHY_PORTS; ulPort++)
                  for (j = 0; j < MAX_SUB_PRIORITIES; j++)
                     if (pDevCtx->pTxPriorities[ulPtmPriority][ulPort][j] > pTxQInfo)
                        pDevCtx->pTxPriorities[ulPtmPriority][ulPort][j]--;
                     
            /* Count the total number of transmit queues used across all device
             * interfaces.
             */
            for( j = 0, ulTxQs = 0; j < MAX_DEV_CTXS; j++ )
                if( pGi->pDevCtxs[j] )
                    ulTxQs += pGi->pDevCtxs[j]->ulTxQInfosSize;
            ulTxQs = pGi->ulNumTxQs;

            break;
        }
    }

    return( nRet );
} /* DoUnsetTxQueue */


/***************************************************************************
 * Function Name: DoSendCellReq
 * Description  : Processes an XTMRT_CMD_SEND_CELL command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoSendCellReq( PBCMXTMRT_DEV_CONTEXT pDevCtx, PXTMRT_CELL pC )
{
    int nRet = 0;

    if( pDevCtx->ulLinkState == LINK_UP )
    {
        struct sk_buff *skb = dev_alloc_skb(CELL_PAYLOAD_SIZE);

        if( skb )
        {
            UINT32 i;
            UINT32 ulPort = pC->ConnAddr.u.Conn.ulPortMask ;
            UINT32 ulPtmPriority = 0;

            /* A network device instance can potentially have transmit queues
             * on different ports. Find a transmit queue for the port specified
             * in the cell structure.  The cell structure should only specify
             * one port.
             */
            for( i = 0; i < MAX_SUB_PRIORITIES; i++ )
            {
                if( pDevCtx->pTxPriorities[ulPtmPriority][ulPort][i] )
                {
                    skb->mark = i;
                    break;
                }
            }

            skb->dev = pDevCtx->pDev;
            __skb_put(skb, CELL_PAYLOAD_SIZE);
            memcpy(skb->data, pC->ucData, CELL_PAYLOAD_SIZE);

            switch( pC->ucCircuitType )
            {
            case CTYPE_OAM_F5_SEGMENT:
                skb->protocol = FSTAT_CT_OAM_F5_SEG;
                break;

            case CTYPE_OAM_F5_END_TO_END:
                skb->protocol = FSTAT_CT_OAM_F5_E2E;
                break;

            case CTYPE_OAM_F4_SEGMENT:
                skb->protocol = FSTAT_CT_OAM_F4_SEG;
                break;

            case CTYPE_OAM_F4_END_TO_END:
                skb->protocol = FSTAT_CT_OAM_F4_E2E;
                break;

            case CTYPE_ASM_P0:
                skb->protocol = FSTAT_CT_ASM_P0;
                break;

            case CTYPE_ASM_P1:
                skb->protocol = FSTAT_CT_ASM_P1;
                break;

            case CTYPE_ASM_P2:
                skb->protocol = FSTAT_CT_ASM_P2;
                break;

            case CTYPE_ASM_P3:
                skb->protocol = FSTAT_CT_ASM_P3;
                break;
            }

            skb->protocol |= SKB_PROTO_ATM_CELL;

            bcmxtmrt_xmit( SKBUFF_2_PNBUFF(skb), pDevCtx->pDev );
        }
        else
            nRet = -ENOMEM;
    }
    else
        nRet = -EPERM;

    return( nRet );
} /* DoSendCellReq */


/***************************************************************************
 * Function Name: DoDeleteDeviceReq
 * Description  : Processes an XTMRT_CMD_DELETE_DEVICE command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoDeleteDeviceReq( PBCMXTMRT_DEV_CONTEXT pDevCtx )
{
    int nRet = -EPERM;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 i;

    for( i = 0; i < MAX_DEV_CTXS; i++ )
        if( pGi->pDevCtxs[i] == pDevCtx )
        {
            pGi->pDevCtxs[i] = NULL;

            kerSysReleaseMacAddress( pDevCtx->pDev->dev_addr );

            unregister_netdev( pDevCtx->pDev );
            free_netdev( pDevCtx->pDev );

            nRet = 0;
            break;
        }

    for( i = 0; i < MAX_MATCH_IDS; i++ )
        if( pGi->pDevCtxsByMatchId[i] == pDevCtx )
            pGi->pDevCtxsByMatchId[i] = NULL;

    return( nRet );
} /* DoDeleteDeviceReq */


/***************************************************************************
 * Function Name: DoGetNetDevTxChannel
 * Description  : Processes an XTMRT_CMD_GET_NETDEV_TXCHANNEL command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoGetNetDevTxChannel( PXTMRT_NETDEV_TXCHANNEL pParm )
{
    int nRet = 0;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    PBCMXTMRT_DEV_CONTEXT pDevCtx;
    PTXQINFO pTqi;
    UINT32 i, j;

    for( i = 0; i < MAX_DEV_CTXS; i++ )
    {
        pDevCtx = pGi->pDevCtxs[i];
        if ( pDevCtx != (PBCMXTMRT_DEV_CONTEXT) NULL )
        {
            if ( pDevCtx->ulOpenState == XTMRT_DEV_OPENED )
            {
                for ( j=0, pTqi=pDevCtx->TxQInfos;
                      j<pDevCtx->ulTxQInfosSize; j++, pTqi++)
                {
                    if ( pTqi->ulDmaIndex == pParm->txChannel )
                    {
                        pParm->pDev = (void*)pDevCtx->pDev;
                        return nRet;
                    }
                }
            }
        }
    }

    return -EEXIST;
} /* DoGetNetDevTxChannel */


/***************************************************************************
 * Function Name: bcmxtmrt_add_proc_files
 * Description  : Adds proc file system directories and entries.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_add_proc_files( void )
{
    proc_mkdir ("driver/xtm", NULL);
    create_proc_read_entry("driver/xtm/txdmainfo", 0, NULL, ProcDmaTxInfo, 0);

    create_proc_read_entry("driver/xtm/rxbondctrs", 0, NULL, ProcRxBondCtrs, 0);

    create_proc_read_entry("driver/xtm/txbondinfo", 0, NULL, ProcTxBondInfo, 0);
    return(0);
} /* bcmxtmrt_add_proc_files */


/***************************************************************************
 * Function Name: bcmxtmrt_del_proc_files
 * Description  : Deletes proc file system directories and entries.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_del_proc_files( void )
{
    remove_proc_entry("driver/xtm/txbondinfo", NULL);
    remove_proc_entry("driver/xtm/rxbondctrs", NULL);
    remove_proc_entry("driver/xtm/txdmainfo", NULL);
    remove_proc_entry("driver/xtm", NULL);

    return(0);
} /* bcmxtmrt_del_proc_files */


/***************************************************************************
 * Function Name: ProcDmaTxInfo
 * Description  : Displays information about transmit DMA channels for all
 *                network interfaces.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int ProcDmaTxInfo(char *page, char **start, off_t off, int cnt, 
    int *eof, void *data)
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    PBCMXTMRT_DEV_CONTEXT pDevCtx;
    PTXQINFO pTqi;
    UINT32 i, j;
    int sz = 0;

    for( i = 0; i < MAX_DEV_CTXS; i++ )
    {
        pDevCtx = pGi->pDevCtxs[i];
        if ( pDevCtx != (PBCMXTMRT_DEV_CONTEXT) NULL )
        {
            for( j = 0, pTqi = pDevCtx->TxQInfos;
                j < pDevCtx->ulTxQInfosSize; j++, pTqi++ )
            {
                sz += sprintf(page + sz, "dev: %s, tx_chan_size: %lu, tx_chan"
                    "_filled: %lu\n", pDevCtx->pDev->name, pTqi->ulQueueSize,
                    pTqi->ulNumTxBufsQdOne);
            }
        }
    }

    sz += sprintf(page + sz, "\next_buf_size: %lu, reserve_buf_size: %lu, tx_"
        "total_filled: %lu\n\n", pGi->ulNumExtBufs, pGi->ulNumExtBufsRsrvd,
        pGi->ulNumTxBufsQdAll);

    sz += sprintf(page + sz, "queue_condition: %lu %lu %lu, drop_condition: "
        "%lu %lu %lu\n\n", pGi->ulDbgQ1, pGi->ulDbgQ2, pGi->ulDbgQ3,
        pGi->ulDbgD1, pGi->ulDbgD2, pGi->ulDbgD3);

    *eof = 1;
    return( sz );
} /* ProcDmaTxInfo */


/***************************************************************************
 * MACRO to call driver initialization and cleanup functions.
 ***************************************************************************/
module_init(bcmxtmrt_init);
module_exit(bcmxtmrt_cleanup);
MODULE_LICENSE("Proprietary");

EXPORT_SYMBOL(bcmxtmrt_request);

