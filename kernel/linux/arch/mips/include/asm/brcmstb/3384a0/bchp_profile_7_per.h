/***************************************************************************
 *     Copyright (c) 1999-2012, Broadcom Corporation
 *     All Rights Reserved
 *     Confidential Property of Broadcom Corporation
 *
 *
 * THIS SOFTWARE MAY ONLY BE USED SUBJECT TO AN EXECUTED SOFTWARE LICENSE
 * AGREEMENT  BETWEEN THE USER AND BROADCOM.  YOU HAVE NO RIGHT TO USE OR
 * EXPLOIT THIS MATERIAL EXCEPT SUBJECT TO THE TERMS OF SUCH AN AGREEMENT.
 *
 * $brcm_Workfile: $
 * $brcm_Revision: $
 * $brcm_Date: $
 *
 * Module Description:
 *                     DO NOT EDIT THIS FILE DIRECTLY
 *
 * This module was generated magically with RDB from a source description
 * file. You must edit the source file for changes to be made to this file.
 *
 *
 * Date:           Generated on         Wed Nov 14 03:20:39 2012
 *                 MD5 Checksum         d41d8cd98f00b204e9800998ecf8427e
 *
 * Compiled with:  RDB Utility          combo_header.pl
 *                 RDB Parser           3.0
 *                 unknown              unknown
 *                 Perl Interpreter     5.008008
 *                 Operating System     linux
 *
 * Revision History:
 *
 * $brcm_Log: $
 *
 ***************************************************************************/

#ifndef BCHP_PROFILE_7_PER_H__
#define BCHP_PROFILE_7_PER_H__

/***************************************************************************
 *PROFILE_7_PER - Profile #7 register bank PER
 ***************************************************************************/
#define BCHP_PROFILE_7_PER_CLK_CTRL              0x14e011e0 /* Clock Control Register */
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL           0x14e011e4 /* Signaling Control Register */
#define BCHP_PROFILE_7_PER_MODE_CTRL             0x14e011e8 /* Profile Mode Control */
#define BCHP_PROFILE_7_PER_POLLING_CONFIG        0x14e011ec /* Profile Polling Config */
#define BCHP_PROFILE_7_PER_POLLING_AND_MASK      0x14e011f0 /* Polling AND Mask */
#define BCHP_PROFILE_7_PER_POLLING_COMPARE       0x14e011f4 /* Polling Compare Value */
#define BCHP_PROFILE_7_PER_POLLING_TIMEOUT       0x14e011f8 /* Polling Timeout Value */
#define BCHP_PROFILE_7_PER_RESERVED_WORD         0x14e011fc /* Reserved */

/***************************************************************************
 *CLK_CTRL - Clock Control Register
 ***************************************************************************/
/* PROFILE_7_PER :: CLK_CTRL :: reserved0 [31:17] */
#define BCHP_PROFILE_7_PER_CLK_CTRL_reserved0_MASK                 0xfffe0000
#define BCHP_PROFILE_7_PER_CLK_CTRL_reserved0_SHIFT                17

/* PROFILE_7_PER :: CLK_CTRL :: CLK_POLARITY [16:16] */
#define BCHP_PROFILE_7_PER_CLK_CTRL_CLK_POLARITY_MASK              0x00010000
#define BCHP_PROFILE_7_PER_CLK_CTRL_CLK_POLARITY_SHIFT             16
#define BCHP_PROFILE_7_PER_CLK_CTRL_CLK_POLARITY_DEFAULT           0x00000000

/* PROFILE_7_PER :: CLK_CTRL :: ACCUM_RST_ON_LOOP [15:15] */
#define BCHP_PROFILE_7_PER_CLK_CTRL_ACCUM_RST_ON_LOOP_MASK         0x00008000
#define BCHP_PROFILE_7_PER_CLK_CTRL_ACCUM_RST_ON_LOOP_SHIFT        15
#define BCHP_PROFILE_7_PER_CLK_CTRL_ACCUM_RST_ON_LOOP_DEFAULT      0x00000001

/* PROFILE_7_PER :: CLK_CTRL :: reserved1 [14:12] */
#define BCHP_PROFILE_7_PER_CLK_CTRL_reserved1_MASK                 0x00007000
#define BCHP_PROFILE_7_PER_CLK_CTRL_reserved1_SHIFT                12

/* PROFILE_7_PER :: CLK_CTRL :: BYPASS_DIVIDER [11:11] */
#define BCHP_PROFILE_7_PER_CLK_CTRL_BYPASS_DIVIDER_MASK            0x00000800
#define BCHP_PROFILE_7_PER_CLK_CTRL_BYPASS_DIVIDER_SHIFT           11
#define BCHP_PROFILE_7_PER_CLK_CTRL_BYPASS_DIVIDER_DEFAULT         0x00000000

/* PROFILE_7_PER :: CLK_CTRL :: FREQ_CTRL_WORD [10:00] */
#define BCHP_PROFILE_7_PER_CLK_CTRL_FREQ_CTRL_WORD_MASK            0x000007ff
#define BCHP_PROFILE_7_PER_CLK_CTRL_FREQ_CTRL_WORD_SHIFT           0
#define BCHP_PROFILE_7_PER_CLK_CTRL_FREQ_CTRL_WORD_DEFAULT         0x00000067

/***************************************************************************
 *SIGNAL_CTRL - Signaling Control Register
 ***************************************************************************/
/* PROFILE_7_PER :: SIGNAL_CTRL :: reserved0 [31:17] */
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_reserved0_MASK              0xfffe0000
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_reserved0_SHIFT             17

/* PROFILE_7_PER :: SIGNAL_CTRL :: ASYNC_INPUT_PATH_EN [16:16] */
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_ASYNC_INPUT_PATH_EN_MASK    0x00010000
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_ASYNC_INPUT_PATH_EN_SHIFT   16
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_ASYNC_INPUT_PATH_EN_DEFAULT 0x00000000

/* PROFILE_7_PER :: SIGNAL_CTRL :: LAUNCH_MSB_FIRST [15:15] */
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_LAUNCH_MSB_FIRST_MASK       0x00008000
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_LAUNCH_MSB_FIRST_SHIFT      15
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_LAUNCH_MSB_FIRST_DEFAULT    0x00000001

/* PROFILE_7_PER :: SIGNAL_CTRL :: LATCH_MSB_FIRST [14:14] */
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_LATCH_MSB_FIRST_MASK        0x00004000
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_LATCH_MSB_FIRST_SHIFT       14
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_LATCH_MSB_FIRST_DEFAULT     0x00000001

/* PROFILE_7_PER :: SIGNAL_CTRL :: LAUNCH_RISING [13:13] */
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_LAUNCH_RISING_MASK          0x00002000
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_LAUNCH_RISING_SHIFT         13
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_LAUNCH_RISING_DEFAULT       0x00000000

/* PROFILE_7_PER :: SIGNAL_CTRL :: LATCH_RISING [12:12] */
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_LATCH_RISING_MASK           0x00001000
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_LATCH_RISING_SHIFT          12
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_LATCH_RISING_DEFAULT        0x00000001

/* PROFILE_7_PER :: SIGNAL_CTRL :: reserved1 [11:08] */
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_reserved1_MASK              0x00000f00
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_reserved1_SHIFT             8

/* PROFILE_7_PER :: SIGNAL_CTRL :: SS_OFFTIME [07:00] */
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_SS_OFFTIME_MASK             0x000000ff
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_SS_OFFTIME_SHIFT            0
#define BCHP_PROFILE_7_PER_SIGNAL_CTRL_SS_OFFTIME_DEFAULT          0x00000004

/***************************************************************************
 *MODE_CTRL - Profile Mode Control
 ***************************************************************************/
/* PROFILE_7_PER :: MODE_CTRL :: reserved0 [31:28] */
#define BCHP_PROFILE_7_PER_MODE_CTRL_reserved0_MASK                0xf0000000
#define BCHP_PROFILE_7_PER_MODE_CTRL_reserved0_SHIFT               28

/* PROFILE_7_PER :: MODE_CTRL :: PREPENDBYTE_CNT [27:24] */
#define BCHP_PROFILE_7_PER_MODE_CTRL_PREPENDBYTE_CNT_MASK          0x0f000000
#define BCHP_PROFILE_7_PER_MODE_CTRL_PREPENDBYTE_CNT_SHIFT         24
#define BCHP_PROFILE_7_PER_MODE_CTRL_PREPENDBYTE_CNT_DEFAULT       0x00000004

/* PROFILE_7_PER :: MODE_CTRL :: reserved1 [23:21] */
#define BCHP_PROFILE_7_PER_MODE_CTRL_reserved1_MASK                0x00e00000
#define BCHP_PROFILE_7_PER_MODE_CTRL_reserved1_SHIFT               21

/* PROFILE_7_PER :: MODE_CTRL :: MODE_ONE_WIRE [20:20] */
#define BCHP_PROFILE_7_PER_MODE_CTRL_MODE_ONE_WIRE_MASK            0x00100000
#define BCHP_PROFILE_7_PER_MODE_CTRL_MODE_ONE_WIRE_SHIFT           20
#define BCHP_PROFILE_7_PER_MODE_CTRL_MODE_ONE_WIRE_DEFAULT         0x00000000

/* PROFILE_7_PER :: MODE_CTRL :: MULTIDATA_WR_SIZE [19:18] */
#define BCHP_PROFILE_7_PER_MODE_CTRL_MULTIDATA_WR_SIZE_MASK        0x000c0000
#define BCHP_PROFILE_7_PER_MODE_CTRL_MULTIDATA_WR_SIZE_SHIFT       18
#define BCHP_PROFILE_7_PER_MODE_CTRL_MULTIDATA_WR_SIZE_DEFAULT     0x00000000

/* PROFILE_7_PER :: MODE_CTRL :: MULTIDATA_RD_SIZE [17:16] */
#define BCHP_PROFILE_7_PER_MODE_CTRL_MULTIDATA_RD_SIZE_MASK        0x00030000
#define BCHP_PROFILE_7_PER_MODE_CTRL_MULTIDATA_RD_SIZE_SHIFT       16
#define BCHP_PROFILE_7_PER_MODE_CTRL_MULTIDATA_RD_SIZE_DEFAULT     0x00000000

/* PROFILE_7_PER :: MODE_CTRL :: MULTIDATA_WR_STRT [15:12] */
#define BCHP_PROFILE_7_PER_MODE_CTRL_MULTIDATA_WR_STRT_MASK        0x0000f000
#define BCHP_PROFILE_7_PER_MODE_CTRL_MULTIDATA_WR_STRT_SHIFT       12
#define BCHP_PROFILE_7_PER_MODE_CTRL_MULTIDATA_WR_STRT_DEFAULT     0x00000002

/* PROFILE_7_PER :: MODE_CTRL :: MULTIDATA_RD_STRT [11:08] */
#define BCHP_PROFILE_7_PER_MODE_CTRL_MULTIDATA_RD_STRT_MASK        0x00000f00
#define BCHP_PROFILE_7_PER_MODE_CTRL_MULTIDATA_RD_STRT_SHIFT       8
#define BCHP_PROFILE_7_PER_MODE_CTRL_MULTIDATA_RD_STRT_DEFAULT     0x00000002

/* PROFILE_7_PER :: MODE_CTRL :: FILLBYTE [07:00] */
#define BCHP_PROFILE_7_PER_MODE_CTRL_FILLBYTE_MASK                 0x000000ff
#define BCHP_PROFILE_7_PER_MODE_CTRL_FILLBYTE_SHIFT                0
#define BCHP_PROFILE_7_PER_MODE_CTRL_FILLBYTE_DEFAULT              0x000000ff

/***************************************************************************
 *POLLING_CONFIG - Profile Polling Config
 ***************************************************************************/
/* PROFILE_7_PER :: POLLING_CONFIG :: reserved0 [31:07] */
#define BCHP_PROFILE_7_PER_POLLING_CONFIG_reserved0_MASK           0xffffff80
#define BCHP_PROFILE_7_PER_POLLING_CONFIG_reserved0_SHIFT          7

/* PROFILE_7_PER :: POLLING_CONFIG :: DO_EQUAL_TO [06:06] */
#define BCHP_PROFILE_7_PER_POLLING_CONFIG_DO_EQUAL_TO_MASK         0x00000040
#define BCHP_PROFILE_7_PER_POLLING_CONFIG_DO_EQUAL_TO_SHIFT        6
#define BCHP_PROFILE_7_PER_POLLING_CONFIG_DO_EQUAL_TO_DEFAULT      0x00000001

/* PROFILE_7_PER :: POLLING_CONFIG :: REPEAT_CNT [05:04] */
#define BCHP_PROFILE_7_PER_POLLING_CONFIG_REPEAT_CNT_MASK          0x00000030
#define BCHP_PROFILE_7_PER_POLLING_CONFIG_REPEAT_CNT_SHIFT         4
#define BCHP_PROFILE_7_PER_POLLING_CONFIG_REPEAT_CNT_DEFAULT       0x00000000

/* PROFILE_7_PER :: POLLING_CONFIG :: OFFSET_STRT [03:00] */
#define BCHP_PROFILE_7_PER_POLLING_CONFIG_OFFSET_STRT_MASK         0x0000000f
#define BCHP_PROFILE_7_PER_POLLING_CONFIG_OFFSET_STRT_SHIFT        0
#define BCHP_PROFILE_7_PER_POLLING_CONFIG_OFFSET_STRT_DEFAULT      0x00000002

/***************************************************************************
 *POLLING_AND_MASK - Polling AND Mask
 ***************************************************************************/
/* PROFILE_7_PER :: POLLING_AND_MASK :: AND_MASK [31:00] */
#define BCHP_PROFILE_7_PER_POLLING_AND_MASK_AND_MASK_MASK          0xffffffff
#define BCHP_PROFILE_7_PER_POLLING_AND_MASK_AND_MASK_SHIFT         0
#define BCHP_PROFILE_7_PER_POLLING_AND_MASK_AND_MASK_DEFAULT       0x00000001

/***************************************************************************
 *POLLING_COMPARE - Polling Compare Value
 ***************************************************************************/
/* PROFILE_7_PER :: POLLING_COMPARE :: COMPARE_VALUE [31:00] */
#define BCHP_PROFILE_7_PER_POLLING_COMPARE_COMPARE_VALUE_MASK      0xffffffff
#define BCHP_PROFILE_7_PER_POLLING_COMPARE_COMPARE_VALUE_SHIFT     0
#define BCHP_PROFILE_7_PER_POLLING_COMPARE_COMPARE_VALUE_DEFAULT   0x00000001

/***************************************************************************
 *POLLING_TIMEOUT - Polling Timeout Value
 ***************************************************************************/
/* PROFILE_7_PER :: POLLING_TIMEOUT :: reserved0 [31:16] */
#define BCHP_PROFILE_7_PER_POLLING_TIMEOUT_reserved0_MASK          0xffff0000
#define BCHP_PROFILE_7_PER_POLLING_TIMEOUT_reserved0_SHIFT         16

/* PROFILE_7_PER :: POLLING_TIMEOUT :: TIMEOUT_VALUE [15:00] */
#define BCHP_PROFILE_7_PER_POLLING_TIMEOUT_TIMEOUT_VALUE_MASK      0x0000ffff
#define BCHP_PROFILE_7_PER_POLLING_TIMEOUT_TIMEOUT_VALUE_SHIFT     0
#define BCHP_PROFILE_7_PER_POLLING_TIMEOUT_TIMEOUT_VALUE_DEFAULT   0x0000ffff

/***************************************************************************
 *RESERVED_WORD - Reserved
 ***************************************************************************/
/* PROFILE_7_PER :: RESERVED_WORD :: reserved0 [31:00] */
#define BCHP_PROFILE_7_PER_RESERVED_WORD_reserved0_MASK            0xffffffff
#define BCHP_PROFILE_7_PER_RESERVED_WORD_reserved0_SHIFT           0

#endif /* #ifndef BCHP_PROFILE_7_PER_H__ */

/* End of File */
