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
//#include <linux/config.h>

#include <board.h>
#if defined(CONFIG_BCM93380) || defined(CONFIG_BCMKILAUEA) || defined(CONFIG_BCM93383)
#include <ProgramStore.h>
#else
#include <bcmTag.h>
#endif
#include <bcm_map_part.h>
#define  VERSION	"1.0"

#if defined(CONFIG_BCM93380) || defined(CONFIG_BCMKILAUEA) || defined(CONFIG_BCM93383)
#include <flash_api.h>
extern BcmProgramHeader * kerSysImageTagGet(void);
#else
extern PFILE_TAG kerSysImageTagGet(void);
#endif

static struct mtd_info *mymtd;

static map_word brcm_physmap_read16(struct map_info *map, unsigned long ofs)
{
	map_word val;
	
        kerSysReadFromFlash( &val.x[0], map->map_priv_1 + ofs, sizeof(short) );
	
	return val;
}

static void brcm_physmap_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
        kerSysReadFromFlash( to, map->map_priv_1 + from, len );
}

static struct map_info brcm_physmap_map = {
	.name		= "Physically mapped flash",
	.bankwidth	= 2,
	.read		= brcm_physmap_read16,
	.copy_from	= brcm_physmap_copy_from
};

static int __init init_brcm_physmap(void)
{
#if defined(CONFIG_BCM93380) || defined(CONFIG_BCMKILAUEA) || defined(CONFIG_BCM93383)
	extern int get_part_offset(void);
    u_int32_t rootfs_offset, kernel_offset;
	BcmProgramHeader * pHdr = NULL;
	printk("bcm963xx_mtd driver v%s\n", VERSION);

	/* Read the flash memory map from flash memory. */
	if (!(pHdr = kerSysImageTagGet()))
	{
		printk("Failed to read image hdr from flash\n");
		return -EIO;
	}
	/* read program store header, calculate the offset for rootfs */
	/* assuming image 1 starts two sectors from the beginning. image 2 */
	/* starts from the middle of the flash */
	/* assuming kernel first, then padding, then rootfs */
#if defined(CONFIG_BCM_RAMDISK)
	kernel_offset = 0x87A00000;
#else
	kernel_offset = get_part_offset();
#endif
	
	kernel_offset += sizeof(BcmProgramHeader);
	rootfs_offset = kernel_offset + pHdr->ulfilelength - pHdr->ulCompressedLength2;
	brcm_physmap_map.size = pHdr->ulCompressedLength2;
//#if defined(CONFIG_BCM93380)
#if defined(CONFIG_BCM_RAMDISK)
	brcm_physmap_map.map_priv_1 = rootfs_offset;
#else
	brcm_physmap_map.map_priv_1 = rootfs_offset + FLASH_BASE;
#endif
//#else
	//brcm_physmap_map.map_priv_1 = (unsigned long)0xBFC00000 + rootfs_offset;
//#endif
#if defined(CONFIG_SPI_FLASH_DEBUG)
	printk("kernel_offset = %08x\n", kernel_offset);
	printk("rootfs_offset = %08x\n", rootfs_offset);
	printk("size = %08lx\n", brcm_physmap_map.size);
	printk("map_priv_1 = %08lx\n", brcm_physmap_map.map_priv_1);
#endif
#else    
        PFILE_TAG pTag = NULL;
        u_int32_t rootfs_addr, kernel_addr;

	printk("bcm963xx_mtd driver v%s\n", VERSION);

        /* Read the flash memory map from flash memory. */
        if (!(pTag = kerSysImageTagGet())) {
                printk("Failed to read image tag from flash\n");
                return -EIO;
        }

        rootfs_addr = (u_int32_t) simple_strtoul(pTag->rootfsAddress, NULL, 10) + BOOT_OFFSET;
        kernel_addr = (u_int32_t) simple_strtoul(pTag->kernelAddress, NULL, 10) + BOOT_OFFSET;
	
	brcm_physmap_map.size = kernel_addr - rootfs_addr;
	brcm_physmap_map.map_priv_1 = (unsigned long)rootfs_addr;
#endif

	if (!brcm_physmap_map.map_priv_1) {
		printk("Wrong rootfs starting address\n");
		return -EIO;
	}
	
	if (brcm_physmap_map.size <= 0) {
		printk("Wrong rootfs size\n");
		return -EIO;
	}	
	
	mymtd = do_map_probe("map_rom", &brcm_physmap_map);
	if (mymtd) {
		mymtd->owner = THIS_MODULE;
		add_mtd_device(mymtd);

		return 0;
	}

	return -ENXIO;
}

static void __exit cleanup_brcm_physmap(void)
{
	if (mymtd) {
		del_mtd_device(mymtd);
		map_destroy(mymtd);
	}
	if (brcm_physmap_map.map_priv_1) {
		brcm_physmap_map.map_priv_1 = 0;
	}
}

module_init(init_brcm_physmap);
module_exit(cleanup_brcm_physmap);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Song Wang songw@broadcom.com");
MODULE_DESCRIPTION("Configurable MTD map driver for read-only root file system");
