/*
<:copyright-gpl
 Copyright 2002 Broadcom Corp. All Rights Reserved.

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
 ***************************************************************************
 * File Name  : bcm63xx_flash.c
 *
 * Description: This file contains the flash device driver APIs for bcm63xx board. 
 *
 * Created on :  8/10/2002  seanl:  use cfiflash.c, cfliflash.h (AMD specific)
 *
 ***************************************************************************/


/* Includes. */
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/preempt.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/mtd/mtd.h>
#include <linux/kernel.h>
#include <linux/fcntl.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/jffs2.h>
#include <linux/mount.h>
#include <asm/uaccess.h>
#include <asm/delay.h>

#include <bcm_map_part.h>
#include <board.h>
#if defined(CONFIG_BCM93378) || defined(CONFIG_BCM93380) || defined(CONFIG_BCMKILAUEA) || defined(CONFIG_BCM93383)
#include <ProgramStore.h>
#else
#include <bcmTag.h>
#endif
#include "flash_api.h"
#include "boardparms.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
#include <linux/fs_struct.h>
#endif
// #define DEBUG_FLASH

#if !defined(CONFIG_BCM93378) && !defined(CONFIG_BCM93380) && !defined(CONFIG_BCMKILAUEA) && !defined(CONFIG_BCM93383)
extern PFILE_TAG kerSysImageTagGet(void);
extern int jffs2_remount_fs (struct super_block *sb, int *flags, char *data);
int fsync_super(struct super_block *sb);

NVRAM_DATA bootNvramData;
static FLASH_ADDR_INFO fInfo;
static struct semaphore semflash;
static char bootCfeVersion[CFE_VERSION_MARK_SIZE+CFE_VERSION_SIZE];
static int bootFromNand = 0;

static int setScratchPad(char *buf, int len);
static char *getScratchPad(int len);

#define ALLOC_TYPE_KMALLOC   0
#define ALLOC_TYPE_VMALLOC   1

void *retriedKmalloc(size_t size)
{
    void *pBuf;
    unsigned char *bufp8 ;

    size += 4 ; /* 4 bytes are used to store the housekeeping information used for freeing */

    // Memory allocation changed from kmalloc() to vmalloc() as the latter is not susceptible to memory fragmentation under low memory conditions
    // We have modified Linux VM to search all pages by default, it is no longer necessary to retry here
    if (!in_interrupt() ) {
        pBuf = vmalloc(size);
        if (pBuf) {
            memset(pBuf, 0, size);
            bufp8 = (unsigned char *) pBuf ;
            *bufp8 = ALLOC_TYPE_VMALLOC ;
            pBuf = bufp8 + 4 ;
        }
    }
    else { // kmalloc is still needed if in interrupt
        pBuf = kmalloc(size, GFP_ATOMIC);
        if (pBuf) {
            memset(pBuf, 0, size);
            bufp8 = (unsigned char *) pBuf ;
            *bufp8 = ALLOC_TYPE_KMALLOC ;
            pBuf = bufp8 + 4 ;
        }
    }

    return pBuf;
}

void retriedKfree(void *pBuf)
{
    unsigned char *bufp8  = (unsigned char *) pBuf ;
    bufp8 -= 4 ;

    if (*bufp8 == ALLOC_TYPE_KMALLOC)
        kfree(bufp8);
    else
        vfree(bufp8);
}

// get shared blks into *** pTempBuf *** which has to be released bye the caller!
// return: if pTempBuf != NULL, poits to the data with the dataSize of the buffer
// !NULL -- ok
// NULL  -- fail
static char *getSharedBlks(int start_blk, int num_blks)
{
    int i = 0;
    int usedBlkSize = 0;
    int sect_size = 0;
    char *pTempBuf = NULL;
    char *pBuf = NULL;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    /* not allowed to ignore the return of down_interruptible in new kernel */
    if (down_interruptible(&semflash) )
    {
        return NULL;
    }
#else
    down_interruptible(&semflash);
#endif

    for (i = start_blk; i < (start_blk + num_blks); i++)
        usedBlkSize += flash_get_sector_size((unsigned short) i);

    if ((pTempBuf = (char *) retriedKmalloc(usedBlkSize)) == NULL)
    {
        printk("failed to allocate memory with size: %d\n", usedBlkSize);
        up(&semflash);
        return pTempBuf;
    }
    
    pBuf = pTempBuf;
    for (i = start_blk; i < (start_blk + num_blks); i++)
    {
        sect_size = flash_get_sector_size((unsigned short) i);

#if defined(DEBUG_FLASH)
        printk("getSharedBlks: blk=%d, sect_size=%d\n", i, sect_size);
#endif
        flash_read_buf((unsigned short)i, 0, pBuf, sect_size);
        pBuf += sect_size;
    }
    up(&semflash);
    
    return pTempBuf;
}

// Set the pTempBuf to flash from start_blk for num_blks
// return:
// 0 -- ok
// -1 -- fail
static int setSharedBlks(int start_blk, int num_blks, char *pTempBuf)
{
    int i = 0;
    int sect_size = 0;
    int sts = 0;
    char *pBuf = pTempBuf;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    /* not allowed to ignore the return of down_interruptible in new kernel */
    if (down_interruptible(&semflash) )
    {
        return -1;
    }
#else
    down_interruptible(&semflash);
#endif

    for (i = start_blk; i < (start_blk + num_blks); i++)
    {
        sect_size = flash_get_sector_size((unsigned short) i);
        flash_sector_erase_int(i);

        if (flash_write_buf(i, 0, pBuf, sect_size) != sect_size)
        {
            printk("Error writing flash sector %d.", i);
            sts = -1;
            break;
        }

#if defined(DEBUG_FLASH)
        printk("setShareBlks: blk=%d, sect_size=%d\n", i, sect_size);
#endif

        pBuf += sect_size;
    }

    up(&semflash);

    return sts;
}
#endif

#if !defined(CONFIG_BRCM_IKOS)
// Initialize the flash and fill out the fInfo structure
void kerSysEarlyFlashInit( void )
{
    flash_init();
#if !defined(CONFIG_BCM93380) && !defined(CONFIG_BCMKILAUEA) && !defined(CONFIG_BCM93383)
    bootFromNand = 0;

#if defined(CONFIG_BCM96368)
    if( ((GPIO->StrapBus & MISC_STRAP_BUS_BOOT_SEL_MASK) >>
        MISC_STRAP_BUS_BOOT_SEL_SHIFT) == MISC_STRAP_BUS_BOOT_NAND)
        bootFromNand = 1;
#elif defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM96816)
    if( ((MISC->miscStrapBus & MISC_STRAP_BUS_BOOT_SEL_MASK) >>
        MISC_STRAP_BUS_BOOT_SEL_SHIFT) == MISC_STRAP_BUS_BOOT_NAND )
        bootFromNand = 1;
#endif

#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM96816)
    if( bootFromNand == 1 )
    {
        NAND->NandNandBootConfig = NBC_AUTO_DEV_ID_CFG | 0x101;
        NAND->NandCsNandXor = 1;

        memcpy((unsigned char *)&bootCfeVersion, (unsigned char *)
            FLASH_BASE + CFE_VERSION_OFFSET, sizeof(bootCfeVersion));
        memcpy((unsigned char *)&bootNvramData, (unsigned char *)
            FLASH_BASE + NVRAM_DATA_OFFSET, sizeof(NVRAM_DATA));

        NAND->NandNandBootConfig = NBC_AUTO_DEV_ID_CFG | 0x2;
        NAND->NandCsNandXor = 0;
    }
    else
#endif
    {
        fInfo.flash_rootfs_start_offset = flash_get_sector_size(0);
        if( fInfo.flash_rootfs_start_offset < FLASH_LENGTH_BOOT_ROM )
            fInfo.flash_rootfs_start_offset = FLASH_LENGTH_BOOT_ROM;
     
        flash_read_buf (NVRAM_SECTOR, CFE_VERSION_OFFSET,
            (unsigned char *)&bootCfeVersion, sizeof(bootCfeVersion));

        /* Read the flash contents into NVRAM buffer */
        flash_read_buf (NVRAM_SECTOR, NVRAM_DATA_OFFSET,
            (unsigned char *)&bootNvramData, sizeof (NVRAM_DATA)) ;
    }

#if defined(DEBUG_FLASH)
    printk("reading nvram into bootNvramData\n");
    printk("ulPsiSize 0x%x\n", (unsigned int)bootNvramData.ulPsiSize);
    printk("backupPsi 0x%x\n", (unsigned int)bootNvramData.backupPsi);
    printk("ulSyslogSize 0x%x\n", (unsigned int)bootNvramData.ulSyslogSize);
#endif

    if ((BpSetBoardId(bootNvramData.szBoardId) != BP_SUCCESS))
        printk("\n*** Board is not initialized properly ***\n\n");
#endif
}

#if defined(CONFIG_BCM93380) || defined(CONFIG_BCMKILAUEA) || defined(CONFIG_BCM93383)
BcmProgramHeader * kerSysImageTagGet(void)
{
	extern int get_part_offset(void);
	static unsigned char pgmHdr1[sizeof(BcmProgramHeader)]; /* for image 1 */
	int sect;
    unsigned int fromaddr;
   
#if defined(CONFIG_BCM_RAMDISK)
	memcpy(&pgmHdr1[0], (void *)0x87A00000, sizeof(BcmProgramHeader));
#else
    /* Get flash offset from which Linux image starts */ 
    fromaddr = get_part_offset();

    /* Conver to sector */ 
    fromaddr += FLASH_BASE;
    sect = flash_get_blk((int) fromaddr);

    flash_read_buf( sect, 0, &pgmHdr1[0], sizeof(BcmProgramHeader) );
#endif

#if 0
	for(sect = 0; sect < sizeof(BcmProgramHeader); sect+=4)
	{
		printk("%08lx\n", *(unsigned long *)(pgmHdr1 + 4 * sect));
	}

	printk("\n\n");
	for(sect = 0; sect < sizeof(BcmProgramHeader); sect+=4)
	{
		printk("%08lx\n", *(unsigned long *)(0xBFC20000 + 4 * sect));
	}
#endif
	return (BcmProgramHeader *)pgmHdr1;
}

BcmProgramHeader * kerAppImageTagGet(void)
{
    static unsigned char pgmHdr1[sizeof(BcmProgramHeader)];
    int sect;
    unsigned int fromaddr;

    fromaddr = FLASH_BASE + 0x1000000;
    sect = flash_get_blk((int) fromaddr);
    flash_read_buf( sect, 0, &pgmHdr1[0], sizeof(BcmProgramHeader) );

    return (BcmProgramHeader *)pgmHdr1;
}

#else
/***********************************************************************
 * Function Name: kerSysCfeVersionGet
 * Description  : Get CFE Version.
 * Returns      : 1 -- ok, 0 -- fail
 ***********************************************************************/
int kerSysCfeVersionGet(char *string, int stringLength)
{
    memcpy(string, (unsigned char *)&bootCfeVersion, stringLength);
    return(0);
}

/*******************************************************************************
 * NVRAM functions
 *******************************************************************************/
// get nvram data
// return:
//  0 - ok
//  -1 - fail
int kerSysNvRamGet(char *string, int strLen, int offset)
{
    char *pBuf = NULL;

    if (bootFromNand) {
        memcpy(string, (unsigned char *) &bootNvramData, strLen);
    }
    else {
        if ((pBuf = getSharedBlks(NVRAM_SECTOR, 1)) == NULL)
            return -1;

        // get string off the memory buffer
        memcpy(string, (pBuf + NVRAM_DATA_OFFSET + offset), strLen);

        retriedKfree(pBuf);
    }

    return 0;
}

// set nvram 
// return:
//  0 - ok
//  -1 - fail
int kerSysNvRamSet(char *string, int strLen, int offset)
{
    int sts = 0;
    char *pBuf = NULL;

    if (bootFromNand == 0)
    {
        if ((pBuf = getSharedBlks(NVRAM_SECTOR, 1)) == NULL)
            return -1;

        // set string to the memory buffer
        memcpy((pBuf + NVRAM_DATA_OFFSET + offset), string, strLen);

        if (setSharedBlks(NVRAM_SECTOR, 1, pBuf) != 0)
            sts = -1;
    
        retriedKfree(pBuf);
    }
    else
        sts = -1;

    return sts;
}

/***********************************************************************
 * Function Name: kerSysEraseNvRam
 * Description  : Erase the NVRAM storage section of flash memory.
 * Returns      : 1 -- ok, 0 -- fail
 ***********************************************************************/
int kerSysEraseNvRam(void)
{
    int sts = 1;

    char *tempStorage = retriedKmalloc(NVRAM_LENGTH);
    
    if (bootFromNand == 0)
    {
        // just write the whole buf with '0xff' to the flash
        if (!tempStorage)
            sts = 0;
        else
        {
            memset(tempStorage, 0xff, NVRAM_LENGTH);
            if (kerSysNvRamSet(tempStorage, NVRAM_LENGTH, 0) != 0)
                sts = 0;
            retriedKfree(tempStorage);
        }
    }
    else
        sts = 0;

    return sts;
}
#endif

#if defined(CONFIG_BCM_RAMDISK)
int kerSysMemoryMappedFlashSizeGet(void)
{
    return( 8 * 1024 * 1024 );
}

unsigned long kerSysReadFromFlash( void *toaddr, unsigned long fromaddr,
    unsigned long len )
{
    memcpy(toaddr, (void *)fromaddr, len);
    return( len );
}
#else
unsigned long kerSysReadFromFlash( void *toaddr, unsigned long fromaddr,
    unsigned long len )
{
    int sect = flash_get_blk((int) fromaddr);
    unsigned char *start = flash_get_memptr(sect);
    flash_read_buf( sect, (int) fromaddr - (int) start, toaddr, len );

    return( len );
}
#endif
#else // CONFIG_BRCM_IKOS
static NVRAM_DATA ikos_nvram_data =
    {
    NVRAM_VERSION_NUMBER,
    "",
    "ikos",
    0,
    DEFAULT_PSI_SIZE,
    11,
    {0x02, 0x10, 0x18, 0x01, 0x00, 0x01},
    0x00, 0x00,
    0x720c9f60
    };

void kerSysEarlyFlashInit( void )
{
    memcpy((unsigned char *)&bootNvramData, (unsigned char *)&ikos_nvram_data,
        sizeof (NVRAM_DATA));
    fInfo.flash_scratch_pad_length = 0;
    fInfo.flash_persistent_start_blk = 0;
}

int kerSysCfeVersionGet(char *string, int stringLength)
{
    *string = '\0';
    return(0);
}

int kerSysNvRamGet(char *string, int strLen, int offset)
{
    memcpy(string, (unsigned char *) &ikos_nvram_data, sizeof(NVRAM_DATA));
    return(0);
}

int kerSysNvRamSet(char *string, int strLen, int offset)
{
    return(0);
}

int kerSysEraseNvRam(void)
{
    return(0);
}

unsigned long kerSysReadFromFlash( void *toaddr, unsigned long fromaddr,
    unsigned long len )
{
    return(memcpy((unsigned char *) toaddr, (unsigned char *) fromaddr, len));
}
#endif  // CONFIG_BRCM_IKOS


void kerSysFlashInit( void )
{
#if !defined(CONFIG_BCM93380) && !defined(CONFIG_BCMKILAUEA) && !defined(CONFIG_BCM93383)
    sema_init(&semflash, 1);
    flash_init_info(&bootNvramData, &fInfo);
#endif
}

#if !defined(CONFIG_BCM93380) && !defined(CONFIG_BCMKILAUEA) && !defined(CONFIG_BCM93383)
/***********************************************************************
 * Function Name: kerSysFlashAddrInfoGet
 * Description  : Fills in a structure with information about the NVRAM
 *                and persistent storage sections of flash memory.  
 *                Fro physmap.c to mount the fs vol.
 * Returns      : None.
 ***********************************************************************/
void kerSysFlashAddrInfoGet(PFLASH_ADDR_INFO pflash_addr_info)
{
    memcpy(pflash_addr_info, &fInfo, sizeof(FLASH_ADDR_INFO));
}

/*******************************************************************************
 * PSI functions
 *******************************************************************************/
#define PSI_FILE_NAME           "/data/psi"
#define PSI_BACKUP_FILE_NAME    "/data/psibackup"
#define SCRATCH_PAD_FILE_NAME   "/data/scratchpad"


// get psi data
// return:
//  0 - ok
//  -1 - fail
int kerSysPersistentGet(char *string, int strLen, int offset)
{
    char *pBuf = NULL;

    if( bootFromNand )
    {
        /* Root file system is on a writable NAND flash.  Read PSI from
         * a file on the NAND flash.
         */
        struct file *fp;
        mm_segment_t fs;
        int len;

        memset(string, 0x00, strLen);
        fp = filp_open(PSI_FILE_NAME, O_RDONLY, 0);
        if (!IS_ERR(fp) && fp->f_op && fp->f_op->read)
        {
            fs = get_fs();
            set_fs(get_ds());

            fp->f_pos = 0;

            if((len = (int) fp->f_op->read(fp, (void *) string, strLen,
               &fp->f_pos)) <= 0)
                printk("Failed to read psi from '%s'\n", PSI_FILE_NAME);

            filp_close(fp, NULL);
            set_fs(fs);
        }

        return 0;
    }

    if ((pBuf = getSharedBlks(fInfo.flash_persistent_start_blk,
        fInfo.flash_persistent_number_blk)) == NULL)
        return -1;

    // get string off the memory buffer
    memcpy(string, (pBuf + fInfo.flash_persistent_blk_offset + offset), strLen);

    retriedKfree(pBuf);

    return 0;
}

int kerSysBackupPsiGet(char *string, int strLen, int offset)
{
    char *pBuf = NULL;

    if( bootFromNand )
    {
        /* Root file system is on a writable NAND flash.  Read backup PSI from
         * a file on the NAND flash.
         */
        struct file *fp;
        mm_segment_t fs;
        int len;

        memset(string, 0x00, strLen);
        fp = filp_open(PSI_BACKUP_FILE_NAME, O_RDONLY, 0);
        if (!IS_ERR(fp) && fp->f_op && fp->f_op->read)
        {
            fs = get_fs();
            set_fs(get_ds());

            fp->f_pos = 0;

            if((len = (int) fp->f_op->read(fp, (void *) string, strLen,
               &fp->f_pos)) <= 0)
                printk("Failed to read psi from '%s'\n", PSI_BACKUP_FILE_NAME);

            filp_close(fp, NULL);
            set_fs(fs);
        }

        return 0;
    }

    if (fInfo.flash_backup_psi_number_blk <= 0)
    {
        printk("No backup psi blks allocated, change it in CFE\n");
        return -1;
    }

    if (fInfo.flash_persistent_start_blk == 0)
        return -1;

    if ((pBuf = getSharedBlks(fInfo.flash_backup_psi_start_blk,
        (fInfo.flash_backup_psi_start_blk + fInfo.flash_backup_psi_number_blk))) == NULL)
        return -1;

    // get string off the memory buffer
    memcpy(string, (pBuf + offset), strLen);

    retriedKfree(pBuf);

    return 0;
}

// set psi 
// return:
//  0 - ok
//  -1 - fail
int kerSysPersistentSet(char *string, int strLen, int offset)
{
    int sts = 0;
    char *pBuf = NULL;

    if( bootFromNand )
    {
        /* Root file system is on a writable NAND flash.  Write PSI to
         * a file on the NAND flash.
         */
        struct file *fp;
        mm_segment_t fs;

        fp = filp_open(PSI_FILE_NAME, O_RDWR | O_TRUNC | O_CREAT,
            S_IRUSR | S_IWUSR);

        if (!IS_ERR(fp) && fp->f_op && fp->f_op->write)
        {
            fs = get_fs();
            set_fs(get_ds());

            fp->f_pos = 0;

            if((int) fp->f_op->write(fp, (void *) string, strLen,
               &fp->f_pos) != strLen)
                printk("Failed to write psi to '%s'.\n", PSI_FILE_NAME);

            filp_close(fp, NULL);
            set_fs(fs);
        }
        else
            printk("Unable to open '%s'.\n", PSI_FILE_NAME);

        return 0;
    }

    if ((pBuf = getSharedBlks(fInfo.flash_persistent_start_blk,
        fInfo.flash_persistent_number_blk)) == NULL)
        return -1;

    // set string to the memory buffer
    memcpy((pBuf + fInfo.flash_persistent_blk_offset + offset), string, strLen);

    if (setSharedBlks(fInfo.flash_persistent_start_blk, 
        fInfo.flash_persistent_number_blk, pBuf) != 0)
        sts = -1;
    
    retriedKfree(pBuf);

    return sts;
}

int kerSysBackupPsiSet(char *string, int strLen, int offset)
{
    int i;
    int sts = 0;
    int usedBlkSize = 0;
    char *pBuf = NULL;

    if( bootFromNand )
    {
        /* Root file system is on a writable NAND flash.  Write backup PSI to
         * a file on the NAND flash.
         */
        struct file *fp;
        mm_segment_t fs;

        fp = filp_open(PSI_BACKUP_FILE_NAME, O_RDWR | O_TRUNC | O_CREAT,
            S_IRUSR | S_IWUSR);

        if (!IS_ERR(fp) && fp->f_op && fp->f_op->write)
        {
            fs = get_fs();
            set_fs(get_ds());

            fp->f_pos = 0;

            if((int) fp->f_op->write(fp, (void *) string, strLen,
               &fp->f_pos) != strLen)
                printk("Failed to write psi to '%s'.\n", PSI_BACKUP_FILE_NAME);

            filp_close(fp, NULL);
            set_fs(fs);
        }
        else
            printk("Unable to open '%s'.\n", PSI_BACKUP_FILE_NAME);


        return 0;
    }

    if (fInfo.flash_backup_psi_number_blk <= 0)
    {
        printk("No backup psi blks allocated, change it in CFE\n");
        return -1;
    }

    if (fInfo.flash_persistent_start_blk == 0)
        return -1;

    /*
     * The backup PSI does not share its blocks with anybody else, so I don't have
     * to read the flash first.  But now I have to make sure I allocate a buffer
     * big enough to cover all blocks that the backup PSI spans.
     */
    for (i=fInfo.flash_backup_psi_start_blk;
         i < (fInfo.flash_backup_psi_start_blk + fInfo.flash_backup_psi_number_blk); i++)
    {
       usedBlkSize += flash_get_sector_size((unsigned short) i);
    }

    if ((pBuf = (char *) retriedKmalloc(usedBlkSize)) == NULL)
    {
       printk("failed to allocate memory with size: %d\n", usedBlkSize);
       return -1;
    }

    memset(pBuf, 0, usedBlkSize);

    // set string to the memory buffer
    memcpy((pBuf + offset), string, strLen);

    if (setSharedBlks(fInfo.flash_backup_psi_start_blk, 
        (fInfo.flash_backup_psi_start_blk + fInfo.flash_backup_psi_number_blk), pBuf) != 0)
        sts = -1;
    
    retriedKfree(pBuf);

    return sts;
}

#define SYSLOG_FILE_NAME        "/etc/syslog"

int kerSysSyslogGet(char *string, int strLen, int offset)
{
    char *pBuf = NULL;

    if( bootFromNand )
    {
        /* Root file system is on a writable NAND flash.  Read syslog from
         * a file on the NAND flash.
         */
        struct file *fp;
        mm_segment_t fs;

        memset(string, 0x00, strLen);
        fp = filp_open(SYSLOG_FILE_NAME, O_RDONLY, 0);
        if (!IS_ERR(fp) && fp->f_op && fp->f_op->read)
        {
            fs = get_fs();
            set_fs(get_ds());

            fp->f_pos = 0;

            if((int) fp->f_op->read(fp, (void *) string, strLen,
               &fp->f_pos) <= 0)
                printk("Failed to read psi from '%s'\n", SYSLOG_FILE_NAME);

            filp_close(fp, NULL);
            set_fs(fs);
        }

        return 0;
    }

    if (fInfo.flash_syslog_number_blk <= 0)
    {
        printk("No syslog blks allocated, change it in CFE\n");
        return -1;
    }
    
    if (strLen > fInfo.flash_syslog_length)
        return -1;

    if ((pBuf = getSharedBlks(fInfo.flash_syslog_start_blk,
        (fInfo.flash_syslog_start_blk + fInfo.flash_syslog_number_blk))) == NULL)
        return -1;

    // get string off the memory buffer
    memcpy(string, (pBuf + offset), strLen);

    retriedKfree(pBuf);

    return 0;
}

int kerSysSyslogSet(char *string, int strLen, int offset)
{
    int i;
    int sts = 0;
    int usedBlkSize = 0;
    char *pBuf = NULL;

    if( bootFromNand )
    {
        /* Root file system is on a writable NAND flash.  Write PSI to
         * a file on the NAND flash.
         */
        struct file *fp;
        mm_segment_t fs;

        fp = filp_open(PSI_FILE_NAME, O_RDWR | O_TRUNC | O_CREAT,
            S_IRUSR | S_IWUSR);

        if (!IS_ERR(fp) && fp->f_op && fp->f_op->write)
        {
            fs = get_fs();
            set_fs(get_ds());

            fp->f_pos = 0;

            if((int) fp->f_op->write(fp, (void *) string, strLen,
               &fp->f_pos) != strLen)
                printk("Failed to write psi to '%s'.\n", PSI_FILE_NAME);

            filp_close(fp, NULL);
            set_fs(fs);
        }
        else
            printk("Unable to open '%s'.\n", PSI_FILE_NAME);

        return 0;
    }

    if (fInfo.flash_syslog_number_blk <= 0)
    {
        printk("No syslog blks allocated, change it in CFE\n");
        return -1;
    }
    
    if (strLen > fInfo.flash_syslog_length)
        return -1;

    /*
     * The syslog does not share its blocks with anybody else, so I don't have
     * to read the flash first.  But now I have to make sure I allocate a buffer
     * big enough to cover all blocks that the syslog spans.
     */
    for (i=fInfo.flash_syslog_start_blk;
         i < (fInfo.flash_syslog_start_blk + fInfo.flash_syslog_number_blk); i++)
    {
        usedBlkSize += flash_get_sector_size((unsigned short) i);
    }

    if ((pBuf = (char *) retriedKmalloc(usedBlkSize)) == NULL)
    {
       printk("failed to allocate memory with size: %d\n", usedBlkSize);
       return -1;
    }

    memset(pBuf, 0, usedBlkSize);

    // set string to the memory buffer
    memcpy((pBuf + offset), string, strLen);

    if (setSharedBlks(fInfo.flash_syslog_start_blk, 
        (fInfo.flash_syslog_start_blk + fInfo.flash_syslog_number_blk), pBuf) != 0)
        sts = -1;

    retriedKfree(pBuf);

    return sts;
}


// NAND flash bcm image 
// return: 
// 0 - ok
// !0 - the sector number fail to be flashed (should not be 0)
static int nandImageSet( int flash_start_addr, char *string, int img_size)
{
    int sts = -1;
    int blk = 0;
    int i;
    struct mtd_info *mtd  = NULL;
    struct mtd_info *mtd0 = get_mtd_device_nm("rootfs");
    struct mtd_info *mtd1 = get_mtd_device_nm("nvram");

    if( mtd0 && mtd1 )
    {
        // Disable other tasks from this point on
#if defined(CONFIG_SMP)
        smp_send_stop();
        udelay(20);
#endif
        local_bh_disable();

        if( *(unsigned short *) string == JFFS2_MAGIC_BITMASK )
            mtd = mtd0; /* only flash file system */
        else
        {
            mtd = mtd1; /* flash first boot block and then file system */

            /* Copy NVRAM data to block to be flashed so it is preserved. */
            memcpy(string + NVRAM_DATA_OFFSET, (unsigned char *)&bootNvramData,
                sizeof(NVRAM_DATA));
        }

        sts = blk = 0;
        while( sts == 0 )
        {
            /* block_is bad returns 0 if block is not bad */
            if( mtd->block_isbad(mtd, blk) == 0 )
            {
                unsigned char oobbuf[64]; /* expected to be a max size */
                struct mtd_oob_ops ops;

                ops.ooblen = mtd->oobsize;
                ops.ooboffs = 0;
                ops.datbuf = NULL;
                ops.oobbuf = oobbuf;
                ops.len = 0;
                ops.mode = MTD_OOB_PLACE;

                /* Read and save the spare area. */
                sts = mtd->read_oob(mtd, blk, &ops);
                if( sts == 0 )
                {
                    struct erase_info erase;

                    /* Erase the flash block. */
                    memset(&erase, 0x00, sizeof(erase));
                    erase.addr = blk;
                    erase.len = mtd->erasesize;
                    erase.mtd = mtd;

                    sts = mtd->erase(mtd, &erase);
                    if( sts == 0 )
                    {
                        /* Function local_bh_disable has been called and this
                         * is the only operation that should be occurring.
                         * Therefore, spin waiting for erase to complete.
                         */
                        for(i = 0; i < 10000 && erase.state != MTD_ERASE_DONE &&
                            erase.state != MTD_ERASE_FAILED; i++ )
                        {
                            udelay(100);
                        }

                        if( erase.state != MTD_ERASE_DONE )
                            sts = -1;
                    }

                    if( sts == 0 )
                    {
                        ops.ooblen = mtd->oobsize;
                        ops.ooboffs = 0;
                        ops.datbuf = NULL;
                        ops.oobbuf = oobbuf;
                        ops.len = 0;
                        ops.mode = MTD_OOB_PLACE;

                        /* Restore the spare area. */
                        sts = mtd->write_oob(mtd, blk, &ops);

                        /* Write out a block of the image if there is still
                         * image left to flash.
                         */
                        if( sts == 0 && blk < img_size )
                        {
                            int retlen = 0;

                            /* Write a block of the image to flash. */
                            sts = mtd->write(mtd, blk, mtd->erasesize, &retlen,
                                string);

                            string += mtd->erasesize;
                            printk(".");

                            if( retlen != mtd->erasesize )
                                sts = -1;

                            if( sts )
                                printk("nandImageSet - Block 0x%8.8x. Error writing"
                                    " block.\n", blk);
                        }
                        else
                            if( sts )
                                printk("nandImageSet - Block 0x%8.8x. Error writing"
                                    " spare area.\n", blk);
                    }
                    else
                        printk("nandImageSet - Block 0x%8.8x. Error erasing "
                            "block.\n", blk);
                }
                else
                    printk("nandImageSet - Block 0x%8.8x. Error read spare area.\n",
                        blk);
            }

            if( mtd == mtd1 )
            {
                /* The first raw, boot block was flashed.  Change to rootfs. */
                img_size -= mtd->erasesize;
                mtd = mtd0;
                blk = 0;
            }
            else
            {
                /* Update to next block, stop when all blocks flashed. */
                blk += mtd->erasesize;
                if( blk >= mtd->size )
                    break;
            }
        }

        local_bh_enable();

        if( sts )
            sts = (blk > mtd->erasesize) ? blk / mtd->erasesize : 1;
    }

    if( mtd0 )
        put_mtd_device(mtd0);

    if( mtd1 )
        put_mtd_device(mtd1);

    return sts;
}

// flash bcm image 
// return: 
// 0 - ok
// !0 - the sector number fail to be flashed (should not be 0)
int kerSysBcmImageSet( int flash_start_addr, char *string, int size)
{
    int sts;
    int sect_size;
    int blk_start;
    int savedSize = size;
    int whole_image = 0;

    if( bootFromNand )
        return( nandImageSet( flash_start_addr, string, size ) );

    if (flash_start_addr == FLASH_BASE)
        whole_image = 1;

#if defined(DEBUG_FLASH)
    printk("kerSysBcmImageSet: flash_start_addr=0x%x string=%p len=%d whole_image=%d\n",
           flash_start_addr, string, size, whole_image);
#endif

    blk_start = flash_get_blk(flash_start_addr);
    if( blk_start < 0 )
        return( -1 );

    // Disable other tasks from this point on
#if defined(CONFIG_SMP)
    smp_send_stop();
    udelay(20);
#endif
    local_bh_disable();

    /* write image to flash memory */
    do 
    {
        sect_size = flash_get_sector_size(blk_start);

        flash_sector_erase_int(blk_start);     // erase blk before flash

        if (sect_size > size) 
        {
            if (size & 1) 
                size++;
            sect_size = size;
        }
        
        if (flash_write_buf(blk_start, 0, string, sect_size) != sect_size) {
            break;
        }

        printk(".");
        blk_start++;
        string += sect_size;
        size -= sect_size; 
    } while (size > 0);

    if (whole_image && savedSize > fInfo.flash_rootfs_start_offset)
    {
        // If flashing a whole image, erase to end of flash.
        int total_blks = flash_get_numsectors();
        while( blk_start < total_blks )
        {
            flash_sector_erase_int(blk_start);
            printk(".");
            blk_start++;
        }
    }

    printk("\n\n");

    if( size == 0 ) 
        sts = 0;  // ok
    else  
        sts = blk_start;    // failed to flash this sector

    local_bh_enable();

    return sts;
}

/*******************************************************************************
 * SP functions
 *******************************************************************************/

// get scratch pad data into *** pTempBuf *** which has to be released by the
//      caller!
// return: if pTempBuf != NULL, points to the data with the dataSize of the
//      buffer
// !NULL -- ok
// NULL  -- fail
static char *getScratchPad(int len)
{
    /* Root file system is on a writable NAND flash.  Read scratch pad from
     * a file on the NAND flash.
     */
    char *ret = NULL;

    if( (ret = retriedKmalloc(len)) != NULL )
    {
        struct file *fp;
        mm_segment_t fs;

        memset(ret, 0x00, len);
        fp = filp_open(SCRATCH_PAD_FILE_NAME, O_RDONLY, 0);
        if (!IS_ERR(fp) && fp->f_op && fp->f_op->read)
        {
            fs = get_fs();
            set_fs(get_ds());

            fp->f_pos = 0;

            if((int) fp->f_op->read(fp, (void *) ret, len, &fp->f_pos) <= 0)
                printk("Failed to read scratch pad from '%s'\n",
                    SCRATCH_PAD_FILE_NAME);

            filp_close(fp, NULL);
            set_fs(fs);
        }
    }
    else
        printk("Could not allocate scratch pad memory.\n");

    return( ret );
}

// set scratch pad - write the scratch pad file
// return:
// 0 -- ok
// -1 -- fail
static int setScratchPad(char *buf, int len)
{
    /* Root file system is on a writable NAND flash.  Write PSI to
     * a file on the NAND flash.
     */
    int ret = -1;
    struct file *fp;
    mm_segment_t fs;

    fp = filp_open(SCRATCH_PAD_FILE_NAME, O_RDWR | O_TRUNC | O_CREAT,
        S_IRUSR | S_IWUSR);

    if (!IS_ERR(fp) && fp->f_op && fp->f_op->write)
    {
        fs = get_fs();
        set_fs(get_ds());

        fp->f_pos = 0;

        if((int) fp->f_op->write(fp, (void *) buf, len, &fp->f_pos) == len)
            ret = 0;
        else
            printk("Failed to write scratch pad to '%s'.\n", 
                SCRATCH_PAD_FILE_NAME);

        filp_close(fp, NULL);
        set_fs(fs);
    }
    else
        printk("Unable to open '%s'.\n", SCRATCH_PAD_FILE_NAME);

    return( ret );
}

/*
 * get list of all keys/tokenID's in the scratch pad.
 * NOTE: memcpy work here -- not using copy_from/to_user
 *
 * return:
 *         greater than 0 means number of bytes copied to tokBuf,
 *         0 means fail,
 *         negative number means provided buffer is not big enough and the
 *         absolute value of the negative number is the number of bytes needed.
 */
int kerSysScratchPadList(char *tokBuf, int bufLen)
{
    PSP_TOKEN pToken = NULL;
    char *pBuf = NULL;
    char *pShareBuf = NULL;
    char *startPtr = NULL;
    int usedLen;
    int tokenNameLen=0;
    int copiedLen=0;
    int needLen=0;
    int sts = 0;

    if( bootFromNand )
    {
        if((pShareBuf = getScratchPad(fInfo.flash_scratch_pad_length)) == NULL)
            return sts;

        pBuf = pShareBuf;
    }
    else
    {
        if( (pShareBuf = getSharedBlks(fInfo.flash_scratch_pad_start_blk,
            fInfo.flash_scratch_pad_number_blk)) == NULL )
        {
            printk("could not getSharedBlks.\n");
            return sts;
        }

        // pBuf points to SP buf
        pBuf = pShareBuf + fInfo.flash_scratch_pad_blk_offset;  
    }

    if(memcmp(((PSP_HEADER)pBuf)->SPMagicNum, MAGIC_NUMBER, MAGIC_NUM_LEN) != 0) 
    {
        printk("Scratch pad is not initialized.\n");
        retriedKfree(pShareBuf);
        return sts;
    }

    // Walk through all the tokens
    usedLen = sizeof(SP_HEADER);
    startPtr = pBuf + sizeof(SP_HEADER);
    pToken = (PSP_TOKEN) startPtr;

    while( pToken->tokenName[0] != '\0' && pToken->tokenLen > 0 &&
           ((usedLen + pToken->tokenLen) <= fInfo.flash_scratch_pad_length))
    {
        tokenNameLen = strlen(pToken->tokenName);
        needLen += tokenNameLen + 1;
        if (needLen <= bufLen)
        {
            strcpy(&tokBuf[copiedLen], pToken->tokenName);
            copiedLen += tokenNameLen + 1;
        }

        usedLen += ((pToken->tokenLen + 0x03) & ~0x03);
        startPtr += sizeof(SP_TOKEN) + ((pToken->tokenLen + 0x03) & ~0x03);
        pToken = (PSP_TOKEN) startPtr;
    }

    if ( needLen > bufLen )
    {
        // User may purposely pass in a 0 length buffer just to get
        // the size, so don't log this as an error.
        sts = needLen * (-1);
    }
    else
    {
        sts = copiedLen;
    }

    retriedKfree(pShareBuf);

    return sts;
}

/*
 * get sp data.  NOTE: memcpy work here -- not using copy_from/to_user
 * return:
 *         greater than 0 means number of bytes copied to tokBuf,
 *         0 means fail,
 *         negative number means provided buffer is not big enough and the
 *         absolute value of the negative number is the number of bytes needed.
 */
int kerSysScratchPadGet(char *tokenId, char *tokBuf, int bufLen)
{
    PSP_TOKEN pToken = NULL;
    char *pBuf = NULL;
    char *pShareBuf = NULL;
    char *startPtr = NULL;
    int usedLen;
    int sts = 0;

    if( bootFromNand )
    {
        if((pShareBuf = getScratchPad(fInfo.flash_scratch_pad_length)) == NULL)
            return sts;

        pBuf = pShareBuf;
    }
    else
    {
        if( (pShareBuf = getSharedBlks(fInfo.flash_scratch_pad_start_blk,
            fInfo.flash_scratch_pad_number_blk)) == NULL )
        {
            printk("could not getSharedBlks.\n");
            return sts;
        }

        // pBuf points to SP buf
        pBuf = pShareBuf + fInfo.flash_scratch_pad_blk_offset;
    }

    if(memcmp(((PSP_HEADER)pBuf)->SPMagicNum, MAGIC_NUMBER, MAGIC_NUM_LEN) != 0) 
    {
        printk("Scratch pad is not initialized.\n");
        retriedKfree(pShareBuf);
        return sts;
    }

    // search for the token
    usedLen = sizeof(SP_HEADER);
    startPtr = pBuf + sizeof(SP_HEADER);
    pToken = (PSP_TOKEN) startPtr;
    while( pToken->tokenName[0] != '\0' && pToken->tokenLen > 0 &&
        pToken->tokenLen < fInfo.flash_scratch_pad_length &&
        usedLen < fInfo.flash_scratch_pad_length )
    {

        if (strncmp(pToken->tokenName, tokenId, TOKEN_NAME_LEN) == 0)
        {
            if ( pToken->tokenLen > bufLen )
            {
               // User may purposely pass in a 0 length buffer just to get
               // the size, so don't log this as an error.
               // printk("The length %d of token %s is greater than buffer len %d.\n", pToken->tokenLen, pToken->tokenName, bufLen);
                sts = pToken->tokenLen * (-1);
            }
            else
            {
                memcpy(tokBuf, startPtr + sizeof(SP_TOKEN), pToken->tokenLen);
                sts = pToken->tokenLen;
            }
            break;
        }

        usedLen += ((pToken->tokenLen + 0x03) & ~0x03);
        startPtr += sizeof(SP_TOKEN) + ((pToken->tokenLen + 0x03) & ~0x03);
        pToken = (PSP_TOKEN) startPtr;
    }

    retriedKfree(pShareBuf);

    return sts;
}

// set sp.  NOTE: memcpy work here -- not using copy_from/to_user
// return:
//  0 - ok
//  -1 - fail
int kerSysScratchPadSet(char *tokenId, char *tokBuf, int bufLen)
{
    PSP_TOKEN pToken = NULL;
    char *pShareBuf = NULL;
    char *pBuf = NULL;
    SP_HEADER SPHead;
    SP_TOKEN SPToken;
    char *curPtr;
    int sts = -1;

    if( bufLen >= fInfo.flash_scratch_pad_length - sizeof(SP_HEADER) -
        sizeof(SP_TOKEN) )
    {
        printk("Scratch pad overflow by %d bytes.  Information not saved.\n",
            bufLen  - fInfo.flash_scratch_pad_length - sizeof(SP_HEADER) -
            sizeof(SP_TOKEN));
        return sts;
    }

    if( bootFromNand )
    {
        if((pShareBuf = getScratchPad(fInfo.flash_scratch_pad_length)) == NULL)
            return sts;

        pBuf = pShareBuf;
    }
    else
    {
        if( (pShareBuf = getSharedBlks( fInfo.flash_scratch_pad_start_blk,
            fInfo.flash_scratch_pad_number_blk)) == NULL )
        {
            return sts;
        }

        // pBuf points to SP buf
        pBuf = pShareBuf + fInfo.flash_scratch_pad_blk_offset;  
    }

    // form header info.
    memset((char *)&SPHead, 0, sizeof(SP_HEADER));
    memcpy(SPHead.SPMagicNum, MAGIC_NUMBER, MAGIC_NUM_LEN);
    SPHead.SPVersion = SP_VERSION;

    // form token info.
    memset((char*)&SPToken, 0, sizeof(SP_TOKEN));
    strncpy(SPToken.tokenName, tokenId, TOKEN_NAME_LEN - 1);
    SPToken.tokenLen = bufLen;

    if(memcmp(((PSP_HEADER)pBuf)->SPMagicNum, MAGIC_NUMBER, MAGIC_NUM_LEN) != 0)
    {
        // new sp, so just flash the token
        printk("No scratch pad found.  Initialize scratch pad...\n");
        memcpy(pBuf, (char *)&SPHead, sizeof(SP_HEADER));
        curPtr = pBuf + sizeof(SP_HEADER);
        memcpy(curPtr, (char *)&SPToken, sizeof(SP_TOKEN));
        curPtr += sizeof(SP_TOKEN);
        if( tokBuf )
            memcpy(curPtr, tokBuf, bufLen);
    }
    else  
    {
        int putAtEnd = 1;
        int curLen;
        int usedLen;
        int skipLen;

        /* Calculate the used length. */
        usedLen = sizeof(SP_HEADER);
        curPtr = pBuf + sizeof(SP_HEADER);
        pToken = (PSP_TOKEN) curPtr;
        skipLen = (pToken->tokenLen + 0x03) & ~0x03;
        while( pToken->tokenName[0] >= 'A' && pToken->tokenName[0] <= 'z' &&
            strlen(pToken->tokenName) < TOKEN_NAME_LEN &&
            pToken->tokenLen > 0 &&
            pToken->tokenLen < fInfo.flash_scratch_pad_length &&
            usedLen < fInfo.flash_scratch_pad_length )
        {
            usedLen += sizeof(SP_TOKEN) + skipLen;
            curPtr += sizeof(SP_TOKEN) + skipLen;
            pToken = (PSP_TOKEN) curPtr;
            skipLen = (pToken->tokenLen + 0x03) & ~0x03;
        }

        if( usedLen + SPToken.tokenLen + sizeof(SP_TOKEN) >
            fInfo.flash_scratch_pad_length )
        {
            printk("Scratch pad overflow by %d bytes.  Information not saved.\n",
                (usedLen + SPToken.tokenLen + sizeof(SP_TOKEN)) -
                fInfo.flash_scratch_pad_length);
            return sts;
        }

        curPtr = pBuf + sizeof(SP_HEADER);
        curLen = sizeof(SP_HEADER);
        while( curLen < usedLen )
        {
            pToken = (PSP_TOKEN) curPtr;
            skipLen = (pToken->tokenLen + 0x03) & ~0x03;
            if (strncmp(pToken->tokenName, tokenId, TOKEN_NAME_LEN) == 0)
            {
                // The token id already exists.
                if( tokBuf && pToken->tokenLen == bufLen )
                {
                    // The length of the new data and the existing data is the
                    // same.  Overwrite the existing data.
                    memcpy((curPtr+sizeof(SP_TOKEN)), tokBuf, bufLen);
                    putAtEnd = 0;
                }
                else
                {
                    // The length of the new data and the existing data is
                    // different.  Shift the rest of the scratch pad to this
                    // token's location and put this token's data at the end.
                    char *nextPtr = curPtr + sizeof(SP_TOKEN) + skipLen;
                    int copyLen = usedLen - (curLen+sizeof(SP_TOKEN) + skipLen);
                    memcpy( curPtr, nextPtr, copyLen );
                    memset( curPtr + copyLen, 0x00, 
                        fInfo.flash_scratch_pad_length - (curLen + copyLen) );
                    usedLen -= sizeof(SP_TOKEN) + skipLen;
                }
                break;
            }

            // get next token
            curPtr += sizeof(SP_TOKEN) + skipLen;
            curLen += sizeof(SP_TOKEN) + skipLen;
        } // end while

        if( putAtEnd )
        {
            if( tokBuf )
            {
                memcpy( pBuf + usedLen, &SPToken, sizeof(SP_TOKEN) );
                memcpy( pBuf + usedLen + sizeof(SP_TOKEN), tokBuf, bufLen );
            }
            memcpy( pBuf, &SPHead, sizeof(SP_HEADER) );
        }

    } // else if not new sp

    if( bootFromNand )
        sts = setScratchPad(pShareBuf, fInfo.flash_scratch_pad_length);
    else
        sts = setSharedBlks(fInfo.flash_scratch_pad_start_blk, 
            fInfo.flash_scratch_pad_number_blk, pShareBuf);
    
    retriedKfree(pShareBuf);

    return sts;

    
}

// wipe out the scratchPad
// return:
//  0 - ok
//  -1 - fail
int kerSysScratchPadClearAll(void)
{ 
    int sts = -1;
    char *pShareBuf = NULL;
   int j ;
   int usedBlkSize = 0;

   // printk ("kerSysScratchPadClearAll.... \n") ;

    if( bootFromNand )
    {
        if((pShareBuf = getScratchPad(fInfo.flash_scratch_pad_length)) == NULL)
            return sts;

        memset(pShareBuf, 0x00, fInfo.flash_scratch_pad_length);

        setScratchPad(pShareBuf, fInfo.flash_scratch_pad_length);
    }
    else
    {
        if( (pShareBuf = getSharedBlks( fInfo.flash_scratch_pad_start_blk,
            fInfo.flash_scratch_pad_number_blk)) == NULL )
            return sts;

        if (fInfo.flash_scratch_pad_number_blk == 1)
            memset(pShareBuf + fInfo.flash_scratch_pad_blk_offset, 0x00, fInfo.flash_scratch_pad_length) ;
        else
        {
            for (j = fInfo.flash_scratch_pad_start_blk;
                j < (fInfo.flash_scratch_pad_start_blk + fInfo.flash_scratch_pad_number_blk);
                j++)
            {
                usedBlkSize += flash_get_sector_size((unsigned short) j);
            }

            memset(pShareBuf, 0x00, usedBlkSize) ;
        }

        sts = setSharedBlks(fInfo.flash_scratch_pad_start_blk,    
            fInfo.flash_scratch_pad_number_blk,  pShareBuf);
    }

   retriedKfree(pShareBuf);

   //printk ("kerSysScratchPadClearAll Done.... \n") ;
   return sts;
}

int kerSysFlashSizeGet(void)
{
    int ret = 0;

    if( bootFromNand )
    {
        struct mtd_info *mtd;

        if( (mtd = get_mtd_device_nm("rootfs")) != NULL )
        {
            ret = mtd->size;
            put_mtd_device(mtd);
        }
    }
    else
        ret = flash_get_total_size();

    return ret;
}
#endif
