/*
 * Copyright (C) 2009 Broadcom Corporation
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

#include <linux/init.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/compiler.h>

#include <asm/mipsregs.h>
#include <asm/barrier.h>
#include <asm/cacheflush.h>
#include <asm/r4kcache.h>
#include <asm/asm-offsets.h>
#include <asm/inst.h>
#include <asm/fpu.h>
#include <asm/hazards.h>
#include <asm/cpu-features.h>
#include <asm/brcmstb/brcmstb.h>


/***********************************************************************
 * MIPS features, caches, and bus interface
 ***********************************************************************/

void bchip_mips_setup(void)
{
	/* enable RDHWR, BRDHWR */
	set_c0_brcm_config(BIT(17) | BIT(21));

	if (kernel_uses_smartmips_rixi) {
		/* XI enable */
		set_c0_brcm_config(BIT(27));

		/* enable MIPS32R2 ROR instruction for XI TLB handlers */
		__asm__ __volatile__(
		"	li	$8, 0x5a455048\n"
		"	.word	0x4088b00f\n"	/* mtc0 $8, $22, 15 */
		"	nop; nop; nop\n"
		"	.word	0x4008b008\n"	/* mfc0 $8, $22, 8 */
		"	lui	$9, 0x0100\n"
		"	or	$8, $9\n"
		"	.word	0x4088b008\n"	/* mtc0 $8, $22, 8 */
		: : : "$8", "$9");
	}
}
