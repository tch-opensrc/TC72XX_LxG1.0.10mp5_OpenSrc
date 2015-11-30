//****************************************************************************
//
// Copyright(c) 2007-2008 Broadcom Corporation
//
// This program is the proprietary software of Broadcom Corporation and/or
// its licensors, and may only be used, duplicated, modified or distributed
// pursuant to the terms and conditions of a separate, written license
// agreement executed between you and Broadcom (an "Authorized License").
// Except as set forth in an Authorized License, Broadcom grants no license
// (express or implied), right to use, or waiver of any kind with respect to
// the Software, and Broadcom expressly reserves all rights in and to the
// Software and all intellectual property rights therein.  IF YOU HAVE NO
// AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY,
// AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE
// SOFTWARE.  
//
// Except as expressly set forth in the Authorized License,
//
// 1.     This program, including its structure, sequence and organization,
// constitutes the valuable trade secrets of Broadcom, and you shall use all
// reasonable efforts to protect the confidentiality thereof, and to use this
// information only in connection with your use of Broadcom integrated circuit
// products.
//
// 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
// "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS
// OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
// RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL
// IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR
// A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET
// ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME
// THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
//
// 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM
// OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
// INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY
// RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM
// HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN
// EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1,
// WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY
// FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
//
//****************************************************************************
//  $Id$
//
//  Filename:       3384_map.h
//  Author:         Russell Enderby and much later Peter Sulc
//  Creation Date:  June 13, 2008 and 12/12/12
//
//****************************************************************************
//  Description:
//      This file defines addresses of major hardware components of 3384
//
//****************************************************************************

#ifndef __3384_MAP_PART_H
#define __3384_MAP_PART_H

#include "bcmtypes.h"

#ifdef __cplusplus
extern "C" {
#endif

// ------------------- Physical Memory Map -----------------------------------

#define PHYS_DRAM_BASE           0x00000000     // Dynamic RAM Base

// matches isb_decoder.v
#define INTC_BASE    0xb4e00000   // interrupts controller registers
#define UART_BASE0   0xb4e00500   // uart registers
#define UART_BASE1   0xb4e00520   // uart registers
#define UART_BASE2   0xb4e00540   // uart registers
#if defined(CONFIG_BMIPS5000)
#define UART_BASE    UART_BASE1
#elif defined(CONFIG_BMIPS4350)
#define UART_BASE    UART_BASE2
#endif

#define SPIM_BASE    0xb4e01000   // high speed SPI master 
#define HSSPIM_BASE  SPIM_BASE
#define EHCI_BASE    0x15400300
#define OHCI_BASE    0x15400400
#define USB_CTL_BASE 0xb5400200
#define NAND_REG_BASE               0xb4e02200  /* nand interrupt control */
#define NAND_CACHE_BASE             0xb4e02600 

typedef struct
{
    uint32 CtrlSetup;
      #define SFTRST (1 << 6)
      #define IPP    (1 << 5)
      #define IOC    (1 << 4)
      #define WABO   (1 << 3)
      #define FNBO   (1 << 2)
      #define FNHW   (1 << 1)
      #define BABO   (1 << 0)
    uint32 PllCtrl1;
    uint32 FladjVal;
    uint32 SwapCtrl;
      #define EHCI_SWAP (1 << 4)
      #define OHCI_SWAP (1 << 1)
    uint32 Spare1;
    uint32 Mdio;
    uint32 Mdio2;
    uint32 TestPortCtrl;
    uint32 SimCtrl;
    uint32 TestCtrl;
    uint32 TestMon;
    uint32 UtmiCtrl1;
    uint32 Spare2;
} UsbCtrl;

#define USB_CTL ((volatile UsbCtrl * const) USB_CTL_BASE)

// IntControlRegs TimerControl bits
#define SOFT_RESET              1

#define CP0_CMT_TPID (1<<31)

// macro to convert logical data addresses to physical
// DMA hardware must see physical address
#define LtoP( x ) ( (uint32)x & 0x1fffffff )
#define PtoL( x ) ( LtoP(x) | 0xa0000000 )


// Interrupt Controller mask and status group
typedef struct
{
    uint32  iMask;
    uint32  iStatus;
} IntMaskStatus;


#define IRQ_BITS 32 
// Interrupt Controller
typedef struct IntControl
{
    uint32          RevID;              // (00)       // Revision ID
    uint32          TimerControl;       // (10)       // Timer Control
    IntMaskStatus   DocsisIrq[3];       // (14..28)   // Docsis Interrupt Mask/Status
    uint32          Reserved0;          // (2c)
    IntMaskStatus   PeriphIrq[4];       // (30..4c)   // Internal Periph IRQ Mask/Status
    IntMaskStatus   IopIrq[2];          // (50..5c)   // IOP Interrupt Mask/Status
    uint32          DocsisIrqSense;     // (60)       // Docsis Interrupt Sense (0 = hi, 1 = lo)
    uint32          PeriphIrqSense;     // (64)       // Periph Interrupt Sense (0 = hi, 1 = lo)
    uint32          IopIrqSense;        // (68)       // IOP Interrupt Sense (0 = hi, 1 = lo)
    uint32          Ext0IrqControl;     // (6c)       // External Interrupt Configuration 0
    uint32          Ext1IrqControl;     // (70)       // External Interrupt Configuration 1
    uint32          Ext2IrqControl;     // (74)       // External Interrupt Configuration 2
    uint32          IrqOutMask;         // (78)       // Interrupt Out Mask
    uint32          DiagSelectControl;  // (7c)       // Diagnostic Select Control
    uint32          DiagReadBack;       // (80)       // Diagnostic Readback
    uint32          DiagReadBackHi;     // (84)       // Diagnostic High Readback
    uint32          DiagMiscControl;    // (88)       // Miscellaneous Control
    uint32          SoftResetBLow;      // (8c)       // Soft ResetB Register Lo
	uint32			Reserved1;			// (90)
    uint32          DSramIRQStatus;     // (94)       // Soft ResetB Register Hi
    uint32          ExtIrqMuxSelect0;   // (98)       // External IRQ Mux Select Register
	uint32          IntPeriphIRQSTATUS; // (9c)		  // Internal Periph IRQ Status Register
	uint32          IntPeriphIRQMASK; 	// (a0)		  // Internal Periph IRQ Mask Register
	uint32			Reserved2;	 		// (a4)		  // Reserved
	uint32			Reserved3;	 		// (a8)		  // Reserved
	uint32			Reserved4;	 		// (ac)		  // Reserved
	uint32			Reserved5;	 		// (b0)		  // Reserved
	uint32			Reserved6;	 		// (b4)		  // Reserved
	uint32			Reserved7;	 		// (bc)		  // Reserved
	uint32			SoftNoWatchdogResetB; // (c0)	  // Soft No Watchdog ResetB Register	
} IntControl;

#define PERF ((volatile IntControl * const) INTC_BASE)

typedef struct Uart
{
    byte    unused0;
    byte    control;
        #define BRGEN           (1<<7)
        #define TXEN            (1<<6)
        #define RXEN            (1<<5)
        #define LOOPBK          (1<<4)
        #define TXPARITYEN      (1<<3)
        #define TXPARITYEVEN    (1<<2)
        #define RXPARITYEN      (1<<1)
        #define RXPARITYEVEN    (1<<0)

    byte    config;
        #define XMITBREAK   0x40
        #define BITS5SYM    0x00
        #define BITS6SYM    0x10
        #define BITS7SYM    0x20
        #define BITS8SYM    0x30
        // 4 low bits are STOP bits/char in 1/8 bit-time intervals.  Zero
        // represents 1/8 stop bit interval.  Fifteen represents 2 stop bits.
        #define ONESTOP     0x07
        #define TWOSTOP     0x0f

    byte    fifoctl;
        #define  RSTTXFIFOS  0x80
        #define  RSTRXFIFOS  0x40
        #define  RSTTXDN     0x20
        // 5-bit TimeoutCnt is in low bits of this register.
        //  This count represents the number of character
        //  idle times before setting receive Irq when below threshold

    // When we divide SysClk/2/(1+baudword) we should get 32*bit-rate
    uint32  baudword;

    // Read-only fifo depth
    byte    txf_levl;
    byte    rxf_levl;

    // Upper 4-bits are TxThresh, Lower are RxThresh.  Irq can be asserted
    //   when rxf_level > RxThresh and/or txf_level < TxThresh
    byte    fifocfg;

    // Set value of DTR & RTS if bits are enabled to GPIO_o
    byte    prog_out;
        #define UART_DTR_OUT    0x01
        #define UART_RTS_OUT    0x02

    byte    unused1;

    // Low 4-bits, set corr bit to 1 to detect irq on rising AND falling
    // edges for corresponding GPIO_if enabled (edge insensitive)
    byte    DeltaIPEdgeNoSense;

    // Upper 4 bits: 1 for posedge sense, 0 for negedge sense if
    //   not configured for edge insensitive (see above)
    // Lower 4 bits: Mask to enable change detection IRQ for corresponding
    //  GPIO_i
    byte    DeltaIPConfig_Mask;

    // Upper 4 bits show which bits have changed (may set IRQ).
    //  read automatically clears bit
    // Lower 4 bits are actual status
    byte    DeltaIP_SyncIP;


    uint16  intMask;
    uint16  intStatus;
        #define  TXCHARDONE  (1<<15)
        #define  RXBRK       (1<<14)
        #define  RXPARERR    (1<<13)
        #define  RXFRAMERR   (1<<12)
        #define  RXFIFONE    (1<<11)
        #define  RXFIFOTHOLD (1<<10)
        #define  RXFIFOFULL  (1<< 9)
        #define  RXTIMEOUT   (1<< 8)
        #define  RXOVFERR    (1<< 7)
        #define  RXUNDERR    (1<< 6)
        #define  TXFIFOEMT   (1<< 5)
        #define  TXREADLATCH (1<< 4)
        #define  TXFIFOTHOLD (1<< 3)
        #define  TXOVFERR    (1<< 2)
        #define  TXUNDERR    (1<< 1)
        #define  DELTAIP     (1<< 0)

    uint16  unused2;

    // Write to TX, Read from RX.  Bits 11:8 are BRK,PAR,FRM errors
    uint16  Data;

} Uart;

#define UART0 ((volatile Uart * const) UART_BASE)

/*
** High-Speed SPI Controller
*/

#define	__mask(end, start)		(((1 << ((end - start) + 1)) - 1) << start)
typedef struct HsSpiControl {

  uint32	hs_spiGlobalCtrl;	// 0x0000
#define	HS_SPI_MOSI_IDLE		(1 << 18)
#define	HS_SPI_CLK_STATE_GATED		(1 << 17)
#define	HS_SPI_CLK_GATE_SSOFF		(1 << 16)
#define	HS_SPI_PLL_CLK_CTRL		(8)
#define	HS_SPI_PLL_CLK_CTRL_MASK	__mask(15, HS_SPI_PLL_CLK_CTRL)
#define	HS_SPI_SS_POLARITY		(0)
#define	HS_SPI_SS_POLARITY_MASK		__mask(7, HS_SPI_SS_POLARITY)

  uint32	hs_spiExtTrigCtrl;	// 0x0004
#define	HS_SPI_TRIG_RAW_STATE		(24)
#define	HS_SPI_TRIG_RAW_STATE_MASK	__mask(31, HS_SPI_TRIG_RAW_STATE)
#define	HS_SPI_TRIG_LATCHED		(16)
#define	HS_SPI_TRIG_LATCHED_MASK	__mask(23, HS_SPI_TRIG_LATCHED)
#define	HS_SPI_TRIG_SENSE		(8)
#define	HS_SPI_TRIG_SENSE_MASK		__mask(15, HS_SPI_TRIG_SENSE)
#define	HS_SPI_TRIG_TYPE		(0)
#define	HS_SPI_TRIG_TYPE_MASK		__mask(7, HS_SPI_TRIG_TYPE)
#define	HS_SPI_TRIG_TYPE_EDGE		(0)
#define	HS_SPI_TRIG_TYPE_LEVEL		(1)

  uint32	hs_spiIntStatus;	// 0x0008
#define	HS_SPI_IRQ_PING1_USER		(28)
#define	HS_SPI_IRQ_PING1_USER_MASK	__mask(31, HS_SPI_IRQ_PING1_USER)
#define	HS_SPI_IRQ_PING0_USER		(24)
#define	HS_SPI_IRQ_PING0_USER_MASK	__mask(27, HS_SPI_IRQ_PING0_USER)

#define	HS_SPI_IRQ_PING1_CTRL_INV	(1 << 12)
#define	HS_SPI_IRQ_PING1_POLL_TOUT	(1 << 11)
#define	HS_SPI_IRQ_PING1_TX_UNDER	(1 << 10)
#define	HS_SPI_IRQ_PING1_RX_OVER	(1 << 9)
#define	HS_SPI_IRQ_PING1_CMD_DONE	(1 << 8)

#define	HS_SPI_IRQ_PING0_CTRL_INV	(1 << 4)
#define	HS_SPI_IRQ_PING0_POLL_TOUT	(1 << 3)
#define	HS_SPI_IRQ_PING0_TX_UNDER	(1 << 2)
#define	HS_SPI_IRQ_PING0_RX_OVER	(1 << 1)
#define	HS_SPI_IRQ_PING0_CMD_DONE	(1 << 0)

  uint32	hs_spiIntStatusMasked;	// 0x000C
#define	HS_SPI_IRQSM__PING1_USER	(28)
#define	HS_SPI_IRQSM__PING1_USER_MASK	__mask(31, HS_SPI_IRQSM__PING1_USER)
#define	HS_SPI_IRQSM__PING0_USER	(24)
#define	HS_SPI_IRQSM__PING0_USER_MASK	__mask(27, HS_SPI_IRQSM__PING0_USER)

#define	HS_SPI_IRQSM__PING1_CTRL_INV	(1 << 12)
#define	HS_SPI_IRQSM__PING1_POLL_TOUT	(1 << 11)
#define	HS_SPI_IRQSM__PING1_TX_UNDER	(1 << 10)
#define	HS_SPI_IRQSM__PING1_RX_OVER	(1 << 9)
#define	HS_SPI_IRQSM__PING1_CMD_DONE	(1 << 8)

#define	HS_SPI_IRQSM__PING0_CTRL_INV	(1 << 4)
#define	HS_SPI_IRQSM__PING0_POLL_TOUT	(1 << 3)
#define	HS_SPI_IRQSM__PING0_TX_UNDER	(1 << 2)
#define	HS_SPI_IRQSM__PING0_RX_OVER	(1 << 1)
#define	HS_SPI_IRQSM__PING0_CMD_DONE	(1 << 0)

  uint32	hs_spiIntMask;		// 0x0010
#define	HS_SPI_IRQM_PING1_USER		(28)
#define	HS_SPI_IRQM_PING1_USER_MASK	__mask(31, HS_SPI_IRQM_PING1_USER)
#define	HS_SPI_IRQM_PING0_USER		(24)
#define	HS_SPI_IRQM_PING0_USER_MASK	__mask(27, HS_SPI_IRQM_PING0_USER)

#define	HS_SPI_IRQM_PING1_CTRL_INV	(1 << 12)
#define	HS_SPI_IRQM_PING1_POLL_TOUT	(1 << 11)
#define	HS_SPI_IRQM_PING1_TX_UNDER	(1 << 10)
#define	HS_SPI_IRQM_PING1_RX_OVER	(1 << 9)
#define	HS_SPI_IRQM_PING1_CMD_DONE	(1 << 8)

#define	HS_SPI_IRQM_PING0_CTRL_INV	(1 << 4)
#define	HS_SPI_IRQM_PING0_POLL_TOUT	(1 << 3)
#define	HS_SPI_IRQM_PING0_TX_UNDER	(1 << 2)
#define	HS_SPI_IRQM_PING0_RX_OVER	(1 << 1)
#define	HS_SPI_IRQM_PING0_CMD_DONE	(1 << 0)

#define HS_SPI_INTR_CLEAR_ALL       (0xFF001F1F)

  uint32	hs_spiFlashCtrl;	// 0x0014
#define	HS_SPI_FCTRL_MB_ENABLE		(1 << 23)
#define	HS_SPI_FCTRL_SS_NUM		(20)
#define	HS_SPI_FCTRL_SS_NUM_MASK	__mask(22, HS_SPI_FCTRL_SS_NUM)
#define	HS_SPI_FCTRL_PROFILE_NUM	(16)
#define	HS_SPI_FCTRL_PROFILE_NUM_MASK	__mask(18, HS_SPI_FCTRL_PROFILE_NUM)
#define	HS_SPI_FCTRL_DUMMY_BYTES	(10)
#define	HS_SPI_FCTRL_DUMMY_BYTES_MASK	__mask(11, HS_SPI_FCTRL_DUMMY_BYTES)
#define	HS_SPI_FCTRL_ADDR_BYTES		(8)
#define	HS_SPI_FCTRL_ADDR_BYTES_MASK	__mask(9, HS_SPI_FCTRL_ADDR_BYTES)
#define	HS_SPI_FCTRL_ADDR_BYTES_2	(0)
#define	HS_SPI_FCTRL_ADDR_BYTES_3	(1)
#define	HS_SPI_FCTRL_ADDR_BYTES_4	(2)
#define	HS_SPI_FCTRL_READ_OPCODE	(0)
#define	HS_SPI_FCTRL_READ_OPCODE_MASK	__mask(7, HS_SPI_FCTRL_READ_OPCODE)

  uint32	hs_spiFlashAddrBase;	// 0x0018

  char		fill0[0x80 - 0x18];

  uint32	hs_spiPP_0_Cmd;		// 0x0080
#define	HS_SPI_PP_SS_NUM		(12)
#define	HS_SPI_PP_SS_NUM_MASK		__mask(14, HS_SPI_PP_SS_NUM)
#define	HS_SPI_PP_PROFILE_NUM		(8)
#define	HS_SPI_PP_PROFILE_NUM_MASK	__mask(10, HS_SPI_PP_PROFILE_NUM)

} HsSpiControl;

typedef struct HsSpiPingPong {

	uint32 command;
#define HS_SPI_SS_NUM (12)
#define HS_SPI_PROFILE_NUM (8)
#define HS_SPI_TRIGGER_NUM (4)
#define HS_SPI_COMMAND_VALUE (0)
    #define HS_SPI_COMMAND_NOOP (0)
    #define HS_SPI_COMMAND_START_NOW (1)
    #define HS_SPI_COMMAND_START_TRIGGER (2)
    #define HS_SPI_COMMAND_HALT (3)
    #define HS_SPI_COMMAND_FLUSH (4)

	uint32 status;
#define HS_SPI_ERROR_BYTE_OFFSET (16)
#define HS_SPI_WAIT_FOR_TRIGGER (2)
#define HS_SPI_SOURCE_BUSY (1)
#define HS_SPI_SOURCE_GNT (0)

	uint32 fifo_status;
	uint32 control;

} HsSpiPingPong;

typedef struct HsSpiProfile {

	uint32 clk_ctrl;
#define HS_SPI_ACCUM_RST_ON_LOOP (15)
#define HS_SPI_SPI_CLK_2X_SEL (14)
#define HS_SPI_FREQ_CTRL_WORD (0)
	
	uint32 signal_ctrl;

	uint32 mode_ctrl;
#define HS_SPI_PREPENDBYTE_CNT (24)
#define HS_SPI_MODE_ONE_WIRE (20)
#define HS_SPI_MULTIDATA_WR_SIZE (18)
#define HS_SPI_MULTIDATA_RD_SIZE (16)
#define HS_SPI_MULTIDATA_WR_STRT (12)
#define HS_SPI_MULTIDATA_RD_STRT (8)
#define HS_SPI_FILLBYTE (0)

	uint32 polling_config;
	uint32 polling_and_mask;
	uint32 polling_compare;
	uint32 polling_timeout;
	uint32 reserved;

} HsSpiProfile;

#define HS_SPI_OP_CODE 13
    #define HS_SPI_OP_SLEEP (0)
    #define HS_SPI_OP_READ_WRITE (1)
    #define HS_SPI_OP_WRITE (2)
    #define HS_SPI_OP_READ (3)
    #define HS_SPI_OP_SETIRQ (4)

#define	HS_SPI ((volatile HsSpiControl * const) HSSPIM_BASE)
#define	HS_SPI_PINGPONG0 ((volatile HsSpiPingPong * const) (HSSPIM_BASE+0x80))
#define	HS_SPI_PINGPONG1 ((volatile HsSpiPingPong * const) (HSSPIM_BASE+0xc0))
#define	HS_SPI_PROFILES ((volatile HsSpiProfile * const) (HSSPIM_BASE+0x100))
#define	HS_SPI_FIFO0 ((volatile uint8 * const) (HSSPIM_BASE+0x200))
#define	HS_SPI_FIFO1 ((volatile uint8 * const) (HSSPIM_BASE+0x400))

/*
** NAND Controller Registers
*/
typedef struct NandCtrlRegs {
    uint32 NandRevision;            /* NAND Revision */
    uint32 NandCmdStart;            /* Nand Flash Command Start */
#define NCMD_MASK           0x1f000000
#define NCMD_LOW_LEVEL_OP   0x10000000
#define NCMD_PARAM_CHG_COL  0x0f000000
#define NCMD_PARAM_READ     0x0e000000
#define NCMD_BLK_LOCK_STS   0x0d000000
#define NCMD_BLK_UNLOCK     0x0c000000
#define NCMD_BLK_LOCK_DOWN  0x0b000000
#define NCMD_BLK_LOCK       0x0a000000
#define NCMD_FLASH_RESET    0x09000000
#define NCMD_BLOCK_ERASE    0x08000000
#define NCMD_DEV_ID_READ    0x07000000
#define NCMD_COPY_BACK      0x06000000
#define NCMD_PROGRAM_SPARE  0x05000000
#define NCMD_PROGRAM_PAGE   0x04000000
#define NCMD_STS_READ       0x03000000
#define NCMD_SPARE_READ     0x02000000
#define NCMD_PAGE_READ      0x01000000

    uint32 NandCmdExtAddr;          /* Nand Flash Command Extended Address */
    uint32 NandCmdAddr;             /* Nand Flash Command Address */
    uint32 NandCmdEndAddr;          /* Nand Flash Command End Address */
    uint32 NandNandBootConfig;      /* Nand Flash Boot Config */
#define NBC_CS_LOCK         0x80000000
#define NBC_AUTO_DEV_ID_CFG 0x40000000
#define NBC_WR_PROT_BLK0    0x10000000
#define NBC_EBI_CS7_USES_NAND (1<<15)
#define NBC_EBI_CS6_USES_NAND (1<<14)
#define NBC_EBI_CS5_USES_NAND (1<<13)
#define NBC_EBI_CS4_USES_NAND (1<<12)
#define NBC_EBI_CS3_USES_NAND (1<<11)
#define NBC_EBI_CS2_USES_NAND (1<<10)
#define NBC_EBI_CS1_USES_NAND (1<< 9)
#define NBC_EBI_CS0_USES_NAND (1<< 8)
#define NBC_EBC_CS7_SEL       (1<< 7)
#define NBC_EBC_CS6_SEL       (1<< 6)
#define NBC_EBC_CS5_SEL       (1<< 5)
#define NBC_EBC_CS4_SEL       (1<< 4)
#define NBC_EBC_CS3_SEL       (1<< 3)
#define NBC_EBC_CS2_SEL       (1<< 2)
#define NBC_EBC_CS1_SEL       (1<< 1)
#define NBC_EBC_CS0_SEL       (1<< 0)

    uint32 NandCsNandXor;           /* Nand Flash EBI CS Address XOR with */
                                    /*   1FC0 Control */
    uint32 NandReserved1;
    uint32 NandSpareAreaReadOfs0;   /* Nand Flash Spare Area Read Bytes 0-3 */
    uint32 NandSpareAreaReadOfs4;   /* Nand Flash Spare Area Read Bytes 4-7 */
    uint32 NandSpareAreaReadOfs8;   /* Nand Flash Spare Area Read Bytes 8-11 */
    uint32 NandSpareAreaReadOfsC;   /* Nand Flash Spare Area Read Bytes 12-15*/
    uint32 NandSpareAreaWriteOfs0;  /* Nand Flash Spare Area Write Bytes 0-3 */
    uint32 NandSpareAreaWriteOfs4;  /* Nand Flash Spare Area Write Bytes 4-7 */
    uint32 NandSpareAreaWriteOfs8;  /* Nand Flash Spare Area Write Bytes 8-11*/
    uint32 NandSpareAreaWriteOfsC;  /* Nand Flash Spare Area Write Bytes12-15*/
    uint32 NandAccControl;          /* Nand Flash Access Control */
#define NAC_RD_ECC_EN       0x80000000
#define NAC_WR_ECC_EN       0x40000000
#define NAC_RD_ECC_BLK0_EN  0x20000000
#define NAC_FAST_PGM_RDIN   0x10000000
#define NAC_RD_ERASED_ECC_EN 0x08000000
#define NAC_PARTIAL_PAGE_EN 0x04000000
#define NAC_WR_PREEMPT_EN   0x02000000
#define NAC_PAGE_HIT_EN     0x01000000
#define NAC_ECC_LVL_0_SHIFT 20
#define NAC_ECC_LVL_0_MASK  0x00f00000
#define NAC_ECC_LVL_SHIFT   16
#define NAC_ECC_LVL_MASK    0x000f0000
#define NAC_ECC_LVL_DISABLE 0
#define NAC_ECC_LVL_BCH_1   1
#define NAC_ECC_LVL_BCH_2   2
#define NAC_ECC_LVL_BCH_3   3
#define NAC_ECC_LVL_BCH_4   4
#define NAC_ECC_LVL_BCH_5   5
#define NAC_ECC_LVL_BCH_6   6
#define NAC_ECC_LVL_BCH_7   7
#define NAC_ECC_LVL_BCH_8   8
#define NAC_ECC_LVL_BCH_9   9
#define NAC_ECC_LVL_BCH_10  10
#define NAC_ECC_LVL_BCH_11  11
#define NAC_ECC_LVL_BCH_12  12
#define NAC_ECC_LVL_RESVD_1 13
#define NAC_ECC_LVL_RESVD_2 14
#define NAC_ECC_LVL_HAMMING 15
#define NAC_SPARE_SZ_0_SHIFT 8
#define NAC_SPARE_SZ_0_MASK 0x00003f00
#define NAC_SPARE_SZ_SHIFT  0
#define NAC_SPARE_SZ_MASK   0x0000003f
    uint32 NandReserved2;
    uint32 NandConfig;              /* Nand Flash Config */
#define NC_CONFIG_LOCK      0x80000000
#define NC_BLK_SIZE_MASK    0x70000000
#define NC_BLK_SIZE_2048K   0x60000000
#define NC_BLK_SIZE_1024K   0x50000000
#define NC_BLK_SIZE_512K    0x30000000
#define NC_BLK_SIZE_128K    0x10000000
#define NC_BLK_SIZE_16K     0x00000000
#define NC_BLK_SIZE_8K      0x20000000
#define NC_BLK_SIZE_256K    0x40000000
#define NC_DEV_SIZE_MASK    0x0f000000
#define NC_DEV_SIZE_SHIFT   24
#define NC_DEV_WIDTH_MASK   0x00800000
#define NC_DEV_WIDTH_16     0x00800000
#define NC_DEV_WIDTH_8      0x00000000
#define NC_PG_SIZE_MASK     0x00300000
#define NC_PG_SIZE_8K       0x00300000
#define NC_PG_SIZE_4K       0x00200000
#define NC_PG_SIZE_2K       0x00100000
#define NC_PG_SIZE_512B     0x00000000
#define NC_FUL_ADDR_MASK    0x00070000
#define NC_FUL_ADDR_SHIFT   16
#define NC_BLK_ADDR_MASK    0x00000700
#define NC_BLK_ADDR_SHIFT   8

    uint32 NandReserved3;
    uint32 NandTiming1;             /* Nand Flash Timing Parameters 1 */
    uint32 NandTiming2;             /* Nand Flash Timing Parameters 2 */
    uint32 NandSemaphore;           /* Semaphore */
    uint32 NandReserved4;
    uint32 NandFlashDeviceId;       /* Nand Flash Device ID */
    uint32 NandFlashDeviceIdExt;    /* Nand Flash Extended Device ID */
    uint32 NandBlockLockStatus;     /* Nand Flash Block Lock Status */
    uint32 NandIntfcStatus;         /* Nand Flash Interface Status */
#define NIS_CTLR_READY      0x80000000
#define NIS_FLASH_READY     0x40000000
#define NIS_CACHE_VALID     0x20000000
#define NIS_SPARE_VALID     0x10000000
#define NIS_FLASH_STS_MASK  0x000000ff
#define NIS_WRITE_PROTECT   0x00000080
#define NIS_DEV_READY       0x00000040
#define NIS_PGM_ERASE_ERROR 0x00000001

    uint32 NandEccCorrExtAddr;      /* ECC Correctable Error Extended Address*/
    uint32 NandEccCorrAddr;         /* ECC Correctable Error Address */
    uint32 NandEccUncExtAddr;       /* ECC Uncorrectable Error Extended Addr */
    uint32 NandEccUncAddr;          /* ECC Uncorrectable Error Address */
    uint32 NandReadErrorCount;      /* Read Error Count */
    uint32 NandCorrStatThreshold;   /* Correctable Error Reporting Threshold */
    uint32 NandOnfiStatus;          /* ONFI Status */
    uint32 NandOnfiDebugData;       /* ONFI Debug Data */
    uint32 NandFlashReadExtAddr;    /* Flash Read Data Extended Address */
    uint32 NandFlashReadAddr;       /* Flash Read Data Address */
    uint32 NandProgramPageExtAddr;  /* Page Program Extended Address */
    uint32 NandProgramPageAddr;     /* Page Program Address */
    uint32 NandCopyBackExtAddr;     /* Copy Back Extended Address */
    uint32 NandCopyBackAddr;        /* Copy Back Address */
    uint32 NandBlockEraseExtAddr;   /* Block Erase Extended Address */
    uint32 NandBlockEraseAddr;      /* Block Erase Address */
    uint32 NandInvReadExtAddr;      /* Flash Invalid Data Extended Address */
    uint32 NandInvReadAddr;         /* Flash Invalid Data Address */
    uint32 NandBlkWrProtect;        /* Block Write Protect Enable and Size */
                                    /*   for EBI_CS0b */
    uint32 NandAccControlCs1;       /* Nand Flash Access Control */
    uint32 NandConfigCs1;           /* Nand Flash Config */
    uint32 NandTiming1Cs1;          /* Nand Flash Timing Parameters 1 */
    uint32 NandTiming2Cs1;          /* Nand Flash Timing Parameters 2 */
    uint32 NandAccControlCs2;       /* Nand Flash Access Control */
    uint32 NandConfigCs2;           /* Nand Flash Config */
    uint32 NandTiming1Cs2;          /* Nand Flash Timing Parameters 1 */
    uint32 NandTiming2Cs2;          /* Nand Flash Timing Parameters 2 */
    uint32 NandSpareAreaReadOfs10;  /* Nand Flash Spare Area Read Bytes 16-19 */
    uint32 NandSpareAreaReadOfs14;  /* Nand Flash Spare Area Read Bytes 20-23 */
    uint32 NandSpareAreaReadOfs18;  /* Nand Flash Spare Area Read Bytes 24-27 */
    uint32 NandSpareAreaReadOfs1C;  /* Nand Flash Spare Area Read Bytes 28-31 */
    uint32 NandLlOpNand;            /* Flash Low Level Operation */
    uint32 NandLlRdData;            /* Nand Flash Low Level Read Data */
} NandCtrlRegs;

#define NAND ((volatile NandCtrlRegs * const) NAND_REG_BASE)
#define NAND_BBT_THRESHOLD_KB   (512 * 1024)
#define NAND_BBT_SMALL_SIZE_KB  1024
#define NAND_BBT_BIG_SIZE_KB    4096

#ifdef __cplusplus
}
#endif

#endif
