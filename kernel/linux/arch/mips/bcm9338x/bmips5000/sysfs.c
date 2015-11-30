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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>

#include <asm/cpu-info.h>
#include <asm/r4kcache.h>
#include <asm/mipsregs.h>
#include <asm/brcmstb/brcmstb.h>

atomic_t brcm_unaligned_fp_count = ATOMIC_INIT(0);
atomic_t brcm_rdhwr_count = ATOMIC_INIT(0);

static struct platform_device *brcmstb_pdev;

static ssize_t show_unaligned_fp_count(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
		atomic_read(&brcm_unaligned_fp_count));
}

static ssize_t show_rdhwr_count(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
		atomic_read(&brcm_rdhwr_count));
}

static ssize_t show_chip_id(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%04x\n", BRCM_CHIP_ID());
}

static ssize_t show_chip_rev(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%02X\n", BRCM_CHIP_REV() + 0xa0);
}

static ssize_t show_cpu_name(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", __cpu_name[0]);
}

static ssize_t show_cpu_khz(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%ld\n", brcm_adj_cpu_khz);
}

static ssize_t show_bmem(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = 0;
	unsigned long addr = 0, size = 0;
	const char *name = attr->attr.name;

	while (*name != 0) {
		if (isdigit(*name)) {
			idx = simple_strtoul(name, NULL, 10);
			break;
		}
		name++;
	}
	bmem_region_info(idx, &addr, &size);

	return snprintf(buf, PAGE_SIZE, "0x%08lx 0x%08lx\n", addr, size);
}

static ssize_t show_ocap_partition(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%08lx 0x%08lx\n",
		brcm_mtd_ocap_start, brcm_mtd_ocap_len);
}

static ssize_t show_cfe_boardname(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", brcm_cfe_boardname);
}

static ssize_t show_cache_max_writeback(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct brcm_cache_info info;
	brcm_get_cache_info(&info);
	return snprintf(buf, PAGE_SIZE, "%d\n", info.max_writeback);
}

static ssize_t show_cache_max_writethrough(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct brcm_cache_info info;
	brcm_get_cache_info(&info);
	return snprintf(buf, PAGE_SIZE, "%d\n", info.max_writethrough);
}

static ssize_t show_cache_prefetch_enabled(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct brcm_cache_info info;
	brcm_get_cache_info(&info);
	return snprintf(buf, PAGE_SIZE, "%d\n", info.prefetch_enabled);
}

static ssize_t show_cp0_config_regs(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%08x 0x%08x 0x%08x\n",
		__read_32bit_c0_register($16, 0),
		__read_32bit_c0_register($16, 1),
		__read_32bit_c0_register($16, 7));
}

static inline ssize_t show_cp0_brcm_regs(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
		"0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
		__read_32bit_c0_register($22, 0),
		__read_32bit_c0_register($22, 1),
		__read_32bit_c0_register($22, 2),
		__read_32bit_c0_register($22, 3),
		__read_32bit_c0_register($22, 4),
		__read_32bit_c0_register($22, 5),
		__read_32bit_c0_register($22, 6),
		__read_32bit_c0_register($22, 7));
}

static inline ssize_t dump_core_regs(unsigned long cbr,
	const int *regs, int n_regs, char *buf)
{
	int i, bytes = 0;

	for (i = 0; i < n_regs; i++)
		bytes += snprintf(&buf[bytes], PAGE_SIZE - bytes, "0x%08lx ",
			DEV_RD(cbr + regs[i]));
	buf[bytes - 1] = '\n';
	return bytes;
}

#ifdef CONFIG_BMIPS3300
static ssize_t show_bmips3300_core_regs(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	const int regs[] = { BMIPS_RAC_CONFIG, BMIPS_RAC_ADDRESS_RANGE };

	return dump_core_regs(BMIPS_GET_CBR(), regs, ARRAY_SIZE(regs), buf);
}
#endif

#ifdef CONFIG_BMIPS4380
static ssize_t show_bmips4380_core_regs(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	const int regs[] = {
		BMIPS_RAC_CONFIG, BMIPS_RAC_ADDRESS_RANGE, BMIPS_RAC_CONFIG_1,
		BMIPS_L2_CONFIG, BMIPS_LMB_CONTROL, BMIPS_SYSTEM_BASE,
		BMIPS_PERF_GLOBAL_CONTROL, BMIPS_PERF_CONTROL_0,
		BMIPS_PERF_CONTROL_1, BMIPS_PERF_COUNTER_0,
		BMIPS_PERF_COUNTER_1, BMIPS_PERF_COUNTER_2,
		BMIPS_PERF_COUNTER_3, BMIPS_RELO_VECTOR_CONTROL_0,
		BMIPS_RELO_VECTOR_CONTROL_1 };

	return dump_core_regs(BMIPS_GET_CBR(), regs, ARRAY_SIZE(regs), buf);
}
#endif

/* Display secondary cache module settings */

#if defined(CONFIG_BRCM_ZSCM_L2)
static u32 read_zscm_reg(unsigned long addr)
{
	u32 ret;

	cache_op(Index_Load_Tag_S, addr);

	/* 7420 L2 Load Tag workaround */
	__asm__ __volatile__(
		".set push\n"
		".set noreorder\n"
		"sync\n"
		"ssnop\n"
		"ssnop\n"
		"ssnop\n"
		"ssnop\n"
		"ssnop\n"
		"ssnop\n"
		"ssnop\n"
		"mfc0 %0, $28, 3\n"
		"ssnop\n"
		".set pop\n"
		: "=&r" (ret) : : "memory");
	return ret;
}

static ssize_t show_zscm_regs(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
		"0x%08x 0x%08x 0x%08x 0x%08x 0x%08x "
		"0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
		read_zscm_reg(0x97000000), read_zscm_reg(0x97000010),
		read_zscm_reg(0x97000020), read_zscm_reg(0x97000030),
		read_zscm_reg(0x97000040), read_zscm_reg(0x97000050),
		read_zscm_reg(0x97000060), read_zscm_reg(0x97000070),
		read_zscm_reg(0x97000080), read_zscm_reg(0x97000090));
}
#endif

static struct device_attribute brcmstb_attr_list[] = {
	__ATTR(unaligned_fp_count, 0444, show_unaligned_fp_count, NULL),
	__ATTR(rdhwr_count, 0444, show_rdhwr_count, NULL),
	__ATTR(chip_id, 0444, show_chip_id, NULL),
	__ATTR(chip_rev, 0444, show_chip_rev, NULL),
	__ATTR(cpu_name, 0444, show_cpu_name, NULL),
	__ATTR(cpu_khz, 0444, show_cpu_khz, NULL),
	__ATTR(cp0_config_regs, 0444, show_cp0_config_regs, NULL),
	__ATTR(ocap_partition, 0444, show_ocap_partition, NULL),
	__ATTR(cfe_boardname, 0444, show_cfe_boardname, NULL),
	__ATTR(cache_max_writeback, 0444, show_cache_max_writeback, NULL),
	__ATTR(cache_max_writethrough, 0444, show_cache_max_writethrough, NULL),
	__ATTR(cache_prefetch_enabled, 0444, show_cache_prefetch_enabled, NULL),
#if   defined(CONFIG_BMIPS3300)
	__ATTR(cp0_brcm_regs, 0444, show_cp0_brcm_regs, NULL),
	__ATTR(bmips3300_core_regs, 0444, show_bmips3300_core_regs, NULL),
#elif defined(CONFIG_BMIPS4380)
	__ATTR(cp0_brcm_regs, 0444, show_cp0_brcm_regs, NULL),
	__ATTR(bmips4380_core_regs, 0444, show_bmips4380_core_regs, NULL),
#elif defined(CONFIG_BMIPS5000)
	__ATTR(cp0_brcm_regs, 0444, show_cp0_brcm_regs, NULL),
#endif
#if defined(CONFIG_BRCM_ZSCM_L2)
	__ATTR(zscm_regs, 0444, show_zscm_regs, NULL),
#endif
#if defined(CONFIG_BRCM_CPU_DIV)
	__ATTR(cpu_div, 0644, brcm_pm_show_cpu_div, brcm_pm_store_cpu_div),
#endif
#if defined(CONFIG_BRCM_CPU_PLL)
	__ATTR(cpu_pll, 0644, brcm_pm_show_cpu_pll, brcm_pm_store_cpu_pll),
#endif
	__ATTR(sata_power, 0644, brcm_pm_show_sata_power,
		brcm_pm_store_sata_power),
	__ATTR(ddr_timeout, 0644, brcm_pm_show_ddr_timeout,
		brcm_pm_store_ddr_timeout),
	__ATTR(standby_flags, 0644, brcm_pm_show_standby_flags,
		brcm_pm_store_standby_flags),
	__ATTR(standby_timeout, 0644, brcm_pm_show_standby_timeout,
		brcm_pm_store_standby_timeout),
	__ATTR(time_at_wakeup, 0444, brcm_pm_show_time_at_wakeup, NULL),
#if defined(CONFIG_BRCM_HAS_1GB_MEMC1) || defined(CONFIG_BCM7420)
	__ATTR(memc1_power, 0644, brcm_pm_show_memc1_power,
		brcm_pm_store_memc1_power),
#endif
	__ATTR(halt_mode, 0644, brcm_pm_show_halt_mode,
		brcm_pm_store_halt_mode),
	__ATTR_NULL,
};

static int __init brcm_pdev_init(void)
{
	struct device *dev;
	int i;
	struct device_attribute *attr;

	brcmstb_pdev = platform_device_alloc("brcmstb", -1);
	if (brcmstb_pdev == NULL) {
		printk(KERN_WARNING "%s: can't allocate device\n",
			__func__);
		return -ENODEV;
	}
	platform_device_add(brcmstb_pdev);
	dev = &brcmstb_pdev->dev;

	for (attr = brcmstb_attr_list; attr->attr.name != NULL; attr++)
		if (device_create_file(dev, attr) != 0)
			WARN(1, "Can't create sysfs file\n");

	/* create an attribute for each bmem region */
	for (i = 0; ; i++) {
		unsigned long addr, size;
		struct device_attribute *attr;
		char *name;

		if (bmem_region_info(i, &addr, &size) < 0)
			break;
		attr = kzalloc(sizeof(*attr), GFP_KERNEL);
		if (attr == NULL)
			break;

		name = kzalloc(16, GFP_KERNEL);
		if (name == NULL)
			break;
		snprintf(name, 16, "bmem.%d", i);

		sysfs_attr_init(&attr->attr);
		attr->attr.name = name;
		attr->attr.mode = 0444;
		attr->show = show_bmem;

		if (device_create_file(dev, attr) != 0)
			WARN(1, "Can't create sysfs file\n");
	}

	return 0;
}

arch_initcall(brcm_pdev_init);
