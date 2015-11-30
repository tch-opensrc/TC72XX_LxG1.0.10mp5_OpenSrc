/*
<:copyright-gpl
 Copyright 2002 Broadcom Corp. All Rights Reserved.

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
 * Generic setup routines for Broadcom 963xx MIPS boards
 */

//#include <linux/config.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kdev_t.h>
#include <linux/types.h>
#include <linux/console.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/bootmem.h>

#include <asm/addrspace.h>
#include <asm/bcache.h>
#include <asm/irq.h>
#include <asm/time.h>
#include <asm/reboot.h>
//#include <asm/gdb-stub.h>
#include <asm/bootinfo.h>
#include <asm/cpu.h>

#include <bcm_map_part.h>
#include <bcm_intr.h>
#include <board.h>
#include <boardparms.h>
#include <asm/brcmstb/brcmstb.h>

#if defined(CONFIG_BCM3385)
#define LINUX_BASE	(CONFIG_MIPS_BRCM_TEXT & 0xfff00000)
#define LINUX_MEM_SIZE	0x00800000
#elif defined(CONFIG_BCM3384)
#define LINUX_BASE	(CONFIG_MIPS_BRCM_TEXT & 0xfff00000)
#define LINUX_MEM_SIZE	0x01000000
#else
BCMChip not supported;
#endif

extern void send_halted_msg_to_ecos(void);
extern void send_restart_msg_to_ecos(void);
extern void send_poweroff_msg_to_ecos(void);
extern u32 __init cpu_speed(void);

void __init plat_mem_setup(void)
{
#if defined(CONFIG_BCM3385)
	add_memory_region(virt_to_phys((const void *)LINUX_BASE),
			  LINUX_MEM_SIZE,
			  BOOT_MEM_RAM);
	add_memory_region(0x16c00000, 0x01400000, BOOT_MEM_RAM);
#endif
}



unsigned long brcm_setup_ebase(void)
{
	u32 *tp1_relo_vector = (u32 *)0xff438000;
	write_c0_brcm_cbr(0xff40000c);
	*tp1_relo_vector = LINUX_BASE | (1 << 18);
	printk("relo vector: %08x\n", *tp1_relo_vector);
	return LINUX_BASE;
}

void __init plat_time_init(void)
{
	mips_hpt_frequency = cpu_speed() / 2;

	// Enable cp0 counter/compare interrupt only when not using power management
	write_c0_status(IE_IRQ5 | read_c0_status());
}

static void brcm_machine_restart(char *command)
{
	send_restart_msg_to_ecos();
	send_halted_msg_to_ecos();
}

static void brcm_machine_halt(void)
{
	send_halted_msg_to_ecos();
}

static void brcm_machine_poweroff(void)
{
	send_poweroff_msg_to_ecos();
	send_halted_msg_to_ecos();
}


static int __init brcm338x_setup(void)
{
    _machine_restart = brcm_machine_restart;
    _machine_halt = brcm_machine_halt;
    pm_power_off = brcm_machine_poweroff;

    return 0;
}

arch_initcall(brcm338x_setup);

