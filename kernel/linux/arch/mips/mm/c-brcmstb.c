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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/pfn.h>
#include <linux/bitops.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/highmem.h>

#include <asm/bcache.h>
#include <asm/bootinfo.h>
#include <asm/cache.h>
#include <asm/cacheops.h>
#include <asm/cpu.h>
#include <asm/cpu-features.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/r4kcache.h>
#include <asm/system.h>
#include <asm/mmu_context.h>
#include <asm/war.h>
#include <asm/cacheflush.h>
#include <asm/brcmstb/brcmstb.h>
#include <dma-coherence.h>

/*
 * R4K-style cache flush functions
 *
 * BMIPS5000 uses an optimized implementation from this file
 * Everything uses uses a mostly-stock c-r4k.c
 */

#if !defined(CONFIG_BMIPS5000)
#include "c-r4k.c"
#else /* !defined(CONFIG_BMIPS5000) */

static unsigned long icache_size __read_mostly;
static unsigned long dcache_size __read_mostly;
static unsigned long scache_size __read_mostly;

/*
 * Dummy cache handling routines for machines without boardcaches
 */
static void cache_noop(void) {}

static struct bcache_ops no_sc_ops = {
	.bc_enable = (void *)cache_noop,
	.bc_disable = (void *)cache_noop,
	.bc_wback_inv = (void *)cache_noop,
	.bc_inv = (void *)cache_noop,
};

struct bcache_ops *bcops = &no_sc_ops;

static void (* r4k_blast_dcache_page)(unsigned long addr);
static void (* r4k_blast_dcache_page_indexed)(unsigned long addr);
static void (* r4k_blast_dcache)(void);
static void (* r4k_blast_icache_page)(unsigned long addr);
static void (* r4k_blast_icache_page_indexed)(unsigned long addr);
static void (* r4k_blast_icache)(void);
static void (* r4k_blast_scache_page)(unsigned long addr);
static void (* r4k_blast_scache_page_indexed)(unsigned long addr);
static void (* r4k_blast_scache)(void);

static inline void r4k_blast_dcache_page_dc32(unsigned long addr)
{
	blast_dcache32_page(addr);
}

static void r4k___flush_cache_all(void)
{
	/* L2 flush implicitly flushes L1 */
	r4k_blast_scache();
	__sync();
}

static void r4k_noop(void)
{
}

static void r4k_instruction_hazard(void)
{
	instruction_hazard();
}

static void r4k_dma_cache_wback_inv(unsigned long addr, unsigned long size)
{
	/* Catch bad driver code */
	BUG_ON(size == 0);

	/* L2 flush implicitly flushes L1 */
	if (size >= scache_size)
		r4k_blast_scache();
	else
		bc_wback_inv(addr, size);

	__sync();
}

static char *way_string[] __cpuinitdata = { NULL, "direct mapped", "2-way",
	"3-way", "4-way", "5-way", "6-way", "7-way", "8-way"
};

static void __cpuinit probe_pcache(void)
{
	struct cpuinfo_mips *c = &current_cpu_data;
	unsigned int config = read_c0_config();
	unsigned long config1;
	unsigned int lsize;

	switch (c->cputype) {
	default:
		if (!(config & MIPS_CONF_M))
			panic("Don't know how to probe P-caches on this cpu.");

		/*
		 * So we seem to be a MIPS32 or MIPS64 CPU
		 * So let's probe the I-cache ...
		 */
		config1 = read_c0_config1();

		if ((lsize = ((config1 >> 19) & 7)))
			c->icache.linesz = 2 << lsize;
		else
			c->icache.linesz = lsize;
		c->icache.sets = 64 << ((config1 >> 22) & 7);
		c->icache.ways = 1 + ((config1 >> 16) & 7);

		icache_size = c->icache.sets *
		              c->icache.ways *
		              c->icache.linesz;
		c->icache.waybit = __ffs(icache_size/c->icache.ways);

		if (config & 0x8)		/* VI bit */
			c->icache.flags |= MIPS_CACHE_VTAG;

		/*
		 * Now probe the MIPS32 / MIPS64 data cache.
		 */
		c->dcache.flags = 0;

		if ((lsize = ((config1 >> 10) & 7)))
			c->dcache.linesz = 2 << lsize;
		else
			c->dcache.linesz= lsize;
		c->dcache.sets = 64 << ((config1 >> 13) & 7);
		c->dcache.ways = 1 + ((config1 >> 7) & 7);

		dcache_size = c->dcache.sets *
		              c->dcache.ways *
		              c->dcache.linesz;
		c->dcache.waybit = __ffs(dcache_size/c->dcache.ways);

		c->options |= MIPS_CPU_PREFETCH;
		break;
	}

	/* compute a couple of other cache variables */
	c->icache.waysize = icache_size / c->icache.ways;
	c->dcache.waysize = dcache_size / c->dcache.ways;

	c->icache.sets = c->icache.linesz ?
		icache_size / (c->icache.linesz * c->icache.ways) : 0;
	c->dcache.sets = c->dcache.linesz ?
		dcache_size / (c->dcache.linesz * c->dcache.ways) : 0;

	printk("Primary instruction cache %ldkB, %s, %s, linesize %d bytes.\n",
	       icache_size >> 10,
	       cpu_has_vtag_icache ? "virtually tagged" : "physically tagged",
	       way_string[c->icache.ways], c->icache.linesz);

	printk("Primary data cache %ldkB, %s, linesize %d bytes.\n",
	       dcache_size >> 10, way_string[c->dcache.ways], c->dcache.linesz);
}

extern int mips_sc_init(void);

static void __cpuinit setup_scache(void)
{
	struct cpuinfo_mips *c = &current_cpu_data;

	if (mips_sc_init ()) {
		scache_size = c->scache.ways * c->scache.sets * c->scache.linesz;
		printk("MIPS secondary cache %ldkB, %s, linesize %d bytes.\n",
		       scache_size >> 10,
		       way_string[c->scache.ways], c->scache.linesz);
	} else {
		panic("c-r4k cache functions require L2 enabled");
	}
	return;
}

static int __cpuinitdata cca = -1;

static int __init cca_setup(char *str)
{
	get_option(&str, &cca);

	return 1;
}

__setup("cca=", cca_setup);

static void __cpuinit coherency_setup(void)
{
	if (cca < 0 || cca > 7)
		cca = read_c0_config() & CONF_CM_CMASK;
	_page_cachable_default = cca << _CACHE_SHIFT;

	pr_debug("Using cache attribute %d\n", cca);
	change_c0_config(CONF_CM_CMASK, cca);
}

void __cpuinit r4k_cache_init(void)
{
	extern void build_clear_page(void);
	extern void build_copy_page(void);
	extern char except_vec2_generic;

	/* Default cache error handler for R4000 and R5000 family */
	set_uncached_handler (0x100, &except_vec2_generic, 0x80);

	probe_pcache();
	setup_scache();

	BUG_ON(cpu_icache_line_size() != 64);
	BUG_ON(cpu_dcache_line_size() != 32);
	BUG_ON(cpu_scache_line_size() != 128);

	r4k_blast_dcache_page = r4k_blast_dcache_page_dc32;
	r4k_blast_dcache_page_indexed = blast_dcache32_page_indexed;
	r4k_blast_dcache = blast_dcache32;
	r4k_blast_icache_page = blast_icache64_page;
	r4k_blast_icache_page_indexed = blast_icache64_page_indexed;
	r4k_blast_icache = blast_icache64;
	r4k_blast_scache_page = blast_scache128_page;
	r4k_blast_scache_page_indexed = blast_scache128_page_indexed;
	r4k_blast_scache = blast_scache128;

	shm_align_mask = PAGE_SIZE-1;

	__flush_cache_vmap	= r4k___flush_cache_all;
	__flush_cache_vunmap	= r4k___flush_cache_all;

	flush_cache_all		= (void *)r4k_noop;
	__flush_cache_all	= r4k___flush_cache_all;
	flush_cache_mm		= (void *)r4k_noop;
	flush_cache_page	= (void *)r4k_instruction_hazard;
	flush_cache_range	= (void *)r4k_instruction_hazard;

	flush_cache_sigtramp	= (void *)r4k_noop;
	flush_icache_all	= (void *)r4k_noop;
	local_flush_data_cache_page	= (void *)r4k_instruction_hazard;
	flush_data_cache_page	= (void *)r4k_instruction_hazard;
	flush_icache_range	= (void *)r4k_instruction_hazard;
	local_flush_icache_range	= (void *)r4k_instruction_hazard;

	_dma_cache_wback_inv	= r4k_dma_cache_wback_inv;
	_dma_cache_wback	= r4k_dma_cache_wback_inv;
	_dma_cache_inv		= r4k_dma_cache_wback_inv;

	build_clear_page();
	build_copy_page();
	r4k___flush_cache_all();
	coherency_setup();
}

#endif /* !defined(CONFIG_BMIPS5000) */

/*
 * Fine-grained cacheflush() syscall for usermode Nexus
 */
int brcm_cacheflush(unsigned long addr, unsigned long bytes,
	unsigned int cache)
{
#if !defined(CONFIG_BRCM_ZSCM_L2)
	/* partial RAC invalidate is not supported */
	if (cache == RACACHE) {
		brcm_inv_prefetch(0, 0);
		return 0;
	}
#endif
	/* ICACHE/BCACHE is handled by the standard implementation */
	if (cache & ICACHE) {
		flush_icache_range(addr, addr + bytes);
		return 0;
	}

	/* New DCACHE implementation flushes D$/L2 out to RAM for DMA */
	if (cache & DCACHE) {
		unsigned long pg = addr & PAGE_MASK, end = addr + bytes;

		if (bytes == 0 || (bytes >= dcache_size)) {
			r4k_blast_dcache();
			r4k_blast_scache();
			__sync();
			return 0;
		}

		down_read(&current->mm->mmap_sem);
		while (pg < end) {
			pgd_t *pgdp = pgd_offset(current->mm, pg);
			pud_t *pudp = pud_offset(pgdp, pg);
			pmd_t *pmdp = pmd_offset(pudp, pg);
			pte_t *ptep = pte_offset(pmdp, pg);

			if (!(pte_val(*ptep) & _PAGE_PRESENT)) {
				up_read(&current->mm->mmap_sem);
				return -EFAULT;
			}

#if !defined(CONFIG_BRCM_ZSCM_L2)
			r4k_blast_dcache_page(pg);
#endif
			r4k_blast_scache_page(pg);
			pg += PAGE_SIZE;
		}
		up_read(&current->mm->mmap_sem);
	}

	if (cache & RACACHE)
		brcm_inv_prefetch(addr, bytes);

	__sync();
	return 0;
}
EXPORT_SYMBOL(brcm_cacheflush);

/*
 * Flush RAC or prefetch lines after DMA from device
 */
void brcm_inv_prefetch(unsigned long addr, unsigned long size)
{
#if defined(CONFIG_BMIPS3300)
	DEV_SET_RB(BMIPS_GET_CBR() + BMIPS_RAC_CONFIG, 0x100);
#elif defined(CONFIG_BMIPS4380)
	DEV_SET_RB(BMIPS_GET_CBR() + BMIPS_RAC_CONFIG, 0x100);
#elif defined(CONFIG_BRCM_ZSCM_L2)
	unsigned int linesz = cpu_scache_line_size();
	unsigned long addr0 = addr, addr1;

	BUG_ON(size == 0);

	addr0 &= ~(linesz - 1);
	addr1 = (addr0 + size - 1) & ~(linesz - 1);

	protected_writeback_scache_line(addr0);
	if (likely(addr1 != addr0))
		protected_writeback_scache_line(addr1);
	else
		return;

	addr0 += linesz;
	if (likely(addr1 != addr0))
		protected_writeback_scache_line(addr0);
	else
		return;

	addr1 -= linesz;
	if (likely(addr1 > addr0))
		protected_writeback_scache_line(addr0);
#endif
}
EXPORT_SYMBOL(brcm_inv_prefetch);

/*
 * Hooks for extra RAC/prefetch flush after DMA
 */
static inline void __brcm_sync(struct page *page,
	unsigned long offset, size_t size, enum dma_data_direction direction)
{
	size_t left = size;

	if (direction == DMA_TO_DEVICE)
		return;

	do {
		size_t len = left;

		if (PageHighMem(page)) {
			void *addr;

			if (offset + len > PAGE_SIZE) {
				if (offset >= PAGE_SIZE) {
					page += offset >> PAGE_SHIFT;
					offset &= ~PAGE_MASK;
				}
				len = PAGE_SIZE - offset;
			}

			addr = kmap_atomic(page, KM_USER0);
			brcm_inv_prefetch((unsigned long)addr + offset, len);
			kunmap_atomic(addr, KM_USER0);
		} else
			brcm_inv_prefetch((unsigned long)page_address(page) +
				offset, size);
		offset = 0;
		page++;
		left -= len;
	} while (left);
}

void plat_unmap_dma_mem(struct device *dev, dma_addr_t dma_addr,
	size_t size, int dir)
{
	unsigned long pa = plat_dma_addr_to_phys(dev, dma_addr);
	__brcm_sync(pfn_to_page(PFN_DOWN(pa)), pa & ~PAGE_MASK, size, dir);
}

static void brcm_dma_sync_single_for_cpu(struct device *dev,
	dma_addr_t dma_handle, size_t size, enum dma_data_direction dir)
{
	unsigned long pa = plat_dma_addr_to_phys(dev, dma_handle);
	__brcm_sync(pfn_to_page(PFN_DOWN(pa)), pa & ~PAGE_MASK, size, dir);
}

static void brcm_dma_sync_sg_for_cpu(struct device *dev,
	struct scatterlist *sg, int nelems, enum dma_data_direction direction)
{
	int i;
	for (i = 0; i < nelems; i++, sg++)
		__brcm_sync(sg_page(sg), sg->offset, sg->length, direction);
}

static int __init brcm_setup_dma_ops(void)
{
	mips_dma_map_ops->sync_single_for_cpu = brcm_dma_sync_single_for_cpu;
	mips_dma_map_ops->sync_sg_for_cpu = brcm_dma_sync_sg_for_cpu;
	return 0;
}
core_initcall(brcm_setup_dma_ops);

/*
 * Provide cache details for PI/Nexus
 */
void brcm_get_cache_info(struct brcm_cache_info *info)
{
	info->max_writeback = cpu_scache_line_size() ? : cpu_dcache_line_size();
	info->max_writethrough = 0;
	info->prefetch_enabled = 0;

#if defined(CONFIG_BRCM_ZSCM_L2)
	/* chips with prefetching L2 */
	info->prefetch_enabled = 1;
#elif defined(CONFIG_BMIPS3300) || defined(CONFIG_BMIPS4380)
	/* chips with write-through RAC */
	info->max_writethrough = info->max_writeback * 4;
	info->prefetch_enabled = 1;
#endif
}
EXPORT_SYMBOL(brcm_get_cache_info);
