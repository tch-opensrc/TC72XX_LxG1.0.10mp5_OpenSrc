/*
 * Flash mapping for BCM7xxx boards
 *
 * Copyright (C) 2001 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * THT				10-23-2002
 * Steven J. Hill	09-25-2001
 * Mark Huang		09-15-2001
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/init.h>
#include <bcm_map_part.h>
#include <bcm_hwdefs.h>


extern int kerSysNvRamGet(char *string, int strLen, int offset);

#define FLASH_SIZE 0x2000000
#define BUSWIDTH 2

static struct mtd_info *bcm63xx_mtd;

static map_word bcm63xx_map_read(struct map_info *map, unsigned long ofs)
{
	return inline_map_read(map, ofs);
}

static void bcm63xx_map_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->virt + from, len);
}

static void bcm63xx_map_write(struct map_info *map, const map_word d, unsigned long ofs)
{
	inline_map_write(map, d, ofs);
}

static void bcm63xx_map_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	inline_map_copy_to(map, to, from, len);
}

struct map_info bcm63xx_map
= {
	name: "Broadcom 3329 mapped flash",
	// size: WINDOW_SIZE, // THT: Now a value to be determined at run-time.
	bankwidth: BUSWIDTH,

	read: bcm63xx_map_read,
	copy_from: bcm63xx_map_copy_from,
	write: bcm63xx_map_write,
	copy_to: bcm63xx_map_copy_to
};

/*
 * Don't define DEFAULT_SIZE_MB if the platform does not support a standard partition table.
 * Defining it will allow the user to specify flashsize=nnM at boot time for non-standard flash size, however.
 */

#define SMALLEST_FLASH_SIZE	(16<<20)
#define DEFAULT_RESERVED_SIZE (4<<20)  // CFE areas, from 1FC0_0000H to 2000_0000H

#ifdef CONFIG_MTD_ECM_PARTITION
#define DEFAULT_OCAP_SIZE		(6<<20)
#define DEFAULT_AVAIL1_SIZE 	(32<<20)
#define DEFAULT_ECM_SIZE 		(DEFAULT_OCAP_SIZE+DEFAULT_AVAIL1_SIZE)
#define AVAIL1_PART				(1)
#define OCAP_PART				(2)
#else
#define DEFAULT_ECM_SIZE		(0)
#define DEFAULT_OCAP_SIZE		(0)
#define DEFAULT_AVAIL1_SIZE		(0)
#endif
// DEFAULT_SIZE_MB will be defined later based on platforms.
#define DEFAULT_ROOTFS_SIZE (DEFAULT_SIZE_MB - DEFAULT_RESERVED_SIZE - DEFAULT_ECM_SIZE)

static struct mtd_partition bcm63xx_parts[] =
{
    {name: "rootfs", offset: 0x00020000, size: 0x00EE0000},
    {name: "data",  offset: 0x00007800, size: 0x00000400},
    {name: "nvram",  offset: 0x01E00000, size: 0x00100000},
    {name: NULL,     offset: 0, size: 0}
};

int __init init_bcm63xx_map(void)
{
	int i, numparts;
    NVRAM_DATA nvram;

    kerSysNvRamGet((char *)&nvram, sizeof(nvram), 0);
    numparts = 3;

#if 0
    /* Root FS.
     * TBD - Support dual images and select the root fs that was was booted.
     */
    bcm63xx_parts[0].offset = nvram.ulNandPartOfsKb[NP_ROOTFS_1]*1024;
    bcm63xx_parts[0].size = nvram.ulNandPartSizeKb[NP_ROOTFS_1]*1024;

    /* Data (psi, scratch pad) */
    bcm63xx_parts[1].offset = nvram.ulNandPartOfsKb[NP_DATA] * 1024;
    bcm63xx_parts[1].size = nvram.ulNandPartSizeKb[NP_DATA] * 1024;

    /* Boot and NVRAM data */
    bcm63xx_parts[2].offset = nvram.ulNandPartOfsKb[NP_BOOT] * 1024;
    bcm63xx_parts[2].size = nvram.ulNandPartSizeKb[NP_BOOT] * 1024;

    printk("Part[0] name=%s, size=%x, offset=%x\n", bcm63xx_parts[0].name,
    		bcm63xx_parts[0].size, bcm63xx_parts[0].offset);
    printk("Part[1] name=%s, size=%x, offset=%x\n", bcm63xx_parts[1].name,
    		bcm63xx_parts[1].size, bcm63xx_parts[1].offset);
    printk("Part[2] name=%s, size=%x, offset=%x\n", bcm63xx_parts[2].name,
    		bcm63xx_parts[2].size, bcm63xx_parts[2].offset);
#endif
	
	printk(KERN_NOTICE "BCM97XXX flash device: 0x%08lx @ 0x%08lx\n", FLASH_SIZE, FLASH_BASE);
	bcm63xx_map.size = FLASH_SIZE;
	numparts = ARRAY_SIZE(bcm63xx_parts);
	
	//bcm63xx_map.virt = ioremap_nocache((unsigned long)0xBFC00000, FLASH_SIZE);
	bcm63xx_map.virt = FLASH_BASE;

	if (!bcm63xx_map.virt) {
		printk("Failed to ioremap\n");
		return -EIO;
	}
	
	bcm63xx_mtd = do_map_probe("cfi_probe", &bcm63xx_map);
	if (!bcm63xx_mtd) {
		iounmap((void *)bcm63xx_map.virt);
		return -ENXIO;
	}
		
	add_mtd_partitions(bcm63xx_mtd, bcm63xx_parts, numparts);
	bcm63xx_mtd->owner = THIS_MODULE;
	return 0;
}

void __exit cleanup_bcm63xx_map(void)
{
	if (bcm63xx_mtd) {
		del_mtd_partitions(bcm63xx_mtd);
		map_destroy(bcm63xx_mtd);
	}
	if (bcm63xx_map.virt) {
		iounmap((void *)bcm63xx_map.virt);
		bcm63xx_map.virt = 0;
	}
}

module_init(init_bcm63xx_map);
module_exit(cleanup_bcm63xx_map);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Steven J. Hill <shill@broadcom.com>");
MODULE_DESCRIPTION("Broadcom 63xx MTD map driver");
