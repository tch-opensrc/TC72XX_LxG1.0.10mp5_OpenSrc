/***************************************************************************
 *     Copyright (c) 2008-2011, Broadcom Corporation
 *     All Rights Reserved
 *     Confidential Property of Broadcom Corporation
 *
 *  THIS SOFTWARE MAY ONLY BE USED SUBJECT TO AN EXECUTED SOFTWARE LICENSE
 *  AGREEMENT  BETWEEN THE USER AND BROADCOM.  YOU HAVE NO RIGHT TO USE OR
 *  EXPLOIT THIS MATERIAL EXCEPT SUBJECT TO THE TERMS OF SUCH AN AGREEMENT.
 *
 * Module Description:
 *
 * Revision History:
 *
 * 	4/4/08	v1.0	Initial drop of code to support 3380
 *
***************************************************************************/

#ifndef __BMIPS_H__
#define __BMIPS_H__

/**********************************************************************
 *  Bitfield macros
 ********************************************************************* */

/*
 * Make a mask for 1 bit at position 'n'
 */

#define _MM_MAKEMASK1(n) (1 << (n))

/*
 * Make a mask for 'v' bits at position 'n'
 */

#define _MM_MAKEMASK(v,n) (((1<<(v))-1) << (n))

/*
 * Make a value at 'v' at bit position 'n'
 */

#define _MM_MAKEVALUE(v,n) ((v) << (n))

/*
 * Retrieve a value from 'v' at bit position 'n' with 'm' mask bits
 */

#define _MM_GETVALUE(v,n,m) (((v) & (m)) >> (n))


/**********************************************************************
 *  Register Definitions.
 ********************************************************************* */

#if defined(__ASSEMBLER__)
#define zero		$0
#define	AT		$1		/* assembler temporaries */
#define	v0		$2		/* value holders */
#define	v1		$3
#define	a0		$4		/* arguments */
#define	a1		$5
#define	a2		$6
#define	a3		$7
#define	t0		$8		/* temporaries */
#define	t1		$9
#define	t2		$10
#define	t3		$11
#define	t4		$12
#define	t5		$13
#define	t6		$14
#define	t7		$15
#define ta0		$12
#define ta1		$13
#define ta2		$14
#define ta3		$15
#define	s0		$16		/* saved registers */
#define	s1		$17
#define	s2		$18
#define	s3		$19
#define	s4		$20
#define	s5		$21
#define	s6		$22
#define	s7		$23
#define	t8		$24		/* temporaries */
#define	t9		$25
#define	k0		$26		/* kernel registers */
#define	k1		$27
#define	gp		$28		/* global pointer */
#define	sp		$29		/* stack pointer */
#define	s8		$30		/* saved register */
#define	fp		$30		/* frame pointer */
#define	ra		$31		/* return address */
#endif



/**********************************************************************
 *  Macros 
 ********************************************************************* */

#define cacheop(kva, size, linesize, op) 	\
	.set noreorder			;	\
	addu		t1, kva, size   ;	\
        subu		t2, linesize, 1 ;	\
        not		t2		;	\
        and		t0, kva, t2     ;	\
        addiu		t1, t1, -1      ;    	\
        and		t1, t2          ;	\
9:	cache		op, 0(t0)       ;	\
        bne		t0, t1, 9b	;	\
        addu		t0, linesize    ;	\
        .set reorder			;		



/**********************************************************************
 *  Macros for generating assembly language routines
 ********************************************************************* */

#if defined(__ASSEMBLER__)

/* global leaf function (does not call other functions) */
#define LEAF(name)		\
  	.globl	name;		\
  	.ent	name;		\
name:

/* global alternate entry to (local or global) leaf function */
#define XLEAF(name)		\
  	.globl	name;		\
  	.aent	name;		\
name:

/* end of a global function */
#define END(name)		\
  	.size	name,.-name;	\
  	.end	name

/* local leaf function (does not call other functions) */
#define SLEAF(name)		\
  	.ent	name;		\
name:

/* local alternate entry to (local or global) leaf function */
#define SXLEAF(name)		\
  	.aent	name;		\
name:

/* end of a local function */
#define SEND(name)		\
  	END(name)

/* define & export a symbol */
#define EXPORT(name)		\
  	.globl name;		\
name:

/* import a symbol */
#define	IMPORT(name, size)	\
	.extern	name,size

/* define a zero-fill common block (BSS if not overridden) with a global name */
#define COMM(name,size)		\
	.comm	name,size

/* define a zero-fill common block (BSS if not overridden) with a local name */
#define LCOMM(name,size)		\
  	.lcomm	name,size

#endif


/************************************************************************
 * Primary Cache operations
 ************************************************************************/

#define Index_Invalidate_I               0x0         /* 0       0 */
#define Index_Writeback_Inv_D            0x1         /* 0       1 */
#define Index_Invalidate_SI              0x2         /* 0       2 */
#define Index_Writeback_Inv_SD           0x3         /* 0       3 */
#define Index_Load_Tag_I                 0x4         /* 1       0 */
#define Index_Load_Tag_D                 0x5         /* 1       1 */
#define Index_Load_Tag_SI                0x6         /* 1       2 */
#define Index_Load_Tag_SD                0x7         /* 1       3 */
#define Index_Store_Tag_I                0x8         /* 2       0 */
#define Index_Store_Tag_D                0x9         /* 2       1 */
#define Index_Store_Tag_SI               0xA         /* 2       2 */
#define Index_Store_Tag_SD               0xB         /* 2       3 */
#define Create_Dirty_Exc_D               0xD         /* 3       1 */
#define Create_Dirty_Exc_SD              0xF         /* 3       3 */
#define Hit_Invalidate_I                 0x10        /* 4       0 */
#define Hit_Invalidate_D                 0x11        /* 4       1 */
#define Hit_Invalidate_SI                0x12        /* 4       2 */
#define Hit_Invalidate_SD                0x13        /* 4       3 */
#define Fill_I                           0x14        /* 5       0 */
#define Hit_Writeback_Inv_D              0x15        /* 5       1 */
#define Hit_Writeback_Inv_SD             0x17        /* 5       3 */
#define Hit_Writeback_I                  0x18        /* 6       0 */
#define Hit_Writeback_D                  0x19        /* 6       1 */
#define Hit_Writeback_SD                 0x1B        /* 6       3 */
#define Hit_Set_Virtual_SI               0x1E        /* 7       2 */
#define Hit_Set_Virtual_SD               0x1F        /* 7       3 */


/************************************************************************
 * KSEG Mapping Definitions and Macro's
 ************************************************************************/

#define K0_BASE          0x80000000
#define K0_SIZE          0x20000000
#define K1_BASE          0xa0000000
#define K1_SIZE          0x20000000
#define K2_BASE          0xc0000000

#ifndef PHYS_TO_K0
	#define PHYS_TO_K0(x)   ((x) | 0x80000000)
#endif
#ifndef PHYS_TO_K1
	#define PHYS_TO_K1(x)   ((x) | 0xa0000000)
#endif
#ifndef K0_TO_PHYS
	#define K0_TO_PHYS(x)   ((x) & 0x1fffffff)
#endif
#ifndef K1_TO_PHYS
	#define K1_TO_PHYS(x)   (K0_TO_PHYS(x))
#endif
#ifndef K0_TO_K1
	#define K0_TO_K1(x)     ((x) | 0x20000000)
#endif
#ifndef K1_TO_K0
	#define K1_TO_K0(x)     ((x) & 0xdfffffff)
#endif

#define	T_VEC		K0_BASE			/* tlbmiss vector */
#define	X_VEC		(K0_BASE+0x80)		/* xtlbmiss vector */
#define	C_VEC		(K1_BASE+0x100)		/* cache exception vector */
#define	E_VEC		(K0_BASE+0x180)		/* exception vector */
#define	R_VEC		(K1_BASE+0x1fc00000)	/* reset vector */

#define REG_ADDR(X)	PHYS_TO_K1(BCHP_PHYSICAL_OFFSET+X)


#define K0_WRITE_THROUGH		(0x0)		
#define K0_WRITE_BACK			(0x3)
#define K0_UNCACHED			(0x0)



#endif
