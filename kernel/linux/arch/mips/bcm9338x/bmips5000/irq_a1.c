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

extern spinlock_t brcm_irqlock;

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

void plat_irq_dispatch_a1(void)
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

void enable_brcm_irq_a1(unsigned int irq)
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

void disable_periph_irq_a1(unsigned int irq)
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
