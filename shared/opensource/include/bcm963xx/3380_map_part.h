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
//  Filename:       3380_map.h
//  Author:         Russell Enderby
//  Creation Date:  June 13, 2008
//
//****************************************************************************
//  Description:
//      This file defines addresses of major hardware components of 3380
//
//****************************************************************************

#ifndef __BCM3380_MAP_PART_H
#define __BCM3380_MAP_PART_H

#include "bcmtypes.h"
#if defined(CONFIG_BCMKILAUEA)
/*
#include "ioproc.h"
#include "ioproc_blockdef.h"
*/
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ------------------- Physical Memory Map -----------------------------------

#define PHYS_DRAM_BASE           0x00000000     // Dynamic RAM Base

// matches isb_decoder.v
#define INTC_BASE   0xb4e00000   // interrupts controller registers
#define UART_BASE0  0xb4e00200   // uart registers
#define UART_BASE1  0xb4e00220   // uart registers
#define UART_BASE   UART_BASE1
#define SPIM_BASE       0xb4e02000   // serial port interface registers

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
    uint32          RevID;              // (00)       // Revision ID Register
    uint32          ClkCtrlLow;         // (04)       // Clock Control Register LOW
    uint32          ClkCtrlHigh;        // (08)       // Clock Control Register HI
    uint32          ClkCtrlUBus;        // (0c)       // UBUS Clock Control Register
    uint32          TimerControl;       // (10)       // Timer Control Register
    IntMaskStatus   DocsisIrq[3];       // (14..28)   // Docsis Interrupt Mask/Status Registers
    uint32          IntPeriphIrqStatus; // (2c)       // Internal Periph IRQ Status Register
    IntMaskStatus   PeriphIrq[4];       // (30..4c)   // Internal Periph IRQ Mask/Status Registers
    IntMaskStatus   IopIrq[2];          // (50..5c)   // IOP Interrupt Mask/Status Registers
    uint32          DocsisIrqSense;     // (60)       // Docsis Interrupt Sense Register (0 = hi, 1 = lo)
    uint32          PeriphIrqSense;     // (64)       // Periph Interrupt Sense Register (0 = hi, 1 = lo)
    uint32          IopIrqSense;        // (68)       // IOP Interrupt Sense Register    (0 = hi, 1 = lo)
    uint32          Ext0IrqControl;     // (6c)       // External Interrupt Configuration Register 0
    uint32          DiagControl;        // (70)       // Diagnostic and MBIST Control Register
    uint32          Ext1IrqControl;     // (74)       // External Interrupt Configuration Register 1
    uint32          IrqOutMask;         // (78)       // Interrupt Out Mask Register
    uint32          DiagSelectControl;  // (7c)       // Diagnostic Select Control Register
    uint32          DiagReadBack;       // (80)       // Diagnostic Readback Register
    uint32          DiagReadBackHi;     // (84)       // Diagnostic High Readback Register
    uint32          DiagMiscControl;    // (88)       // Miscellaneous Control Register
    uint32          SoftResetBLow;      // (8c)       // Soft ResetB Register Lo
    uint32          SoftResetBHigh;     // (90)       // Soft ResetB Register Hi
    uint32          SoftReset;          // (94)       // Soft Reset Register
    uint32          ExtIrqMuxSelect0;   // (98)       // External IRQ Mux Select Register
} IntControl;

#define PERF ((volatile IntControl * const) INTC_BASE)
#if defined(CONFIG_BCM_LOT1)
#define IrqStatus PeriphIrq[3].iStatus
#define IrqMask PeriphIrq[3].iMask
#else
#define IrqStatus PeriphIrq[2].iStatus
#define IrqMask PeriphIrq[2].iMask
#endif
#define pll_control TimerControl

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
** Spi Controller
*/

/* TBD.  Taken from 6358_map.h.  Need to verify for BCM6368. */
typedef struct SpiControl {
  uint16        spiMsgCtl;              /* (0x0) control byte */
#define FULL_DUPLEX_RW                  0
#define HALF_DUPLEX_W                   1
#define HALF_DUPLEX_R                   2
#define SPI_MSG_TYPE_SHIFT              14
#define SPI_BYTE_CNT_SHIFT              0
  byte          spiMsgData[0x21e];      /* (0x02 - 0x21f) msg data */
  byte          unused0[0x1e0];
  byte          spiRxDataFifo[0x220];   /* (0x400 - 0x61f) rx data */
  byte          unused1[0xe0];

  uint16        spiCmd;                 /* (0x700): SPI command */
#define SPI_CMD_NOOP                    0
#define SPI_CMD_SOFT_RESET              1
#define SPI_CMD_HARD_RESET              2
#define SPI_CMD_START_IMMEDIATE         3

#define SPI_CMD_COMMAND_SHIFT           0
#define SPI_CMD_COMMAND_MASK            0x000f

#define SPI_CMD_DEVICE_ID_SHIFT         4
#define SPI_CMD_PREPEND_BYTE_CNT_SHIFT  8
#define SPI_CMD_ONE_BYTE_SHIFT          11
#define SPI_CMD_ONE_WIRE_SHIFT          12
#define SPI_DEV_ID_0                    0
#define SPI_DEV_ID_1                    1
#define SPI_DEV_ID_2                    2
#define SPI_DEV_ID_3                    3

  byte          spiIntStatus;           /* (0x702): SPI interrupt status */
  byte          spiMaskIntStatus;       /* (0x703): SPI masked interrupt status */

  byte          spiIntMask;             /* (0x704): SPI interrupt mask */
#define SPI_INTR_CMD_DONE               0x01
#define SPI_INTR_RX_OVERFLOW            0x02
#define SPI_INTR_INTR_TX_UNDERFLOW      0x04
#define SPI_INTR_TX_OVERFLOW            0x08
#define SPI_INTR_RX_UNDERFLOW           0x10
#define SPI_INTR_CLEAR_ALL              0x1f

  byte          spiStatus;              /* (0x705): SPI status */
#define SPI_RX_EMPTY                    0x02
#define SPI_CMD_BUSY                    0x04
#define SPI_SERIAL_BUSY                 0x08

  byte          spiClkCfg;              /* (0x706): SPI clock configuration */
#define SPI_CLK_0_391MHZ                1
#define SPI_CLK_0_781MHZ                2 /* default */
#define SPI_CLK_1_563MHZ                3
#define SPI_CLK_3_125MHZ                4
#define SPI_CLK_6_250MHZ                5
#define SPI_CLK_12_50MHZ                6
#define SPI_CLK_MASK                    0x07
#define SPI_SSOFFTIME_MASK              0x38
#define SPI_SSOFFTIME_SHIFT             3
#define SPI_BYTE_SWAP                   0x80

  byte          spiFillByte;            /* (0x707): SPI fill byte */
  byte          unused2; 
  byte          spiMsgTail;             /* (0x709): msgtail */
  byte          unused3; 
  byte          spiRxTail;              /* (0x70B): rxtail */
} SpiControl;

#define SPI ((volatile SpiControl * const) SPIM_BASE)

#ifdef __cplusplus
}
#endif

#endif
