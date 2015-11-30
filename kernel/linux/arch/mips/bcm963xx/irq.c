/******************************************************************************
 * Copyright 2009-2012 Broadcom Corporation
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation (the "GPL").
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * A copy of the GPL is available at
 * http://www.broadcom.com/licenses/GPLv2.php,
 * or by writing to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA
 *
 *****************************************************************************/
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


#if defined(CONFIG_SMP)
    #define AFFINITY_OF(d) (*(d)->affinity)
#else
    #define AFFINITY_OF(d) ((void)(d), CPU_MASK_CPU0)
#endif

#if IRQ_BITS == 64
    #define IRQ_TYPE uint64
#else
    #define IRQ_TYPE uint32
#endif

#if !defined(CONFIG_BCM93380) && !defined(CONFIG_BCMKILAUEA) && !defined(CONFIG_BCM93383)
volatile IrqControl_t * brcm_irq_ctrl[NR_CPUS];
#endif
spinlock_t brcm_irqlock;

#if defined(CONFIG_SMP)
extern DEFINE_PER_CPU(unsigned int, ipi_pending);
#endif

#if defined(CONFIG_BCM93380) || defined(CONFIG_BCMKILAUEA) || defined(CONFIG_BCM93383)
struct _int_ctl_ 
{
	volatile uint32 *pStatus; 
	volatile uint32 *pMask; 
	uint32 offset;
};

static struct _int_ctl_ *pperf = 0;
#if defined(CONFIG_BCM93380)
static struct _int_ctl_ perf[5] =
{
    {&PERF->PeriphIrq[2].iStatus, &PERF->PeriphIrq[2].iMask, INTERNAL_ISR_TABLE_OFFSET},
    {&PERF->IopIrq[0].iStatus, &PERF->IopIrq[0].iMask, ((32 * 1) + INTERNAL_ISR_TABLE_OFFSET)},
    {&PERF->DocsisIrq[2].iStatus, &PERF->DocsisIrq[2].iMask, ((32 * 2) + INTERNAL_ISR_TABLE_OFFSET)},
    {&PERF->PeriphIrq[3].iStatus, &PERF->PeriphIrq[3].iMask, INTERNAL_ISR_TABLE_OFFSET},
    {&PERF->IopIrq[1].iStatus, &PERF->IopIrq[1].iMask, ((32 * 1) + INTERNAL_ISR_TABLE_OFFSET)}
};
#elif defined(CONFIG_BCMKILAUEA)
static struct _int_ctl_ perf[5] =
{
    /* IRQ's from IOP periph space */
    {(volatile uint32 *)0xb6001004, (volatile uint32 *)0xb6001000, INTERNAL_ISR_TABLE_OFFSET},
    /* IOP interrupts from periph */
    {&PERF->IopIrq[1].iStatus, &PERF->IopIrq[1].iMask, ((32 * 1) + INTERNAL_ISR_TABLE_OFFSET)},
    /* DOCSIS interrupts from periph */
    {&PERF->DocsisIrq[0].iStatus, &PERF->DocsisIrq[2].iMask, ((32 * 2) + INTERNAL_ISR_TABLE_OFFSET)},
    /* Periph interrupts from periph */
    {&PERF->PeriphIrq[0].iStatus, &PERF->PeriphIrq[2].iMask, ((32 * 3) + INTERNAL_ISR_TABLE_OFFSET)}
};
#elif defined(CONFIG_BCM93383)
static struct _int_ctl_ perf[5] =
{
    /* Periph interrupts from periph */
    {(volatile uint32*)0xb4e00034, (volatile uint32 *)0xb4e00030, INTERNAL_ISR_TABLE_OFFSET},
    /* IRQ's from IOP periph space */
    {(volatile uint32 *)0xb6001004, (volatile uint32 *)0xb6001000, ((32 * 1) + INTERNAL_ISR_TABLE_OFFSET)},
    /* IOP interrupts from periph */
    {&PERF->IopIrq[1].iStatus, &PERF->IopIrq[1].iMask, ((32 * 2) + INTERNAL_ISR_TABLE_OFFSET)},
    /* DOCSIS interrupts from periph */
    {&PERF->DocsisIrq[0].iStatus, &PERF->DocsisIrq[0].iMask, ((32 * 2) + INTERNAL_ISR_TABLE_OFFSET)},
#if 0
    /* IRQ's from IOP periph space */
    {(volatile uint32 *)0xb6001004, (volatile uint32 *)0xb6001000, INTERNAL_ISR_TABLE_OFFSET},
    /* IOP interrupts from periph */
    {&PERF->IopIrq[1].iStatus, &PERF->IopIrq[1].iMask, ((32 * 1) + INTERNAL_ISR_TABLE_OFFSET)},
    /* DOCSIS interrupts from periph */
    {&PERF->DocsisIrq[0].iStatus, &PERF->DocsisIrq[2].iMask, ((32 * 2) + INTERNAL_ISR_TABLE_OFFSET)},
    /* Periph interrupts from periph */
    {&PERF->PeriphIrq[0].iStatus, &PERF->PeriphIrq[0].iMask, ((32 * 3) + INTERNAL_ISR_TABLE_OFFSET)}
#endif
};
#endif
extern void prom_putchar(char);
static void irq_dispatch_int(int i)
{
    IRQ_TYPE pendingIrqs;
    static IRQ_TYPE irqBit;

    static uint32 isrNumber = (sizeof(irqBit) * 8) - 1;

    spin_lock(&brcm_irqlock);
    pendingIrqs = (*perf[i].pStatus) & (*perf[i].pMask);
    spin_unlock(&brcm_irqlock);

    if (!pendingIrqs) {
        return;
    }

    while (1) {
        irqBit <<= 1;
        isrNumber++;
        if (isrNumber == (sizeof(irqBit) * 8)) {
            isrNumber = 0;
            irqBit = 0x1;
        }
        if (pendingIrqs & irqBit) {
            unsigned int irq = isrNumber + perf[i].offset;
            do_IRQ(irq);
            break;
        }
    }
}
#else
static void irq_dispatch_int(void)
{
    int cpu = smp_processor_id();
    IRQ_TYPE pendingIrqs;
    static IRQ_TYPE irqBit;

    static uint32 isrNumber = (sizeof(irqBit) * 8) - 1;

    spin_lock(&brcm_irqlock);
    pendingIrqs = brcm_irq_ctrl[cpu]->IrqStatus & brcm_irq_ctrl[cpu]->IrqMask;
    spin_unlock(&brcm_irqlock);

    if (!pendingIrqs) {
        return;
    }

    while (1) {
        irqBit <<= 1;
        isrNumber++;
        if (isrNumber == (sizeof(irqBit) * 8)) {
            isrNumber = 0;
            irqBit = 0x1;
        }
        if (pendingIrqs & irqBit) {
            unsigned int irq = isrNumber + INTERNAL_ISR_TABLE_OFFSET;
            spin_lock(&brcm_irqlock);
            if (irq >= INTERRUPT_ID_EXTERNAL_0 && irq <= INTERRUPT_ID_EXTERNAL_3) {
                PERF->ExtIrqCfg |= (1 << (irq - INTERRUPT_ID_EXTERNAL_0 + EI_CLEAR_SHFT));      // Clear
            }
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816)
            else if (irq >= INTERRUPT_ID_EXTERNAL_4 && irq <= INTERRUPT_ID_EXTERNAL_5) {
                PERF->ExtIrqCfg1 |= (1 << (irq - INTERRUPT_ID_EXTERNAL_4 + EI_CLEAR_SHFT));      // Clear
            }
#endif
            spin_unlock(&brcm_irqlock);
            do_IRQ(irq);
            break;
        }
    }
}
#endif

#if defined(CONFIG_BCM_PWRMNGT)	|| defined(CONFIG_BCM_PWRMNGT_MODULE)
extern void BcmPwrMngtResumeFullSpeed (void);
#endif

asmlinkage void plat_irq_dispatch(void)
{
    u32 cause;

#if defined(CONFIG_BCM_PWRMNGT)	|| defined(CONFIG_BCM_PWRMNGT_MODULE)
//    BcmPwrMngtResumeFullSpeed();
#endif

    while((cause = (read_c0_cause() & read_c0_status() & CAUSEF_IP))) {
        if (cause & CAUSEF_IP7)
        {
            do_IRQ(MIPS_TIMER_INT);
        }
        else if (cause & CAUSEF_IP0)
            do_IRQ(INTERRUPT_ID_SOFTWARE_0);
        else if (cause & CAUSEF_IP1)
            do_IRQ(INTERRUPT_ID_SOFTWARE_1);
#if defined(CONFIG_BCM93380)
#if defined(CONFIG_BCM_LOT1)
	    else if (cause & CAUSEF_IP4)
            irq_dispatch_int(3);
        else if (cause & CAUSEF_IP6)
            irq_dispatch_int(4);
#endif
	    else if (cause & CAUSEF_IP3)
            irq_dispatch_int(0);
        else if (cause & CAUSEF_IP5)
            irq_dispatch_int(1);
#elif defined(CONFIG_BCMKILAUEA) || defined(CONFIG_BCM93383)
#if 0
        else if (cause & CAUSEF_IP2)
            irq_dispatch_int(1);
        else if (cause & CAUSEF_IP3)
            irq_dispatch_int(1);
        else if (cause & CAUSEF_IP4)
            irq_dispatch_int(1);
#endif
        else if (cause & CAUSEF_IP5)
            irq_dispatch_int(0);
	else if (cause & CAUSEF_IP2)
	    irq_dispatch_int(1);
	
#else
#if defined (CONFIG_SMP)
        else if (cause & (CAUSEF_IP2 | CAUSEF_IP3))
#else 
        else if (cause & CAUSEF_IP2)
#endif
            irq_dispatch_int();
#endif
    }
}


#if defined(CONFIG_BCM93380) || defined(CONFIG_BCMKILAUEA) || defined(CONFIG_BCM93383)
void enable_brcm_irq(unsigned int irq)
{
    unsigned long flags;
    
    if( irq == MIPS_TIMER_INT ) {
//        *(unsigned int *)0 = 0;
        return;
    }

/*    local_irq_save(flags); */
    spin_lock_irqsave(&brcm_irqlock, flags);

    if( irq >= INTERNAL_ISR_TABLE_OFFSET ) {
		irq -= INTERNAL_ISR_TABLE_OFFSET;
		*pperf[irq/32].pMask |= (1 << (irq%32));
    }
    else if ((irq == INTERRUPT_ID_SOFTWARE_0) || (irq == INTERRUPT_ID_SOFTWARE_1)) {
        set_c0_status(0x1 << (STATUSB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
    }
/*    local_irq_restore(flags); */

    spin_unlock_irqrestore(&brcm_irqlock, flags);
}
#else
void enable_brcm_irq(unsigned int irq)
{
    int cpu;
    struct irq_desc *desc = irq_desc + irq;
    unsigned long flags;

    spin_lock_irqsave(&brcm_irqlock, flags);

    if( irq >= INTERNAL_ISR_TABLE_OFFSET ) {
        for_each_cpu_mask(cpu, AFFINITY_OF(desc)) {
            brcm_irq_ctrl[cpu]->IrqMask |= (((IRQ_TYPE)1)  << (irq - INTERNAL_ISR_TABLE_OFFSET));
        }
    }
    else if ((irq == INTERRUPT_ID_SOFTWARE_0) || (irq == INTERRUPT_ID_SOFTWARE_1)) {
        set_c0_status(0x1 << (STATUSB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
    }

    if (irq >= INTERRUPT_ID_EXTERNAL_0 && irq <= INTERRUPT_ID_EXTERNAL_3) {
        PERF->ExtIrqCfg &= ~(1 << (irq - INTERRUPT_ID_EXTERNAL_0 + EI_INSENS_SHFT));    // Edge insesnsitive
        PERF->ExtIrqCfg |= (1 << (irq - INTERRUPT_ID_EXTERNAL_0 + EI_LEVEL_SHFT));      // Level triggered
        PERF->ExtIrqCfg &= ~(1 << (irq - INTERRUPT_ID_EXTERNAL_0 + EI_SENSE_SHFT));     // Low level
        PERF->ExtIrqCfg |= (1 << (irq - INTERRUPT_ID_EXTERNAL_0 + EI_CLEAR_SHFT));      // Clear
        PERF->ExtIrqCfg |= (1 << (irq - INTERRUPT_ID_EXTERNAL_0 + EI_MASK_SHFT));       // Unmask
    }
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816)
    else if (irq >= INTERRUPT_ID_EXTERNAL_4 && irq <= INTERRUPT_ID_EXTERNAL_5) {
        PERF->ExtIrqCfg1 &= ~(1 << (irq - INTERRUPT_ID_EXTERNAL_4 + EI_INSENS_SHFT));    // Edge insesnsitive
        PERF->ExtIrqCfg1 |= (1 << (irq - INTERRUPT_ID_EXTERNAL_4 + EI_LEVEL_SHFT));      // Level triggered
        PERF->ExtIrqCfg1 &= ~(1 << (irq - INTERRUPT_ID_EXTERNAL_4 + EI_SENSE_SHFT));     // Low level
        PERF->ExtIrqCfg1 |= (1 << (irq - INTERRUPT_ID_EXTERNAL_4 + EI_CLEAR_SHFT));      // Clear
        PERF->ExtIrqCfg1 |= (1 << (irq - INTERRUPT_ID_EXTERNAL_4 + EI_MASK_SHFT));       // Unmask
    }
#endif

    spin_unlock_irqrestore(&brcm_irqlock, flags);
}
#endif
#if defined(CONFIG_BCM93380) || defined(CONFIG_BCMKILAUEA) || defined(CONFIG_BCM93383)
void __disable_ack_brcm_irq(unsigned int irq)
{
    if( irq == MIPS_TIMER_INT ) {
        return;
    }

    if( irq >= INTERNAL_ISR_TABLE_OFFSET ) {
	irq -= INTERNAL_ISR_TABLE_OFFSET;
	*pperf[irq/32].pMask &= ~(1 << (irq%32));
    }
/*
    else if ((irq == INTERRUPT_ID_SOFTWARE_0) || (irq == INTERRUPT_ID_SOFTWARE_1)) {
        clear_c0_status(0x1 << (STATUSB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
    }
*/
}
#else
void __disable_ack_brcm_irq(unsigned int irq)
{
    int cpu;
    struct irq_desc *desc = irq_desc + irq;

    if( irq >= INTERNAL_ISR_TABLE_OFFSET ) {
        for_each_cpu_mask(cpu, AFFINITY_OF(desc)) {
            brcm_irq_ctrl[cpu]->IrqMask &= ~(((IRQ_TYPE)1) << (irq - INTERNAL_ISR_TABLE_OFFSET));
        }
    }
}
#endif

void disable_brcm_irq(unsigned int irq)
{
    unsigned long flags;

    spin_lock_irqsave(&brcm_irqlock, flags);
    __disable_ack_brcm_irq(irq);
    if ((irq == INTERRUPT_ID_SOFTWARE_0) || (irq == INTERRUPT_ID_SOFTWARE_1)) {
        clear_c0_status(0x1 << (STATUSB_IP0 + irq - INTERRUPT_ID_SOFTWARE_0));
    }
    spin_unlock_irqrestore(&brcm_irqlock, flags);
}


void ack_brcm_irq(unsigned int irq)
{
    unsigned long flags;

    spin_lock_irqsave(&brcm_irqlock, flags);
    __disable_ack_brcm_irq(irq);

#if defined(CONFIG_SMP)
    if (irq == INTERRUPT_ID_SOFTWARE_0) {
        int this_cpu = smp_processor_id();
        int other_cpu = !this_cpu;
        per_cpu(ipi_pending, this_cpu) = 0;
        clear_c0_cause(1<<CAUSEB_IP0);
        if (per_cpu(ipi_pending, other_cpu)) {
            set_c0_cause(1<<CAUSEB_IP0);
        }
    }
#else
    if (irq == INTERRUPT_ID_SOFTWARE_0) {
        clear_c0_cause(1<<CAUSEB_IP0);
    }
#endif

    if (irq == INTERRUPT_ID_SOFTWARE_1) {
        clear_c0_cause(1<<CAUSEB_IP1);
    }

    spin_unlock_irqrestore(&brcm_irqlock, flags);
}


void mask_ack_brcm_irq(unsigned int irq)
{
    unsigned long flags;

    spin_lock_irqsave(&brcm_irqlock, flags);
    __disable_ack_brcm_irq(irq);

#if defined(CONFIG_SMP)
    if (irq == INTERRUPT_ID_SOFTWARE_0) {
        int this_cpu = smp_processor_id();
        int other_cpu = !this_cpu;
        per_cpu(ipi_pending, this_cpu) = 0;
        mb();
        clear_c0_cause(1<<CAUSEB_IP0);
        if (per_cpu(ipi_pending, other_cpu)) {
            set_c0_cause(1<<CAUSEB_IP0);
        }
        clear_c0_status(1<<STATUSB_IP0);
    }
#else
    if (irq == INTERRUPT_ID_SOFTWARE_0) {
        clear_c0_status(1<<STATUSB_IP0);
        clear_c0_cause(1<<CAUSEB_IP0);
    }
#endif

    if (irq == INTERRUPT_ID_SOFTWARE_1) {
        clear_c0_status(1<<STATUSB_IP1);
        clear_c0_cause(1<<CAUSEB_IP1);
    }

    spin_unlock_irqrestore(&brcm_irqlock, flags);
}


void unmask_brcm_irq_noop(unsigned int irq)
{
}

#if defined(CONFIG_BCM93380) || defined(CONFIG_BCMKILAUEA) || defined(CONFIG_BCM93383)
void set_brcm_affinity(unsigned int irq, const struct cpumask *mask)
{
}
#else
void set_brcm_affinity(unsigned int irq, const struct cpumask *mask)
{
    int cpu;
    struct irq_desc *desc = irq_desc + irq;
    unsigned long flags;

    spin_lock_irqsave(&brcm_irqlock, flags);

    if( irq >= INTERNAL_ISR_TABLE_OFFSET ) {
        for_each_online_cpu(cpu) {
            if (cpu_isset(cpu, *mask) && !(desc->status & IRQ_DISABLED)) {
                brcm_irq_ctrl[cpu]->IrqMask |= (((IRQ_TYPE)1)  << (irq - INTERNAL_ISR_TABLE_OFFSET));
            }
            else {
                brcm_irq_ctrl[cpu]->IrqMask &= ~(((IRQ_TYPE)1) << (irq - INTERNAL_ISR_TABLE_OFFSET));
            }
        }
    }

    spin_unlock_irqrestore(&brcm_irqlock, flags);
}
#endif

#if defined(CONFIG_BCM93380)
#if defined(CONFIG_BCM_LOT1)
#define ALLINTS_NOTIMER_T1 (IE_SW0 | IE_SW1 | IE_IRQ2 | IE_IRQ4)
#endif
#define ALLINTS_NOTIMER (IE_SW0 | IE_SW1 | IE_IRQ1 | IE_IRQ3)
#elif defined(CONFIG_BCM93383)
#define ALLINTS_NOTIMER (IE_IRQ0  | IE_IRQ1 | IE_IRQ2 | IE_IRQ3)
#else
#define ALLINTS_NOTIMER IE_IRQ0
#endif

static struct irq_chip brcm_irq_chip = {
    .name = "BCM63xx",
    .enable = enable_brcm_irq,
    .disable = disable_brcm_irq,
    .ack = ack_brcm_irq,
    .mask = disable_brcm_irq,
    .mask_ack = mask_ack_brcm_irq,
    .unmask = enable_brcm_irq,
    .set_affinity = set_brcm_affinity
};

static struct irq_chip brcm_irq_chip_no_unmask = {
    .name = "BCM63xx",
    .enable = enable_brcm_irq,
    .disable = disable_brcm_irq,
    .ack = ack_brcm_irq,
    .mask = disable_brcm_irq,
    .mask_ack = mask_ack_brcm_irq,
    .unmask = unmask_brcm_irq_noop,
    .set_affinity = set_brcm_affinity
};

#if defined(CONFIG_BCM_LOT1) && defined(CONFIG_BCM93380)
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
#endif

void __init arch_init_irq(void)
{
    int i;
#if defined(CONFIG_BCM93380)
    unsigned int tp;
#endif

    spin_lock_init(&brcm_irqlock);

#if defined(CONFIG_BCM93380)
    tp = ((unsigned int)read_c0_diag3() >> 31);
    if(tp == 0)
	pperf = perf;
    else
	pperf = perf + 3;
#elif defined(CONFIG_BCMKILAUEA) || defined(CONFIG_BCM93383)
	pperf = perf;
#else
   for (i = 0; i < NR_CPUS; i++) {
        brcm_irq_ctrl[i] = &PERF->IrqControl[i];
    }
#endif

    for (i = 0; i < NR_IRQS; i++) {
        set_irq_chip_and_handler(i, &brcm_irq_chip, handle_level_irq); 
    }

    clear_c0_status(ST0_BEV);
#if defined(CONFIG_BCM_LOT1) && defined(CONFIG_BCM93380)
    if(tp)
        change_c0_status(ST0_IM, ALLINTS_NOTIMER_T1);
    else
#endif
    change_c0_status(ST0_IM, ALLINTS_NOTIMER);

#if defined(CONFIG_BCM_LOT1) && defined(CONFIG_BCM93380)
    brcm_route_irq();
#endif
#ifdef CONFIG_REMOTE_DEBUG
    rs_kgdb_hook(0);
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

#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816)
    if( irq == INTERRUPT_ID_MPI ) {
        irqflags |= IRQF_SHARED;
    }
#endif

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
