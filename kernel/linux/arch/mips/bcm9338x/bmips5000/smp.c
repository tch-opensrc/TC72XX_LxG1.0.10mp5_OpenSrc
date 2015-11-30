/*
 * Copyright (C) 2009 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/version.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/reboot.h>
#include <linux/io.h>

#include <asm/time.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/system.h>
#include <asm/bootinfo.h>
#include <asm/pmon.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/mipsregs.h>
#include <asm/brcmstb/brcmstb.h>

/* initial $sp, $gp - used by arch/mips/brcmstb/vector.S */
unsigned long brcmstb_smp_boot_sp;
unsigned long brcmstb_smp_boot_gp;

static void brcmstb_send_ipi_single(int cpu, unsigned int action);
static void brcmstb_ack_ipi(unsigned int irq);

/* Early cpumask setup - runs on TP0 */
static void brcmstb_smp_setup(void)
{
	__cpu_number_map[0] = 0;
	__cpu_logical_map[0] = 0;
	set_cpu_possible(0, 1);
	set_cpu_present(0, 1);

	__cpu_number_map[1] = 1;
	__cpu_logical_map[1] = 1;
	set_cpu_possible(1, 1);
	set_cpu_present(1, 1);

#if defined(CONFIG_BMIPS4380)
	/* NBK and weak order flags */
	set_c0_brcm_config_0(0x30000);

	/*
	 * MIPS interrupts 0,1 (SW INT 0,1) cross over to the other TP
	 * MIPS interrupt 2 (HW INT 0) is the TP0 L1 controller output
	 * MIPS interrupt 3 (HW INT 1) is the TP1 L1 controller output
	 */
	change_c0_brcm_cmt_intr(0xf8018000, (0x02 << 27) | (0x03 << 15));
#elif defined(CONFIG_BMIPS5000)
	/* enable raceless SW interrupts */
	set_c0_brcm_config(0x03 << 22);

	/* clear any pending SW interrupts */
	write_c0_brcm_action(0x2000 | (0 << 9) | (0 << 8));
	write_c0_brcm_action(0x2000 | (0 << 9) | (1 << 8));
	write_c0_brcm_action(0x2000 | (1 << 9) | (0 << 8));
	write_c0_brcm_action(0x2000 | (1 << 9) | (1 << 8));
#endif
}

static irqreturn_t brcmstb_ipi_interrupt(int irq, void *dev_id);

/* IRQ setup - runs on TP0 */
static void brcmstb_prepare_cpus(unsigned int max_cpus)
{
	if (request_irq(BRCM_IRQ_IPI0, brcmstb_ipi_interrupt, IRQF_DISABLED,
			"smp_ipi_tp0", NULL))
		panic("Can't request TP0 IPI interrupt\n");
	if (request_irq(BRCM_IRQ_IPI1, brcmstb_ipi_interrupt, IRQF_DISABLED,
			"smp_ipi_tp1", NULL))
		panic("Can't request TP1 IPI interrupt\n");
}

/* Tell the hardware to boot TP1 - runs on TP0 */
static void brcmstb_boot_secondary(int cpu, struct task_struct *idle)
{
	brcmstb_smp_boot_sp = __KSTK_TOS(idle);
	brcmstb_smp_boot_gp = (unsigned long)task_thread_info(idle);
	mb();

	/*
	 * TP1 initial boot sequence:
	 *   brcm_reset_nmi_vec @ a000_0000 ->
	 *   brcmstb_tp1_entry ->
	 *   brcm_upper_tlb_setup (cached function call) ->
	 *   start_secondary (cached jump)
	 *
	 * TP1 warm restart sequence:
	 *   play_dead WAIT loop ->
	 *   brcm_tp1_int_vec @ BRCM_WARM_RESTART_VEC ->
	 *   eret to play_dead ->
	 *   brcmstb_tp1_reentry ->
	 *   start_secondary
	 *
	 * Vector relocation code is in arch/mips/brcmstb/prom.c
	 * Actual boot vectors are in arch/mips/brcmstb/vector.S
	 */

	printk(KERN_INFO "SMP: Booting CPU%d...\n", cpu);

	/* warm restart */
	brcmstb_send_ipi_single(1, 0);

#if defined(CONFIG_BMIPS4380)
	set_c0_brcm_cmt_ctrl(0x01);
#elif defined(CONFIG_BMIPS5000)
	write_c0_brcm_action(0x9);
#endif
}

/* Early setup - runs on TP1 after cache probe */
static void brcmstb_init_secondary(void)
{
#if defined(CONFIG_BMIPS4380)
	unsigned long cbr = BMIPS_GET_CBR();
	unsigned long old_vec = DEV_RD(cbr + BMIPS_RELO_VECTOR_CONTROL_1);

	/* make sure the NMI vector is in kseg0 now that we've booted */
	DEV_WR_RB(cbr + BMIPS_RELO_VECTOR_CONTROL_1, old_vec & ~0x20000000);
#elif defined(CONFIG_BMIPS5000)
	write_c0_brcm_bootvec(read_c0_brcm_bootvec() & ~0x20000000);
#endif
	brcmstb_ack_ipi(0);

	write_c0_compare(read_c0_count() + mips_hpt_frequency / HZ);
	/* hw irq lines 3+4 (gfap) goes to tp0 (secondary thread) */
	set_c0_status(IE_SW0 | IE_SW1 | IE_IRQ3 | IE_IRQ4 | IE_IRQ5 | ST0_IE);
	irq_enable_hazard();
}

#if defined(CONFIG_BRCM_PERF_TEST)
extern int custom_perf_test(void);
#endif
/* Late setup - runs on TP1 before entering the idle loop */
static void brcmstb_smp_finish(void)
{
	printk(KERN_INFO "SMP: CPU%d is running\n", smp_processor_id());
#if defined(CONFIG_BRCM_PERF_TEST)
	custom_perf_test();
#endif
}

/* Runs on TP0 after all CPUs have been booted */
static void brcmstb_cpus_done(void)
{
#if defined(CONFIG_BRCM_PERF_TEST)
	custom_perf_test();
#endif
}

#if defined(CONFIG_BMIPS4380)
static DEFINE_SPINLOCK(ipi_lock);

static void brcmstb_send_ipi_single(int cpu, unsigned int action)
{
	unsigned long flags;
	spin_lock_irqsave(&ipi_lock, flags);
	set_c0_cause(smp_processor_id() ? C_SW0 : C_SW1);
	irq_enable_hazard();
	spin_unlock_irqrestore(&ipi_lock, flags);
}

static void brcmstb_ack_ipi(unsigned int irq)
{
	unsigned long flags;
	spin_lock_irqsave(&ipi_lock, flags);
	clear_c0_cause(smp_processor_id() ? C_SW1 : C_SW0);
	irq_enable_hazard();
	spin_unlock_irqrestore(&ipi_lock, flags);
}

#elif defined(CONFIG_BMIPS5000)

static void brcmstb_send_ipi_single(int cpu, unsigned int action)
{
	unsigned int bit = cpu;
	write_c0_brcm_action(0x3000 | (bit << 8) | (cpu << 9));
	irq_enable_hazard();
}

static void brcmstb_ack_ipi(unsigned int irq)
{
	unsigned int bit = smp_processor_id();
	write_c0_brcm_action(0x2000 | (bit << 8) | (smp_processor_id() << 9));
	irq_enable_hazard();
}

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
static void brcmstb_send_ipi_mask(const struct cpumask *mask,
	unsigned int action)
{
	unsigned int i;

	for_each_cpu(i, mask)
		brcmstb_send_ipi_single(i, action);
}
#else
static void brcmstb_send_ipi_mask(cpumask_t mask, unsigned int action)
{
	unsigned int i;

	for_each_cpu_mask(i, mask)
		brcmstb_send_ipi_single(i, action);
}
#endif

static irqreturn_t brcmstb_ipi_interrupt(int irq, void *dev_id)
{
	brcmstb_ack_ipi(irq);
	smp_call_function_interrupt();
	return IRQ_HANDLED;
}

#ifdef CONFIG_HOTPLUG_CPU

static int brcmstb_cpu_disable(void)
{
	unsigned int cpu = smp_processor_id();

	if (cpu == 0)
		return -EBUSY;

	printk(KERN_INFO "SMP: CPU%d is offline\n", cpu);

	cpu_clear(cpu, cpu_online_map);

	local_flush_tlb_all();
	local_flush_icache_range(0, ~0);

	return 0;
}

static void brcmstb_cpu_die(unsigned int cpu)
{
	extern void wait_forever(void);

	printk("CPU %d wait forever\n", cpu);
	
	wait_forever();
}

void __ref play_dead(void)
{
	idle_task_exit();

	/* flush data cache */
	_dma_cache_wback_inv(0, ~0);

	/*
	 * Wakeup is on SW0 or SW1; disable everything else
	 * Use BEV !IV (BRCM_WARM_RESTART_VEC) to avoid the regular Linux
	 * IRQ handlers; this clears ST0_IE and returns immediately.
	 */
	clear_c0_cause(CAUSEF_IV | C_SW0 | C_SW1);
	change_c0_status(IE_IRQ5 | IE_IRQ1 | IE_SW0 | IE_SW1 | ST0_IE | ST0_BEV,
		IE_SW0 | IE_SW1 | ST0_IE | ST0_BEV);
	irq_disable_hazard();

	/*
	 * wait for SW interrupt from brcmstb_boot_secondary(), then jump
	 * back to start_secondary()
	 */
	__asm__ __volatile__(
	"	wait\n"
	"	j	brcmstb_tp1_reentry\n"
	: : : "memory");
}

#endif /* CONFIG_HOTPLUG_CPU */

struct plat_smp_ops brcmstb_smp_ops = {
	.send_ipi_single	= brcmstb_send_ipi_single,
	.send_ipi_mask		= brcmstb_send_ipi_mask,
	.init_secondary		= brcmstb_init_secondary,
	.smp_finish			= brcmstb_smp_finish,
	.cpus_done			= brcmstb_cpus_done,
	.boot_secondary		= brcmstb_boot_secondary,
	.smp_setup			= brcmstb_smp_setup,
	.prepare_cpus		= brcmstb_prepare_cpus,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_disable		= brcmstb_cpu_disable,
	.cpu_die			= brcmstb_cpu_die,
#endif
};
