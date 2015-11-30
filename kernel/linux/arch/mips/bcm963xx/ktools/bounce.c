#if defined(CONFIG_BRCM_BOUNCE)
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
 * File Name   : bounce.c
 *******************************************************************************
 */

#warning "[0;31m                +++++++++ Compiling bounce.c +++++++++[0m"

#include <asm/bounce.h>
#include <linux/sched.h>
#include <linux/autoconf.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/fs.h>

#ifdef BOUNCE_COLOR
#define _H_						"\e[0;36;44m"
#define _N_						"\e[0m"
#define _R_						"\e[0;31m"
#define _G_						"\e[0;32m"
#else
#define _H_
#define _N_
#define _R_
#define _G_
#endif

#undef  BOUNCE_DECL
#define BOUNCE_DECL(x)			#x,

/*----- typedefs -----*/
typedef struct bounceDev
{

	BounceMode_t	mode;		/* mode of operation */
	BounceLog_t	  * log_p;

	uint32_t		wrap;		/* buffer wrapped at least once */
	uint32_t		run;		/* trace incarnation */
	uint32_t		count;		/* log count .. (not function count) */

	dev_t			dev;
	struct cdev		cdev;

	BounceLog_t		log[ BOUNCE_SIZE ];

} BounceDev_t;

/*----- Forward definition -----*/

static int bounce_open(struct inode *inodep, struct file *filep)BOUNCE_NOINSTR;
static int bounce_rel(struct inode *inodep, struct file *filep)	BOUNCE_NOINSTR;
static int bounce_ioctl(struct inode *inodep, struct file *filep,
						uint32_t command, unsigned long arg)	BOUNCE_NOINSTR;
extern void bounce_up(BounceMode_t mode, uint32_t limit)		BOUNCE_NOINSTR;
extern asmlinkage void bounce_dn(void);
extern asmlinkage void bounce_panic(void);
extern void bounce_dump(uint32_t last)							BOUNCE_NOINSTR;

static int  __init bounce_init(void)							BOUNCE_NOINSTR;
static void __exit bounce_exit(void)							BOUNCE_NOINSTR;

/*----- Globals -----*/

BounceDev_t bounce_g = { .mode = BOUNCE_MODE_DISABLED };

static struct file_operations bounce_fops_g =
{
	.ioctl =    bounce_ioctl,
	.open =     bounce_open,
	.release =  bounce_rel,
	.owner =    THIS_MODULE
};

static const char * bounce_mode_str_g[] =
{
    BOUNCE_DECL(BOUNCE_MODE_DISABLED)
    BOUNCE_DECL(BOUNCE_MODE_LIMITED)    /* auto disable when count goes to 0 */
    BOUNCE_DECL(BOUNCE_MODE_CONTINUOUS) /* explicit disable via bounce_dn() */
    BOUNCE_DECL(BOUNCE_MODE_MAXIMUM)
};

#ifdef BOUNCE_DEBUG
static const char * bounce_ioctl_str_g[] =
{
    BOUNCE_DECL(BOUNCE_START_IOCTL)
    BOUNCE_DECL(BOUNCE_STOP_IOCTL)
    BOUNCE_DECL(BOUNCE_DUMP_IOCTL)
    BOUNCE_DECL(BOUNCE_INVLD_IOCTL)
};
#endif

/* MACROS used by __cyg_profile_func_ enter() and exit() */
#define __BOUNCE_BGN()												\
																	\
	unsigned long flags;											\
																	\
	if ( bounce_g.mode == BOUNCE_MODE_DISABLED )					\
		return;														\
																	\
	local_irq_save(flags);											\
	if ( bounce_g.mode == BOUNCE_MODE_LIMITED )						\
	{																\
		if ( bounce_g.count == 0 )									\
		{															\
			bounce_g.mode = BOUNCE_MODE_DISABLED;					\
			local_irq_restore( flags );								\
			return;													\
		}															\
		bounce_g.count--;											\
	}


#define __BOUNCE_END()												\
																	\
	bounce_g.log_p++;												\
																	\
	if ( bounce_g.log_p == &bounce_g.log[ BOUNCE_SIZE ] )			\
	{																\
		bounce_g.wrap = 1;											\
		bounce_g.log_p = &bounce_g.log[0];							\
	}																\
																	\
	local_irq_restore( flags )


/* Function entry stub providied by -finstrument-functions */
void __cyg_profile_func_enter(void *ced, void *cer)
{
	__BOUNCE_BGN();

	bounce_g.log_p->ced.func = ced;
	bounce_g.log_p->ced.site.type = 1;		/* called function entry */

	bounce_g.log_p->pid = (uint32_t)(current_thread_info()->task->pid);

	__BOUNCE_END();
}

/* Function exit stub providied by -finstrument-functions */
void __cyg_profile_func_exit(void *ced, void *cer)
{
#if defined(CONFIG_BRCM_BOUNCE_EXIT)
	__BOUNCE_BGN();

	bounce_g.log_p->ced.func = ced;
	bounce_g.log_p->ced.site.type = 0;		/* called function exit */

	bounce_g.log_p->pid = (uint32_t)(current_thread_info()->task->pid);

	__BOUNCE_END();

#endif	/* defined(CONFIG_BRCM_BOUNCE_EXIT) */
}

static int bounce_panic_dump = 0;

/* Start tracing */
void bounce_up(BounceMode_t mode, uint32_t limit)
{
	bounce_g.wrap = 0;						/* setup trace buffer */
	bounce_g.log_p = &bounce_g.log[0];
	bounce_g.count = limit;					/* setup stop semantics */
	bounce_g.mode = mode;					/* tracing enabled now */

	bounce_panic_dump = 1;
}

/* Stop tracing */
void bounce_dn(void)
{
	BOUNCE_LOGK(bounce_dn);

    if ( bounce_g.mode != BOUNCE_MODE_DISABLED )
		bounce_g.mode = BOUNCE_MODE_LIMITED;/* initiate stop semantics */
}

/* Auto dump last BOUNCE_PANIC items on a panic/bug */
void bounce_panic(void)
{
	BOUNCE_LOGK(bounce_dn);

	if ( bounce_panic_dump ) {
		bounce_panic_dump = 0;
		bounce_g.mode = BOUNCE_MODE_DISABLED;
		bounce_dump( BOUNCE_PANIC );
	}
}

/* Dump the trace buffer via printk */
void bounce_dump(uint32_t last)
{
	BounceLog_t * log_p;
	uint32_t logs;
	uint32_t wrap;
	uint32_t count;
	BounceMode_t mode;

	count = bounce_g.count;
	bounce_g.count = 0;

	mode = bounce_g.mode;
	bounce_g.mode  = BOUNCE_MODE_DISABLED;

	printk(_H_ "BOUNCE DUMP BGN: FUNC_EXIT<%d> run<%u> wrap<%u> count<%u> %s\n"
	       "B[0x%08x] L[0x%08x] E[0x%08x], %u:%u bounce_dn[<0x%08x>]\n\n" _N_,
#if defined(CONFIG_BRCM_BOUNCE_EXIT)
			1,
#else
			0,
#endif
		    bounce_g.run, bounce_g.wrap, count, bounce_mode_str_g[mode],
            (int)&bounce_g.log[0],
            (int)bounce_g.log_p, (int)&bounce_g.log[BOUNCE_SIZE],
			(((uint32_t)bounce_g.log_p - (uint32_t)&bounce_g.log[0])
            / sizeof(BounceLog_t)),
			(((uint32_t)(&bounce_g.log[BOUNCE_SIZE])
            - (uint32_t)bounce_g.log_p) / sizeof(BounceLog_t)),
            (int)bounce_dn );

	/* Dump the last few records */
	if ( last != 0 )
	{
		uint32_t items;

		if ( last > BOUNCE_SIZE )
			last = BOUNCE_SIZE;

		items = (((uint32_t)bounce_g.log_p - (uint32_t)&bounce_g.log[0])
				 / sizeof(BounceLog_t));

		if ( items > last )
		{
			log_p = (BounceLog_t*)
				((uint32_t)bounce_g.log_p - (last * sizeof(BounceLog_t)));
			wrap = 0;
		}
		else
		{
			items = last - items; 	/* remaining items */
			log_p = (BounceLog_t*)
				((uint32_t)(&bounce_g.log[BOUNCE_SIZE]
				 - (items * sizeof(BounceLog_t))));
			wrap = 1;
		}
	}
	else
	{
		wrap = bounce_g.wrap;
		if ( bounce_g.wrap )
			log_p = bounce_g.log_p;
		else
			log_p = & bounce_g.log[0];
	}

	logs = 0;

	if ( wrap )
	{
		for ( ; log_p != & bounce_g.log[BOUNCE_SIZE]; logs++, log_p++ )
		{
			printk( "%s %5u [<%08x>]" _N_ "\n",
					(log_p->ced.site.type) ? _R_ "=>" : _G_ "<=",
					log_p->pid, (((int)(log_p->ced.func)) & BOUNCE_ADDR_MASK)
					);
		}

		log_p = & bounce_g.log[0];
	}

	for ( ; log_p != bounce_g.log_p; logs++, log_p++ )
	{
		printk( "%s %5u [<%08x>]" _N_ "\n",
				(log_p->ced.site.type) ? _R_ "=>" : _G_ "<=",
				log_p->pid, (((int)(log_p->ced.func)) & BOUNCE_ADDR_MASK)
				);
	}

	printk( _H_ "\nBOUNCE DUMP END: logs<%u>\n\n\n" _N_, logs );
}

/* ioctl fileops */
int bounce_ioctl(struct inode *inodep, struct file *filep,
				 uint32_t command, unsigned long arg)
{
	BounceIoctl_t cmd;

	if ( command > BOUNCE_INVLD_IOCTL )
		cmd = BOUNCE_INVLD_IOCTL;
	else
		cmd = (BounceIoctl_t)command;

	BDBG( printk(KERN_DEBUG "BOUNCE DEV: ioctl cmd[%d,%s] arg[%lu 0x%08x]\n",
		         command, bounce_ioctl_str_g[cmd], arg, (int)arg ); );

	switch ( cmd )
	{
		case BOUNCE_START_IOCTL:
			{
				BounceMode_t mode = (BounceMode_t) ( arg & 7 );
				uint32_t limit = ( arg >> 3 );

				bounce_up( mode, limit );
				break;
			}
				
		case BOUNCE_STOP_IOCTL:	bounce_dn(); break;

		case BOUNCE_DUMP_IOCTL:	bounce_dump(arg); break;

		default:
			printk( KERN_ERR "BOUNCE DEV: invalid ioctl <%u>\n", command );
			return -EINVAL;
	}

	return 0;
}

/* open fileops */
int bounce_open(struct inode *inodep, struct file *filep)
{
	int minor = MINOR(inodep->i_rdev) & 0xf;    /* fetch minor */

	if (minor > 0)
	{
		printk(KERN_WARNING "BOUNCE DEV: multiple open " BOUNCE_DEV_NAME);
		return -ENODEV;
	}
	return 0;
}

/* release fileops */
int bounce_rel(struct inode *inodep, struct file *filep)
{
	return 0;
}

/* module init: register character device */
int __init bounce_init(void)
{
	int ret;
	memset(&bounce_g, 0, sizeof(BounceDev_t));
	bounce_g.mode  = BOUNCE_MODE_DISABLED;
	bounce_g.count = BOUNCE_SIZE;
	bounce_g.log_p = &bounce_g.log[0];

	bounce_g.dev = MKDEV(BOUNCE_DEV_MAJ, 0);

	cdev_init(&bounce_g.cdev, &bounce_fops_g);
	bounce_g.cdev.ops = &bounce_fops_g;

	ret = cdev_add(&bounce_g.cdev, bounce_g.dev, 1);

	if (ret) {
		printk( KERN_ERR _R_ "BOUNCE DEV: Error %d adding device "
				BOUNCE_DEV_NAME " [%d,%d] added.\n" _N_,
				ret, MAJOR(bounce_g.dev), MINOR(bounce_g.dev));
		return ret;
	} else {
		printk( KERN_DEBUG _G_ "BOUNCE DEV: "
				BOUNCE_DEV_NAME " [%d,%d] added.\n" _N_,
				MAJOR(bounce_g.dev), MINOR(bounce_g.dev));
	}

	return ret;
}

/* cleanup : did not bother with char device de-registration */
void __exit bounce_exit(void)
{
	cdev_del(&bounce_g.cdev);
	memset(&bounce_g, 0, sizeof(BounceDev_t));
}

module_init(bounce_init);
module_exit(bounce_exit);

EXPORT_SYMBOL(__cyg_profile_func_enter);
EXPORT_SYMBOL(__cyg_profile_func_exit);

EXPORT_SYMBOL(bounce_up);
EXPORT_SYMBOL(bounce_dn);
EXPORT_SYMBOL(bounce_dump);
EXPORT_SYMBOL(bounce_panic);

#endif
