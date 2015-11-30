/************************************************************************/
/*                                                                      */
/*  SPI Flash Memory Drivers                                            */
/*  File name: spiflash.c                                               */
/*  Revision:  1.0  1/27/2004                                           */
/*                                                                      */
/************************************************************************/                        

/** Includes. **/
#ifdef _CFE_                                                
#include "lib_types.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "bcm_map.h"  
#define printk  printf
#else       // linux
#include <linux/version.h>
#include <linux/param.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
#include <linux/semaphore.h>
#endif
#include <linux/hardirq.h>
#if defined(CONFIG_BCM93380) || defined(CONFIG_BCM93383)
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#endif
#include <bcm_map_part.h> 
#endif

#include "ProgramStore.h"
#include "bcmtypes.h"
#include "bcm_hwdefs.h"
#include "flash_api.h"
#include "flashmap.h"
#include "bcmSpiRes.h"


/** Defines. **/
#define OSL_DELAY(X)                        \
    do { { int i; for( i = 0; i < (X) * 500; i++ ) ; } } while(0)

#define MAX_RETRY           3

#define FAR

#ifndef NULL
#define NULL 0
#endif

#define MAXSECTORS          1024    /* maximum number of sectors supported */

#define FLASH_PAGE_128      128
#define FLASH_PAGE_256      256
#define SECTOR_SIZE_4K      (4 * 1024)
#define SECTOR_SIZE_32K     (32 * 1024)
#define SECTOR_SIZE_64K     (64 * 1024)
#define SST25VF020_SECTOR   64  /* 2 Mbit */
#define SST25VF040_SECTOR   128 /* 4 Mbit */
#define SST25VF080_SECTOR   256 /* 8 Mbit */
#define SST25VF016B_SECTOR  512 /* 16 Mbit */
#define SPAN25FL004A_SECTOR 8
#define SPAN25FL008A_SECTOR 16
#define SPAN25FL016A_SECTOR 32
#define SPAN25FL032A_SECTOR 64
#define SPAN25FL064A_SECTOR 128
#define SPAN25FL128P_SECTOR 256
#define SPAN25FL256S_SECTOR 512
#define AT25F512_SECTOR     2
#define AT25F2048_SECTOR    4
#define AMD25FL_SECTOR      4

#define CMD_LEN_1           1
#define CMD_LEN_4           4

/* Standard Boolean declarations */
#define TRUE                1
#define FALSE               0

/* Command codes for the flash_command routine */
#define FLASH_READ          0x03    /* read data from memory array */
#define FLASH_READ_FAST     0x0B    /* read data from memory array */
#define FLASH_PROG          0x02    /* program data into memory array */
#define FLASH_WREN          0x06    /* set write enable latch */
#define FLASH_WRDI          0x04    /* reset write enable latch */
#define FLASH_RDSR          0x05    /* read status register */
#define FLASH_WRST          0x01    /* write status register */
#define FLASH_EWSR          0x50    /* enable write status */
#define FLASH_WORD_AAI      0xAD    /* auto address increment word program */
#define FLASH_AAI           0xAF    /* auto address increment program */

#define SST_FLASH_CERASE    0x60    /* erase all sectors in memory array */
#define SST_FLASH_SERASE    0x20    /* erase one sector in memroy array */
#define SST_FLASH_RDID      0x90    /* read manufacturer and product id */

#define ATMEL_FLASH_CERASE  0x62    /* erase all sectors in memory array */
#define ATMEL_FLASH_SERASE  0x52    /* erase one sector in memroy array */
#define ATMEL_FLASH_RDID    0x15    /* read manufacturer and product id */

#define ATMEL_FLASH_PSEC    0x36    /* protect sector */
#define ATMEL_FLASH_UNPSEC  0x39    /* unprotect sector */
#define ATMEL_FLASH_RDPREG  0x3C    /* read protect sector registers */

#define AMD_FLASH_CERASE    0xC7    /* erase all sectors in memory array */
#define AMD_FLASH_SERASE    0xD8    /* erase one sector in memroy array */
#define AMD_FLASH_RDID      0xAB    /* read manufacturer and product id */

#define SPAN_FLASH_CERASE   0xC7    /* erase all sectors in memory array */
#define SPAN_FLASH_SERASE   0xD8    /* erase one sector in memory array */
#define SPAN_FLASH_RDID     0x9F    /* read manufacturer and product id */
#define SPAN_FLASH_READ4    0x13    /* read data from meory array with 32bit address */
#define SPAN_FLASH_FAST_READ4    0x0C    /* read data from meory array with 32bit address */
#define SPAN_FLASH_SERASE4    0xDC    /* erase one sector from meory array with 32bit address */
#define SPAN_FLASH_PROG4    0x12    /* program data into meory array with 32bit address */

#define ST_FLASH_RDID       0x9F   /* read manufacturer and product id */

/* RDSR return status bit definition */
#define SR_WPEN             0x80
#define SR_BP2              0x10
#define SR_BP1              0x08
#define SR_BP0              0x04
#define SR_WEN              0x02
#define SR_RDY              0x01


#define AT_SWP1             0x08
#define AT_SWP0             0x04
#define AT_SEC_LOCK         0xFF

/* Return codes from flash_status */
#define STATUS_READY        0       /* ready for action */
#define STATUS_BUSY         1       /* operation in progress */
#define STATUS_TIMEOUT      2       /* operation timed out */
#define STATUS_ERROR        3       /* unclassified but unhappy status */

/* Used to mask of bytes from word data */
#define HIGH_BYTE(a)        (a >> 8)
#define LOW_BYTE(a)         (a & 0xFF)

/* Define different type of flash */
#define FLASH_UNDEFINED     0
#define FLASH_SST           1
#define FLASH_ATMEL         2
#define FLASH_AMD           3
#define FLASH_SPAN          4

/* ATMEL's manufacturer ID */
#define ATMELPART           0x1F

/* A list of ATMEL device ID's - add others as needed */
#define ID_AT25F512         0x60
#define ID_AT25F512A        0x65
#define ID_AT25F2048        0x63
#define ID_AT26F004         0x04
#define ID_AT25DF041A       0x44

/* AMD's device ID */
#define AMD_S25FL002D       0x11

/* SST's manufacturer ID */
#define SSTPART             0xBF

/* A list of SST device ID's - add others as needed */
#define ID_SST25VF016B      0x41
#define ID_SST25VF020       0x43
#define ID_SST25VF040       0x44
#define ID_SST25VF080       0x80

/* NexFlash's manufacturer ID */
#define NXPART              0xEF

/* A list of NexFlash device ID's - add others as needed */
#define ID_NX25P20          0x11
#define ID_NX25P40          0x12
#define ID_W25X16           0x14
#define ID_W25X32           0x15
#define ID_W25X64           0x16

/* StFlash's manufacturer ID */
#define STPART              0x12 

/* A list of StFlash device ID's - add others as needed */
#define ID_M25P40           0x12

/* SPANSION manufacturer ID */
#define SPANPART            0x01

/* SPANSION device ID's */
#define ID_SPAN25FL004A     0x12
#define ID_SPAN25FL008A     0x13
#define ID_SPAN25FL016A     0x14
#define ID_SPAN25FL032A     0x15
#define ID_SPAN25FL064A     0x16
#define ID_SPAN25FL128P     0x18
#define ID_SPAN25FL256S     0x19

/* EON manufacturer ID */
#define EONPART             0x1C
/* NUMONYX manufacturer ID */
#define NUMONYXPART         0x20
/* Macronix manufacturer ID */
#define MACRONIXPART        0xC2

/* JEDEC device ID */
#define ID_M25P64           0x17
#define ID_M25L25	    0x19

#define SPI_MAKE_ID(A,B)    \
    (((unsigned short) (A) << 8) | ((unsigned short) B & 0xff))

#define SPI_FLASH_DEVICES                                   \
    {{SPI_MAKE_ID(ATMELPART, ID_AT25F512), "AT25F512"},     \
     {SPI_MAKE_ID(ATMELPART, ID_AT25F512A), "AT25F512A"},   \
     {SPI_MAKE_ID(ATMELPART, ID_AT25F2048), "AT25F2048"},   \
     {SPI_MAKE_ID(ATMELPART, ID_AT26F004), "AT26F004"},     \
     {SPI_MAKE_ID(ATMELPART, ID_AT25DF041A), "AT25DF041A"}, \
     {SPI_MAKE_ID(AMD_S25FL002D, 0), "AMD_S25FL002D"},      \
     {SPI_MAKE_ID(SSTPART, ID_SST25VF016B), "SST25VF016B"}, \
     {SPI_MAKE_ID(SSTPART, ID_SST25VF020), "SST25VF020"},   \
     {SPI_MAKE_ID(SSTPART, ID_SST25VF040), "SST25VF040"},   \
     {SPI_MAKE_ID(SSTPART, ID_SST25VF080), "SST25VF080"},   \
     {SPI_MAKE_ID(NXPART, ID_NX25P20), "NX25P20"},          \
     {SPI_MAKE_ID(NXPART, ID_NX25P40), "NX25P40"},          \
     {SPI_MAKE_ID(NXPART, ID_W25X16), "ID_W25X16"},         \
     {SPI_MAKE_ID(NXPART, ID_W25X32), "ID_W25X32"},         \
     {SPI_MAKE_ID(NXPART, ID_W25X64), "ID_W25X64"},         \
     {SPI_MAKE_ID(STPART, ID_M25P40), "M25P40"},            \
     {SPI_MAKE_ID(SPANPART, ID_SPAN25FL004A), "S25FL004A"}, \
     {SPI_MAKE_ID(SPANPART, ID_SPAN25FL008A), "S25FL008A"}, \
     {SPI_MAKE_ID(SPANPART, ID_SPAN25FL016A), "S25FL016A"}, \
     {SPI_MAKE_ID(SPANPART, ID_SPAN25FL032A), "S25FL032A"}, \
     {SPI_MAKE_ID(SPANPART, ID_SPAN25FL064A), "S25FL064A"}, \
     {SPI_MAKE_ID(SPANPART, ID_SPAN25FL128P), "S25FL128P"}, \
     {SPI_MAKE_ID(SPANPART, ID_SPAN25FL256S), "S25FL256S"}, \
     {SPI_MAKE_ID(EONPART, ID_M25P64), "EN25P64"},          \
     {SPI_MAKE_ID(NUMONYXPART, ID_M25P64), "NMNXM25P64"},   \
     {SPI_MAKE_ID(MACRONIXPART, ID_M25P64), "MX25L64"},     \
     {SPI_MAKE_ID(MACRONIXPART, ID_M25L25), "MX25L25735E"},  \
     {0,""}                                                 \
    }

/** Structs. **/
/* A structure for identifying a flash part.  There is one for each
 * of the flash part definitions.  We need to keep track of the
 * sector organization, the address register used, and the size
 * of the sectors.
 */
struct flashinfo {
    char *name;         /* "AT25F512", etc. */
    unsigned long addr; /* physical address, once translated */
    int nsect;          /* # of sectors */
    struct {
        long size;      /* # of bytes in this sector */
        long base;      /* offset from beginning of device */
    } sec[MAXSECTORS];  /* per-sector info */
};

struct flash_name_from_id {
    unsigned short fnfi_id;
    char fnfi_name[30];
};


extern BcmProgramHeader * kerSysImageTagGet(void);
extern BcmProgramHeader * kerAppImageTagGet(void);

/** Prototypes. **/
static int my_spi_read( unsigned char *msg_buf, int prependcnt, int nbytes, unsigned short slaveId );
static int my_spi_write( unsigned char *msg_buf, int nbytes, unsigned short slaveId );

int spi_flash_init(flash_device_info_t **flash_info);
static int spi_flash_sector_erase_int(unsigned short sector);
static int spi_flash_reset(void);
static int spi_flash_read_buf(unsigned short sector, int offset,
    unsigned char *buffer, int numbytes);
static int spi_flash_ub(unsigned short sector);
static int spi_flash_write_status(unsigned char status);
static int spi_flash_write_buf(unsigned short sector, int offset,
    unsigned char *buffer, int numbytes);
static int spi_flash_reset_ub(void);
static int spi_flash_get_numsectors(void);
static int spi_flash_get_sector_size(unsigned short sector);
static unsigned char *spi_get_flash_memptr(unsigned short sector);
static unsigned char *spi_flash_get_memptr(unsigned short sector);
static int spi_flash_write(unsigned short sector, int offset, unsigned char *buf,
    int nbytes,int ub, unsigned short slaveId);
static int spi_flash_status(void);
static unsigned short spi_flash_get_device_id(unsigned short sector);
static int spi_flash_get_blk(int addr);
static int spi_flash_get_total_size(void);
static int at_spi_flash_ub(unsigned short sector);
static int get_flash_partition(loff_t address);

#if defined(CONFIG_BCM93380) || defined(CONFIG_BCM93383)
void spi_register_remote_handler(void *p, void *arg);
#endif

/** Variables. **/
static flash_device_info_t flash_spi_dev =
    {
        0xffff,
        FLASH_IFC_SPI,
        "",
        spi_flash_sector_erase_int,
        spi_flash_read_buf,
        spi_flash_write_buf,
        spi_flash_get_numsectors,
        spi_flash_get_sector_size,
        spi_flash_get_memptr,
        spi_flash_get_blk,
        spi_flash_get_total_size
    };

static struct flash_name_from_id fnfi[] = SPI_FLASH_DEVICES;

static char bcmmtd_name[] = "bcmmtd_device";

static char *part_names[] = {"bootl", "image1", "image2", "linux", "linuxapps", "permnv", "dhtml", "dynnv"};

/* the controller will handle operati0ns that are greater than the FIFO size
   code that relies on READ_BUF_LEN_MAX, READ_BUF_LEN_MIN or spi_max_op_len
   could be changed */
#define READ_BUF_LEN_MAX   544    /* largest of the maximum transaction sizes for SPI */
#define READ_BUF_LEN_MIN   60     /* smallest of the maximum transaction sizes for SPI */
/* this is the slave ID of the SPI flash for use with the SPI controller */
#define SPI_FLASH_SLAVE_DEV_ID    0
#define SPI_FLASH_SLAVE_DEV_ID_2  1
/* clock defines for the flash */
#define SPI_FLASH_DEF_CLOCK       781000

/* default to smallest transaction size - updated later */
static int spi_max_op_len   = READ_BUF_LEN_MIN; 
static int fastRead         = 0;
static int flash_page_size  = FLASH_PAGE_256;

/* default to legacy controller - updated later */
static int spi_flash_clock  = SPI_FLASH_DEF_CLOCK;
static int spi_flash_busnum = LEG_SPI_BUS_NUM;

/* Pointer to flash map in memory */
struct mtd_partition *flash_parts;
struct flash_partition_t *flash_partition_ptr; 

/* Use spansion 32 bit flash commands */
static int spansion_32bit = 0;

#ifndef _CFE_                                                
static DECLARE_MUTEX(spi_flash_lock);
static bool bSpiFlashSlaveRes = FALSE;
#endif

/*********************************************************************/
/* 'meminfo' should be a pointer, but most C compilers will not      */
/* allocate static storage for a pointer without calling             */
/* non-portable functions such as 'new'.  We also want to avoid      */
/* the overhead of passing this pointer for every driver call.       */
/* Systems with limited heap space will need to do this.             */
/*********************************************************************/
static struct flashinfo meminfo; /* Flash information structure */
static int totalSize = 0;
static int flashFamily = FLASH_UNDEFINED;
static int sstAaiWordProgram = FALSE;
static int atUnlockBlock = FALSE;

#if defined(CONFIG_BCM93380) || defined(CONFIG_BCM93383)
static int (*rpc_flash_op)(void *, unsigned int, unsigned char *, int, int, unsigned char) = NULL; 
static void *rpc_handle = 0;
static int spi_dual_flash = 0;
extern int get_part_offset(void);
void spi_register_remote_handler(void *p, void *arg)
{
    rpc_handle = arg;
    rpc_flash_op = (int (*)(void *, unsigned int, unsigned char *, int, int, unsigned char))p;
    printk("SWITCHED to REMOTE FLASH!!!\n");
}
EXPORT_SYMBOL(spi_register_remote_handler);

static inline void GPIOSetOutput( unsigned int gpioBit, unsigned int value )
{
    uint32 mask;
    volatile unsigned int *pGPIO = (volatile unsigned int *)0xb4e00108;

    mask = ~(1 << gpioBit);
    *pGPIO = ((*pGPIO & mask) | (value << gpioBit));
}
#endif

static int my_spi_read(unsigned char *msg_buf, int prependcnt, int nbytes, unsigned short slaveId)
{
    int status; 
    
#ifndef _CFE_
    if ( FALSE == bSpiFlashSlaveRes)
#endif
    {
        status = BcmSpi_Read(msg_buf, prependcnt, nbytes, spi_flash_busnum, slaveId, spi_flash_clock);
    }
#ifndef _CFE_
    else
    {
        /* the Linux SPI framework provides a non blocking mechanism for SPI transfers. While waiting for a spi
           transaction to complete the kernel will look to see if another process can run. This scheduling 
           can only occur if kernel preemption is active. The SPI flash interfaces can be run when kernel
           preemption is enabled or disabled. When kernel preemption is disabled we cannot use the framework */
        if ( in_atomic() )
            status = BcmSpi_Read(msg_buf, prependcnt, nbytes, spi_flash_busnum, slaveId, spi_flash_clock);
        else
            status = BcmSpiSyncTrans(NULL, msg_buf, prependcnt, nbytes, spi_flash_busnum, slaveId);
    }
#endif

    return status;
}

static int my_spi_write(unsigned char *msg_buf, int nbytes, unsigned short slaveId)
{
    int status; 

#ifndef _CFE_
    if ( FALSE == bSpiFlashSlaveRes)
#endif
    {
        status = BcmSpi_Write(msg_buf, nbytes, spi_flash_busnum, slaveId, spi_flash_clock);
    }
#ifndef _CFE_
    else
    {
        /* the Linux SPI framework provides a non blocking mechanism for SPI transfers. While waiting for a spi
           transaction to complete the kernel will look to see if another process can run. This scheduling 
           can only occur if kernel preemtion is active. The SPI flash interfaces can be run when kernel
           preemption is enabled or disabled. When kernel preemption is disabled we cannot use the framework */
        if ( in_atomic() )
            status = BcmSpi_Write(msg_buf, nbytes, spi_flash_busnum, slaveId, spi_flash_clock);
        else
            status = BcmSpiSyncTrans(msg_buf, NULL, 0, nbytes, spi_flash_busnum, slaveId);
    }
#endif
    return status;
}


/*********************************************************************/
/* Init_flash is used to build a sector table. This information is   */
/* translated from erase_block information to base:offset information*/
/* for each individual sector. This information is then stored       */
/* in the meminfo structure, and used throughout the driver to access*/
/* sector information.                                               */
/*                                                                   */
/* This is more efficient than deriving the sector base:offset       */
/* information every time the memory map switches (since on the      */
/* development platform can only map 64k at a time).  If the entire  */
/* flash memory array can be mapped in, then the addition static     */
/* allocation for the meminfo structure can be eliminated, but the   */
/* drivers will have to be re-written.                               */
/*                                                                   */
/* The meminfo struct occupies 44 bytes of heap space, depending     */
/* on the value of the define MAXSECTORS.  Adjust to suit            */
/* application                                                       */ 
/*********************************************************************/

int spi_flash_init(flash_device_info_t **flash_info)
{
    struct flash_name_from_id *fnfi_ptr;
    int i=0, count=0;
    int basecount=0L;
    unsigned short device_id;
    int sectorsize = 0;
    int numsector = 0;
   
#if defined(CONFIG_BCM93380)
    spi_flash_busnum = LEG_SPI_BUS_NUM;
    spi_flash_clock = 25000000;
#endif
#if defined(CONFIG_BCM93383)
    spi_flash_busnum = HS_SPI_BUS_NUM;
    spi_flash_clock = 25000000;
#endif
#if defined(_BCM96816_) || defined(CONFIG_BCM96816)
    uint32 miscStrapBus = MISC->miscStrapBus;

    if (miscStrapBus & MISC_STRAP_BUS_LS_SPIM_ENABLED)
    {
        spi_flash_busnum = LEG_SPI_BUS_NUM;
        if ( miscStrapBus & MISC_STRAP_BUS_SPI_CLK_FAST )
        {
            spi_flash_clock = 20000000;
        }
        else
        {
            spi_flash_clock = 781000;
        }
    }
    else
    {
        spi_flash_busnum = HS_SPI_BUS_NUM;
        if ( miscStrapBus & MISC_STRAP_BUS_SPI_CLK_FAST )
        {
            spi_flash_clock = 40000000;
        }
        else
        {
            spi_flash_clock = 20000000;
        }
    }
#endif
#if defined(_BCM96328_) || defined(CONFIG_BCM96328) || defined(_BCM96362_) || defined(CONFIG_BCM96362)
    spi_flash_busnum = HS_SPI_BUS_NUM;
    spi_flash_clock  = 40000000;
#endif
#if defined(_BCM96368_) || defined(CONFIG_BCM96368)
    uint32 miscStrapBus = GPIO->StrapBus;
 
    if ( miscStrapBus & (1 << 6) )
    {
       spi_flash_clock = 20000000;
    }
    else
    {
       spi_flash_clock = 781000;
    }
#endif

    /* retrieve the maximum read/write transaction length from the SPI controller */
    spi_max_op_len = BcmSpi_GetMaxRWSize( spi_flash_busnum );

    if (HS_SPI_BUS_NUM == spi_flash_busnum)
    {
        flash_spi_dev.flash_type = FLASH_IFC_HS_SPI;
    }
    *flash_info = &flash_spi_dev;

#if 0
    /* 
     * in case of flash corrupt, the following steps can erase the flash
     * 1. jumper USE_SPI_SLAVE to make SPI in slave mode
     * 2. start up JTAG debuger and remove the USE_SPI_SLAVE jumper 
     * 3. run the following code to erase the flash
     */
    flash_sector_erase_int(0);
    flash_sector_erase_int(1);
    printk("flash_init: erase all sectors\n");
    return FLASH_API_OK;
#endif
#if defined(CONFIG_BCM93380)
	*(unsigned long *)0xb4e00188 = (*(unsigned long *)0xb4e00188 | 1<<19) & ~(1<<16);

    SPI->spiClkCfg = ((2 << 3) | 7);
#endif

#if defined(CONFIG_BCM_RAMDISK)
    flashFamily = FLASH_SPAN;
    sectorsize = SECTOR_SIZE_64K;
    numsector = SPAN25FL064A_SECTOR;
	basecount = 0;
#else 
#if defined(CONFIG_BCM93380)
    /* Chip select 0 */
    GPIOSetOutput( 14, 1 );
#endif
    flash_spi_dev.flash_device_id = device_id = spi_flash_get_device_id(0);
#if 1 // defined(CONFIG_SPI_FLASH_DEBUG)
    printk("device_id(%08x) = %08x\n", *(unsigned int *)0xb4e00108, (unsigned int)device_id);
#endif
#if defined(CONFIG_BCM93380) || defined(CONFIG_BCM93383)
#if defined(CONFIG_BCM933830)
    GPIOSetOutput( 14, 0 );
    device_id = spi_flash_get_device_id(0);
#else
    device_id = spi_flash_get_device_id(1);
#endif
#if 1 //defined(CONFIG_SPI_FLASH_DEBUG)
    printk("device_id(%08x) = %08x\n", *(unsigned int *)0xb4e00108, (unsigned int)device_id);
#endif
    if(device_id == flash_spi_dev.flash_device_id)
    {
        spi_dual_flash = 1;
    }
    else
    {
        device_id = flash_spi_dev.flash_device_id;
    }
    
#endif

    switch( device_id >> 8 ) {
        case ATMELPART:
            flashFamily = FLASH_ATMEL;
            switch ((char)(device_id & 0x00ff)) {
                case ID_AT25F512A:
                    flash_page_size = FLASH_PAGE_128;
                case ID_AT25F512:
                    numsector = AT25F512_SECTOR;
                    sectorsize = SECTOR_SIZE_32K;
                    break;
                case ID_AT25F2048:
                    numsector = AT25F2048_SECTOR;
                    sectorsize = SECTOR_SIZE_64K;
                    break;
                case ID_AT26F004:
                case ID_AT25DF041A:
                    flashFamily = FLASH_SST; 
                    numsector = 128;
                    sectorsize = SECTOR_SIZE_4K;
                    atUnlockBlock = TRUE;
                    break;
                default:
                    break;
            }
            break;

        case SSTPART:
            flashFamily = FLASH_SST;
            sectorsize = SECTOR_SIZE_4K;
            switch ((unsigned char)(device_id & 0x00ff)) {
                case ID_SST25VF016B:
                    numsector = SST25VF016B_SECTOR;
                    sstAaiWordProgram = TRUE;
                    break;
                case ID_SST25VF080:
                    numsector = SST25VF080_SECTOR;
                    break;
                case ID_SST25VF040:
                    numsector = SST25VF040_SECTOR;
                    break;
                case ID_SST25VF020:
                default:
                    numsector = SST25VF020_SECTOR;
                    break;
            }
            break;

        case AMD_S25FL002D:
            flashFamily = FLASH_AMD;
            numsector = AMD25FL_SECTOR;
            sectorsize = SECTOR_SIZE_64K;
            device_id &= 0xff00;
            break;

        case NXPART:
            /* NexFlash parts are AMD compatible. */
            flashFamily = FLASH_AMD;
            sectorsize = SECTOR_SIZE_64K;
            switch ((unsigned char)(device_id & 0x00ff)) {
                case ID_NX25P20:
                    numsector = 4;
                    break;
                case ID_NX25P40:
                    numsector = 8;
                    break;
                case ID_W25X16:
                    numsector = 32;
                    break;
                case ID_W25X32:
                    numsector = 64;
                    break;
                case ID_W25X64:
                    numsector = 128;
                    break;
            }
            break;

        case STPART:
            /* StFlash parts are AMD compatible. */
            flashFamily = FLASH_AMD;
            sectorsize = SECTOR_SIZE_64K;
            numsector = 8; /* 8 * 64KB == 512KB flash size */
            break;

        case SPANPART:
            flashFamily = FLASH_SPAN;
            sectorsize = SECTOR_SIZE_64K;
            switch ((unsigned short)(device_id & 0x00ff)) {
                case ID_SPAN25FL256S:
                    spansion_32bit = 1;
                    numsector = SPAN25FL256S_SECTOR;
                    break;
                case ID_SPAN25FL128P:
                    numsector = SPAN25FL128P_SECTOR;
                    break;
                case ID_SPAN25FL064A:
                    numsector = SPAN25FL064A_SECTOR;
                    break;
                case ID_SPAN25FL032A:
                    numsector = SPAN25FL032A_SECTOR;
                    break;
                case ID_SPAN25FL016A:
                    numsector = SPAN25FL016A_SECTOR;
                    break;
                case ID_SPAN25FL008A:
                    numsector = SPAN25FL008A_SECTOR;
                    break;
                case ID_SPAN25FL004A:
                default:
                    numsector = SPAN25FL004A_SECTOR;
                    break;
            }
            break;

        case EONPART:
        case NUMONYXPART:
        case MACRONIXPART:
            flashFamily = FLASH_SPAN;
            sectorsize = SECTOR_SIZE_64K;
            switch ((unsigned short)(device_id & 0x00ff)) {
                case ID_M25P64:
                    fastRead = 1;
                    numsector = 128;
                    break;
		case ID_M25L25:
                    spi_dual_flash = 0;
		    fastRead = 1;
		    numsector = 512;
		    break;
            }
            break;

        default:
            meminfo.addr = 0L;
            meminfo.nsect = 1;
            meminfo.sec[0].size = SECTOR_SIZE_4K;
            meminfo.sec[0].base = 0x00000;
            return FLASH_API_ERROR;
    }
#if defined(CONFIG_BCM93380) || defined(CONFIG_BCM93383)
    if(spi_dual_flash)
    {
        numsector *= 2;
    }
#endif
#endif
#if defined(CONFIG_SPI_FLASH_DEBUG)
    printk("numsector = %d sectorsize = %d basecount = %d\n", numsector, sectorsize, basecount);
#endif
    meminfo.addr = 0L;
    meminfo.nsect = numsector;
    for (i = 0; i < numsector; i++) {
        meminfo.sec[i].size = sectorsize;
        meminfo.sec[i].base = basecount;
        basecount += meminfo.sec[i].size;
        count++;
    }
    totalSize = meminfo.sec[count-1].base + meminfo.sec[count-1].size;

    for( fnfi_ptr = fnfi; fnfi_ptr->fnfi_id != 0; fnfi_ptr++ ) {
        if( fnfi_ptr->fnfi_id == device_id ) {
            strcpy( flash_spi_dev.flash_device_name, fnfi_ptr->fnfi_name ); 
#if defined(CONFIG_BCM93380)
            if(spi_dual_flash && (device_id == SPI_MAKE_ID(SPANPART, ID_SPAN25FL128P)))
            {
                strcpy( flash_spi_dev.flash_device_name, "S70FL256P"); 
            }
#endif
           break;
        }
    }

#if !defined(CONFIG_BCM_RAMDISK)
    if ( 0 == fastRead )
    {
        BcmSpi_SetFlashCtrl(FLASH_READ, 1, 0, spi_flash_busnum, SPI_FLASH_SLAVE_DEV_ID);
    }
    else
    {
        BcmSpi_SetFlashCtrl(FLASH_READ_FAST, 1, 1, spi_flash_busnum, SPI_FLASH_SLAVE_DEV_ID);
    }
#endif

    return (FLASH_API_OK);
}

/*********************************************************************/
/* Flash_sector_erase_int() wait until the erase is completed before */
/* returning control to the calling function.  This can be used in   */
/* cases which require the program to hold until a sector is erased, */
/* without adding the wait check external to this function.          */
/*********************************************************************/

static int spi_flash_sector_erase_int(unsigned short sector)
{
    unsigned char buf[4];
    unsigned int addr;
    unsigned short slaveId = 0;

    if (flashFamily == FLASH_UNDEFINED)
        return FLASH_API_ERROR;

#ifndef _CFE_
    if (!in_interrupt()) {
        down_interruptible(&spi_flash_lock);
    }
#endif

#if defined(CONFIG_BCM93380) || defined(CONFIG_BCM93383)
    if(spi_dual_flash)
    {
        if(sector < (spi_flash_get_numsectors()/2))
        {
	    #if defined(CONFIG_BCM93380)
            GPIOSetOutput( 14, 1 );
	    #endif
	    slaveId = 0;
        }
        else
        {
	    #if defined(CONFIG_BCM93380)
            GPIOSetOutput( 14, 0 );
	    #endif
	    slaveId = 1;
        }
    }
#endif
    /* set device to write enabled */
    spi_flash_reset();
    spi_flash_ub(sector);

    switch (flashFamily) {
        case FLASH_SST:
            buf[0] = SST_FLASH_SERASE;
            break;
        case FLASH_ATMEL:
            buf[0] = ATMEL_FLASH_SERASE;
            break;
        case FLASH_AMD:
            buf[0] = AMD_FLASH_SERASE;
            break;
        case FLASH_SPAN:
            buf[0] = SPAN_FLASH_SERASE;
            break;
    };

    /* erase the sector  */
    addr = (unsigned int) spi_get_flash_memptr(sector);
    buf[1] = (unsigned char)((addr & 0x00ff0000) >> 16);
    buf[2] = (unsigned char)((addr & 0x0000ff00) >> 8);
    buf[3] = (unsigned char)(addr & 0x000000ff);

    /* check device is ready */
    if (my_spi_write(buf, sizeof(buf), slaveId) == SPI_STATUS_OK) {
        while (spi_flash_status() != STATUS_READY);
    }

#ifndef _CFE_
    if (!in_interrupt()) {
        up(&spi_flash_lock);
    }
#endif

    return(FLASH_API_OK);
}

/*********************************************************************/
/* flash_chip_erase_int() wait until the erase is completed before   */
/* returning control to the calling function.  This can be used in   */
/* cases which require the program to hold until a sector is erased, */
/* without adding the wait check external to this function.          */
/*********************************************************************/

#if 0 /* not used */
unsigned char spi_flash_chip_erase_int(void)
{
    unsigned char buf[4];

    if (flashFamily == FLASH_UNDEFINED)
        return FLASH_API_ERROR;

    /* set device to write enabled */
    buf[0] = FLASH_WREN;

    /* check device is ready */
    if (my_spi_write(buf, 1) == SPI_STATUS_OK) {
        do {
            buf[0] = FLASH_RDSR;
            if (my_spi_read(buf, 1, 1) == SPI_STATUS_OK) {
                if (buf[0] & SR_WEN) {
                    break;
                }
            } else {
                break;
            }
        } while (1);
    }
    
    switch (flashFamily) {
        case FLASH_SST:
            buf[0] = SST_FLASH_CERASE;
            break;
        case FLASH_ATMEL:
            buf[0] = ATMEL_FLASH_CERASE;
            break;
        case FLASH_AMD:
            buf[0] = AMD_FLASH_CERASE;
            break;
    };
    /* erase the sector  */
    /* check device is ready */
    if (my_spi_write(buf, 1) == SPI_STATUS_OK) {
        while (spi_flash_status() != STATUS_READY);
    }

    return(FLASH_API_OK);
}
#endif

/*********************************************************************/
/* flash_reset() will reset the flash device to reading array data.  */
/* It is good practice to call this function after autoselect        */
/* sequences had been performed.                                     */
/*********************************************************************/

static int spi_flash_reset(void)
{
    if (flashFamily == FLASH_UNDEFINED)
        return FLASH_API_ERROR;
    spi_flash_reset_ub();
    while (spi_flash_status() != STATUS_READY);
    return(FLASH_API_OK);
}

/*********************************************************************/
/* flash_read_buf() reads buffer of data from the specified          */
/* offset from the sector parameter.                                 */
/*********************************************************************/

static int spi_flash_read_buf(unsigned short sector, int offset,
    unsigned char *buffer, int numbytes)
{
    unsigned char buf[READ_BUF_LEN_MAX];
    unsigned int addr;
    int maxread;
    int idx;
    unsigned char readCommand = fastRead ? FLASH_READ_FAST : FLASH_READ;
    unsigned char prependCnt = fastRead ? 5 : 4;
    unsigned short slaveId = 0;
	int ret = -1;

    if (flashFamily == FLASH_UNDEFINED)
        return FLASH_API_ERROR;

#ifndef _CFE_
    if (!in_interrupt()) {
        down_interruptible(&spi_flash_lock);
    }
#endif

#if defined(CONFIG_BCM93380) || defined(CONFIG_BCM93383)
    if(rpc_handle)
    {
		int partition; 
		int sector_offset;
		flash_partition_cache_t *fcache;

        /* get the device offset */
        addr = (unsigned int) spi_get_flash_memptr(sector);
        addr += offset;
        /* hardcode for now 8MB */
        // addr -= 0x00800000;
		partition = get_flash_partition(addr);
		fcache = flash_cache[partition];
		addr -= flash_partition_ptr[partition].offset;

		// Flash cache hit
		if (fcache->flashPartOffset != -1) 
		{
			if ((addr >= fcache->flashPartOffset) && ((addr+numbytes) <= (fcache->flashPartOffset + fcache->flashCacheBufSize)))
			{
				//printk("Cache hit!\n");
				memcpy((void *)buffer, (void *)(fcache->flashCacheBuf + (addr - fcache->flashPartOffset)), numbytes);
				if (!in_interrupt()) {
					up(&spi_flash_lock);
				}
				return FLASH_API_OK;
				// return numbytes;
			}
		}

		if ((fcache->flashPartOffset == -1) || (addr < fcache->flashPartOffset) || (addr >= (fcache->flashPartOffset + fcache->flashCacheBufSize)))
		{
			// Flash cache miss - go get flash sector from main MIPS
			//printk("Cache miss!\n");
			sector_offset = (addr / fcache->flashCacheBufSize) * fcache->flashCacheBufSize;
			ret = rpc_flash_op(rpc_handle, sector_offset, fcache->flashCacheBuf, fcache->flashCacheBufSize, partition, REMOTE_READ);
			if (ret < 0) {
				if (!in_interrupt()) {
					up(&spi_flash_lock);
				}
				return ret;
			}
			memcpy((void *)buffer, (void *)(fcache->flashCacheBuf + (addr - sector_offset)), numbytes);
			fcache->flashPartOffset = sector_offset;
		}
		else
		{
			ret = rpc_flash_op(rpc_handle, addr, buffer, numbytes, partition, REMOTE_READ);    
			if (ret < 0) {
				if (!in_interrupt()) {
					up(&spi_flash_lock);
				}
				return ret;
			}
		}
    }
    else
    {
        if(spi_dual_flash)
        {
            if(sector < (spi_flash_get_numsectors()/2))
            {
#if defined(CONFIG_BCM93380)
                GPIOSetOutput( 14, 1 );
#else
				slaveId = 0;
#endif
            }
            else
            {
#if defined (CONFIG_BCM93380)
                GPIOSetOutput( 14, 0 );
#else
				slaveId = 1;
#endif
            }
            //printk("*(0xb4e00108) = %08x\n", *(unsigned int *)0xb4e00108);
        }
#endif
		spi_flash_reset();

		addr = (unsigned int) spi_get_flash_memptr(sector);
		addr += offset;
		idx = 0;
		while (numbytes > 0) {
			maxread = (numbytes < spi_max_op_len) ? numbytes : spi_max_op_len;
			buf[0] = readCommand;
#if defined(CONFIG_BCM93380)
			if ((*((volatile unsigned int *)0xb4e00140) & 0x1) == 0)
#else
				if(spansion_32bit)
#endif
				{
					buf[0] = SPAN_FLASH_READ4;
					buf[1] = (unsigned char)((addr & 0xff000000) >> 24);
					buf[2] = (unsigned char)((addr & 0x00ff0000) >> 16);
					buf[3] = (unsigned char)((addr & 0x0000ff00) >> 8);
					buf[4] = (unsigned char)(addr & 0x000000ff);
					prependCnt++;
				}
				else
				{
					buf[1] = (unsigned char)((addr & 0x00ff0000) >> 16);
					buf[2] = (unsigned char)((addr & 0x0000ff00) >> 8);
					buf[3] = (unsigned char)(addr & 0x000000ff);
					buf[4] = (unsigned char)0xff;
				}
			my_spi_read(buf, prependCnt, maxread, slaveId);
			while (spi_flash_status() != STATUS_READY);
			memcpy(buffer+idx, buf, maxread);
			idx += maxread;
			numbytes -= maxread;
			addr += maxread;
		}

#if defined(CONFIG_BCM93380) || defined(CONFIG_BCM93383)
    }
#endif

#ifndef _CFE_
    if (!in_interrupt()) {
        up(&spi_flash_lock);
    }
#endif

    return (FLASH_API_OK);
}

/*********************************************************************/
/* at_spi_flash_ub() places the atmel new protect flash into unlock  */
/* bypass mode.                                                      */
/*********************************************************************/

static int at_spi_flash_ub(unsigned short sector)
{
    unsigned char buf[4];
    unsigned int addr = 0;

    addr = (unsigned int) spi_get_flash_memptr(sector);

    /* check the sector is unlocked */
    buf[0] = ATMEL_FLASH_RDPREG;
    buf[1] = (unsigned char)((addr & 0x00ff0000) >> 16);
    buf[2] = (unsigned char)((addr & 0x0000ff00) >> 8);
    buf[3] = (unsigned char)(addr & 0x000000ff);
    if (my_spi_read(buf, 4, 1, SPI_FLASH_SLAVE_DEV_ID) == SPI_STATUS_OK) {
        if(!(buf[0]&AT_SEC_LOCK))
        {
          return(FLASH_API_OK); 
        }
    }
    
    /* set device to write enabled */
    buf[0] = FLASH_WREN;
    if (my_spi_write(buf, 1, SPI_FLASH_SLAVE_DEV_ID) == SPI_STATUS_OK) {
        while (spi_flash_status() != STATUS_READY);
        buf[0] = ATMEL_FLASH_UNPSEC;
        buf[1] = (unsigned char)((addr & 0x00ff0000) >> 16);
        buf[2] = (unsigned char)((addr & 0x0000ff00) >> 8);
        buf[3] = (unsigned char)(addr & 0x000000ff);
        if (my_spi_write(buf, 4, SPI_FLASH_SLAVE_DEV_ID) == SPI_STATUS_OK)
            while (spi_flash_status() != STATUS_READY);
    }

      /* check the sector is unlocked */
    buf[0] = ATMEL_FLASH_RDPREG;
    buf[1] = (unsigned char)((addr & 0x00ff0000) >> 16);
    buf[2] = (unsigned char)((addr & 0x0000ff00) >> 8);
    buf[3] = (unsigned char)(addr & 0x000000ff);
    
    if (my_spi_read(buf, 4, 1, SPI_FLASH_SLAVE_DEV_ID) == SPI_STATUS_OK) {
        while (spi_flash_status() != STATUS_READY);
        if (!(buf[0] & AT_SEC_LOCK))
            return (FLASH_API_OK); 
        else
            return (FLASH_API_ERROR); 
    }
    return (FLASH_API_ERROR); 
}

/*********************************************************************/
/* flash_ub() places the flash into unlock bypass mode.  This        */
/* is REQUIRED to be called before any of the other unlock bypass    */
/* commands will become valid (most will be ignored without first    */
/* calling this function.                                            */
/*********************************************************************/

static int spi_flash_ub(unsigned short sector)
{
    unsigned char buf[4];

    if (atUnlockBlock) {
        while (at_spi_flash_ub(sector) != FLASH_API_OK);
    }
    else {
        do {
            buf[0] = FLASH_RDSR;
            if (my_spi_read(buf, 1, 1, SPI_FLASH_SLAVE_DEV_ID) == SPI_STATUS_OK) {
                while (spi_flash_status() != STATUS_READY);
                if (buf[0] & (SR_BP2|SR_BP1|SR_BP0)) {
                    spi_flash_write_status((unsigned char)~(SR_WPEN|SR_BP2|SR_BP1|SR_BP0));
                }
                else {
                    break;
                }
            }
            else {
                break;
            }
        } while (1);
    }

    /* set device to write enabled */
    buf[0] = FLASH_WREN;

    /* check device is ready */
    if (my_spi_write(buf, 1, SPI_FLASH_SLAVE_DEV_ID) == SPI_STATUS_OK) {
        while (spi_flash_status() != STATUS_READY);
        do {
            buf[0] = FLASH_RDSR;
            if (my_spi_read(buf, 1, 1, SPI_FLASH_SLAVE_DEV_ID) == SPI_STATUS_OK) {
                while (spi_flash_status() != STATUS_READY);
                if (buf[0] & SR_WEN) {
                    break;
                }
            } 
            else {
                break;
            }
        } while (1);
    }

    return(FLASH_API_OK);
}

static int spi_flash_write_status(unsigned char status)
{
    unsigned char buf[4];
    int rc = SPI_STATUS_OK;

    if (flashFamily == FLASH_UNDEFINED)
        return FLASH_API_ERROR;

    switch (flashFamily) {
        case FLASH_SST:
            buf[0] = FLASH_EWSR;
            rc = my_spi_write(buf, 1, SPI_FLASH_SLAVE_DEV_ID);
            break;
        case FLASH_SPAN:
            buf[0] = FLASH_WREN;
            rc = my_spi_write(buf, 1, SPI_FLASH_SLAVE_DEV_ID);
            break;
        default:
            break;
    }
    if (rc == SPI_STATUS_OK) {
        buf[0] = FLASH_WRST;
        buf[1] = (status & (SR_WPEN|SR_BP2|SR_BP1|SR_BP0));
        if (my_spi_write(buf, 2, SPI_FLASH_SLAVE_DEV_ID) == SPI_STATUS_OK)
            while (spi_flash_status() != STATUS_READY);
    }

    return FLASH_API_OK;
}

/*********************************************************************/
/* flash_write_buf() utilizes                                        */
/* the unlock bypass mode of the flash device.  This can remove      */
/* significant overhead from the bulk programming operation, and     */
/* when programming bulk data a sizeable performance increase can be */
/* observed.                                                         */
/*********************************************************************/

static int spi_flash_write_buf(unsigned short sector, int offset,
							   unsigned char *buffer, int numbytes)
{
    int ret = FLASH_API_ERROR;
    unsigned short slaveId = 0;

    if (flashFamily == FLASH_UNDEFINED)
        return FLASH_API_ERROR;

#ifndef _CFE_
	if (!in_interrupt()) {
		down_interruptible(&spi_flash_lock);
	}
#endif

#if defined(CONFIG_BCM93380) || defined(CONFIG_BCM93383)

    if(rpc_handle)
    {
		int partition; 
		int sector_offset;
		flash_partition_cache_t *fcache;
    	unsigned int addr;

        /* get the device offset */
        addr = (unsigned int) spi_get_flash_memptr(sector);
        addr += offset;
		partition = get_flash_partition(addr);
		fcache = flash_cache[partition];
		addr -= flash_partition_ptr[partition].offset;	

		ret = rpc_flash_op(rpc_handle, addr, buffer, numbytes, partition, REMOTE_WRITE);
		if (ret < 0) {
			if (!in_interrupt()) {
				up(&spi_flash_lock);
			}
			return ret;
		}
		/* Invalidate flash cache if the sector we wrote is cached */
		if (fcache->flashPartOffset != -1) 
		{
			if ((addr >= fcache->flashPartOffset) && ((addr+numbytes) <= (fcache->flashPartOffset + fcache->flashCacheBufSize)))
			{
				/* Invalidate cache by setting cache offset to -1 */
				fcache->flashPartOffset = -1;
			}
		}
#ifndef _CFE_
        if (!in_interrupt()) {
            up(&spi_flash_lock);
        }
#endif	
		return (FLASH_API_OK);
    }
    else
    {
        if(spi_dual_flash)
        {
            if(sector < (spi_flash_get_numsectors()/2))
            {
#if defined(CONFIG_BCM93380)
            	GPIOSetOutput( 14, 1 );
#endif
				slaveId = 0;
            }
            else
            {
#if defined(CONFIG_BCM93380)
                GPIOSetOutput( 14, 0 );
#endif
				slaveId = 1;
            }
        }
    }
#endif

    ret = spi_flash_write(sector, offset, buffer, numbytes, TRUE, slaveId);

#ifndef _CFE_
	if (!in_interrupt()) {
		up(&spi_flash_lock);
	}
#endif

    if( ret == -1 )
        printk( "Flash write error.  Verify failed\n" );

    return( ret );
}

/*********************************************************************/
/* flash_reset_ub() is required to remove the flash from unlock      */
/* bypass mode.  This is important, as other flash commands will be  */
/* ignored while the flash is in unlock bypass mode.                 */
/*********************************************************************/

static int spi_flash_reset_ub(void)
{
    unsigned char buf[4];

    if (flashFamily == FLASH_UNDEFINED)
        return FLASH_API_ERROR;
    /* set device to write disabled */
    buf[0] = FLASH_WRDI;
    my_spi_write(buf, 1, SPI_FLASH_SLAVE_DEV_ID);
    while (spi_flash_status() != STATUS_READY);

    return(FLASH_API_OK);
}

/*********************************************************************/
/* Usefull funtion to return the number of sectors in the device.    */
/* Can be used for functions which need to loop among all the        */
/* sectors, or wish to know the number of the last sector.           */
/*********************************************************************/

static int spi_flash_get_numsectors(void)
{
    return meminfo.nsect;
}

/*********************************************************************/
/* flash_get_sector_size() is provided for cases in which the size   */
/* of a sector is required by a host application.  The sector size   */
/* (in bytes) is returned in the data location pointed to by the     */
/* 'size' parameter.                                                 */
/*********************************************************************/

static int spi_flash_get_sector_size(unsigned short sector)
{
    return meminfo.sec[sector].size;
}

/*********************************************************************/
/* The purpose of get_flash_memptr() is to return a memory pointer   */
/* which points to the beginning of memory space allocated for the   */
/* flash.  All function pointers are then referenced from this       */
/* pointer.                                  */
/*                                                                   */
/* Different systems will implement this in different ways:          */
/* possibilities include:                                            */
/*  - A direct memory pointer                                        */
/*  - A pointer to a memory map                                      */
/*  - A pointer to a hardware port from which the linear             */
/*    address is translated                                          */
/*  - Output of an MMU function / service                            */
/*                                                                   */
/* Also note that this function expects the pointer to a specific    */
/* sector of the device.  This can be provided by dereferencing      */
/* the pointer from a translated offset of the sector from a         */
/* global base pointer (e.g. flashptr = base_pointer + sector_offset)*/
/*                                                                   */
/* Important: Many AMD flash devices need both bank and or sector    */
/* address bits to be correctly set (bank address bits are A18-A16,  */
/* and sector address bits are A18-A12, or A12-A15).  Flash parts    */
/* which do not need these bits will ignore them, so it is safe to   */
/* assume that every part will require these bits to be set.         */
/*********************************************************************/

static unsigned char *spi_get_flash_memptr(unsigned short sector)
{
    unsigned char *memptr = (unsigned char*)
        (FLASH_BASE + meminfo.sec[sector].base);

    return (memptr);
}

static unsigned char *spi_flash_get_memptr(unsigned short sector)
{
    return( spi_get_flash_memptr(sector) );
}

/*********************************************************************/
/* Flash_write extends the functionality of flash_program() by       */
/* providing an faster way to program multiple data words, without   */
/* needing the function overhead of looping algorithms which         */
/* program word by word.  This function utilizes fast pointers       */
/* to quickly loop through bulk data.                                */
/*********************************************************************/
static int spi_flash_write(unsigned short sector, int offset, unsigned char *buf,
    int nbytes, int ub, unsigned short slaveId)
{
    unsigned char wbuf[FLASH_PAGE_256+32];
    unsigned int addr;
    unsigned int dst;
    unsigned char *pbuf;
    int cmdlen;
    int maxwrite;
    int pagelimit;

    addr = (unsigned int) spi_get_flash_memptr(sector);
    dst = addr + offset;
#if 0
    printk("spi_flash_write: addr=%08x dst=%08x offset=%d, FLASH_BASE=%08x (int)FLASH_BASE=%d\n",
        addr, dst, offset, FLASH_BASE, (int)FLASH_BASE);
    printk("spi_flash_write: %08x\n", (unsigned int)addr-(unsigned int)FLASH_BASE);
    printk("spi_flash_write: %08x\n", (unsigned int)((int)addr-(int)FLASH_BASE));
    printk("spi_flash_write: %08x\n", (int)addr+(int)FLASH_BASE);
#endif    
    pbuf = buf;
    switch (flashFamily) {
        case FLASH_SST:
            if( sstAaiWordProgram == FALSE )
            {
                /* Auto Address Increment one byte at a time. */
                spi_flash_ub(sector); /* enable write */
                wbuf[0] = FLASH_AAI;
                while (nbytes) {
                    if (pbuf != buf) {
                        wbuf[1] = *pbuf; 
                        cmdlen = CMD_LEN_1;
                    } else {
                        wbuf[1] = (unsigned char)((dst & 0x00ff0000) >> 16);
                        wbuf[2] = (unsigned char)((dst & 0x0000ff00) >> 8);
                        wbuf[3] = (unsigned char)(dst & 0x000000ff);
                        wbuf[4] = *pbuf;
                        cmdlen = CMD_LEN_4;
                    }
                    my_spi_write(wbuf, 1+cmdlen, slaveId);
                    while (spi_flash_status() != STATUS_READY);
                    pbuf++; /* update address and count by one byte */
                    nbytes--;
                }
                spi_flash_reset_ub();
                while (spi_flash_status() != STATUS_READY);
            }
            else
            {
                /* Auto Address Increment one word (2 bytes) at a time. */
                spi_flash_ub(sector); /* enable write */
                wbuf[0] = FLASH_WORD_AAI;
                while (nbytes) {
                    if (pbuf != buf) {
                        wbuf[1] = *pbuf; 
                        wbuf[2] = *(pbuf + 1); 
                        cmdlen = 3;
                    } else {
                        wbuf[1] = (unsigned char)((dst & 0x00ff0000) >> 16);
                        wbuf[2] = (unsigned char)((dst & 0x0000ff00) >> 8);
                        wbuf[3] = (unsigned char)(dst & 0x000000ff);
                        wbuf[4] = *pbuf;
                        wbuf[5] = *(pbuf + 1); 
                        cmdlen = 6;
                    }
                    my_spi_write(wbuf, cmdlen, slaveId);
                    while (spi_flash_status() != STATUS_READY);
                    pbuf += 2; /* update address and count by two bytes */
                    nbytes -= 2;
                }
                spi_flash_reset_ub();
                while (spi_flash_status() != STATUS_READY);
            }
            break;

        case FLASH_ATMEL:
        case FLASH_AMD:
        case FLASH_SPAN:
            while (nbytes > 0) {
                spi_flash_ub(sector); /* enable write */
                /* set length to the smaller of controller limit (spi_max_op_len) or nbytes
                   spi_max_op_len considers both controllers */
                maxwrite = (nbytes < (spi_max_op_len-CMD_LEN_4))
                    ? nbytes : (spi_max_op_len-CMD_LEN_4);
                /* maxwrite is limit to page boundary */
                pagelimit = flash_page_size - (dst & (flash_page_size - 1));
                maxwrite = (maxwrite < pagelimit) ? maxwrite : pagelimit;

                wbuf[0] = FLASH_PROG;
                wbuf[1] = (unsigned char)((dst & 0x00ff0000) >> 16);
                wbuf[2] = (unsigned char)((dst & 0x0000ff00) >> 8);
                wbuf[3] = (unsigned char)(dst & 0x000000ff);
                memcpy(&wbuf[4], pbuf, maxwrite);
                my_spi_write(wbuf, maxwrite+CMD_LEN_4, slaveId);
                while (spi_flash_status() != STATUS_READY);
                pbuf += maxwrite;
                nbytes -= maxwrite;
                dst += maxwrite;
            }
            break;

        default:
            return 0;
    }
    return (pbuf-buf);
}

/*********************************************************************/
/* Flash_status return an appropriate status code                    */
/*********************************************************************/

static int spi_flash_status(void)
{
    unsigned char buf[4];
    int retry = 10;

    /* check device is ready */
    do {
        buf[0] = FLASH_RDSR;
        if (my_spi_read(buf, 1, 1, SPI_FLASH_SLAVE_DEV_ID) == SPI_STATUS_OK) {
            if (!(buf[0] & SR_RDY)) {
                return STATUS_READY;
            }
        } else {
            return STATUS_ERROR;
        }
        OSL_DELAY(10);
    } while (retry--);

    return STATUS_TIMEOUT;
}

/*********************************************************************/
/* flash_get_device_id() return the device id of the component.      */
/*********************************************************************/

static unsigned short spi_flash_get_device_id(unsigned short altSlave)
{
    unsigned char buf[4];
    int prependcnt;
    int i, j;

    for (i = 0; i < MAX_RETRY; i++) {
        /* read product id command */
        buf[0] = SST_FLASH_RDID;
        buf[1] = 0;
        buf[2] = 0;
        buf[3] = 0;
        prependcnt = 4;
        my_spi_read(buf, prependcnt, sizeof(unsigned short), altSlave);
        for (j = 0; j < MAX_RETRY * 10; j++) {
            if (spi_flash_status() == STATUS_READY) {
                if (buf[0] == SSTPART || buf[0] == NXPART)
                    return( *(unsigned short *)&buf[0] );
            }
        }
    }
    spi_flash_reset();

    for (i = 0; i < MAX_RETRY; i++) {
        buf[0] = ATMEL_FLASH_RDID;
        prependcnt = 1;
        my_spi_read(buf, prependcnt, sizeof(unsigned short), altSlave);
        for (j = 0; j < MAX_RETRY * 10; j++) {
            if (spi_flash_status() == STATUS_READY) {
                if (buf[0] == ATMELPART)
                    return( *(unsigned short *)&buf[0] );
            }
        }
    }
    spi_flash_reset();

    for (i = 0; i < MAX_RETRY; i++) {
        buf[0] = SPAN_FLASH_RDID;
        prependcnt = 1;
        my_spi_read(buf, prependcnt, 3, altSlave);
        for (j = 0; j < MAX_RETRY * 10; j++) {
            if (spi_flash_status() == STATUS_READY) {
                //ATMEL AT26xxx Series RDID =  SPAN_FLASH_RDID  = 9F   
                if(buf[0] == ATMELPART)
                    return( *(unsigned short *)&buf[0] ); 

                if ((buf[0] == NUMONYXPART) || (buf[0] == SPANPART) || (buf[0] == EONPART) || (buf[0] == MACRONIXPART)) {
                    buf[1] = buf[2];
                    return( *(unsigned short *)&buf[0] );
                }
            }
        }
    }
    spi_flash_reset();

    // AMD_FLASH_RDID is the same as RES command for SPAN,
    // so it has to be the last one.

    for (i = 0; i < MAX_RETRY; i++) {
        buf[0] = AMD_FLASH_RDID;
        buf[1] = 0;
        buf[2] = 0;
        buf[3] = 0;
        prependcnt = 4;
        my_spi_read(buf, prependcnt, sizeof(unsigned short), altSlave);
        for (j = 0; j < MAX_RETRY * 10; j++) {
            if (spi_flash_status() == STATUS_READY) {
                if (buf[0] == AMD_S25FL002D || buf[0] == STPART)
                    return( *(unsigned short *)&buf[0] );
            }
        }
     }
    /* return manufacturer code and device code */
    return( *(unsigned short *)&buf[0] );
}

/*********************************************************************/
/* The purpose of flash_get_blk() is to return a the block number */
/* for a given memory address.                                       */
/*********************************************************************/

static int spi_flash_get_blk(int addr)
{
    int blk_start, i;
    int last_blk = spi_flash_get_numsectors();
//#if defined(CONFIG_BCM93380)
//	int relative_addr = addr;
//#else
    int relative_addr = addr - (int) FLASH_BASE;
//#endif
    for(blk_start=0, i=0; i < relative_addr && blk_start < last_blk; blk_start++)
        i += spi_flash_get_sector_size(blk_start);

    if( (unsigned int)i > (unsigned int)relative_addr ) {
        blk_start--;        // last blk, dec by 1
    } else {
        if( blk_start == last_blk )
        {
            printk("Address is too big: %d\n", blk_start);
            //*(unsigned int *)0 = 0;
            blk_start = -1;
        }
    }

    return( blk_start );
}

/************************************************************************/
/* The purpose of flash_get_total_size() is to return the total size of */
/* the flash                                                            */
/************************************************************************/
static int spi_flash_get_total_size(void)
{
    return totalSize;
}

static int init_partition_flash_cache(unsigned char partition)
{
    flash_cache[partition] = kzalloc(sizeof(flash_partition_cache_t),GFP_KERNEL);
    flash_cache[partition]->flashPartOffset = -1;
    flash_cache[partition]->flashCacheBufSize = spi_flash_get_sector_size(0);
    flash_cache[partition]->flashCacheBuf = (unsigned char *)kmalloc(flash_cache[partition]->flashCacheBufSize, GFP_KERNEL);
}

static int get_flash_partition(loff_t address)
{
    int i;

    for (i = 0; i < NUM_FLASH_PARTITIONS; i++)
	if ((address >= flash_parts[i].offset) && (address < flash_parts[i].offset + flash_parts[i].size))
	    return i;

    return -1;
}

static int bcmmtd_erase(struct mtd_info *mtd, struct erase_info *instr)
{
    int sector;
    uint32_t rem;
    u32 addr,len;
    int partition;
	int ret;

    /* sanity checks */
    if (instr->addr + instr->len > mtd->size)
		return -EINVAL;

    div_u64_rem(instr->len, mtd->erasesize, &rem);

    if (rem)
		return -EINVAL;

    addr = instr->addr;
    len = instr->len;

    /* whole-chip erase is not supported */
    if (len == mtd->size) 
    {
        instr->state = MTD_ERASE_FAILED;
        return -EIO;
    }
    else 
    { 

        while (len) 
        {
			if (rpc_handle)
			{
                /* get the device offset */
				partition = get_flash_partition(addr);
				addr -= flash_partition_ptr[partition].offset;
				sector = ( addr / 65536 ) ;

				ret = rpc_flash_op(rpc_handle, sector, (unsigned char *)0 , 65536, partition, REMOTE_ERASE);
				if (ret < 0)
					return ret;
			}
			else
			{
            	sector = addr/meminfo.sec[0].size;
            	if(spi_flash_sector_erase_int(sector)==FLASH_API_ERROR)
            	{
					instr->state = MTD_ERASE_FAILED;
					return -EIO;
				}
			}
    	    addr += mtd->erasesize;
			len -= mtd->erasesize;
		}
    }

    instr->state = MTD_ERASE_DONE;
    mtd_erase_callback(instr);

    return 0;
}

static int bcmmtd_read(struct mtd_info *mtd, loff_t from, size_t len,
	size_t *retlen, u_char *buf)
{
    int sector;
    int sector_offset;
    int partition;

    if (retlen)
 	*retlen = 0;

    /* sanity checks */
    if (!len)
	return 0;

    if (from + len > mtd->size)
	return -EINVAL;


    /* For uniform sectors, let's make an optimization */
    sector = (unsigned int)from/meminfo.sec[0].size;
    sector_offset = (unsigned int)from%meminfo.sec[0].size;

    /* 
     * Check if this is the first time someone's trying to read 
     * from this partition. If so, setup the partition cache
     * TODO:  This needs to be enclosed in some sort of conditional
     *        code so that we don't wastefully do the caching if
     *        Linux has direct access to flash instead of remote access.
     */
    partition = get_flash_partition(from);
    if (partition < 0)
    {
	return -ENODEV;
    }

    if (!flash_cache[partition])
	init_partition_flash_cache(partition);

    //printk("MTDRead: off: 0x%08x, len: %d, sector: %d, sect_off: %d\n", (unsigned int) from, len, sector, sector_offset);
    int ret = spi_flash_read_buf(sector, sector_offset, (void *)buf, len);
	if (ret < 0)
		return ret;
	
    /* set return length */
    if (retlen)
	*retlen = len;

    return 0;
}

static int bcmmtd_write(struct mtd_info *mtd, loff_t to, size_t len,
	size_t *retlen, const u_char *buf)
{
    int sector;
    int sector_offset;

    if (retlen)
		*retlen = 0;

    /* sanity checks */
    if (!len)
        return(0);

    if (to + len > mtd->size)
        return -EINVAL;

    sector = (unsigned int)to/meminfo.sec[0].size;
    sector_offset = (unsigned int)to%meminfo.sec[0].size;

    //printk("MTDWRITE: off: 0x%08x, len: %d, sector: %d, sect_off: %d oob_size: %d\n", (unsigned int) to, len, sector, sector_offset, mtd->oobsize);
    int ret = spi_flash_write_buf(sector, sector_offset, (unsigned char *)buf, len);
	if (ret < 0)
		return ret;

    /* set return length */
    if (retlen)
		*retlen = len;

    return 0;
}

int __init get_partition_map(struct mtd_partition **p)
{
    int nr_parts;
    BcmProgramHeader * pHdr = NULL;

    printk("bcm9338x_mtd driver\n");

    /* 
     * Look for flash map signature stored by bootloader in RAM
     * If it exists, then we can parse the flash map passed in
     * by bootloader to figure out the partitions.
     */
    if (*((unsigned int *)(FLASH_MAP_RAM_OFFSET)) == FLASH_MAP_SIGNATURE)
    {
	int i;
	// struct flash_partition_t *flash_partition_ptr = (struct flash_partition_t *)(FLASH_MAP_RAM_OFFSET+4);

	flash_partition_ptr = (struct flash_partition_t *)kzalloc(sizeof(struct flash_partition_t) * NUM_FLASH_PARTITIONS, GFP_KERNEL);
	if (!flash_partition_ptr)
 	    return -ENOMEM;
	memcpy((void *)flash_partition_ptr, (void *)(FLASH_MAP_RAM_OFFSET+4), sizeof(struct flash_partition_t) * NUM_FLASH_PARTITIONS);

	nr_parts = NUM_FLASH_PARTITIONS;
        flash_parts = kzalloc(sizeof(struct mtd_partition) * nr_parts, GFP_KERNEL);
        if (! flash_parts)
 	    return -ENOMEM;

	/* Allocate an array of pointers to flash_caches */
	flash_cache = kzalloc(sizeof(unsigned int) * nr_parts, GFP_KERNEL);
	
	for (i = 0; i < NUM_FLASH_PARTITIONS; i++)
	{
	    if (i == LINUX_KERNEL_ROOTFS_PARTITION)
	    {
		/* Read the flash memory map from flash memory. */
    		if (!(pHdr = kerSysImageTagGet()))
    		{
  		    printk("Failed to read image hdr from flash\n");
		    return -EIO;
    		}

    		/* Get rootfs offset */
    		flash_parts[i].offset = flash_partition_ptr[i].offset + sizeof(BcmProgramHeader) + 
			           pHdr->ulfilelength - pHdr->ulCompressedLength2;
		flash_parts[i].size = pHdr->ulCompressedLength2;

	    }
	    #if defined(CONFIG_BCM93383)
            else if (i == LINUX_APPS_PARTITION)
	    {
		if (!(pHdr = kerAppImageTagGet()))
		{
		    printk("Failed to read apps image hdr from flash\n");
		    flash_parts[i].offset = flash_partition_ptr[i].offset;
		}
		else
		{
		    // flash_parts[i].offset = flash_partition_ptr[i].offset + sizeof(BcmProgramHeader);
		    flash_parts[i].offset = flash_partition_ptr[i].offset + 65536;
		}
		flash_parts[i].size = flash_partition_ptr[i].size - 65536;
	    }
	    #endif 
            else
	    {
	        flash_parts[i].offset = flash_partition_ptr[i].offset;
	    	flash_parts[i].size = flash_partition_ptr[i].size;
	    }
	    flash_parts[i].name = kzalloc(11, GFP_KERNEL);
	    strncpy(flash_parts[i].name, flash_partition_ptr[i].name, 10);
	    flash_parts[i].name[10] = '\0';
            #if defined(CONFIG_BCM93383)
	    /* Everything but the apps partition is non-writeable */
	    if (i != LINUX_APPS_PARTITION)
	        flash_parts[i].mask_flags = MTD_WRITEABLE;
            #endif
	}

	/* Allocate and setup the flash caches for the partitions Linux accesses remotely */
	{
	    init_partition_flash_cache(LINUX_KERNEL_ROOTFS_PARTITION);
            #if defined(CONFIG_BCM93383)
	    init_partition_flash_cache(LINUX_APPS_PARTITION);
            #endif
	}
    }
    else
    {
	printk("NO FLASH MAP FOUND!!!\n");
	return 0;
    }
	   
    *p = flash_parts;
    return nr_parts;
}


static int __init BcmSpiflash_init(void)
{
    int                   flashType;
#if !defined(CONFIG_BCM_RAMDISK)
    int	                  nr_parts = 0;    
    struct mtd_info       *mtd;
    struct mtd_partition  *parts = NULL;
#endif

    /* If serial flash is present then register the device. Otherwise do nothing */
    flashType  = flash_get_flash_type();
    if ((FLASH_IFC_SPI == flashType) || (FLASH_IFC_HS_SPI == flashType))
    {
        /* register the device */
        BcmSpiReserveSlave(spi_flash_busnum, SPI_FLASH_SLAVE_DEV_ID, spi_flash_clock);
	BcmSpiReserveSlave(spi_flash_busnum, SPI_FLASH_SLAVE_DEV_ID_2, spi_flash_clock);
        bSpiFlashSlaveRes = TRUE;
    }

#if !defined(CONFIG_BCM_RAMDISK)
    /* Set up MTD INFO struct */
    mtd = kzalloc(sizeof(*mtd), GFP_KERNEL);
    if (!mtd)
    {
	return -ENODEV;
    }

    mtd->name = bcmmtd_name;
    mtd->type = MTD_NORFLASH;
    mtd->size = 32 * 1024 * 1024;
    mtd->read = bcmmtd_read; 
    mtd->write = bcmmtd_write;
    mtd->erase = bcmmtd_erase;
    mtd->erasesize = 65536;
    mtd->writesize = 1;
    mtd->flags = MTD_CAP_NORFLASH;

    if ((nr_parts = get_partition_map(&parts)) > 0)
    {
	return add_mtd_partitions(mtd, parts, nr_parts);
    }
    else
    {
	return -ENODEV;
    }
#else
    return 0;
#endif
}
module_init(BcmSpiflash_init);

static void __exit BcmSpiflash_exit(void)
{
    bSpiFlashSlaveRes = FALSE;
}
module_exit(BcmSpiflash_exit);
