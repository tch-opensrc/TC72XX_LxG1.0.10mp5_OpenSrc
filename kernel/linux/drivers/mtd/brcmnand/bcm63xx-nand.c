/*
 *
 *  drivers/mtd/brcmnand/bcm63xx-nand.c
 *
    Copyright (c) 2005-2006 Broadcom Corporation                 
    
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License version 2 as
 published by the Free Software Foundation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

    File: bcm63xx-nand.c (used to be bcm7xxx-nand.c)

    Description: 
    This is a device driver for the Broadcom NAND flash for bcm963xx boards.
when    who what
-----   --- ----
051011  tht codings derived from OneNand generic.c implementation.
 */

#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/mtd/mtd.h>

#include <linux/mtd/partitions.h>


#include <asm/io.h>
//#include <asm/mach/flash.h>

#include <linux/mtd/brcmnand.h>
#include <bcm_map_part.h>
#include <board.h>
#include "brcmnand_priv.h"

#define PRINTK(...)
//#define PRINTK printk

#define DRIVER_NAME "bcm63xx-nand"
#define DRIVER_INFO "Broadcom DSL NAND controller"

//#ifdef CONFIG_MTD_PARTITIONS
//static const char *part_probes[] = { "cmdlinepart", NULL,  };
//#endif

/* Size and offset are variable, depending on the size of the chip, but 
 * cfe_kernel always starts at 1FC0_0000 and is 4MB size.
 * Here is the layout of the NAND flash
 *
 *  Physical offset         Size            partition    Owner/comment
 *  <eof-4MB>   <eof>       4MB             BBT          BBT if fsize > 512MB
 *  2000_0000   <eof - 1MB> FlSize-256MB    data         Linux File System --
 *                                                       if fsize > 512MB
 *  1ff0_0000   1fff_ffff   1MB=8x128k      BBT/or res   Linux RW --
 *                                                       if fsize <=512MB
 *  1fe0_0000   1fef_ffff   1MB=8x128k      nvm          CFE, RO for Linux
 *  1fc0_0000   1fdf_ffff   2MB             CFE          CFE
 *
 *  [Optional] 1MB splash screen image partition carved out from the partition
 *             immediately preceding the CFE partition when bcmsplash=1
 *    
 *  1f80_0000   1fbf_ffff   4MB             Linux Kernel CFE
 *  start flash 1f7f_ffff   flashSize-8MB   rootfs       Linux File System
 */
#define SMALLEST_FLASH_SIZE (16<<20)
#define DEFAULT_RESERVED_SIZE   (8<<20) 
#define DEFAULT_SPLASH_SIZE     (1<<20)
#define DEFAULT_BBT0_SIZE_MB    (1)
#define DEFAULT_BBT1_SIZE_MB    (4)

#ifdef CONFIG_MTD_ECM_PARTITION
#define DEFAULT_OCAP_SIZE   (6<<20)
#define DEFAULT_AVAIL1_SIZE (32<<20)
#define DEFAULT_ECM_SIZE (DEFAULT_OCAP_SIZE+DEFAULT_AVAIL1_SIZE)
#define AVAIL1_PART (1)
#define OCAP_PART   (2)
#else
#define DEFAULT_ECM_SIZE    (0)
#define DEFAULT_OCAP_SIZE   (0)
#define DEFAULT_AVAIL1_SIZE (0)
#define AVAIL1_PART (-1)
#define OCAP_PART   (-1)
#endif
#define DEFAULT_ROOTFS_SIZE (SMALLEST_FLASH_SIZE - DEFAULT_RESERVED_SIZE - DEFAULT_ECM_SIZE)

#if defined(CONFIG_MIPS_BRCM)
/* Only used for data storage. */
static struct mtd_partition bcm63XX_nand_parts[] = 
{
    {name: "rootfs", offset: 0, size: 0},
    {name: "data",  offset: 0, size: 0},
    {name: "nvram",  offset: 0, size: 0},
    {name: NULL,     offset: 0, size: 0}
};

static inline u_int64_t device_size(struct mtd_info *mtd) 
{
    return mtd->size;
}

#else
static struct mtd_partition bcm7XXX_nand_parts[] = 
{
#define ROOTFS_PART (0)
    { name: "rootfs", offset: 0, size: DEFAULT_ROOTFS_SIZE },    
#ifdef CONFIG_MTD_ECM_PARTITION
    { name: "avail1", offset: DEFAULT_ROOTFS_SIZE, size: DEFAULT_AVAIL1_SIZE },
    { name: "ocap", offset: DEFAULT_ROOTFS_SIZE+DEFAULT_AVAIL1_SIZE,
      size: DEFAULT_OCAP_SIZE },
#endif
    { name: "kernel",   offset: 0x00800000,         size: 4<<20 },
    { name: "cfe",      offset: 0x00C00000,         size: 2<<20 },
    { name: "nvm",      offset: 0x00E00000,         size: 1<<20 },
    /* BBT 1MB not mountable by anyone */
    { name: "data",     offset: 0x20000000,     size: 0 },
/* Add 1 extra place-holder partition for splash, and a safety guard element */
    {name: NULL, offset: 0, size: 0},
    {name: NULL, offset: 0, size: 0}
};
#endif

struct brcmnand_info
{
    struct mtd_info     mtd;
    struct mtd_partition*   parts;
    struct brcmnand_chip    brcmnand;
};

static struct brcmnand_info *info;

extern int gBcmSplash;

void* get_brcmnand_handle(void)
{
    void* handle = &info->brcmnand;
    return handle;
}
//EXPORT_SYMBOL(get_brcmnand_handle);


#if defined(CONFIG_MIPS_BRCM)
static void __devinit 
brcmnanddrv_setup_mtd_partitions(struct brcmnand_info* nandinfo, int *numParts)
{
    NVRAM_DATA nvram;
    struct mtd_info* mtd = &nandinfo->mtd;
    unsigned long size = (unsigned long) device_size(mtd);

    kerSysNvRamGet((char *)&nvram, sizeof(nvram), 0);
    *numParts = 3;
    nandinfo->parts = bcm63XX_nand_parts;

    /* Root FS.
     * TBD - Support dual images and select the root fs that was was booted.
     */
    bcm63XX_nand_parts[0].offset = nvram.ulNandPartOfsKb[NP_ROOTFS_1]*1024;
    bcm63XX_nand_parts[0].size = nvram.ulNandPartSizeKb[NP_ROOTFS_1]*1024;
    bcm63XX_nand_parts[0].ecclayout = mtd->ecclayout;

    /* Data (psi, scratch pad) */
    bcm63XX_nand_parts[1].offset = nvram.ulNandPartOfsKb[NP_DATA] * 1024;
    bcm63XX_nand_parts[1].size = nvram.ulNandPartSizeKb[NP_DATA] * 1024;
    bcm63XX_nand_parts[1].ecclayout = mtd->ecclayout;

    /* Boot and NVRAM data */
    bcm63XX_nand_parts[2].offset = nvram.ulNandPartOfsKb[NP_BOOT] * 1024;
    bcm63XX_nand_parts[2].size = nvram.ulNandPartSizeKb[NP_BOOT] * 1024;
    bcm63XX_nand_parts[2].ecclayout = mtd->ecclayout;

    PRINTK("Part[0] name=%s, size=%x, offset=%x\n", bcm63XX_nand_parts[0].name,
        bcm63XX_nand_parts[0].size, bcm63XX_nand_parts[0].offset);
    PRINTK("Part[1] name=%s, size=%x, offset=%x\n", bcm63XX_nand_parts[1].name,
        bcm63XX_nand_parts[1].size, bcm63XX_nand_parts[1].offset);
    PRINTK("Part[2] name=%s, size=%x, offset=%x\n", bcm63XX_nand_parts[2].name,
        bcm63XX_nand_parts[2].size, bcm63XX_nand_parts[2].offset);
}
#else
/* 
 * Size and offset are variable, depending on the size of the chip, but 
 * cfe_kernel always starts at 1FC0_0000 and is 4MB size.
 * The entire reserved area (kernel + CFE + BBT) occupies the last 8 MB of the
 * flash.
 */
static void __devinit 
brcmnanddrv_setup_mtd_partitions(struct brcmnand_info* nandinfo, int *numParts)
{
    struct mtd_info* mtd = &nandinfo->mtd;
    unsigned long size; 
    int i = 0;
    unsigned int ecm_size = DEFAULT_ECM_SIZE;
#ifdef CONFIG_MTD_ECM_PARTITION
    unsigned int ocap_size = DEFAULT_OCAP_SIZE;
#endif
    unsigned int avail1_size = DEFAULT_AVAIL1_SIZE;

    if (device_size(mtd) <= (512<<20))
    {
        // mtd->size may be different than nandinfo->size
        size = (unsigned long) device_size(mtd);

        /* take into account the extra 2 parts and the data partition */
        *numParts = ARRAY_SIZE(bcm7XXX_nand_parts) - 3;
    }
    else
    {
        size = 512 << 20;

        // take into account the extra 2 parts
        *numParts = ARRAY_SIZE(bcm7XXX_nand_parts) - 2;
    }

#ifdef CONFIG_MTD_ECM_PARTITION
    /*Do not generate AVAIL1 partition if usable flash size is less than 64MB*/
    if (size < (64<<20))
    {
        ecm_size = DEFAULT_OCAP_SIZE;
        bcm7XXX_nand_parts[AVAIL1_PART].size = avail1_size = 0;
        (*numParts)--;
    }
    else
    {
        int factor = size / (64 << 20); // Remember size is capped at 512MB
        bcm7XXX_nand_parts[OCAP_PART].size = ocap_size =
            factor * DEFAULT_OCAP_SIZE;
        bcm7XXX_nand_parts[AVAIL1_PART].size =
            avail1_size = factor*DEFAULT_AVAIL1_SIZE;
        ecm_size = ocap_size + avail1_size;
    }
#endif
    nandinfo->parts = bcm7XXX_nand_parts;
    bcm7XXX_nand_parts[0].size = size - DEFAULT_RESERVED_SIZE - ecm_size;
    bcm7XXX_nand_parts[0].ecclayout = mtd->ecclayout;
    PRINTK("Part[%d] name=%s, size=%x, offset=%x\n", i,
        bcm7XXX_nand_parts[0].name, bcm7XXX_nand_parts[0].size,
        bcm7XXX_nand_parts[0].offset);
    for (i=1; i<(*numParts); i++)
    {
#ifdef CONFIG_MTD_ECM_PARTITION
        //if (0 == bcm7XXX_nand_parts[i].size)
        //  continue;

        /* Skip avail1 if size is less than 64 MB) */
        if (0 == avail1_size && AVAIL1_PART == i)
        {
            bcm7XXX_nand_parts[i].offset = bcm7XXX_nand_parts[i-1].size +
                bcm7XXX_nand_parts[i-1].offset;
            continue;
        }
#endif
        bcm7XXX_nand_parts[i].offset = bcm7XXX_nand_parts[i-1].size +
            bcm7XXX_nand_parts[i-1].offset;

        // For now every partition uses the same oobinfo
        bcm7XXX_nand_parts[i].ecclayout = mtd->ecclayout;
        PRINTK("Part[%d] name=%s, size=%x, offset=%x\n", i,
            bcm7XXX_nand_parts[i].name, bcm7XXX_nand_parts[i].size,
            bcm7XXX_nand_parts[i].offset);
    }
    if  (device_size(mtd) > (uint64_t) (512 << 20))
    {
        // For total flash size > 512MB, we must split the rootfs into 2
        // partitions
        i = *numParts - 1;
        bcm7XXX_nand_parts[i].offset = 512 << 20;
        bcm7XXX_nand_parts[i].size = device_size(mtd) -
            ((512+DEFAULT_BBT1_SIZE_MB) << 20);
        bcm7XXX_nand_parts[i].ecclayout = mtd->ecclayout;

#ifdef CONFIG_MTD_ECM_PARTITION
        PRINTK("Part[%d] name=%s, size=%x, offset=%x\n", avail1_size? i: i-1,
            bcm7XXX_nand_parts[i].name, bcm7XXX_nand_parts[i].size,
            bcm7XXX_nand_parts[i].offset);
#else
        PRINTK("Part[%d] name=%s, size=%x, offset=%x\n", i,
            bcm7XXX_nand_parts[i].name, 
        bcm7XXX_nand_parts[i].size, bcm7XXX_nand_parts[i].offset);
#endif
    }
#ifdef CONFIG_MTD_ECM_PARTITION

    /* Shift partitions 1 up if avail1_size is 0 */
    if (0 == avail1_size)
    {
        for (i=AVAIL1_PART; i < *numParts; i++)
        {
            bcm7XXX_nand_parts[i].offset = bcm7XXX_nand_parts[i+1].offset;
            bcm7XXX_nand_parts[i].size = bcm7XXX_nand_parts[i+1].size;
        }
        bcm7XXX_nand_parts[*numParts].offset = 0;
        bcm7XXX_nand_parts[*numParts].size = 0;
    }
#endif
    if (gBcmSplash)
    {
        PRINTK("In bcmSplash, numParts=%d\n", *numParts);
        for (i=0; i<*numParts; i++)
        {
            PRINTK("bcm7xxx-nand.c: i=%d\n", i);
            PRINTK("B4 Part[%d] name=%s, size=%x, offset=%x\n",  i, 
            bcm7XXX_nand_parts[i].name, bcm7XXX_nand_parts[i].size,
                bcm7XXX_nand_parts[i].offset);

            /* we will carve out 512KB from avail1 partition if it exists, or
             * rootfs otherwise
             */
            if ((avail1_size && i == AVAIL1_PART) ||
                (avail1_size == 0 && i == ROOTFS_PART))
            {
                int j;

                /* i now points to the avail1 and/or rootfs partition */
                bcm7XXX_nand_parts[i].size -= DEFAULT_SPLASH_SIZE;
                if (i > 0)
                {
                    bcm7XXX_nand_parts[i].offset=bcm7XXX_nand_parts[i-1].size +
                        bcm7XXX_nand_parts[i-1].offset;
                }
                else
                {
                    bcm7XXX_nand_parts[i].offset = 0;
                }

                /* Move all partitions that follow one down */
                for (j=*numParts - 1; j > i; j--)
                {
                    bcm7XXX_nand_parts[j+1] = bcm7XXX_nand_parts[j];
                    PRINTK("Moved partition[%d] down to [%d], name=%s\n", j,
                        j+1, bcm7XXX_nand_parts[j+1].name);
                }
                (*numParts)++;
                PRINTK("original: #parts=%d, Part[%d] name=%s, size=%x, " \
                    "offset=%x\n", *numParts, i, bcm7XXX_nand_parts[i].name,
                    bcm7XXX_nand_parts[i].size, bcm7XXX_nand_parts[i].offset);
                i++;

                /* i now points to the newly created splash partition */
                bcm7XXX_nand_parts[i].offset = bcm7XXX_nand_parts[i-1].size +
                    bcm7XXX_nand_parts[i-1].offset;
                bcm7XXX_nand_parts[i].size = DEFAULT_SPLASH_SIZE;
                bcm7XXX_nand_parts[i].name = "splash";
                PRINTK("splash: #parts=%d, Part[%d] name=%s, size=%x, " \
                    "offset=%x\n", *numParts, i, bcm7XXX_nand_parts[i].name,
                    bcm7XXX_nand_parts[i].size, bcm7XXX_nand_parts[i].offset);
            }
        }
    }
}
#endif


static void* gPageBuffer;

static int __devinit brcmnanddrv_probe(struct platform_device *pdev)
{
    //struct flash_platform_data *pdata = pdev->dev.platform_data;
    //struct resource *res = pdev->resource;
    //unsigned long size = res->end - res->start + 1;
    int err = 0;
    int numParts = 0;
    volatile unsigned long* pReg = (volatile unsigned long*)
        (BRCMNAND_CTRL_REGS + BCHP_NAND_CS_NAND_SELECT - BCHP_NAND_REVISION);

	PRINTK(">-- brcmnanddrv_probe\n");

    // Enable NAND data on MII ports.
    PERF->blkEnables |= NAND_CLK_EN;

    /* Enable controller to automatically read device id.  Possibly enable
     * selecting EBI CS0.  This bit is undefined in BCM6368 RDB but is set
     * in a simulation test program.
     */
    *pReg = BCHP_NAND_CS_NAND_SELECT_AUTO_DEVICE_ID_CONFIG_MASK |
        BCHP_NAND_CS_NAND_SELECT_EBI_CS_1_SEL_MASK;

    gPageBuffer = NULL;
    info = kmalloc(sizeof(struct brcmnand_info), GFP_KERNEL);
    if (!info)
        return -ENOMEM;
    memset(info, 0, sizeof(struct brcmnand_info));
#ifndef CONFIG_MTD_BRCMNAND_EDU
    gPageBuffer = kmalloc(sizeof(struct nand_buffers), GFP_KERNEL);
    info->brcmnand.buffers = (struct nand_buffers*) gPageBuffer;
#else

    /* Align on 512B boundary for efficient DMA transfer */
    gPageBuffer = kmalloc(sizeof(struct nand_buffers) + 511, GFP_DMA);
    info->brcmnand.buffers =
        (struct nand_buffers*) (((unsigned int) gPageBuffer+511) & (~511));
#endif
    if (!info->brcmnand.buffers)
    {
        kfree(info);
        return -ENOMEM;
    }
    memset(info->brcmnand.buffers, 0, sizeof(struct nand_buffers));
    info->brcmnand.numchips = 1; // For now, we only support 1 chip
    info->brcmnand.chip_shift = 0; // Only 1 chip
    //info->brcmnand.regs = pdev->resource[0].start;
    info->brcmnand.priv = &info->mtd;
    //info->brcmnand.mmcontrol = NULL;  // THT: Sync Burst Read TBD.
    info->mtd.name = dev_name(&pdev->dev);
    info->mtd.priv = &info->brcmnand;
    info->mtd.owner = THIS_MODULE;

    /* Enable the following for a flash based bad block table */
    info->brcmnand.options |= NAND_USE_FLASH_BBT;
    PRINTK("brcmnand_scan\n");
    if (brcmnand_scan(&info->mtd, MAX_NAND_CS))
    {
        err = -ENXIO;
        goto out_free_info;
    }
    PRINTK("  brcmnanddrv_setup_mtd_partitions\n");
    printk("    numchips=%d, size=%llx\n", info->brcmnand.numchips,
        device_size(&(info->mtd)));
    brcmnanddrv_setup_mtd_partitions(info, &numParts);
    PRINTK("  add_mtd_partitions\n");
    add_mtd_partitions(&info->mtd, info->parts, numParts);
    PRINTK("  dev_set_drvdata\n");    
    dev_set_drvdata(&pdev->dev, info);
    PRINTK("<-- brcmnanddrv_probe\n");
    return 0;

out_free_info:
    if (gPageBuffer)
        kfree(gPageBuffer);
    kfree(info);
    return err;
}

static int __devexit brcmnanddrv_remove(struct platform_device *pdev)	
{
    struct brcmnand_info *info = dev_get_drvdata(&pdev->dev);

    //struct resource *res = pdev->resource;
    //unsigned long size = res->end - res->start + 1;
    dev_set_drvdata(&pdev->dev, NULL);
    if (info)
    {
        if (info->parts)
            del_mtd_partitions(&info->mtd);
        //else
        //del_mtd_device(&info->mtd);
        brcmnand_release(&info->mtd);
        //release_mem_region(res->start, size);
        //iounmap(info->brcmnand.base);
        kfree(gPageBuffer);
        kfree(info);
    }
    return 0;
}

static struct platform_driver bcm63xx_nand_driver = {
	.probe		= brcmnanddrv_probe,
	.remove		= brcmnanddrv_remove,
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

MODULE_ALIAS(DRIVER_NAME);

static int __init brcmnanddrv_init(void)
{
    int err = 0;
    struct resource devRes[1];
    struct platform_device* pdev;
    unsigned long getMemorySize(void);

    printk (DRIVER_INFO " (BrcmNand Controller)\n");
    memset(devRes, 0, sizeof(devRes));
    devRes[0].name = "brcmnand-base";
    devRes[0].start = BRCMNAND_CTRL_REGS;
    devRes[0].end = BRCMNAND_CTRL_REGS_END;
    devRes[0].flags = IORESOURCE_MEM;
    // Will need this whenever we use interrupt to look at the flash status
    //devRes[1].name ="brcmNand-irq";
    //devRes[1].start = tbd;
    //devRes[1].end = tbd;
    //devRes[1].flags = IORESOURCE_IRQ;
    // Before we register the driver, add a simple device matching our driver
    pdev = platform_device_register_simple( (char*) bcm63xx_nand_driver.driver.name,
        0, /* ID */ devRes, ARRAY_SIZE(devRes));
    if (IS_ERR(pdev))
    {
        printk("brcmnanddrv_init: device register failed, err=%d\n", err);
        return PTR_ERR(pdev);
    }
    // Set up dma_mask for our platform device
    // Overwrite whatever value it was set to.
    if (1 /*!pdev->dev.dma_mask*/)
    {
        // default is 32MB 0x01ffffff;
        //dma_set_mask(&pdev->dev, (u64) ((unsigned long) upper_memory - 1UL));
        //dma_set_mask(&pdev->dev, 0x01ffffff);
        //pdev->dev.dma_mask = (u64*) 0x01ffffff;  
        pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask; 
        pdev->dev.coherent_dma_mask = (u64)  ( getMemorySize() - 1UL);
    }

	err = platform_driver_register(&bcm63xx_nand_driver);
    if (err)
    {
        printk("brcmnanddrv_init: driver_register failed, err=%d\n", err);
        return err;
    }
    return 0;
}

static void __exit brcmnanddrv_exit(void)
{
    platform_driver_unregister(&bcm63xx_nand_driver);
}

module_init(brcmnanddrv_init);
module_exit(brcmnanddrv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ton Truong <ttruong@broadcom.com>");
MODULE_DESCRIPTION("Broadcom NAND flash driver");

