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
#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <asm/brcmstb/brcmstb.h>

#define INSTRUCTION_CNT	(1000 * 1000 * 1000)
#define MEM_SIZE		(10 * 1024 * 1024)

extern u32 instruction_perf_test(u32 icnt);
extern u32 mem_read_test(u32 mem_base, u32 mem_size);
extern u32 mem_write_test(u32 mem_base, u32 mem_size);
extern u32 mem_rmw_test(u32 mem_base, u32 mem_size);

static void pmuldiv(u32 memsz, u32 hz, u32 ticks)
{
	u32 mhz = hz / 1000000;
	u32 mbps = (memsz * mhz) / ticks;
	if (mbps > 10)
	{
		printk("%4d mbps\n", mbps);
	}
	else
	{
		u32 kbps = (memsz * mhz) / (ticks / 1000);
		if (kbps > 10)
		{
			printk("%4d kbps\n", kbps);
		}
		else
		{
			u32 bps = (memsz * mhz) / (ticks / (1000 * 1000));
			printk("%4d bps\n", bps);
		}
	}
}

static void mem_test_suite(u32 mem_base, u32 mem_size)
{
	u32 cticks, i, num, total_mem;
	total_mem = MEM_SIZE;
	num = total_mem / mem_size;
	if (num == 0)
	{
		num = 1;
		total_mem = mem_size;
	}
	cticks = 0;
	for (i = 0; i < num; i++)
		cticks += mem_read_test(mem_base, mem_size);
	printk("Mem read test:  %10d bytes, %10d ticks, %2d bpt, ",
		   mem_size, cticks / i, total_mem / cticks);
	pmuldiv(total_mem, mips_hpt_frequency, cticks);
		
	cticks = 0;
	for (i = 0; i < num; i++)
		cticks += mem_write_test(mem_base, mem_size);
	printk("Mem write test: %10d bytes, %10d ticks, %2d bpt, ",
		   mem_size, cticks / i, total_mem / cticks);
	pmuldiv(total_mem, mips_hpt_frequency, cticks);
	cticks = 0;
	for (i = 0; i < num; i++)
		cticks += mem_rmw_test(mem_base, mem_size);
	printk("Mem RMW test:   %10d bytes, %10d ticks, %2d bpt, ",
		   mem_size, cticks / i, total_mem / cticks);
	pmuldiv(total_mem, mips_hpt_frequency, cticks);
}

extern char _end[];
int __init custom_perf_test(void)
{
	u32 cticks, ipt;

	u32 membase = (u32)&_end[0];
	membase += 0x00100000;
	membase &= ~(0x00100000 - 1);
	membase += 0x01000000;

	printk("Custom Perf Tests at %08x\n", membase);
	local_irq_disable();
	cticks = instruction_perf_test(INSTRUCTION_CNT);
	ipt = INSTRUCTION_CNT / cticks;
	if (mips_hpt_frequency == 0)
		plat_time_init();
	printk("Instr test:     %d instr, %d ticks, %d ipt, %d ips\n",
		   INSTRUCTION_CNT, cticks, ipt, ipt * mips_hpt_frequency);
	printk("------------------ %d mem test ---------------------\n", 0x4000);
	mem_test_suite(membase, 0x4000);
	printk("------------------ %d mem test ---------------------\n", 0x20000);
	mem_test_suite(membase, 0x20000);
	printk("------------------ %d mem test ---------------------\n", MEM_SIZE);
	mem_test_suite(membase, MEM_SIZE);
	printk("------------------ Uncached mem test ---------------------\n");
	mem_test_suite(KSEG1ADDR(membase), MEM_SIZE);
	printk("------------------ END mem tests ---------------------\n");
	local_irq_enable();

	return 0;
}

early_initcall(custom_perf_test);
