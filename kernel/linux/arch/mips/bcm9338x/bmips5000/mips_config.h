/***************************************************************************
 *     Copyright (c) 2008-2010, Broadcom Corporation
 *     All Rights Reserved
 *     Confidential Property of Broadcom Corporation
 *
 *  THIS SOFTWARE MAY ONLY BE USED SUBJECT TO AN EXECUTED SOFTWARE LICENSE
 *  AGREEMENT  BETWEEN THE USER AND BROADCOM.  YOU HAVE NO RIGHT TO USE OR
 *  EXPLOIT THIS MATERIAL EXCEPT SUBJECT TO THE TERMS OF SUCH AN AGREEMENT.
 *
 * $brcm_Workfile: mips_config.h $
 * $brcm_Revision: Hydra_Software_Devel/1 $
 * $brcm_Date: 11/9/10 4:54p $
 *
 * Module Description:
 *
 * Revision History:
 *
 * $brcm_Log: /rockford/bsp/bcm97422/no-os/src/sde/mips_config.h $
 * 
 * Hydra_Software_Devel/1   11/9/10 4:54p jkim
 * SWCFE-399: initial files
 * 
 * Bsp_Software_Devel/3   9/25/09 1:53p kaushikb
 * SWCFE-239: Updating for 7420 CFE release v2.11
 *
 *                        4/4/08   	 tinn
 * Modified for 3380 chip.
 * 
 * Bsp_Software_Devel/2   10/24/07 6:58p farshidf
 * PR36360: new  BSP code
 * 
 * Bsp_Software_Devel/1   10/22/07 4:41p farshidf
 * PR36360: New BSP code
 *

***************************************************************************/
#ifndef __MIPS_CONFIG_H__
#define __MIPS_CONFIG_H__

#define	MIPS_5xxx		1

#if defined(__ASSEMBLER__)

/* common for 4380, 4350, and 5xxx */
#define CP0_INDEX               $0
#define CP0_RANDOM              $1
#define CP0_ENTRY_LO_0          $2
#define CP0_ENTRY_LO_1          $3
#define CP0_CONTEXT             $4
#define CP0_PAGE_MASK           $5
#define CP0_WIRED               $6
#define CP0_BAD_VADDR           $8
#define CP0_COUNT               $9
#define CP0_ENTRY_HI            $10
#define CP0_COMPARE             $11
#define CP0_STATUS              $12
#define CP0_CAUSE               $13
#define CP0_EPC                 $14
#define CPO_PROC_ID             $15
#define CP0_CONFIG              $16
#define CP0_CONFIG1             $16, 1
#define CP0_LLADDR              $17
#define CP0_BRCM_CONFIG0        $22
#define CP0_DEBUG               $23
#define CP0_DEBUG_EPC           $24
#define CP0_ERROR_EPC           $30
#define CP0_DE_SAVE             $31

/* specific for 5xxx */
#if defined MIPS_5xxx

#define CP0_USER_LOCAL		$4, 2
#define CP0_HWRENA		$7
#define CP0_WAIT_COUNT_X	$9
#define CP0_WAIT_COUNT		$9, 7
#define CP0_INTRPT_CTRL		$12, 1
#define CP0_SRS_CTRL		$12, 2
#define CPO_EXCPT_BASE		$15, 1
#define CP0_CONFIG2             $16, 2
#define CP0_CONFIG3             $16, 3
#define CP0_CONFIG7             $16, 7
#define CP0_WATCH_LO_INST	$18
#define CP0_WATCH_LO_LS		$18, 1
#define CP0_WATCH_HI_INST	$19
#define CP0_WATCH_HI_LS		$19, 1
#define CP0_X_CONTEXT		$20
#define CP0_BRCM_MODE		$22, 1
#define CP0_BRCM_ACTION		$22, 2
#define CP0_BRCM_EDSP_CTRL	$22, 3
#define CP0_BRCM_BOOT_VEC	$22, 4
#define CP0_BRCM_SLEEP_COUNT	$22, 7
#define CP0_TRACE_CTRL		$23, 1
#define CP0_TRACE_CTRL2		$23, 2
#define CP0_TRACE_CTRL3		$23, 3
#define CP0_TRACE_DATA		$23, 4
#define CP0_TRACE_TRIGGER	$23, 5
#define CP0_DEBUG2		$23, 6
#define CP0_PERF_COUNT		$25
#define CP0_ERROR_COUNT		$26
#define CP0_CACHE_ERROR		$27
#define CP0_CACHE_ERROR1	$27, 1
#define CP0_CACHE_ERROR2	$27, 2
#define CP0_CACHE_ERROR3	$27, 3
#define CP0_ICACHE_TAG_LO	$28
#define CP0_ICACHE_DATA_LO	$28, 1
#define CP0_DCACHE_TAG_LO	$28, 2
#define CP0_D_SEC_CACHE_DATA_LO	$28, 3
#define CP0_ICACHE_TAG_HI	$29
#define CP0_ICACHE_DATA_HI	$29, 1
#define CP0_DCACHE_TAG_HI	$29, 2
#define CP0_D_SEC_CACHE_DATA_HI	$29, 3

#endif /* MIPS_5xxx */

/* specific for 4380 & 4350*/
#if defined ( MIPS_4350 ) || defined ( MIPS_4380 )

#define CP0_CMT_INTERRUPT       $22,1           /* 4350 & 4380 */
#define CP0_CMT_CONTROL         $22,2           /* 4350 & 4380 */
#define CP0_CMT_LOCAL           $22,3           /* 4350 & 4380 */
#define CP0_EXCP_SER            $22,4           /* 4350 */
#define CP0_BRCM_CONFIG1        $22,5           /* 4350 & 4380 */
#define CP0_CORE_BASE           $22,6           /* 4350 & 4380 */
#define CP0_TAG_LO              $28
#define CP0_DATA_LO             $28,1
#endif	/*  MIPS_4350 || defined MIPS_4380 */

#endif

/* -------------------- 5xxx MIPS configuration -------------------- */
#ifdef	MIPS_5xxx		

#ifndef K0_CACHE_MODE
#define K0_CACHE_MODE			(K0_WRITE_BACK)
#endif

#define	ENABLE_LLMB			(0x1)
#define	DISABLE_LLMB			(0x0)

#define	ENABLE_L2			(0x1)
#define	DISABLE_L2			(0x0)

#ifndef LLMB_MODE
#define	LLMB_MODE			ENABLE_LLMB
#endif

#ifndef	L2_MODE
#define	L2_MODE				ENABLE_L2
#endif

#ifndef ENABLE_BRANCH_PREDICTION
#define ENABLE_BRANCH_PREDICTION	1
#endif

#ifndef ENABLE_FPU
#define ENABLE_FPU			1
#endif

#endif /* MIPS_5xxx */


/* -------------------- 4350 MIPS configuration -------------------- */
#ifdef MIPS_4350		

#ifndef K0_CACHE_MODE
#define K0_CACHE_MODE			(K0_WRITE_BACK)
#endif

#define	CORE_BASE_ADDRESS		(0xff400000)
#define	CORE_BASE_MASK			(0x3)

#define	UPPER_BOUND_ADDRESS		(0x2fff)
#define	LOWER_BOUND_ADDRESS		(0x0000)

#define	ENABLE_SYNC_MODE		(0x0)
#define	DISABLE_SYNC_MODE		(0x1)

#define	ENABLE_LLMB			(0x1)
#define	DISABLE_LLMB			(0x0)

#ifndef	ENABLE_RAC
#define	ENABLE_RAC			1
#endif

#ifndef SYNC_MODE
#define	SYNC_MODE			DISABLE_SYNC_MODE
#endif

#ifndef LLMB_MODE
#define	LLMB_MODE			ENABLE_LLMB
#endif

#ifndef ENABLE_WEAK_ORDER_CONSISTENCY_MODEL
#define ENABLE_WEAK_ORDER_CONSISTENCY_MODEL	1
#endif

#ifndef ENABLE_BRANCH_PREDICTION
#define ENABLE_BRANCH_PREDICTION	1
#endif

#ifndef ENABLE_FPU
#define ENABLE_FPU			1
#endif

#ifndef ENABLE_DMA
#define ENABLE_DMA			1
#endif

#ifndef RAC_CONFIG_PREFETCH_I_TP0
#define RAC_CONFIG_PREFETCH_I_TP0	BMIPS43xx_RAC_CONFIG_PF_I_MASK
#endif

#ifndef RAC_CONFIG_PREFETCH_I_TP1
#define RAC_CONFIG_PREFETCH_I_TP1	BMIPS43xx_RAC_CONFIG_PF_I_MASK
#endif

#ifndef RAC_CONFIG_RAC_I_TP0
#define RAC_CONFIG_RAC_I_TP0		BMIPS43xx_RAC_CONFIG_RAC_I_MASK
#endif

#ifndef RAC_CONFIG_RAC_I_TP1
#define RAC_CONFIG_RAC_I_TP1		BMIPS43xx_RAC_CONFIG_RAC_I_MASK
#endif

#ifndef RAC_CONFIG_PREFETCH_D_TP0
#define RAC_CONFIG_PREFETCH_D_TP0	BMIPS43xx_RAC_CONFIG_PF_D_MASK
#endif

#ifndef RAC_CONFIG_PREFETCH_D_TP1
#define RAC_CONFIG_PREFETCH_D_TP1	BMIPS43xx_RAC_CONFIG_PF_D_MASK
#endif

#ifndef RAC_CONFIG_RAC_D_TP0
#define RAC_CONFIG_RAC_D_TP0		BMIPS43xx_RAC_CONFIG_RAC_D_MASK
#endif

#ifndef RAC_CONFIG_RAC_D_TP1
#define RAC_CONFIG_RAC_D_TP1		BMIPS43xx_RAC_CONFIG_RAC_D_MASK
#endif

#endif /* MIPS_4350 */

/* -------------------- 4380 MIPS configuration -------------------- */
#ifdef	MIPS_4380		

#ifndef K0_CACHE_MODE
#define K0_CACHE_MODE			(K0_WRITE_BACK)
#endif

#define	CORE_BASE_ADDRESS		(0x11f00000)
#define	CORE_BASE_MASK			(0x3)

#define	UPPER_BOUND_ADDRESS		(0x2FFF)
#define	LOWER_BOUND_ADDRESS		(0x0000)

#define	ENABLE_SYNC_MODE		(0x0)
#define	DISABLE_SYNC_MODE		(0x1)

#define	ENABLE_LLMB			(0x1)
#define	DISABLE_LLMB			(0x0)

#define	ENABLE_L2			(0x1)
#define	DISABLE_L2			(0x0)

#define	CBG_MODE			(0x2)

#ifndef	ENABLE_RAC
#define	ENABLE_RAC			0
#endif

#ifndef SYNC_MODE
#define	SYNC_MODE			ENABLE_SYNC_MODE	
#endif

#ifndef LLMB_MODE
#define	LLMB_MODE			ENABLE_LLMB
#endif

#ifndef	L2_MODE
#define	L2_MODE				ENABLE_L2
#endif

#ifndef ENABLE_WEAK_ORDER_CONSISTENCY_MODEL
#define ENABLE_WEAK_ORDER_CONSISTENCY_MODEL	1
#endif

#ifndef ENABLE_BRANCH_PREDICTION
#define ENABLE_BRANCH_PREDICTION	1
#endif

#ifndef ENABLE_FPU
#define ENABLE_FPU			1
#endif

#endif /* MIPS_4380 */

#endif
