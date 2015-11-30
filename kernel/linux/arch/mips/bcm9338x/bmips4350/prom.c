/*
  <:copyright-gpl 
  Copyright 2004 Broadcom Corp. All Rights Reserved. 
 
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
/*
 * prom.c: PROM library initialization code.
 *
 */
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/bootmem.h>
#include <linux/blkdev.h>
#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <asm/time.h>

#include <bcm_map_part.h>
#include <bcm_cpu.h>
#include <board.h>
#include <boardparms.h>
#include <asm/brcmstb/brcmstb.h>

u32 __init cpu_speed(void)
{
	return 600 * 1000 * 1000;
}

const char *get_system_type(void)
{
#if defined (CONFIG_BCM3385)
	return "Linux on 3385 Viper";
#endif
#if defined (CONFIG_BCM3384)
	return "Linux on 3384 Viper";
#endif
}


void __init prom_init(void)
{
	extern unsigned char ecos_to_linux_boot_args[];

	strncpy(arcs_cmdline, ecos_to_linux_boot_args, CL_SIZE-1);
	arcs_cmdline[CL_SIZE-1] = 0;
	/* Count register increments every other clock */
	mips_hpt_frequency = cpu_speed() / 2;
}


extern int tlb_is_inited;
extern void tlb_init(void);

#if defined (CONFIG_BCM3385)
int added_wired_in_prom_putchar = 0;
void prom_putchar(char c)
{
	extern void add_wired_entry(unsigned long entrylo0,
				    unsigned long entrylo1,
				    unsigned long entryhi,
				    unsigned long pagemask);
	u32 tx_level;

	if ((added_wired_in_prom_putchar == 0) ||
	    (tlb_is_inited && added_wired_in_prom_putchar == 1)) {
		add_wired_entry((0xd << 22) | (2 << 3) | 7,
				(0xe << 22) | (2 << 3) | 7,
				0xd0000000,
				PM_64M);
		if (added_wired_in_prom_putchar++ == 0) {
			u32 *reg, val;
			/* uart 1 pin mux and pad control */
			reg = (u32 *)(0xd0000000 | BCHP_CM_TOP_CTRL_PIN_MUX_PAD_CTRL_3);
			val = *reg;
			val &= ~(BCHP_CM_TOP_CTRL_PIN_MUX_PAD_CTRL_3_bnm_gpio_041_pad_ctrl_MASK |
				 BCHP_CM_TOP_CTRL_PIN_MUX_PAD_CTRL_3_bnm_gpio_042_pad_ctrl_MASK);
			*reg = val;
		
			reg = (u32 *)(0xd0000000 | BCHP_CM_TOP_CTRL_PIN_MUX_CTRL_5);
			val = *reg;
			val &= ~(BCHP_CM_TOP_CTRL_PIN_MUX_CTRL_5_bnm_gpio_041_MASK |
				 BCHP_CM_TOP_CTRL_PIN_MUX_CTRL_5_bnm_gpio_042_MASK);
			val |= (BCHP_CM_TOP_CTRL_PIN_MUX_CTRL_5_bnm_gpio_041_BNM_UART_RXD_1 <<
				BCHP_CM_TOP_CTRL_PIN_MUX_CTRL_5_bnm_gpio_041_SHIFT);
			val |= (BCHP_CM_TOP_CTRL_PIN_MUX_CTRL_5_bnm_gpio_042_BNM_UART_TXD_1 <<
				BCHP_CM_TOP_CTRL_PIN_MUX_CTRL_5_bnm_gpio_042_SHIFT);
			*reg = val;

			reg = (u32 *)(0xd0000000 | BCHP_UART1_PER_CONTROL);
			*reg = 0x00e03710;
			reg = (u32 *)(0xd0000000 | BCHP_UART1_PER_BAUDWORD);
			*reg = 0xe;
		}
	}
	while(1)
	{
		tx_level = *(u32 *)(0xd0000000 | BCHP_UART1_PER_MISC_CTL);
		tx_level = tx_level >> 24;
		tx_level &= 0x1f;
		if(tx_level < 14)
			break;
	}

	*(u32 *)(0xd0000000 | BCHP_UART1_PER_UARTFIFO) = (unsigned int)c;
}
#elif defined (CONFIG_BCM3384)


void prom_putchar(char c)
{
    unsigned int tx_level;

    while(1)
    {
        tx_level = *(volatile unsigned int *)0xb4e00548;
        tx_level = tx_level >> 24;
        tx_level &= 0x1f;
        if(tx_level < 14)
            break;
    }
    *(volatile unsigned int *)0xb4e00554 = (unsigned int)c;
}

#endif

void __init prom_free_prom_memory(void)
{
}
