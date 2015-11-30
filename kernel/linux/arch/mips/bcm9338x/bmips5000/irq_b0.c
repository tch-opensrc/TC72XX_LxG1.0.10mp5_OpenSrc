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
	uint32 				vstart;				/* first virtual irq number */
	uint32				vend;				/* beyond the last irq number */
	uint32				next_level_mask;	/* status bits indicate level 2 */
	struct irq_ctl		*next_level;		/* level 2 vector array */
	int					next_level_cnt;		/* size of level 2 vector array */
	struct irq_ctl		*lower_level;		/* if this is a level 2, pointer to level 1 */
	int					lower_level_bit;	/* which level 1 bit this level 2 muxes into */
	uint32				cause_ip;			/* mips int mask in C0 cause and status reg */
} irq_ctl;

static irq_ctl periph_irqs;

static irq_ctl zmips_irqs =
{
	(IRQ_TYPE *)KSEG1ADDR(BCHP_G2U_REGS_GB_L1_IRQ_ZMIPS_STATUS),
	(IRQ_TYPE *)KSEG1ADDR(BCHP_G2U_REGS_GB_L1_IRQ_ZMIPS_MASK),
	ZMIPS_INT_START,
	ZMIPS_INT_END,
	0,
	0,
	0,
	&periph_irqs,
	BCHP_INT_PER_PeriphIRQSTATUS2_UGB_ZMIPS_IRQ_SHIFT,
	0
};

static irq_ctl periph_irqs =
{
	(IRQ_TYPE *)KSEG1ADDR(BCHP_INT_PER_PeriphIRQSTATUS1),
	(IRQ_TYPE *)KSEG1ADDR(BCHP_INT_PER_PeriphIRQMASK1),
	PERIPH_INT_START,
	PERIPH_INT_END,
	BCHP_INT_PER_PeriphIRQSTATUS2_UGB_ZMIPS_IRQ_SHIFT,
	&zmips_irqs,
	1,
	0,
	0,
	CAUSEF_IP4
};

static irq_ctl periph_ext_irqs =
{
	(IRQ_TYPE *)KSEG1ADDR(BCHP_INT_EXT_PER_PeriphIRQSTATUS1_2),
	(IRQ_TYPE *)KSEG1ADDR(BCHP_INT_EXT_PER_PeriphIRQMASK1_2),
	PERIPH_EXT_INT_START,
	PERIPH_EXT_INT_END,
	0,
	0,
	0,
	0,
	0,
	CAUSEF_IP4
};

static irq_ctl iop_irqs =
{
	(IRQ_TYPE *)KSEG1ADDR(BCHP_INT_PER_IOPIRQSTATUS1),
	(IRQ_TYPE *)KSEG1ADDR(BCHP_INT_PER_IOPIRQMASK1),
	IOP_INT_START,
	IOP_INT_END,
	0,
	0,
	0,
	0,
	0,
	CAUSEF_IP6
};
	
	
	

static irq_ctl *irq_table[] =
{
	&periph_irqs,
	&periph_ext_irqs,
	&zmips_irqs,
	&iop_irqs
};

static void irq_dispatch(irq_ctl *irqs)
{
    IRQ_TYPE pending, mask;
    int ibit;

    spin_lock(&brcm_irqlock);
    pending = *(irqs->status) & *(irqs->enable);
    spin_unlock(&brcm_irqlock);

    if (!pending)
        return;

    for (ibit = 0, mask = 1; ibit < (sizeof(IRQ_TYPE) * 8); ibit++, mask <<= 1)
	{
        if (pending & mask) {
			if ((pending & mask) & (1 << irqs->next_level_mask))
			{
				irq_ctl *next = irqs->next_level;
				int i = 0;
				while (i < irqs->next_level_cnt)
				{
					irq_dispatch(next);
					i++;
					next++;
				}
			}
			else
			{
				do_IRQ(ibit + irqs->vstart);
			}
        }
    }
}

static void irq_dispatch_int(uint32 cause_ip)
{
    int i;

	for (i = 0; i < (sizeof(irq_table)/sizeof(irq_ctl *)); i++)
		if (cause_ip == irq_table[i]->cause_ip)
			irq_dispatch(irq_table[i]);
}

void plat_irq_dispatch_b0(void)
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
		if (cause & CAUSEF_IP2)
		{
            irq_dispatch_int(CAUSEF_IP2);
		}
		if (cause & CAUSEF_IP3)
		{
            irq_dispatch_int(CAUSEF_IP3);
		}
		if (cause & CAUSEF_IP4)
		{
            irq_dispatch_int(CAUSEF_IP4);
		}
		if (cause & CAUSEF_IP5)
		{
            irq_dispatch_int(CAUSEF_IP5);
		}
		if (cause & CAUSEF_IP6)
		{
            irq_dispatch_int(CAUSEF_IP6);
		}
		if (cause & CAUSEF_IP7)
		{
            do_IRQ(MIPS_TIMER_INT);
		}
    }
}

void enable_brcm_irq_b0(unsigned int irq)
{
    unsigned long flags;
	int i, bit;

	if (irq <= MIPS_TIMER_INT)
	{
		if (irq == MIPS_TIMER_INT)
			return;
		set_c0_status(1 << (STATUSB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
		return;
	}
	for (i = 0; i < (sizeof(irq_table)/sizeof(irq_ctl *)); i++)
	{
		if (irq >= irq_table[i]->vstart && irq < irq_table[i]->vend)
		{
			bit = irq - irq_table[i]->vstart;
			spin_lock_irqsave(&brcm_irqlock, flags);
			*(irq_table[i]->enable) |= (1 << bit);
			if (irq_table[i]->lower_level)
			{
				*(irq_table[i]->lower_level->enable) |= 1 << irq_table[i]->lower_level_bit;
			}
			spin_unlock_irqrestore(&brcm_irqlock, flags);
			return;
		}
	}
	printk("ERROR in %s: unable to enable irq %d\n", __func__, irq);
}

void disable_periph_irq_b0(unsigned int irq)
{
    unsigned long flags;
	int i, bit;

	for (i = 0; i < (sizeof(irq_table)/sizeof(irq_ctl *)); i++)
	{
		if (irq >= irq_table[i]->vstart && irq < irq_table[i]->vend)
		{
			bit = irq - irq_table[i]->vstart;
			spin_lock_irqsave(&brcm_irqlock, flags);
			*(irq_table[i]->enable) &= ~(1 << bit);
			spin_unlock_irqrestore(&brcm_irqlock, flags);
			return;
		}
	}
	printk("ERROR in %s: unable to disable irq %d\n", __func__, irq);
}
