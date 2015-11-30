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
#include <linux/types.h>
#include <linux/compiler.h>

#include <asm/mipsregs.h>
#include <asm/brcmstb/brcmstb.h>
#include <asm/tlbflush.h>
#include <asm/sections.h>

#define MAX_GP_REGS	16
#define MAX_CP0_REGS	32
#define MAX_IO_REGS	(256 - MAX_GP_REGS - MAX_CP0_REGS)

#if 0
#define DBG		printk
#else
#define DBG(...)	do { } while (0)
#endif

#define CALCULATE_MEM_HASH		(0)
#define WARM_BOOT_MAGIC_WORD		(0x5AFEB007)
#define CFE_PARAMETER_BLOCK		(0)
#define AUTHENTICATION_REGION_SIZE	(16*1024*1024)

#define AON_CTRL_HASH_START_INDEX	(CFE_PARAMETER_BLOCK+5)
#define AON_SAVE_CPB(idx, val) \
	BDEV_WR_RB(AON_RAM_BASE + ((CFE_PARAMETER_BLOCK+idx)<<2), val)
#define AON_READ_CPB(idx) \
	BDEV_RD(AON_RAM_BASE + ((CFE_PARAMETER_BLOCK+idx)<<2))
#define AON_SAVE_HASH(idx, val) \
	BDEV_WR_RB(AON_RAM_BASE + ((AON_CTRL_HASH_START_INDEX+idx)<<2), val)
#define AON_READ_HASH(idx) \
	BDEV_RD(AON_RAM_BASE + ((AON_CTRL_HASH_START_INDEX+idx)<<2))

struct cp0_value {
	u32	value;
	u32	index;
	u32	select;
};

struct	brcm_pm_s3_context {
	u32			gp_regs[MAX_GP_REGS];
	struct cp0_value	cp0_regs[MAX_CP0_REGS];
	int			cp0_regs_idx;
	u32			memc0_rts[NUM_MEMC_CLIENTS];
};

struct brcm_pm_s3_context s3_context;

asmlinkage int brcm_pm_s3_standby_asm(unsigned long options,
	void (*dram_encoder_start)(void), int dcache_linesz);
extern int s3_reentry;

extern void brcmstb_enable_xks01(void);

static void __raw_uart_putc(char c);

static void brcm_pm_dump_context(struct brcm_pm_s3_context *cxt);

#define CPO_SAVE_INFO(cxt, ci, idx, _select) \
do { \
	cxt->cp0_regs[ci].index = idx; \
	cxt->cp0_regs[ci].select = _select; \
	cxt->cp0_regs[ci++].value = read_c0_reg(idx, _select); \
} while (0);
#define read_c0_reg(n, s)	__read_32bit_c0_register($##n, s)
#define write_c0_reg(n, s, v)	__write_32bit_c0_register($##n, s, v)

static __maybe_unused void brcm_pm_init_cp0_context(
		struct brcm_pm_s3_context *cxt)
{
	cxt->cp0_regs_idx = 0;
}

static __maybe_unused void brcm_pm_save_cp0_context(
		struct brcm_pm_s3_context *cxt)
{
	int ci = cxt->cp0_regs_idx;

	CPO_SAVE_INFO(cxt, ci, 4, 0); /* context */
	CPO_SAVE_INFO(cxt, ci, 4, 2); /* userlocal */
	CPO_SAVE_INFO(cxt, ci, 5, 0); /* pagemask */
	CPO_SAVE_INFO(cxt, ci, 7, 0); /* hwrena */
	CPO_SAVE_INFO(cxt, ci, 11, 0); /* compare */
	CPO_SAVE_INFO(cxt, ci, 12, 0); /* status */

	/* Broadcom specific */
	CPO_SAVE_INFO(cxt, ci, 22, 0); /* config */
	CPO_SAVE_INFO(cxt, ci, 22, 1); /* mode */
	CPO_SAVE_INFO(cxt, ci, 22, 3); /* eDSP */

	cxt->cp0_regs_idx = ci;
}

static __maybe_unused void brcm_pm_restore_cp0_context(
		struct brcm_pm_s3_context *cxt)
{
	int ci = cxt->cp0_regs_idx;

	/* Broadcom specific */
	write_c0_reg(22, 3, cxt->cp0_regs[--ci].value); /* eDSP */
	write_c0_reg(22, 1, cxt->cp0_regs[--ci].value); /* mode */
	write_c0_reg(22, 0, cxt->cp0_regs[--ci].value); /* config */

	write_c0_reg(12, 0, cxt->cp0_regs[--ci].value); /* status */
	write_c0_reg(11, 0, cxt->cp0_regs[--ci].value); /* compare */
	write_c0_reg(7, 0, cxt->cp0_regs[--ci].value); /* hwrena */
	write_c0_reg(5, 0, cxt->cp0_regs[--ci].value); /* pagemask */
	write_c0_reg(4, 2, cxt->cp0_regs[--ci].value); /* userlocal */
	write_c0_reg(4, 0, cxt->cp0_regs[--ci].value); /* context */

}

static __maybe_unused void brcm_pm_cmp_context(
		struct brcm_pm_s3_context *cxt,
		struct brcm_pm_s3_context *cxt2, const char* title)
{
	int i;
	int identical = 1;
	if (title)
		DBG("%s\n", title);
	if (cxt->cp0_regs_idx != cxt2->cp0_regs_idx) {
		identical = 0;
		DBG("CP0 reg # is different: %d %d\n",
			cxt->cp0_regs_idx, cxt2->cp0_regs_idx);
	} else {
		for (i = 0; i < cxt->cp0_regs_idx; i++) {
			if (cxt->cp0_regs[i].value
					!= cxt2->cp0_regs[i].value) {
				identical = 0;
				DBG("\tCP0[%02d.%01d]:  %08x != %08x\n",
					cxt->cp0_regs[i].index,
					cxt->cp0_regs[i].select,
					cxt->cp0_regs[i].value,
					cxt2->cp0_regs[i].value);
			}
		}
	}
	if (identical)
		DBG("Contexts are identical\n");
	else {
		brcm_pm_dump_context(cxt);
		brcm_pm_dump_context(cxt2);
	}
}

static void brcm_pm_dump_context(struct brcm_pm_s3_context *cxt)
{
	int i;
	DBG("CPO:\n");
	for (i = 0; i < cxt->cp0_regs_idx; i++) {
		DBG("[%02d.%0d]=%08x ",
			cxt->cp0_regs[i].index,
			cxt->cp0_regs[i].select,
			cxt->cp0_regs[i].value);
		if (i%8 == 7)
			DBG("\n");
	}
	DBG("\n\n");
}

/***********************************************************************
 * Encryption setup for S3 suspend
 ***********************************************************************/
#if CALCULATE_MEM_HASH
/*
 * Default scheme encrypts specified memory region and writes encoded data
 * to a temporary buffer, overwriting previous data on every block write.
 * Upon encoding final block, last 16 bytes are stored into AON, then
 * entire temporary buffer is wiped out
 * Bootloader will repeat encoding immediately after S3 wakeup to see
 * if hash value matches.
 * brcm_pm_s3_tmpbuf - temporary buffer
 * hash_pointer - pointer to location in memory where hash value is located
 * after encoding
 */
static char __section(.s3_enc_tmpbuf) brcm_pm_s3_tmpbuf[PAGE_SIZE*16];
static u32 *hash_pointer;

#define MEM_ENCRYPTED_START_ADDR	(0x80000000)
#define MEM_ENCRYPTED_LEN		(1*1024*1024)
#define	S3_HASH_LEN			16

#define MAX_TRANSFER_SIZE_MB		(16)
#define MAX_TRANSFER_SIZE		(MAX_TRANSFER_SIZE_MB*1024*1024)

#define AES_CBC_ENCRYPTION_KEY_SLOT	(5)
#define AES_CBC_DECRYPTION_KEY_SLOT	(6)

/*
 * brcm_pm_dram_encoder_set_area
 * Generates a list of descriptors needed to encrypt area
 * VA _begin.._begin+len-1 into PA outbuf..outbuf+out_len-1
 * On return out_len is the length of the last transfer, this
 * can be used as an offset into the outbuf to extract hash
 */
static struct brcm_mem_transfer *brcm_pm_dram_encoder_set_area(void *_begin,
	u32 len, dma_addr_t outbuf, u32 *out_len,
	struct brcm_mem_transfer *next)
{
	int num_descr = (len + *out_len - 1) / *out_len;
	struct brcm_mem_transfer *xfer, *x, *last_x;
	void *_end = _begin + len;

	BUG_ON(*out_len > MAX_TRANSFER_SIZE);

	xfer = kzalloc(num_descr * sizeof(struct brcm_mem_transfer),
		GFP_ATOMIC);

	if (!xfer) {
		printk(KERN_ERR "%s: out of memory\n", __func__);
		return NULL;
	}

	last_x = x = xfer;
	while (_begin < _end) {
		dma_addr_t pa_src = virt_to_phys(_begin);
		if (_begin + *out_len > _end)
			*out_len = _end - _begin;
		/* workaround - M2M DMA does not like 0 address */
		x->len		= *out_len;
		if (!pa_src) {
			pa_src += 0x10;
			x->len -= 0x10;
		}
		x->pa_src	= pa_src;
		x->pa_dst	= outbuf;
		x->mode		= BRCM_MEM_DMA_SCRAM_BLOCK;
		x->key		= AES_CBC_ENCRYPTION_KEY_SLOT;
		x->next		= x + 1;
		last_x = x;
		x++;
		_begin += *out_len;
	}
	last_x->next = next;

	return xfer;
}

static void brcm_pm_dram_encoder_free_area(struct brcm_mem_transfer *xfer)
{
	kfree(xfer);
}

/* Memory region over which M2M DMA calculates MAC cannot be modified
   after a call to brcm_pm_dram_encoder_start() */
void brcm_pm_dram_encode(void)
{
	u32 *hp = hash_pointer;
	u32 outbuflen = sizeof(brcm_pm_s3_tmpbuf);
	uint32_t tmp0, tmp1;
	/* clear temporary buffer */
	memset(brcm_pm_s3_tmpbuf, 0, sizeof(brcm_pm_s3_tmpbuf));
	_dma_cache_wback_inv(0, ~0);
	/* start M2M encoder */
	brcm_pm_dram_encoder_start();
	/* save hash in AON register */
	AON_SAVE_HASH(0, *hp++);
	AON_SAVE_HASH(1, *hp++);
	AON_SAVE_HASH(2, *hp++);
	AON_SAVE_HASH(3, *hp++);

	/* clear temporary buffer
	 * cannot use library function because it will use memory (stack) */
	__asm__ __volatile__(
	"	.set	noreorder\n"
	"	la	%0, brcm_pm_s3_tmpbuf\n"
	"	move	%1, $0\n"
	"1:	sw	$0, 0(%0)\n"
	"	addi	%0, 4\n"
	"	addi	%1, 4\n"
	"	bne	%2, %1, 1b\n"
	"	nop\n"
	: "=&r" (tmp0), "=&r" (tmp1)
	: "r" (outbuflen));
}
#else
#define brcm_pm_dram_encode	NULL
#endif

int __ref brcm_pm_s3_standby(int dcache_linesz, unsigned long options)
{
	int retval = 0;
	unsigned long flags;
#if CALCULATE_MEM_HASH
	int r1_len, r2_len, total_len;
	struct brcm_mem_transfer *xfer[3] = {0};
	u32 outlen = sizeof(brcm_pm_s3_tmpbuf);
	u32 outbuflen = outlen, tmp, ii;
	u32 region_size = AUTHENTICATION_REGION_SIZE;
	void *src;
	u32 hash[4];

	/*
	 * We are using bi-directional mapping to avoid synchronizing cache
	 * after encoding
	 */
	dma_addr_t pa_dst = dma_map_single(NULL, brcm_pm_s3_tmpbuf,
		outbuflen, DMA_BIDIRECTIONAL);

	if (dma_mapping_error(NULL, pa_dst)) {
		printk(KERN_ERR "%s: cannot remap temp buffer\n", __func__);
		return -1;
	}

	if (region_size < brcm_min_auth_region_size)
		region_size = brcm_min_auth_region_size;

	printk(KERN_DEBUG "Authentication region size: 0x%08x\n", region_size);
	printk(KERN_DEBUG "Output buffer  : 0x%08x-0x%08x (0x%08x)\n",
	       (u32)brcm_pm_s3_tmpbuf,
	       (u32)brcm_pm_s3_tmpbuf + outlen, outlen);

	/* We add areas to the head of the list - in reverse order
	 * of M2M DMA execution */

	/* This region starts from _text and bypasses temporary
	 * output buffer */
	src = _text;
	/* check if in/out regions overlap */
	if (brcm_pm_s3_tmpbuf >= (char *)(src + region_size) ||
	    brcm_pm_s3_tmpbuf + outlen <= (char *)src) {
		/* non-overlapping case */
		r1_len = region_size;
		r2_len = 0;
	} else {
		/* need to cut out the output buffer */
		r1_len = (int)(brcm_pm_s3_tmpbuf - (char *)src);
		r2_len = region_size - r1_len;
	}
	total_len = r1_len + r2_len;

	xfer[0] = brcm_pm_dram_encoder_set_area((void *)src, r1_len,
		pa_dst, &outlen, NULL);
	if (!xfer[0]) {
		printk(KERN_ERR "%s: cannot setup M2M DMA [0]\n", __func__);
		goto _failed;
	}

	printk(KERN_DEBUG "Region 0       : 0x%08x-0x%08x (0x%08x)\n",
		(u32)src, (u32)src + r1_len, r1_len);

	if (r2_len > 0) {
		outlen = outbuflen;
		src = (void *)((u32)(brcm_pm_s3_tmpbuf + outbuflen));
		xfer[1] = xfer[0];
		xfer[0] = brcm_pm_dram_encoder_set_area(src, r2_len,
			pa_dst, &outlen, xfer[1]);
		if (!xfer[0]) {
			printk(KERN_ERR "%s: cannot setup M2M DMA [1]\n",
			       __func__);
			goto _failed;
		}
		printk(KERN_DEBUG "Region 1       : 0x%08x-0x%08x (0x%08x)\n",
			(u32)src, (u32)(src + r2_len), r2_len);
	}

	/* First descriptor must include kernel reentry point
	 * We are processing one output buffer worth of data */
	src = (void *)((u32)(&s3_reentry) & ~(PAGE_SIZE-1));
	tmp = outbuflen;
	xfer[2] = brcm_pm_dram_encoder_set_area((void *)src, outbuflen,
		pa_dst, &tmp, xfer[0]);
	if (!xfer[2]) {
		printk(KERN_ERR "%s: cannot setup M2M DMA [2]\n", __func__);
		goto _failed;
	}
	printk(KERN_DEBUG "Region 2       : 0x%08x-0x%08x (0x%08x)\n",
		(u32)src, (u32)src + outbuflen, outbuflen);
	total_len += outbuflen;
	if (brcm_pm_dram_encoder_prepare(xfer[2])) {
		printk(KERN_ERR "%s: cannot initialize M2M DMA\n", __func__);
		goto _failed;
	}

	hash_pointer = (u32 *)(brcm_pm_s3_tmpbuf +
		(outlen - S3_HASH_LEN + outbuflen) % outbuflen);

	/* M2M DMA descriptor address */
	if (brcm_pm_hash_enabled)
		AON_SAVE_CPB(2, BDEV_RD(BCHP_MEM_DMA_0_FIRST_DESC));
	else
		AON_SAVE_CPB(2, 0);
	/* Encryption key slot */
	AON_SAVE_CPB(3, AES_CBC_ENCRYPTION_KEY_SLOT);
	/* Address of hash */
	AON_SAVE_CPB(4, (u32)hash_pointer);
	/* Total authentication region size */
	AON_SAVE_CPB(9, total_len);
	printk(KERN_DEBUG "Encryption slot: %d\n",
	       AES_CBC_ENCRYPTION_KEY_SLOT);
	printk(KERN_DEBUG "Hash pointer   : 0x%p\n", hash_pointer);
	printk(KERN_DEBUG "Total length   : 0x%08x\n", (u32)total_len);
	printk(KERN_DEBUG "Reentry point  : 0x%p\n", &s3_reentry);
#else
	AON_SAVE_CPB(2, 0);
#endif

	/* Warm Boot Magic Word */
	AON_SAVE_CPB(0, WARM_BOOT_MAGIC_WORD);
	/* Kernel Reentry Address */
	AON_SAVE_CPB(1, (u32)&s3_reentry);

	/* clear RESET_HISTORY */
	BDEV_WR_F_RB(AON_CTRL_RESET_CTRL, clear_reset_history, 1);

	local_irq_save(flags);
	/* save CP0 context */
	brcm_pm_init_cp0_context(&s3_context);
	brcm_pm_save_cp0_context(&s3_context);

	/* inhibit DDR_RSTb pulse */
	BDEV_UNSET(BCHP_DDR40_PHY_CONTROL_REGS_0_STANDBY_CONTROL, 0x0f);
	BDEV_SET(BCHP_DDR40_PHY_CONTROL_REGS_0_STANDBY_CONTROL, BIT(5) | 0x05);

	/* save RTS */
	brcm_pm_save_restore_rts(BCHP_MEMC_ARB_0_REG_START,
		s3_context.memc0_rts, 0);

	/* save I/O context */

	local_flush_tlb_all();
	_dma_cache_wback_inv(0, ~0);

	retval = brcm_pm_s3_standby_asm(options, brcm_pm_dram_encode,
		dcache_linesz);

	/* CPU reconfiguration */
	bchip_mips_setup();
	brcm_setup_ebase();

	/* restore RTS */
	brcm_pm_save_restore_rts(BCHP_MEMC_ARB_0_REG_START,
		s3_context.memc0_rts, 1);

	/* restore I/O context */
	board_pinmux_setup();
	ebi_restore_settings();

#if defined(CONFIG_BRCM_SDIO)
	bchip_sdio_init(0, BCHP_SDIO_0_CFG_REG_START);

#if defined(BCHP_SDIO_1_CFG_REG_START)
	bchip_sdio_init(1, BCHP_SDIO_1_CFG_REG_START);
#endif /* defined(BCHP_SDIO_1_CFG_REG_START) */

#endif /* defined(CONFIG_BRCM_SDIO) */

	/* restore CP0 context */
	brcm_pm_restore_cp0_context(&s3_context);

#if defined(BCHP_IRQ0_UART_IRQEN_uarta_MASK)
	/* 3548 style - separate register */
	BDEV_WR(BCHP_IRQ0_UART_IRQEN, BCHP_IRQ0_UART_IRQEN_uarta_MASK |
		BCHP_IRQ0_UART_IRQEN_uartb_MASK |
		BCHP_IRQ0_UART_IRQEN_uartc_MASK);
	BDEV_WR(BCHP_IRQ0_IRQEN, 0);
#elif defined(BCHP_IRQ0_IRQEN_uarta_irqen_MASK)
	/* 7405 style - shared with L2 */
	BDEV_WR(BCHP_IRQ0_IRQEN, BCHP_IRQ0_IRQEN_uarta_irqen_MASK
		| BCHP_IRQ0_IRQEN_uartb_irqen_MASK
#if defined(BCHP_IRQ0_IRQEN_uartc_irqen_MASK)
		| BCHP_IRQ0_IRQEN_uartc_irqen_MASK
#endif
		);
#endif

	local_irq_restore(flags);

#if defined(CONFIG_BRCM_HAS_PCIE) && defined(CONFIG_PCI)
	BDEV_WR_F_RB(WKTMR_EVENT, wktmr_alarm_event, 1);
	BDEV_WR_F_RB(WKTMR_PRESCALER, wktmr_prescaler, WKTMR_FREQ);

	if (brcm_pcie_enabled) {
		brcm_early_pcie_setup();
		brcm_setup_pcie_bridge();
	}
#endif

#if CALCULATE_MEM_HASH
	for (ii = 0; ii < 4; ii++)
		hash[ii] = AON_READ_HASH(ii);
	printk(KERN_DEBUG "hash: %08x:%08x:%08x:%08x\n",
	       hash[0], hash[1], hash[2], hash[3]);

	brcm_pm_dram_encoder_complete(xfer[0]);
_failed:
	for (ii = 0; ii < 3; ii++)
		brcm_pm_dram_encoder_free_area(xfer[ii]);
	dma_unmap_single(NULL, pa_dst, outbuflen, DMA_BIDIRECTIONAL);

#endif
	return retval;
}

static void __maybe_unused __raw_uart_putc(char c)
{
	while (!(BDEV_RD(BCHP_UARTA_LSR) & BCHP_UARTA_LSR_THRE_MASK))
		;
	BDEV_WR_RB(BCHP_UARTA_THR, (u32)c);
}
