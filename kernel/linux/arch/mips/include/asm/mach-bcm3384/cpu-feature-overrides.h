/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2003 Ralf Baechle
 * Copyright (C) 2009 Broadcom Corporation
 */
#ifndef __ASM_MACH_BRCMSTB_CPU_FEATURE_OVERRIDES_H
#define __ASM_MACH_BRCMSTB_CPU_FEATURE_OVERRIDES_H

#if defined(CONFIG_BMIPS4350)

#define cpu_has_tlb			1
#define cpu_has_4kex			4
#define cpu_has_4ktlb			8
#define cpu_has_4k_cache		1
#define cpu_has_fpu			0
#define cpu_has_32fpr			0
#define cpu_has_counter			0x40
#define cpu_has_watch			0
#define cpu_has_mips16			0
#define cpu_has_divec			0x200
#define cpu_has_vce			0
#define cpu_has_cache_cdex_p		0
#define cpu_has_cache_cdex_s		0
#define cpu_has_prefetch		0
#define cpu_has_mcheck			0x2000
#define cpu_has_ejtag			0x4000
#define cpu_has_llsc			0x10000
#define cpu_has_vtag_icache		0
/* #define cpu_has_dc_aliases	? */
#define cpu_has_ic_fills_f_dc		0

#define cpu_has_nofpuex			0
#define cpu_has_64bits			0
#define cpu_has_64bit_zero_reg		0
#define cpu_has_64bit_gp_regs		0
/* #define cpu_has_inclusive_pcaches ? */
#define cpu_has_64bit_addresses		0

#define cpu_has_subset_pcaches		0

#define cpu_dcache_line_size()		16
#define cpu_icache_line_size()		16
#define cpu_scache_line_size()		0

#elif defined(CONFIG_BMIPS3300)

#define cpu_has_dc_aliases		1
#define cpu_has_ic_fills_f_dc		0
#define cpu_has_vtag_icache		0
#define cpu_has_inclusive_pcaches	0
#define cpu_icache_snoops_remote_store	1
#define cpu_has_mips32r1		1
#define cpu_has_mips32r2		0
#define cpu_has_mips64r1		0
#define cpu_has_mips64r2		0
#define cpu_dcache_line_size()		16
#define cpu_icache_line_size()		16
#define cpu_scache_line_size()		0

#elif defined(CONFIG_BMIPS5000)

#define cpu_has_dc_aliases		0
#define cpu_has_ic_fills_f_dc		1
#define cpu_has_vtag_icache		0
#define cpu_has_inclusive_pcaches	1
#define cpu_icache_snoops_remote_store	1
#define cpu_has_mips32r1		1
#define cpu_has_mips32r2		0
#define cpu_has_mips64r1		0
#define cpu_has_mips64r2		0
#define cpu_dcache_line_size()		32
#define cpu_icache_line_size()		64
#define cpu_scache_line_size()		128

#endif

#if defined(CONFIG_BRCM_HAS_XI)
#define kernel_uses_smartmips_rixi	1
#else
#define kernel_uses_smartmips_rixi	0
#endif

#endif /* __ASM_MACH_BRCMSTB_CPU_FEATURE_OVERRIDES_H */
