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

#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/root_dev.h>
#include <linux/types.h>
#include <linux/smp.h>
#include <linux/version.h>
#include <linux/serial_reg.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/compiler.h>
#include <linux/mtd/mtd.h>

#include <asm/bootinfo.h>
#include <asm/r4kcache.h>
#include <asm/traps.h>
#include <asm/cacheflush.h>
#include <asm/mipsregs.h>
#include <asm/hazards.h>
#include <asm/brcmstb/brcmstb.h>

#include <spaces.h>

static void __cpuinit set_board_nmi_handler(void);

int no_early_printk = 0;
/***********************************************************************
 * Main entry point
 ***********************************************************************/

void __init prom_init(void)
{
	extern unsigned char ecos_to_linux_boot_args[];

	strncpy(arcs_cmdline, ecos_to_linux_boot_args, CL_SIZE-1);
	arcs_cmdline[CL_SIZE-1] = 0;
	if (strstr(arcs_cmdline, "nouart"))
		no_early_printk = 1;

	bchip_mips_setup();
	set_board_nmi_handler();

#ifdef CONFIG_SMP
	register_smp_ops(&brcmstb_smp_ops);
#endif
}

/***********************************************************************
 * Vector relocation for NMI and SMP TP1 boot
 ***********************************************************************/

extern void nmi_exception_handler(struct pt_regs *regs);
void (*brcm_nmi_handler)(struct pt_regs *) = &nmi_exception_handler;

void brcm_set_nmi_handler(void (*fn)(struct pt_regs *))
{
	brcm_nmi_handler = fn;
}
EXPORT_SYMBOL(brcm_set_nmi_handler);

void __cpuinit brcm_wr_vec(unsigned long dst, char *start, char *end)
{
	memcpy((void *)dst, start, end - start);
	dma_cache_wback((unsigned long)start, end - start);
	local_flush_icache_range(dst, dst + (end - start));
	instruction_hazard();
}

static inline void __cpuinit brcm_nmi_handler_setup(void)
{
	brcm_wr_vec(BRCM_NMI_VEC, brcm_reset_nmi_vec, brcm_reset_nmi_vec_end);
	brcm_wr_vec(BRCM_WARM_RESTART_VEC, brcm_tp1_int_vec, brcm_tp1_int_vec_end);
}

static void __cpuinit set_board_nmi_handler(void)
{
	board_nmi_handler_setup = &brcm_nmi_handler_setup;
}

unsigned long brcm_setup_ebase(void)
{
	/*
	 * BMIPS5000 is similar to BMIPS4380, but it uses different
	 * configuration registers with different semantics:
	 *
	 * 8000_0000 - new reset/NMI vector            (was: bfc0_0000)
	 * 8000_1000 - new !BEV exception base         (was: 8000_0000)
	 *
	 * The initial reset/NMI vector for TP1 is at a000_0000 because
	 * CP0 CONFIG comes up in an undefined state, but it is almost
	 * immediately moved down to kseg0.
	 */
	unsigned long ebase = 0x80001000;

	write_c0_brcm_bootvec(0xa0088008);
	write_c0_ebase(ebase);

	return ebase;
}

/***********************************************************************
 * Miscellaneous utility functions
 ***********************************************************************/

void prom_putchar(char c)
{
    unsigned int tx_level;

	if (no_early_printk)
		return;

    while(1)
    {
        tx_level = *(volatile unsigned int *)0xb4e00528;
        tx_level = tx_level >> 24;
        tx_level &= 0x1f;
        if(tx_level < 14)
            break;
    }
    *(volatile unsigned int *)0xb4e00534 = (unsigned int)c;
}
