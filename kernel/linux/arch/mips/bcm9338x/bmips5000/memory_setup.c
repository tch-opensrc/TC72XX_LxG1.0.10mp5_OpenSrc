/*
  <:copyright-gpl
  Copyright 2013 Broadcom Corp. All Rights Reserved.

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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm/brcmstb/brcmstb.h>


#define NUM_ATWS	(BCHP_MEMC_ATW_UBUS_0_ENABLEi_ARRAY_END + 1)

typedef struct
{
	u32 enable[NUM_ATWS];
	u32 source_addr[NUM_ATWS];
	u32 size_mask[NUM_ATWS];
	u32 dest_addr[NUM_ATWS];
	u32 ubus_clients[NUM_ATWS];
}	memc_atw_regs;

typedef struct
{
	u32	memc_base;
	u32	ubus_base;
	u32	size;
}	addr_trans;

/* for translating memory to/from eCos */
static addr_trans addr_trans_table[NUM_ATWS];
	
static u32 ddr_mem_size(void)
{
	u32 *ddrconfig       = (u32*)0xb50b5004;
	u32 *ddrcntrlrconfig = (u32*)0xb50b2000;
	u32 *gpio_per_sdram_space = (u32*)0xb4e00284;
	u32 memsize = ( 32*1024*1024 ) << ((*ddrconfig & 1) + (*ddrcntrlrconfig & 7));

	printk("%s: %08x\n", __func__, memsize);

	if (memsize > (256*1024*1024)) {
		/* we cannot rely on CM side to set this up */
		switch(memsize >> 28) {
			case 0x2:	/* 512M */
				*gpio_per_sdram_space = 0x9;
				break;
			case 0x4:	/* 1G */
				*gpio_per_sdram_space = 0xa;
				break;
			case 0x8:	/* 2G */
				*gpio_per_sdram_space = 0xb;
				break;
			case 0xc:	/* 3G */
				*gpio_per_sdram_space = 0xc;
				break;
			default:
				printk("ERROR: Unsupported sdram size %08x in ubus space\n",
				       memsize);
		}
	}
	return memsize;
}
/******************************************************************************
 * memory setup:
 * Read the ATW regs and assume that any region with a memory controller base
 * address of ZERO is Linux.
 * Next, Calculate total memory size and assume that any memory left over that
 * has not been mapped by the ATW's is Linux memory (HIGHMEM).
 *****************************************************************************/
void __init plat_mem_setup(void)
{
	int i;
	memc_atw_regs *atws = (memc_atw_regs *)KSEG1ADDR(BCHP_MEMC_ATW_UBUS_0_REG_START);
	addr_trans *linux_trans = NULL;
	u32 memc_min = 0xffffffff;
	u32 memc_max = 0;
	u32 memsize;

	for (i = 0; i < NUM_ATWS; i++)
	{
		addr_trans *att = &addr_trans_table[i];
		if (atws->enable[i] & 1)
		{
			att->memc_base = atws->dest_addr[i];
			att->ubus_base = atws->source_addr[i];
			att->size = atws->size_mask[i] + 0x10000;
			if (att->memc_base == 0)
			{
				linux_trans = att;
			}
			else
			{
				add_memory_region(att->memc_base,
						  att->size,
						  BOOT_MEM_RESERVED);
				printk("Added NON Linux Memory Region %08x - %08x ubus %08x\n",
				       att->memc_base,
				       att->memc_base + att->size,
				       att->ubus_base);
				if (att->memc_base < memc_min)
					memc_min = att->memc_base;
			}
			if ((att->memc_base + att->size) > memc_max)
				memc_max = att->memc_base + att->size;
		}
		else
		{
			memset(att, 0, sizeof(addr_trans));
		}
	}
	BUG_ON(!linux_trans);
	linux_trans->size = memc_min;
	add_memory_region(linux_trans->memc_base, linux_trans->size, BOOT_MEM_RAM);
	printk("Added Linux Memory Region %08x - %08x ubus %08x\n",
	       linux_trans->memc_base,
	       linux_trans->memc_base + linux_trans->size,
	       linux_trans->ubus_base);

	memsize = ddr_mem_size();
	if (memsize > memc_max)
	{
		u32 highmemsize = memsize - memc_max;
		add_memory_region(HIGHMEM_START, highmemsize, BOOT_MEM_RAM);
		printk("Added Linux Memory Region %08x - %08x ubus %08x\n",
		       HIGHMEM_START, HIGHMEM_START + highmemsize, HIGHMEM_START);
	}
}

u32 viper_to_zephyr_addr(u32 viper_addr)
{
	int i;
	u32 ubus, memc, size;
	u32 viper_phys = CPHYSADDR(viper_addr);
	u32 kseg = KSEGX(viper_addr);
	addr_trans	*att = &addr_trans_table[0];
	
	for (i = 0; i < NUM_ATWS; i++)
	{
		memc = att->memc_base;
		ubus = att->ubus_base;
		size = att->size;
		if ((viper_phys >= ubus) && (viper_phys < (ubus+size)))
		{
			return kseg | (memc + (viper_phys - ubus));
		}
		att++;
	}
	printk("%s: Unable to translate %08x phys %08x\n",
	       __func__, viper_addr, viper_phys);
	return viper_addr;
}
EXPORT_SYMBOL(viper_to_zephyr_addr);

u32 zephyr_to_viper_addr(u32 zephyr_addr)
{
	int i;
	u32 ubus, memc, size;
	u32 zephyr_phys = CPHYSADDR(zephyr_addr);
	u32 kseg = KSEGX(zephyr_addr);
	addr_trans	*att = &addr_trans_table[0];
	
	for (i = 0; i < NUM_ATWS; i++)
	{
		memc = att->memc_base;
		ubus = att->ubus_base;
		size = att->size;
		if ((zephyr_phys >= memc) && (zephyr_phys < (memc+size)))
		{
			return kseg | (ubus + (zephyr_phys - memc));
		}
		att++;
	}
	printk("%s: Unable to translate %08x phys %08x\n",
	       __func__, zephyr_addr, zephyr_phys);
	return zephyr_addr;
}
EXPORT_SYMBOL(zephyr_to_viper_addr);
