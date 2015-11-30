/*
 * Copyright (C) 2011 Broadcom Corporation
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
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>

#include <asm/mipsregs.h>
#include <asm/cacheflush.h>
#include <asm/brcmstb/brcmstb.h>

#ifdef BCHP_MEM_DMA_0_CTRL

struct brcm_mem_dma_descr {
	u32     read_addr;
	u32     write_addr;
	u32     word[6];
};

#define DESCR_WORD2     0
#define DESCR_WORD3     1
#define DESCR_WORD4     2

#define BRCM_MEM_DMA_TIMEOUT	(0)	/* no timeout */

#define BRCM_MEM_DMA_DEBUG	(0)

static int brcm_mem_dma_prepare_transfer(struct brcm_mem_transfer *xfer);
static int brcm_mem_dma_complete_transfer(struct brcm_mem_transfer *xfer);
static void brcm_mem_dma_start_transfer(void);

#if BRCM_MEM_DMA_DEBUG
#define DBG			printk
static void brcm_mem_dma_dump_regs(const char *title)
{
	printk(KERN_DEBUG "=== MEM DMA registers ===\n");
	printk(KERN_DEBUG
	       "FIRST_DESC = 0x%08x CTRL       = 0x%08x WAKE_CTRL  = 0x%08x\n",
	       (unsigned int)BDEV_RD(BCHP_MEM_DMA_0_FIRST_DESC),
	       (unsigned int)BDEV_RD(BCHP_MEM_DMA_0_CTRL),
	       (unsigned int)BDEV_RD(BCHP_MEM_DMA_0_WAKE_CTRL));
	printk(KERN_DEBUG
	       "STATUS     = 0x%08x CUR_DESC   = 0x%08x CUR_BYTE   = 0x%08x\n",
	       (unsigned int)BDEV_RD(BCHP_MEM_DMA_0_STATUS),
	       (unsigned int)BDEV_RD(BCHP_MEM_DMA_0_CUR_DESC),
	       (unsigned int)BDEV_RD(BCHP_MEM_DMA_0_CUR_BYTE));
}

static void brcm_mem_dma_dump_descr(struct brcm_mem_dma_descr *d, int num)
{
	int index = 0, last = 0;
	printk(KERN_DEBUG "=== MEM DMA descriptors [%p] ===\n", d);
	do {
		last = d->word[DESCR_WORD2] &
			BCHP_MEM_DMA_DESC_WORD2_LAST_MASK;
		printk(KERN_DEBUG "[%02d]: s  = 0x%08x d  = 0x%08x "
			"w2 = 0x%08x w3 = 0x%08x w4 = 0x%08x\n",
			index++, d->read_addr, d->write_addr,
			d->word[0], d->word[1], d->word[2]);
		d++;
		if (!--num)
			break;
	} while (!last);
}
#else
#define DBG(...)		do { } while (0)
static inline void brcm_mem_dma_dump_regs(const char *title) {}
static inline void brcm_mem_dma_dump_descr(struct brcm_mem_dma_descr *d,
	int num) {}
#endif

/***********************************************************************
 * Low-level M2M utilities
 ***********************************************************************/
static inline void brcm_mem_dma_set_raddr(struct brcm_mem_dma_descr *descr,
	u32 addr)
{
	descr->read_addr = addr;
}

static inline void brcm_mem_dma_set_waddr(struct brcm_mem_dma_descr *descr,
	u32 addr)
{
	descr->write_addr = addr;
}

/***********************************************************************
 * Enable/disable completion interrupt for descriptor
 ***********************************************************************/
static inline void brcm_mem_dma_interrupt_enable(
	struct brcm_mem_dma_descr *descr, int enable)
{
	if (enable)
		descr->word[DESCR_WORD2] |=
			BCHP_MEM_DMA_DESC_WORD2_INTR_ENABLE_MASK;
	else
		descr->word[DESCR_WORD2] &=
			~BCHP_MEM_DMA_DESC_WORD2_INTR_ENABLE_MASK;
}

/***********************************************************************
 * Set transfer size for descriptor
 ***********************************************************************/
static inline void brcm_mem_dma_set_xfer_size(struct brcm_mem_dma_descr *descr,
	u32 len)
{
	len &= BCHP_MEM_DMA_DESC_WORD2_TRANSFER_SIZE_MASK;
	descr->word[DESCR_WORD2] &=
		~BCHP_MEM_DMA_DESC_WORD2_TRANSFER_SIZE_MASK;
	descr->word[DESCR_WORD2] |= len;
}

/***********************************************************************
 * Link to the next descriptor
 * next - virtual address of the descriptor to link with
 ***********************************************************************/
static inline void brcm_mem_dma_link_next(struct brcm_mem_dma_descr *descr,
	struct brcm_mem_dma_descr *next)
{
	u32 pa_next = 0;

	if (next) {
		pa_next = virt_to_phys(next) &
			BCHP_MEM_DMA_DESC_WORD3_NEXT_DESC_ADDR_MASK;
		descr->word[DESCR_WORD2] &= ~BCHP_MEM_DMA_DESC_WORD2_LAST_MASK;
	} else
		descr->word[DESCR_WORD2] |= BCHP_MEM_DMA_DESC_WORD2_LAST_MASK;

	descr->word[DESCR_WORD3] &=
		~BCHP_MEM_DMA_DESC_WORD3_NEXT_DESC_ADDR_MASK;
	descr->word[DESCR_WORD3] |= pa_next;

}

/***********************************************************************
 * Configure SCRAM
 * TODO: encoder/decoder init bit ???
 ***********************************************************************/
static inline void brcm_mem_dma_set_scram(struct brcm_mem_dma_descr *descr,
	int enable, int start, int end, int mode, u8 slot, int init)
{
	u32 word4 = 0;
	word4 |= enable ? BCHP_MEM_DMA_DESC_WORD4_SG_ENABLE_MASK : 0;
	word4 |= start  ? BCHP_MEM_DMA_DESC_WORD4_SG_SCRAM_START_MASK : 0;
	word4 |= end    ? BCHP_MEM_DMA_DESC_WORD4_SG_SCRAM_END_MASK : 0;
	word4 |= (mode << BCHP_MEM_DMA_DESC_WORD4_MODE_SEL_SHIFT) &
		BCHP_MEM_DMA_DESC_WORD4_MODE_SEL_MASK;
	word4 |= (slot * 6) & BCHP_MEM_DMA_DESC_WORD4_KEY_SELECT_MASK;

	if (mode && init)
		word4 |= BCHP_MEM_DMA_DESC_WORD4_ENC_DEC_INIT_MASK;

	descr->word[DESCR_WORD4] = word4;
}

/***********************************************************************
 * Poll for transfer completion
 ***********************************************************************/
static int brcm_mem_dma_wait_for_ready(int timeout)
{
	while (BDEV_RD_F(MEM_DMA_0_STATUS, DMA_STATUS) == 1) {
		if (timeout > 0)
			if (!--timeout)
				return -1;
		mdelay(1);
	}
	return 0;
}

static int brcm_mem_dma_num_descriptors(struct brcm_mem_transfer *xfer)
{
	int xfer_cnt = 0;

	do {
		xfer_cnt++;
	} while ((xfer = xfer->next));

	return xfer_cnt;
}

/***********************************************************************
 * Generate descriptor according to transfer request
 * Returns 0 on success, -1 on incorrect request parameters
 ***********************************************************************/
static int brcm_mem_dma_gen_descriptor(struct brcm_mem_transfer *xfer,
	struct brcm_mem_dma_descr *d, int index, int total)
{
	int last = index == total - 1;

	/* sanity checks */
	if (xfer->len > 0x1000000) {
		printk(KERN_ERR "MEM DMA "
			"does not support transfers bigger than 16 MByte\n");
		return -1;
	}
	if ((xfer->pa_dst > xfer->pa_src) &&
	    (xfer->pa_dst < (xfer->pa_src + xfer->len))) {
		printk(KERN_ERR "MEM DMA "
		"does not support overlapping read-write regions\n");
		return -1;
	}

	/* set up mem-dma descriptor */
	brcm_mem_dma_set_raddr(d, xfer->pa_src);
	brcm_mem_dma_set_waddr(d, xfer->pa_dst);
	brcm_mem_dma_set_xfer_size(d, xfer->len);
	brcm_mem_dma_set_scram(d, 1, index == 0, last,
		xfer->mode, xfer->key, index == 0);
	brcm_mem_dma_link_next(d, last ? NULL : d + 1);

	return 0;
}

/***********************************************************************
 * If caller remapped the buffers, it set pa_src and pa_dst fields
 * of transfer request. If these fields are not set, we need to remap
 * them here.
 * To properly unmap the buffers after the transfer, set bit flags
 * in the transfer if we are doing remapping. Caller should never touch
 * these flags.
 ***********************************************************************/
static int brcm_mem_dma_map_buffers(struct brcm_mem_transfer *xfer)
{
	if (!xfer->len)
		return -1;

	if (!xfer->pa_src && !xfer->pa_dst && (xfer->src == xfer->dst)) {
		xfer->pa_dst = xfer->pa_src =
			dma_map_single(NULL, xfer->src, xfer->len,
				DMA_BIDIRECTIONAL);
		if (dma_mapping_error(NULL, xfer->pa_dst))
			return -1;
		xfer->src_dst_remapped = 1;
	}

	if (!xfer->pa_src) {
		xfer->pa_src = dma_map_single(NULL, xfer->src, xfer->len,
			DMA_TO_DEVICE);
		if (dma_mapping_error(NULL, xfer->pa_src))
			return -1;
		xfer->src_remapped = 1;
	}

	if (!xfer->pa_dst) {
		xfer->pa_dst = dma_map_single(NULL, xfer->dst, xfer->len,
			DMA_FROM_DEVICE);
		if (dma_mapping_error(NULL, xfer->pa_dst))
			return -1;
		xfer->dst_remapped = 1;
	}

	return 0;
}

/***********************************************************************
 * Unmap the buffers if we mapped them before
 ***********************************************************************/
static void brcm_mem_dma_unmap_buffers(struct brcm_mem_transfer *xfer)
{
	if (xfer->src_dst_remapped) {
		dma_unmap_single(NULL, xfer->pa_src, xfer->len,
				DMA_BIDIRECTIONAL);
		return;
	}

	if (xfer->src_remapped) {
		dma_unmap_single(NULL, xfer->pa_src, xfer->len,
			DMA_TO_DEVICE);
	}
	if (xfer->dst_remapped) {
		dma_unmap_single(NULL, xfer->pa_dst, xfer->len,
			DMA_FROM_DEVICE);
	}
}

/***********************************************************************
 * Map the descriptors and write the first descriptor address to the
 * M2M register. Do not initiate transfer yet.
 ***********************************************************************/
static dma_addr_t brcm_mem_dma_prepare_descriptors(
	struct brcm_mem_dma_descr *d, int size)
{
	dma_addr_t pa_d;
	pa_d = dma_map_single(NULL, d, size, DMA_TO_DEVICE);
	brcm_mem_dma_wait_for_ready(0);
	BDEV_WR_RB(BCHP_MEM_DMA_0_FIRST_DESC, pa_d);

	return pa_d;
}

/***********************************************************************
 * Unmap the descriptor after completion
 ***********************************************************************/
static void brcm_mem_dma_free_descriptors(dma_addr_t pa, int size)
{
	brcm_mem_dma_wait_for_ready(0);
	dma_unmap_single(NULL, pa, size, DMA_TO_DEVICE);
}

static struct brcm_mem_dma_descr *cur_descr;
static int cur_total_size;
static dma_addr_t cur_pa_d;

/***********************************************************************
 * _brcm_mem_dma_start_transfer
 * Initiate M2M transfer. If sync is set, wait for completion with timeout.
 ***********************************************************************/
static void _brcm_mem_dma_start_transfer(int on, int sync)
{
	/* clear RUN bit and wait for IDLE */
	BDEV_WR_F_RB(MEM_DMA_0_CTRL, RUN, 0);
	brcm_mem_dma_wait_for_ready(BRCM_MEM_DMA_TIMEOUT);

	if (on) {
		BDEV_WR_F_RB(MEM_DMA_0_CTRL, RUN, 1);
		if (sync)
			brcm_mem_dma_wait_for_ready(BRCM_MEM_DMA_TIMEOUT);
	}
}

/***********************************************************************
 * brcm_mem_dma_start_transfer
 * Initiate M2M transfer
 ***********************************************************************/
static void brcm_mem_dma_start_transfer(void)
{
	DBG("%s:%d\n", __func__, __LINE__);
	_brcm_mem_dma_start_transfer(1, 1);
}

/***********************************************************************
 * brcm_mem_dma_prepare_transfer
 * Generates mem dma descriptors, flushes them to memory
 * and sets up M2M registers, but _DOES NOT_ initiate the transfer.
 * Use brcm_mem_dma_start_transfer() to start transfer/encoding
 * This allows S3 standby code to be sure code does not attempt to change
 * memory content after it has been encoded
 ***********************************************************************/
static int brcm_mem_dma_prepare_transfer(struct brcm_mem_transfer *xfer)
{
	int xfer_cnt = brcm_mem_dma_num_descriptors(xfer);
	int total_size = xfer_cnt * sizeof(struct brcm_mem_dma_descr);
	int index = 0;
	struct brcm_mem_transfer *save_xfer = xfer;
	struct brcm_mem_dma_descr *d =
		kzalloc(total_size, GFP_ATOMIC), *cur_d;

	DBG("%s:%d\n", __func__, __LINE__);

	WARN_ON(cur_descr);

	if (unlikely(!d))
		return -1;

	cur_descr = cur_d = d;

	do {
		if (brcm_mem_dma_map_buffers(xfer)) {
			printk(KERN_ERR "%s: cannot map buffers", __func__);
			goto _error;
		}

		if (brcm_mem_dma_gen_descriptor(xfer, cur_d,
		    index++, xfer_cnt)) {
			printk(KERN_ERR "%s: cannot generate descr", __func__);
			goto _error;
		}

		xfer = xfer->next;
		cur_d++;
	} while (xfer);

	brcm_mem_dma_dump_descr(d, xfer_cnt);
	/* flush the descriptors */
	cur_pa_d = brcm_mem_dma_prepare_descriptors(d, total_size);
	cur_total_size = total_size;

	return 0;

_error:
	brcm_mem_dma_complete_transfer(save_xfer);
	return -1;
}

/***********************************************************************
 * brcm_mem_dma_complete_transfer
 * Must be called after M2M transfer was complete
 * Unmaps everything that was mapped in brcm_mem_dma_prepare_transfer,
 * frees up all memory
 ***********************************************************************/
static int brcm_mem_dma_complete_transfer(struct brcm_mem_transfer *xfer)
{
	WARN_ON(cur_descr == NULL);
	WARN_ON(!cur_pa_d);
	WARN_ON(!cur_total_size);

	DBG("%s:%d\n", __func__, __LINE__);

	/* clear RUN bit and wait for IDLE */
	_brcm_mem_dma_start_transfer(0, 1);

	brcm_mem_dma_free_descriptors(cur_pa_d, cur_total_size);

	do {
		brcm_mem_dma_unmap_buffers(xfer);
		xfer = xfer->next;
	} while (xfer);

	kfree(cur_descr);

	cur_descr = NULL;
	cur_total_size = 0;
	cur_pa_d = 0;

	return 0;
}

/***********************************************************************
 * brcm_mem_dma_simple_transfer
 * Initiates a single M2M tranfser according to transfer request
 ***********************************************************************/
int brcm_mem_dma_simple_transfer(struct brcm_mem_transfer *xfer)
{
	int retval = 0;
	struct brcm_mem_dma_descr *d =
		kzalloc(sizeof(struct brcm_mem_dma_descr), GFP_ATOMIC);

	DBG("%s:%d\n", __func__, __LINE__);

	WARN_ON(brcm_mem_dma_num_descriptors(xfer) != 1);

	if (d) {
		u32 pa_d;

		/* remap the buffers unless the caller has done that */
		if (brcm_mem_dma_map_buffers(xfer)) {
			retval = -1;
			goto _error;
		}
		/* set up mem-dma descriptor */
		if (!brcm_mem_dma_gen_descriptor(xfer, d, 0, 1)) {

			brcm_mem_dma_dump_descr(d, 1);
			/* flush the descriptor */
			pa_d = brcm_mem_dma_prepare_descriptors(d,
				sizeof(struct brcm_mem_dma_descr));
			_brcm_mem_dma_start_transfer(1, 1);
			brcm_mem_dma_dump_regs(__func__);
			_brcm_mem_dma_start_transfer(0, 0);
			brcm_mem_dma_free_descriptors(pa_d,
				sizeof(struct brcm_mem_dma_descr));
		}

		brcm_mem_dma_unmap_buffers(xfer);
_error:
		kfree(d);
		return retval;
	}

	return -1;
}

/***********************************************************************
 * brcm_mem_dma_transfer_sg
 * Scatter-gather version of M2M tranfser
 ***********************************************************************/
int brcm_mem_dma_transfer_sg(struct brcm_mem_transfer *xfer)
{
	printk(KERN_WARNING "%s: NOT IMPLEMENTED YET\n", __func__);
	return -1;
}

/***********************************************************************
 * brcm_mem_dma_transfer
 * Initiates a M2M tranfser according to transfer request. Chained transfer
 * will generate several descriptors.
 ***********************************************************************/
int brcm_mem_dma_transfer(struct brcm_mem_transfer *xfer)
{
	struct brcm_mem_dma_descr *d;
	int index = 0, retval = 0;
	struct brcm_mem_dma_descr *cur_d;
	int xfer_cnt = brcm_mem_dma_num_descriptors(xfer), total_size;
	struct brcm_mem_transfer *xfer_save = xfer;
	dma_addr_t pa_d;

	DBG("%s:%d\n", __func__, __LINE__);

	total_size = xfer_cnt * sizeof(struct brcm_mem_dma_descr);

	d = kzalloc(total_size, GFP_ATOMIC);
	if (!d)
		return -1;

	cur_d = d;
	do {
		/* remap the buffers unless the caller has done that */
		if (brcm_mem_dma_map_buffers(xfer)) {
			retval = -1;
			goto _done;
		}

		if (brcm_mem_dma_gen_descriptor(xfer, cur_d,
		    index++, xfer_cnt)) {
			retval = -1;
			goto _done;
		}

		xfer = xfer->next;
		cur_d++;
	} while (xfer);

	brcm_mem_dma_dump_descr(d, xfer_cnt);

	/* flush the descriptors */
	pa_d = brcm_mem_dma_prepare_descriptors(d, total_size);
	brcm_mem_dma_dump_regs(__func__);
	_brcm_mem_dma_start_transfer(1, 1);
	_brcm_mem_dma_start_transfer(0, 0);

	brcm_mem_dma_free_descriptors(pa_d, total_size);

_done:
	xfer = xfer_save;
	do {
		brcm_mem_dma_unmap_buffers(xfer);
		xfer = xfer->next;
	} while (xfer);

	kfree(d);

	return retval;
}
#else
static int brcm_mem_dma_prepare_transfer(struct brcm_mem_transfer *xfer)
{
	return -1;
}
static int brcm_mem_dma_complete_transfer(struct brcm_mem_transfer *xfer)
{
	return -1;
}
static void brcm_mem_dma_start_transfer(void) {}

int brcm_mem_dma_simple_transfer(struct brcm_mem_transfer *xfer)
{
	return -1;
}

int brcm_mem_dma_transfer(struct brcm_mem_transfer *xfer)
{
	return -1;
}

#endif /* BCHP_MEM_DMA_0_CTRL */

static struct brcm_dram_encoder_ops m2m_ops = {
	.prepare = brcm_mem_dma_prepare_transfer,
	.start = brcm_mem_dma_start_transfer,
	.complete = brcm_mem_dma_complete_transfer,
};

static int brcm_mem_dma_init(void)
{
	/* TODO: request interrupt */
	/* register encoder with power management subsystem */
	brcm_pm_set_dram_encoder(&m2m_ops);
	return 0;
}
late_initcall(brcm_mem_dma_init);
