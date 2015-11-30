/*
 * Copyright (C) 2010 Broadcom Corporation
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

#include <asm/asm.h>
#include <asm/asmmacro.h>
#include <asm/cacheops.h>
#include <asm/regdef.h>
#include <asm/mipsregs.h>
#include <asm/stackframe.h>
#include <asm/addrspace.h>
#include <asm/brcmstb/brcmstb.h>

	.macro	CLR_FPR a b c d
	mtc1	$0, $\a
	mtc1	$0, $\b
	mtc1	$0, $\c
	mtc1	$0, $\d
	.endm

	.macro	BARRIER
	.set	mips32
	ssnop
	ssnop
	ssnop
	.set	mips0
	.endm

/***********************************************************************
 * NMI vector
 * This entire function gets copied to 8000_0000.
 ***********************************************************************/

	__CPUINIT

NESTED(brcm_reset_nmi_vec, PT_SIZE, sp)
	.set	push
	.set	noat
	.align	4

#ifdef CONFIG_SMP
	/* if the NMI bit is clear, assume this is a TP1 reset instead */
	li	k1, (1 << 19)
	mfc0	k0, CP0_STATUS
	and	k0, k1
	beqz	k0, brcmstb_tp1_entry
#endif /* CONFIG_SMP */

	SAVE_ALL
 	move	a0, sp

	/* clear EXL, ERL, BEV so that TLB refills still work */
	mfc0	k0, CP0_STATUS
	li	k1, ST0_ERL | ST0_EXL | ST0_BEV | ST0_IE
	or	k0, k1
	xor	k0, k1
	mtc0	k0, CP0_STATUS
	BARRIER

	/* call the NMI handler function (probably doesn't return) */
	la	k0, brcm_nmi_handler
	lw	k1, 0(k0)
	jalr	k1

	RESTORE_ALL
	.set	mips3
	eret

/***********************************************************************
 * TP1 reset vector (used for the initial boot only)
 * This is still part of brcm_reset_nmi_vec().
 ***********************************************************************/

#ifdef CONFIG_SMP

brcmstb_tp1_entry:

	/* set up CP0 STATUS; enable FPU */
	li	k0, 0x30000000
	mtc0	k0, CP0_STATUS
	BARRIER

	/* set local CP0 CONFIG to make kseg0 cacheable, write-back */
	mfc0	k0, CP0_CONFIG
	ori	k0, 0x07
	xori	k0, 0x04
	mtc0	k0, CP0_CONFIG	

	/* initialize FPU registers */
	CLR_FPR	f0 f1 f2 f3
	CLR_FPR	f4 f5 f6 f7
	CLR_FPR	f8 f9 f10 f11
	CLR_FPR	f12 f13 f14 f15
	CLR_FPR	f16 f17 f18 f19
	CLR_FPR	f20 f21 f22 f23
	CLR_FPR	f24 f25 f26 f27
	CLR_FPR	f28 f29 f30 f31

#if defined(CONFIG_BMIPS4380)
	/* initialize TP1's local I-cache */
	li	k0, 0x80000000
	li	k1, 0x80008000
	mtc0	zero, $28
	mtc0	zero, $28, 1
	BARRIER

1:	cache	Index_Store_Tag_I, 0(k0)
	addiu	k0, 64
	bne	k0, k1, 1b
#elif defined(CONFIG_BMIPS5000)
	/* set exception vector base */
	la	k0, ebase
	lw	k0, 0(k0)
	mtc0	k0, $15, 1
	BARRIER
#endif
	/* jump back to kseg0 in case we need to remap the kseg1 area */
	la	k0, 1f
	jr	k0
1:
#if defined(CONFIG_BRCM_UPPER_768MB)
	la	k0, brcmstb_enable_xks01
	jalr	k0
#endif
	/* use temporary stack to set up upper memory TLB */
	li	sp, BRCM_WARM_RESTART_VEC
	la	k0, brcm_upper_tlb_setup
	jalr	k0

	/* switch to permanent stack and continue booting */

	.global	brcmstb_tp1_reentry
brcmstb_tp1_reentry:
	la	k0, brcmstb_smp_boot_sp
	lw	sp, 0(k0)
	la	k0, brcmstb_smp_boot_gp
	lw	gp, 0(k0)
	la	k0, start_secondary
	jr	k0

#endif /* CONFIG_SMP */

	.align	4
	.global	brcm_reset_nmi_vec_end
brcm_reset_nmi_vec_end:

	END(brcm_reset_nmi_vec)

	.set	pop
	.previous

/***********************************************************************
 * TP1 warm restart vector (used for second and subsequent boots).
 * Also used for passive standby recovery (PM).
 * This entire function gets copied to (BRCM_WARM_RESTART_VEC)
 ***********************************************************************/

LEAF(brcm_tp1_int_vec)

	.align	4
	mfc0	k0, CP0_STATUS
	ori	k0, 0x01
	xori	k0, 0x01
	mtc0	k0, CP0_STATUS
	eret

	.align	4
	.global	brcm_tp1_int_vec_end
brcm_tp1_int_vec_end:

	END(brcm_tp1_int_vec)

/***********************************************************************
 * XKS01 support
 ***********************************************************************/

LEAF(brcmstb_enable_xks01)

#if defined(CONFIG_BRCM_UPPER_768MB)

	/*
	 * Enable XKS01 1024MB kseg0
	 *
	 * Memory map is documented in <asm/mach-brcmstb/spaces.h>
	 */

#if defined(CONFIG_BMIPS4380)
	mfc0	t0, $22, 3
	li	t1, 0x1ff0
	li	t2, (1 << 12) | (1 << 9)
	or	t0, t1
	xor	t0, t1
	or	t0, t2
	mtc0	t0, $22, 3
	BARRIER
#elif defined(CONFIG_BMIPS5000)
	mfc0	t0, $22, 5
	li	t1, 0x01ff
	li	t2, (1 << 8) | (1 << 5)
	or	t0, t1
	xor	t0, t1
	or	t0, t2
	mtc0	t0, $22, 5
	BARRIER
#else

#error Missing XKS01 setup

#endif

#endif /* defined(CONFIG_BRCM_UPPER_768MB) */

	jr	ra

END(brcmstb_enable_xks01)
