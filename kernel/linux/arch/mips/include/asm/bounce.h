#ifndef __BOUNCE_H_INCLUDED__
#define __BOUNCE_H_INCLUDED__

#if defined(CONFIG_BRCM_BOUNCE) || defined(CONFIG_BOUNCE)
/*
<:copyright-broadcom

 Copyright (c) 2007 Broadcom Corporation
 All Rights Reserved
 No portions of this material may be reproduced in any form without the
 written permission of:
          Broadcom Corporation
          5300 California Avenue
          Irvine, California 92617
 All information contained in this document is Broadcom Corporation
 company private, proprietary, and trade secret.

:>
*/
/*
 *******************************************************************************
 * File Name   : bounce.h
 * Description : Tracing function call entry and exit, using compiler support
 *				 for instrumenting function entry and exit.
 *				 The GCC -finstrument-functions compiler option enables this.
 *
 *				 Files that need to be instrumented may be compiled with the
 *				 compiler option -finstrument-functions via the Makefile.
 *
 *				 To disable instrumentation of specific functions in a file
 *				 that is compiled with the option -finstrument-functions, you
 *				 may append __attribute__ ((no_instrument_function)) to it's
 *				 definition, e.g.
 *				 	void hello( void ) __attribute__ ((no_instrument_function));
 *
 *				 You may enable tracing by invoking bounce_up().
 *
 *				 Two modes of tracing are defined:
 *				 - Continuous tracing with an EXPLICIT bounce_dn() to stop.
 *				 - Auto stop, when a limited number of functions are logged.
 *                 bounce_dn() may also be invoked to stop in this mode.
 *
 *				 The collected trace is retained until the next start.
 *******************************************************************************
 */
#ifndef __ASSEMBLY__

#if defined(__cplusplus)
extern "C" {
#endif

#if defined( __KERNEL__ )
#include <linux/types.h>        /* ISO C99 7.18 Integer types */
#else
#include <stdint.h>             /* ISO C99 7.18 Integer types */
#endif

#define BOUNCE_ERROR				(-1)

#define BOUNCE_NOINSTR __attribute__((no_instrument_function))

#if defined(CONFIG_BOUNCE_EXIT)
#define BOUNCE_SIZE					(128*1024)
#define BOUNCE_PANIC				10000
#else
#define BOUNCE_SIZE					(256*1024)
#define BOUNCE_PANIC				20000
#endif

#define BOUNCE_COLOR

//#define BOUNCE_DEBUG
#ifdef BOUNCE_DEBUG
#define BDBG(code)      			code
#else
#define BDBG(code)					do {} while(0)
#endif

#define BOUNCE_VERSION(a,b,c)		(((a) << 16) + ((b) << 8) + ((c) << 0))
#define BOUNCE_VER_A(version)		((version >>16) & 0xff)
#define BOUNCE_VER_B(version)		((version >> 8) & 0xff)
#define BOUNCE_VER_C(version)		((version >> 0) & 0xff)

#define BOUNCE_DEV_VERSION			(BOUNCE_VERSION(01,00,00))
#define BOUNCE_CTL_VERSION			(BOUNCE_VERSION(01,00,00))

    /* Device name in : /proc/devices */
#define BOUNCE_DEV_NAME				"bounce"
#define BOUNCE_DEV_PATH          	"/dev/" BOUNCE_DEV_NAME
#define BOUNCE_DEV_MAJ           	213

#undef  BOUNCE_DECL
#define BOUNCE_DECL(x)				x,

typedef enum bounceMode
{
	BOUNCE_DECL(BOUNCE_MODE_DISABLED)
	BOUNCE_DECL(BOUNCE_MODE_CONTINUOUS)	/* explicit disable via bounce_dn() */
	BOUNCE_DECL(BOUNCE_MODE_LIMITED)	/* auto disable when count goes to 0 */
    BOUNCE_DECL(BOUNCE_MODE_MAXIMUM)
} BounceMode_t;

typedef enum bounceIoctl
{
	BOUNCE_DECL(BOUNCE_START_IOCTL)
	BOUNCE_DECL(BOUNCE_STOP_IOCTL)
	BOUNCE_DECL(BOUNCE_DUMP_IOCTL)
	BOUNCE_DECL(BOUNCE_INVLD_IOCTL)
} BounceIoctl_t;


#ifdef __KERNEL__

#define BOUNCE_ADDR_MASK			(0xFFFFFFFE)

typedef struct bounceLog
{
	union {
		void * func;

		struct {
			uint32_t addr	: 31;
			uint32_t type	:  1;	/* entry=1 or exit=0 */
		} site;

	} ced;							/* called function */

	uint32_t pid; 					/* task context */
} BounceLog_t;


extern void	bounce_up(BounceMode_t mode, unsigned int limit);
extern void bounce_dn(void);
extern void bounce_panic(void);
extern void	bounce_dump(unsigned int last);

extern void __cyg_profile_func_enter(void *ced, void *cer) BOUNCE_NOINSTR;
extern void __cyg_profile_func_exit( void *ced, void *cer) BOUNCE_NOINSTR;

#define			BOUNCE_LOGK(func)	__cyg_profile_func_enter((void*)func,	\
								 			__builtin_return_address(0))
#endif	/* defined __KERNEL__ */

#if defined(__cplusplus)
}
#endif

#endif  /* defined __ASSEMBLY__ */


#else	/* !defined(CONFIG_BRCM_BOUNCE) */

#define			bounce_up(m,l)		do {} while(0)
#define			bounce_dn()			do {} while(0)
#define			bounce_dump(l)		do {} while(0)
#define			bounce_panic()		do {} while(0)

#define			BOUNCE_LOGK(f)		do {} while(0)

#endif	/* #defined(CONFIG_BRCM_BOUNCE) */

#endif	/* !defined(__BOUNCE_H_INCLUDED__) */

