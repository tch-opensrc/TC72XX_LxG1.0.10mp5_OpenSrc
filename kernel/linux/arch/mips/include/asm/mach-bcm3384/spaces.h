/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2009 Broadcom Corporation
 */

#ifndef _ASM_MACH_BRCMSTB_SPACES_H
#define _ASM_MACH_BRCMSTB_SPACES_H

#include <linux/const.h>

/***********************************************************************
 * Kernel virtual address space
 ***********************************************************************/

#if !defined(CONFIG_BRCM_UPPER_MEMORY)

/*
 * 256MB standard MIPS32 virtual address map
 *
 * 8000_0000 - 8fff_ffff: lower 256MB, cached mapping
 * 9000_0000 - 9fff_ffff: EBI/registers, cached mapping (unused)
 * a000_0000 - afff_ffff: lower 256MB, uncached mapping
 * b000_0000 - bfff_ffff: EBI/registers, uncached mapping
 * c000_0000 - ff1f_7fff: vmalloc region
 * ff1f_8000 - ff1f_ffff: FIXMAP
 */

#define KSEG0_SIZE		_AC(0x20000000, UL)
#define KSEG1_SIZE		_AC(0x20000000, UL)
#define MAP_BASE		_AC(0xc0000000, UL)
#define FIXADDR_TOP		_AC(0xff200000, UL)
#define BRCM_MAX_UPPER_MB	_AC(0, UL)

#elif defined(CONFIG_BRCM_UPPER_256MB)

/*
 * 512MB Broadcom 256+256 virtual address map
 *
 * 8000_0000 - 8fff_ffff: lower 256MB, cached mapping
 * 9000_0000 - 9fff_ffff: EBI/registers, cached mapping (unused)
 * a000_0000 - afff_ffff: lower 256MB, uncached mapping
 * b000_0000 - bfff_ffff: EBI/registers, uncached mapping
 * c000_0000 - cfff_ffff: upper 256MB, cached mapping
 * d000_0000 - dfff_ffff: upper 256MB, uncached mapping
 * e000_0000 - fffd_7fff: vmalloc region
 * fffd_8000 - fffd_ffff: FIXMAP
 *
 * PA 3000_0000 and above are accessed through HIGHMEM (BMIPS5000 only).
 * VA d000_0000 is unmapped on BMIPS3300, to save TLB entries.
 *   CONSISTENT_DMA is used instead, in this case.
 */

#define CAC_BASE_UPPER		_AC(0xc0000000, UL)
#define UNCAC_BASE_UPPER	_AC(0xd0000000, UL)
#define KSEG0_SIZE		_AC(0x20000000, UL)
#define KSEG1_SIZE		_AC(0x20000000, UL)
#define MAP_BASE		_AC(0xe0000000, UL)
#define BRCM_MAX_UPPER_MB	_AC(256, UL)

#ifdef CONFIG_BRCM_CONSISTENT_DMA
#define FIXADDR_TOP		_AC(0xfec00000, UL)
#define CONSISTENT_BASE		_AC(0xfec00000, UL)
#define CONSISTENT_END		_AC(0xff000000, UL)
#endif

#elif defined(CONFIG_BRCM_UPPER_768MB)

/*
 * 1024MB Broadcom 256+768 virtual address map
 *
 * 8000_0000 - 8fff_ffff: 256MB RAM @ 0000_0000, cached
 * 9000_0000 - 9fff_ffff: 256MB EBI/Registers @ 1000_0000, uncached
 * a000_0000 - cfff_ffff: 768MB RAM @ 2000_0000, cached
 * d000_0000 - dfff_ffff: TBD
 * e000_0000 - ff1f_7fff: vmalloc region
 * ff1f_8000 - ff1f_ffff: FIXMAP
 * ff40_0000 - ff7f_ffff: CONSISTENT region
 *
 * PA 5000_0000 and above are accessed through HIGHMEM (BMIPS5000 only).
 */

#define TLB_UPPERMEM_VA		_AC(0xc0000000, UL)
#define TLB_UPPERMEM_PA		_AC(0x40000000, UL)
#define KSEG0_SIZE		_AC(0x40000000, UL)
#define KSEG1_SIZE		_AC(0x00000000, UL)
#define MAP_BASE		_AC(0xe0000000, UL)
#define FIXADDR_TOP		_AC(0xff200000, UL)
/* BASE and END must be 4MB-aligned (PGDIR_SIZE) */
#define CONSISTENT_BASE		_AC(0xff400000, UL)
#define CONSISTENT_END		_AC(0xff800000, UL)
#define BRCM_MAX_UPPER_MB	_AC(768, UL)

#endif /* CONFIG_BRCM_UPPER_MEMORY */

/***********************************************************************
 * Physical / PCI address space
 ***********************************************************************/

#ifdef CONFIG_BRCM_HAS_2GB_MEMC0

/*
 * Physical address map for 2GB MEMC0
 *
 * 0000_0000 - 0fff_ffff: MEMC0 (256MB)
 * 1000_0000 - 1fff_ffff: EBI/Registers (256MB)
 * 2000_0000 - 8fff_ffff: MEMC0 (1792MB)
 * 9000_0000 - cfff_ffff: MEMC1 (1024MB)
 * d000_0000 - efff_ffff: PCIe MEM BARs (512MB)
 *
 * PCIe inbound BAR:
 *
 * 0000_0000 - 0fff_ffff: MEMC0 (256MB  @ 0000_0000)
 * 1000_0000 - 7fff_ffff: MEMC0 (1792MB @ 2000_0000)
 * 8000_0000 - bfff_ffff: MEMC1 (1024MB @ 9000_0000)
 */

#define PCI_MEM_START		_AC(0xdeadbeef, UL)
#define PCI_MEM_SIZE		_AC(0xdeadbeef, UL)

#define PCI_IO_START		_AC(0xffff0000, UL)
#define PCI_IO_SIZE		_AC(0x00001000, UL)

#define PCIE_MEM_START		_AC(0xd0000000, UL)
#define PCIE_MEM_SIZE		_AC(0x20000000, UL)

#define MEMC1_START		_AC(0x90000000, UL)
#define MEMC1_PCI_OFFSET	_AC(0x10000000, UL)

#else /* CONFIG_BRCM_HAS_2GB_MEMC0 */

/*
 * Physical address map for <= 1GB MEMC0
 *
 * 0000_0000 - 0fff_ffff: MEMC0 (256MB)
 * 1000_0000 - 1fff_ffff: EBI/Registers (256MB)
 * 2000_0000 - 4fff_ffff: MEMC0 (768MB)
 * 6000_0000 - 7fff_ffff: MEMC1 (512MB)
 * a000_0000 - bfff_ffff: PCIe MEM BARs (512MB)
 * d000_0000 - efff_ffff: PCI2.3 MEM BARs (512MB)
 * f000_0000 - f05f_ffff: PCI2.3 IO BARs (6MB)
 * f000_0000 - f060_000b: PCI2.3 Configuration access (12B)
 * f100_0000 - f100_001f: PCIe Configuration access - 7420 only (32B)
 *
 * PCI2.3/PCIe inbound BAR:
 *
 * 0000_0000 - 0fff_ffff: MEMC0 (256MB  @ 0000_0000)
 * 1000_0000 - 3fff_ffff: MEMC0 (768MB  @ 2000_0000)
 * 4000_0000 - 7fff_ffff: MEMC1 (1024MB @ 6000_0000) (not currently implemented)
 */

#define PCI_MEM_START		_AC(0xd0000000, UL)
#define PCI_MEM_SIZE		_AC(0x20000000, UL)

/* this is really 6MB long, but 32k ought to be enough for anyone */
#define PCI_IO_START		_AC(0xf0000000, UL)
#define PCI_IO_SIZE		_AC(0x00008000, UL)
#define PCI_IO_ACTUAL_SIZE	_AC(0x00600000, UL)

#define PCI_IO_REG_START	_AC(0xf0600000, UL)
#define PCI_IO_REG_SIZE		_AC(0x0000000c, UL)

#define PCIE_MEM_START		_AC(0xa0000000, UL)
#define PCIE_MEM_SIZE		_AC(0x20000000, UL)

#define MEMC1_START		_AC(0x60000000, UL)
#define MEMC1_PCI_OFFSET	_AC(0x20000000, UL)

#endif /* CONFIG_BRCM_HAS_2GB_MEMC0 */

#define UPPERMEM_START		_AC(0x20000000, UL)
#define HIGHMEM_START		(UPPERMEM_START + (BRCM_MAX_UPPER_MB << 20))

#define BRCM_PCI_HOLE_START	_AC(0x10000000, UL)
#define BRCM_PCI_HOLE_SIZE	_AC(0x10000000, UL)

#define BRCM_MAX_LOWER_MB	_AC(256, UL)

#include <asm/mach-generic/spaces.h>

#endif /* _ASM_MACH_BRCMSTB_SPACES_H */
