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
#include <asm/brcmstb/brcmstb.h>

extern irqreturn_t brcm_timer_interrupt(int irq, void *dev_id);

#include <bcm_map_part.h>
#include <bcm_intr.h>
#include <board.h>
#include <boardparms.h>

#if defined(CONFIG_PCI)
#include <linux/pci.h>
#include <bcmpci.h>
#endif

extern void send_halted_msg_to_ecos(void);
extern void send_restart_msg_to_ecos(void);
extern void send_poweroff_msg_to_ecos(void);
extern void fpm_token_status(void);


const char *get_system_type(void)
{
	return "BCM3384 Cable Modem";
}
void __init prom_free_prom_memory(void)
{
}

char *cfgstrings[16] = { 
    "?",  "D",  "E",  "G", // 0-3
    "?",  "M",  "?",  "?", // 4-7
    "?",  "?",  "Z",  "MV",// 8-b
    "ED", "ZD", "MD", "MR",// c-f
};
#define NUM_REV_IDS 16
char *revstrings[NUM_REV_IDS] = {
    "A0",
    "A1",
    "A2",
    "A3",
    "A4",
    "A5",
    "A6",
    "A7",
    "B0",
    "B1",
    "B2",
    "B3",
    "B4",
    "B5",
    "B6",
    "B7",
};
#define REV_ID_IDX_B1 9

uint32 __init calculateCpuSpeed(void)
{
	/*
	  33843   : 800 MHz
	  3384 B0 : 800 MHz
	  3384 B1 : 1 GHz
	  3384 B2 : 1 GHz
	*/
	uint32 *int_per_revid = (uint32 *)0xb4e00000;
    uint32 idreg = *int_per_revid;
    uint16 chipid = idreg >> 16;
    uint8  cfgid  = idreg >> 4 & 0xf;
    uint8  revid  = idreg & 0xf;
	uint32 cpu_speed;
    if ( idreg == 0 )
    {
        printk( "Chip not OTP'ed\n" );
        return 800 * 1000 * 1000;
    }
    if ( revid >= NUM_REV_IDS )
    {
        revid = NUM_REV_IDS-1;
    }
	if (chipid == 0x3384 && revid >= REV_ID_IDX_B1)
		cpu_speed = 1000 * 1000 * 1000;
	else
		cpu_speed = 800 * 1000 * 1000;
	
    printk( "Chip ID: BCM%04x%s-%s %dMHZ\n", 
            (int) chipid, 
            cfgstrings[cfgid], 
            revstrings[revid],
			cpu_speed / (1000 * 1000));
	/* there's a console handover after this so put some space */
	printk("\r\n");
	return cpu_speed;
}

void __init plat_time_init(void)
{
    mips_hpt_frequency = calculateCpuSpeed() / 8;
    write_c0_status(IE_IRQ5 | read_c0_status());
}

static void brcm_machine_halt(void)
{
	extern void wait_forever(void);

	int cpu = get_cpu();

	if (cpu == 0)
	{
		fpm_token_status();
		send_halted_msg_to_ecos();
		smp_call_function_single(1, (void(*)(void *))brcm_machine_halt, 0, 0);
	}
	wait_forever();
}

static void brcm_machine_restart(char *command)
{
	send_restart_msg_to_ecos();
	brcm_machine_halt();
}

static void brcm_machine_poweroff(void)
{
	send_poweroff_msg_to_ecos();
	brcm_machine_halt();
}


static int __init brcm338x_setup(void)
{
    extern int panic_timeout;

    _machine_restart = brcm_machine_restart;
    _machine_halt = brcm_machine_halt;
    pm_power_off = brcm_machine_poweroff;

    panic_timeout = 1;

    return 0;
}

arch_initcall(brcm338x_setup);


#if 1

/***************************************************************************
 * C++ New and delete operator functions
 ***************************************************************************/

/* void *operator new(unsigned int sz) */
void *_Znwj(unsigned int sz)
{
    return( kmalloc(sz, GFP_KERNEL) );
}

/* void *operator new[](unsigned int sz)*/
void *_Znaj(unsigned int sz)
{
    return( kmalloc(sz, GFP_KERNEL) );
}

/* placement new operator */
/* void *operator new (unsigned int size, void *ptr) */
void *ZnwjPv(unsigned int size, void *ptr)
{
    return ptr;
}

/* void operator delete(void *m) */
void _ZdlPv(void *m)
{
    kfree(m);
}

/* void operator delete[](void *m) */
void _ZdaPv(void *m)
{
    kfree(m);
}

EXPORT_SYMBOL(_Znwj);
EXPORT_SYMBOL(_Znaj);
EXPORT_SYMBOL(ZnwjPv);
EXPORT_SYMBOL(_ZdlPv);
EXPORT_SYMBOL(_ZdaPv);

#endif

