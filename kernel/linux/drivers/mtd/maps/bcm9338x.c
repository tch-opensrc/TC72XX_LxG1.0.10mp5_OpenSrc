/*
 * A simple flash mapping code for BCM963xx board flash memory
 * It is simple because it only treats all the flash memory as ROM
 * It is used with chips/map_rom.c
 *
 *  Song Wang (songw@broadcom.com)
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>

#define  VERSION	"1.0"

extern int __init bcm_vflash_init(void);
extern int __init linux_services_init(void);
extern int __init rpc_init(void);

static int __init init_brcm_338x_drivers(void)
{
#if defined (CONFIG_BCM_ITCRPC)
	rpc_init();
    printk("bcm9338x_mtd driver v%s\n", VERSION);
    bcm_vflash_init();
	printk("bcm9338x Linux services init\n");
	linux_services_init();
#endif
        return 0;
}

static void __exit cleanup_brcm_338x_drivers(void)
{
}

module_init(init_brcm_338x_drivers);
module_exit(cleanup_brcm_338x_drivers);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Song Wang songw@broadcom.com");
MODULE_DESCRIPTION("Configurable MTD map driver for read-only root file system");
