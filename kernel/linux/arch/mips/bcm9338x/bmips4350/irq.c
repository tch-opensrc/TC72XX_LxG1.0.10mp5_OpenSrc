/*
  <:copyright-gpl 
  Copyright 2013 Broadcom Corp. All Rights Reserved. 

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
	volatile IRQ_TYPE	*status;	/* status register */
	volatile IRQ_TYPE	*enable;	/* enable register */
	uint32 			vstart;		/* first virtual irq number */
	uint32			vend;		/* beyond the last irq number*/
	uint32			next_level_mask;/* bits indicate level 2 */
	struct irq_ctl		*next_level;	/* level 2 vector array */
	int			next_level_cnt;	/* size of level 2 array */
	struct irq_ctl		*lower_level;	/* if this l2, pointer to l1 */
	int			lower_level_bit;/* which l1 bit this l2 muxes*/
	uint32			cause_ip;	/* mips int mask in C0 */
} irq_ctl;

/******************************************************************************
 * SOC specific tables for irq routing and enumeration
 *****************************************************************************/

#if defined(CONFIG_BCM3385)

#define REGBASE		0xd0000000
#define REGADDR(x) 	(CPHYSADDR(x) | REGBASE)

/******************************************************************************
 * 3385
 *****************************************************************************/
static irq_ctl periph_ext_irqs;
static irq_ctl cmips_irqs =
{
	(IRQ_TYPE *)REGADDR(BCHP_CPU_COMM_REGS_CPUC_L2_IRQ_CMIPS1_STATUS),
	(IRQ_TYPE *)REGADDR(BCHP_CPU_COMM_REGS_CPUC_L2_IRQ_CMIPS1_MASK),
	CPU_COMM_INT_START,
	CPU_COMM_INT_END,
	0,
	0,
	0,
	&periph_ext_irqs,
	BCHP_INT_EXT_PER_PeriphIRQMASK_MSB0_CMIPS1_irq_SHIFT,
	0
};
static irq_ctl periph_irqs =
{
	(IRQ_TYPE *)REGADDR(BCHP_INT_PER_PeriphIRQSTATUS3),
	(IRQ_TYPE *)REGADDR(BCHP_INT_PER_PeriphIRQMASK3),
	PERIPH_INT_START,
	PERIPH_INT_END,
	BCHP_INT_EXT_PER_PeriphIRQSTATUS_MSB3_CMIPS1_irq_SHIFT,
	&cmips_irqs,
	1,
	0,
	0,
	CAUSEF_IP4
};
static irq_ctl periph_ext_irqs =
{
	(IRQ_TYPE *)REGADDR(BCHP_INT_EXT_PER_PeriphIRQSTATUS_MSB3),
	(IRQ_TYPE *)REGADDR(BCHP_INT_EXT_PER_PeriphIRQMASK_MSB3),
	PERIPH_EXT_INT_START,
	PERIPH_EXT_INT_END,
	BCHP_INT_EXT_PER_PeriphIRQSTATUS_MSB3_CMIPS1_irq_SHIFT,
	&cmips_irqs,
	1,
	0,
	0,
	CAUSEF_IP4
};

static irq_ctl *irq_table[] =
{
	&periph_irqs,
	&periph_ext_irqs,
	&cmips_irqs,
};

/******************************************************************************
 * end 3385
 *****************************************************************************/

#elif defined(CONFIG_BCM3384)

static irq_ctl periph_irqs;
static irq_ctl cmips_irqs =
{
	(IRQ_TYPE *)KSEG1ADDR(BCHP_UB_G2U_REGS_UB_L1_IRQ_CMIPS1_STATUS),
	(IRQ_TYPE *)KSEG1ADDR(BCHP_UB_G2U_REGS_UB_L1_IRQ_CMIPS1_MASK),
	CMIPS_INT_START,
	CMIPS_INT_END,
	0,
	0,
	0,
	&periph_irqs,
	BCHP_INT_PER_PeriphIRQSTATUS1_UGB_CMIPS1_IRQ_SHIFT,
	0
};

static irq_ctl periph_irqs =
{
	(IRQ_TYPE *)KSEG1ADDR(BCHP_INT_PER_PeriphIRQSTATUS3),
	(IRQ_TYPE *)KSEG1ADDR(BCHP_INT_PER_PeriphIRQMASK3),
	PERIPH_INT_START,
	PERIPH_INT_END,
	BCHP_INT_PER_PeriphIRQSTATUS1_UGB_CMIPS1_IRQ_SHIFT,
	&cmips_irqs,
	1,
	0,
	0,
	CAUSEF_IP4
};

static irq_ctl periph_ext_irqs =
{
	(IRQ_TYPE *)KSEG1ADDR(BCHP_INT_EXT_PER_PeriphIRQSTATUS3_2),
	(IRQ_TYPE *)KSEG1ADDR(BCHP_INT_EXT_PER_PeriphIRQMASK3_2),
	PERIPH_EXT_INT_START,
	PERIPH_EXT_INT_END,
	BCHP_INT_PER_PeriphIRQSTATUS1_UGB_CMIPS1_IRQ_SHIFT,
	&cmips_irqs,
	1,
	0,
	0,
	CAUSEF_IP4
};

static irq_ctl *irq_table[] =
{
	&periph_irqs,
	&periph_ext_irqs,
	&cmips_irqs,
};

/******************************************************************************
 * end 3384
 *****************************************************************************/
#endif

unsigned long perif_block_frequency = 54000000;	/* used by uart driver */
EXPORT_SYMBOL(perif_block_frequency);

static void irq_dispatch(irq_ctl *irqs)
{
	IRQ_TYPE pending, mask;
	int ibit;

	spin_lock(&brcm_irqlock);
	pending = *(irqs->status) & *(irqs->enable);
	spin_unlock(&brcm_irqlock);

	if (!pending)
		return;

	for (ibit = 0, mask = 1; ibit < (sizeof(IRQ_TYPE) * 8); ibit++, mask <<= 1) {
		if (pending & mask) {
			if ((pending & mask) & (1 << irqs->next_level_mask)) {
				irq_ctl *next = irqs->next_level;
				int i = 0;
				while (i < irqs->next_level_cnt) {
					irq_dispatch(next);
					i++;
					next++;
				}
			}
			else {
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

void enable_brcm_irq(unsigned int irq)
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
	if (irq <= MIPS_TIMER_INT)
	{
		spin_lock_irqsave(&brcm_irqlock, flags);
		clear_c0_status(1 << (STATUSB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
		spin_unlock_irqrestore(&brcm_irqlock, flags);
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

	if (irq <= MIPS_TIMER_INT)
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


	if (irq <= MIPS_TIMER_INT)
	{
		spin_lock_irqsave(&brcm_irqlock, flags);
		clear_c0_status(1 << (STATUSB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
		clear_c0_cause(1 << (CAUSEB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
		spin_unlock_irqrestore(&brcm_irqlock, flags);
	}
	else
		disable_periph_irq(irq);

}

#define ALLINTS_NOTIMER (IE_SW0 | IE_SW1 | IE_IRQ2)

static struct irq_chip brcm_irq_chip = {
    .name = "BMIPS4355",
    .enable = enable_brcm_irq,
    .disable = disable_brcm_irq,
    .ack = ack_brcm_irq,
    .mask = disable_brcm_irq,
    .mask_ack = mask_ack_brcm_irq,
    .unmask = enable_brcm_irq,
};

void __init brcm_route_irq(void)
{
   uint32 cp0Reg;
   uint32 crossover;

   if( ((unsigned int)read_c0_diag3() >> 31) == 0 )
   {
	   return;
   }
   else
   {
       /* When running on TP1, route MIPS INT 2 & 4 */
       crossover = (1<<2) | (1<<4);

       cp0Reg = (uint32)read_c0_diag1();
       write_c0_diag1((cp0Reg | (crossover << 27)));
   }
}

void __init arch_init_irq(void)
{
    int i;

    uint32_t chiprev = *((uint32_t *)KSEG1ADDR(BCHP_INT_PER_REVID));
	
    printk("Broadcom CHIP ID: %08X\n", chiprev);
    if (( chiprev & 0xf ) >= 8 )
    {	/* B0 */
	    perif_block_frequency = 54000000;
    }
    else
    {
	    perif_block_frequency = 73358490;
    }
    brcm_route_irq();
    for (i = 0; i < NR_IRQS; i++) {
        set_irq_chip_and_handler(i, &brcm_irq_chip, handle_level_irq);
    }
    clear_c0_status(ST0_BEV);
    change_c0_status(ST0_IM, ALLINTS_NOTIMER);
}


unsigned int BcmHalMapInterrupt(FN_HANDLER pfunc, unsigned int param, unsigned int irq)
{
    char *devname;
    unsigned long irqflags;

    devname = kmalloc(16, GFP_KERNEL);
    if (!devname) {
        return -1;
    }
    sprintf( devname, "brcm_%d", irq );

    set_irq_chip_and_handler(irq, &brcm_irq_chip, handle_level_irq);

    irqflags = IRQF_DISABLED | IRQF_SAMPLE_RANDOM;

    return request_irq(irq, (void*)pfunc, irqflags, devname, (void *) param);
}


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
