#ifndef __BCM_SPI_H__
#define __BCM_SPI_H__

#ifndef _CFE_ 
#include <linux/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>

struct bcmspi
{
    spinlock_t               lock;
    char *                   devName;
    int                      irq;
    unsigned                 bus_num;
    unsigned                 num_chipselect;  
    u8                       stopping;
    struct list_head         queue;
    struct platform_device  *pdev;
    struct spi_transfer     *curTrans;
};
#endif

#define BCM_SPI_READ  0
#define BCM_SPI_WRITE 1
#define BCM_SPI_FULL  2

#endif /* __BCM_SPI_H__ */

