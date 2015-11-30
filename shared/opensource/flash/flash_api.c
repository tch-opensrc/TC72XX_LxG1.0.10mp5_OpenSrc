/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2, as published by
the Free Software Foundation (the "GPL").

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GPL is available at http://www.gnu.org/licenses/old-licenses/gpl-2.0.html, or by
writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.

<:license-gpl
*/
/***************************************************************************
 * File Name  : flash_api.c
 *
 * Description: This file contains the implementation of the wrapper functions
 *              for the flash device interface.
 ***************************************************************************/

/** Includes. */
#ifdef _CFE_
#include "lib_types.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "bcm_map.h"  
#define printk  printf
#else // Linux
#include <linux/kernel.h>
#include <linux/module.h>
#include "bcm_map_part.h"
#endif

#include "bcmtypes.h"
#include "bcm_hwdefs.h"
#include "flash_api.h"

/** Externs. **/
#if !defined(INC_CFI_FLASH_DRIVER)
#define INC_CFI_FLASH_DRIVER        0  
#endif

#if !defined(INC_SPI_FLASH_DRIVER)
#define INC_SPI_FLASH_DRIVER        0
#endif

#if !defined(INC_NAND_FLASH_DRIVER)
#define INC_NAND_FLASH_DRIVER       0
#endif

#if (INC_CFI_FLASH_DRIVER==1)
extern int cfi_flash_init(flash_device_info_t **flash_info);
#else
#define cfi_flash_init(x)           FLASH_API_ERROR
#endif

#if (INC_SPI_FLASH_DRIVER==1)
extern int spi_flash_init(flash_device_info_t **flash_info);
#else
#define spi_flash_init(x)           FLASH_API_ERROR
#endif

#if (INC_NAND_FLASH_DRIVER==1)
extern int nand_flash_init(flash_device_info_t **flash_info);
#else
#define nand_flash_init(x)           FLASH_API_ERROR
#endif

/** Variables. **/
static flash_device_info_t *g_flash_info = NULL;

/***************************************************************************
 * Function Name: display_flash_info
 * Description  : Displays information about the flash part.
 * Returns      : None.
 ***************************************************************************/
static void display_flash_info(int ret, flash_device_info_t *flash_info)
{
    switch (flash_info->flash_type) {
    case FLASH_IFC_PARALLEL:
        printk( "Parallel flash device");
        break;

    case FLASH_IFC_SPI:
        printk( "Serial flash device");
        break;

    case FLASH_IFC_HS_SPI:
        printk( "HS Serial flash device");
        break;

    case FLASH_IFC_NAND:
        printk( "NAND flash device");
        break;
    }

    if( ret == FLASH_API_OK ) {
        printk(": name %s, id 0x%4.4x",
            flash_info->flash_device_name, flash_info->flash_device_id);
        printk(" size %dKB", flash_get_total_size() / 1024);
        printk("\n");
    }
    else {
        printk( " id %4.4x is not supported.\n", flash_info->flash_device_id );
    }
} /* display_flash_info */

/***************************************************************************
 * Function Name: flash_init
 * Description  : Initialize flash part.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
int flash_init(void)
{
    int type = FLASH_IFC_UNKNOWN;
    int ret = FLASH_API_ERROR;

    /* If available, use bootstrap to decide which flash to use */
#if defined(_BCM96816_) || defined(CONFIG_BCM96816) || defined(_BCM96368_) || defined(CONFIG_BCM96368)
    unsigned int bootsel;
#if defined(_BCM96816_) || defined(CONFIG_BCM96816)
    bootsel = MISC->miscStrapBus;
#elif defined(_BCM96368_) || defined(CONFIG_BCM96368)
    bootsel = GPIO->StrapBus;
#endif
    switch ((bootsel & MISC_STRAP_BUS_BOOT_SEL_MASK)>>MISC_STRAP_BUS_BOOT_SEL_SHIFT) {
    case MISC_STRAP_BUS_BOOT_PARALLEL:
        type = FLASH_IFC_PARALLEL;
        break;

    case MISC_STRAP_BUS_BOOT_SERIAL:
        type = FLASH_IFC_SPI;
        break;

    case MISC_STRAP_BUS_BOOT_NAND:
        type = FLASH_IFC_NAND;
        break;

    }
#elif defined(_BCM96362_) || defined(CONFIG_BCM96362) || defined(_BCM96328_) || defined(CONFIG_BCM96328) 
    if( ((MISC->miscStrapBus & MISC_STRAP_BUS_BOOT_SEL_MASK) >>
        MISC_STRAP_BUS_BOOT_SEL_SHIFT) ==  MISC_STRAP_BUS_BOOT_SERIAL )
        type = FLASH_IFC_SPI;
    else
        type = FLASH_IFC_NAND;
#elif defined(CONFIG_BCM93380)
    type = FLASH_IFC_SPI;
#endif

    switch (type) {
    case FLASH_IFC_PARALLEL:
        ret = cfi_flash_init( &g_flash_info );
        break;

    case FLASH_IFC_SPI:
        ret = spi_flash_init( &g_flash_info );
        break;

    case FLASH_IFC_NAND:
        ret = nand_flash_init( &g_flash_info );
        break;

    case FLASH_IFC_UNKNOWN:
        /* Try to detect flash chips, give priority to parallel flash */
        /* Our reference design has both, and we usually use parallel. */
        ret = cfi_flash_init( &g_flash_info );
        if (ret != FLASH_API_OK) {
            ret = spi_flash_init( &g_flash_info );
        }
        break;
    }

    if (g_flash_info != NULL) {
        display_flash_info(ret, g_flash_info);
    }
    else {
        printk( "BCM Flash API. Flash device is not found.\n" );
    }

    return( ret );
} /* flash_init */

/***************************************************************************
 * Function Name: flash_sector_erase_int
 * Description  : Erase the specfied flash sector.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
int flash_sector_erase_int(unsigned short sector)
{
    return( (g_flash_info)
        ? (*g_flash_info->fn_flash_sector_erase_int) (sector)
        : FLASH_API_ERROR );
} /* flash_sector_erase_int */

/***************************************************************************
 * Function Name: flash_read_buf
 * Description  : Reads from flash memory.
 * Returns      : number of bytes read or FLASH_API_ERROR
 ***************************************************************************/
int flash_read_buf(unsigned short sector, int offset, unsigned char *buffer,
    int numbytes)
{
    return( (g_flash_info)
        ? (*g_flash_info->fn_flash_read_buf) (sector, offset, buffer, numbytes)
        : FLASH_API_ERROR );
} /* flash_read_buf */

/***************************************************************************
 * Function Name: flash_write_buf
 * Description  : Writes to flash memory.
 * Returns      : number of bytes written or FLASH_API_ERROR
 ***************************************************************************/
int flash_write_buf(unsigned short sector, int offset, unsigned char *buffer,
    int numbytes)
{
    return( (g_flash_info)
        ? (*g_flash_info->fn_flash_write_buf) (sector, offset, buffer, numbytes)
        : FLASH_API_ERROR );
} /* flash_write_buf */

/***************************************************************************
 * Function Name: flash_get_numsectors
 * Description  : Returns the number of sectors in the flash device.
 * Returns      : Number of sectors in the flash device.
 ***************************************************************************/
int flash_get_numsectors(void)
{
    return( (g_flash_info)
        ? (*g_flash_info->fn_flash_get_numsectors) ()
        : FLASH_API_ERROR );
} /* flash_get_numsectors */

/***************************************************************************
 * Function Name: flash_get_sector_size
 * Description  : Returns the number of bytes in the specfied flash sector.
 * Returns      : Number of bytes in the specfied flash sector.
 ***************************************************************************/
int flash_get_sector_size(unsigned short sector)
{
    return( (g_flash_info)
        ? (*g_flash_info->fn_flash_get_sector_size) (sector)
        : FLASH_API_ERROR );
} /* flash_get_sector_size */

/***************************************************************************
 * Function Name: flash_get_memptr
 * Description  : Returns the base MIPS memory address for the specfied flash
 *                sector.
 * Returns      : Base MIPS memory address for the specfied flash sector.
 ***************************************************************************/
unsigned char *flash_get_memptr(unsigned short sector)
{
    return( (g_flash_info)
        ? (*g_flash_info->fn_flash_get_memptr) (sector)
        : NULL );
} /* flash_get_memptr */

/***************************************************************************
 * Function Name: flash_get_blk
 * Description  : Returns the flash sector for the specfied MIPS address.
 * Returns      : Flash sector for the specfied MIPS address.
 ***************************************************************************/
int flash_get_blk(int addr)
{
    return( (g_flash_info)
        ? (*g_flash_info->fn_flash_get_blk) (addr)
        : FLASH_API_ERROR );
} /* flash_get_blk */

/***************************************************************************
 * Function Name: flash_get_total_size
 * Description  : Returns the number of bytes in the flash device.
 * Returns      : Number of bytes in the flash device.
 ***************************************************************************/
int flash_get_total_size(void)
{
    return( (g_flash_info)
        ? (*g_flash_info->fn_flash_get_total_size) ()
        : FLASH_API_ERROR );
} /* flash_get_total_size */

/***************************************************************************
 * Function Name: flash_get_flash_type
 * Description  : Returns type of the flash memory.
 * Returns      : Type of the flash memory.
 ***************************************************************************/
int flash_get_flash_type(void)
{
    return( (g_flash_info)
        ? (g_flash_info->flash_type)
        : FLASH_API_ERROR );
} /* flash_get_flash_type */

EXPORT_SYMBOL(flash_sector_erase_int);
EXPORT_SYMBOL(flash_read_buf);
EXPORT_SYMBOL(flash_write_buf);
EXPORT_SYMBOL(flash_get_numsectors);
EXPORT_SYMBOL(flash_get_sector_size);
EXPORT_SYMBOL(flash_get_memptr);
EXPORT_SYMBOL(flash_get_blk);
EXPORT_SYMBOL(flash_get_total_size);
EXPORT_SYMBOL(flash_get_flash_type);

