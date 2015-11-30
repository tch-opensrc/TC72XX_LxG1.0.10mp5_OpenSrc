/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000  Ani Joshi <ajoshi@unixbox.com>
 * Copyright (C) 2000, 2001, 06  Ralf Baechle <ralf@linux-mips.org>
 * swiped from i386, and cloned for MIPS by Geert, polished by Ralf.
 */

#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/string.h>
#include <linux/gfp.h>
#include <linux/highmem.h>

#include <asm/cache.h>
#include <asm/io.h>

#include <dma-coherence.h>

static unsigned long dma_addr_to_virt(dma_addr_t dma_addr)
{
	unsigned long addr = plat_dma_addr_to_phys(dma_addr);
	unsigned long virt = (unsigned long)phys_to_virt(addr);

	return virt;
}
static struct page *dma_addr_to_page(dma_addr_t dma_addr)
{
	return pfn_to_page(plat_dma_addr_to_phys(dma_addr) >> PAGE_SHIFT);
}

/*
 * Warning on the terminology - Linux calls an uncached area coherent;
 * MIPS terminology calls memory areas with hardware maintained coherency
 * coherent.
 */

static inline int cpu_is_noncoherent_r10000(struct device *dev)
{
	return !plat_device_is_coherent(dev) &&
	       (current_cpu_type() == CPU_R10000 ||
	       current_cpu_type() == CPU_R12000);
}

static gfp_t massage_gfp_flags(const struct device *dev, gfp_t gfp)
{
	/* ignore region specifiers */
	gfp &= ~(__GFP_DMA | __GFP_DMA32 | __GFP_HIGHMEM);

#ifdef CONFIG_ZONE_DMA
	if (dev == NULL)
		gfp |= __GFP_DMA;
	else if (dev->coherent_dma_mask < DMA_BIT_MASK(24))
		gfp |= __GFP_DMA;
	else
#endif
#ifdef CONFIG_ZONE_DMA32
	     if (dev->coherent_dma_mask < DMA_BIT_MASK(32))
		gfp |= __GFP_DMA32;
	else
#endif
		;

	/* Don't invoke OOM killer */
	gfp |= __GFP_NORETRY;

	return gfp;
}

void *dma_alloc_noncoherent(struct device *dev, size_t size,
	dma_addr_t * dma_handle, gfp_t gfp)
{
	void *ret;

	gfp = massage_gfp_flags(dev, gfp);

	ret = (void *) __get_free_pages(gfp, get_order(size));

	if (ret != NULL) {
		memset(ret, 0, size);
		*dma_handle = plat_map_dma_mem(dev, ret, size);
	}

	return ret;
}

EXPORT_SYMBOL(dma_alloc_noncoherent);

void *dma_alloc_coherent(struct device *dev, size_t size,
	dma_addr_t * dma_handle, gfp_t gfp)
{
	void *ret;

	gfp = massage_gfp_flags(dev, gfp);

	ret = (void *) __get_free_pages(gfp, get_order(size));

	if (ret) {
		memset(ret, 0, size);
		*dma_handle = plat_map_dma_mem(dev, ret, size);

		if (!plat_device_is_coherent(dev)) {
			dma_cache_wback_inv((unsigned long) ret, size);
			ret = UNCAC_ADDR(ret);
		}
	}

	return ret;
}

EXPORT_SYMBOL(dma_alloc_coherent);

void dma_free_noncoherent(struct device *dev, size_t size, void *vaddr,
	dma_addr_t dma_handle)
{
	plat_unmap_dma_mem(dev, dma_handle);
	free_pages((unsigned long) vaddr, get_order(size));
}

EXPORT_SYMBOL(dma_free_noncoherent);

void dma_free_coherent(struct device *dev, size_t size, void *vaddr,
	dma_addr_t dma_handle)
{
	unsigned long addr = (unsigned long) vaddr;

	plat_unmap_dma_mem(dev, dma_handle);

	if (!plat_device_is_coherent(dev))
		addr = CAC_ADDR(addr);

	free_pages(addr, get_order(size));
}

EXPORT_SYMBOL(dma_free_coherent);

static void __dma_sync_virtual(unsigned long addr, size_t size,
	enum dma_data_direction direction)
{
	switch (direction) {
	case DMA_TO_DEVICE:
		dma_cache_wback(addr, size);
		break;

	case DMA_FROM_DEVICE:
		dma_cache_inv(addr, size);
		break;

	case DMA_BIDIRECTIONAL:
		dma_cache_wback_inv(addr, size);
		break;

	default:
		BUG();
	}
}

void __dma_sync_pages(struct page *page,
		      unsigned long offset,
		      size_t size,
		      enum dma_data_direction direction)
{
	size_t left = size;

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

			addr = kmap_atomic(page, 0);
			__dma_sync_virtual((unsigned long)addr + offset,
					   len,
					   direction);
			kunmap_atomic(addr, 0);
		} else
			__dma_sync_virtual((unsigned long)page_address(page) + offset,
					   size,
					   direction);
		offset = 0;
		page++;
		left -= len;
	} while (left);
}

dma_addr_t dma_map_single(struct device *dev, void *ptr, size_t size,
	enum dma_data_direction direction)
{
	unsigned long addr = (unsigned long) ptr;

	if (!plat_device_is_coherent(dev))
		__dma_sync_virtual(addr, size, direction);

	return plat_map_dma_mem(dev, ptr, size);
}

EXPORT_SYMBOL(dma_map_single);

void dma_unmap_single(struct device *dev, dma_addr_t dma_addr, size_t size,
	enum dma_data_direction direction)
{
	if (cpu_is_noncoherent_r10000(dev))
		__dma_sync_virtual(dma_addr_to_virt(dma_addr), size, direction);

	plat_unmap_dma_mem(dev, dma_addr);
}

EXPORT_SYMBOL(dma_unmap_single);

int dma_map_sg(struct device *dev, struct scatterlist *sg, int nents,
	enum dma_data_direction direction)
{
	int i;
	unsigned long	addr;
	dma_addr_t	paddr;
	struct page 	*pg;

	BUG_ON(direction == DMA_NONE);

	for (i = 0; i < nents; i++, sg++) {
		addr = (unsigned long) sg_virt(sg);
		pg = sg_page(sg);
		if (!plat_device_is_coherent(dev)) {
			if (addr) {
				__dma_sync_virtual(addr, sg->length, direction);
			}
			else
				__dma_sync_pages(pg, sg->offset, sg->length, direction);
		}
		paddr = plat_map_dma_mem_page(dev, pg);
		sg->dma_address =  paddr + sg->offset;
	}

	return nents;
}

EXPORT_SYMBOL(dma_map_sg);

dma_addr_t dma_map_page(struct device *dev, struct page *page,
	unsigned long offset, size_t size, enum dma_data_direction direction)
{
	BUG_ON(direction == DMA_NONE);

	if (!plat_device_is_coherent(dev)) {
		__dma_sync_pages(page, offset, size, direction);
	}
	return plat_map_dma_mem_page(dev, page) + offset;
}

EXPORT_SYMBOL(dma_map_page);

void dma_unmap_sg(struct device *dev, struct scatterlist *sg, int nhwentries,
	enum dma_data_direction direction)
{
	int i;

	BUG_ON(direction == DMA_NONE);

	for (i = 0; i < nhwentries; i++, sg++) {
		if (!plat_device_is_coherent(dev) &&
		    direction != DMA_TO_DEVICE) {
			__dma_sync_pages(sg_page(sg),
					 sg->offset,
					 sg->length,
					 direction);
		}
		plat_unmap_dma_mem(dev, sg->dma_address);
	}
}

EXPORT_SYMBOL(dma_unmap_sg);

void dma_sync_single_for_cpu(struct device *dev, dma_addr_t dma_handle,
	size_t size, enum dma_data_direction direction)
{
	BUG_ON(direction == DMA_NONE);

	if (cpu_is_noncoherent_r10000(dev)) {
		unsigned long addr = dma_addr_to_virt(dma_handle);
		if (addr)
			__dma_sync_virtual(addr, size, direction);
		else
			__dma_sync_pages(dma_addr_to_page(dma_handle),
					 dma_handle & ~PAGE_MASK,
					 size,
					 direction);
	}
}

EXPORT_SYMBOL(dma_sync_single_for_cpu);

void dma_sync_single_for_device(struct device *dev, dma_addr_t dma_handle,
	size_t size, enum dma_data_direction direction)
{
	BUG_ON(direction == DMA_NONE);

	plat_extra_sync_for_device(dev);
	if (!plat_device_is_coherent(dev)) {
		unsigned long addr = dma_addr_to_virt(dma_handle);
		if (addr)
			__dma_sync_virtual(addr, size, direction);
		else
			__dma_sync_pages(dma_addr_to_page(dma_handle),
					 dma_handle & ~PAGE_MASK,
					 size,
					 direction);
	}
}

EXPORT_SYMBOL(dma_sync_single_for_device);

void dma_sync_single_range_for_cpu(struct device *dev, dma_addr_t dma_handle,
	unsigned long offset, size_t size, enum dma_data_direction direction)
{
	BUG_ON(direction == DMA_NONE);

	if (cpu_is_noncoherent_r10000(dev)) {
		unsigned long addr = dma_addr_to_virt(dma_handle);
		if (addr)
			__dma_sync_virtual(addr + offset, size, direction);
		else
			__dma_sync_pages(dma_addr_to_page(dma_handle),
					 offset,
					 size,
					 direction);
	}
}

EXPORT_SYMBOL(dma_sync_single_range_for_cpu);

void dma_sync_single_range_for_device(struct device *dev, dma_addr_t dma_handle,
	unsigned long offset, size_t size, enum dma_data_direction direction)
{
	BUG_ON(direction == DMA_NONE);

	plat_extra_sync_for_device(dev);
	if (!plat_device_is_coherent(dev)) {
		unsigned long addr = dma_addr_to_virt(dma_handle);
		if (addr)
			__dma_sync_virtual(addr + offset, size, direction);
		else
			__dma_sync_pages(dma_addr_to_page(dma_handle),
					 offset,
					 size,
					 direction);
	}
}

EXPORT_SYMBOL(dma_sync_single_range_for_device);

void dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg, int nelems,
	enum dma_data_direction direction)
{
	int i;

	BUG_ON(direction == DMA_NONE);

	/* Make sure that gcc doesn't leave the empty loop body.  */
	for (i = 0; i < nelems; i++, sg++) {
		if (cpu_is_noncoherent_r10000(dev))
			__dma_sync_pages(sg_page(sg),
					 sg->offset,
					 sg->length,
					 direction);
	}
}

EXPORT_SYMBOL(dma_sync_sg_for_cpu);

void dma_sync_sg_for_device(struct device *dev, struct scatterlist *sg, int nelems,
	enum dma_data_direction direction)
{
	int i;

	BUG_ON(direction == DMA_NONE);

	/* Make sure that gcc doesn't leave the empty loop body.  */
	for (i = 0; i < nelems; i++, sg++) {
		if (!plat_device_is_coherent(dev))
			__dma_sync_pages(sg_page(sg),
					 sg->offset,
					 sg->length,
					 direction);
	}
}

EXPORT_SYMBOL(dma_sync_sg_for_device);

int dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	return plat_dma_mapping_error(dev, dma_addr);
}

EXPORT_SYMBOL(dma_mapping_error);

int dma_supported(struct device *dev, u64 mask)
{
	return plat_dma_supported(dev, mask);
}

EXPORT_SYMBOL(dma_supported);

int dma_is_consistent(struct device *dev, dma_addr_t dma_addr)
{
	return plat_device_is_coherent(dev);
}

EXPORT_SYMBOL(dma_is_consistent);

void dma_cache_sync(struct device *dev, void *vaddr, size_t size,
	       enum dma_data_direction direction)
{
	BUG_ON(direction == DMA_NONE);

	plat_extra_sync_for_device(dev);
	if (!plat_device_is_coherent(dev))
		__dma_sync_virtual((unsigned long)vaddr, size, direction);
}

EXPORT_SYMBOL(dma_cache_sync);
