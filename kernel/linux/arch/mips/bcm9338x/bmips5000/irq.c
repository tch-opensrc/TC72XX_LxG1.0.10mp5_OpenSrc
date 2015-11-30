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
 * Interrupt control functions for Broadcom 963xx MIPS boards
 */

#include <asm/atomic.h>

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/linkage.h>

#include <asm/irq.h>
#include <asm/mipsregs.h>
#include <asm/addrspace.h>
#include <asm/signal.h>
#include <bcm_map_part.h>
#include <bcm_intr.h>
#include <asm/brcmstb/brcmstb.h>

spinlock_t brcm_irqlock;

/* 3384 B0 */
extern void plat_irq_dispatch_b0(void);
extern void enable_brcm_irq_b0(unsigned int irq);
extern void disable_periph_irq_b0(unsigned int irq);

/* 3384 A1 */
extern void plat_irq_dispatch_a1(void);
extern void enable_brcm_irq_a1(unsigned int irq);
extern void disable_periph_irq_a1(unsigned int irq);

static void (*plat_irq_dispatch_func)(void);
static void (*enable_brcm_irq_func)(unsigned int irq);
static void (*disable_periph_irq_func)(unsigned int irq);

unsigned long perif_block_frequency;	/* used by uart driver */
EXPORT_SYMBOL(perif_block_frequency);

void enable_brcm_irq(unsigned int irq)
{
	enable_brcm_irq_func(irq);
}

void disable_brcm_irq(unsigned int irq)
{
    unsigned long flags;

	if (irq == MIPS_TIMER_INT)
		return;
	if (irq <= MIPS_TIMER_INT)
	{
		spin_lock_irqsave(&brcm_irqlock, flags);
		clear_c0_status(1 << (STATUSB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
		spin_unlock_irqrestore(&brcm_irqlock, flags);
	}
	else
	{
		disable_periph_irq_func(irq);
	}
}


void ack_brcm_irq(unsigned int irq)
{
    unsigned long flags;

	if (irq == MIPS_TIMER_INT)
		return;

	if (irq <= MIPS_TIMER_INT)
	{
		spin_lock_irqsave(&brcm_irqlock, flags);
        clear_c0_cause(1 << (CAUSEB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
		spin_unlock_irqrestore(&brcm_irqlock, flags);
	}
	else
	{
		disable_periph_irq_func(irq);
	}
}


void mask_ack_brcm_irq(unsigned int irq)
{
    unsigned long flags;

	if (irq == MIPS_TIMER_INT)
		return;


	if (irq <= MIPS_TIMER_INT)
	{
		spin_lock_irqsave(&brcm_irqlock, flags);
		clear_c0_status(1 << (STATUSB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
        clear_c0_cause(1 << (CAUSEB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
		spin_unlock_irqrestore(&brcm_irqlock, flags);
	}
	else
		disable_periph_irq_func(irq);

}


void unmask_brcm_irq_noop(unsigned int irq)
{
}

void set_brcm_affinity(unsigned int irq, const struct cpumask *mask)
{
}

asmlinkage void plat_irq_dispatch(void)
{
	plat_irq_dispatch_func();
}

#define ALLINTS_NOTIMER_B0 (IE_SW0 | IE_SW1 | IE_IRQ2 | IE_IRQ3 | IE_IRQ4)
#define ALLINTS_NOTIMER_A1 (IE_SW0 | IE_SW1 | IE_IRQ4)

static struct irq_chip brcm_irq_chip = {
    .name = "BCM3384",
    .enable = enable_brcm_irq,
    .disable = disable_brcm_irq,
    .ack = ack_brcm_irq,
    .mask = disable_brcm_irq,
    .mask_ack = mask_ack_brcm_irq,
    .unmask = enable_brcm_irq,
    .set_affinity = set_brcm_affinity
};

static struct irq_chip brcm_irq_chip_no_unmask = {
    .name = "BCM3384",
    .enable = enable_brcm_irq,
    .disable = disable_brcm_irq,
    .ack = ack_brcm_irq,
    .mask = disable_brcm_irq,
    .mask_ack = mask_ack_brcm_irq,
    .unmask = unmask_brcm_irq_noop,
    .set_affinity = set_brcm_affinity
};

void __init arch_init_irq(void)
{
    int i;
	uint32_t allints_notimer;
	uint32_t chiprev = *((uint32_t *)KSEG1ADDR(BCHP_INT_PER_REVID));
	
	printk("Broadcom CHIP ID: %08X\n", chiprev);
	if (( chiprev & 0xf ) >= 8 )
	{	/* B0 */
		plat_irq_dispatch_func =  plat_irq_dispatch_b0;
		enable_brcm_irq_func =    enable_brcm_irq_b0;
		disable_periph_irq_func = disable_periph_irq_b0;
		allints_notimer = ALLINTS_NOTIMER_B0;
		perif_block_frequency = 54000000;
	}
	else
	{
		plat_irq_dispatch_func =  plat_irq_dispatch_a1;
		enable_brcm_irq_func =    enable_brcm_irq_a1;
		disable_periph_irq_func = disable_periph_irq_a1;
		allints_notimer = ALLINTS_NOTIMER_A1;
		perif_block_frequency = 73358490;
	}

    for (i = 0; i < NR_IRQS; i++) {
        set_irq_chip_and_handler(i, &brcm_irq_chip, handle_level_irq);
    }
    clear_c0_status(ST0_BEV);
    change_c0_status(ST0_IM, allints_notimer);
#if defined(CONFIG_SMP)
	/* send most HW interrupts to TP1 (primary boot) */
	clear_c0_brcm_mode(0x07 << 27);
	/*
	 * send GFAP DQM ints to TP0 (secondary)
	 * external hw int lines 3+4 (iop line 0+1)
	 */
	set_c0_brcm_mode(~0x07 << 27);
#else
	/* send all HW interrupts to TP0 */
	clear_c0_brcm_mode(0x1f << 27);
#endif	
}


// This is a wrapper to standand Linux request_irq
// Differences are:
//    - The irq won't be renabled after ISR is done and needs to be explicity re-enabled, which is good for NAPI drivers.
//      The change is implemented by filling in an no-op unmask function in brcm_irq_chip_no_unmask and set it as the irq_chip
//    - IRQ flags and interrupt names are automatically set
// Either request_irq or BcmHalMapInterrupt can be used. Just make sure re-enabling IRQ is handled correctly.

unsigned int BcmHalMapInterrupt(FN_HANDLER pfunc, unsigned int param, unsigned int irq)
{
    char *devname;
    unsigned long irqflags;

    devname = kmalloc(16, GFP_KERNEL);
    if (!devname) {
        return -1;
    }
    sprintf( devname, "brcm_%d", irq );

    set_irq_chip_and_handler(irq, &brcm_irq_chip_no_unmask, handle_level_irq);

    irqflags = IRQF_DISABLED | IRQF_SAMPLE_RANDOM;

    return request_irq(irq, (void*)pfunc, irqflags, devname, (void *) param);
}


//***************************************************************************
//  void  BcmHalGenerateSoftInterrupt
//
//   Triggers a software interrupt.
//
//***************************************************************************
void BcmHalGenerateSoftInterrupt( unsigned int irq )
{
    unsigned long flags;

    local_irq_save(flags);

    set_c0_cause(0x1 << (CAUSEB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));

    local_irq_restore(flags);
}


EXPORT_SYMBOL(enable_brcm_irq);
EXPORT_SYMBOL(disable_brcm_irq);
EXPORT_SYMBOL(BcmHalMapInterrupt);
EXPORT_SYMBOL(BcmHalGenerateSoftInterrupt);
