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

#ifndef __ASM_MACH_BCM3384_IRQ_H
#define __ASM_MACH_BCM3384_IRQ_H

#define INTERRUPT_ID_SOFTWARE_0         0
#define INTERRUPT_ID_SOFTWARE_1         1

#define BRCM_IRQ_IPI0			INTERRUPT_ID_SOFTWARE_0
#define BRCM_IRQ_IPI1			INTERRUPT_ID_SOFTWARE_1

/*=====================================================================*/
/* BCM3384 Timer Interrupt Level Assignments                          */
/*=====================================================================*/
#define MIPS_TIMER_INT			7

/*=====================================================================*/
/* Logical Peripheral Interrupt IDs                                    */
/*=====================================================================*/

#define PERIPH_INT_START		 (MIPS_TIMER_INT + 1)
#define PERIPH_INT_END		         (PERIPH_INT_START + 32)

#define INTERRUPT_ID_TIMER               (PERIPH_INT_START + 0)	/* 8 */
#define INTERRUPT_ID_UART0               (PERIPH_INT_START + 1)
#define INTERRUPT_ID_UART1               (PERIPH_INT_START + 2)
#define INTERRUPT_ID_NAND                (PERIPH_INT_START + 3)
#define INTERRUPT_ID_I2C                 (PERIPH_INT_START + 4)
#define INTERRUPT_ID_HS_SPI              (PERIPH_INT_START + 5)
#define INTERRUPT_ID_PERIPH_ERR          (PERIPH_INT_START + 6)
#define INTERRUPT_ID_UBUS_REQOUT_ERR     (PERIPH_INT_START + 7)
#define INTERRUPT_ID_UART2               (PERIPH_INT_START + 8)
#define INTERRUPT_ID_DSCRAM_KEYDONE      (PERIPH_INT_START + 9)
#define INTERRUPT_ID_DSCRAM_RNG_READY    (PERIPH_INT_START + 10)
#define INTERRUPT_ID_EXT_IRQ             (PERIPH_INT_START + 15)
#define INTERRUPT_ID_DEMOD_XPT_TX_WAKEUP (PERIPH_INT_START + 23)
#define INTERRUPT_ID_DEMOD_XPT_TX        (PERIPH_INT_START + 24)
#define INTERRUPT_ID_DEMOD_XPT_RX_WAKEUP (PERIPH_INT_START + 25)
#define INTERRUPT_ID_DEMOD_XPT_RX        (PERIPH_INT_START + 26)
#define INTERRUPT_ID_LMBB_BNM_HOST       (PERIPH_INT_START + 27)
#define INTERRUPT_ID_PMC_MIPS            (PERIPH_INT_START + 28)
#define INTERRUPT_ID_UGB_ZMIPS           (PERIPH_INT_START + 29)
#define INTERRUPT_ID_UGB_CMIPS1          (PERIPH_INT_START + 30)
#define INTERRUPT_ID_UGB_CMIPS           (PERIPH_INT_START + 31) /* 39 */

/*=====================================================================*/
/* Logical EXT Peripheral Interrupt IDs                                */
/*=====================================================================*/

#define PERIPH_EXT_INT_START             (PERIPH_INT_END)
#define PERIPH_EXT_INT_END		 (PERIPH_EXT_INT_START + 32)

#define INTERRUPT_ID_UNI_DMA		 (PERIPH_EXT_INT_START + 0) /* 40 */
#define INTERRUPT_ID_UNI1		 (PERIPH_EXT_INT_START + 1)
#define INTERRUPT_ID_UNI0		 (PERIPH_EXT_INT_START + 2)
#define INTERRUPT_ID_GPHY		 (PERIPH_EXT_INT_START + 3)
#define INTERRUPT_ID_UNI0_MPD_WOL	 (PERIPH_EXT_INT_START + 4)
#define INTERRUPT_ID_UNI0_HFB_WOL	 (PERIPH_EXT_INT_START + 5)
#define INTERRUPT_ID_UNI1_MPD_WOL	 (PERIPH_EXT_INT_START + 6)
#define INTERRUPT_ID_UNI1_HFB_WOL	 (PERIPH_EXT_INT_START + 7)
#define INTERRUPT_ID_USBH_OHCI		 (PERIPH_EXT_INT_START + 8)
#define INTERRUPT_ID_USBH_EHCI		 (PERIPH_EXT_INT_START + 9)
#define INTERRUPT_ID_USBH_DISCON	 (PERIPH_EXT_INT_START + 10)
#define INTERRUPT_ID_USBH_CCS	         (PERIPH_EXT_INT_START + 11)
#define INTERRUPT_ID_PCIE0	         (PERIPH_EXT_INT_START + 12)
#define INTERRUPT_ID_PCIE1		 (PERIPH_EXT_INT_START + 13)
#define INTERRUPT_ID_APM		 (PERIPH_EXT_INT_START + 14)
#define INTERRUPT_ID_APM_IUDMA		 (PERIPH_EXT_INT_START + 15)
#define INTERRUPT_ID_DAVIC		 (PERIPH_EXT_INT_START + 16)
#define INTERRUPT_ID_FPM		 (PERIPH_EXT_INT_START + 17)
#define INTERRUPT_ID_MEMSYS		 (PERIPH_EXT_INT_START + 18)
#define INTERRUPT_ID_DECT0		 (PERIPH_EXT_INT_START + 19)
#define INTERRUPT_ID_DECT1		 (PERIPH_EXT_INT_START + 20)
#define INTERRUPT_ID_USBD_SYS		 (PERIPH_EXT_INT_START + 21)
#define INTERRUPT_ID_USBD_CONTROL_RX	 (PERIPH_EXT_INT_START + 22)
#define INTERRUPT_ID_USBD_CONTROL_TX	 (PERIPH_EXT_INT_START + 23)
#define INTERRUPT_ID_USBD_BULK_OUT		(PERIPH_EXT_INT_START + 24)
#define INTERRUPT_ID_USBD_BULK_IN		(PERIPH_EXT_INT_START + 25)
#define INTERRUPT_ID_USBD_OUT			(PERIPH_EXT_INT_START + 26)
#define INTERRUPT_ID_USBD_IN			(PERIPH_EXT_INT_START + 27)
#define INTERRUPT_ID_PER_ERR_PORT		(PERIPH_EXT_INT_START + 28)
#define INTERRUPT_ID_MEMSYS_UB_AEGIS_WRCH	(PERIPH_EXT_INT_START + 29)
#define INTERRUPT_ID_MEMSYS_UB_AEGIS_ARCH	(PERIPH_EXT_INT_START + 30)
#define INTERRUPT_ID_PM_MIPS_READY_FOR_PWRDN	(PERIPH_EXT_INT_START + 31) /* 71 */

#if defined(CONFIG_BMIPS5000)
/*=====================================================================*/
/* ZMIPS level 2 interrupts                                            */
/*=====================================================================*/
#define ZMIPS_INT_START                    (PERIPH_EXT_INT_END)
#define ZMIPS_INT_END                      (ZMIPS_INT_START + 32)
#define XMIPS_INT_END                      ZMIPS_INT_END

#define INTERRUPT_ID_G2U_ZMIPS_MBOX0       (ZMIPS_INT_START + 0) /* 72 */
#define INTERRUPT_ID_G2U_ZMIPS_MBOX1       (ZMIPS_INT_START + 1)
#define INTERRUPT_ID_G2U_ZMIPS_MBOX2       (ZMIPS_INT_START + 2)
#define INTERRUPT_ID_G2U_ZMIPS_MBOX3       (ZMIPS_INT_START + 3)
#define INTERRUPT_ID_G2U_ZMIPS_MBOX4       (ZMIPS_INT_START + 4)
#define INTERRUPT_ID_G2U_ZMIPS_MBOX5       (ZMIPS_INT_START + 5)
#define INTERRUPT_ID_G2U_ZMIPS_MBOX6       (ZMIPS_INT_START + 6)
#define INTERRUPT_ID_G2U_ZMIPS_MBOX7       (ZMIPS_INT_START + 7)
#define INTERRUPT_ID_G2U_ZMIPS_MBOX8       (ZMIPS_INT_START + 8)
#define INTERRUPT_ID_G2U_ZMIPS_MBOX9       (ZMIPS_INT_START + 9)
#define INTERRUPT_ID_G2U_ZMIPS_MBOX10      (ZMIPS_INT_START + 10)
#define INTERRUPT_ID_G2U_ZMIPS_MBOX11      (ZMIPS_INT_START + 11)
#define INTERRUPT_ID_G2U_ZMIPS_MBOX12      (ZMIPS_INT_START + 12)
#define INTERRUPT_ID_G2U_ZMIPS_MBOX13      (ZMIPS_INT_START + 13)
#define INTERRUPT_ID_G2U_ZMIPS_MBOX14      (ZMIPS_INT_START + 14)
#define INTERRUPT_ID_G2U_ZMIPS_MBOX15      (ZMIPS_INT_START + 15)
#define INTERRUPT_ID_G2U_ZMIPS_ERROR_G2U_TIMEOUT      (ZMIPS_INT_START + 16)
#define INTERRUPT_ID_G2U_ZMIPS_ERROR_UB_SLAVE_TIMEOUT (ZMIPS_INT_START + 17)
#define INTERRUPT_ID_G2U_ZMIPS_ERROR_G2U_IRQ          (ZMIPS_INT_START + 18)
#define INTERRUPT_ID_G2U_ZMIPS_ERROR_UB_MASTER        (ZMIPS_INT_START + 19)
#define INTERRUPT_ID_G2U_ZMIPS_ERROR_GB_DQM_OVERFLOW  (ZMIPS_INT_START + 20)
#define INTERRUPT_ID_G2U_ZMIPS_ERROR_UB_DQM_OVERFLOW  (ZMIPS_INT_START + 21)
#define INTERRUPT_ID_G2U_ZMIPS_DQM                    (ZMIPS_INT_START + 22)
#define INTERRUPT_ID_G2U_ZMIPS_COUNTER                (ZMIPS_INT_START + 23) /* 95 */

#define INTERRUPT_ID_DQM_IRQ      			INTERRUPT_ID_G2U_ZMIPS_DQM
#define INTERRUPT_ID_G2U_DQM      			INTERRUPT_ID_G2U_ZMIPS_DQM

#elif defined(CONFIG_BMIPS4350)

/*=====================================================================*/
/* CMIPS level 2 interrupts                                            */
/*=====================================================================*/
#define CMIPS_INT_START                    (PERIPH_EXT_INT_END)
#define CMIPS_INT_END                      (CMIPS_INT_START + 32)
#define XMIPS_INT_END                      CMIPS_INT_END

#define INTERRUPT_ID_G2U_CMIPS_MBOX0       (CMIPS_INT_START + 0) /* 72 */
#define INTERRUPT_ID_G2U_CMIPS_MBOX1       (CMIPS_INT_START + 1)
#define INTERRUPT_ID_G2U_CMIPS_MBOX2       (CMIPS_INT_START + 2)
#define INTERRUPT_ID_G2U_CMIPS_MBOX3       (CMIPS_INT_START + 3)
#define INTERRUPT_ID_G2U_CMIPS_MBOX4       (CMIPS_INT_START + 4)
#define INTERRUPT_ID_G2U_CMIPS_MBOX5       (CMIPS_INT_START + 5)
#define INTERRUPT_ID_G2U_CMIPS_MBOX6       (CMIPS_INT_START + 6)
#define INTERRUPT_ID_G2U_CMIPS_MBOX7       (CMIPS_INT_START + 7)
#define INTERRUPT_ID_G2U_CMIPS_MBOX8       (CMIPS_INT_START + 8)
#define INTERRUPT_ID_G2U_CMIPS_MBOX9       (CMIPS_INT_START + 9)
#define INTERRUPT_ID_G2U_CMIPS_MBOX10      (CMIPS_INT_START + 10)
#define INTERRUPT_ID_G2U_CMIPS_MBOX11      (CMIPS_INT_START + 11)
#define INTERRUPT_ID_G2U_CMIPS_MBOX12      (CMIPS_INT_START + 12)
#define INTERRUPT_ID_G2U_CMIPS_MBOX13      (CMIPS_INT_START + 13)
#define INTERRUPT_ID_G2U_CMIPS_MBOX14      (CMIPS_INT_START + 14)
#define INTERRUPT_ID_G2U_CMIPS_MBOX15      (CMIPS_INT_START + 15)
#define INTERRUPT_ID_G2U_CMIPS_ERROR_G2U_TIMEOUT      (CMIPS_INT_START + 16)
#define INTERRUPT_ID_G2U_CMIPS_ERROR_UB_SLAVE_TIMEOUT (CMIPS_INT_START + 17)
#define INTERRUPT_ID_G2U_CMIPS_ERROR_G2U_IRQ          (CMIPS_INT_START + 18)
#define INTERRUPT_ID_G2U_CMIPS_ERROR_UB_MASTER        (CMIPS_INT_START + 19)
#define INTERRUPT_ID_G2U_CMIPS_ERROR_GB_DQM_OVERFLOW  (CMIPS_INT_START + 20)
#define INTERRUPT_ID_G2U_CMIPS_ERROR_UB_DQM_OVERFLOW  (CMIPS_INT_START + 21)
#define INTERRUPT_ID_G2U_CMIPS_DQM                    (CMIPS_INT_START + 22)
#define INTERRUPT_ID_G2U_CMIPS_COUNTER                (CMIPS_INT_START + 23) /* 95 */

#define INTERRUPT_ID_DQM_IRQ      			INTERRUPT_ID_G2U_CMIPS_DQM
#define INTERRUPT_ID_G2U_DQM      			INTERRUPT_ID_G2U_CMIPS_DQM

#endif

/*=====================================================================*/
/* IOP ISR Numbers                                                     */
/*=====================================================================*/
#define IOP_INT_START                    (XMIPS_INT_END)
#define IOP_INT_END                      (IOP_INT_START + 32)

#define INTERRUPT_ID_UTPIRQ              (IOP_INT_START + 0)
#define INTERRUPT_ID_DTPIRQ              (IOP_INT_START + 1)
#define INTERRUPT_ID_FAP3IRQ             (IOP_INT_START + 2)
#define INTERRUPT_ID_FAP2IRQ             (IOP_INT_START + 3)
#define INTERRUPT_ID_FAP1IRQ             (IOP_INT_START + 4)
#define INTERRUPT_ID_PMCIRQ              (IOP_INT_START + 5)
#define INTERRUPT_ID_UTPIRQ_1            (IOP_INT_START + 8)
#define INTERRUPT_ID_DTPIRQ_1            (IOP_INT_START + 9)
#define INTERRUPT_ID_FAP3IRQ_1           (IOP_INT_START + 10)
#define INTERRUPT_ID_FAP2IRQ_1           (IOP_INT_START + 11)
#define INTERRUPT_ID_FAP1IRQ_1           (IOP_INT_START + 12)
#define INTERRUPT_ID_PMCIRQ_1            (IOP_INT_START + 13)

/* #define NR_IRQS	(INTERRUPT_ID_PMCIRQ_1 + 1) */

#define MIPS_CPU_IRQ_BASE 0
#define NR_IRQS	256

#if defined(CONFIG_BMIPS5000)
#define INTERRUPT_ID_UART                INTERRUPT_ID_UART1
#elif defined(CONFIG_BMIPS4350)
#define INTERRUPT_ID_UART                INTERRUPT_ID_UART2
#endif
#define INTERRUPT_ID_GFAP                INTERRUPT_ID_FAP3IRQ_1
#define INTERRUPT_ID_MSPIRQ              INTERRUPT_ID_FAP2IRQ
#define INTERRUPT_ID_USB_OHCI            INTERRUPT_ID_USBH_OHCI
#define INTERRUPT_ID_USB_EHCI            INTERRUPT_ID_USBH_EHCI

#endif /* __ASM_MACH_BRCMSTB_IRQ_H */
