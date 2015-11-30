#ifdef _CFE_ 
    spi_flash_clock = 25000000;
#include "lib_types.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "bcm_map.h"  
#define  printk  printf
#else
#include <linux/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>

#include <bcm_map_part.h>
#include <bcm_intr.h>
#endif

/* if HS_SPI is defined then the HS SPI controller is available, otherwise do not compile this code */

#ifdef HS_SPI

#include "bcmSpiRes.h"
#include "bcmSpi.h"

int BcmHsSpiRead(unsigned char * msg_buf, int prependcnt, int nbytes, int devId, int freqHz);
int BcmHsSpiWrite(unsigned char * msg_buf, int nbytes, int devId, int freqHz);

#ifndef _CFE_
//#define HS_SPI_USE_INTERRUPTS   /* define this to use interrupts instead of polling */
static struct bcmspi BcmHsSpi = { SPIN_LOCK_UNLOCKED,
                                  "bcmHsSpiDev",
                                };
#else
#define udelay(X)                        \
    do { { int i; for( i = 0; i < (X) * 500; i++ ) ; } } while(0)
#endif

static int hsSpiRead( unsigned char *pRxBuf, int prependcnt, int nbytes, int devId )
{
    uint16 msgCtrl;

    HS_SPI_PROFILES[0].mode_ctrl = prependcnt<<HS_SPI_PREPENDBYTE_CNT | 0<<HS_SPI_MODE_ONE_WIRE | 
                                   0<<HS_SPI_MULTIDATA_WR_SIZE | 0<<HS_SPI_MULTIDATA_RD_SIZE | 2<<HS_SPI_MULTIDATA_WR_STRT | 
                                   2<<HS_SPI_MULTIDATA_RD_STRT | 0xff<<HS_SPI_FILLBYTE;

    msgCtrl = (HS_SPI_OP_READ<<HS_SPI_OP_CODE) | nbytes;
    memcpy((char *)HS_SPI_FIFO0, (void *)(&msgCtrl), 2);
   
    if ( 0 != prependcnt )
    {
        memcpy((char *)(HS_SPI_FIFO0+2), (char *)pRxBuf, prependcnt);
    }

    HS_SPI_PINGPONG0->command = devId<<HS_SPI_SS_NUM | 0<<HS_SPI_PROFILE_NUM | 0<<HS_SPI_TRIGGER_NUM | 
                                HS_SPI_COMMAND_START_NOW<<HS_SPI_COMMAND_VALUE;

    return SPI_STATUS_OK;

}

static int hsSpiWriteFull( unsigned char *pTxBuf, int nbytes, int devId, int opcode )
{
    uint16 msgCtrl;

    HS_SPI_PROFILES[0].mode_ctrl = 0<<HS_SPI_PREPENDBYTE_CNT | 0<<HS_SPI_MODE_ONE_WIRE | 
                                   0<<HS_SPI_MULTIDATA_WR_SIZE | 0<<HS_SPI_MULTIDATA_RD_SIZE | 2<<HS_SPI_MULTIDATA_WR_STRT | 
                                   2<<HS_SPI_MULTIDATA_RD_STRT | 0xff<<HS_SPI_FILLBYTE;

    if (BCM_SPI_FULL == opcode)
    {
        msgCtrl  = (HS_SPI_OP_READ_WRITE<<HS_SPI_OP_CODE) | nbytes;
    }
    else
    {
        msgCtrl  = (HS_SPI_OP_WRITE<<HS_SPI_OP_CODE) | nbytes;
    }
    memcpy((char *)HS_SPI_FIFO0, (void *)(&msgCtrl), 2);
    memcpy((char *)(HS_SPI_FIFO0+2), (void *)pTxBuf, nbytes);

    HS_SPI_PINGPONG0->command = devId<<HS_SPI_SS_NUM | 0<<HS_SPI_PROFILE_NUM | 0<<HS_SPI_TRIGGER_NUM | 
                                HS_SPI_COMMAND_START_NOW<<HS_SPI_COMMAND_VALUE;

    return SPI_STATUS_OK;

}

static int hsSpiTransEnd( unsigned char *rxBuf, int nbytes )
{
    if ( NULL != rxBuf )
    {
        memcpy((char *)rxBuf, (void *)HS_SPI_FIFO0, nbytes);
    }
   
    return SPI_STATUS_OK;
    
}

static int hsSpiTransPoll(void)
{
    unsigned int wait;

    for (wait = (100*1000); wait>0; wait--)
    {
        if (!(HS_SPI_PINGPONG0->status & 1<<HS_SPI_SOURCE_BUSY ))
        {
            break;
        }
        udelay(1);
    }

    if (wait == 0)
    {
        return SPI_STATUS_ERR;
    }
    
    return SPI_STATUS_OK;
}


static void hsSpiClearIntStatus(void)
{
    HS_SPI->hs_spiIntStatus = HS_SPI_INTR_CLEAR_ALL; 
}

#ifdef HS_SPI_USE_INTERRUPTS
static void hsSpiEnableInt(bool bEnable)
{
    if ( bEnable )
    {
        HS_SPI->hs_spiIntMask   = HS_SPI_IRQ_PING0_CMD_DONE;
    }
    else
    {
        HS_SPI->hs_spiIntMask   = 0;
    }
}
#endif

#ifndef _CFE_
static int hsSpiSetClock( int clockHz, int profile )
{
#if 1
    int clock;

    clock = HS_SPI_PLL_FREQ/clockHz;
    if (HS_SPI_PLL_FREQ%HS_SPI_CLOCK_DEF)
        clock++;

    clock = 2048/clock;
    if (2048%(clock))
        clock++;

    //clock = 0x80;

    HS_SPI_PROFILES[profile].clk_ctrl = 1<<HS_SPI_ACCUM_RST_ON_LOOP | 0<<HS_SPI_SPI_CLK_2X_SEL | clock<<HS_SPI_FREQ_CTRL_WORD;
#endif
    return SPI_STATUS_OK;
}
#endif

/* these interfaces are availble for the CFE and spi flash driver only
   all modules must use the linux kernel framework 
   if this is called by a module and interrupts are being used there will
   be a problem */
int BcmHsSpiRead( unsigned char *msg_buf, int prependcnt, int nbytes, int devId, int freqHz )
{
#ifndef _CFE_
    struct bcmspi  *pBcmSpi = &BcmHsSpi;

    if ( pBcmSpi->irq )
    {
        printk("BcmHsSpiRead error - Interrupts are enabled\n");
        return( SPI_STATUS_ERR );
    }

    spin_lock(&pBcmSpi->lock);
    hsSpiSetClock(freqHz, 0);
#endif
    hsSpiClearIntStatus();
    hsSpiRead(msg_buf, prependcnt, nbytes, devId);
    hsSpiTransPoll();
    hsSpiTransEnd(msg_buf, nbytes);
    hsSpiClearIntStatus();
#ifndef _CFE_
    spin_unlock(&pBcmSpi->lock);
#endif

    return( SPI_STATUS_OK );
}

int BcmHsSpiWrite( unsigned char *msg_buf, int nbytes, int devId, int freqHz )
{
#ifndef _CFE_
    struct bcmspi  *pBcmSpi = &BcmHsSpi;

    if ( pBcmSpi->irq )
    {
        printk("BcmHsSpiWrite error - Interrupts are enabled\n");
        return( SPI_STATUS_ERR );
    }

    spin_lock(&pBcmSpi->lock);
    hsSpiSetClock(freqHz, 0);
#endif
    hsSpiClearIntStatus();
    hsSpiWriteFull(msg_buf, nbytes, devId, BCM_SPI_WRITE);
    hsSpiTransPoll();
    hsSpiTransEnd(msg_buf, nbytes);
    hsSpiClearIntStatus();
#ifndef _CFE_
    spin_unlock(&pBcmSpi->lock);
#endif

    return( SPI_STATUS_OK );
}


#ifndef _CFE_
static void hsSpiNextMessage(struct bcmspi *pBcmSpi);

static void hsSpiMsgDone(struct bcmspi *pBcmSpi, struct spi_message *msg, int status)
{
    list_del(&msg->queue);
    msg->status = status;

    spin_unlock(&pBcmSpi->lock);
    msg->complete(msg->context);
    spin_lock(&pBcmSpi->lock);

    pBcmSpi->curTrans = NULL;

    /* continue if needed */
    if (list_empty(&pBcmSpi->queue) || pBcmSpi->stopping)
    {
        // disable controler ...
    }
    else
    {
        hsSpiNextMessage(pBcmSpi);
    }
}

#ifdef HS_SPI_USE_INTERRUPTS
static void hsSpiIntXfer(struct bcmspi *pBcmSpi, struct spi_message *msg)
{
    struct spi_transfer *xfer;
    struct spi_transfer *nextXfer;
    int                  length;
    int                  prependCnt;
    char                *pTxBuf;
    char                *pRxBuf;
    int                  opCode;
   
    xfer = pBcmSpi->curTrans;
    if ( NULL == xfer) 
    {
        xfer = list_entry(msg->transfers.next, struct spi_transfer, transfer_list);
    }
    else
    {
        xfer = list_entry(xfer->transfer_list.next, struct spi_transfer, transfer_list);
    }
    pBcmSpi->curTrans = xfer;

    length     = xfer->len;
    prependCnt = 0;
    pRxBuf     = xfer->rx_buf;
    pTxBuf     = (unsigned char *)xfer->tx_buf;

    if ( (NULL != pRxBuf) && (NULL != pTxBuf) )
    {
        opCode = BCM_SPI_FULL;
    }
    else if ( NULL != pRxBuf )
    {
        opCode = BCM_SPI_READ;
    }
    else
    {
        opCode = BCM_SPI_WRITE;
    }

    if ( msg->state )
    {
        /* this controller does not support keeping the chip select active for all transfers
           non NULL state indicates that we need to combine the transfers */
        nextXfer          = list_entry(xfer->transfer_list.next, struct spi_transfer, transfer_list);
        prependCnt        = length;
        length            = nextXfer->len;
        pRxBuf            = nextXfer->rx_buf;
        opCode            = BCM_SPI_READ;
        pBcmSpi->curTrans = nextXfer;            
    }

    hsSpiSetClock(xfer->speed_hz, 0);

    hsSpiClearIntStatus();
    hsSpiEnableInt(TRUE);
    if ( BCM_SPI_READ == opCode )
    {
        hsSpiRead(pTxBuf, prependCnt, length, msg->spi->chip_select);
    }
    else
    {
        hsSpiWriteFull(pTxBuf, length, msg->spi->chip_select, opCode);
    }

    return;
    
}
#endif

static void hsSpiPollXfer(struct bcmspi *pBcmSpi, struct spi_message *msg)
{
    struct spi_transfer *xfer;
    struct spi_transfer *nextXfer;
    int                  length;
    int                  prependCnt;
    char                *pTxBuf;
    char                *pRxBuf;
    int                  opCode;

    list_for_each_entry(xfer, &msg->transfers, transfer_list)
    {
        pBcmSpi->curTrans = xfer;
        length            = xfer->len;
        prependCnt        = 0;
        pRxBuf            = xfer->rx_buf;
        pTxBuf            = (unsigned char *)xfer->tx_buf;

        if ( (NULL != pRxBuf) && (NULL != pTxBuf) )
        {
            opCode = BCM_SPI_FULL;
        }
        else if ( NULL != pRxBuf )
        {
            opCode = BCM_SPI_READ;
        }
        else
        {
            opCode = BCM_SPI_WRITE;
        }

        if ( msg->state )
        {
            /* this controller does not support keeping the chip select active for all transfers
               non NULL state indicates that we need to combine the transfers */
            nextXfer   = list_entry(xfer->transfer_list.next, struct spi_transfer, transfer_list);
            prependCnt = length;
            length     = nextXfer->len;
            pRxBuf     = nextXfer->rx_buf;
            opCode     = BCM_SPI_READ;
            xfer       = nextXfer;            
        }
        
        hsSpiSetClock(xfer->speed_hz, 0);
        
        hsSpiClearIntStatus();
        if ( BCM_SPI_READ == opCode )
        {
            hsSpiRead(pTxBuf, prependCnt, length, msg->spi->chip_select);
        }
        else
        {
            hsSpiWriteFull(pTxBuf, length, msg->spi->chip_select, opCode);
        }

        hsSpiTransPoll();
        hsSpiTransEnd(pRxBuf, length);
        hsSpiClearIntStatus();

        if (xfer->delay_usecs)
        {
            udelay(xfer->delay_usecs);
        }

        msg->actual_length += length;
    }

    hsSpiMsgDone(pBcmSpi, msg, SPI_STATUS_OK);
    
}


static void hsSpiNextXfer(struct bcmspi *pBcmSpi, struct spi_message *msg)
{
#ifdef HS_SPI_USE_INTERRUPTS
    if (pBcmSpi->irq)
        hsSpiIntXfer(pBcmSpi, msg);
    else
#endif
        hsSpiPollXfer(pBcmSpi, msg);    
}

static void hsSpiNextMessage(struct bcmspi *pBcmSpi)
{
    struct spi_message *msg;

    BUG_ON(pBcmSpi->curTrans);

    msg = list_entry(pBcmSpi->queue.next, struct spi_message, queue);

    /* there will always be one transfer in a given message */
    hsSpiNextXfer(pBcmSpi, msg);
    
}


static int hsSpiSetup(struct spi_device *spi)
{
    struct bcmspi *pBcmSpi;

    pBcmSpi = spi_master_get_devdata(spi->master);

    if (pBcmSpi->stopping)
        return -ESHUTDOWN;

    /* there is nothing to setup */

    return 0;
}


static int hsSpiTransfer(struct spi_device *spi, struct spi_message *msg)
{
    struct bcmspi         *pBcmSpi = &BcmHsSpi;
    struct spi_transfer   *xfer;
    struct spi_transfer   *nextXfer;
    int                    xferCnt;
    int                    bCsChange;
    int                    xferLen;

    if (unlikely(list_empty(&msg->transfers)))
        return -EINVAL;

    if (pBcmSpi->stopping)
        return -ESHUTDOWN;  

    /* make sure a completion callback is set */
    if ( NULL == msg->complete )
    {
        return -EINVAL;        
    }

    xferCnt   = 0;
    bCsChange = 0;
    xferLen   = 0;
    list_for_each_entry(xfer, &msg->transfers, transfer_list)
    {
        /* check transfer parameters */
        if (!(xfer->tx_buf || xfer->rx_buf))
        {
            return -EINVAL;
        }

        /* check the clock setting - if it is 0 then set to max clock of the device */
        if ( 0 == xfer->speed_hz )
        {
            if ( 0 == spi->max_speed_hz )
            {
                return -EINVAL;
            }
            xfer->speed_hz = spi->max_speed_hz;
        }

        xferCnt++;
        xferLen += xfer->len;
        bCsChange |= xfer->cs_change;

        if ( xfer->len > HS_SPI_BUFFER_LEN )
        {
            return -EINVAL;
        }            
    }

    /* this controller does not support keeping the chip select active between
       transfers. If a message is detected with a write transfer followed by a 
       read transfer and cs_change is set to 0 then the two transfers need to be
       combined. The message state is used to indicate that the transfers 
       need to be combined */
    msg->state = NULL;
    if ( (2 == xferCnt) && (0 == bCsChange) )
    {
        xfer = list_entry(msg->transfers.next, struct spi_transfer, transfer_list);
        if ( (NULL != xfer->tx_buf) && (NULL == xfer->rx_buf))
        {
            nextXfer = list_entry(xfer->transfer_list.next, struct spi_transfer, transfer_list);;
            if ( (NULL == nextXfer->tx_buf) && (NULL != nextXfer->rx_buf))
            {
                msg->state = (void *)1;
            }
        }
    }

    msg->status        = -EINPROGRESS;
    msg->actual_length = 0;

#ifdef HS_SPI_USE_INTERRUPTS
    /* disable interrupts for the SPI controller
       using spin_lock_irqsave would disable all interrupts */
    if ( pBcmSpi->irq )
        hsSpiEnableInt(FALSE);
#endif
    spin_lock(&pBcmSpi->lock);

    list_add_tail(&msg->queue, &pBcmSpi->queue);
    if (NULL == pBcmSpi->curTrans)
    {
        hsSpiNextMessage(pBcmSpi);
    }

    spin_unlock(&pBcmSpi->lock);
#ifdef HS_SPI_USE_INTERRUPTS
    if ( pBcmSpi->irq )
        hsSpiEnableInt(TRUE);
#endif
    
    return 0;
}


#ifdef HS_SPI_USE_INTERRUPTS
static irqreturn_t hsSpiIntHandler(int irq, void *dev_id)
{
    struct bcmspi       *pBcmSpi = dev_id;
    struct spi_message  *msg;
    struct spi_transfer *xfer;

    if ( 0 == HS_SPI->hs_spiIntStatusMasked )
    {
        return ( IRQ_NONE );
    }

    hsSpiClearIntStatus();
    hsSpiEnableInt(FALSE);

    spin_lock(&pBcmSpi->lock);
    if ( NULL == pBcmSpi->curTrans )
    {
        spin_unlock(&pBcmSpi->lock);
        return IRQ_HANDLED;
    }

    xfer = pBcmSpi->curTrans;
    msg  = list_entry(pBcmSpi->queue.next, struct spi_message, queue);

    hsSpiTransEnd(xfer->rx_buf, xfer->len);

    /* xfer can specify a delay before the next transfer is started
       this delay would be processed here normally. However, delay in the 
       interrupt handler is bad so it is ignored. It is used for polling
       mode */

    /* check to see if this is the last transfer in the message */
    if (msg->transfers.prev == &xfer->transfer_list)
    {    
        /* report completed message */
        hsSpiMsgDone(pBcmSpi, msg, SPI_STATUS_OK);
    }
    else
    {
        /* Submit the next transfer */
        hsSpiNextXfer(pBcmSpi, msg);
    }

    spin_unlock(&pBcmSpi->lock);

    return IRQ_HANDLED;

}

int __init hsSpiIntrInit( void )
{
    int            ret     = 0;
    struct bcmspi *pBcmSpi = &BcmHsSpi;

    hsSpiEnableInt(FALSE);
    ret = request_irq(INTERRUPT_ID_SPI, hsSpiIntHandler, (IRQF_DISABLED | IRQF_SAMPLE_RANDOM | IRQF_SHARED), pBcmSpi->devName, pBcmSpi);

    spin_lock(&pBcmSpi->lock);
    pBcmSpi->irq = INTERRUPT_ID_SPI;
    spin_unlock(&pBcmSpi->lock);

    BcmHalInterruptEnable(pBcmSpi->irq);

    return( 0 );

}
/* we cannot initialize interrupts early
   The flash module is intialized before an interrupt handler can be installed
   and before the Linux framework can be used. This means it needs direct access
   to the controller initially. This conflicts with the interrupt handling so we
   need to wait for all modules to intialize */
late_initcall(hsSpiIntrInit);
#endif

static void hsSpiCleanup(struct spi_device *spi)
{
    /* would free spi_controller memory here if any was allocated */

}

static int __init hsSpiProbe(struct platform_device *pdev)
{
    int                ret;
    struct spi_master *master;
    struct bcmspi     *pBcmSpi;    

    ret = -ENOMEM;
    master = spi_alloc_master(&pdev->dev, 0);
    if (!master)
        goto out_free;

    master->bus_num        = pdev->id;
    master->num_chipselect = 8;
    master->setup          = hsSpiSetup;
    master->transfer       = hsSpiTransfer;
    master->cleanup        = hsSpiCleanup;
    platform_set_drvdata(pdev, master);

    spi_master_set_devdata(master, (void *)&BcmHsSpi);
    pBcmSpi = spi_master_get_devdata(master);

    INIT_LIST_HEAD(&pBcmSpi->queue);
    
    pBcmSpi->pdev           = pdev;
    pBcmSpi->bus_num        = HS_SPI_BUS_NUM;
    pBcmSpi->num_chipselect = 8;
    pBcmSpi->curTrans       = NULL;

    /* make sure irq is 0 here 
       since this is used to identify when interrupts are enabled
       the IRQ is initialized in hsSpiIntrInit */       
    pBcmSpi->irq            = 0;

    /* Initialize the hardware */

    /* register and we are done */
    ret = spi_register_master(master);
    if (ret)
        goto out_free;

    return 0;

out_free:  
    spi_master_put(master);  
    
    return ret;
}


static int __exit hsSpiRemove(struct platform_device *pdev)
{
    struct spi_master   *master  = platform_get_drvdata(pdev);
    struct bcmspi       *pBcmSpi = spi_master_get_devdata(master);
    struct spi_message  *msg;

    /* reset the hardware and block queue progress */
#ifdef HS_SPI_USE_INTERRUPTS    
    hsSpiEnableInt(FALSE);
#endif
    spin_lock(&pBcmSpi->lock);
    pBcmSpi->stopping = 1;

    /* HW shutdown */
    
    spin_unlock(&pBcmSpi->lock);

    /* Terminate remaining queued transfers */
    list_for_each_entry(msg, &pBcmSpi->queue, queue)
    {
        msg->status = -ESHUTDOWN;
        msg->complete(msg->context);
    } 

#ifdef HS_SPI_USE_INTERRUPTS
    if ( pBcmSpi->irq )
    {
        free_irq(pBcmSpi->irq, master);
    }
#endif
    spi_unregister_master(master);

    return 0;
}

//#ifdef CONFIG_PM
#if 0
static int hsSpiSuspend(struct platform_device *pdev, pm_message_t mesg)
{
    struct spi_master *master = platform_get_drvdata(pdev);
    struct bcmspi     *pBcmSpi     = spi_master_get_devdata(master);

    return 0;
}

static int hsSpiResume(struct platform_device *pdev)
{
    struct spi_master *master = platform_get_drvdata(pdev);
    struct bcmspi     *pBcmSpi     = spi_master_get_devdata(master);

    return 0;
}
#else
#define   hsSpiSuspend   NULL
#define   hsSpiResume    NULL
#endif

static struct platform_device bcm_hsspi_device = {
    .name        = "bcmhs_spi",
    .id          = HS_SPI_BUS_NUM,
};

static struct platform_driver bcm_hsspi_driver = {
    .driver      =
    {
        .name    = "bcmhs_spi",
        .owner   = THIS_MODULE,
    },
    .suspend    = hsSpiSuspend,
    .resume     = hsSpiResume,
    .remove     = __exit_p(hsSpiRemove),
};

int __init hsSpiModInit( void )
{
    platform_device_register(&bcm_hsspi_device);
    return platform_driver_probe(&bcm_hsspi_driver, hsSpiProbe);

}
subsys_initcall(hsSpiModInit);
#endif

#endif /* HS_SPI */

