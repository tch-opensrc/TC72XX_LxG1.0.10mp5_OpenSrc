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

#ifndef _ASM_BRCMSTB_BRCMAPI_H
#define _ASM_BRCMSTB_BRCMAPI_H

#ifdef __KERNEL__

#include <linux/spinlock.h>
#include <linux/skbuff.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/cache.h>
#include <linux/slab.h>
#include <linux/mm_types.h>
#include <linux/scatterlist.h>

#include <net/sock.h>
#include <asm/ptrace.h>
#include <asm/inst.h>
#include <asm/bug.h>
#include <asm/page.h>

/***********************************************************************
 * Kernel hooks
 ***********************************************************************/

#define BRCM_RX_NUM_HOOKS	2
#define BRCM_RX_HOOK_NETACCEL	0
#define BRCM_RX_HOOK_EROUTER	1

void __init brcm_free_bootmem(unsigned long addr, unsigned long size);
void brcm_tlb_init(void);
unsigned long brcm_setup_ebase(void);
int brcm_simulate_opcode(struct pt_regs *regs, unsigned int opcode);
int brcm_unaligned_fp(void __user *addr, union mips_instruction *insn,
	struct pt_regs *regs);
extern int (*brcm_netif_rx_hook[BRCM_RX_NUM_HOOKS])(struct sk_buff *);
int brcm_cacheflush(unsigned long addr, unsigned long bytes,
	unsigned int cache);
void brcm_sync_for_cpu(struct device *dev, dma_addr_t dma_handle, size_t size,
	enum dma_data_direction dir);
void brcm_sync_for_cpu_sg(struct scatterlist *sg, enum dma_data_direction dir);
unsigned long brcm_fixup_ticks(unsigned long delta);
extern unsigned long brcm_adj_cpu_khz;
int brcm_map_coherent(dma_addr_t dma_handle, void *cac_va, size_t size,
	void **uncac_va, gfp_t gfp);
void *brcm_unmap_coherent(void *vaddr);

/***********************************************************************
 * BSP External API
 ***********************************************************************/

/* RAC extension to sys_cacheflush() */
#define	RACACHE	(1<<31)		/* read-ahead cache / prefetching L2 */

struct brcm_cache_info {
	int			max_writeback;
	int			max_writethrough;
	int			prefetch_enabled;
};

struct brcm_ocap_info {
	unsigned long		ocap_part_start;
	unsigned long		ocap_part_len;
	int			docsis_platform;
};

void brcm_inv_prefetch(unsigned long addr, unsigned long size);
void brcm_get_cache_info(struct brcm_cache_info *info);

void brcm_get_ocap_info(struct brcm_ocap_info *info);

static inline void brcm_netif_rx_sethook(unsigned int id,
		int (*fn)(struct sk_buff *)) {
	BUG_ON(id >= BRCM_RX_NUM_HOOKS);
	brcm_netif_rx_hook[id] = fn;
}

int brcm_alloc_macaddr(u8 *buf);

extern spinlock_t brcm_magnum_spinlock;

void brcm_set_nmi_handler(void (*fn)(struct pt_regs *));

/***********************************************************************
 * New exports of standard kernel symbols
 ***********************************************************************/

/* for netaccel IPv6 */
extern struct proto udpv6_prot;

/* for performance tuning */
extern void (*cpu_wait)(void);

/* for power management interrupts */
extern unsigned long ebase;

/* for GENET ring buffer support */
extern struct kmem_cache *skbuff_head_cache __read_mostly;

/***********************************************************************
 * Driver->BSP callbacks
 ***********************************************************************/

int brcm_pm_register_cb(char *name, int (*fn)(int, void *), void *arg);
int brcm_pm_unregister_cb(char *name);

struct brcm_wakeup_ops {
	int	(*enable)(void *ref);
	int	(*disable)(void *ref);
	int	(*poll)(void *ref); /* returns 1 if a wakeup event occurred */
};

int brcm_pm_wakeup_register(struct brcm_wakeup_ops *ops, void* ref, char* name);
int brcm_pm_wakeup_unregister(struct brcm_wakeup_ops *ops, void* ref);

#endif /* __KERNEL__ */

#endif /* _ASM_BRCMSTB_BRCMAPI_H */
