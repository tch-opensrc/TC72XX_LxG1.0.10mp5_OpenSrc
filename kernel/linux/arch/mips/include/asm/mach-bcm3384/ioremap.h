/*
 *	include/asm-mips/mach-brcmstb/ioremap.h
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */
#ifndef __ASM_MACH_BRCMSTB_IOREMAP_H
#define __ASM_MACH_BRCMSTB_IOREMAP_H

#include <linux/types.h>

/*
 * Allow physical addresses to be fixed up to help peripherals located
 * outside the low 32-bit range -- generic pass-through version.
 */
static inline phys_t fixup_bigphys_addr(phys_t phys_addr, phys_t size)
{
	return phys_addr;
}

extern void __iomem *plat_ioremap(phys_t offset, unsigned long size,
	unsigned long flags);
extern int plat_iounmap(const volatile void __iomem *addr);

#endif /* __ASM_MACH_BRCMSTB_IOREMAP_H */
