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

extern unsigned long getMemorySize(void);
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

#if defined(CONFIG_BCM93380)
#if defined(CONFIG_BCM_LOT1) && defined(CONFIG_BCM_UPPER_MEM)
#define CM_SDRAM_SIZE 0x04000000 /* 64MB */
#define CM_SDRAM_BASE 0x04000000 /* 64MB */
#else
#define CM_SDRAM_SIZE 0x02000000 /* 32MB */
#endif
#if defined(CONFIG_BCM_RAMDISK)
#define CM_RESERVED_SDRAM_SIZE 0x00600000 /* 6MB */
#else
#if defined(CONFIG_BCM_T0_IDLE)
#define CM_RESERVED_SDRAM_SIZE 0x00200000 /* 2MB */
#else
#define CM_RESERVED_SDRAM_SIZE 0x00000000 /* 0MB */
#endif
#endif
#define CM_RESERVED_SDRAM_BASE (CM_SDRAM_SIZE - CM_RESERVED_SDRAM_SIZE)
#define ADSL_SDRAM_IMAGE_SIZE CM_RESERVED_SDRAM_SIZE
#elif defined(CONFIG_BCM93383)
#define CM_SDRAM_SIZE 0x04000000 /* 64MB */
#define CM_SDRAM_BASE 0x04000000 /* 64MB */
#if defined(CONFIG_BCM_RAMDISK)
#define CM_RESERVED_SDRAM_SIZE 0x01000000 /* 16MB */
#else
#define CM_RESERVED_SDRAM_SIZE 0x00200000 /* 2MB */
#endif
#define ADSL_SDRAM_IMAGE_SIZE CM_RESERVED_SDRAM_SIZE
#else
#include "softdsl/AdslCoreDefs.h"
#endif

#if defined(CONFIG_BCM_ENDPOINT_MODULE)
#include <dsp_mod_size.h>
#endif

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

void __init plat_mem_setup(void)
{
    add_memory_region(CM_SDRAM_BASE, (getMemorySize() - ADSL_SDRAM_IMAGE_SIZE), BOOT_MEM_RAM);
}


extern UINT32 __init calculateCpuSpeed(void);
#if defined(CONFIG_BCM_PWRMNGT) || defined(CONFIG_BCM_PWRMNGT_MODULE)
extern void BcmPwrMngtInitC0Speed (void);
#endif

void __init plat_time_init(void)
{
    /* JU: TBD: there was some special SMP handling added here in original kernel */
    mips_hpt_frequency = calculateCpuSpeed() / 2;
#if defined(CONFIG_BCM_PWRMNGT) || defined(CONFIG_BCM_PWRMNGT_MODULE)
    BcmPwrMngtInitC0Speed();
#else
    // Enable cp0 counter/compare interrupt only when not using power management
    write_c0_status(IE_IRQ5 | read_c0_status());
#endif
}

void __attribute__((weak)) fpm_token_status(void)
{
}

static void brcm_machine_restart(char *command)
{
	send_restart_msg_to_ecos();
	fpm_token_status();
	send_halted_msg_to_ecos();
}

static void brcm_machine_halt(void)
{
	fpm_token_status();
	send_halted_msg_to_ecos();
}

static void brcm_machine_poweroff(void)
{
	send_poweroff_msg_to_ecos();
	fpm_token_status();
	send_halted_msg_to_ecos();
}


static int __init bcm338x_hw_init(void)
{
}

arch_initcall(bcm338x_hw_init);


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


unsigned long getMemorySize(void)
{
    return CM_SDRAM_SIZE;
}

/* Pointers to memory buffers allocated for the DSP module */
void *dsp_core;
void *dsp_init;
EXPORT_SYMBOL(dsp_core);
EXPORT_SYMBOL(dsp_init);
void __init allocDspModBuffers(void);
/*
*****************************************************************************
** FUNCTION:   allocDspModBuffers
**
** PURPOSE:    Allocates buffers for the init and core sections of the DSP
**             module. This module is special since it has to be allocated
**             in the 0x800.. memory range which is not mapped by the TLB.
**
** PARAMETERS: None
** RETURNS:    Nothing
*****************************************************************************
*/
void __init allocDspModBuffers(void)
{
#if defined(CONFIG_BCM_ENDPOINT_MODULE)
    printk("Allocating memory for DSP module core and initialization code\n");

  dsp_core = (void*)((DSP_CORE_SIZE > 0) ? alloc_bootmem((unsigned long)DSP_CORE_SIZE) : NULL);
  dsp_init = (void*)((DSP_INIT_SIZE > 0) ? alloc_bootmem((unsigned long)DSP_INIT_SIZE) : NULL);

  printk("Allocated DSP module memory - CORE=0x%x SIZE=%d, INIT=0x%x SIZE=%d\n",
         (unsigned int)dsp_core, DSP_CORE_SIZE, (unsigned int)dsp_init , DSP_INIT_SIZE);
#endif
}


/* Pointers to memory buffers allocated for the FAP module */
void *fap_core;
void *fap_init;
EXPORT_SYMBOL(fap_core);
EXPORT_SYMBOL(fap_init);
void __init allocFapModBuffers(void);
/*
*****************************************************************************
** FUNCTION:   allocFapModBuffers
**
** PURPOSE:    Allocates buffers for the init and core sections of the FAP
**             module. This module is special since it has to be allocated
**             in the 0x800.. memory range which is not mapped by the TLB.
**
** PARAMETERS: None
** RETURNS:    Nothing
*****************************************************************************
*/
void __init allocFapModBuffers(void)
{
#if (defined(CONFIG_BCM96362) || defined(CHIP_6362)) && defined(CONFIG_BCM_FAP_MODULE)
    printk("********** Allocating memory for FAP module core and initialization code\n");

    fap_core = (void*)((FAP_CORE_SIZE > 0) ? alloc_bootmem((unsigned long)FAP_CORE_SIZE) : NULL);
    fap_init = (void*)((FAP_INIT_SIZE > 0) ? alloc_bootmem((unsigned long)FAP_INIT_SIZE) : NULL);

    printk("Allocated FAP module memory - CORE=0x%x SIZE=%d, INIT=0x%x SIZE=%d\n",
           (unsigned int)fap_core, FAP_CORE_SIZE, (unsigned int)fap_init , FAP_INIT_SIZE);
#endif
}
