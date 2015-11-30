/*
<:copyright-gpl
 Copyright 2002 Broadcom Corp. All Rights Reserved.

 This program is free software; you can distribute it and/or modify it
 under the terms of the GNU General Public License (Version 2) as
 published by the Free Software Foundation.

 This program is distributed in the hope it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
:>
*/

#ifndef __BCM3329_MAP_H
#define __BCM3329_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bcmtypes.h"

#define PERF_BASE            0xb0000000  /* chip control registers */
#define TIMR_BASE            0xb0000040  /* timer registers */
#define GPIO_BASE            0xb0000080  /* gpio registers */
#define UART_BASE            0xb0000100  /* uart registers */
#define UART1_BASE           0xb0000120  /* uart registers */
#define NAND_REG_BASE        0xb0000200  /* NAND registers */
#define SPI_BASE             0xb0000800  /* SPI master controller registers */
#define HSSPIM_BASE          0xb0001000  /* High-Speed SPI registers */
#define MISC_BASE            0xb0001800  /* Miscellaneous Registers */
#define MPI_BASE             0xb00020A0  /* MPI control registers */
#define USB_CTL_BASE         0xb0002400  /* USB 2.0 device control registers */
#define USB_EHCI_BASE        0x10002500  /* USB host registers */
#define USB_OHCI_BASE        0x10002600  /* USB host registers */
#define USBH_CFG_BASE        0xb0002700
#define DDR_BASE             0xb0003000  /* Memory control registers */
#define GPON_SERDES_BASE     0xb0004800  /* GPON SERDES Interface Registers */
#define APM_HVG_BASE         0xb0008300  /* High-Voltage Generation */
#define APM_HVG_BASE_REG_15  0xb0008408  /* High-Voltage Generation Register 15 */
#define APM_HVG_BASE_REG_19  0xb0008488  /* High-Voltage Generation Register 19 */
#define PCIE_BASE            0xb0e40000  /* PCIE registers */

typedef struct DDRPhyControl {
    uint32 REVISION;               /* 0x00 */
    uint32 CLK_PM_CTRL;            /* 0x04 */
    uint32 unused0[2];             /* 0x08-0x10 */
    uint32 PLL_STATUS;             /* 0x10 */
    uint32 PLL_CONFIG;             /* 0x14 */
    uint32 PLL_PRE_DIVIDER;        /* 0x18 */
    uint32 PLL_DIVIDER;            /* 0x1c */
    uint32 PLL_CONTROL1;           /* 0x20 */
    uint32 PLL_CONTROL2;           /* 0x24 */
    uint32 PLL_SS_EN;              /* 0x28 */
    uint32 PLL_SS_CFG;             /* 0x2c */
    uint32 STATIC_VDL_OVERRIDE;    /* 0x30 */
    uint32 DYNAMIC_VDL_OVERRIDE;   /* 0x34 */
    uint32 IDLE_PAD_CONTROL;       /* 0x38 */
    uint32 ZQ_PVT_COMP_CTL;        /* 0x3c */
    uint32 DRIVE_PAD_CTL;          /* 0x40 */
    uint32 CLOCK_REG_CONTROL;      /* 0x44 */
    uint32 unused1[46];
} DDRPhyControl;

typedef struct DDRPhyByteLaneControl {
    uint32 REVISION;                /* 0x00 */
    uint32 VDL_CALIBRATE;           /* 0x04 */
    uint32 VDL_STATUS;              /* 0x08 */
    uint32 unused;                  /* 0x0c */
    uint32 VDL_OVERRIDE_0;          /* 0x10 */
    uint32 VDL_OVERRIDE_1;          /* 0x14 */
    uint32 VDL_OVERRIDE_2;          /* 0x18 */
    uint32 VDL_OVERRIDE_3;          /* 0x1c */
    uint32 VDL_OVERRIDE_4;          /* 0x20 */
    uint32 VDL_OVERRIDE_5;          /* 0x24 */
    uint32 VDL_OVERRIDE_6;          /* 0x28 */
    uint32 VDL_OVERRIDE_7;          /* 0x2c */
    uint32 READ_CONTROL;            /* 0x30 */
    uint32 READ_FIFO_STATUS;        /* 0x34 */
    uint32 READ_FIFO_CLEAR;         /* 0x38 */
    uint32 IDLE_PAD_CONTROL;        /* 0x3c */
    uint32 DRIVE_PAD_CTL;           /* 0x40 */
    uint32 CLOCK_PAD_DISABLE;       /* 0x44 */
    uint32 WR_PREAMBLE_MODE;        /* 0x48 */
    uint32 CLOCK_REG_CONTROL;       /* 0x4C */
    uint32 unused0[44];
} DDRPhyByteLaneControl;

typedef struct DDRControl {
    uint32 CNFG;                            /* 0x000 */
    uint32 CSST;                            /* 0x004 */
    uint32 CSEND;                           /* 0x008 */
    uint32 unused;                          /* 0x00c */
    uint32 ROW00_0;                         /* 0x010 */
    uint32 ROW00_1;                         /* 0x014 */
    uint32 ROW01_0;                         /* 0x018 */
    uint32 ROW01_1;                         /* 0x01c */
    uint32 unused0[4];
    uint32 ROW20_0;                         /* 0x030 */
    uint32 ROW20_1;                         /* 0x034 */
    uint32 ROW21_0;                         /* 0x038 */
    uint32 ROW21_1;                         /* 0x03c */
    uint32 unused1[4];
    uint32 COL00_0;                         /* 0x050 */
    uint32 COL00_1;                         /* 0x054 */
    uint32 COL01_0;                         /* 0x058 */
    uint32 COL01_1;                         /* 0x05c */
    uint32 unused2[4];
    uint32 COL20_0;                         /* 0x070 */
    uint32 COL20_1;                         /* 0x074 */
    uint32 COL21_0;                         /* 0x078 */
    uint32 COL21_1;                         /* 0x07c */
    uint32 unused3[4];
    uint32 BNK10;                           /* 0x090 */
    uint32 BNK32;                           /* 0x094 */
    uint32 unused4[26];
    uint32 DCMD;                            /* 0x100 */
#define DCMD_CS1          (1 << 5)
#define DCMD_CS0          (1 << 4)
#define DCMD_SET_SREF     4
    uint32 DMODE_0;                         /* 0x104 */
    uint32 DMODE_1;                         /* 0x108 */
#define DMODE_1_DRAMSLEEP (1 << 11)
    uint32 CLKS;                            /* 0x10c */
    uint32 ODT;                             /* 0x110 */
    uint32 TIM1_0;                          /* 0x114 */
    uint32 TIM1_1;                          /* 0x118 */
    uint32 TIM2;                            /* 0x11c */
    uint32 CTL_CRC;                         /* 0x120 */
    uint32 DOUT_CRC;                        /* 0x124 */
    uint32 DIN_CRC;                         /* 0x128 */
    uint32 unused5[53];

    DDRPhyControl           PhyControl;             /* 0x200 */
    DDRPhyByteLaneControl   PhyByteLane0Control;    /* 0x300 */
    DDRPhyByteLaneControl   PhyByteLane1Control;    /* 0x400 */
    DDRPhyByteLaneControl   PhyByteLane2Control;    /* 0x500 */
    DDRPhyByteLaneControl   PhyByteLane3Control;    /* 0x600 */
    uint32 unused6[64];

    uint32 GCFG;                            /* 0x800 */
    uint32 LBIST_CFG;                       /* 0x804 */
    uint32 LBIST_SEED;                      /* 0x808 */
    uint32 ARB;                             /* 0x80c */
    uint32 PI_GCF;                          /* 0x810 */
    uint32 PI_UBUS_CTL;                     /* 0x814 */
    uint32 PI_MIPS_CTL;                     /* 0x818 */
    uint32 PI_DSL_MIPS_CTL;                 /* 0x81c */
    uint32 PI_DSL_PHY_CTL;                  /* 0x820 */
    uint32 PI_UBUS_ST;                      /* 0x824 */
    uint32 PI_MIPS_ST;                      /* 0x828 */
    uint32 PI_DSL_MIPS_ST;                  /* 0x82c */
    uint32 PI_DSL_PHY_ST;                   /* 0x830 */
    uint32 PI_UBUS_SMPL;                    /* 0x834 */
    uint32 TESTMODE;                        /* 0x838 */
    uint32 TEST_CFG1;                       /* 0x83c */
    uint32 TEST_PAT;                        /* 0x840 */
    uint32 TEST_COUNT;                      /* 0x844 */
    uint32 TEST_CURR_COUNT;                 /* 0x848 */
    uint32 TEST_ADDR_UPDT;                  /* 0x84c */
    uint32 TEST_ADDR;                       /* 0x850 */
    uint32 TEST_DATA0;                      /* 0x854 */
    uint32 TEST_DATA1;                      /* 0x858 */
    uint32 TEST_DATA2;                      /* 0x85c */
    uint32 TEST_DATA3;                      /* 0x860 */
} DDRControl;

#define DDR ((volatile DDRControl * const) DDR_BASE)

/*
** Peripheral Controller
*/

#define IRQ_BITS 64
typedef struct  {
    uint64		  IrqMask;
    uint64		  IrqStatus;
} IrqControl_t;

typedef struct PerfControl {
    uint32        RevID;             /* (00) word 0 */
    uint32        blkEnables;        /* (04) word 1 */
#define ACP_A_CLK_EN     (1 << 25)
#define ACP_B_CLK_EN     (1 << 24)
#define NTP_CLK_EN       (1 << 23)
#define PCM_CLK_EN       (1 << 22)
#define BMU_CLK_EN       (1 << 21)
#define PCIE_CLK_EN      (1 << 20)
#define GPON_SER_CLK_EN  (1 << 19)
#define IPSEC_CLK_EN     (1 << 18)
#define NAND_CLK_EN      (1 << 17)
#define DISABLE_GLESS    (1 << 16)
#define USBH_CLK_EN      (1 << 15)
#define APM_CLK_EN       (1 << 14)
#define ROBOSW_CLK_EN    (1 << 12)
#define USBD_CLK_EN      (1 << 10)
#define SPI_CLK_EN       (1 << 9)
#define SWPKT_GPON_CLK_EN (1 << 8)
#define SWPKT_USB_CLK_EN (1 << 7)
#define GPON_CLK_EN      (1 << 6)

    uint32        pll_control;       /* (08) word 2 */
#define SOFT_RESET              0x00000001      // 0

    uint32        deviceTimeoutEn;   /* (0c) word 3 */
    uint32        softResetB;        /* (10) word 4 */
#define SOFT_RST_SERDES_DIG     (1 << 23)
#define SOFT_RST_SERDES         (1 << 22)
#define SOFT_RST_SERDES_MDIO    (1 << 21)
#define SOFT_RST_SERDES_PLL     (1 << 20)
#define SOFT_RST_SERDES_HW      (1 << 19)
#define SOFT_RST_GPON           (1 << 18)
#define SOFT_RST_BMU            (1 << 17)
#define SOFT_RST_HVG            (1 << 16)
#define SOFT_RST_APM            (1 << 15)
#define SOFT_RST_ACP            (1 << 14)
#define SOFT_RST_PCM            (1 << 13)
#define SOFT_RST_USBH           (1 << 12)
#define SOFT_RST_USBD           (1 << 11)
#define SOFT_RST_SWITCH         (1 << 10)
#define SOFT_RST_MOCA_CPU       (1 << 9)
#define SOFT_RST_MOCA_SYS       (1 << 8)
#define SOFT_RST_MOCA           (1 << 7)
#define SOFT_RST_EPHY           (1 << 6)
#define SOFT_RST_PCIE           (1 << 5)
#define SOFT_RST_IPSEC          (1 << 4)
#define SOFT_RST_MPI            (1 << 3)
#define SOFT_RST_PCIE_EXT       (1 << 2)
#define SOFT_RST_PCIE_CORE      (1 << 1)
#define SOFT_RST_SPI            (1 << 0)

    uint32        diagControl;        /* (14) word 5 */
    uint32        ExtIrqCfg;          /* (18) word 6*/
    uint32        ExtIrqCfg1;         /* (1c) word 7 */
#define EI_SENSE_SHFT   0
#define EI_STATUS_SHFT  4
#define EI_CLEAR_SHFT   8
#define EI_MASK_SHFT    12
#define EI_INSENS_SHFT  16
#define EI_LEVEL_SHFT   20

     IrqControl_t IrqControl[2];
} PerfControl;

#define PERF ((volatile PerfControl * const) PERF_BASE)

/*
** Timer
*/
typedef struct Timer {
    uint16        unused0;
    byte          TimerMask;
#define TIMER0EN        0x01
#define TIMER1EN        0x02
#define TIMER2EN        0x04
    byte          TimerInts;
#define TIMER0          0x01
#define TIMER1          0x02
#define TIMER2          0x04
#define WATCHDOG        0x08
    uint32        TimerCtl0;
    uint32        TimerCtl1;
    uint32        TimerCtl2;
#define TIMERENABLE     0x80000000
#define RSTCNTCLR       0x40000000
    uint32        TimerCnt0;
    uint32        TimerCnt1;
    uint32        TimerCnt2;
    uint32        WatchDogDefCount;

    /* Write 0xff00 0x00ff to Start timer
     * Write 0xee00 0x00ee to Stop and re-load default count
     * Read from this register returns current watch dog count
     */
    uint32        WatchDogCtl;

    /* Number of 50-MHz ticks for WD Reset pulse to last */
    uint32        WDResetCount;
} Timer;

#define TIMER ((volatile Timer * const) TIMR_BASE)

/*
** UART
*/
typedef struct UartChannel {
    byte          unused0;
    byte          control;
#define BRGEN           0x80    /* Control register bit defs */
#define TXEN            0x40
#define RXEN            0x20
#define LOOPBK          0x10
#define TXPARITYEN      0x08
#define TXPARITYEVEN    0x04
#define RXPARITYEN      0x02
#define RXPARITYEVEN    0x01

    byte          config;
#define XMITBREAK       0x40
#define BITS5SYM        0x00
#define BITS6SYM        0x10
#define BITS7SYM        0x20
#define BITS8SYM        0x30
#define ONESTOP         0x07
#define TWOSTOP         0x0f
    /* 4-LSBS represent STOP bits/char
     * in 1/8 bit-time intervals.  Zero
     * represents 1/8 stop bit interval.
     * Fifteen represents 2 stop bits.
     */
    byte          fifoctl;
#define RSTTXFIFOS      0x80
#define RSTRXFIFOS      0x40
    /* 5-bit TimeoutCnt is in low bits of this register.
     *  This count represents the number of characters
     *  idle times before setting receive Irq when below threshold
     */
    uint32        baudword;
    /* When divide SysClk/2/(1+baudword) we should get 32*bit-rate
     */

    byte          txf_levl;       /* Read-only fifo depth */
    byte          rxf_levl;       /* Read-only fifo depth */
    byte          fifocfg;        /* Upper 4-bits are TxThresh, Lower are
                                   *      RxThreshold.  Irq can be asserted
                                   *      when rx fifo> thresh, txfifo<thresh
                                   */
    byte          prog_out;       /* Set value of DTR (Bit0), RTS (Bit1)
                                   *  if these bits are also enabled to GPIO_o
                                   */
#define DTREN   0x01
#define RTSEN   0x02

    byte          unused1;
    byte          DeltaIPEdgeNoSense;     /* Low 4-bits, set corr bit to 1 to
                                           * detect irq on rising AND falling
                                           * edges for corresponding GPIO_i
                                           * if enabled (edge insensitive)
                                           */
    byte          DeltaIPConfig_Mask;     /* Upper 4 bits: 1 for posedge sense
                                           *      0 for negedge sense if
                                           *      not configured for edge
                                           *      insensitive (see above)
                                           * Lower 4 bits: Mask to enable change
                                           *  detection IRQ for corresponding
                                           *  GPIO_i
                                           */
    byte          DeltaIP_SyncIP;         /* Upper 4 bits show which bits
                                           *  have changed (may set IRQ).
                                           *  read automatically clears bit
                                           * Lower 4 bits are actual status
                                           */

    uint16        intMask;                /* Same Bit defs for Mask and status */
    uint16        intStatus;
#define DELTAIP         0x0001
#define TXUNDERR        0x0002
#define TXOVFERR        0x0004
#define TXFIFOTHOLD     0x0008
#define TXREADLATCH     0x0010
#define TXFIFOEMT       0x0020
#define RXUNDERR        0x0040
#define RXOVFERR        0x0080
#define RXTIMEOUT       0x0100
#define RXFIFOFULL      0x0200
#define RXFIFOTHOLD     0x0400
#define RXFIFONE        0x0800
#define RXFRAMERR       0x1000
#define RXPARERR        0x2000
#define RXBRK           0x4000

    uint16        unused2;
    uint16        Data;                   /* Write to TX, Read from RX */
                                          /* bits 11:8 are BRK,PAR,FRM errors */

    uint32        unused3;
    uint32        unused4;
} Uart;

#define UART ((volatile Uart * const) UART_BASE)

/*
** Gpio Controller
*/

typedef struct GpioControl {
    uint64      GPIODir;                    /* 0 */
    uint64      GPIOio;                     /* 8 */
#define GPIO_MoCA_OVERLAY_UART_WRITE  39
#define GPIO_MoCA_OVERLAY_UART_READ   38
    uint32      LEDCtrl;
#define LED_ALL_STROBE          0x0f000000
#define LED3_STROBE             0x08000000
#define LED2_STROBE             0x04000000
#define LED1_STROBE             0x02000000
#define LED0_STROBE             0x01000000
#define LED_TEST                0x00010000
#define DISABLE_LINK_ACT_ALL    0x0000f000
#define DISABLE_LINK_ACT_3      0x00008000
#define DISABLE_LINK_ACT_2      0x00004000
#define DISABLE_LINK_ACT_1      0x00002000
#define DISABLE_LINK_ACT_0      0x00001000
#define LED_INTERVAL_SET_MASK   0x00000f00
#define LED_INTERVAL_SET_1280MS 0x00000700
#define LED_INTERVAL_SET_640MS  0x00000600
#define LED_INTERVAL_SET_320MS  0x00000500
#define LED_INTERVAL_SET_160MS  0x00000400
#define LED_INTERVAL_SET_80MS   0x00000300
#define LED_INTERVAL_SET_40MS   0x00000200
#define LED_INTERVAL_SET_20MS   0x00000100
#define LED_ON_ALL              0x000000f0
#define LED_ON_3                0x00000080
#define LED_ON_2                0x00000040
#define LED_ON_1                0x00000020
#define LED_ON_0                0x00000010
#define LED_ENABLE_ALL          0x0000000f
#define LED_ENABLE_3            0x00000008
#define LED_ENABLE_2            0x00000004
#define LED_ENABLE_1            0x00000002
#define LED_ENABLE_0            0x00000001
    uint32      SpiSlaveCfg;                /* 14 */
    uint32      GPIOMode;                   /* 18 */
#define GPIO_MODE_SPI_SSN5          (1<<31)
#define GPIO_MODE_SPI_SSN4          (1<<30)
#define GPIO_MODE_SPI_SSN3          (1<<29)
#define GPIO_MODE_SPI_SSN2          (1<<28)
#define GPIO_MODE_EBI_CS3           (1<<27)
#define GPIO_MODE_EBI_CS2           (1<<26)
#define GPIO_MODE_APM_CLK           (1<<25)
#define GPIO_MODE_APM_SDIN          (1<<24)
#define GPIO_MODE_APM_SDOUT         (1<<23)
#define GPIO_MODE_APM_FRAME_SYNC    (1<<22)
#define GPIO_MODE_PCI_GNT0          (1<<20)
#define GPIO_MODE_PCI_REQ0          (1<<19)
#define GPIO_MODE_PCI_INTB          (1<<18)
#define GPIO_MODE_PCI_GNT1          (1<<17)
#define GPIO_MODE_PCI_REQ1          (1<<16)
#define GPIO_MODE_NTR_PULSE         (1<<15)
#define GPIO_MODE_USBD_LED          (1<<14)
#define GPIO_MODE_ROBOSW_LED1       (1<<13)
#define GPIO_MODE_ROBOSW_LED0       (1<<12)
#define GPIO_MODE_ROBOSW_LED_CLK    (1<<11)
#define GPIO_MODE_ROBOSW_LED_DATA   (1<<10)
#define GPIO_MODE_GPON_LED          (1<<8)
#define GPIO_MODE_GPHY1_LED         (1<<7)
#define GPIO_MODE_GPHY0_LED         (1<<6)
#define GPIO_MODE_MOCA_LED          (1<<5)
#define GPIO_MODE_SERIAL_LED_CLK    (1<<4)
#define GPIO_MODE_SERIAL_LED_DATA   (1<<3)
#define GPIO_MODE_SYS_IRQ           (1<<2)
#define GPIO_MODE_GPON_TX_APC_FAIL  (1<<1) /*Anticipating B0*/
#define GPIO_MODE_GPON_TX_EN_L      (1<<0)

    uint32      VregConfig;                 /* 1C */
    uint32      AuxLedInterval;             /* 20 */
#define AUX_LED_IN_7            0x80000000
#define AUX_LED_IN_6            0x40000000
#define AUX_LED_IN_5            0x20000000
#define AUX_LED_IN_4            0x10000000
#define AUX_LED_IN_MASK         0xf0000000
#define LED_IN_3                0x08000000
#define LED_IN_2                0x04000000
#define LED_IN_1                0x02000000
#define LED_IN_0                0x01000000
#define AUX_LED_TEST            0x00400000
#define USE_NEW_INTV            0x00200000
#define LED7_LNK_ORAND          0x00100000
#define LED7_LNK_MASK           0x000f0000
#define LED7_LNK_MASK_SHFT      16
#define LED7_ACT_MASK           0x0000f000
#define LED7_ACT_MASK_SHFT      12
#define AUX_FLASH_INTV          0x00000fc0
#define AUX_FLASH_INTV_100MS    0x00000140
#define AUX_FLASH_INTV_SHFT     6
#define AUX_BLINK_INTV          0x0000003f
#define AUX_BLINK_INTV_60MS     0x00000003
    uint32      AuxLedCtrl;                 /* 24 */
#define AUX_HW_DISAB_7          0x80000000
#define AUX_STROBE_7            0x40000000
#define AUX_MODE_7              0x30000000
#define AUX_MODE_SHFT_7         28
#define AUX_HW_DISAB_6          0x08000000
#define AUX_STROBE_6            0x04000000
#define AUX_MODE_6              0x03000000
#define AUX_MODE_SHFT_6         24
#define AUX_HW_DISAB_5          0x00800000
#define AUX_STROBE_5            0x00400000
#define AUX_MODE_5              0x00300000
#define AUX_MODE_SHFT_5         20
#define AUX_HW_DISAB_4          0x00080000
#define AUX_STROBE_4            0x00040000
#define AUX_MODE_4              0x00030000
#define AUX_MODE_SHFT_4         16
#define AUX_HW_DISAB_3          0x00008000
#define AUX_STROBE_3            0x00004000
#define AUX_MODE_3              0x00003000
#define AUX_MODE_SHFT_3         12
#define AUX_HW_DISAB_2          0x00000800
#define AUX_STROBE_2            0x00000400
#define AUX_MODE_2              0x00000300
#define AUX_MODE_SHFT_2         8
#define AUX_HW_DISAB_1          0x00000080
#define AUX_STROBE_1            0x00000040
#define AUX_MODE_1              0x00000030
#define AUX_MODE_SHFT_1         4
#define AUX_HW_DISAB_0          0x00000008
#define AUX_STROBE_0            0x00000004
#define AUX_MODE_0              0x00000003
#define AUX_MODE_SHFT_0         0

#define LED_STEADY_OFF          0x0
#define LED_FLASH               0x1
#define LED_BLINK               0x2
#define LED_STEADY_ON           0x3

    uint32      TestControl;                /* 28 */

    uint32      OscControl;                 /* 2C */
    uint32      RoboSWLEDControl;           /* 30 */
    uint32      RoboSWLEDLSR;               /* 34 */
    uint32      GPIOBaseMode;               /* 38 */
#define EN_GMII2                (1<<17)
#define EN_GMII1                (1<<16)
    uint32      RoboswEphyCtrl;             /* 3C */
#define RSW_HW_FWDG_EN          (1<<19)
#define RSW_MII_DUMB_FWDG_EN    (1<<16)
#define EPHY_RST_4              (1<<9)
#define EPHY_RST_3              (1<<8)
#define EPHY_RST_2              (1<<7)
#define EPHY_RST_1              (1<<6)
#define EPHY_PWR_DOWN_4         (1<<5)
#define EPHY_PWR_DOWN_3         (1<<4)
#define EPHY_PWR_DOWN_2         (1<<3)
#define EPHY_PWR_DOWN_1         (1<<2)
    uint32      unused1[2];                 /* 40 */
    uint32      RingOscCtrl0;               /* 48 */
    uint32      RingOscCtrl1;               /* 4C */

    uint32      SerialLed;                  /* 50 */
    uint32      SerialLedCtrl;              /* 54 */
#define SER_LED_BUSY            (1<<3)
#define SER_LED_POLARITY        (1<<2)
#define SER_LED_DIV_1           0
#define SER_LED_DIV_2           1
#define SER_LED_DIV_4           2
#define SER_LED_DIV_8           3
#define SER_LED_DIV_MASK        0x3
#define SER_LED_DIV_SHIFT       0
    uint32    SerialLedBlink;               /* 58 */
    uint32    SerdesCtl;                    /* 5c */
#define SERDES_PCIE_ENABLE  0x00000001
#define SERDES_PCIE_EXD_ENABLE  (1<<15)
    uint32    SerdesStatus;                 /* 60 */
    uint32    unused2;                      /* 64 */
    uint32    DieRevID;                     /* 68 */
    uint32    DiagMemStatus;                /* 6c */
    uint32    DiagSelControl;               /* 70 */
    uint32    DiagReadBack;                 /* 74 */
    uint32    DiagReadBackHi;               /* 78 */
    uint32    DiagMiscControl;              /* 7c */
#define EPHY_SA_RESET_N           0x00000300
#define EPHY_SA_TESTEN            0x00000500
#define EPHY_SA_CLOCK_RESET       0x0000d900
} GpioControl;

#define GPIO ((volatile GpioControl * const) GPIO_BASE)

/* Number to mask conversion macro used for GPIODir and GPIOio */
#define GPIO_NUM_MAX                    40
#define GPIO_NUM_TO_MASK(X)             ( (((X) & BP_GPIO_NUM_MASK) < GPIO_NUM_MAX) ? ((uint64)1 << ((X) & BP_GPIO_NUM_MASK)) : (0) )

/*
** Spi Controller
*/

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


#define SPI ((volatile SpiControl * const) SPI_BASE)


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
** Periph - Misc Register Set Definitions.
*/

typedef struct Misc {
  uint32        miscMoCADiv ;              /* (0x0) MoCA Ref PLL Div */
#define MISC_MOCA_DIV_REF_DIV_FB_MASK                              0xFF000000
#define MISC_MOCA_DIV_REF_DIV_FB_SHIFT                             24
#define MISC_MOCA_DIV_REF_OUTDIV_M1_MASK                           0x00FF0000
#define MISC_MOCA_DIV_REF_OUTDIV_M1_SHIFT                          16
#define MISC_MOCA_DIV_REF_OUTDIV_M4_MASK                           0x0000FF00
#define MISC_MOCA_DIV_REF_OUTDIV_M4_SHIFT                          8
#define MISC_MOCA_DIV_REF_OUTDIV_M5_MASK                           0x000000FF
#define MISC_MOCA_DIV_REF_OUTDIV_M5_SHIFT                          0
  uint32        miscMoCACtl ;              /* (0x4) MoCA Ref PLL Ctl */
#define MISC_MOCA_CTL_REF_OUTCML_LOWCUR_MASK                       0x001FC000
#define MISC_MOCA_CTL_REF_OUTCML_LOWCUR_SHIFT                      14
#define MISC_MOCA_CTL_REF_CLFCNT_MASK                              0x00003000
#define MISC_MOCA_CTL_REF_CLFCNT_SHIFT                             12
#define MISC_MOCA_CTL_REF_QP_ICTRL_MASK                            0x00000E00
#define MISC_MOCA_CTL_REF_QP_ICTRL_SHIFT                           9
#define MISC_MOCA_CTL_REF_VCOBUF_LATCH_ON                          0x00000100
#define MISC_MOCA_CTL_REF_LF_RCNTL_MASK                            0x000000E0
#define MISC_MOCA_CTL_REF_LF_RCNTL_SHIFT                           5
#define MISC_MOCA_CTL_REF_MUX_FBOFF                                0x00000010
#define MISC_MOCA_CTL_REF_MUX_SEL_MASK                             0x0000000C
#define MISC_MOCA_CTL_REF_MUX_SEL_SHIFT                            2
#define MISC_MOCA_CTL_REF_REF_CMLBIAS_MASK                         0x00000003
#define MISC_MOCA_CTL_REF_REF_CMLBIAS_SHIFT                        0
  uint32        miscMoCAPwr ;              /* (0x8) MoCA Ref PLL Pwr */
#define MISC_MOCA_PWR_REF_DEEP_PWRDN                               0x01000000
#define MISC_MOCA_PWR_REF_OUTDIV_PWRDN_M_MASK                      0x00FE0000
#define MISC_MOCA_PWR_REF_OUTDIV_PWRDN_M_SHIFT                     17
#define MISC_MOCA_PWR_REF_OUTCML_PWRDN_M_MASK                      0x0001FC00
#define MISC_MOCA_PWR_REF_OUTCML_PWRDN_M_SHIFT                     10
#define MISC_MOCA_PWR_REF_OUTCMOS_PWRDN_M_MASK                     0x000003F8
#define MISC_MOCA_PWR_REF_OUTCMOS_PWRDN_M_SHIFT                    3
#define MISC_MOCA_PWR_REF_UGB_PWRDN                                0x00000004
#define MISC_MOCA_PWR_REF_MUX_PWRDN                                0x00000002
#define MISC_MOCA_PWR_REF_VCO_PWRDN                                0x00000001
  uint32        miscMoCARst ;              /* (0xC) MoCA Ref PLL Rst */
#define MISC_MOCA_RST_REF_OUTDIV_RESET_M_MASK                      0x00000FE0
#define MISC_MOCA_RST_REF_OUTDIV_RESET_M_SHIFT                     5
#define MISC_MOCA_RST_REF_DIV2RST                                  0x00000010
#define MISC_MOCA_RST_REF_MDIV2RST                                 0x00000008
#define MISC_MOCA_RST_REF_FBDIVRST                                 0x00000004
#define MISC_MOCA_RST_REF_LD_RESET_STRT                            0x00000002
#define MISC_MOCA_RST_REF_VCRST                                    0x00000001
  uint32        miscMemcControl ;          /* (0x10) MEMC Control     */
#define MISC_MEMC_CONTROL_MC_UBUS_ASYNC_MODE                       0x00000008
#define MISC_MEMC_CONTROL_MC_LMB_ASYNC_MODE                        0x00000004
#define MISC_MEMC_CONTROL_DDR_TEST_DONE                            0x00000002
#define MISC_MEMC_CONTROL_DDR_TEST_DISABLE                         0x00000001
  uint32        miscStrapBus ;             /* (0x14) Strap Register   */
#define MISC_STRAP_BUS_MIPS_PLL_FVCO_MASK                          0xF8000000
#define MISC_STRAP_BUS_MIPS_PLL_FVCO_SHIFT                         27
#define MISC_STRAP_BUS_HRD_RST_DELAY                               0x04000000
#define MISC_STRAP_BUS_ALT_BFC_EN                                  0x02000000
#define MISC_STRAP_BUS_MOCA_STANDALONE_B                           0x01000000
#define MISC_STRAP_BUS_MOCA_CONFIG_RATIO                           0x00800000
#define MISC_STRAP_BUS_IXTAL_ADJ_MASK                              0x00600000
#define MISC_STRAP_BUS_IXTAL_ADJ_SHIFT                             21
#define MISC_STRAP_BUS_BYPASS_XTAL                                 0x00100000
#define MISC_STRAP_BUS_TS                                          0x00080000
#define MISC_STRAP_BUS_APM_PICO_BOOT_ROM                           0x00040000
#define MISC_STRAP_BUS_TA                                          0x00020000
#define MISC_STRAP_BUS_ROBOSW_2_MODE_MASK                          0x00018000
#define MISC_STRAP_BUS_ROBOSW_2_MODE_SHIFT                         15
#define MISC_STRAP_BUS_ROBOSW_1_MODE_MASK                          0x00006000
#define MISC_STRAP_BUS_ROBOSW_1_MODE_SHIFT                         13
#define MISC_STRAP_BUS_BIST_CLRMEM_N                               0x00001000
#define MISC_STRAP_BUS_PLL_MIPS_WAIT_FAST_N                        0x00000200
#define MISC_STRAP_BUS_PLL_USE_LOCK                                0x00000100
#define MISC_STRAP_BUS_PCIE_ROOT_COMPLEX                           0x00000080
#define MISC_STRAP_BUS_LS_SPIM_ENABLED                             0x00000040
#define MISC_STRAP_BUS_USE_SPI_MASTER                              0x00000020
#define MISC_STRAP_BUS_SPI_CLK_FAST                                0x00000010
#define MISC_STRAP_BUS_SPI_BOOT_DELAY                              0x00000008
#define MISC_STRAP_BUS_MIPS_BOOT16                                 0x00000004
#define MISC_STRAP_BUS_BOOT_SEL_MASK                               0x00000003
#define MISC_STRAP_BUS_BOOT_SEL_SHIFT                              0
#define MISC_STRAP_BUS_BOOT_PARALLEL                               0x03
#define MISC_STRAP_BUS_BOOT_SERIAL                                 0x01
#define MISC_STRAP_BUS_BOOT_NAND                                   0x00
  uint32        miscStrapOverride ;        /* (0x18) Strap Override Reg */
#define MISC_STRAP_OVERRIDE_INT_MPI_ARB                            0x00000008
#define MISC_STRAP_OVERRIDE_INT_MPI_CLK                            0x00000004
#define MISC_STRAP_OVERRIDE_INT_HOST                               0x00000002
#define MISC_STRAP_OVERRIDE_STRAP_OVERRIDE                         0x00000001
  uint32        miscMoCAClkStrapBus ;      /* (0x1C) MoCA Clock Strap Reg */
#define MISC_MOCA_CLK_STRAP_BUS_MDIV_10_OVERRIDE_MASK              0x0000FF00
#define MISC_MOCA_CLK_STRAP_BUS_MDIV_10_OVERRIDE_SHIFT             8
#define MISC_MOCA_CLK_STRAP_BUS_MDIV_9_OVERRIDE_MASK               0x000000FF
#define MISC_MOCA_CLK_STRAP_BUS_MDIV_9_OVERRIDE_SHIFT              0
  uint32        miscMoCAClkStrapOverride ; /* (0x20) MoCA Clock Strap Overdide Reg */
#define MISC_MOCA_CLK_STRAP_OVERRIDE_MOCA_CLK_STRAP_OVERRIDE_CTL   0x00000001
  uint32        miscMoCAGpioOverlayLo ;    /* (0x24) MoCA GPIO Overlay bus lo Reg */
  uint32        miscMoCAGpioOverlayHi ;    /* (0x28) MoCA GPIO Overlay bus hi Reg */
#define MISC_MOCA_GPIO_UART                 0x000000C0
  uint32        miscDdrPllOutEnCh ;        /* (0x2C) DDR PLL Out En Ch Number */
#define MISC_DDR_PLL_OUTEN_CH_DDR_PLL_OUTEN_CH_MASK                0x000001FF
#define MISC_DDR_PLL_OUTEN_CH_DDR_PLL_OUTEN_CH_SHIFT               0
#define MISC_DDR_PLL_OUTEN_CH_MOCA_CPU_CLK                 0x00000080
#define MISC_DDR_PLL_OUTEN_CH_MOCA_PHY_CLK                 0x00000100
  uint32        miscGpioDiagOverlay ;      /* (0x30) GPIO Diag Overlay Control Reg */
#define MISC_GPIO_DIAG_OVERLAY_DIAG_OVERLAY_EN                     0x00000020
#define MISC_GPIO_DIAG_OVERLAY_DIAG_OVERLAY_MASK                   0x0000001F
#define MISC_GPIO_DIAG_OVERLAY_DIAG_OVERLAY_PORT_39_32             0x00000010
#define MISC_GPIO_DIAG_OVERLAY_DIAG_OVERLAY_PORT_31_24             0x00000008
#define MISC_GPIO_DIAG_OVERLAY_DIAG_OVERLAY_PORT_23_16             0x00000004
#define MISC_GPIO_DIAG_OVERLAY_DIAG_OVERLAY_PORT_15_8              0x00000002
#define MISC_GPIO_DIAG_OVERLAY_DIAG_OVERLAY_PORT_7_0               0x00000001
  uint32        miscGpioModeCtrlHi ;       /* (0x34) GPIO Pin Mode Control Reg Hi */
} Misc ;

#define MISC ((volatile Misc * const) MISC_BASE)

typedef struct HvgMiscRegisterChannelA {
   uint32        mask;
#define   K_PROP                          0x0000000f
#define   K_INTEG                         0x000000f0
#define   SER_TST_OUTPUT_SEL              0x00000700
#define   CONT_OR_BLOCK                   0x00000800
#define   HVG_MODE                        0x00003000
#define   HVG_MODE_OFFHOOK_TRACKING       0x00001000
#define   HVG_MODE_ONHOOK_FIXED           0x00002000
#define   HVG_SOFT_INIT_0                 0x00004000
#define   HVG_RR_SINGLE                   0x00008000
} HvgMiscRegisterChannelA;

#define HVG_MISC_REG_CHANNEL_A ((volatile HvgMiscRegisterChannelA * const) APM_HVG_BASE_REG_15)

typedef struct HvgMiscRegisterChannelB {
   uint32        mask;
} HvgMiscRegisterChannelB;

#define HVG_MISC_REG_CHANNEL_B ((volatile HvgMiscRegisterChannelB * const) APM_HVG_BASE_REG_19)

#define IUDMA_MAX_CHANNELS          32

/*
** DMA Channel Configuration (1 .. 32)
*/
typedef struct DmaChannelCfg {
  uint32        cfg;                    /* (00) assorted configuration */
#define         DMA_ENABLE      0x00000001  /* set to enable channel */
#define         DMA_PKT_HALT    0x00000002  /* idle after an EOP flag is detected */
#define         DMA_BURST_HALT  0x00000004  /* idle after finish current memory burst */
  uint32        intStat;                /* (04) interrupts control and status */
  uint32        intMask;                /* (08) interrupts mask */
#define         DMA_BUFF_DONE   0x00000001  /* buffer done */
#define         DMA_DONE        0x00000002  /* packet xfer complete */
#define         DMA_NO_DESC     0x00000004  /* no valid descriptors */
  uint32        maxBurst;               /* (0C) max burst length permitted */
#define         DMA_DESCSIZE_SEL 0x00040000  /* DMA Descriptor Size Selection */
} DmaChannelCfg;

/*
** DMA State RAM (1 .. 16)
*/
typedef struct DmaStateRam {
  uint32        baseDescPtr;            /* (00) descriptor ring start address */
  uint32        state_data;             /* (04) state/bytes done/ring offset */
  uint32        desc_len_status;        /* (08) buffer descriptor status and len */
  uint32        desc_base_bufptr;       /* (0C) buffer descrpitor current processing */
} DmaStateRam;


/*
** DMA Registers
*/
typedef struct DmaRegs {
    uint32 controller_cfg;              /* (00) controller configuration */
#define DMA_MASTER_EN           0x00000001
#define DMA_FLOWC_CH1_EN        0x00000002
#define DMA_FLOWC_CH3_EN        0x00000004

    // Flow control Ch1
    uint32 flowctl_ch1_thresh_lo;           /* 004 */
    uint32 flowctl_ch1_thresh_hi;           /* 008 */
    uint32 flowctl_ch1_alloc;               /* 00c */
#define DMA_BUF_ALLOC_FORCE     0x80000000

    // Flow control Ch3
    uint32 flowctl_ch3_thresh_lo;           /* 010 */
    uint32 flowctl_ch3_thresh_hi;           /* 014 */
    uint32 flowctl_ch3_alloc;               /* 018 */

    // Flow control Ch5
    uint32 flowctl_ch5_thresh_lo;           /* 01C */
    uint32 flowctl_ch5_thresh_hi;           /* 020 */
    uint32 flowctl_ch5_alloc;               /* 024 */

    // Flow control Ch7
    uint32 flowctl_ch7_thresh_lo;           /* 028 */
    uint32 flowctl_ch7_thresh_hi;           /* 02C */
    uint32 flowctl_ch7_alloc;               /* 030 */

    uint32 ctrl_channel_reset;              /* 034 */
    uint32 ctrl_channel_debug;              /* 038 */
    uint32 reserved1;                       /* 03C */
    uint32 ctrl_global_interrupt_status;    /* 040 */
    uint32 ctrl_global_interrupt_mask;      /* 044 */

    // Unused words
    uint8 reserved2[0x200-0x48];

    // Per channel registers/state ram
    DmaChannelCfg chcfg[IUDMA_MAX_CHANNELS];/* (200-3FF) Channel configuration */
    union {
        DmaStateRam     s[IUDMA_MAX_CHANNELS];
        uint32          u32[4 * IUDMA_MAX_CHANNELS];
    } stram;                                /* (400-5FF) state ram */
} DmaRegs;

#define SW_DMA ((volatile DmaRegs * const) SWITCH_DMA_BASE)

/*
** DMA Buffer
*/
typedef struct DmaDesc {
  union {
    struct {
        uint16        length;                   /* in bytes of data in buffer */
#define          DMA_DESC_USEFPM    0x8000
#define          DMA_DESC_MULTICAST 0x4000
#define          DMA_DESC_BUFLENGTH 0x0fff
        uint16        status;                   /* buffer status */
#define          DMA_OWN                0x8000  /* cleared by DMA, set by SW */
#define          DMA_EOP                0x4000  /* last buffer in packet */
#define          DMA_SOP                0x2000  /* first buffer in packet */
#define          DMA_WRAP               0x1000  /* */
#define          DMA_PRIO               0x0C00  /* Prio for Tx */
#define          DMA_APPEND_BRCM_TAG    0x0200
#define          DMA_APPEND_CRC         0x0100
#define          USB_ZERO_PKT           (1<< 0) // Set to send zero length packet
    };
    uint32      word0;
  };

  uint32        address;                /* address of data */
} DmaDesc;

/*
** 16 Byte DMA Buffer
*/
typedef struct {
  union {
    struct {
        uint16        length;                   /* in bytes of data in buffer */
#define          DMA_DESC_USEFPM        0x8000
#define          DMA_DESC_MULTICAST     0x4000
#define          DMA_DESC_BUFLENGTH     0x0fff
        uint16        status;                   /* buffer status */
#define          DMA_OWN                0x8000  /* cleared by DMA, set by SW */
#define          DMA_EOP                0x4000  /* last buffer in packet */
#define          DMA_SOP                0x2000  /* first buffer in packet */
#define          DMA_WRAP               0x1000  /* */
#define          DMA_PRIO               0x0C00  /* Prio for Tx */
#define          DMA_APPEND_BRCM_TAG    0x0200
#define          DMA_APPEND_CRC         0x0100
#define          USB_ZERO_PKT           (1<< 0) // Set to send zero length packet
    };
    uint32      word0;
  };

  uint32        address;                 /* address of data */
  uint32        control;
#define         GEM_ID_MASK             0x001F
  uint32        reserved;
} DmaDesc16;

/*
** External Bus Interface
*/
typedef struct EbiChipSelect {
    uint32    base;                         /* base address in upper 24 bits */
#define EBI_SIZE_8K         0
#define EBI_SIZE_16K        1
#define EBI_SIZE_32K        2
#define EBI_SIZE_64K        3
#define EBI_SIZE_128K       4
#define EBI_SIZE_256K       5
#define EBI_SIZE_512K       6
#define EBI_SIZE_1M         7
#define EBI_SIZE_2M         8
#define EBI_SIZE_4M         9
#define EBI_SIZE_8M         10
#define EBI_SIZE_16M        11
#define EBI_SIZE_32M        12
#define EBI_SIZE_64M        13
#define EBI_SIZE_128M       14
#define EBI_SIZE_256M       15
    uint32    config;
#define EBI_ENABLE          0x00000001      /* .. enable this range */
#define EBI_WAIT_STATES     0x0000000e      /* .. mask for wait states */
#define EBI_WTST_SHIFT      1               /* .. for shifting wait states */
#define EBI_WORD_WIDE       0x00000010      /* .. 16-bit peripheral, else 8 */
#define EBI_WREN            0x00000020      /* enable posted writes */
#define EBI_POLARITY        0x00000040      /* .. set to invert something,
                                        **    don't know what yet */
#define EBI_TS_TA_MODE      0x00000080      /* .. use TS/TA mode */
#define EBI_TS_SEL          0x00000100      /* .. drive tsize, not bs_b */
#define EBI_FIFO            0x00000200      /* .. use fifo */
#define EBI_RE              0x00000400      /* .. Reverse Endian */
#define EBI_SETUP_SHIFT     16
#define EBI_HOLD_SHIFT      20
#define EBI_SETUP_STATES    0x0f0000
#define EBI_HOLD_STATES     0xf00000
} EbiChipSelect;

typedef struct MpiRegisters {
  EbiChipSelect cs[7];                  /* size chip select configuration */
#define EBI_CS0_BASE            0
#define EBI_CS1_BASE            1
#define EBI_CS2_BASE            2
#define EBI_CS3_BASE            3
#define EBI_CS4_BASE            4
#define EBI_CS5_BASE            5
#define EBI_CS6_BASE            6
  uint32        unused0[2];             /* reserved */
  uint32        ebi_control;            /* ebi control */
#define EBI_ACCESS_TIMEOUT      0x000007FF
  uint32        unused1[7];             /* reserved */

  uint32        sp0range;               /* PCI to internal system bus address space */
#define ADDR_SPACE_MASK         0xFFFF0000
  uint32        sp0remap;
  uint32        sp0cfg;
  uint32        sp1range;
  uint32        sp1remap;
  uint32        sp1cfg;

  uint32        EndianCfg;

  uint32        l2pcfgctl;              /* internal system bus to PCI IO/Cfg control */
#define DIR_CFG_SEL             0x80000000 /* change from PCI I/O access to PCI config access */
#define DIR_CFG_USEREG          0x40000000 /* use this register info for PCI configuration access */
#define DEVICE_NUMBER           0x00007C00 /* device number for the PCI configuration access */
#define FUNC_NUMBER             0x00000300 /* function number for the PCI configuration access */
#define REG_NUMBER              0x000000FC /* register number for the PCI configuration access */
#define CONFIG_TYPE             0x00000003 /* configuration type for the PCI configuration access */

  uint32        l2pmrange1;             /* internal system bus to PCI memory space */
#define PCI_SIZE_64K            0xFFFF0000
#define PCI_SIZE_128K           0xFFFE0000
#define PCI_SIZE_256K           0xFFFC0000
#define PCI_SIZE_512K           0xFFF80000
#define PCI_SIZE_1M             0xFFF00000
#define PCI_SIZE_2M             0xFFE00000
#define PCI_SIZE_4M             0xFFC00000
#define PCI_SIZE_8M             0xFF800000
#define PCI_SIZE_16M            0xFF000000
#define PCI_SIZE_32M            0xFE000000
  uint32        l2pmbase1;              /* kseg0 or kseg1 address & 0x1FFFFFFF */
  uint32        l2pmremap1;
#define CARDBUS_MEM             0x00000004
#define MEM_WINDOW_EN           0x00000001
  uint32        l2pmrange2;
  uint32        l2pmbase2;
  uint32        l2pmremap2;
  uint32        l2piorange;             /* internal system bus to PCI I/O space */
  uint32        l2piobase;
  uint32        l2pioremap;

  uint32        pcimodesel;
#define PCI_INT_BUS_RD_PREFETCH 0x000001F0
#define PCI_BAR2_NOSWAP         0x00000002 /* BAR at offset 0x20 */
#define PCI_BAR1_NOSWAP         0x00000001 /* BAR at affset 0x1c */

  uint32        pciintstat;             /* PCI interrupt mask/status */
#define MAILBOX1_SENT           0x08
#define MAILBOX0_SENT           0x04
#define MAILBOX1_MSG_RCV        0x02
#define MAILBOX0_MSG_RCV        0x01
  uint32        locbuscntrl;            /* internal system bus control */
#define DIR_U2P_NOSWAP          0x00000002
#define EN_PCI_GPIO             0x00000001
  uint32        locintstat;             /* internal system bus interrupt mask/status */
#define CSERR                   0x0200
#define SERR                    0x0100
#define EXT_PCI_INT             0x0080
#define DIR_FAILED              0x0040
#define DIR_COMPLETE            0x0020
#define PCI_CFG                 0x0010
  uint32        unused4[7];

  uint32        mailbox0;
  uint32        mailbox1;

  uint32        pcicfgcntrl;            /* internal system bus PCI configuration control */
#define PCI_CFG_REG_WRITE_EN    0x00000080
#define PCI_CFG_ADDR            0x0000003C
  uint32        pcicfgdata;             /* internal system bus PCI configuration data */

  uint32        locch2ctl;              /* PCI to interrnal system bus DMA (downstream) local control */
#define MPI_DMA_HALT            0x00000008  /* idle after finish current memory burst */
#define MPI_DMA_PKT_HALT        0x00000004  /* idle after an EOP flag is detected */
#define MPI_DMA_STALL           0x00000002  /* idle after an EOP flag is detected */
#define MPI_DMA_ENABLE          0x00000001  /* set to enable channel */
  uint32        locch2intStat;
#define MPI_DMA_NO_DESC         0x00000004  /* no valid descriptors */
#define MPI_DMA_DONE            0x00000002  /* packet xfer complete */
#define MPI_DMA_BUFF_DONE       0x00000001  /* buffer done */
  uint32        locch2intMask;
  uint32        unused5;
  uint32        locch2descaddr;
  uint32        locch2status1;
#define LOCAL_DESC_STATE        0xE0000000
#define PCI_DESC_STATE          0x1C000000
#define BYTE_DONE               0x03FFC000
#define RING_ADDR               0x00003FFF
  uint32        locch2status2;
#define BUFPTR_OFFSET           0x1FFF0000
#define PCI_MASTER_STATE        0x000000C0
#define LOC_MASTER_STATE        0x00000038
#define CONTROL_STATE           0x00000007
  uint32        unused6;

  uint32        locch1Ctl;              /*internal system bus to PCI DMA (upstream) local control */
#define DMA_U2P_LE              0x00000200  /* local bus is little endian */
#define DMA_U2P_NOSWAP          0x00000100  /* lccal bus is little endian but no data swapped */
  uint32        locch1intstat;
  uint32        locch1intmask;
  uint32        unused7;
  uint32        locch1descaddr;
  uint32        locch1status1;
  uint32        locch1status2;
  uint32        unused8;

  uint32        pcich1ctl;              /* internal system bus to PCI DMA PCI control */
  uint32        pcich1intstat;
  uint32        pcich1intmask;
  uint32        pcich1descaddr;
  uint32        pcich1status1;
  uint32        pcich1status2;

  uint32        pcich2Ctl;              /* PCI to internal system bus DMA PCI control */
  uint32        pcich2intstat;
  uint32        pcich2intmask;
  uint32        pcich2descaddr;
  uint32        pcich2status1;
  uint32        pcich2status2;

  uint32        perm_id;                /* permanent device and vendor id */
  uint32        perm_rev;               /* permanent revision id */
} MpiRegisters;

#define MPI ((volatile MpiRegisters * const) MPI_BASE)

/* PCI configuration address space start offset 0x40 */
#define BRCM_PCI_CONFIG_TIMER               0x40
#define BRCM_PCI_CONFIG_TIMER_RETRY_MASK    0x0000FF00
#define BRCM_PCI_CONFIG_TIMER_TRDY_MASK     0x000000FF

typedef struct USBControl {
    uint32 BrtControl1;
    uint32 BrtControl2;
    uint32 BrtStatus1;
    uint32 BrtStatus2;
    uint32 UTMIControl1;
    uint32 TestPortControl;
    uint32 PllControl1;
    uint32 SwapControl;
#define USB_DEVICE_SEL          (1<<6)
#define EHCI_LOGICAL_ADDRESS_EN (1<<5)
#define EHCI_ENDIAN_SWAP        (1<<4)
#define EHCI_DATA_SWAP          (1<<3)
#define OHCI_LOGICAL_ADDRESS_EN (1<<2)
#define OHCI_ENDIAN_SWAP        (1<<1)
#define OHCI_DATA_SWAP          (1<<0)
    uint32 unused1;
    uint32 FrameAdjustValue;
    uint32 Setup;
#define USBH_IOC                (1<<4)
#define USBH_IPP			    (1<<5)
    uint32 MDIO;
    uint32 MDIO32;
    uint32 USBSimControl;
} USBControl;

#define USBH ((volatile USBControl * const) USBH_CFG_BASE)

/*
** GPON SERDES Registers
*/
typedef struct GponSerdesRegs {
  uint32 topCfg;
  uint32 swReset;
  uint32 aeCfg;
  uint32 aeStatus;
  uint32 phyCfg;
  uint32 phyStatus;
  uint32 mdioWr;
  uint32 mdioRd;
  uint32 reserved[2];
  uint32 fifoCfg;
  uint32 fifoStatus;
  uint32 patternCfg[4];
  uint32 patternStatus[2];
  uint32 laserCfg;
#define GPON_SERDES_LASERMODE_MASK (3<<30)
#define GPON_SERDES_LASERMODE_NORMAL (0<<30)
#define GPON_SERDES_LASERMODE_FORCE_OFF (1<<30)
#define GPON_SERDES_LASERMODE_FORCE_ON (2<<30)
  uint32 laserStatus;
  uint32 miscCfg;
  uint32 miscStatus;
  uint32 phDet[5];
} GponSerdesRegs;

#define GPON_SERDES ((volatile GponSerdesRegs * const) GPON_SERDES_BASE)


/*
** PCI-E
*/
typedef struct PcieRegs{
  uint32 devVenID;
  uint16 command;
  uint16 status;
  uint32 revIdClassCode;
  uint32 headerTypeLatCacheLineSize;
  uint32 bar1;
  uint32 bar2;
  uint32 priSecBusNo;
#define PCIE_CFG_TYPE1_PRI_SEC_BUS_NO_SUB_BUS_NO_MASK              0x00ff0000
#define PCIE_CFG_TYPE1_PRI_SEC_BUS_NO_SUB_BUS_NO_SHIFT             16
#define PCIE_CFG_TYPE1_PRI_SEC_BUS_NO_SEC_BUS_NO_MASK              0x0000ff00
#define PCIE_CFG_TYPE1_PRI_SEC_BUS_NO_SEC_BUS_NO_SHIFT             8
#define PCIE_CFG_TYPE1_PRI_SEC_BUS_NO_PRI_BUS_NO_MASK              0x000000ff
} PcieRegs;

typedef struct PcieBlk404Regs{
  uint32 unused;          /* 0x404 */
  uint32 config2;         /* 0x408 */
#define PCIE_IP_BLK404_CONFIG_2_BAR1_SIZE_MASK         0x0000000f
#define PCIE_IP_BLK404_CONFIG_2_BAR1_DISABLE           0
  uint32 config3;         /* 0x40c */
  uint32 pmDataA;         /* 0x410 */
  uint32 pmDataB;         /* 0x414 */
} PcieBlk404Regs;

typedef struct PcieBlk428Regs{
   uint32 vpdIntf;        /* 0x428 */
   uint16 unused_g;       /* 0x42c */
   uint16 vpdAddrFlag;    /* 0x42e */
   uint32 vpdData;        /* 0x430 */
   uint32 idVal1;         /* 0x434 */
   uint32 idVal2;         /* 0x438 */
   uint32 idVal3;         /* 0x43c */
#define PCIE_IP_BLK428_ID_VAL3_REVISION_ID_MASK                    0xff000000
#define PCIE_IP_BLK428_ID_VAL3_REVISION_ID_SHIFT                   24
#define PCIE_IP_BLK428_ID_VAL3_CLASS_CODE_MASK                     0x00ffffff
#define PCIE_IP_BLK428_ID_VAL3_CLASS_CODE_SHIFT                    16
#define PCIE_IP_BLK428_ID_VAL3_SUB_CLASS_CODE_SHIFT                 8
}PcieBlk428Regs;

typedef struct PcieBridgeRegs{
   uint32 bar1Remap;       /* 0x0818*/
   uint32 bar2Remap;       /* 0x081c*/
   uint32 bridgeOptReg1;   /* 0x0820*/
#define PCIE_BRIDGE_OPT_REG1_EN_RD_BE                  (1 << 4)
#define PCIE_BRIDGE_OPT_REG1_EN_RD_BE_NOSWAP           (1 << 5)
   uint32 bridgeOptReg2;    /* 0x0824*/
#define PCIE_BRIDGE_OPT_REG2_cfg_type1_func_no_MASK    0xe0000000
#define PCIE_BRIDGE_OPT_REG2_cfg_type1_func_no_SHIFT   29
#define PCIE_BRIDGE_OPT_REG2_cfg_type1_dev_no_MASK     0x1f000000
#define PCIE_BRIDGE_OPT_REG2_cfg_type1_dev_no_SHIFT    24
#define PCIE_BRIDGE_OPT_REG2_cfg_type1_bus_no_MASK     0x00ff0000
#define PCIE_BRIDGE_OPT_REG2_cfg_type1_bus_no_SHIFT    16
#define PCIE_BRIDGE_OPT_REG2_cfg_type1_bd_sel_MASK     0x00000080
#define PCIE_BRIDGE_OPT_REG2_cfg_type1_bd_sel_SHIFT    7
#define PCIE_BRIDGE_OPT_REG2_dis_pcie_timeout_MASK     0x00000040
#define PCIE_BRIDGE_OPT_REG2_dis_pcie_timeout_SHIFT    6
#define PCIE_BRIDGE_OPT_REG2_dis_pcie_abort_MASK       0x00000020
#define PCIE_BRIDGE_OPT_REG2_dis_pcie_abort_SHIFT      5
}PcieBridgeRegs;

typedef struct PcieBlk1000Regs{
   uint32 undisclosed[18]; /*0x1000 - 0x1044*/
   uint32 dlStatus;        /* 0x1048 */
#define PCIE_IP_BLK1000_DL_STATUS_PHYLINKUP_MASK                   0x00002000
#define PCIE_IP_BLK1000_DL_STATUS_PHYLINKUP_SHIFT                  13
}PcieBlk1000Regs;

typedef struct PcieBlk1800Regs{
#define NUM_PCIE_BLK_1800_PHY_CTRL_REGS         4
  uint32 phyCtrl[NUM_PCIE_BLK_1800_PHY_CTRL_REGS];
#define REG_POWERDOWN_P1PLL_ENA                      (1<<12)
  uint32 phyErrorAttnVec;
  uint32 phyErrorAttnMask;
#define NUM_PCIE_BLK_1800_PHY_LTSSM_HIST_REGS   3
  uint32 phyltssmHist[NUM_PCIE_BLK_1800_PHY_LTSSM_HIST_REGS];
#define NUM_PCIE_BLK_1800_PHY_DBG_REGS          11
  uint32 phyDbg[NUM_PCIE_BLK_1800_PHY_DBG_REGS];
} PcieBlk1800Regs;

#define PCIEH_DEV_OFFSET              0x8000
#define PCIEH                         ((volatile uint32 * const) PCIE_BASE)
#define PCIEH_REGS          ((volatile PcieCfeType1Regs * const) PCIE_BASE)
#define PCIEH_BLK_404_REGS            ((volatile PcieBlk404Regs * const)(PCIE_BASE+0x404))
#define PCIEH_BLK_428_REGS            ((volatile PcieBlk428Regs * const)(PCIE_BASE+0x428))
#define PCIEH_BRIDGE_REGS             ((volatile PcieBridgeRegs * const) (PCIE_BASE+0x818))
#define PCIEH_BLK_1000_REGS           ((volatile PcieBlk1000Regs * const) (PCIE_BASE+0x1000))
#define PCIEH_BLK_1800_REGS           ((volatile PcieBlk1800Regs * const) (PCIE_BASE+0x1800))

/*
** NAND Controller Registers
*/
typedef struct NandCtrlRegs {
    uint32 NandRevision;            /* NAND Revision */
    uint32 NandCmdStart;            /* Nand Flash Command Start */
    uint32 NandCmdExtAddr;          /* Nand Flash Command Extended Address */
    uint32 NandCmdAddr;             /* Nand Flash Command Address */
    uint32 NandCmdEndAddr;          /* Nand Flash Command End Address */
    uint32 NandNandBootConfig;      /* Nand Flash Boot Config */
#define NBC_AUTO_DEV_ID_CFG 0x40000000
    uint32 NandCsNandXor;           /* Nand Flash EBI CS Address XOR with */
} NandCtrlRegs;

#define NAND ((volatile NandCtrlRegs * const) NAND_REG_BASE)

#ifdef __cplusplus
}
#endif

#endif

