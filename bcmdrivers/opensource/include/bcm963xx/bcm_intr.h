/*
<:copyright-gpl 
 Copyright 2003 Broadcom Corp. All Rights Reserved. 
 
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

#ifndef __BCM_INTR_H
#define __BCM_INTR_H

#ifdef __cplusplus
    extern "C" {
#endif

#if defined(CONFIG_BCM96328)
#include <6328_intr.h>
#endif
#if defined(CONFIG_BCM96368)
#include <6368_intr.h>
#endif
#if defined(CONFIG_BCM96816)
#include <6816_intr.h>
#endif
#if defined(CONFIG_BCM96362)
#include <6362_intr.h>
#endif
#if defined(CONFIG_BCM93380)
#include <3380_intr.h>
#endif
#if defined(CONFIG_BCMKILAUEA)
#include <kilauea_intr.h>
#endif
#if defined(CONFIG_BCM93383)
#include <3383_intr.h>
#endif
// The KConfig, create CONFIG_BCM3384, NOT CONFIG_BCM93384
#if defined(CONFIG_BCM3384)
#include <3384_intr.h>
#endif

/* defines */
/* The following definitions must match the definitions in linux/interrupt.h for 
   irqreturn_t and irq_handler_t */

typedef enum {  
    BCM_IRQ_NONE = 0,
    BCM_IRQ_HANDLED = 1,
    BCM_IRQ_WAKE_THREAD = 2
} FN_HANDLER_RT;
typedef FN_HANDLER_RT (*FN_HANDLER) (int, void *);


/* prototypes */
extern void enable_brcm_irq(unsigned int irq);
extern void disable_brcm_irq(unsigned int irq);
extern unsigned int BcmHalMapInterrupt(FN_HANDLER isr, unsigned int param, unsigned int interruptId);
extern void dump_intr_regs(void);

/* compatibility definitions */
#define BcmHalInterruptEnable(irq)      enable_brcm_irq( irq )
#define BcmHalInterruptDisable(irq)     disable_brcm_irq( irq )

#ifdef __cplusplus
    }
#endif

#endif
