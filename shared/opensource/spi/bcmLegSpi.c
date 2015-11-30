#ifdef _CFE_ 
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

/* if SPI is defined then the legacy SPI controller is available, otherwise do not compile this code */
#ifdef SPI

#include "bcmSpiRes.h"
#include "bcmSpi.h"

int BcmLegSpiRead(unsigned char * msg_buf, int prependcnt, int nbytes, int devId, int freqHz);
int BcmLegSpiWrite(unsigned char * msg_buf, int nbytes, int devId, int freqHz);

#ifndef _CFE_
//#define LEG_SPI_USE_INTERRUPTS   /* define this to use interrupts instead of polling */
static struct bcmspi BcmLegSpi = { SPIN_LOCK_UNLOCKED,
                                   "bcmLegSpiDev",
                                 };
#endif

/* following are the frequency tables for the SPI controllers 
   they are ordered by frequency in descending order with column 
   2 represetning the register value */
#if defined(CONFIG_BCM93380)
#define LEG_SPI_FREQ_TABLE_SIZE  8
int legSpiClockFreq[LEG_SPI_FREQ_TABLE_SIZE][2] = { 
            { 25000000, 7},
            { 20000000, 0},
            { 12500000, 6},
            {  6250000, 5},
            {  3125000, 4},
            {  1563000, 3},
            {   781000, 2},
            {   391000, 1} };

#else
#define LEG_SPI_FREQ_TABLE_SIZE  7
int legSpiClockFreq[LEG_SPI_FREQ_TABLE_SIZE][2] = { 
            { 20000000, 0},
            { 12500000, 6},
            {  6250000, 5},
            {  3125000, 4},
            {  1563000, 3},
            {   781000, 2},
            {   391000, 1} };
#endif

static int legSpiRead( unsigned char *pRxBuf, int prependcnt, int nbytes, int devId )
{
    int i;

    SPI->spiMsgCtl = (HALF_DUPLEX_R << SPI_MSG_TYPE_SHIFT) | (nbytes << SPI_BYTE_CNT_SHIFT);

    for (i = 0; i < prependcnt; i++)
    {
        SPI->spiMsgData[i] = pRxBuf[i];
    }

    SPI->spiCmd = (SPI_CMD_START_IMMEDIATE << SPI_CMD_COMMAND_SHIFT | 
                   devId << SPI_CMD_DEVICE_ID_SHIFT | 
                   prependcnt << SPI_CMD_PREPEND_BYTE_CNT_SHIFT |
                   0 << SPI_CMD_ONE_BYTE_SHIFT);    
 
    return SPI_STATUS_OK;
    
}

static int legSpiWriteFull( unsigned char *pTxBuf, int nbytes, int devId, int opcode )
{
    int            i;

    if ( opcode == BCM_SPI_FULL )
    {
        SPI->spiMsgCtl = (FULL_DUPLEX_RW << SPI_MSG_TYPE_SHIFT) | (nbytes << SPI_BYTE_CNT_SHIFT);
    }
    else
    {
        SPI->spiMsgCtl = (HALF_DUPLEX_W << SPI_MSG_TYPE_SHIFT) | (nbytes << SPI_BYTE_CNT_SHIFT);
    }
    
    for (i = 0; i < nbytes; i++)
    {
        SPI->spiMsgData[i] = pTxBuf[i];
    }

    SPI->spiCmd = (SPI_CMD_START_IMMEDIATE << SPI_CMD_COMMAND_SHIFT | 
                  devId << SPI_CMD_DEVICE_ID_SHIFT | 
                  0 << SPI_CMD_PREPEND_BYTE_CNT_SHIFT |
                  0 << SPI_CMD_ONE_BYTE_SHIFT);    
 
    return SPI_STATUS_OK;
    
}


static int legSpiTransEnd( unsigned char *rxBuf, int nbytes )
{
    int i;
    if ( NULL != rxBuf )
    {        
        for (i = 0; i < nbytes; i++)
        {
            rxBuf[i] = SPI->spiRxDataFifo[i];
        }
    }

    return SPI_STATUS_OK;
    
}

static int legSpiTransPoll(void)
{
    while ( 1 )
    {
        if ( SPI->spiIntStatus & SPI_INTR_CMD_DONE )
        {
            break;
        }
    }

    return SPI_STATUS_OK;
}

static void legSpiClearIntStatus(void)
{
    SPI->spiIntStatus = SPI_INTR_CLEAR_ALL; 
}

#ifdef LEG_SPI_USE_INTERRUPTS
static void legSpiEnableInt(bool bEnable)
{
    if ( bEnable )
    {
        SPI->spiIntMask   = SPI_INTR_CMD_DONE;
    }
    else
    {
        SPI->spiIntMask   = 0;
    }
}
#endif

#ifndef _CFE_
static int legSpiSetClock( int clockHz )
{
    int  i;
    int  clock = -1;
  
    for( i = 0; i < LEG_SPI_FREQ_TABLE_SIZE; i++ )
    {
        /* look for the closest frequency that is less than the frequency passed in */
        if ( legSpiClockFreq[i][0] <= clockHz )
        {
            clock = legSpiClockFreq[i][1];
            break;
        }
    }
    /* if no clock was found set to default */
    if ( -1 == clock )
    {
        clock = LEG_SPI_CLOCK_DEF;
    }
    SPI->spiClkCfg = (SPI->spiClkCfg & ~SPI_CLK_MASK) | clock;

    return SPI_STATUS_OK;
}
#endif

/* these interfaces are availble for the CFE and spi flash driver only
   all modules must use the linux kernel framework 
   if this is called by a module and interrupts are being used there will
   be a problem */
int BcmLegSpiRead( unsigned char *msg_buf, int prependcnt, int nbytes, int devId, int freqHz )
{
#ifndef _CFE_
    struct bcmspi  *pBcmSpi = &BcmLegSpi;

    if ( pBcmSpi->irq )
    {
        printk("BcmLegSpiRead error - SPI Interrupts are enabled\n");
        return( SPI_STATUS_ERR );
    }

    spin_lock(&pBcmSpi->lock);
    legSpiSetClock(freqHz);
#endif
    legSpiClearIntStatus();
    legSpiRead(msg_buf, prependcnt, nbytes, devId);
    legSpiTransPoll();
    legSpiTransEnd(msg_buf, nbytes);
    legSpiClearIntStatus();
#ifndef _CFE_
    spin_unlock(&pBcmSpi->lock);
#endif

    return( SPI_STATUS_OK );
}

int BcmLegSpiWrite( unsigned char *msg_buf, int nbytes, int devId, int freqHz )
{
#ifndef _CFE_
    struct bcmspi  *pBcmSpi = &BcmLegSpi;

    if ( pBcmSpi->irq )
    {
        printk("BcmLegSpiWrite error - SPI Interrupts are enabled\n");
        return( SPI_STATUS_ERR );
    }

    spin_lock(&pBcmSpi->lock);
    legSpiSetClock(freqHz);
#endif
    legSpiClearIntStatus();
    legSpiWriteFull(msg_buf, nbytes, devId, BCM_SPI_WRITE);
    legSpiTransPoll();
    legSpiTransEnd(msg_buf, nbytes);
    legSpiClearIntStatus();
#ifndef _CFE_
    spin_unlock(&pBcmSpi->lock);
#endif

    return( SPI_STATUS_OK );
}


#ifndef _CFE_
static void legSpiNextMessage(struct bcmspi *pBcmSpi);

static void legSpiMsgDone(struct bcmspi *pBcmSpi, struct spi_message *msg, int status)
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
        legSpiNextMessage(pBcmSpi);
    }
}

#ifdef LEG_SPI_USE_INTERRUPTS
static void legSpiIntXfer(struct bcmspi *pBcmSpi, struct spi_message *msg)
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

    legSpiSetClock(xfer->speed_hz);

    legSpiClearIntStatus();
    legSpiEnableInt(TRUE);
    if ( BCM_SPI_READ == opCode )
    {
        legSpiRead(pTxBuf, prependCnt, length, msg->spi->chip_select);
    }
    else
    {
        legSpiWriteFull(pTxBuf, length, msg->spi->chip_select, opCode);
    }

    return;
    
}
#endif

static void legSpiPollXfer(struct bcmspi *pBcmSpi, struct spi_message *msg)
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
        
        legSpiSetClock(xfer->speed_hz);
        
        legSpiClearIntStatus();
        if ( BCM_SPI_READ == opCode )
        {
            legSpiRead(pTxBuf, prependCnt, length, msg->spi->chip_select);
        }
        else
        {
            legSpiWriteFull(pTxBuf, length, msg->spi->chip_select, opCode);
        }

        legSpiTransPoll();
        legSpiTransEnd(pRxBuf, length);
        legSpiClearIntStatus();

        if (xfer->delay_usecs)
        {
            udelay(xfer->delay_usecs);
        }

        msg->actual_length += length;
    }

    legSpiMsgDone(pBcmSpi, msg, SPI_STATUS_OK);
    
}


static void legSpiNextXfer(struct bcmspi *pBcmSpi, struct spi_message *msg)
{
#ifdef LEG_SPI_USE_INTERRUPTS
    if (pBcmSpi->irq)
        legSpiIntXfer(pBcmSpi, msg);
    else
#endif        
        legSpiPollXfer(pBcmSpi, msg);

}


static void legSpiNextMessage(struct bcmspi *pBcmSpi)
{
    struct spi_message *msg;

    BUG_ON(pBcmSpi->curTrans);

    msg = list_entry(pBcmSpi->queue.next, struct spi_message, queue);

    /* there will always be one transfer in a given message */
    legSpiNextXfer(pBcmSpi, msg);
    
}


static int legSpiSetup(struct spi_device *spi)
{
    struct bcmspi *pBcmSpi;

    pBcmSpi = spi_master_get_devdata(spi->master);

    if (pBcmSpi->stopping)
        return -ESHUTDOWN;

    /* there is nothing to setup */

    return 0;
}


int legSpiTransfer(struct spi_device *spi, struct spi_message *msg)
{
    struct bcmspi         *pBcmSpi = &BcmLegSpi;
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

        if ( xfer->len > (sizeof(SPI->spiMsgData) & ~0x3) )
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

#ifdef LEG_SPI_USE_INTERRUPTS
    /* disable interrupts for the SPI controller
       using spin_lock_irqsave would disable all interrupts */
    if ( pBcmSpi->irq )
        legSpiEnableInt(FALSE);
#endif
    spin_lock(&pBcmSpi->lock);

    list_add_tail(&msg->queue, &pBcmSpi->queue);
    if (NULL == pBcmSpi->curTrans)
    {
        legSpiNextMessage(pBcmSpi);
    }

    spin_unlock(&pBcmSpi->lock);
#ifdef LEG_SPI_USE_INTERRUPTS
    if ( pBcmSpi->irq )
        legSpiEnableInt(TRUE);
#endif

    return 0;
}


#ifdef LEG_SPI_USE_INTERRUPTS
static irqreturn_t legSpiIntHandler(int irq, void *dev_id)
{
    struct bcmspi        *pBcmSpi = dev_id;
    struct spi_message  *msg;
    struct spi_transfer *xfer;

    if ( 0 == SPI->spiMaskIntStatus )
    {
        return ( IRQ_NONE );
    }

    legSpiClearIntStatus();
    legSpiEnableInt(FALSE);

    spin_lock(&pBcmSpi->lock);
    if ( NULL == pBcmSpi->curTrans )
    {
        spin_unlock(&pBcmSpi->lock);
        return IRQ_HANDLED;
    }

    xfer = pBcmSpi->curTrans;
    msg  = list_entry(pBcmSpi->queue.next, struct spi_message, queue);

    legSpiTransEnd(xfer->rx_buf, xfer->len);

    /* xfer can specify a delay before the next transfer is started 
       this is only used for polling mode */

    /* check to see if this is the last transfer in the message */
    if (msg->transfers.prev == &xfer->transfer_list)
    {    
        /* report completed message */
        legSpiMsgDone(pBcmSpi, msg, SPI_STATUS_OK);
    }
    else
    {
        /* Submit the next transfer */
        legSpiNextXfer(pBcmSpi, msg);
    }

    spin_unlock(&pBcmSpi->lock);

    return IRQ_HANDLED;

}

int __init legSpiIntrInit( void )
{
    int            ret = 0;
    struct bcmspi *pBcmSpi  = &BcmLegSpi;

    legSpiEnableInt(FALSE);
    ret = request_irq(INTERRUPT_ID_SPI, legSpiIntHandler, (IRQF_DISABLED | IRQF_SAMPLE_RANDOM | IRQF_SHARED), pBcmSpi->devName, pBcmSpi);

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
late_initcall(legSpiIntrInit);
#endif

static void legSpiCleanup(struct spi_device *spi)
{
    /* would free spi_controller memory here if any was allocated */

}

static int __init legSpiProbe(struct platform_device *pdev)
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
    master->setup          = legSpiSetup;
    master->transfer       = legSpiTransfer;
    master->cleanup        = legSpiCleanup;
    platform_set_drvdata(pdev, master);

    spi_master_set_devdata(master, (void *)&BcmLegSpi);
    pBcmSpi = spi_master_get_devdata(master);

    INIT_LIST_HEAD(&pBcmSpi->queue);

    pBcmSpi->pdev           = pdev;
    pBcmSpi->bus_num        = LEG_SPI_BUS_NUM;
    pBcmSpi->num_chipselect = 8;
    pBcmSpi->curTrans       = NULL;

    /* make sure irq is 0 here
       since this is used to identify when interrupts are enabled
       the IRQ is initialized in legSpiIntrInit */       
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


static int __exit legSpiRemove(struct platform_device *pdev)
{
    struct spi_master   *master  = platform_get_drvdata(pdev);
    struct bcmspi       *pBcmSpi = spi_master_get_devdata(master);
    struct spi_message  *msg;

    /* reset the hardware and block queue progress */
#ifdef LEG_SPI_USE_INTERRUPTS
    legSpiEnableInt(FALSE);
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

#ifdef LEG_SPI_USE_INTERRUPTS
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
static int legSpiSuspend(struct platform_device *pdev, pm_message_t mesg)
{
    struct spi_master *master = platform_get_drvdata(pdev);
    struct bcmspi     *pBcmSpi     = spi_master_get_devdata(master);

    return 0;
}

static int legSpiResume(struct platform_device *pdev)
{
    struct spi_master *master = platform_get_drvdata(pdev);
    struct bcmspi     *pBcmSpi     = spi_master_get_devdata(master);

    return 0;
}
#else
#define   legSpiSuspend   NULL
#define   legSpiResume    NULL
#endif


static struct platform_device bcm_legacyspi_device = {
    .name        = "bcmleg_spi",
    .id          = LEG_SPI_BUS_NUM,
};

static struct platform_driver bcm_legspi_driver = {
    .driver      =
    {
        .name    = "bcmleg_spi",
        .owner   = THIS_MODULE,
    },
    .suspend     = legSpiSuspend,
    .resume      = legSpiResume,
    .remove      = __exit_p(legSpiRemove),
};


int __init legSpiModInit( void )
{
    platform_device_register(&bcm_legacyspi_device);
    return platform_driver_probe(&bcm_legspi_driver, legSpiProbe);

}
subsys_initcall(legSpiModInit);
#endif

#endif /* SPI */

