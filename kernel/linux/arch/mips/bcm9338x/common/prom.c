/*
  <:copyright-gpl 
  Copyright 2004 Broadcom Corp. All Rights Reserved. 
 
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
/*
 * prom.c: PROM library initialization code.
 *
 */
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/bootmem.h>
#include <linux/blkdev.h>
#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <asm/time.h>

#include <bcm_map_part.h>
#include <bcm_cpu.h>
#include <board.h>
#include <boardparms.h>

extern int  do_syslog(int, char *, int);
extern NVRAM_DATA bootNvramData;

#if defined (CONFIG_BCM_BCMDSP_MODULE)
unsigned int main_tp_num;
#endif

void __init create_root_nfs_cmdline( char *cmdline );
UINT32 __init calculateCpuSpeed(void);


const char *get_system_type(void)
{
#if defined(CONFIG_BCM93380)
	return( "BCM93380 cable modem reference design" );
#elif defined(CONFIG_BCM93383)
	return( "BCM93383 reference design" );
#else
    return( bootNvramData.szBoardId );
#endif
}


/* --------------------------------------------------------------------------
   Name: prom_init
   -------------------------------------------------------------------------- */

extern struct plat_smp_ops brcm_smp_ops;

void __init prom_init(void)
{
    unsigned int prom_argc;
    char ** prom_argv;

    prom_argc = fw_arg0;
    prom_argv = (char **) fw_arg1;

#if defined(BCM93383) && defined(CONFIG_BCM_RAMDISK)
    kerSysEarlyFlashInit();
#endif
#if defined(CONFIG_PRINTK)
    do_syslog(8, NULL, 8);
#endif

    printk( "%s prom init\n", get_system_type() );
    printk("args: %d 0x%08x\n", prom_argc, prom_argv);


#if defined(CONFIG_BCM_BCMDSP_MODULE)
    main_tp_num = ((read_c0_diag3() & CP0_CMT_TPID) == CP0_CMT_TPID) ? 1 : 0;
    printk("Linux TP ID = %u \n", (unsigned int)main_tp_num);
#endif
	
#if !defined(CONFIG_BCM93383)
    PERF->IrqMask = 0;
#endif

    arcs_cmdline[0] = '\0';

	/* if user passes kernel args, ignore the default one */
	if (prom_argc > 1)
	{
		strlcpy(arcs_cmdline, prom_argv[1], sizeof(arcs_cmdline));
	}
	else
	{

#if defined(CONFIG_ROOT_NFS)
		create_root_nfs_cmdline( arcs_cmdline );
#elif defined(CONFIG_ROOT_FLASHFS)
		strcpy(arcs_cmdline, CONFIG_ROOT_FLASHFS);
#endif
		strcat(arcs_cmdline, " ");
		strcat(arcs_cmdline, CONFIG_CMDLINE);
    }

    /* Count register increments every other clock */
    mips_hpt_frequency = calculateCpuSpeed() / 2;

#if defined (CONFIG_SMP)
    register_smp_ops(&brcm_smp_ops);
#endif
}


/* --------------------------------------------------------------------------
   Name: prom_free_prom_memory
   Abstract: 
   -------------------------------------------------------------------------- */
void __init prom_free_prom_memory(void)
{

}


#if defined(CONFIG_ROOT_NFS)
/* This function reads in a line that looks something like this:
 *
 *
 * CFE bootline=bcmEnet(0,0)host:vmlinux e=192.169.0.100:ffffff00 h=192.169.0.1
 *
 *
 * and retuns in the cmdline parameter some that looks like this:
 *
 * CONFIG_CMDLINE="root=/dev/nfs nfsroot=192.168.0.1:/opt/targets/96345R/fs
 * ip=192.168.0.100:192.168.0.1::255.255.255.0::eth0:off rw"
 */
#define BOOT_LINE_ADDR   0x0
#define HEXDIGIT(d) ((d >= '0' && d <= '9') ? (d - '0') : ((d | 0x20) - 'W'))
#define HEXBYTE(b)  (HEXDIGIT((b)[0]) << 4) + HEXDIGIT((b)[1])

void __init create_root_nfs_cmdline( char *cmdline )
{
    char root_nfs_cl[] = "root=/dev/nfs nfsroot=%s:" CONFIG_ROOT_NFS_DIR
        " ip=%s:%s::%s::eth0:off rw";

    char *localip = NULL;
    char *hostip = NULL;
    char mask[16] = "";
    char bootline[128] = "";
    char *p = bootline;

    memcpy(bootline, bootNvramData.szBootline, sizeof(bootline));

    while( *p )
    {
        if( p[0] == 'e' && p[1] == '=' )
        {
            /* Found local ip address */
            p += 2;
            localip = p;
            while( *p && *p != ' ' && *p != ':' )
                p++;
            if( *p == ':' )
            {
                /* Found network mask (eg FFFFFF00 */
                *p++ = '\0';
                sprintf( mask, "%u.%u.%u.%u", HEXBYTE(p), HEXBYTE(p + 2),
						 HEXBYTE(p + 4), HEXBYTE(p + 6) );
                p += 4;
            }
            else if( *p == ' ' )
                *p++ = '\0';
        }
        else if( p[0] == 'h' && p[1] == '=' )
        {
            /* Found host ip address */
            p += 2;
            hostip = p;
            while( *p && *p != ' ' )
                p++;
            if( *p == ' ' )
				*p++ = '\0';
        }
        else 
            p++;
    }

    if( localip && hostip ) 
        sprintf( cmdline, root_nfs_cl, hostip, localip, hostip, mask );
}
#endif

/*  *********************************************************************
 *  calculateCpuSpeed()
 *      Calculate the BCM63xx CPU speed by reading the PLL Config register
 *      and applying the following formula:
 *      Fcpu_clk = (25 * MIPSDDR_NDIV) / MIPS_MDIV
 *  Input parameters:
 *      none
 *  Return value:
 *      none
 ********************************************************************* */

#if defined(CONFIG_BCM93380)
UINT32 __init calculateCpuSpeed(void)
{
    return 333 * 1000 * 1000;
}
#endif

#if defined (CONFIG_BCM93380) || defined (CONFIG_BCM93383)
#if defined(CONFIG_BCM_RAMDISK)
static unsigned int part_offset = 0;
static unsigned int part_size = 0;
#else
static unsigned int part_offset = 0x000800000;
static unsigned int part_size = 0x001800000;
static unsigned int fpt_addr =0x83fffe00; 
#endif
static int __init partoffset(char *str)
{
	get_option(&str, &part_offset);
	return 1;
}

__setup("partoffset=", partoffset);

static int __init partsize(char *str)
{
	get_option(&str, &part_size);
	return 1;
}

__setup("partsize=", partsize);

#if !defined(CONFIG_BCM_RAMDISK)

static int __init fptaddr(char *str)
{
	get_option(&str, &fpt_addr);
	return 1;
}

__setup("fptaddr=", fptaddr);
#endif

unsigned int get_part_offset(void)
{
	return part_offset;
}
EXPORT_SYMBOL(get_part_offset);

unsigned int get_part_size(void)
{
	return part_size;
}
EXPORT_SYMBOL(get_part_size);

#if !defined(CONFIG_BCM_RAMDISK)
unsigned int get_fpt_addr(void)
{
	return fpt_addr;
}
EXPORT_SYMBOL(get_fpt_addr);
#endif
#endif

#if defined (CONFIG_BCM93380)
#if defined(CONFIG_BCM_LOT1)
void prom_putchar(char c)
{
    unsigned int tx_level;

    while(1)
    {
        //tx_level = *(volatile unsigned int *)0xb4e00208;
        tx_level = *(volatile unsigned int *)0xb4e00228;
        tx_level = tx_level >> 24;
        tx_level &= 0x1f;
        if(tx_level < 14)
            break;
    }

    //*(volatile unsigned int *)0xb4e00214 = (unsigned int)c;
    *(volatile unsigned int *)0xb4e00234 = (unsigned int)c;
}
#else
void prom_putchar(char c)
{
    unsigned int tx_level;

    while(1)
    {
        tx_level = *(volatile unsigned int *)0xb4e00208;
        tx_level = tx_level >> 24;
        tx_level &= 0x1f;
        if(tx_level < 14)
            break;
    }

    *(volatile unsigned int *)0xb4e00214 = (unsigned int)c;
}
#endif
#endif

#if defined(CONFIG_BCM93383)
UINT32 __init calculateCpuSpeed(void)
{
    unsigned int rev = *(unsigned int *)0xb4e001ac;

    if(rev == 0x47)
    {
#if defined(CONFIG_BCM_LOT1)
        return 500 * 1000 * 1000;
#else
        return 200 * 1000 * 1000;
#endif
    }
    else
    {
#if defined(CONFIG_BCM_LOT1)
        return 6372 * 100 * 1000;
#else
        return 3186 * 100 * 1000;
#endif
    }
}

void prom_putchar(char c)
{
    unsigned int tx_level;

    while(1)
    {
        tx_level = *(volatile unsigned int *)0xb4e00528;
        tx_level = tx_level >> 24;
        tx_level &= 0x1f;
        if(tx_level < 14)
            break;
    }

    *(volatile unsigned int *)0xb4e00534 = (unsigned int)c;
}
#endif

