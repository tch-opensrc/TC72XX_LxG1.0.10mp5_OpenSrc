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
 *    SMP support for Broadcom 63xx and 68xx chips
 *
 *    05/2009    Created by Xi Wang
 *
 ************************************************************/

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>
#include <linux/compiler.h>

#include <asm/atomic.h>
#include <asm/cacheflush.h>
#include <asm/cpu.h>
#include <asm/processor.h>
#include <asm/system.h>
#include <asm/hardirq.h>
#include <asm/mmu_context.h>
#include <asm/smp.h>
#include <asm/time.h>
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#include <asm/mips_mt.h>

#include <bcm_cpu.h>
#include <bcm_map_part.h>
#include <bcm_intr.h>


static int cpu_ipi_irq;
DEFINE_PER_CPU(unsigned int, ipi_pending);
DEFINE_PER_CPU(unsigned int, ipi_flags);

extern spinlock_t brcm_irqlock;

// boot parameters
struct BootConfig {
    unsigned int func_addr;
    unsigned int gp;
    unsigned int sp;
};

static struct BootConfig boot_config;

void install_smp_boot_ex_handler(void);
static void core_send_ipi_single(int cpu, unsigned int action);
static void core_send_ipi_mask(cpumask_t mask, unsigned int action);


void install_smp_boot_ex_handler(void)
{

	asm (
		".set push\n"
		".set noreorder\n"
		"lui 	$8, 0xa000 \n"
		"ori 	$8, $8, 0x0200    # alternative mips exception vector\n"
		"la		$9, 2f\n"
		"la		$10, 3f\n"
	"1:\n"
		"lw		$11, 0($9)\n"
		"sw		$11, 0($8)\n"
		"addiu	$9, $9, 4\n"
		"bne 	$9, $10, 1b\n"
		"addiu	$8, $8, 4\n"
		"b		3f\n"
		"nop\n"
	"2:    # begin exception handler code\n"
		"mfc0 	$26, $13, 0\n"
		"li 		$27, 0x800100	    # change back to normal exception vector & ack interrupt\n"
		"xor	$26, $27\n"
		"mtc0	$26, $13, 0\n"
		"la		$27, %0    # pointer to boot_config structure\n"
		"lw 	$24, 0($27)    # func_addr - CFE will read this register for kernel entry address\n"
		"lw       $28, 4($27)    # gp\n"
		"lw       $29, 8($27)    # sp\n"
		"eret\n"
	"3:\n"
		".set pop\n"
		:
		: "X" (&boot_config)
	);
}


static irqreturn_t ipi_interrupt(int irq, void *dev_id)
{
	unsigned int ipiflags;

	spin_lock(&brcm_irqlock);
	ipiflags = __get_cpu_var(ipi_flags);
	__get_cpu_var(ipi_flags) = 0;
	spin_unlock(&brcm_irqlock);

	if (ipiflags & 1<<0) {
		/* Interrupt itself is enough for triggering reschedule, do nothing here */
	}

	if (ipiflags & 1<<1) {
		smp_call_function_interrupt();
	}

	return IRQ_HANDLED;
}


static struct irqaction irq_ipi = {
	.handler	= ipi_interrupt,
	.flags		= IRQF_DISABLED|IRQF_PERCPU,
	.name		= "IPI"
};


static void __init brcm_smp_setup(void)
{
	int i;

	cpus_clear(cpu_possible_map);

	for (i = 0; i < 2; i++) {
		cpu_set(i, cpu_possible_map);
		__cpu_number_map[i]	= i;
		__cpu_logical_map[i]	= i;
	}
}


static void __init brcm_prepare_cpus(unsigned int max_cpus)
{
	unsigned int c0tmp;
	int cpu;

	c0tmp = __read_32bit_c0_register($22, 0);
	c0tmp |= CP0_BCM_CFG_NBK;    /* non blocking */
	__write_32bit_c0_register($22, 0, c0tmp);

	c0tmp = __read_32bit_c0_register($22, 2);
	c0tmp &= ~(CP0_CMT_PRIO_TP0 | CP0_CMT_PRIO_TP1); /* equal (random) D-cache priority */
	__write_32bit_c0_register($22, 2, c0tmp);

	//printk("bcm config0 %08x\n", __read_32bit_c0_register($22, 0));
	//printk("cmt control %08x\n", __read_32bit_c0_register($22, 2));
	//printk("cmt local %08x\n", __read_32bit_c0_register($22, 3));
	//printk("bcm config1 %08x\n", __read_32bit_c0_register($22, 5));

	for_each_cpu_mask(cpu, cpu_possible_map) {
		per_cpu(ipi_pending, cpu) = 0;
		per_cpu(ipi_flags, cpu) = 0;
	}

	c0tmp = __read_32bit_c0_register($22, 1);
	c0tmp |= CP0_CMT_SIR_0;
	__write_32bit_c0_register($22, 1, c0tmp);

	cpu_ipi_irq = INTERRUPT_ID_SOFTWARE_0;
	setup_irq(cpu_ipi_irq, &irq_ipi);
	set_irq_handler(cpu_ipi_irq, handle_percpu_irq);
}


// Pass PC, SP, and GP to a secondary core and start it up by sending an inter-core interrupt
static void __cpuinit brcm_boot_secondary(int cpu, struct task_struct *idle)
{
	unsigned int cause;

	boot_config.func_addr = (unsigned long) smp_bootstrap;
	boot_config.sp = (unsigned int) __KSTK_TOS(idle);
	boot_config.gp = (unsigned int) task_thread_info(idle);

	install_smp_boot_ex_handler();

	cause = read_c0_cause();
	cause |= CAUSEF_IP0;
	write_c0_cause(cause);
}


static void __cpuinit brcm_init_secondary(void)
{
	//printk("bcm config0 %08x\n", __read_32bit_c0_register($22, 0));
	//printk("cmt control %08x\n", __read_32bit_c0_register($22, 2));
	//printk("cmt local %08x\n", __read_32bit_c0_register($22, 3));
	//printk("bcm config1 %08x\n", __read_32bit_c0_register($22, 5));

	clear_c0_status(ST0_BEV);
	change_c0_status(ST0_IM, IE_SW0 | IE_SW1 | IE_IRQ1 | IE_IRQ5); 
}


static void __cpuinit brcm_smp_finish(void)
{
}


static void brcm_cpus_done(void)
{
}


// send inter-core interrupts
static void core_send_ipi_single(int cpu, unsigned int action)
{
	unsigned long flags;
	unsigned int cause;
	
	//	printk("== from_cpu %d    to_cpu %d    action %u\n", smp_processor_id(), cpu, action);

	spin_lock_irqsave(&brcm_irqlock, flags);

	switch (action) {
	case SMP_RESCHEDULE_YOURSELF:
		per_cpu(ipi_pending, cpu) = 1;
		per_cpu(ipi_flags, cpu) |= 1<<0;
		break;
	case SMP_CALL_FUNCTION:
		per_cpu(ipi_pending, cpu) = 1;
		per_cpu(ipi_flags, cpu) |= 1<<1;
		break;
	default:
		goto errexit;
	}

	cause = read_c0_cause();
	cause |= CAUSEF_IP0;
	write_c0_cause(cause);

errexit:
	spin_unlock_irqrestore(&brcm_irqlock, flags);
}


static void core_send_ipi_mask(cpumask_t mask, unsigned int action)
{
	unsigned int cpu;

	for_each_cpu_mask(cpu, mask) {
		core_send_ipi_single(cpu, action);
	}
}


struct plat_smp_ops brcm_smp_ops = {
	.send_ipi_single	= core_send_ipi_single,
	.send_ipi_mask		= core_send_ipi_mask,
	.init_secondary		= brcm_init_secondary,
	.smp_finish		= brcm_smp_finish,
	.cpus_done		= brcm_cpus_done,
	.boot_secondary		= brcm_boot_secondary,
	.smp_setup		= brcm_smp_setup,
	.prepare_cpus		= brcm_prepare_cpus
};

