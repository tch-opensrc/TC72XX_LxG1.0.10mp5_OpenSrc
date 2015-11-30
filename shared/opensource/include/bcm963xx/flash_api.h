/*
<:copyright-broadcom

 Copyright (c) 2005 Broadcom Corporation
 All Rights Reserved
 No portions of this material may be reproduced in any form without the
 written permission of:
          Broadcom Corporation
          16215 Alton Parkway
          Irvine, California 92619
 All information contained in this document is Broadcom Corporation
 company private, proprietary, and trade secret.

:>
*/
/***************************************************************************
 * File Name  : flash_api.h
 *
 * Description: This file contains definitions and prototypes for a public
 *              flash device interface and an internal flash device interface.
 ***************************************************************************/

#if !defined(_FLASH_API_H)
#define _FLASH_API_H

#ifdef __cplusplus
extern "C" {
#endif

/* Flash definitions. */
#define FLASH_API_OK                1
#define FLASH_API_ERROR             -1

#define FLASH_IFC_UNKNOWN           0
#define FLASH_IFC_PARALLEL          1
#define FLASH_IFC_SPI               2
#define FLASH_IFC_HS_SPI            3
#define FLASH_IFC_NAND              4

#define NAND_REINIT_FLASH           0xffff

#define REMOTE_FLASH_DEVICE_ID 0
#define REMOTE_SPI_DEVICE_ID 1
#define REMOTE_OPEN     0
#define REMOTE_CLOSE    1
#define REMOTE_READ     2
#define REMOTE_WRITE    3
#define REMOTE_IOCTL    4
#define REMOTE_ERASE    5
#define REMOTE_ECHO     6
#define REMOTE_VERSION  7
#define REMOTE_BLOCK_MARKBAD 8
#define REMOTE_GET_DEV_INFO 9
#define REMOTE_BLOCK_ISBAD 10
#define REMOTE_READ_OOB    11 
#define REMOTE_WRITE_OOB   12 

/* Public Interface Prototypes. */
int flash_init(void);
int flash_sector_erase_int(unsigned short sector);
int flash_read_buf(unsigned short sector, int offset, unsigned char *buffer,
    int numbytes);
int flash_write_buf(unsigned short sector, int offset, unsigned char *buffer,
    int numbytes);
int flash_get_numsectors(void);
int flash_get_sector_size(unsigned short sector);
unsigned char *flash_get_memptr(unsigned short sector);
int flash_get_blk(int addr);
int flash_get_total_size(void);
int flash_get_flash_type(void);

/* Internal Flash Device Driver Information. */
typedef struct flash_device_info_s
{
    unsigned short flash_device_id;
    unsigned short flash_type;
    char flash_device_name[30];

    int (*fn_flash_sector_erase_int) (unsigned short sector);
    int (*fn_flash_read_buf) (unsigned short sector, int offset,
        unsigned char *buffer, int numbytes);
    int (*fn_flash_write_buf) (unsigned short sector, int offset,
        unsigned char *buffer, int numbytes);
    int (*fn_flash_get_numsectors) (void);
    int (*fn_flash_get_sector_size) (unsigned short sector);
    unsigned char * (*fn_flash_get_memptr) (unsigned short sector);
    int (*fn_flash_get_blk) (int addr);
    int (*fn_flash_get_total_size) (void);
} flash_device_info_t;

/* Information on the remote flash device */
typedef struct vflash_info_t
{
	unsigned int type; /* 3 - NOR; 4 - NAND */ 
	unsigned int size_l; /* size lower 32 bit */
	unsigned int size_h; /* size higher 32 bit */
	unsigned int erase_size; /* erase size */
	unsigned int page_size; /* page size */
	//struct nand_ecclayout ecc_layout; /* ECC layout */
} vflash_info_t;

#ifdef __cplusplus
}
#endif

#endif /* _FLASH_API_H */

