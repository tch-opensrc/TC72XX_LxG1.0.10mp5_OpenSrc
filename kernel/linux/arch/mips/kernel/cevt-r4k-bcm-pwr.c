/***********************************************************
 *
 * Copyright (c) 2009 Broadcom Corporation
 * All Rights Reserved
 *
<:license-gpl
 *
 ************************************************************/
/***********************************************************
 *
 * Warning... This code is not yes SMP-compatible. So power management
 * cannot be enabled at the same time as SMP for the moment.
 *
 * This file implements clock events for the Broadcom DSL and GPON CPE
 * when the power management feature is enabled. When the processor
 * is found to be mostly idle, the main CPU clock is slowed down to
 * save power. By slowing down the clock, the C0 counter unfortunately
 * also slows down. This file replaces the (typical) 1 msec clock tick
 * interrupt processing with a reliable timer source which is unaffected
 * by the change in MIPS clock changes.
 *
 * The timer available to replace the C0 timer works differently.
 * The design needs to be adjusted accordingly. The C0 counter is a free
 * running counter which wraps at 0xFFFFFFFF and which runs at different
 * frequencies depending on the MIPS frequency. The C0 compare register
 * requires to be programmed to stay ahead of the C0 counter, to generate
 * an interrupt in the future.
 *
 * The peripheral timers (there are 3 of them) wrap at 0x3fffffff and
 * run at 50 MHz. When the timer reaches a programmed value, it can generate
 * and interrupt and then either stops counting or restarts at 0.
 * This difference in behavior between the C0 counter and the peripheral timers
 * required to use 2 timers for power management. One to generate the periodic
 * interrupts required by the clock events (Timer 0), and one to keep an accurate
 * reference when the clock is slowed down for saving power (Timer 2). Timer 1
 * is planned to be used by the second processor to support SMP.
 *
 ************************************************************/


#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/percpu.h>

#include <asm/smtc_ipi.h>
#include <asm/time.h>
#include <asm/cevt-r4k.h>

#include <bcm_map_part.h>
#include <bcm_intr.h>

extern void BcmPwrMngtCheckWaitCount(void);

DEFINE_PER_CPU(struct clock_event_device, bcm_mips_clockevent_device);
int bcm_timer_irq_installed;

// Reprogram the timer for the required delta
static int bcm_mips_next_event(unsigned long delta,
                           struct clock_event_device *evt)
{
    TIMER->TimerCtl0 = TIMERENABLE | RSTCNTCLR | (delta-1);
    TIMER->TimerInts |= TIMER0;

    return 0;
}

void bcm_mips_set_clock_mode(enum clock_event_mode mode,
				struct clock_event_device *evt)
{
}

void bcm_mips_event_handler(struct clock_event_device *dev)
{
}

irqreturn_t bcm_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *cd;
	int cpu = smp_processor_id();

    TIMER->TimerInts |= TIMER0;

    cd = &per_cpu(bcm_mips_clockevent_device, cpu);
    cd->event_handler(cd);

    BcmPwrMngtCheckWaitCount();

	return IRQ_HANDLED;
}

struct irqaction perf_timer_irqaction = {
	.handler = bcm_timer_interrupt,
	.flags = IRQF_DISABLED,
	.name = "Perf timer",
};

extern unsigned int Timer2C0Snapshot;

int __cpuinit r4k_clockevent_init(void)
{
	unsigned int cpu = smp_processor_id();
	struct clock_event_device *cd;

	cd = &per_cpu(bcm_mips_clockevent_device, cpu);

	cd->name		= "BCM Perf Timer";
	cd->features		= CLOCK_EVT_FEAT_ONESHOT;

	/* Calculate the min / max delta */
	cd->mult	= div_sc((unsigned long) 50000000, NSEC_PER_SEC, 32);
	cd->shift		= 32;
	cd->max_delta_ns	= clockevent_delta2ns(0x3fffffff, cd);
	cd->min_delta_ns	= clockevent_delta2ns(0x300, cd);

	cd->rating		= 300;
	cd->irq			= INTERRUPT_ID_TIMER;
	cd->cpumask		= cpumask_of(cpu);
	cd->set_next_event	= bcm_mips_next_event;
	cd->set_mode		= bcm_mips_set_clock_mode;
	cd->event_handler	= bcm_mips_event_handler;

	clockevents_register_device(cd);

	if (bcm_timer_irq_installed)
		return 0;

	bcm_timer_irq_installed = 1;

    // Start the BCM Timer (not sure it needs this)
    setup_irq(INTERRUPT_ID_TIMER, &perf_timer_irqaction);

    // Start the BCM Timer0 - keep accurate 1 msec tick count
    TIMER->TimerCtl0 = TIMERENABLE | RSTCNTCLR | (50000000-1);
    TIMER->TimerMask |= TIMER0EN;

    // Take a snapshot of the C0 timer when Timer2 was started
    // This will be needed later when having to make adjustments
    Timer2C0Snapshot = read_c0_count();

    // Start the BCM Timer2
    // to keep an accurate free running high precision counter
    TIMER->TimerCtl2 = TIMERENABLE | ((50000000 * 10)-1);

	return 0;
}
