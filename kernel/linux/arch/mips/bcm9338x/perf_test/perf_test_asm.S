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

	.text
	.set		noreorder
	.set		nomacro
	.align		5
	.global		instruction_perf_test

/***********************************************************************
 * extern u32 instruction_perf_test(u32 icnt);
 ***********************************************************************/
instruction_perf_test:
	srl	a0, a0, 5
	mtc0	zero, CP0_COUNT
1:	
	lui	t1, 0xdead
	li	t0, 0xbeef
	or	t0, t0, t1
	lui	t1, 0xfeed
	li	t2, 0xbeef
	or	t1, t1, t2
	xor	t3, t0, t1
	xor 	t4, t3, t2
	li	t3, 0xff
	nor	t3, t4, t3
	add	t3, t3, t0
	addi	t3, t3, 0xffff
	and	t3, t3, t1
	andi	t3, t3, 0xffff
	clo	t4, t3
	clz	t5, t3
	subu	t5, t0, t1
	sra	t5, t5, 5
	sll	t5, t5, 5
	nor	t7, t6, t6
	add	t5, t0, t1
	sll	t6, t0, 5
	sra	t6, t6, 3
	srl	t6, t6, 2
	subu	t7, t0, t1
	add	t6, t0, t1
	xor	t7, t6, t7
	add	t5, t1, t7
	li	t5, 1
	sub	a0, a0, t5
	bnez	a0, 1b
	nop
	mfc0	v0, CP0_COUNT
	j	ra
	nop

/***********************************************************************
 * extern u32 mem_read_test(u32 mem_base, u32 mem_size);
 ***********************************************************************/

	.global		mem_read_test
mem_read_test:
	ori	v0, zero, 32
	mtc0	zero, CP0_COUNT
1:	
	lw	t0, 0x00(a0)
	lw	t1, 0x04(a0)
	lw	t2, 0x08(a0)
	lw	t3, 0x0c(a0)
	lw	t4, 0x10(a0)
	lw	t5, 0x14(a0)
	lw	t6, 0x18(a0)
	lw	t7, 0x1c(a0)
	subu	a1, a1, v0
	addu	a0, a0, v0
	bgtz	a1, 1b
	nop
	mfc0	v0, CP0_COUNT
	j	ra
	nop
/***********************************************************************
 * extern u32 mem_write(u32 mem_base, u32 mem_size);
 ***********************************************************************/

	.set		noreorder
	.global		mem_write_test
mem_write_test:
	ori	v0, zero, 32
	lui	t0, 0xdead
	li 	t1, 0xbeef
	or	t0, t0, t1
	mtc0	zero, CP0_COUNT
1:	
	sw	t0, 0x00(a0)
	sw	t0, 0x04(a0)
	sw	t0, 0x08(a0)
	sw	t0, 0x0c(a0)
	sw	t0, 0x10(a0)
	sw	t0, 0x14(a0)
	sw	t0, 0x18(a0)
	sw	t0, 0x1c(a0)
	subu	a1, a1, v0
	addu	a0, a0, v0
	bgtz	a1, 1b
	nop
	mfc0	v0, CP0_COUNT
	j	ra
	nop
/***********************************************************************
 * extern u32 mem_rmw_test(u32 mem_base, u32 mem_size);
 ***********************************************************************/

	.global		mem_rmw_test
mem_rmw_test:
	ori	v0, zero, 32
	lui	t0, 0xdead
	li 	t1, 0xbeef
	or	v1, t0, t1
	mtc0	zero, CP0_COUNT
1:	
	lw	t0, 0x00(a0)
	lw	t1, 0x04(a0)
	lw	t2, 0x08(a0)
	lw	t3, 0x0c(a0)
	lw	t4, 0x10(a0)
	lw	t5, 0x14(a0)
	lw	t6, 0x18(a0)
	lw	t7, 0x1c(a0)
	xor	t0, t0, v1
	xor	t1, t1, v1
	xor	t2, t2, v1
	xor	t3, t3, v1
	xor	t4, t4, v1
	xor	t5, t5, v1
	xor	t6, t6, v1
	xor	t7, t7, v1
	sw	t0, 0x00(a0)
	sw	t1, 0x04(a0)
	sw	t2, 0x08(a0)
	sw	t3, 0x0c(a0)
	sw	t4, 0x10(a0)
	sw	t5, 0x14(a0)
	sw	t6, 0x18(a0)
	sw	t7, 0x1c(a0)
	subu	a1, a1, v0
	addu	a0, a0, v0
	bgtz	a1, 1b
	nop
	mfc0	v0, CP0_COUNT
	j	ra
	nop
