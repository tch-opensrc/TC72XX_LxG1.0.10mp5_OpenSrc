/*
 * Copyright (C) 2000-2004 Russell King
 * Copyright (C) 2010 Broadcom Corporation
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
 */

#include <linux/bootmem.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/compiler.h>
#include <linux/module.h>

#include <asm/page.h>
#include <asm/pgtable-32.h>
#include <asm/pgtable-bits.h>
#include <asm/addrspace.h>
#include <asm/tlbflush.h>
#include <asm/r4kcache.h>
#include <asm/brcmstb/brcmstb.h>

#include <spaces.h>

void __iomem *plat_ioremap(phys_t offset, unsigned long size,
	unsigned long flags)
{
	/* sanity check */
	if ((offset + size - 1) < offset ||
	    !size ||
	    offset > max(KSEG0_SIZE, KSEG1_SIZE))
		return NULL;

	/* !XKS01, XKS01: uncached access to EBI/registers @ PA 1000_0000 */
	if (offset >= 0x10000000 &&
	    (offset + size) <= 0x20000000 &&
	    flags == _CACHE_UNCACHED)
		return (void *)(KSEG1 + offset);

	/* !XKS01, XKS01: easy cached access to some DRAM */
	if ((offset + size) <= KSEG0_SIZE &&
	    flags == _CACHE_CACHABLE_NONCOHERENT)
		return (void *)(KSEG0 + offset);

	/* !XKS01 only: easy uncached access to some DRAM */
	if ((offset + size) <= KSEG1_SIZE &&
	    flags == _CACHE_UNCACHED)
		return (void *)(KSEG1 + offset);

	/* anything else gets mapped using page tables */
	return NULL;
}
EXPORT_SYMBOL(plat_ioremap);

int plat_iounmap(const volatile void __iomem *addr)
{
	phys_t va = (unsigned long)addr;

	if (va >= KSEG0 && va < (KSEG0 + KSEG0_SIZE))
		return 1;
	if (va >= KSEG1 && va < (KSEG1 + KSEG1_SIZE))
		return 1;
	return 0;
}
EXPORT_SYMBOL(plat_iounmap);
