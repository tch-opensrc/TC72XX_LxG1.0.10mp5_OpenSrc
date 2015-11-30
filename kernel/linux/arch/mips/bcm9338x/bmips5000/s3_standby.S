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

#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/mipsregs.h>
#include <asm/cacheops.h>
#include <asm/addrspace.h>
#include <asm/brcmstb/brcmstb.h>

#define REG_OFFSET		(KSEG1 | BCHP_PHYSICAL_OFFSET)
#define REG(x)			(REG_OFFSET + (x))

/*
 * Bootloader parameters for warm boot:
 *	byte offset	value
 *	0-3:		magic word
 *	4-7:		reentry address
 *	8-11:		m2m descriptors PA
 *	12-15:		encryption slot
 *	16-19:		address of hash
 *	20-35:		hash
 */
	.text
	.set		noreorder
	.align		5
	.global		s3_reentry

#if defined(CONFIG_BMIPS5000)
#define CACHE_WRITEBACK_CMD	Hit_Writeback_Inv_SD
#else
#define CACHE_WRITEBACK_CMD	Hit_Writeback_Inv_D
#endif

/*
 * We do not need stack frame, the entire context gets saved
 * in a dedicated structure brcm_pm_s3_context
 * Upon warm reboot the context gets restored from this structure
 * Parameters:
 *	a0: options
 *	a1: dram encoder start function or NULL if not installed
 *	a2: d-cache line size
 */
	LEAF(brcm_pm_s3_standby_asm)

	la	t0, s3_context
	/* general purpose registers */
	sw	ra, 0(t0)
	sw	s0, 4(t0)
	sw	s1, 8(t0)
	sw	s2, 12(t0)
	sw	s3, 16(t0)
	sw	s4, 20(t0)
	sw	s5, 24(t0)
	sw	s6, 28(t0)
	sw	s7, 32(t0)
	sw	gp, 36(t0)
	sw	sp, 40(t0)
	sw	fp, 44(t0)

	mfc0	t1, CP0_STATUS
# save cp0 sr
	sw	t1, 48(t0)

# Write-back gp registers - cache will be gone after wakeup
# align context area address to the cache line
	addiu	t1, a2, -1
	not	t1
	and	t0, t1

# flush at least 64 bytes
	addiu	t2, t0, 64
	and	t2, t1

1:	cache	CACHE_WRITEBACK_CMD, 0(t0)
	bne	t0, t2, 1b
	addu	t0, a2

	beqz	a1, skip_dram_encode
	nop

	/*
	 * Call DRAM encoder start method if it was installed
	 * Nobody can modify DRAM content by now.
	 */
	jal	a1
	nop

/* NOTE: Uncommenting this block will force hash mismatch
 * if .data segment is in the authentication region.
 * Use for CFE secure warm boot testing.
 */
#	la	t0, secure_s3_dummy_data
#	lw	t1, 0(t0)
#	addi	t1, 1
#	sw	t1, 0(t0)
#	cache	CACHE_WRITEBACK_CMD, 0(t0)
#	nop

skip_dram_encode:

	li	t0, REG(BCHP_AON_CTRL_PM_MIPS_WAIT_COUNT)
	li	t1, 0xffff
	sw	t1, 0(t0)
	lw	zero, 0(t0)

# deep power down request
pd_request:
	li	t0, REG(BCHP_AON_CTRL_PM_CTRL)
# SET: pm_deep_standby, pm_warm_boot, pm_enable_pll_pwrdn
	li	t1, 0x32
	sw	zero, 0(t0)
	lw	zero, 0(t0)
	sw	t1, 0(t0)
	lw	zero, 0(t0)

# SET: pm_start_pwrdn
	li	t1, 0x33
	sw	t1, 0(t0)
	lw	zero, 0(t0)

/***********************************************************************
 * Wait for interrupt
 ***********************************************************************/
wait_loop:
# enable int2 and then wait for an interrupt
	mfc0	t0, CP0_STATUS

	li	t1, ~(ST0_IM | ST0_IE)
	and	t0, t1
	ori	t0, STATUSF_IP2
	mtc0	t0, CP0_STATUS
	nop
	nop
	nop
	ori	t0, ST0_IE
	mtc0	t0, CP0_STATUS
	wait
	nop

s3_reentry:

#ifdef CONFIG_BMIPS5000
# clear CRS and JTB
	li	t0, (0x06 << 16)
	mtc0	t0, $22, 2
	ssnop
	ssnop
	ssnop

	li	t0, (0x04 << 16)
	mtc0	t0, $22, 2
	ssnop
	ssnop
	ssnop
#endif

#ifdef CONFIG_BMIPS4380
# flush 4-entry call/return stack
	jal	1f
	nop
1:	jal	1f
	nop
1:	jal	1f
	nop
1:	jal	1f
	nop
1:
#endif

# restore sr
	sync
	nop

s3_standby_exit:
#ifdef CONFIG_BRCM_UPPER_768MB
#if defined(CONFIG_BMIPS4380)
	mfc0	t0, $22, 3
	li	t1, 0x1ff0
	li	t2, (1 << 12) | (1 << 9)
	or	t0, t1
	xor	t0, t1
	or	t0, t2
	mtc0	t0, $22, 3
#elif defined(CONFIG_BMIPS5000)
	mfc0	t0, $22, 5
	li	t1, 0x01ff
	li	t2, (1 << 8) | (1 << 5)
	or	t0, t1
	xor	t0, t1
	or	t0, t2
	mtc0	t0, $22, 5
#endif
#endif
	sync
	nop

#ifdef CONFIG_BRCM_UPPER_MEMORY
	li	sp, BRCM_WARM_RESTART_VEC
	la	k0, brcm_upper_tlb_setup
	jalr	k0
	nop
#endif

# return to caller
	/* general purpose registers */
	la	t0, s3_context
	lw	ra, 0(t0)
	lw	s0, 4(t0)
	lw	s1, 8(t0)
	lw	s2, 12(t0)
	lw	s3, 16(t0)
	lw	s4, 20(t0)
	lw	s5, 24(t0)
	lw	s6, 28(t0)
	lw	s7, 32(t0)
	lw	gp, 36(t0)
	lw	sp, 40(t0)
	lw	fp, 44(t0)

# restore cp0 sr
	lw	t1, 48(t0)
	mtc0	t1, CP0_STATUS

	li	v0, 1
	jr	ra
	nop


	END(brcm_pm_s3_standby_asm)

	.align 4
	.data
secure_s3_dummy_data:	.word 0

#define UART_LSR_OFFSET		0x14
#define UART_LSR_MASK		BCHP_UARTA_LSR_THRE_MASK
#define UART_TX_OFFSET		0x00

__dputc:

	li	t1, REG(BCHP_UARTA_REG_START)
1:
	lw	t2, UART_LSR_OFFSET(t1)
	andi	t2, UART_LSR_MASK
	beqz	t2, 1b
	nop

	sw	a0, UART_TX_OFFSET(t1)

1:	lw	t2, UART_LSR_OFFSET(t1)
	andi	t2, UART_LSR_MASK
	beqz	t2, 1b
	nop
	jr	ra
	nop

s3_standby_section_end:
	nop
