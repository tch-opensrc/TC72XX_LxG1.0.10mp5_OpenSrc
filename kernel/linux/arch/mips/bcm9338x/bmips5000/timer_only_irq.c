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

#define IRQ_TYPE uint32

spinlock_t brcm_irqlock;

typedef struct irq_ctl
{
	volatile IRQ_TYPE	*status;			/* status register */
	volatile IRQ_TYPE	*enable;			/* enable register */
	IRQ_TYPE			shadow_enable;
	uint32 				vstart;				/* first virtual irq number */
	uint32				vend;				/* beyond the last irq number */
	uint8				timer_based;		/* 0 or 1: cannot touch mask reg due to hw conflict*/
	uint8				cpu;				/* used for timer based only */
} irq_ctl;

static irq_ctl zmips_irqs =
{
	(IRQ_TYPE *)KSEG1ADDR(BCHP_G2U_REGS_GB_L1_IRQ_ZMIPS_STATUS),
	(IRQ_TYPE *)KSEG1ADDR(BCHP_G2U_REGS_GB_L1_IRQ_ZMIPS_MASK),
	0,
	ZMIPS_INT_START,
	ZMIPS_INT_END,
	1, /* timer based */
	0  /* cpu 0 */
};

static irq_ctl periph_irqs =
{
	(IRQ_TYPE *)KSEG1ADDR(BCHP_INT_PER_PeriphIRQSTATUS3),
	(IRQ_TYPE *)KSEG1ADDR(BCHP_INT_PER_PeriphIRQMASK3),
	0,
	PERIPH_INT_START,
	PERIPH_INT_END,
	1, /* timer based */
	0  /* cpu 1 */
};

static irq_ctl periph_ext_irqs =
{
	(IRQ_TYPE *)KSEG1ADDR(BCHP_INT_EXT_PER_PeriphIRQSTATUS3_2),
	(IRQ_TYPE *)KSEG1ADDR(BCHP_INT_EXT_PER_PeriphIRQMASK3_2),
	0,
	PERIPH_EXT_INT_START,
	PERIPH_EXT_INT_END,
	1, /* timer based */
	0  /* cpu 0 */
};

static irq_ctl iop_irqs =
{
	(IRQ_TYPE *)KSEG1ADDR(BCHP_INT_PER_IOPIRQSTATUS1),
	(IRQ_TYPE *)KSEG1ADDR(BCHP_INT_PER_IOPIRQMASK1),
	0,
	IOP_INT_START,
	IOP_INT_END,
	0, /* real irq */
	1  /* cpu 1 */
};
	
static irq_ctl *irq_table[] =
{
	&periph_irqs,
	&periph_ext_irqs,
	&zmips_irqs,
	&iop_irqs,
};

static void timer_irq_dispatch(irq_ctl *irqs)
{
    IRQ_TYPE pending, mask;
    int ibit;

    pending = *(irqs->status) & irqs->shadow_enable;

    if (!pending)
        return;

    for (ibit = 0, mask = 1; ibit < (sizeof(IRQ_TYPE) * 8); ibit++, mask <<= 1)
	{
        if (pending & mask)
		{
			do_IRQ(ibit + irqs->vstart);
        }
    }
}

static void irq_dispatch(irq_ctl *irqs)
{
    IRQ_TYPE pending, mask;
    int ibit;

    pending = *(irqs->status) & *(irqs->enable);

    if (!pending)
        return;

    for (ibit = 0, mask = 1; ibit < (sizeof(IRQ_TYPE) * 8); ibit++, mask <<= 1)
	{
        if (pending & mask)
		{
			do_IRQ(ibit + irqs->vstart);
        }
    }
}

asmlinkage void plat_irq_dispatch(void)
{
    u32 cause;

    while((cause = (read_c0_cause() & read_c0_status() & CAUSEF_IP))) {
        if (cause & CAUSEF_IP0)
		{
            do_IRQ(INTERRUPT_ID_SOFTWARE_0);
		}
        if (cause & CAUSEF_IP1)
		{
            do_IRQ(INTERRUPT_ID_SOFTWARE_1);
		}
		if (cause & CAUSEF_IP6)
		{
            irq_dispatch(&iop_irqs);
		}
		if (cause & CAUSEF_IP7)
		{
			int i = 0;
			while (i < (sizeof(irq_table) / sizeof(irq_ctl *)))
			{
				irq_ctl *irqs = irq_table[i];
				if (irqs->timer_based)
#if defined(CONFIG_SMP)
					if (get_cpu() == irqs->cpu)
#endif
						timer_irq_dispatch(irqs);
				i++;
			}
            do_IRQ(MIPS_TIMER_INT);
		}
    }
}

void enable_brcm_irq(unsigned int irq)
{
    unsigned long flags;
	int i, bit;

	if (irq == MIPS_TIMER_INT)
		return;
	if (irq < MIPS_TIMER_INT)
	{
		set_c0_status(1 << (STATUSB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
		return;
	}
	for (i = 0; i < (sizeof(irq_table)/sizeof(irq_ctl *)); i++)
	{
		if (irq >= irq_table[i]->vstart && irq < irq_table[i]->vend)
		{
			bit = irq - irq_table[i]->vstart;
			spin_lock_irqsave(&brcm_irqlock, flags);
			if (irq_table[i]->timer_based)
				irq_table[i]->shadow_enable |= (1 << bit);
			else
				*(irq_table[i]->enable) |= (1 << bit);
			spin_unlock_irqrestore(&brcm_irqlock, flags);
			return;
		}
	}
	printk("ERROR in %s: unable to enable irq %d\n", __func__, irq);
}

void disable_periph_irq(unsigned int irq)
{
    unsigned long flags;
	int i, bit;

	for (i = 0; i < (sizeof(irq_table)/sizeof(irq_ctl *)); i++)
	{
		if (irq >= irq_table[i]->vstart && irq < irq_table[i]->vend)
		{
			bit = irq - irq_table[i]->vstart;
			spin_lock_irqsave(&brcm_irqlock, flags);
			if (irq_table[i]->timer_based)
				irq_table[i]->shadow_enable &= ~(1 << bit);
			else
				*(irq_table[i]->enable) &= ~(1 << bit);
			spin_unlock_irqrestore(&brcm_irqlock, flags);
			return;
		}
	}
	printk("ERROR in %s: unable to disable irq %d\n", __func__, irq);
}

void disable_brcm_irq(unsigned int irq)
{
    unsigned long flags;

	if (irq == MIPS_TIMER_INT)
		return;
	if (irq < MIPS_TIMER_INT)
	{
		clear_c0_status(1 << (STATUSB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
	}
	else
	{
		disable_periph_irq(irq);
	}
}


void ack_brcm_irq(unsigned int irq)
{
    unsigned long flags;

	if (irq == MIPS_TIMER_INT)
		return;

	if (irq < MIPS_TIMER_INT)
	{
		spin_lock_irqsave(&brcm_irqlock, flags);
        clear_c0_cause(1 << (CAUSEB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
		spin_unlock_irqrestore(&brcm_irqlock, flags);
	}
	else
	{
		disable_periph_irq(irq);
	}
}


void mask_ack_brcm_irq(unsigned int irq)
{
    unsigned long flags;

	if (irq == MIPS_TIMER_INT)
		return;


	if (irq < MIPS_TIMER_INT)
	{
		spin_lock_irqsave(&brcm_irqlock, flags);
		clear_c0_status(1 << (STATUSB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
        clear_c0_cause(1 << (CAUSEB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
		spin_unlock_irqrestore(&brcm_irqlock, flags);
	}
	else
		disable_periph_irq(irq);

}


void unmask_brcm_irq_noop(unsigned int irq)
{
}

void set_brcm_affinity(unsigned int irq, const struct cpumask *mask)
{
}

#define ALLINTS_NOTIMER (IE_SW0 | IE_SW1 | IE_IRQ4)

static struct irq_chip brcm_irq_chip = {
    .name = "BCM338x",
    .enable = enable_brcm_irq,
    .disable = disable_brcm_irq,
    .ack = ack_brcm_irq,
    .mask = disable_brcm_irq,
    .mask_ack = mask_ack_brcm_irq,
    .unmask = enable_brcm_irq,
    .set_affinity = set_brcm_affinity
};

static struct irq_chip brcm_irq_chip_no_unmask = {
    .name = "BCM338x",
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

	printk("<<<< %s >>>>>\n", __func__);
    spin_lock_init(&brcm_irqlock);

    for (i = 0; i < NR_IRQS; i++) {
        set_irq_chip_and_handler(i, &brcm_irq_chip, handle_level_irq);
    }
    clear_c0_status(ST0_BEV);
    change_c0_status(ST0_IM, ALLINTS_NOTIMER);
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
