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
 * Date:           Generated on         Wed Nov 14 03:20:37 2012
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

#ifndef BCHP_GB_QUEUE_3_CNTRL_GB_H__
#define BCHP_GB_QUEUE_3_CNTRL_GB_H__

/***************************************************************************
 *GB_QUEUE_3_CNTRL_GB
 ***************************************************************************/
#define BCHP_GB_QUEUE_3_CNTRL_GB_SIZE            0x104b0430 /* Queue Size Register */
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGA            0x104b0434 /* Queue Config A Register */
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGB            0x104b0438 /* Queue Config B Register */
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGC            0x104b043c /* Queue Config C Register */

/***************************************************************************
 *SIZE - Queue Size Register
 ***************************************************************************/
/* GB_QUEUE_3_CNTRL_GB :: SIZE :: Q_HEAD_PTR [31:18] */
#define BCHP_GB_QUEUE_3_CNTRL_GB_SIZE_Q_HEAD_PTR_MASK              0xfffc0000
#define BCHP_GB_QUEUE_3_CNTRL_GB_SIZE_Q_HEAD_PTR_SHIFT             18
#define BCHP_GB_QUEUE_3_CNTRL_GB_SIZE_Q_HEAD_PTR_DEFAULT           0x00000000

/* GB_QUEUE_3_CNTRL_GB :: SIZE :: Q_TAIL_PTR [17:04] */
#define BCHP_GB_QUEUE_3_CNTRL_GB_SIZE_Q_TAIL_PTR_MASK              0x0003fff0
#define BCHP_GB_QUEUE_3_CNTRL_GB_SIZE_Q_TAIL_PTR_SHIFT             4
#define BCHP_GB_QUEUE_3_CNTRL_GB_SIZE_Q_TAIL_PTR_DEFAULT           0x00000000

/* GB_QUEUE_3_CNTRL_GB :: SIZE :: reserved0 [03:02] */
#define BCHP_GB_QUEUE_3_CNTRL_GB_SIZE_reserved0_MASK               0x0000000c
#define BCHP_GB_QUEUE_3_CNTRL_GB_SIZE_reserved0_SHIFT              2

/* GB_QUEUE_3_CNTRL_GB :: SIZE :: Q_TKN_SIZE [01:00] */
#define BCHP_GB_QUEUE_3_CNTRL_GB_SIZE_Q_TKN_SIZE_MASK              0x00000003
#define BCHP_GB_QUEUE_3_CNTRL_GB_SIZE_Q_TKN_SIZE_SHIFT             0
#define BCHP_GB_QUEUE_3_CNTRL_GB_SIZE_Q_TKN_SIZE_DEFAULT           0x00000000

/***************************************************************************
 *CFGA - Queue Config A Register
 ***************************************************************************/
/* GB_QUEUE_3_CNTRL_GB :: CFGA :: Q_SIZE [31:16] */
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGA_Q_SIZE_MASK                  0xffff0000
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGA_Q_SIZE_SHIFT                 16
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGA_Q_SIZE_DEFAULT               0x00000000

/* GB_QUEUE_3_CNTRL_GB :: CFGA :: Q_START_ADDR [15:00] */
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGA_Q_START_ADDR_MASK            0x0000ffff
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGA_Q_START_ADDR_SHIFT           0
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGA_Q_START_ADDR_DEFAULT         0x00000000

/***************************************************************************
 *CFGB - Queue Config B Register
 ***************************************************************************/
/* GB_QUEUE_3_CNTRL_GB :: CFGB :: reserved0 [31:30] */
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGB_reserved0_MASK               0xc0000000
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGB_reserved0_SHIFT              30

/* GB_QUEUE_3_CNTRL_GB :: CFGB :: Q_NUM_TKNS [29:16] */
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGB_Q_NUM_TKNS_MASK              0x3fff0000
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGB_Q_NUM_TKNS_SHIFT             16
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGB_Q_NUM_TKNS_DEFAULT           0x00000000

/* GB_QUEUE_3_CNTRL_GB :: CFGB :: reserved1 [15:14] */
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGB_reserved1_MASK               0x0000c000
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGB_reserved1_SHIFT              14

/* GB_QUEUE_3_CNTRL_GB :: CFGB :: Q_LOW_WATERMARK [13:00] */
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGB_Q_LOW_WATERMARK_MASK         0x00003fff
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGB_Q_LOW_WATERMARK_SHIFT        0
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGB_Q_LOW_WATERMARK_DEFAULT      0x00000000

/***************************************************************************
 *CFGC - Queue Config C Register
 ***************************************************************************/
/* GB_QUEUE_3_CNTRL_GB :: CFGC :: reserved0 [31:14] */
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGC_reserved0_MASK               0xffffc000
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGC_reserved0_SHIFT              14

/* GB_QUEUE_3_CNTRL_GB :: CFGC :: Q_HIGH_WATERMARK [13:00] */
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGC_Q_HIGH_WATERMARK_MASK        0x00003fff
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGC_Q_HIGH_WATERMARK_SHIFT       0
#define BCHP_GB_QUEUE_3_CNTRL_GB_CFGC_Q_HIGH_WATERMARK_DEFAULT     0x00000000

#endif /* #ifndef BCHP_GB_QUEUE_3_CNTRL_GB_H__ */

/* End of File */
