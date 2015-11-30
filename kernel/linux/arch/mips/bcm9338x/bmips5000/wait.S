/*
 * Copyright (C) 2011 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/mipsregs.h>
#include <asm/cacheops.h>
#include <asm/addrspace.h>
#include <asm/brcmstb/brcmstb.h>

	.text
	.set		noreorder
	.align		5
	.global		wait_forever

/***********************************************************************
 * Wait for interrupt
 ***********************************************************************/
wait_forever:
	mfc0	t0, CP0_STATUS
	li	t1, ~(ST0_IM | ST0_IE)
	and	t0, t1
	mtc0	t0, CP0_STATUS
	nop
	nop
	nop
	wait
	nop
