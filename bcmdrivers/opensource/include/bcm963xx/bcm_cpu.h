/*
<:copyright-gpl 
 Copyright 2006 Broadcom Corp. All Rights Reserved. 
 
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

#ifndef __BCM_CPU_H
#define __BCM_CPU_H

#if defined(CONFIG_BCM96328)
#include <6328_cpu.h>
#endif
#if defined(CONFIG_BCM96368)
#include <6368_cpu.h>
#endif
#if defined(CONFIG_BCM96816)
#include <6816_cpu.h>
#endif
#if defined(CONFIG_BCM96362)
#include <6362_cpu.h>
#endif

#endif

