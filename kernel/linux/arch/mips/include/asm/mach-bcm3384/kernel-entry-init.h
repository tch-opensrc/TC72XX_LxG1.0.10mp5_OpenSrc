/***************************************************************************
 *     Copyright (c) 2008, Broadcom Corporation
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
 *
 ***************************************************************************/

#ifndef __ASM_MACH_BCM3384_KERNEL_ENTRY_H
#define __ASM_MACH_BCM3384_KERNEL_ENTRY_H

#define KERNEL_ENTRY_BASE	KSEG1ADDR(CONFIG_MIPS_BRCM_TEXT & 0xfffc0000)

	.macro kernel_entry_setup
	/* create exception vectors */
1:
	li  	t1, 0x1000ffff  /* loop forever instruction */
	la  	t9, KERNEL_ENTRY_BASE
	sw	t1, 0x000(t9)
	sw	$0, 0x004(t9)
	sw	t1, 0x080(t9)
	sw	t1, 0x100(t9)
	sw	t1, 0x180(t9)
	sw	t1, 0x200(t9)
	sw	t1, 0x280(t9)
	sw	t1, 0x300(t9)
	sw	t1, 0x380(t9)
	sw	t1, 0x400(t9)
	
#if defined(CONFIG_BMIPS4350)
	li	t1, 0xff40000c
	mtc0	t1, $22, 6
	li	t1, (KERNEL_ENTRY_BASE | (1 << 18))
	li	t2, 0xff438000
	sw	t1, 0(t2)
#endif
	/* clear the bev: we have valid exception vectors */
	mfc0	t1, $12
	li	t2, ~(1 << 22)
	and	t1, t1, t2
	mtc0	t1, $12
#if defined(CONFIG_BMIPS5000)
#if defined(CONFIG_EARLIER_THAN_EARLY_PRINTK) && 0
	/* this is dangerous - check the pins on your board! */
	/* Setup the UART first */
	/* 3384 pin mux */
	li	t9, 0xb4e002a4		/* TP_BLOCK_DATA_MSB */
	li	t1, (1 << 12) | 22	/* pinmux 1 uart1 rts 22 */
	sw	$0, 0(t9)
	sw	t1, 4(t9)
	li	t2, 0x21		/* load mux register */
	sw	t2, 8(t9)		/* TP_COMMAND */
	addi	t1, t1, 1		/* pinmux 1 uart1 cts 23 */
	sw	t1, 4(t9)
	sw	t2, 8(t9)		/* TP_COMMAND */
	addi	t1, t1, 1		/* pinmux 1 uart1 cd  24 */
	sw	t1, 4(t9)
	sw	t2, 8(t9)		/* TP_COMMAND */
	addi	t1, t1, 1		/* pinmux 1 uart1 ri  25 */
	sw	t1, 4(t9)
	sw	t2, 8(t9)		/* TP_COMMAND */
	addi	t1, t1, 1		/* pinmux 1 uart1 dsr 26 */
	sw	t1, 4(t9)
	sw	t2, 8(t9)		/* TP_COMMAND */
	addi	t1, t1, 1		/* pinmux 1 uart1 dtr 27 */
	sw	t1, 4(t9)
	sw	t2, 8(t9)		/* TP_COMMAND */
	addi	t1, t1, 1		/* pinmux 1 uart1 sin 28 */
	sw	t1, 4(t9)
	sw	t2, 8(t9)		/* TP_COMMAND */
	addi	t1, t1, 1		/* pinmux 1 uart1 sout 29 */
	sw	t1, 4(t9)
	sw	t2, 8(t9)		/* TP_COMMAND */
	/* set up the uart port (1) 8 */
	li      t9, 0xb4e00520	/* uart1 */
	li      t1, 0xe53705
	sw      t1, 0x00(t9)
	li      t1, 0xe
	sw      t1, 0x04(t9)
	li      t1, 0x0000
	sw      t1, 0x08(t9)
		 
	li	t1, '\r'
	sw	t1, 0x14(t9)
	li	t1, '\n'
	sw	t1, 0x14(t9)
	li	t1, '\r'
	sw	t1, 0x14(t9)
	li	t1, 'A'
	sw	t1, 0x14(t9)
#endif
	 
	jal	bmips_5xxx_init
	nop
	jal	brcmstb_enable_xks01
	nop
#endif	/* BMIPS5000 */
#if defined(CONFIG_BMIPS4350)
	/* Setup the UART first */
	/* 3384 pin mux */
	li	t9, 0xb4e002a4		/* TP_BLOCK_DATA_MSB */
	li	t1, (1 << 12) | 8	/* pinmux 1 uart2 sin 8 */
	sw	$0, 0(t9)
	sw	t1, 4(t9)
	li	t2, 0x21		/* load mux register */
	sw	t2, 8(t9)		/* TP_COMMAND */
	addi	t1, t1, 1		/* pinmux 1 uart2 sout 9 */
	sw	t1, 4(t9)
	sw	t2, 8(t9)		/* TP_COMMAND */
	/* set up the uart port (1) 8 */
	li      t9, 0xb4e00540	/* uart2 */
	li      t1, 0xe53705
	sw      t1, 0x00(t9)
	/* baudword */
	li	t2, 0xb4e00000	/* chip id */
	lw	t3, 0(t2)
	andi	t3, t3, 0xf
	li	t2, 8
	sub	t3, t3, t2
	li      t1, 0xe		/* B0 and up */
	bgez	t3, 1f
	nop
	li      t1, 0x13	/* A1 */
1:	
	sw      t1, 0x04(t9)
	li      t1, 0x0000
	sw      t1, 0x08(t9)
		 
	li	t1, '\r'
	sw	t1, 0x14(t9)
	li	t1, '\n'
	sw	t1, 0x14(t9)
	li	t1, 'X'
	sw	t1, 0x14(t9)
#endif	/* BMIPS4350 */
	.endm

	.macro  smp_slave_setup
	.endm

#endif /* __ASM_MACH_BCM3384_KERNEL_ENTRY_H */
