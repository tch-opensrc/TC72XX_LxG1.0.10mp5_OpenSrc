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

#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/mipsregs.h>
#include <asm/cacheops.h>
#include <asm/addrspace.h>
#include <asm/brcmstb/brcmstb.h>

#define REG_OFFSET		(KSEG1 | BCHP_PHYSICAL_OFFSET)
#define REG(x)			(REG_OFFSET + (x))
#define UPG_USEC_TICKS		(UPGTMR_FREQ / 1000000)

/* debug output - BRCM_STANDBY_VERBOSE */
#define UART_BASE		BCHP_UARTA_REG_START
#define UART_LSR_OFFSET		0x14
#define UART_LSR_MASK		BCHP_UARTA_LSR_THRE_MASK
#define UART_TX_OFFSET		0x00

#define USE_WATCHDOG		0

#if defined(BCHP_AON_CTRL_PM_CTRL_pm_clk_divider_reset_en_MASK) || \
 defined(BCHP_SUN_TOP_CTRL_PM_CTRL_pm_clk_divider_reset_en_MASK)
#define PM_CMD_BASE		0x0b
#else
#define PM_CMD_BASE		0x03
#endif

#if defined(CONFIG_BMIPS5000)
#define PM_USE_MIPS_READY	0x04
#else
#define PM_USE_MIPS_READY	0x00
#endif

#define PM_STANDBY_COMMAND	(PM_CMD_BASE|PM_USE_MIPS_READY)

#ifdef BCHP_MEMC_DDR23_APHY_WL0_0_BYTE0_VCDL_PHASE_CNTL
#define VCDL_SAVE_REG		BCHP_MEMC_DDR23_APHY_WL1_0_SPARE0_RW
#define RELOAD_VCDL		1
#else
#define RELOAD_VCDL		0
#endif

#if defined(CONFIG_BRCM_HAS_AON)
#define PM_WAIT_COUNT		BCHP_AON_CTRL_PM_MIPS_WAIT_COUNT
#define PM_CONTROL		BCHP_AON_CTRL_PM_CTRL
#define PM_STATUS		BCHP_AON_CTRL_PM_STATUS
#define PM_STATUS_bsp_ready	BCHP_AON_CTRL_PM_STATUS_pm_bsp_ready_for_pwrdn_MASK
#else
#define PM_WAIT_COUNT		BCHP_SUN_TOP_CTRL_PM_MIPS_WAIT_COUNT
#define PM_CONTROL		BCHP_SUN_TOP_CTRL_PM_CTRL
#define PM_STATUS		PM_CONTROL
#define PM_STATUS_bsp_ready	BCHP_SUN_TOP_CTRL_PM_CTRL_pm_bsp_ready_for_pwrdn_MASK
#endif

#define FLAGREG			s5

	.text
	.set	noreorder
	.align	5

# a0 is icache line size
# a1 is the interrupt vector (or general exception base)
# a2 is the interrupt vector size
# a3 is brcm_pm_standby_flags

	LEAF(brcm_pm_standby_asm)
	subu	sp, 64
	sw	ra, 0(sp)
	sw	s0, 4(sp)
	sw	s1, 8(sp)
	sw	s2, 12(sp)
	sw	s3, 16(sp)
	sw	s4, 20(sp)
	sw	s5, 24(sp)
	sw	s6, 28(sp)
	sw	s7, 32(sp)

	move	FLAGREG, a3

	bal	memc_settings_valid
	nop
	bnez	v0, standby_exit
	nop

#ifdef BCHP_MEMC_DDR23_APHY_WL0_0_BYTE0_VCDL_PHASE_CNTL
	/*
	 * Save VCDL values:
	 * WL0_0_BYTE0 -> bits 07:00
	 * WL0_0_BYTE1 -> bits 15:08
	 * WL1_0_BYTE0 -> bits 23:16
	 * WL1_0_BYTE1 -> bits 31:24
	 */
	li	t0, REG(BCHP_MEMC_DDR23_APHY_WL0_0_BYTE0_VCDL_PHASE_CNTL)
	lw	t1, 0(t0)
	andi	t2, t1, 0x1f
	lw	t1, 4(t0)
	andi	t1, t1, 0x1f
	sll	t1, 8
	or	t2, t1

	li	t0, REG(BCHP_MEMC_DDR23_APHY_WL1_0_BYTE0_VCDL_PHASE_CNTL)
	lw	t1, 0(t0)
	andi	t1, t1, 0x1f
	sll	t1, 16
	or	t2, t1
	lw	t1, 4(t0)
	andi	t1, t1, 0x1f
	sll	t1, 24
	or	t2, t1

	li	t0, REG(VCDL_SAVE_REG)
	sw	t2, 0(t0)
	lw	zero, 0(t0)
#endif

#ifdef CONFIG_BMIPS5000
	/*
	 * reset jump target buffer and call/return stack, to avoid
	 * unexpected accesses to DRAM
	 */
	li	t0, 0x00040000
	mtc0	t0, $22, 2
	mfc0	t0, $22, 2

	li	t0, 0x00060000
	mtc0	t0, $22, 2
	mfc0	t0, $22, 2

	ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop ;
	ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop ;
	ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop ;
	ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop ; ssnop
#endif

# lock the asm section into the I-cache
	move	s0, a0
	addiu	t1, s0, -1
	not	t1

	la	t0, brcm_pm_standby_asm
	and	t0, t1

	la	t2, passiveIRQ_end
	and	t2, t1

1:	cache	0x1c, 0(t0)
	bne	t0, t2, 1b
	addu	t0, s0

# now lock the interrupt vector
	move	t0, zero
2:
	cache	0x1c, 0(a1)
	addu	a1, s0
	addu	t0, s0
	ble	t0, a2, 2b
	nop

	bal	dputc
	li	a0, 's'

/***********************************************************************
 * Flush memory transactions and put MEMC into standby
 ***********************************************************************/
	sync

#ifdef CONFIG_BRCM_HAS_ANALOG_DDR_PHY
	li	t0, BRCM_STANDBY_DDR_PLL_ON
	and	t0, FLAGREG
	beqz	t0, 2f
	nop

	bal	set_pll_frequency
	li	a0, 0x3d43

	bal	set_pll_frequency
	li	a0, 0x2d43
2:
#endif

#if !defined(CONFIG_BRCM_PWR_HANDSHAKE)
	li	t0, REG(BCHP_MEMC_DDR_0_SSPD_CMD)
	li	t1, BCHP_MEMC_DDR_0_SSPD_CMD_SSPD_MASK
	sw	t1, 0(t0)

	li	t0, REG(BCHP_MEMC_DDR_0_POWER_DOWN_STATUS)
1:	lw	t1, 0(t0)
	andi	t1, BCHP_MEMC_DDR_0_POWER_DOWN_STATUS_SSPD_MASK
	beqz	t1, 1b
	nop

	bal	dputc
	li	a0, 'd'

#endif

/*
 * enable power down of MEMSYS PLL
 * 7436 and 7425 only
 */
#if defined(CONFIG_BCM7346)
	li	t0, REG(BCHP_CLKGEN_MEMSYS_32_INST_POWER_MANAGEMENT)
	li	t1, BCHP_CLKGEN_MEMSYS_32_INST_POWER_MANAGEMENT_MEMSYS_PLL_PWRDN_POWER_MANAGEMENT_MASK
	sw	t1, 0(t0)
#endif

#if defined(CONFIG_BCM7425)
	li	t0, REG(BCHP_CLKGEN_MEMSYS_0_32_POWER_MANAGEMENT)
	li	t1, BCHP_CLKGEN_MEMSYS_0_32_POWER_MANAGEMENT_MEMSYS_PLL_PWRDN_POWER_MANAGEMENT_MASK
	sw	t1, 0(t0)
#endif

# power down request
2:
	/* delay if requested by the user: 4 * 30s */
	li	t0, BRCM_STANDBY_DELAY
	and	t0, FLAGREG
	beqz	t0, 2f
	nop

	bal	dputc
	li	a0, 'w'

	li	v1, 4
1:	li	a0, (UPGTMR_FREQ * 30)
	bal	delay
	addiu	v1, -1
	bnez	v1, 1b
	nop

	bal	dputc
	li	a0, 'W'

# power down request
2:
	/* set wait count */
	li	t0, REG(PM_WAIT_COUNT)
	li	t1, 0xffff
	sw	t1, 0(t0)
	lw	zero, 0(t0)

	li	t0, REG(PM_CONTROL)
	li	t1, PM_STANDBY_COMMAND
	sw	zero, 0(t0)
	lw	zero, 0(t0)
	sw	t1, 0(t0)
	lw	t1, 0(t0)

/***********************************************************************
 * Wait for interrupt
 ***********************************************************************/

wait_loop:
# enable int2 and then wait for an interrupt
	mfc0	t0, CP0_STATUS
# save cp0 sr
	move	s2, t0

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

/***********************************************************************
 * Save wakeup timestamp in AON
 ***********************************************************************/
#ifdef CONFIG_BRCM_HAS_AON
	li	t0, REG(BCHP_WKTMR_COUNTER)
	lw	a0, 0(t0)	/* WKTMR_COUNTER */
	lw	a1, 12(t0)	/* WKTMR_PRESCALER_VAL */
	li	t0, REG(AON_RAM_BASE)
	sw	a0, 0(t0)
	sw	a1, 4(t0)
#endif

/***********************************************************************
 * Bring MEMC back up
 ***********************************************************************/
	bal	dputc
	li	a0, 'M'

#if USE_WATCHDOG
# arm the watchdog timer
	li	t0, REG(BCHP_TIMER_WDTIMEOUT)
	li	t1, 3 * UPGTMR_FREQ
	sw	t1, 0(t0)
	lw	t1, 0(t0)

	li	t0, REG(BCHP_TIMER_WDCMD)
	li	t1, 0xff00
	sw	t1, 0(t0)
	lw	t1, 0(t0)
	li	t1, 0x00ff
	sw	t1, 0(t0)
	lw	t1, 0(t0)
#endif

	/*
	 * Bypass MEMC wakeup if DDR PLL is on
	 */
	li	t0, BRCM_STANDBY_DDR_PLL_ON
	and	t0, FLAGREG
	bnez	t0, 2f
	nop

	bal	wake_up_memc_0
	nop

2:
	bal	dputc
	li	a0, 'm'

#if defined(CONFIG_BRCM_PWR_HANDSHAKE_V0)
# disarm the BSP
	li	t0, REG(BCHP_BSP_GLB_CONTROL_REG_START + 0x38)
	lw	t1, 0(t0)
	ori	t1, 0xff00
	xori	t1, 0xfe00
	sw	t1, 0(t0)

	bal	dputc
	li	a0, 'B'

	li	t0, REG(PM_STATUS)
	li	t2, PM_STATUS_bsp_ready
1:
	lw	t1, 0(t0)
	and	t1, t2
	bnez	t1, 1b
	nop

	bal	dputc
	li	a0, 'b'
#elif defined(CONFIG_BRCM_PWR_HANDSHAKE_V1)
	li	t0, REG(BCHP_AON_CTRL_HOST_MISC_CMDS)
	li	t1, BCHP_AON_CTRL_HOST_MISC_CMDS_pm_restore_MASK
	sw	t1, 0(t0)
	lw	t1, 0(t0)
#endif /* CONFIG_BRCM_PWR_HANDSHAKE_V0/V1 */

	li	t0, REG(PM_CONTROL)
	sw	zero, 0(t0)
	lw	zero, 0(t0)

#ifdef CONFIG_BRCM_HAS_ANALOG_DDR_PHY
	li	t0, BRCM_STANDBY_DDR_PLL_ON
	and	t0, FLAGREG
	beqz	t0, 2f
	nop

	bal	set_pll_frequency
	li	a0, 0x3d43

	bal	set_pll_frequency
	li	a0, 0x4a43
2:
#endif

/***********************************************************************
 * Unlock I$ lines and resume execution
 ***********************************************************************/
	sync

rtn_from_wait:
	bal	dputc
	li	a0, 'S'

# unlock I$ lines
	addiu	t1, s0, -1
	not	t1

	la	t0, brcm_pm_standby_asm
	and	t0, t1

	la	t2, passiveIRQ_end
	and	t2, t1

1:	cache	Hit_Invalidate_I, 0(t0)
	bne	t0, t2, 1b
	addu	t0, s0

	bal	dputc
	li	a0, 'L'

# restore sr
	sync
	nop

	bal	dputc
	li	a0, 'l'
	bal	dputc
	li	a0, 0x0d
	bal	dputc
	li	a0, 0x0a

	mtc0	s2, CP0_STATUS
	nop

	li	v0, 0

standby_exit:

# return to caller
	lw	s7, 32(sp)
	lw	s6, 28(sp)
	lw	s5, 24(sp)
	lw	s4, 20(sp)
	lw	s3, 16(sp)
	lw	s2, 12(sp)
	lw	s1, 8(sp)
	lw	s0, 4(sp)
	lw	ra, 0(sp)
	addiu	sp, 64

	jr	ra
	nop
	END(brcm_pm_standby_asm)

# debug character output

dputc:
	/* INPUT: char in a0 */
	/* USES: t1, t2 */

	li	t1, BRCM_STANDBY_VERBOSE
	and	t1, FLAGREG
	bnez	t1, 1f
	nop
	jr	ra
	nop

1:
	li	t1, REG(UART_BASE)
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

/***********************************************************************
 * Delay loop
 ***********************************************************************/

delay:
# need to reset TIMER1 since it is not normally used
	li	t2, 0x0
	li	t1, REG(BCHP_TIMER_TIMER1_CTRL)
	sw	t2, 0(t1)
	lw	t2, 0(t1)

	li	t2, 0xbfffffff
	sw	t2, 0(t1)
	lw	t2, 0(t1)

	li	t2, 0x3fffffff
	li	t1, REG(BCHP_TIMER_TIMER1_STAT)
	lw	t0, 0(t1)
	and	t0, t2
	add	t0, a0
	not	t2
	and	t2, t0
	bnez	t2, delay_overflow
	nop

# wait for timer value (t2) to exceed expiry time (t0)
1:	lw	t2, 0(t1)
	sltu	t2, t2, t0
	bnez	t2, 1b
	nop

	jr	ra
	nop

# timer1 overflow (this should never happen)
delay_overflow:
	move	t4, ra
	bal	dputc
	li	a0, 'O'

	jr	t4
	nop

/***********************************************************************
 * PM IRQ handler
 ***********************************************************************/

LEAF(brcm_pm_irq)

# Block interrupts and then return to the wait loop
	mfc0	k0, CP0_STATUS
	li	k1, ~ST0_IE
	and	k0, k1
	mtc0	k0, CP0_STATUS
	eret
	nop

END(brcm_pm_irq)

/***********************************************************************
 * MEMC wakeup
 ***********************************************************************/

#ifdef CONFIG_BRCM_HAS_ANALOG_DDR_PHY

#define SLEEP_1_MILLI_SEC		27000

/***********************************************************************
 * Delay routine
 ***********************************************************************/
LEAF(upg_sleep)

	li	t0, REG(BCHP_TIMER_TIMER0_CTRL)
	lw	t1, 0(t0)

	/* TIMER_TIMER0_CTRL.MODE  = 1 => count down mode */
	li	t2, BCHP_TIMER_TIMER0_CTRL_MODE_MASK
	or	t1, t1, t2

	/* TIMER_TIMER0_CTRL.TIMEOUT_VAL = a0 */

	li	t2, BCHP_TIMER_TIMER0_CTRL_TIMEOUT_VAL_MASK
	li	t3, BCHP_TIMER_TIMER0_CTRL_TIMEOUT_VAL_SHIFT

	not	t2, t2
	and	t1, t1, t2
	sll	a0, a0, t3
	or	t1, t1, a0

	sw	t1, 0(t0)

	li	t0, REG(BCHP_TIMER_TIMER_IS)
	lw	t1, 0(t0)

	/* TIMER_TIMER_IS.TMR0TO = 1 => clears this bit, resets time count */

	#li	t2, ~BCHP_TIMER_TIMER_IS_TMR0TO_MASK
	#and	t1, t1, t2
	sw	t1, 0(t0)

	/* TIMER_TIMER0_CTRL.ENA = 1 => start counting */

	li	t0, REG(BCHP_TIMER_TIMER0_CTRL)
	lw	t1, 0(t0)

	li	t2, BCHP_TIMER_TIMER0_CTRL_ENA_MASK
	or	t1, t1, t2
	sw	t1, 0(t0)

	li	t0, REG(BCHP_TIMER_TIMER_IS)
	li	t2, BCHP_TIMER_TIMER_IS_TMR0TO_MASK

1:	lw	t1, 0(t0)
	and	t1, t1, t2
	beq	zero, t1, 1b
	nop

	/* TIMER_TIMER0_CTRL.ENA = 0 => stop counting */

	li	t0, REG(BCHP_TIMER_TIMER0_CTRL)
	lw	t1, 0(t0)
	li	t2, ~BCHP_TIMER_TIMER0_CTRL_ENA_MASK
	and	t1, t1, t2
	sw	t1, 0(t0)

	jr	ra
	nop

END(upg_sleep)

/***********************************************************************
 * Main MEMC0 recovery function
 ***********************************************************************/
LEAF(wake_up_memc_0)

	move	s7, ra

	li	a0, 0
	bal	set_memc_0_write_dqs_phase_cntl_direct
	nop

	li	a0, 0
	bal	set_memc_0_write_dq_phase_cntl_direct
	nop

	li	a0, 0
	bal	set_memc_0_pll_ch2_clk_phase_cntrl
	nop

	li	a0, 0
	bal	set_memc_0_deskew_bypass_phase
	nop

	li	a0, REG(BCHP_MEMC_DDR23_APHY_WL0_0_DDR_PAD_CNTRL)
	lw	a1, 0(a0)
	li	a2, ~(BCHP_MEMC_DDR23_APHY_WL0_0_DDR_PAD_CNTRL_IDDQ_MODE_ON_SELFREF_MASK)
	and	a1, a1, a2
	sw	a1, 0(a0)

	li	a0, REG(BCHP_MEMC_DDR23_APHY_WL1_0_DDR_PAD_CNTRL)
	lw	a1, 0(a0)
	li	a2, ~(BCHP_MEMC_DDR23_APHY_WL1_0_DDR_PAD_CNTRL_IDDQ_MODE_ON_SELFREF_MASK)
	and	a1, a1, a2
	sw	a1, 0(a0)


	li	a0, REG(BCHP_MEMC_DDR23_APHY_WL0_0_WORDSLICE_CNTRL_1)
	lw	a1, 0(a0)
	li	a2, ~(BCHP_MEMC_DDR23_APHY_WL0_0_WORDSLICE_CNTRL_1_PWRDN_DLL_ON_SELFREF_MASK)
	and	a1, a1, a2
	sw	a1, 0(a0)

	li	a0, REG(BCHP_MEMC_DDR23_APHY_WL1_0_WORDSLICE_CNTRL_1)
	lw	a1, 0(a0)
	li	a2, ~(BCHP_MEMC_DDR23_APHY_WL1_0_WORDSLICE_CNTRL_1_PWRDN_DLL_ON_SELFREF_MASK)
	and	a1, a1, a2
	sw	a1, 0(a0)

	li	a0, REG(BCHP_MEMC_DDR23_APHY_AC_0_POWERDOWN)
	lw	a1, 0(a0)
	li	a2, ~(BCHP_MEMC_DDR23_APHY_AC_0_POWERDOWN_PLLCLKS_OFF_ON_SELFREF_MASK)
	and	a1, a1, a2
	sw	a1, 0(a0)

	bal	reprogram_ddr_pll
	nop

	jr	s7
	nop

END(wake_up_memc_0)

/***********************************************************************
 * VCDL incremental reload
 * a0 - VCDL register
 * a1 - maximum value
 ***********************************************************************/
reload_vcdl:
	sll	v0, a1, 8
	or	a1, v0		# max value to load

	move	v1, zero
vcdl_loop:
	addi	v1, 0x0101
	sw	v1, 0(a0)
	lw	zero, 0(a0)
	bne	v1, a1, vcdl_loop
	nop

	jr	ra
	nop

/***********************************************************************
 * Set new DDR PLL frequency divider
 * Used when suspending with DDR PLL on
 * a0 - Frequency divider value
 ***********************************************************************/
LEAF(set_pll_frequency)

	move	s3, ra

	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_PLL_FREQ_CNTL)
	sw	a0, 0(v0)
	lw	zero, 0(v0)

	li	a0, (27000)
	bal	upg_sleep
	nop

	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_DDR_PLL_LOCK_STATUS)
	li	a0, BCHP_MEMC_DDR23_APHY_AC_0_DDR_PLL_LOCK_STATUS_LOCK_STATUS_MASK
1:
	lw	v1, 0(v0)
	and	v1, v1, a0

	beqz	v1, 1b
	nop

	jr	s3
	nop
END(set_pll_frequency)

/***********************************************************************
 * DDR PLL reset and reconfiguration
 ***********************************************************************/
LEAF(reprogram_ddr_pll)

	move	s6, ra

	/* reset the freq divider */
	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_RESET)
	li	v1, BCHP_MEMC_DDR23_APHY_AC_0_RESET_FREQ_DIV_RESET_MASK
	lw	a0, 0(v0)
	or	a0, a0, v1
	sw	a0, 0(v0)

	li	a0, (SLEEP_1_MILLI_SEC)
	bal	upg_sleep
	nop

	/* reset the vcxo */
	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_RESET)
	li	v1, BCHP_MEMC_DDR23_APHY_AC_0_RESET_VCXO_RESET_MASK
	lw	a0, 0(v0)
	or	a0, a0, v1
	sw	a0, 0(v0)

	li	a0, (SLEEP_1_MILLI_SEC)
	bal	upg_sleep
	nop

	/* reset DATAPATH_216, RD_DATAPATH_RESET, RESET_DATAPATH_DDR  */
	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_RESET)
	li	v1, (BCHP_MEMC_DDR23_APHY_AC_0_RESET_DATAPATH_216_RESET_MASK | \
		     BCHP_MEMC_DDR23_APHY_AC_0_RESET_RD_DATAPATH_RESET_MASK | \
		     BCHP_MEMC_DDR23_APHY_AC_0_RESET_DATAPATH_DDR_RESET_MASK)
	lw	a0, 0(v0)
	or	a0, a0, v1
	sw	a0, 0(v0)

	/* de-assert reset from vcxo */
	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_RESET)
	li	v1, ~BCHP_MEMC_DDR23_APHY_AC_0_RESET_VCXO_RESET_MASK
	lw	a0, 0(v0)
	and	a0, a0, v1
	sw	a0, 0(v0)

	/* de-assert reset from freq divider */
	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_RESET)
	li	v1, ~BCHP_MEMC_DDR23_APHY_AC_0_RESET_FREQ_DIV_RESET_MASK
	lw	a0, 0(v0)
	and	a0, a0, v1
	sw	a0, 0(v0)

	/* wait for 1ms for stable clock */
	li	a0, (SLEEP_1_MILLI_SEC)
	bal	upg_sleep
	nop

	/* check for pll lock */

	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_DDR_PLL_LOCK_STATUS)
	li	a0, BCHP_MEMC_DDR23_APHY_AC_0_DDR_PLL_LOCK_STATUS_LOCK_STATUS_MASK
1:
	lw	v1, 0(v0)
	and	v1, v1, a0

	beqz	v1, 1b
	nop

	li	a1, REG(BCHP_MEMC_GEN_0_MSA_WR_DATA4)
	lw	a0, 0(a1)

	bal	set_memc_0_wl0_dq_phase
	nop

	/* set wl1_dq phase */
	li	a1, REG(BCHP_MEMC_GEN_0_MSA_WR_DATA5)
	lw	a0, 0(a1)

	bal	set_memc_0_wl1_dq_phase
	nop

	/* set ch2 phase */
	li	a1, REG(BCHP_MEMC_GEN_0_MSA_WR_DATA6)
	lw	a0, 0(a1)

	move	a1, a0
	and	a0, a0, zero
1:
	bal	set_memc_0_pll_ch2_clk_phase_cntrl
	nop
	add	a0, a0, 1

	bne	a0, a1, 1b
	nop

	/* set ch6 phase */
	li	a1, REG(BCHP_MEMC_MISC_0_SCRATCH_0)
	lw	a0, 0(a1)

	move	a1, a0
	and	a0, a0, zero
1:
	bal	set_memc_0_deskew_bypass_phase
	nop

	add	a0, a0, 1

	bne	a0, a1, 1b
	nop

	/* set wl0_dqs0 phases */
	li	a1, REG(BCHP_MEMC_GEN_0_MSA_WR_DATA0)
	lw	a0, 0(a1)

	bal	set_memc_0_dqs0_phase
	nop

	/* set wl0_dqs1 phases */

	li	a1, REG(BCHP_MEMC_GEN_0_MSA_WR_DATA1)
	lw	a0, 0(a1)

	bal	set_memc_0_dqs1_phase
	nop

	/* set wl1_dqs0 phases */

	li	a1, REG(BCHP_MEMC_GEN_0_MSA_WR_DATA2)
	lw	a0, 0(a1)

	bal	set_memc_0_dqs2_phase
	nop
	/* set wl1_dqs1 phases */

	li	a1, REG(BCHP_MEMC_GEN_0_MSA_WR_DATA3)
	lw	a0, 0(a1)

	bal	set_memc_0_dqs3_phase
	nop

	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_DDR_PAD_CNTRL)
	lw	v1, 0(v0)
	ori     v1, (BCHP_MEMC_DDR23_APHY_AC_0_DDR_PAD_CNTRL_DEVCLK_OFF_ON_SELFREF_MASK | BCHP_MEMC_DDR23_APHY_AC_0_DDR_PAD_CNTRL_IDDQ_MODE_ON_SELFREF_MASK)
	xori	v1, (BCHP_MEMC_DDR23_APHY_AC_0_DDR_PAD_CNTRL_DEVCLK_OFF_ON_SELFREF_MASK | BCHP_MEMC_DDR23_APHY_AC_0_DDR_PAD_CNTRL_IDDQ_MODE_ON_SELFREF_MASK)
	sw	v1, 0(v0)
	lw	zero, 0(v0)

	li	a0, (SLEEP_1_MILLI_SEC)
	bal	upg_sleep
	nop

	/* reset the word slice dll */

	li	v0, REG(BCHP_MEMC_DDR23_APHY_WL0_0_WORD_SLICE_DLL_RESET)
	li	v1, 1
	sw	v1, 0(v0)
	li	v0, REG(BCHP_MEMC_DDR23_APHY_WL1_0_WORD_SLICE_DLL_RESET)
	sw	v1, 0(v0)

	li	a0, (SLEEP_1_MILLI_SEC)
	bal	upg_sleep
	nop

	/* reset VCDL values */
	li	t0, REG(BCHP_MEMC_DDR23_APHY_WL0_0_BYTE0_VCDL_PHASE_CNTL)
	sw	zero, 0(t0)
	lw	zero, 0(t0)
	sw	zero, 4(t0)
	lw	zero, 4(t0)

	li	t0, REG(BCHP_MEMC_DDR23_APHY_WL1_0_BYTE0_VCDL_PHASE_CNTL)
	sw	zero, 0(t0)
	lw	zero, 0(t0)
	sw	zero, 4(t0)
	lw	zero, 4(t0)

	li	v0, REG(BCHP_MEMC_DDR23_APHY_WL0_0_WORD_SLICE_DLL_RESET)
	li	v1, 0
	sw	v1, 0(v0)
	li	v0, REG(BCHP_MEMC_DDR23_APHY_WL1_0_WORD_SLICE_DLL_RESET)
	sw	v1, 0(v0)

	li	a0, (SLEEP_1_MILLI_SEC)
	bal	upg_sleep
	nop

	/* de-assert reset from DATAPATH_216 and DATAPATH_DDR */
	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_RESET)
	lw	v1, 0(v0)

	/* de-assert reset from DATAPATH_216_RESET */
	li	a0, ~(BCHP_MEMC_DDR23_APHY_AC_0_RESET_DATAPATH_216_RESET_MASK)
	and	v1, v1, a0
	sw	v1, 0(v0)

	/* de-assert reset from RD_DATAPATH_RESET */
	li	a0, ~(BCHP_MEMC_DDR23_APHY_AC_0_RESET_RD_DATAPATH_RESET_MASK)
	and	v1, v1, a0
	sw	v1, 0(v0)

	/* de-assert reset from DATAPATH_DDR_RESET */
	li	a0, ~(BCHP_MEMC_DDR23_APHY_AC_0_RESET_DATAPATH_DDR_RESET_MASK)
	and	v1, v1, a0
	sw	v1, 0(v0)

	li	a0, (SLEEP_1_MILLI_SEC)
	bal	upg_sleep
	nop

	/*
	 * Reload VCDL values:
	 * WL0_0_BYTE0 <- bits 07:00
	 * WL0_0_BYTE1 <- bits 15:08
	 * WL1_0_BYTE0 <- bits 23:16
	 * WL1_0_BYTE1 <- bits 31:24
	 */

	li	a0, REG(BCHP_MEMC_DDR23_APHY_WL0_0_BYTE0_VCDL_PHASE_CNTL)
	li	t0, REG(VCDL_SAVE_REG)
	lw	t0, 0(t0)
	bal	reload_vcdl
	andi	a1, t0, 0x1f

	srl	a1, t0, 8
	li	a0, REG(BCHP_MEMC_DDR23_APHY_WL0_0_BYTE1_VCDL_PHASE_CNTL)
	bal	reload_vcdl
	andi	a1, 0x1f

	srl	a1, t0, 16
	li	a0, REG(BCHP_MEMC_DDR23_APHY_WL1_0_BYTE0_VCDL_PHASE_CNTL)
	bal	reload_vcdl
	andi	a1, 0x1f

	srl	a1, t0, 24
	li	a0, REG(BCHP_MEMC_DDR23_APHY_WL1_0_BYTE1_VCDL_PHASE_CNTL)
	bal	reload_vcdl
	andi	a1, 0x1f

	jr	s6
	nop

END(reprogram_ddr_pll)

LEAF(set_memc_0_write_dqs_phase_cntl_direct)

	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_PLL_CH4_WL0_DQS0_PHASE_CNTRL)
	sw	a0, 0(v0)
	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_PLL_CH5_WL0_DQS1_PHASE_CNTRL)
	sw	a0, 0(v0)

	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_PLL_CH8_WL1_DQS0_PHASE_CNTRL)
	sw	a0, 0(v0)
	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_PLL_CH9_WL1_DQS1_PHASE_CNTRL)
	sw	a0, 0(v0)

	jr	ra
	nop

END(set_memc_0_write_dqs_phase_cntl_direct)

LEAF(set_memc_0_write_dq_phase_cntl_direct)

	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_PLL_CH3_WL0_DQ_PHASE_CNTRL)
	sw	a0, 0(v0)
	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_PLL_CH7_WL1_DQ_PHASE_CNTRL)
	sw	a0, 0(v0)

	jr	ra
	nop

END(set_memc_0_write_dq_phase_cntl_direct)

LEAF(set_memc_0_pll_ch2_clk_phase_cntrl)

	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_PLL_CH2_CLOCK_PHASE_CNTRL)
	sw	a0, 0(v0)

	jr	ra
	nop
END(set_memc_0_pll_ch2_clk_phase_cntrl)

LEAF(set_memc_0_deskew_bypass_phase)

	li	t8, REG(BCHP_MEMC_DDR23_APHY_AC_0_DESKEW_DLL_CNTRL)
	lw	t7, 0(t8)

	li	t6, ~BCHP_MEMC_DDR23_APHY_AC_0_DESKEW_DLL_CNTRL_BYPASS_PHASE_MASK
	and	t7, t7, t6

	li	t6, BCHP_MEMC_DDR23_APHY_AC_0_DESKEW_DLL_CNTRL_BYPASS_PHASE_SHIFT
	sll	t5, a0, t6
	or	t7, t7, t5
	sw	t7, 0(t8)

	jr	ra
	nop
END(set_memc_0_deskew_bypass_phase)

LEAF(set_memc_0_wl0_dq_phase)

	addi	a0, a0, 1
	and	a1, a1, zero
	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_PLL_CH3_WL0_DQ_PHASE_CNTRL)
2:
	sw	a1, 0(v0)
	addi	a1, a1, 1
	bne	a1, a0, 2b
	nop

	jr	ra
	nop
END(set_memc_0_wl0_dq_phase)

LEAF(set_memc_0_wl1_dq_phase)

	addi	a0, a0, 1
	and	a1, a1, zero
	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_PLL_CH7_WL1_DQ_PHASE_CNTRL)
2:
	sw	a1, 0(v0)
	addi	a1, a1, 1
	bne	a1, a0, 2b
	nop

	jr	ra
	nop
END(set_memc_0_wl1_dq_phase)


LEAF(set_memc_0_dqs0_phase)

	li	a2, -1
	blt	a0, zero, 1f
	nop

	li	a2, 1
1:
	and	a1, a1, zero
	add	a0, a0, a2
	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_PLL_CH4_WL0_DQS0_PHASE_CNTRL)
2:
	sw	a1, 0(v0)
	add	a1, a1, a2
	bne	a1, a0, 2b
	nop

	jr	ra
	nop

END(set_memc_0_dqs0_phase)

LEAF(set_memc_0_dqs1_phase)

	li	a2, -1
	blt	a0, zero, 1f
	nop

	li	a2, 1
1:
	and	a1, a1, zero
	add	a0, a0, a2
	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_PLL_CH5_WL0_DQS1_PHASE_CNTRL)
2:
	sw	a1, 0(v0)
	add	a1, a1, a2
	bne	a1, a0, 2b
	nop

	jr	ra
	nop


END(set_memc_0_dqs1_phase)

LEAF(set_memc_0_dqs2_phase)

	li	a2, -1
	blt	a0, zero, 1f
	nop

	li	a2, 1
1:
	and	a1, a1, zero
	add	a0, a0, a2
	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_PLL_CH8_WL1_DQS0_PHASE_CNTRL)
2:
	sw	a1, 0(v0)
	add	a1, a1, a2
	bne	a1, a0, 2b
	nop

	jr	ra
	nop

END(set_memc_0_dqs2_phase)

LEAF(set_memc_0_dqs3_phase)

	li	a2, -1
	blt	a0, zero, 1f
	nop

	li	a2, 1
1:
	and	a1, a1, zero
	add	a0, a0, a2
	li	v0, REG(BCHP_MEMC_DDR23_APHY_AC_0_PLL_CH9_WL1_DQS1_PHASE_CNTRL)
2:
	sw	a1, 0(v0)
	add	a1, a1, a2
	bne	a1, a0, 2b
	nop

	jr	ra
	nop

END(set_memc_0_dqs3_phase)

LEAF(memc_settings_valid)

	li	t2, 0xdeadbeef
	li	t1, REG(BCHP_MEMC_GEN_0_MSA_WR_DATA7)
	lw	t0, 0(t1)
	li	v0, 0
	beq	t0, t2, 1f
	nop
	li	v0, -1

1:	jr	ra
	nop

END(memc_settings_valid)

#endif /* CONFIG_BRCM_HAS_ANALOG_DDR_PHY */

#ifdef CONFIG_BRCM_HAS_DIGITAL_DDR_PHY

LEAF(memc_settings_valid)

1:	jr	ra
	li	v0, 0

END(memc_settings_valid)

LEAF(wake_up_memc_0)

	move	s7, ra

#if !defined(CONFIG_BRCM_PWR_HANDSHAKE)
	li	t0, REG(BCHP_MEMC_DDR_0_SSPD_CMD)
	li	t1, 0
	sw	t1, 0(t0)
#endif

#ifdef BCHP_DDR23_PHY_CONTROL_REGS_0_PLL_STATUS
	li	t0, REG(BCHP_DDR23_PHY_CONTROL_REGS_0_PLL_STATUS)
#elif defined(BCHP_DDR40_PHY_CONTROL_REGS_0_PLL_STATUS)
	li	t0, REG(BCHP_DDR40_PHY_CONTROL_REGS_0_PLL_STATUS)
#else
#error No MEMC phy control register defined
#endif

1:	lw	t1, 0(t0)
	andi	t1, 1
	beqz	t1, 1b
	nop

# 1 ms delay - needed on 7552/7358 for stable recovery
	li	a0, (27000)
	bal	delay
	nop

	jr	s7
	nop

END(wake_up_memc_0)

#endif /* CONFIG_BRCM_HAS_DIGITAL_DDR_PHY */

	.globl passiveIRQ_end
passiveIRQ_end:
	nop
